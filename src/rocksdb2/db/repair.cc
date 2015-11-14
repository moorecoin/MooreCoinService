//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// we recover the contents of the descriptor from the other files we find.
// (1) any log files are first converted to tables
// (2) we scan every table to compute
//     (a) smallest/largest for the table
//     (b) largest sequence number in the table
// (3) we generate descriptor contents:
//      - log number is set to zero
//      - next-file-number is set to 1 + largest file number we found
//      - last-sequence-number is set to largest sequence# found across
//        all tables (see 2c)
//      - compaction pointers are cleared
//      - every table file is added at level 0
//
// possible optimization 1:
//   (a) compute total size and use to pick appropriate max-level m
//   (b) sort tables by largest sequence# in the table
//   (c) for each table: if it overlaps earlier table, place in level-0,
//       else place in level-m.
// possible optimization 2:
//   store per-table metadata (smallest, largest, largest-seq#, ...)
//   in the table's meta section to speed up scantable.

#ifndef rocksdb_lite

#define __stdc_format_macros
#include <inttypes.h>
#include "db/builder.h"
#include "db/db_impl.h"
#include "db/dbformat.h"
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/table_cache.h"
#include "db/version_edit.h"
#include "db/write_batch_internal.h"
#include "rocksdb/comparator.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"

namespace rocksdb {

namespace {

class repairer {
 public:
  repairer(const std::string& dbname, const options& options)
      : dbname_(dbname),
        env_(options.env),
        icmp_(options.comparator),
        options_(sanitizeoptions(dbname, &icmp_, options)),
        raw_table_cache_(
            // tablecache can be small since we expect each table to be opened
            // once.
            newlrucache(10, options_.table_cache_numshardbits,
                        options_.table_cache_remove_scan_count_limit)),
        next_file_number_(1) {
    table_cache_ =
        new tablecache(&options_, storage_options_, raw_table_cache_.get());
    edit_ = new versionedit();
  }

  ~repairer() {
    delete table_cache_;
    raw_table_cache_.reset();
    delete edit_;
  }

  status run() {
    status status = findfiles();
    if (status.ok()) {
      convertlogfilestotables();
      extractmetadata();
      status = writedescriptor();
    }
    if (status.ok()) {
      uint64_t bytes = 0;
      for (size_t i = 0; i < tables_.size(); i++) {
        bytes += tables_[i].meta.fd.getfilesize();
      }
      log(options_.info_log,
          "**** repaired rocksdb %s; "
          "recovered %zu files; %" priu64
          "bytes. "
          "some data may have been lost. "
          "****",
          dbname_.c_str(), tables_.size(), bytes);
    }
    return status;
  }

 private:
  struct tableinfo {
    filemetadata meta;
    sequencenumber min_sequence;
    sequencenumber max_sequence;
  };

  std::string const dbname_;
  env* const env_;
  internalkeycomparator const icmp_;
  options const options_;
  std::shared_ptr<cache> raw_table_cache_;
  tablecache* table_cache_;
  versionedit* edit_;

  std::vector<std::string> manifests_;
  std::vector<filedescriptor> table_fds_;
  std::vector<uint64_t> logs_;
  std::vector<tableinfo> tables_;
  uint64_t next_file_number_;
  const envoptions storage_options_;

  status findfiles() {
    std::vector<std::string> filenames;
    bool found_file = false;
    for (uint32_t path_id = 0; path_id < options_.db_paths.size(); path_id++) {
      status status =
          env_->getchildren(options_.db_paths[path_id].path, &filenames);
      if (!status.ok()) {
        return status;
      }
      if (!filenames.empty()) {
        found_file = true;
      }

      uint64_t number;
      filetype type;
      for (size_t i = 0; i < filenames.size(); i++) {
        if (parsefilename(filenames[i], &number, &type)) {
          if (type == kdescriptorfile) {
            assert(path_id == 0);
            manifests_.push_back(filenames[i]);
          } else {
            if (number + 1 > next_file_number_) {
              next_file_number_ = number + 1;
            }
            if (type == klogfile) {
              assert(path_id == 0);
              logs_.push_back(number);
            } else if (type == ktablefile) {
              table_fds_.emplace_back(number, path_id, 0);
            } else {
              // ignore other files
            }
          }
        }
      }
    }
    if (!found_file) {
      return status::corruption(dbname_, "repair found no files");
    }
    return status::ok();
  }

  void convertlogfilestotables() {
    for (size_t i = 0; i < logs_.size(); i++) {
      std::string logname = logfilename(dbname_, logs_[i]);
      status status = convertlogtotable(logs_[i]);
      if (!status.ok()) {
        log(options_.info_log,
            "log #%" priu64 ": ignoring conversion error: %s", logs_[i],
            status.tostring().c_str());
      }
      archivefile(logname);
    }
  }

  status convertlogtotable(uint64_t log) {
    struct logreporter : public log::reader::reporter {
      env* env;
      std::shared_ptr<logger> info_log;
      uint64_t lognum;
      virtual void corruption(size_t bytes, const status& s) {
        // we print error messages for corruption, but continue repairing.
        log(info_log, "log #%" priu64 ": dropping %d bytes; %s", lognum,
            static_cast<int>(bytes), s.tostring().c_str());
      }
    };

    // open the log file
    std::string logname = logfilename(dbname_, log);
    unique_ptr<sequentialfile> lfile;
    status status = env_->newsequentialfile(logname, &lfile, storage_options_);
    if (!status.ok()) {
      return status;
    }

    // create the log reader.
    logreporter reporter;
    reporter.env = env_;
    reporter.info_log = options_.info_log;
    reporter.lognum = log;
    // we intentially make log::reader do checksumming so that
    // corruptions cause entire commits to be skipped instead of
    // propagating bad information (like overly large sequence
    // numbers).
    log::reader reader(std::move(lfile), &reporter, false/*do not checksum*/,
                       0/*initial_offset*/);

    // read all the records and add to a memtable
    std::string scratch;
    slice record;
    writebatch batch;
    memtable* mem = new memtable(icmp_, options_);
    auto cf_mems_default = new columnfamilymemtablesdefault(mem, &options_);
    mem->ref();
    int counter = 0;
    while (reader.readrecord(&record, &scratch)) {
      if (record.size() < 12) {
        reporter.corruption(
            record.size(), status::corruption("log record too small"));
        continue;
      }
      writebatchinternal::setcontents(&batch, record);
      status = writebatchinternal::insertinto(&batch, cf_mems_default);
      if (status.ok()) {
        counter += writebatchinternal::count(&batch);
      } else {
        log(options_.info_log, "log #%" priu64 ": ignoring %s", log,
            status.tostring().c_str());
        status = status::ok();  // keep going with rest of file
      }
    }

    // do not record a version edit for this conversion to a table
    // since extractmetadata() will also generate edits.
    filemetadata meta;
    meta.fd = filedescriptor(next_file_number_++, 0, 0);
    readoptions ro;
    ro.total_order_seek = true;
    iterator* iter = mem->newiterator(ro);
    status = buildtable(dbname_, env_, options_, storage_options_, table_cache_,
                        iter, &meta, icmp_, 0, 0, knocompression);
    delete iter;
    delete mem->unref();
    delete cf_mems_default;
    mem = nullptr;
    if (status.ok()) {
      if (meta.fd.getfilesize() > 0) {
        table_fds_.push_back(meta.fd);
      }
    }
    log(options_.info_log,
        "log #%" priu64 ": %d ops saved to table #%" priu64 " %s", log, counter,
        meta.fd.getnumber(), status.tostring().c_str());
    return status;
  }

  void extractmetadata() {
    for (size_t i = 0; i < table_fds_.size(); i++) {
      tableinfo t;
      t.meta.fd = table_fds_[i];
      status status = scantable(&t);
      if (!status.ok()) {
        std::string fname = tablefilename(
            options_.db_paths, t.meta.fd.getnumber(), t.meta.fd.getpathid());
        char file_num_buf[kformatfilenumberbufsize];
        formatfilenumber(t.meta.fd.getnumber(), t.meta.fd.getpathid(),
                         file_num_buf, sizeof(file_num_buf));
        log(options_.info_log, "table #%s: ignoring %s", file_num_buf,
            status.tostring().c_str());
        archivefile(fname);
      } else {
        tables_.push_back(t);
      }
    }
  }

  status scantable(tableinfo* t) {
    std::string fname = tablefilename(options_.db_paths, t->meta.fd.getnumber(),
                                      t->meta.fd.getpathid());
    int counter = 0;
    uint64_t file_size;
    status status = env_->getfilesize(fname, &file_size);
    t->meta.fd = filedescriptor(t->meta.fd.getnumber(), t->meta.fd.getpathid(),
                                file_size);
    if (status.ok()) {
      iterator* iter = table_cache_->newiterator(
          readoptions(), storage_options_, icmp_, t->meta.fd);
      bool empty = true;
      parsedinternalkey parsed;
      t->min_sequence = 0;
      t->max_sequence = 0;
      for (iter->seektofirst(); iter->valid(); iter->next()) {
        slice key = iter->key();
        if (!parseinternalkey(key, &parsed)) {
          log(options_.info_log, "table #%" priu64 ": unparsable key %s",
              t->meta.fd.getnumber(), escapestring(key).c_str());
          continue;
        }

        counter++;
        if (empty) {
          empty = false;
          t->meta.smallest.decodefrom(key);
        }
        t->meta.largest.decodefrom(key);
        if (parsed.sequence < t->min_sequence) {
          t->min_sequence = parsed.sequence;
        }
        if (parsed.sequence > t->max_sequence) {
          t->max_sequence = parsed.sequence;
        }
      }
      if (!iter->status().ok()) {
        status = iter->status();
      }
      delete iter;
    }
    log(options_.info_log, "table #%" priu64 ": %d entries %s",
        t->meta.fd.getnumber(), counter, status.tostring().c_str());
    return status;
  }

  status writedescriptor() {
    std::string tmp = tempfilename(dbname_, 1);
    unique_ptr<writablefile> file;
    status status = env_->newwritablefile(
        tmp, &file, env_->optimizeformanifestwrite(storage_options_));
    if (!status.ok()) {
      return status;
    }

    sequencenumber max_sequence = 0;
    for (size_t i = 0; i < tables_.size(); i++) {
      if (max_sequence < tables_[i].max_sequence) {
        max_sequence = tables_[i].max_sequence;
      }
    }

    edit_->setcomparatorname(icmp_.user_comparator()->name());
    edit_->setlognumber(0);
    edit_->setnextfile(next_file_number_);
    edit_->setlastsequence(max_sequence);

    for (size_t i = 0; i < tables_.size(); i++) {
      // todo(opt): separate out into multiple levels
      const tableinfo& t = tables_[i];
      edit_->addfile(0, t.meta.fd.getnumber(), t.meta.fd.getpathid(),
                     t.meta.fd.getfilesize(), t.meta.smallest, t.meta.largest,
                     t.min_sequence, t.max_sequence);
    }

    //fprintf(stderr, "newdescriptor:\n%s\n", edit_.debugstring().c_str());
    {
      log::writer log(std::move(file));
      std::string record;
      edit_->encodeto(&record);
      status = log.addrecord(record);
    }

    if (!status.ok()) {
      env_->deletefile(tmp);
    } else {
      // discard older manifests
      for (size_t i = 0; i < manifests_.size(); i++) {
        archivefile(dbname_ + "/" + manifests_[i]);
      }

      // install new manifest
      status = env_->renamefile(tmp, descriptorfilename(dbname_, 1));
      if (status.ok()) {
        status = setcurrentfile(env_, dbname_, 1, nullptr);
      } else {
        env_->deletefile(tmp);
      }
    }
    return status;
  }

  void archivefile(const std::string& fname) {
    // move into another directory.  e.g., for
    //    dir/foo
    // rename to
    //    dir/lost/foo
    const char* slash = strrchr(fname.c_str(), '/');
    std::string new_dir;
    if (slash != nullptr) {
      new_dir.assign(fname.data(), slash - fname.data());
    }
    new_dir.append("/lost");
    env_->createdir(new_dir);  // ignore error
    std::string new_file = new_dir;
    new_file.append("/");
    new_file.append((slash == nullptr) ? fname.c_str() : slash + 1);
    status s = env_->renamefile(fname, new_file);
    log(options_.info_log, "archiving %s: %s\n",
        fname.c_str(), s.tostring().c_str());
  }
};
}  // namespace

status repairdb(const std::string& dbname, const options& options) {
  repairer repairer(dbname, options);
  return repairer.run();
}

}  // namespace rocksdb

#endif  // rocksdb_lite
