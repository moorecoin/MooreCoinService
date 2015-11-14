//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <algorithm>
#include <iostream>
#include <set>
#include <unistd.h>
#include <unordered_set>
#include <utility>

#include "db/dbformat.h"
#include "db/db_impl.h"
#include "db/filename.h"
#include "db/version_set.h"
#include "db/write_batch_internal.h"
#include "rocksdb/cache.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/perf_context.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/table.h"
#include "rocksdb/options.h"
#include "rocksdb/table_properties.h"
#include "rocksdb/utilities/write_batch_with_index.h"
#include "table/block_based_table_factory.h"
#include "table/plain_table_factory.h"
#include "util/hash.h"
#include "util/hash_linklist_rep.h"
#include "utilities/merge_operators.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/rate_limiter.h"
#include "util/statistics.h"
#include "util/testharness.h"
#include "util/sync_point.h"
#include "util/testutil.h"

namespace rocksdb {

static bool snappycompressionsupported(const compressionoptions& options) {
  std::string out;
  slice in = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  return port::snappy_compress(options, in.data(), in.size(), &out);
}

static bool zlibcompressionsupported(const compressionoptions& options) {
  std::string out;
  slice in = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  return port::zlib_compress(options, in.data(), in.size(), &out);
}

static bool bzip2compressionsupported(const compressionoptions& options) {
  std::string out;
  slice in = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  return port::bzip2_compress(options, in.data(), in.size(), &out);
}

static bool lz4compressionsupported(const compressionoptions &options) {
  std::string out;
  slice in = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  return port::lz4_compress(options, in.data(), in.size(), &out);
}

static bool lz4hccompressionsupported(const compressionoptions &options) {
  std::string out;
  slice in = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  return port::lz4hc_compress(options, in.data(), in.size(), &out);
}

static std::string randomstring(random *rnd, int len) {
  std::string r;
  test::randomstring(rnd, len, &r);
  return r;
}

namespace anon {
class atomiccounter {
 private:
  port::mutex mu_;
  int count_;
 public:
  atomiccounter() : count_(0) { }
  void increment() {
    mutexlock l(&mu_);
    count_++;
  }
  int read() {
    mutexlock l(&mu_);
    return count_;
  }
  void reset() {
    mutexlock l(&mu_);
    count_ = 0;
  }
};

struct optionsoverride {
  std::shared_ptr<const filterpolicy> filter_policy = nullptr;
};

}  // namespace anon

static std::string key(int i) {
  char buf[100];
  snprintf(buf, sizeof(buf), "key%06d", i);
  return std::string(buf);
}

// special env used to delay background operations
class specialenv : public envwrapper {
 public:
  // sstable sync() calls are blocked while this pointer is non-nullptr.
  port::atomicpointer delay_sstable_sync_;

  // simulate no-space errors while this pointer is non-nullptr.
  port::atomicpointer no_space_;

  // simulate non-writable file system while this pointer is non-nullptr
  port::atomicpointer non_writable_;

  // force sync of manifest files to fail while this pointer is non-nullptr
  port::atomicpointer manifest_sync_error_;

  // force write to manifest files to fail while this pointer is non-nullptr
  port::atomicpointer manifest_write_error_;

  // force write to log files to fail while this pointer is non-nullptr
  port::atomicpointer log_write_error_;

  bool count_random_reads_;
  anon::atomiccounter random_read_counter_;

  bool count_sequential_reads_;
  anon::atomiccounter sequential_read_counter_;

  anon::atomiccounter sleep_counter_;

  std::atomic<int64_t> bytes_written_;

  explicit specialenv(env* base) : envwrapper(base) {
    delay_sstable_sync_.release_store(nullptr);
    no_space_.release_store(nullptr);
    non_writable_.release_store(nullptr);
    count_random_reads_ = false;
    count_sequential_reads_ = false;
    manifest_sync_error_.release_store(nullptr);
    manifest_write_error_.release_store(nullptr);
    log_write_error_.release_store(nullptr);
    bytes_written_ = 0;
  }

  status newwritablefile(const std::string& f, unique_ptr<writablefile>* r,
                         const envoptions& soptions) {
    class sstablefile : public writablefile {
     private:
      specialenv* env_;
      unique_ptr<writablefile> base_;

     public:
      sstablefile(specialenv* env, unique_ptr<writablefile>&& base)
          : env_(env),
            base_(std::move(base)) {
      }
      status append(const slice& data) {
        if (env_->no_space_.acquire_load() != nullptr) {
          // drop writes on the floor
          return status::ok();
        } else {
          env_->bytes_written_ += data.size();
          return base_->append(data);
        }
      }
      status close() { return base_->close(); }
      status flush() { return base_->flush(); }
      status sync() {
        while (env_->delay_sstable_sync_.acquire_load() != nullptr) {
          env_->sleepformicroseconds(100000);
        }
        return base_->sync();
      }
      void setiopriority(env::iopriority pri) {
        base_->setiopriority(pri);
      }
    };
    class manifestfile : public writablefile {
     private:
      specialenv* env_;
      unique_ptr<writablefile> base_;
     public:
      manifestfile(specialenv* env, unique_ptr<writablefile>&& b)
          : env_(env), base_(std::move(b)) { }
      status append(const slice& data) {
        if (env_->manifest_write_error_.acquire_load() != nullptr) {
          return status::ioerror("simulated writer error");
        } else {
          return base_->append(data);
        }
      }
      status close() { return base_->close(); }
      status flush() { return base_->flush(); }
      status sync() {
        if (env_->manifest_sync_error_.acquire_load() != nullptr) {
          return status::ioerror("simulated sync error");
        } else {
          return base_->sync();
        }
      }
    };
    class logfile : public writablefile {
     private:
      specialenv* env_;
      unique_ptr<writablefile> base_;
     public:
      logfile(specialenv* env, unique_ptr<writablefile>&& b)
          : env_(env), base_(std::move(b)) { }
      status append(const slice& data) {
        if (env_->log_write_error_.acquire_load() != nullptr) {
          return status::ioerror("simulated writer error");
        } else {
          return base_->append(data);
        }
      }
      status close() { return base_->close(); }
      status flush() { return base_->flush(); }
      status sync() { return base_->sync(); }
    };

    if (non_writable_.acquire_load() != nullptr) {
      return status::ioerror("simulated write error");
    }

    status s = target()->newwritablefile(f, r, soptions);
    if (s.ok()) {
      if (strstr(f.c_str(), ".sst") != nullptr) {
        r->reset(new sstablefile(this, std::move(*r)));
      } else if (strstr(f.c_str(), "manifest") != nullptr) {
        r->reset(new manifestfile(this, std::move(*r)));
      } else if (strstr(f.c_str(), "log") != nullptr) {
        r->reset(new logfile(this, std::move(*r)));
      }
    }
    return s;
  }

  status newrandomaccessfile(const std::string& f,
                             unique_ptr<randomaccessfile>* r,
                             const envoptions& soptions) {
    class countingfile : public randomaccessfile {
     private:
      unique_ptr<randomaccessfile> target_;
      anon::atomiccounter* counter_;
     public:
      countingfile(unique_ptr<randomaccessfile>&& target,
                   anon::atomiccounter* counter)
          : target_(std::move(target)), counter_(counter) {
      }
      virtual status read(uint64_t offset, size_t n, slice* result,
                          char* scratch) const {
        counter_->increment();
        return target_->read(offset, n, result, scratch);
      }
    };

    status s = target()->newrandomaccessfile(f, r, soptions);
    if (s.ok() && count_random_reads_) {
      r->reset(new countingfile(std::move(*r), &random_read_counter_));
    }
    return s;
  }

  status newsequentialfile(const std::string& f, unique_ptr<sequentialfile>* r,
                           const envoptions& soptions) {
    class countingfile : public sequentialfile {
     private:
      unique_ptr<sequentialfile> target_;
      anon::atomiccounter* counter_;

     public:
      countingfile(unique_ptr<sequentialfile>&& target,
                   anon::atomiccounter* counter)
          : target_(std::move(target)), counter_(counter) {}
      virtual status read(size_t n, slice* result, char* scratch) {
        counter_->increment();
        return target_->read(n, result, scratch);
      }
      virtual status skip(uint64_t n) { return target_->skip(n); }
    };

    status s = target()->newsequentialfile(f, r, soptions);
    if (s.ok() && count_sequential_reads_) {
      r->reset(new countingfile(std::move(*r), &sequential_read_counter_));
    }
    return s;
  }

  virtual void sleepformicroseconds(int micros) {
    sleep_counter_.increment();
    target()->sleepformicroseconds(micros);
  }
};

class dbtest {
 protected:
  // sequence of option configurations to try
  enum optionconfig {
    kdefault = 0,
    kblockbasedtablewithprefixhashindex = 1,
    kblockbasedtablewithwholekeyhashindex = 2,
    kplaintablefirstbyteprefix = 3,
    kplaintableallbytesprefix = 4,
    kvectorrep = 5,
    khashlinklist = 6,
    khashcuckoo = 7,
    kmergeput = 8,
    kfilter = 9,
    kuncompressed = 10,
    knumlevel_3 = 11,
    kdblogdir = 12,
    kwaldir = 13,
    kmanifestfilesize = 14,
    kcompactonflush = 15,
    kperfoptions = 16,
    kdeletesfilterfirst = 17,
    khashskiplist = 18,
    kuniversalcompaction = 19,
    kcompressedblockcache = 20,
    kinfinitemaxopenfiles = 21,
    kxxhashchecksum = 22,
    kfifocompaction = 23,
    kend = 24
  };
  int option_config_;

 public:
  std::string dbname_;
  specialenv* env_;
  db* db_;
  std::vector<columnfamilyhandle*> handles_;

  options last_options_;

  // skip some options, as they may not be applicable to a specific test.
  // to add more skip constants, use values 4, 8, 16, etc.
  enum optionskip {
    knoskip = 0,
    kskipdeletesfilterfirst = 1,
    kskipuniversalcompaction = 2,
    kskipmergeput = 4,
    kskipplaintable = 8,
    kskiphashindex = 16,
    kskipnoseektolast = 32,
    kskiphashcuckoo = 64,
    kskipfifocompaction = 128,
  };


  dbtest() : option_config_(kdefault),
             env_(new specialenv(env::default())) {
    dbname_ = test::tmpdir() + "/db_test";
    assert_ok(destroydb(dbname_, options()));
    db_ = nullptr;
    reopen();
  }

  ~dbtest() {
    close();
    options options;
    options.db_paths.emplace_back(dbname_, 0);
    options.db_paths.emplace_back(dbname_ + "_2", 0);
    options.db_paths.emplace_back(dbname_ + "_3", 0);
    options.db_paths.emplace_back(dbname_ + "_4", 0);
    assert_ok(destroydb(dbname_, options));
    delete env_;
  }

  // switch to a fresh database with the next option configuration to
  // test.  return false if there are no more configurations to test.
  bool changeoptions(int skip_mask = knoskip) {
    for(option_config_++; option_config_ < kend; option_config_++) {
      if ((skip_mask & kskipdeletesfilterfirst) &&
          option_config_ == kdeletesfilterfirst) {
        continue;
      }
      if ((skip_mask & kskipuniversalcompaction) &&
          option_config_ == kuniversalcompaction) {
        continue;
      }
      if ((skip_mask & kskipmergeput) && option_config_ == kmergeput) {
        continue;
      }
      if ((skip_mask & kskipnoseektolast) &&
          (option_config_ == khashlinklist ||
           option_config_ == khashskiplist)) {;
        continue;
      }
      if ((skip_mask & kskipplaintable)
          && (option_config_ == kplaintableallbytesprefix
              || option_config_ == kplaintablefirstbyteprefix)) {
        continue;
      }
      if ((skip_mask & kskiphashindex) &&
          (option_config_ == kblockbasedtablewithprefixhashindex ||
           option_config_ == kblockbasedtablewithwholekeyhashindex)) {
        continue;
      }
      if ((skip_mask & kskiphashcuckoo) && (option_config_ == khashcuckoo)) {
        continue;
      }
      if ((skip_mask & kskipfifocompaction) &&
          option_config_ == kfifocompaction) {
        continue;
      }
      break;
    }

    if (option_config_ >= kend) {
      destroy(&last_options_);
      return false;
    } else {
      destroyandreopen();
      return true;
    }
  }

  // switch between different compaction styles (we have only 2 now).
  bool changecompactoptions(options* prev_options = nullptr) {
    if (option_config_ == kdefault) {
      option_config_ = kuniversalcompaction;
      if (prev_options == nullptr) {
        prev_options = &last_options_;
      }
      destroy(prev_options);
      tryreopen();
      return true;
    } else {
      return false;
    }
  }

  // return the current option configuration.
  options currentoptions(
      const anon::optionsoverride& options_override = anon::optionsoverride()) {
    options options;
    return currentoptions(options, options_override);
  }

  options currentoptions(
      const options& defaultoptions,
      const anon::optionsoverride& options_override = anon::optionsoverride()) {
    // this redudant copy is to minimize code change w/o having lint error.
    options options = defaultoptions;
    blockbasedtableoptions table_options;
    bool set_block_based_table_factory = true;
    switch (option_config_) {
      case khashskiplist:
        options.prefix_extractor.reset(newfixedprefixtransform(1));
        options.memtable_factory.reset(
            newhashskiplistrepfactory(16));
        break;
      case kplaintablefirstbyteprefix:
        options.table_factory.reset(new plaintablefactory());
        options.prefix_extractor.reset(newfixedprefixtransform(1));
        options.allow_mmap_reads = true;
        options.max_sequential_skip_in_iterations = 999999;
        set_block_based_table_factory = false;
        break;
      case kplaintableallbytesprefix:
        options.table_factory.reset(new plaintablefactory());
        options.prefix_extractor.reset(newnooptransform());
        options.allow_mmap_reads = true;
        options.max_sequential_skip_in_iterations = 999999;
        set_block_based_table_factory = false;
        break;
      case kmergeput:
        options.merge_operator = mergeoperators::createputoperator();
        break;
      case kfilter:
        table_options.filter_policy.reset(newbloomfilterpolicy(10));
        break;
      case kuncompressed:
        options.compression = knocompression;
        break;
      case knumlevel_3:
        options.num_levels = 3;
        break;
      case kdblogdir:
        options.db_log_dir = test::tmpdir();
        break;
      case kwaldir:
        options.wal_dir = test::tmpdir() + "/wal";
        break;
      case kmanifestfilesize:
        options.max_manifest_file_size = 50; // 50 bytes
      case kcompactonflush:
        options.purge_redundant_kvs_while_flush =
          !options.purge_redundant_kvs_while_flush;
        break;
      case kperfoptions:
        options.hard_rate_limit = 2.0;
        options.rate_limit_delay_max_milliseconds = 2;
        // todo -- test more options
        break;
      case kdeletesfilterfirst:
        options.filter_deletes = true;
        break;
      case kvectorrep:
        options.memtable_factory.reset(new vectorrepfactory(100));
        break;
      case khashlinklist:
        options.prefix_extractor.reset(newfixedprefixtransform(1));
        options.memtable_factory.reset(
            newhashlinklistrepfactory(4, 0, 3, true, 4));
        break;
      case khashcuckoo:
        options.memtable_factory.reset(
            newhashcuckoorepfactory(options.write_buffer_size));
        break;
      case kuniversalcompaction:
        options.compaction_style = kcompactionstyleuniversal;
        break;
      case kcompressedblockcache:
        options.allow_mmap_writes = true;
        table_options.block_cache_compressed = newlrucache(8*1024*1024);
        break;
      case kinfinitemaxopenfiles:
        options.max_open_files = -1;
        break;
      case kxxhashchecksum: {
        table_options.checksum = kxxhash;
        break;
      }
      case kfifocompaction: {
        options.compaction_style = kcompactionstylefifo;
        break;
      }
      case kblockbasedtablewithprefixhashindex: {
        table_options.index_type = blockbasedtableoptions::khashsearch;
        options.prefix_extractor.reset(newfixedprefixtransform(1));
        break;
      }
      case kblockbasedtablewithwholekeyhashindex: {
        table_options.index_type = blockbasedtableoptions::khashsearch;
        options.prefix_extractor.reset(newnooptransform());
        break;
      }
      default:
        break;
    }

    if (options_override.filter_policy) {
      table_options.filter_policy = options_override.filter_policy;
    }
    if (set_block_based_table_factory) {
      options.table_factory.reset(newblockbasedtablefactory(table_options));
    }
    return options;
  }

  dbimpl* dbfull() {
    return reinterpret_cast<dbimpl*>(db_);
  }

  void createcolumnfamilies(const std::vector<std::string>& cfs,
                            const columnfamilyoptions* options = nullptr) {
    columnfamilyoptions cf_opts;
    if (options != nullptr) {
      cf_opts = columnfamilyoptions(*options);
    } else {
      cf_opts = columnfamilyoptions(currentoptions());
    }
    int cfi = handles_.size();
    handles_.resize(cfi + cfs.size());
    for (auto cf : cfs) {
      assert_ok(db_->createcolumnfamily(cf_opts, cf, &handles_[cfi++]));
    }
  }

  void createandreopenwithcf(const std::vector<std::string>& cfs,
                             const options* options = nullptr) {
    createcolumnfamilies(cfs, options);
    std::vector<std::string> cfs_plus_default = cfs;
    cfs_plus_default.insert(cfs_plus_default.begin(), kdefaultcolumnfamilyname);
    reopenwithcolumnfamilies(cfs_plus_default, options);
  }

  void reopenwithcolumnfamilies(const std::vector<std::string>& cfs,
                                const std::vector<const options*>& options) {
    assert_ok(tryreopenwithcolumnfamilies(cfs, options));
  }

  void reopenwithcolumnfamilies(const std::vector<std::string>& cfs,
                                const options* options = nullptr) {
    assert_ok(tryreopenwithcolumnfamilies(cfs, options));
  }

  status tryreopenwithcolumnfamilies(
      const std::vector<std::string>& cfs,
      const std::vector<const options*>& options) {
    close();
    assert_eq(cfs.size(), options.size());
    std::vector<columnfamilydescriptor> column_families;
    for (size_t i = 0; i < cfs.size(); ++i) {
      column_families.push_back(columnfamilydescriptor(cfs[i], *options[i]));
    }
    dboptions db_opts = dboptions(*options[0]);
    return db::open(db_opts, dbname_, column_families, &handles_, &db_);
  }

  status tryreopenwithcolumnfamilies(const std::vector<std::string>& cfs,
                                     const options* options = nullptr) {
    close();
    options opts = (options == nullptr) ? currentoptions() : *options;
    std::vector<const options*> v_opts(cfs.size(), &opts);
    return tryreopenwithcolumnfamilies(cfs, v_opts);
  }

  void reopen(options* options = nullptr) {
    assert_ok(tryreopen(options));
  }

  void close() {
    for (auto h : handles_) {
      delete h;
    }
    handles_.clear();
    delete db_;
    db_ = nullptr;
  }

  void destroyandreopen(options* options = nullptr) {
    //destroy using last options
    destroy(&last_options_);
    assert_ok(tryreopen(options));
  }

  void destroy(options* options) {
    close();
    assert_ok(destroydb(dbname_, *options));
  }

  status readonlyreopen(options* options) {
    return db::openforreadonly(*options, dbname_, &db_);
  }

  status tryreopen(options* options = nullptr) {
    close();
    options opts;
    if (options != nullptr) {
      opts = *options;
    } else {
      opts = currentoptions();
      opts.create_if_missing = true;
    }
    last_options_ = opts;
    return db::open(opts, dbname_, &db_);
  }

  status flush(int cf = 0) {
    if (cf == 0) {
      return db_->flush(flushoptions());
    } else {
      return db_->flush(flushoptions(), handles_[cf]);
    }
  }

  status put(const slice& k, const slice& v, writeoptions wo = writeoptions()) {
    if (kmergeput == option_config_ ) {
      return db_->merge(wo, k, v);
    } else {
      return db_->put(wo, k, v);
    }
  }

  status put(int cf, const slice& k, const slice& v,
             writeoptions wo = writeoptions()) {
    if (kmergeput == option_config_) {
      return db_->merge(wo, handles_[cf], k, v);
    } else {
      return db_->put(wo, handles_[cf], k, v);
    }
  }

  status delete(const std::string& k) {
    return db_->delete(writeoptions(), k);
  }

  status delete(int cf, const std::string& k) {
    return db_->delete(writeoptions(), handles_[cf], k);
  }

  std::string get(const std::string& k, const snapshot* snapshot = nullptr) {
    readoptions options;
    options.verify_checksums = true;
    options.snapshot = snapshot;
    std::string result;
    status s = db_->get(options, k, &result);
    if (s.isnotfound()) {
      result = "not_found";
    } else if (!s.ok()) {
      result = s.tostring();
    }
    return result;
  }

  std::string get(int cf, const std::string& k,
                  const snapshot* snapshot = nullptr) {
    readoptions options;
    options.verify_checksums = true;
    options.snapshot = snapshot;
    std::string result;
    status s = db_->get(options, handles_[cf], k, &result);
    if (s.isnotfound()) {
      result = "not_found";
    } else if (!s.ok()) {
      result = s.tostring();
    }
    return result;
  }

  // return a string that contains all key,value pairs in order,
  // formatted like "(k1->v1)(k2->v2)".
  std::string contents(int cf = 0) {
    std::vector<std::string> forward;
    std::string result;
    iterator* iter = (cf == 0) ? db_->newiterator(readoptions())
                               : db_->newiterator(readoptions(), handles_[cf]);
    for (iter->seektofirst(); iter->valid(); iter->next()) {
      std::string s = iterstatus(iter);
      result.push_back('(');
      result.append(s);
      result.push_back(')');
      forward.push_back(s);
    }

    // check reverse iteration results are the reverse of forward results
    unsigned int matched = 0;
    for (iter->seektolast(); iter->valid(); iter->prev()) {
      assert_lt(matched, forward.size());
      assert_eq(iterstatus(iter), forward[forward.size() - matched - 1]);
      matched++;
    }
    assert_eq(matched, forward.size());

    delete iter;
    return result;
  }

  std::string allentriesfor(const slice& user_key, int cf = 0) {
    iterator* iter;
    if (cf == 0) {
      iter = dbfull()->test_newinternaliterator();
    } else {
      iter = dbfull()->test_newinternaliterator(handles_[cf]);
    }
    internalkey target(user_key, kmaxsequencenumber, ktypevalue);
    iter->seek(target.encode());
    std::string result;
    if (!iter->status().ok()) {
      result = iter->status().tostring();
    } else {
      result = "[ ";
      bool first = true;
      while (iter->valid()) {
        parsedinternalkey ikey(slice(), 0, ktypevalue);
        if (!parseinternalkey(iter->key(), &ikey)) {
          result += "corrupted";
        } else {
          if (last_options_.comparator->compare(ikey.user_key, user_key) != 0) {
            break;
          }
          if (!first) {
            result += ", ";
          }
          first = false;
          switch (ikey.type) {
            case ktypevalue:
              result += iter->value().tostring();
              break;
            case ktypemerge:
              // keep it the same as ktypevalue for testing kmergeput
              result += iter->value().tostring();
              break;
            case ktypedeletion:
              result += "del";
              break;
            default:
              assert(false);
              break;
          }
        }
        iter->next();
      }
      if (!first) {
        result += " ";
      }
      result += "]";
    }
    delete iter;
    return result;
  }

  int numtablefilesatlevel(int level, int cf = 0) {
    std::string property;
    if (cf == 0) {
      // default cfd
      assert_true(db_->getproperty(
          "rocksdb.num-files-at-level" + numbertostring(level), &property));
    } else {
      assert_true(db_->getproperty(
          handles_[cf], "rocksdb.num-files-at-level" + numbertostring(level),
          &property));
    }
    return atoi(property.c_str());
  }

  int totaltablefiles(int cf = 0, int levels = -1) {
    if (levels == -1) {
      levels = currentoptions().num_levels;
    }
    int result = 0;
    for (int level = 0; level < levels; level++) {
      result += numtablefilesatlevel(level, cf);
    }
    return result;
  }

  // return spread of files per level
  std::string filesperlevel(int cf = 0) {
    int num_levels =
        (cf == 0) ? db_->numberlevels() : db_->numberlevels(handles_[1]);
    std::string result;
    int last_non_zero_offset = 0;
    for (int level = 0; level < num_levels; level++) {
      int f = numtablefilesatlevel(level, cf);
      char buf[100];
      snprintf(buf, sizeof(buf), "%s%d", (level ? "," : ""), f);
      result += buf;
      if (f > 0) {
        last_non_zero_offset = result.size();
      }
    }
    result.resize(last_non_zero_offset);
    return result;
  }

  int countfiles() {
    std::vector<std::string> files;
    env_->getchildren(dbname_, &files);

    std::vector<std::string> logfiles;
    if (dbname_ != last_options_.wal_dir) {
      env_->getchildren(last_options_.wal_dir, &logfiles);
    }

    return static_cast<int>(files.size() + logfiles.size());
  }

  int countlivefiles() {
    std::vector<livefilemetadata> metadata;
    db_->getlivefilesmetadata(&metadata);
    return metadata.size();
  }

  uint64_t size(const slice& start, const slice& limit, int cf = 0) {
    range r(start, limit);
    uint64_t size;
    if (cf == 0) {
      db_->getapproximatesizes(&r, 1, &size);
    } else {
      db_->getapproximatesizes(handles_[1], &r, 1, &size);
    }
    return size;
  }

  void compact(int cf, const slice& start, const slice& limit) {
    assert_ok(db_->compactrange(handles_[cf], &start, &limit));
  }

  void compact(const slice& start, const slice& limit) {
    assert_ok(db_->compactrange(&start, &limit));
  }

  // do n memtable compactions, each of which produces an sstable
  // covering the range [small,large].
  void maketables(int n, const std::string& small, const std::string& large,
                  int cf = 0) {
    for (int i = 0; i < n; i++) {
      assert_ok(put(cf, small, "begin"));
      assert_ok(put(cf, large, "end"));
      assert_ok(flush(cf));
    }
  }

  // prevent pushing of new sstables into deeper levels by adding
  // tables that cover a specified range to all levels.
  void filllevels(const std::string& smallest, const std::string& largest,
                  int cf) {
    maketables(db_->numberlevels(handles_[cf]), smallest, largest, cf);
  }

  void dumpfilecounts(const char* label) {
    fprintf(stderr, "---\n%s:\n", label);
    fprintf(stderr, "maxoverlap: %lld\n",
            static_cast<long long>(
                dbfull()->test_maxnextleveloverlappingbytes()));
    for (int level = 0; level < db_->numberlevels(); level++) {
      int num = numtablefilesatlevel(level);
      if (num > 0) {
        fprintf(stderr, "  level %3d : %d files\n", level, num);
      }
    }
  }

  std::string dumpsstablelist() {
    std::string property;
    db_->getproperty("rocksdb.sstables", &property);
    return property;
  }

  int getsstfilecount(std::string path) {
    std::vector<std::string> files;
    env_->getchildren(path, &files);

    int sst_count = 0;
    uint64_t number;
    filetype type;
    for (size_t i = 0; i < files.size(); i++) {
      if (parsefilename(files[i], &number, &type) && type == ktablefile) {
        sst_count++;
      }
    }
    return sst_count;
  }

  void generatenewfile(random* rnd, int* key_idx) {
    for (int i = 0; i < 11; i++) {
      assert_ok(put(key(*key_idx), randomstring(rnd, (i == 10) ? 1 : 10000)));
      (*key_idx)++;
    }
    dbfull()->test_waitforflushmemtable();
    dbfull()->test_waitforcompact();
  }

  std::string iterstatus(iterator* iter) {
    std::string result;
    if (iter->valid()) {
      result = iter->key().tostring() + "->" + iter->value().tostring();
    } else {
      result = "(invalid)";
    }
    return result;
  }

  options optionsforlogitertest() {
    options options = currentoptions();
    options.create_if_missing = true;
    options.wal_ttl_seconds = 1000;
    return options;
  }

  std::unique_ptr<transactionlogiterator> opentransactionlogiter(
      const sequencenumber seq) {
    unique_ptr<transactionlogiterator> iter;
    status status = dbfull()->getupdatessince(seq, &iter);
    assert_ok(status);
    assert_true(iter->valid());
    return std::move(iter);
  }

  std::string dummystring(size_t len, char c = 'a') {
    return std::string(len, c);
  }

  void verifyiterlast(std::string expected_key, int cf = 0) {
    iterator* iter;
    readoptions ro;
    if (cf == 0) {
      iter = db_->newiterator(ro);
    } else {
      iter = db_->newiterator(ro, handles_[cf]);
    }
    iter->seektolast();
    assert_eq(iterstatus(iter), expected_key);
    delete iter;
  }

  // used to test inplaceupdate

  // if previous value is nullptr or delta is > than previous value,
  //   sets newvalue with delta
  // if previous value is not empty,
  //   updates previous value with 'b' string of previous value size - 1.
  static updatestatus
      updateinplacesmallersize(char* prevvalue, uint32_t* prevsize,
                               slice delta, std::string* newvalue) {
    if (prevvalue == nullptr) {
      *newvalue = std::string(delta.size(), 'c');
      return updatestatus::updated;
    } else {
      *prevsize = *prevsize - 1;
      std::string str_b = std::string(*prevsize, 'b');
      memcpy(prevvalue, str_b.c_str(), str_b.size());
      return updatestatus::updated_inplace;
    }
  }

  static updatestatus
      updateinplacesmallervarintsize(char* prevvalue, uint32_t* prevsize,
                                     slice delta, std::string* newvalue) {
    if (prevvalue == nullptr) {
      *newvalue = std::string(delta.size(), 'c');
      return updatestatus::updated;
    } else {
      *prevsize = 1;
      std::string str_b = std::string(*prevsize, 'b');
      memcpy(prevvalue, str_b.c_str(), str_b.size());
      return updatestatus::updated_inplace;
    }
  }

  static updatestatus
      updateinplacelargersize(char* prevvalue, uint32_t* prevsize,
                              slice delta, std::string* newvalue) {
    *newvalue = std::string(delta.size(), 'c');
    return updatestatus::updated;
  }

  static updatestatus
      updateinplacenoaction(char* prevvalue, uint32_t* prevsize,
                            slice delta, std::string* newvalue) {
    return updatestatus::update_failed;
  }

  // utility method to test inplaceupdate
  void validatenumberofentries(int numvalues, int cf = 0) {
    iterator* iter;
    if (cf != 0) {
      iter = dbfull()->test_newinternaliterator(handles_[cf]);
    } else {
      iter = dbfull()->test_newinternaliterator();
    }
    iter->seektofirst();
    assert_eq(iter->status().ok(), true);
    int seq = numvalues;
    while (iter->valid()) {
      parsedinternalkey ikey;
      ikey.sequence = -1;
      assert_eq(parseinternalkey(iter->key(), &ikey), true);

      // checks sequence number for updates
      assert_eq(ikey.sequence, (unsigned)seq--);
      iter->next();
    }
    delete iter;
    assert_eq(0, seq);
  }

  void copyfile(const std::string& source, const std::string& destination,
                uint64_t size = 0) {
    const envoptions soptions;
    unique_ptr<sequentialfile> srcfile;
    assert_ok(env_->newsequentialfile(source, &srcfile, soptions));
    unique_ptr<writablefile> destfile;
    assert_ok(env_->newwritablefile(destination, &destfile, soptions));

    if (size == 0) {
      // default argument means copy everything
      assert_ok(env_->getfilesize(source, &size));
    }

    char buffer[4096];
    slice slice;
    while (size > 0) {
      uint64_t one = std::min(uint64_t(sizeof(buffer)), size);
      assert_ok(srcfile->read(one, &slice, buffer));
      assert_ok(destfile->append(slice));
      size -= slice.size();
    }
    assert_ok(destfile->close());
  }

};

static long testgettickercount(const options& options, tickers ticker_type) {
  return options.statistics->gettickercount(ticker_type);
}

// a helper function that ensures the table properties returned in
// `getpropertiesofalltablestest` is correct.
// this test assumes entries size is differnt for each of the tables.
namespace {
void verifytableproperties(db* db, uint64_t expected_entries_size) {
  tablepropertiescollection props;
  assert_ok(db->getpropertiesofalltables(&props));

  assert_eq(4u, props.size());
  std::unordered_set<uint64_t> unique_entries;

  // indirect test
  uint64_t sum = 0;
  for (const auto& item : props) {
    unique_entries.insert(item.second->num_entries);
    sum += item.second->num_entries;
  }

  assert_eq(props.size(), unique_entries.size());
  assert_eq(expected_entries_size, sum);
}
}  // namespace

test(dbtest, empty) {
  do {
    options options;
    options.env = env_;
    options.write_buffer_size = 100000;  // small write buffer
    options = currentoptions(options);
    createandreopenwithcf({"pikachu"}, &options);

    std::string num;
    assert_true(dbfull()->getproperty(
        handles_[1], "rocksdb.num-entries-active-mem-table", &num));
    assert_eq("0", num);

    assert_ok(put(1, "foo", "v1"));
    assert_eq("v1", get(1, "foo"));
    assert_true(dbfull()->getproperty(
        handles_[1], "rocksdb.num-entries-active-mem-table", &num));
    assert_eq("1", num);

    env_->delay_sstable_sync_.release_store(env_);  // block sync calls
    put(1, "k1", std::string(100000, 'x'));         // fill memtable
    assert_true(dbfull()->getproperty(
        handles_[1], "rocksdb.num-entries-active-mem-table", &num));
    assert_eq("2", num);

    put(1, "k2", std::string(100000, 'y'));         // trigger compaction
    assert_true(dbfull()->getproperty(
        handles_[1], "rocksdb.num-entries-active-mem-table", &num));
    assert_eq("1", num);

    assert_eq("v1", get(1, "foo"));
    env_->delay_sstable_sync_.release_store(nullptr);   // release sync calls

    assert_ok(db_->disablefiledeletions());
    assert_true(
        dbfull()->getproperty("rocksdb.is-file-deletions-enabled", &num));
    assert_eq("1", num);

    assert_ok(db_->disablefiledeletions());
    assert_true(
        dbfull()->getproperty("rocksdb.is-file-deletions-enabled", &num));
    assert_eq("2", num);

    assert_ok(db_->disablefiledeletions());
    assert_true(
        dbfull()->getproperty("rocksdb.is-file-deletions-enabled", &num));
    assert_eq("3", num);

    assert_ok(db_->enablefiledeletions(false));
    assert_true(
        dbfull()->getproperty("rocksdb.is-file-deletions-enabled", &num));
    assert_eq("2", num);

    assert_ok(db_->enablefiledeletions());
    assert_true(
        dbfull()->getproperty("rocksdb.is-file-deletions-enabled", &num));
    assert_eq("0", num);
  } while (changeoptions());
}

test(dbtest, readonlydb) {
  assert_ok(put("foo", "v1"));
  assert_ok(put("bar", "v2"));
  assert_ok(put("foo", "v3"));
  close();

  options options;
  assert_ok(readonlyreopen(&options));
  assert_eq("v3", get("foo"));
  assert_eq("v2", get("bar"));
  iterator* iter = db_->newiterator(readoptions());
  int count = 0;
  for (iter->seektofirst(); iter->valid(); iter->next()) {
    assert_ok(iter->status());
    ++count;
  }
  assert_eq(count, 2);
  delete iter;
  close();

  // reopen and flush memtable.
  reopen();
  flush();
  close();
  // now check keys in read only mode.
  assert_ok(readonlyreopen(&options));
  assert_eq("v3", get("foo"));
  assert_eq("v2", get("bar"));
}

// make sure that when options.block_cache is set, after a new table is
// created its index/filter blocks are added to block cache.
test(dbtest, indexandfilterblocksofnewtableaddedtocache) {
  options options = currentoptions();
  options.create_if_missing = true;
  options.statistics = rocksdb::createdbstatistics();
  blockbasedtableoptions table_options;
  table_options.cache_index_and_filter_blocks = true;
  table_options.filter_policy.reset(newbloomfilterpolicy(20));
  options.table_factory.reset(new blockbasedtablefactory(table_options));
  createandreopenwithcf({"pikachu"}, &options);

  assert_ok(put(1, "key", "val"));
  // create a new table.
  assert_ok(flush(1));

  // index/filter blocks added to block cache right after table creation.
  assert_eq(1, testgettickercount(options, block_cache_index_miss));
  assert_eq(1, testgettickercount(options, block_cache_filter_miss));
  assert_eq(2, /* only index/filter were added */
            testgettickercount(options, block_cache_add));
  assert_eq(0, testgettickercount(options, block_cache_data_miss));
  uint64_t int_num;
  assert_true(
      dbfull()->getintproperty("rocksdb.estimate-table-readers-mem", &int_num));
  assert_eq(int_num, 0u);

  // make sure filter block is in cache.
  std::string value;
  readoptions ropt;
  db_->keymayexist(readoptions(), handles_[1], "key", &value);

  // miss count should remain the same.
  assert_eq(1, testgettickercount(options, block_cache_filter_miss));
  assert_eq(1, testgettickercount(options, block_cache_filter_hit));

  db_->keymayexist(readoptions(), handles_[1], "key", &value);
  assert_eq(1, testgettickercount(options, block_cache_filter_miss));
  assert_eq(2, testgettickercount(options, block_cache_filter_hit));

  // make sure index block is in cache.
  auto index_block_hit = testgettickercount(options, block_cache_filter_hit);
  value = get(1, "key");
  assert_eq(1, testgettickercount(options, block_cache_filter_miss));
  assert_eq(index_block_hit + 1,
            testgettickercount(options, block_cache_filter_hit));

  value = get(1, "key");
  assert_eq(1, testgettickercount(options, block_cache_filter_miss));
  assert_eq(index_block_hit + 2,
            testgettickercount(options, block_cache_filter_hit));
}

test(dbtest, getpropertiesofalltablestest) {
  options options = currentoptions();
  reopen(&options);
  // create 4 tables
  for (int table = 0; table < 4; ++table) {
    for (int i = 0; i < 10 + table; ++i) {
      db_->put(writeoptions(), std::to_string(table * 100 + i), "val");
    }
    db_->flush(flushoptions());
  }

  // 1. read table properties directly from file
  reopen(&options);
  verifytableproperties(db_, 10 + 11 + 12 + 13);

  // 2. put two tables to table cache and
  reopen(&options);
  // fetch key from 1st and 2nd table, which will internally place that table to
  // the table cache.
  for (int i = 0; i < 2; ++i) {
    get(std::to_string(i * 100 + 0));
  }

  verifytableproperties(db_, 10 + 11 + 12 + 13);

  // 3. put all tables to table cache
  reopen(&options);
  // fetch key from 1st and 2nd table, which will internally place that table to
  // the table cache.
  for (int i = 0; i < 4; ++i) {
    get(std::to_string(i * 100 + 0));
  }
  verifytableproperties(db_, 10 + 11 + 12 + 13);
}

test(dbtest, levellimitreopen) {
  options options = currentoptions();
  createandreopenwithcf({"pikachu"}, &options);

  const std::string value(1024 * 1024, ' ');
  int i = 0;
  while (numtablefilesatlevel(2, 1) == 0) {
    assert_ok(put(1, key(i++), value));
  }

  options.num_levels = 1;
  options.max_bytes_for_level_multiplier_additional.resize(1, 1);
  status s = tryreopenwithcolumnfamilies({"default", "pikachu"}, &options);
  assert_eq(s.isinvalidargument(), true);
  assert_eq(s.tostring(),
            "invalid argument: db has more levels than options.num_levels");

  options.num_levels = 10;
  options.max_bytes_for_level_multiplier_additional.resize(10, 1);
  assert_ok(tryreopenwithcolumnfamilies({"default", "pikachu"}, &options));
}

test(dbtest, preallocation) {
  const std::string src = dbname_ + "/alloc_test";
  unique_ptr<writablefile> srcfile;
  const envoptions soptions;
  assert_ok(env_->newwritablefile(src, &srcfile, soptions));
  srcfile->setpreallocationblocksize(1024 * 1024);

  // no writes should mean no preallocation
  size_t block_size, last_allocated_block;
  srcfile->getpreallocationstatus(&block_size, &last_allocated_block);
  assert_eq(last_allocated_block, 0ul);

  // small write should preallocate one block
  srcfile->append("test");
  srcfile->getpreallocationstatus(&block_size, &last_allocated_block);
  assert_eq(last_allocated_block, 1ul);

  // write an entire preallocation block, make sure we increased by two.
  std::string buf(block_size, ' ');
  srcfile->append(buf);
  srcfile->getpreallocationstatus(&block_size, &last_allocated_block);
  assert_eq(last_allocated_block, 2ul);

  // write five more blocks at once, ensure we're where we need to be.
  buf = std::string(block_size * 5, ' ');
  srcfile->append(buf);
  srcfile->getpreallocationstatus(&block_size, &last_allocated_block);
  assert_eq(last_allocated_block, 7ul);
}

test(dbtest, putdeleteget) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "foo", "v1"));
    assert_eq("v1", get(1, "foo"));
    assert_ok(put(1, "foo", "v2"));
    assert_eq("v2", get(1, "foo"));
    assert_ok(delete(1, "foo"));
    assert_eq("not_found", get(1, "foo"));
  } while (changeoptions());
}


test(dbtest, getfromimmutablelayer) {
  do {
    options options;
    options.env = env_;
    options.write_buffer_size = 100000;  // small write buffer
    options = currentoptions(options);
    createandreopenwithcf({"pikachu"}, &options);

    assert_ok(put(1, "foo", "v1"));
    assert_eq("v1", get(1, "foo"));

    env_->delay_sstable_sync_.release_store(env_);   // block sync calls
    put(1, "k1", std::string(100000, 'x'));          // fill memtable
    put(1, "k2", std::string(100000, 'y'));          // trigger flush
    assert_eq("v1", get(1, "foo"));
    assert_eq("not_found", get(0, "foo"));
    env_->delay_sstable_sync_.release_store(nullptr);   // release sync calls
  } while (changeoptions());
}

test(dbtest, getfromversions) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "foo", "v1"));
    assert_ok(flush(1));
    assert_eq("v1", get(1, "foo"));
    assert_eq("not_found", get(0, "foo"));
  } while (changeoptions());
}

test(dbtest, getsnapshot) {
  do {
    createandreopenwithcf({"pikachu"});
    // try with both a short key and a long key
    for (int i = 0; i < 2; i++) {
      std::string key = (i == 0) ? std::string("foo") : std::string(200, 'x');
      assert_ok(put(1, key, "v1"));
      const snapshot* s1 = db_->getsnapshot();
      assert_ok(put(1, key, "v2"));
      assert_eq("v2", get(1, key));
      assert_eq("v1", get(1, key, s1));
      assert_ok(flush(1));
      assert_eq("v2", get(1, key));
      assert_eq("v1", get(1, key, s1));
      db_->releasesnapshot(s1);
    }
    // skip as hashcuckoorep does not support snapshot
  } while (changeoptions(kskiphashcuckoo));
}

test(dbtest, getlevel0ordering) {
  do {
    createandreopenwithcf({"pikachu"});
    // check that we process level-0 files in correct order.  the code
    // below generates two level-0 files where the earlier one comes
    // before the later one in the level-0 file list since the earlier
    // one has a smaller "smallest" key.
    assert_ok(put(1, "bar", "b"));
    assert_ok(put(1, "foo", "v1"));
    assert_ok(flush(1));
    assert_ok(put(1, "foo", "v2"));
    assert_ok(flush(1));
    assert_eq("v2", get(1, "foo"));
  } while (changeoptions());
}

test(dbtest, getorderedbylevels) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "foo", "v1"));
    compact(1, "a", "z");
    assert_eq("v1", get(1, "foo"));
    assert_ok(put(1, "foo", "v2"));
    assert_eq("v2", get(1, "foo"));
    assert_ok(flush(1));
    assert_eq("v2", get(1, "foo"));
  } while (changeoptions());
}

test(dbtest, getpickscorrectfile) {
  do {
    createandreopenwithcf({"pikachu"});
    // arrange to have multiple files in a non-level-0 level.
    assert_ok(put(1, "a", "va"));
    compact(1, "a", "b");
    assert_ok(put(1, "x", "vx"));
    compact(1, "x", "y");
    assert_ok(put(1, "f", "vf"));
    compact(1, "f", "g");
    assert_eq("va", get(1, "a"));
    assert_eq("vf", get(1, "f"));
    assert_eq("vx", get(1, "x"));
  } while (changeoptions());
}

test(dbtest, getencountersemptylevel) {
  do {
    createandreopenwithcf({"pikachu"});
    // arrange for the following to happen:
    //   * sstable a in level 0
    //   * nothing in level 1
    //   * sstable b in level 2
    // then do enough get() calls to arrange for an automatic compaction
    // of sstable a.  a bug would cause the compaction to be marked as
    // occuring at level 1 (instead of the correct level 0).

    // step 1: first place sstables in levels 0 and 2
    int compaction_count = 0;
    while (numtablefilesatlevel(0, 1) == 0 || numtablefilesatlevel(2, 1) == 0) {
      assert_le(compaction_count, 100) << "could not fill levels 0 and 2";
      compaction_count++;
      put(1, "a", "begin");
      put(1, "z", "end");
      assert_ok(flush(1));
    }

    // step 2: clear level 1 if necessary.
    dbfull()->test_compactrange(1, nullptr, nullptr, handles_[1]);
    assert_eq(numtablefilesatlevel(0, 1), 1);
    assert_eq(numtablefilesatlevel(1, 1), 0);
    assert_eq(numtablefilesatlevel(2, 1), 1);

    // step 3: read a bunch of times
    for (int i = 0; i < 1000; i++) {
      assert_eq("not_found", get(1, "missing"));
    }

    // step 4: wait for compaction to finish
    env_->sleepformicroseconds(1000000);

    assert_eq(numtablefilesatlevel(0, 1), 1);  // xxx
  } while (changeoptions(kskipuniversalcompaction | kskipfifocompaction));
}

// keymayexist can lead to a few false positives, but not false negatives.
// to make test deterministic, use a much larger number of bits per key-20 than
// bits in the key, so that false positives are eliminated
test(dbtest, keymayexist) {
  do {
    readoptions ropts;
    std::string value;
    anon::optionsoverride options_override;
    options_override.filter_policy.reset(newbloomfilterpolicy(20));
    options options = currentoptions(options_override);
    options.statistics = rocksdb::createdbstatistics();
    createandreopenwithcf({"pikachu"}, &options);

    assert_true(!db_->keymayexist(ropts, handles_[1], "a", &value));

    assert_ok(put(1, "a", "b"));
    bool value_found = false;
    assert_true(
        db_->keymayexist(ropts, handles_[1], "a", &value, &value_found));
    assert_true(value_found);
    assert_eq("b", value);

    assert_ok(flush(1));
    value.clear();

    long numopen = testgettickercount(options, no_file_opens);
    long cache_added = testgettickercount(options, block_cache_add);
    assert_true(
        db_->keymayexist(ropts, handles_[1], "a", &value, &value_found));
    assert_true(!value_found);
    // assert that no new files were opened and no new blocks were
    // read into block cache.
    assert_eq(numopen, testgettickercount(options, no_file_opens));
    assert_eq(cache_added, testgettickercount(options, block_cache_add));

    assert_ok(delete(1, "a"));

    numopen = testgettickercount(options, no_file_opens);
    cache_added = testgettickercount(options, block_cache_add);
    assert_true(!db_->keymayexist(ropts, handles_[1], "a", &value));
    assert_eq(numopen, testgettickercount(options, no_file_opens));
    assert_eq(cache_added, testgettickercount(options, block_cache_add));

    assert_ok(flush(1));
    db_->compactrange(handles_[1], nullptr, nullptr);

    numopen = testgettickercount(options, no_file_opens);
    cache_added = testgettickercount(options, block_cache_add);
    assert_true(!db_->keymayexist(ropts, handles_[1], "a", &value));
    assert_eq(numopen, testgettickercount(options, no_file_opens));
    assert_eq(cache_added, testgettickercount(options, block_cache_add));

    assert_ok(delete(1, "c"));

    numopen = testgettickercount(options, no_file_opens);
    cache_added = testgettickercount(options, block_cache_add);
    assert_true(!db_->keymayexist(ropts, handles_[1], "c", &value));
    assert_eq(numopen, testgettickercount(options, no_file_opens));
    assert_eq(cache_added, testgettickercount(options, block_cache_add));

    // keymayexist function only checks data in block caches, which is not used
    // by plain table format.
  } while (
      changeoptions(kskipplaintable | kskiphashindex | kskipfifocompaction));
}

test(dbtest, nonblockingiteration) {
  do {
    readoptions non_blocking_opts, regular_opts;
    options options = currentoptions();
    options.statistics = rocksdb::createdbstatistics();
    non_blocking_opts.read_tier = kblockcachetier;
    createandreopenwithcf({"pikachu"}, &options);
    // write one kv to the database.
    assert_ok(put(1, "a", "b"));

    // scan using non-blocking iterator. we should find it because
    // it is in memtable.
    iterator* iter = db_->newiterator(non_blocking_opts, handles_[1]);
    int count = 0;
    for (iter->seektofirst(); iter->valid(); iter->next()) {
      assert_ok(iter->status());
      count++;
    }
    assert_eq(count, 1);
    delete iter;

    // flush memtable to storage. now, the key should not be in the
    // memtable neither in the block cache.
    assert_ok(flush(1));

    // verify that a non-blocking iterator does not find any
    // kvs. neither does it do any ios to storage.
    long numopen = testgettickercount(options, no_file_opens);
    long cache_added = testgettickercount(options, block_cache_add);
    iter = db_->newiterator(non_blocking_opts, handles_[1]);
    count = 0;
    for (iter->seektofirst(); iter->valid(); iter->next()) {
      count++;
    }
    assert_eq(count, 0);
    assert_true(iter->status().isincomplete());
    assert_eq(numopen, testgettickercount(options, no_file_opens));
    assert_eq(cache_added, testgettickercount(options, block_cache_add));
    delete iter;

    // read in the specified block via a regular get
    assert_eq(get(1, "a"), "b");

    // verify that we can find it via a non-blocking scan
    numopen = testgettickercount(options, no_file_opens);
    cache_added = testgettickercount(options, block_cache_add);
    iter = db_->newiterator(non_blocking_opts, handles_[1]);
    count = 0;
    for (iter->seektofirst(); iter->valid(); iter->next()) {
      assert_ok(iter->status());
      count++;
    }
    assert_eq(count, 1);
    assert_eq(numopen, testgettickercount(options, no_file_opens));
    assert_eq(cache_added, testgettickercount(options, block_cache_add));
    delete iter;

    // this test verifies block cache behaviors, which is not used by plain
    // table format.
    // exclude khashcuckoo as it does not support iteration currently
  } while (changeoptions(kskipplaintable | kskipnoseektolast |
                         kskiphashcuckoo));
}

// a delete is skipped for key if keymayexist(key) returns false
// tests writebatch consistency and proper delete behaviour
test(dbtest, filterdeletes) {
  do {
    anon::optionsoverride options_override;
    options_override.filter_policy.reset(newbloomfilterpolicy(20));
    options options = currentoptions(options_override);
    options.filter_deletes = true;
    createandreopenwithcf({"pikachu"}, &options);
    writebatch batch;

    batch.delete(handles_[1], "a");
    dbfull()->write(writeoptions(), &batch);
    assert_eq(allentriesfor("a", 1), "[ ]");  // delete skipped
    batch.clear();

    batch.put(handles_[1], "a", "b");
    batch.delete(handles_[1], "a");
    dbfull()->write(writeoptions(), &batch);
    assert_eq(get(1, "a"), "not_found");
    assert_eq(allentriesfor("a", 1), "[ del, b ]");  // delete issued
    batch.clear();

    batch.delete(handles_[1], "c");
    batch.put(handles_[1], "c", "d");
    dbfull()->write(writeoptions(), &batch);
    assert_eq(get(1, "c"), "d");
    assert_eq(allentriesfor("c", 1), "[ d ]");  // delete skipped
    batch.clear();

    assert_ok(flush(1));  // a stray flush

    batch.delete(handles_[1], "c");
    dbfull()->write(writeoptions(), &batch);
    assert_eq(allentriesfor("c", 1), "[ del, d ]");  // delete issued
    batch.clear();
  } while (changecompactoptions());
}


test(dbtest, iterseekbeforeprev) {
  assert_ok(put("a", "b"));
  assert_ok(put("c", "d"));
  dbfull()->flush(flushoptions());
  assert_ok(put("0", "f"));
  assert_ok(put("1", "h"));
  dbfull()->flush(flushoptions());
  assert_ok(put("2", "j"));
  auto iter = db_->newiterator(readoptions());
  iter->seek(slice("c"));
  iter->prev();
  iter->seek(slice("a"));
  iter->prev();
  delete iter;
}

namespace {
std::string makelongkey(size_t length, char c) {
  return std::string(length, c);
}
}  // namespace

test(dbtest, iterlongkeys) {
  assert_ok(put(makelongkey(20, 0), "0"));
  assert_ok(put(makelongkey(32, 2), "2"));
  assert_ok(put("a", "b"));
  dbfull()->flush(flushoptions());
  assert_ok(put(makelongkey(50, 1), "1"));
  assert_ok(put(makelongkey(127, 3), "3"));
  assert_ok(put(makelongkey(64, 4), "4"));
  auto iter = db_->newiterator(readoptions());

  // create a key that needs to be skipped for seq too new
  iter->seek(makelongkey(20, 0));
  assert_eq(iterstatus(iter), makelongkey(20, 0) + "->0");
  iter->next();
  assert_eq(iterstatus(iter), makelongkey(50, 1) + "->1");
  iter->next();
  assert_eq(iterstatus(iter), makelongkey(32, 2) + "->2");
  iter->next();
  assert_eq(iterstatus(iter), makelongkey(127, 3) + "->3");
  iter->next();
  assert_eq(iterstatus(iter), makelongkey(64, 4) + "->4");
  delete iter;

  iter = db_->newiterator(readoptions());
  iter->seek(makelongkey(50, 1));
  assert_eq(iterstatus(iter), makelongkey(50, 1) + "->1");
  iter->next();
  assert_eq(iterstatus(iter), makelongkey(32, 2) + "->2");
  iter->next();
  assert_eq(iterstatus(iter), makelongkey(127, 3) + "->3");
  delete iter;
}


test(dbtest, iternextwithnewerseq) {
  assert_ok(put("0", "0"));
  dbfull()->flush(flushoptions());
  assert_ok(put("a", "b"));
  assert_ok(put("c", "d"));
  assert_ok(put("d", "e"));
  auto iter = db_->newiterator(readoptions());

  // create a key that needs to be skipped for seq too new
  for (uint64_t i = 0; i < last_options_.max_sequential_skip_in_iterations + 1;
       i++) {
    assert_ok(put("b", "f"));
  }

  iter->seek(slice("a"));
  assert_eq(iterstatus(iter), "a->b");
  iter->next();
  assert_eq(iterstatus(iter), "c->d");
  delete iter;
}

test(dbtest, iterprevwithnewerseq) {
  assert_ok(put("0", "0"));
  dbfull()->flush(flushoptions());
  assert_ok(put("a", "b"));
  assert_ok(put("c", "d"));
  assert_ok(put("d", "e"));
  auto iter = db_->newiterator(readoptions());

  // create a key that needs to be skipped for seq too new
  for (uint64_t i = 0; i < last_options_.max_sequential_skip_in_iterations + 1;
       i++) {
    assert_ok(put("b", "f"));
  }

  iter->seek(slice("d"));
  assert_eq(iterstatus(iter), "d->e");
  iter->prev();
  assert_eq(iterstatus(iter), "c->d");
  iter->prev();
  assert_eq(iterstatus(iter), "a->b");

  iter->prev();
  delete iter;
}

test(dbtest, iterprevwithnewerseq2) {
  assert_ok(put("0", "0"));
  dbfull()->flush(flushoptions());
  assert_ok(put("a", "b"));
  assert_ok(put("c", "d"));
  assert_ok(put("d", "e"));
  auto iter = db_->newiterator(readoptions());
  iter->seek(slice("c"));
  assert_eq(iterstatus(iter), "c->d");

  // create a key that needs to be skipped for seq too new
  for (uint64_t i = 0; i < last_options_.max_sequential_skip_in_iterations + 1;
      i++) {
    assert_ok(put("b", "f"));
  }

  iter->prev();
  assert_eq(iterstatus(iter), "a->b");

  iter->prev();
  delete iter;
}

test(dbtest, iterempty) {
  do {
    createandreopenwithcf({"pikachu"});
    iterator* iter = db_->newiterator(readoptions(), handles_[1]);

    iter->seektofirst();
    assert_eq(iterstatus(iter), "(invalid)");

    iter->seektolast();
    assert_eq(iterstatus(iter), "(invalid)");

    iter->seek("foo");
    assert_eq(iterstatus(iter), "(invalid)");

    delete iter;
  } while (changecompactoptions());
}

test(dbtest, itersingle) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "a", "va"));
    iterator* iter = db_->newiterator(readoptions(), handles_[1]);

    iter->seektofirst();
    assert_eq(iterstatus(iter), "a->va");
    iter->next();
    assert_eq(iterstatus(iter), "(invalid)");
    iter->seektofirst();
    assert_eq(iterstatus(iter), "a->va");
    iter->prev();
    assert_eq(iterstatus(iter), "(invalid)");

    iter->seektolast();
    assert_eq(iterstatus(iter), "a->va");
    iter->next();
    assert_eq(iterstatus(iter), "(invalid)");
    iter->seektolast();
    assert_eq(iterstatus(iter), "a->va");
    iter->prev();
    assert_eq(iterstatus(iter), "(invalid)");

    iter->seek("");
    assert_eq(iterstatus(iter), "a->va");
    iter->next();
    assert_eq(iterstatus(iter), "(invalid)");

    iter->seek("a");
    assert_eq(iterstatus(iter), "a->va");
    iter->next();
    assert_eq(iterstatus(iter), "(invalid)");

    iter->seek("b");
    assert_eq(iterstatus(iter), "(invalid)");

    delete iter;
  } while (changecompactoptions());
}

test(dbtest, itermulti) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "a", "va"));
    assert_ok(put(1, "b", "vb"));
    assert_ok(put(1, "c", "vc"));
    iterator* iter = db_->newiterator(readoptions(), handles_[1]);

    iter->seektofirst();
    assert_eq(iterstatus(iter), "a->va");
    iter->next();
    assert_eq(iterstatus(iter), "b->vb");
    iter->next();
    assert_eq(iterstatus(iter), "c->vc");
    iter->next();
    assert_eq(iterstatus(iter), "(invalid)");
    iter->seektofirst();
    assert_eq(iterstatus(iter), "a->va");
    iter->prev();
    assert_eq(iterstatus(iter), "(invalid)");

    iter->seektolast();
    assert_eq(iterstatus(iter), "c->vc");
    iter->prev();
    assert_eq(iterstatus(iter), "b->vb");
    iter->prev();
    assert_eq(iterstatus(iter), "a->va");
    iter->prev();
    assert_eq(iterstatus(iter), "(invalid)");
    iter->seektolast();
    assert_eq(iterstatus(iter), "c->vc");
    iter->next();
    assert_eq(iterstatus(iter), "(invalid)");

    iter->seek("");
    assert_eq(iterstatus(iter), "a->va");
    iter->seek("a");
    assert_eq(iterstatus(iter), "a->va");
    iter->seek("ax");
    assert_eq(iterstatus(iter), "b->vb");

    iter->seek("b");
    assert_eq(iterstatus(iter), "b->vb");
    iter->seek("z");
    assert_eq(iterstatus(iter), "(invalid)");

    // switch from reverse to forward
    iter->seektolast();
    iter->prev();
    iter->prev();
    iter->next();
    assert_eq(iterstatus(iter), "b->vb");

    // switch from forward to reverse
    iter->seektofirst();
    iter->next();
    iter->next();
    iter->prev();
    assert_eq(iterstatus(iter), "b->vb");

    // make sure iter stays at snapshot
    assert_ok(put(1, "a", "va2"));
    assert_ok(put(1, "a2", "va3"));
    assert_ok(put(1, "b", "vb2"));
    assert_ok(put(1, "c", "vc2"));
    assert_ok(delete(1, "b"));
    iter->seektofirst();
    assert_eq(iterstatus(iter), "a->va");
    iter->next();
    assert_eq(iterstatus(iter), "b->vb");
    iter->next();
    assert_eq(iterstatus(iter), "c->vc");
    iter->next();
    assert_eq(iterstatus(iter), "(invalid)");
    iter->seektolast();
    assert_eq(iterstatus(iter), "c->vc");
    iter->prev();
    assert_eq(iterstatus(iter), "b->vb");
    iter->prev();
    assert_eq(iterstatus(iter), "a->va");
    iter->prev();
    assert_eq(iterstatus(iter), "(invalid)");

    delete iter;
  } while (changecompactoptions());
}

// check that we can skip over a run of user keys
// by using reseek rather than sequential scan
test(dbtest, iterreseek) {
  options options = currentoptions();
  options.max_sequential_skip_in_iterations = 3;
  options.create_if_missing = true;
  options.statistics = rocksdb::createdbstatistics();
  destroyandreopen(&options);
  createandreopenwithcf({"pikachu"}, &options);

  // insert two keys with same userkey and verify that
  // reseek is not invoked. for each of these test cases,
  // verify that we can find the next key "b".
  assert_ok(put(1, "a", "one"));
  assert_ok(put(1, "a", "two"));
  assert_ok(put(1, "b", "bone"));
  iterator* iter = db_->newiterator(readoptions(), handles_[1]);
  iter->seektofirst();
  assert_eq(testgettickercount(options, number_of_reseeks_in_iteration), 0);
  assert_eq(iterstatus(iter), "a->two");
  iter->next();
  assert_eq(testgettickercount(options, number_of_reseeks_in_iteration), 0);
  assert_eq(iterstatus(iter), "b->bone");
  delete iter;

  // insert a total of three keys with same userkey and verify
  // that reseek is still not invoked.
  assert_ok(put(1, "a", "three"));
  iter = db_->newiterator(readoptions(), handles_[1]);
  iter->seektofirst();
  assert_eq(iterstatus(iter), "a->three");
  iter->next();
  assert_eq(testgettickercount(options, number_of_reseeks_in_iteration), 0);
  assert_eq(iterstatus(iter), "b->bone");
  delete iter;

  // insert a total of four keys with same userkey and verify
  // that reseek is invoked.
  assert_ok(put(1, "a", "four"));
  iter = db_->newiterator(readoptions(), handles_[1]);
  iter->seektofirst();
  assert_eq(iterstatus(iter), "a->four");
  assert_eq(testgettickercount(options, number_of_reseeks_in_iteration), 0);
  iter->next();
  assert_eq(testgettickercount(options, number_of_reseeks_in_iteration), 1);
  assert_eq(iterstatus(iter), "b->bone");
  delete iter;

  // testing reverse iterator
  // at this point, we have three versions of "a" and one version of "b".
  // the reseek statistics is already at 1.
  int num_reseeks =
      (int)testgettickercount(options, number_of_reseeks_in_iteration);

  // insert another version of b and assert that reseek is not invoked
  assert_ok(put(1, "b", "btwo"));
  iter = db_->newiterator(readoptions(), handles_[1]);
  iter->seektolast();
  assert_eq(iterstatus(iter), "b->btwo");
  assert_eq(testgettickercount(options, number_of_reseeks_in_iteration),
            num_reseeks);
  iter->prev();
  assert_eq(testgettickercount(options, number_of_reseeks_in_iteration),
            num_reseeks + 1);
  assert_eq(iterstatus(iter), "a->four");
  delete iter;

  // insert two more versions of b. this makes a total of 4 versions
  // of b and 4 versions of a.
  assert_ok(put(1, "b", "bthree"));
  assert_ok(put(1, "b", "bfour"));
  iter = db_->newiterator(readoptions(), handles_[1]);
  iter->seektolast();
  assert_eq(iterstatus(iter), "b->bfour");
  assert_eq(testgettickercount(options, number_of_reseeks_in_iteration),
            num_reseeks + 2);
  iter->prev();

  // the previous prev call should have invoked reseek
  assert_eq(testgettickercount(options, number_of_reseeks_in_iteration),
            num_reseeks + 3);
  assert_eq(iterstatus(iter), "a->four");
  delete iter;
}

test(dbtest, itersmallandlargemix) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "a", "va"));
    assert_ok(put(1, "b", std::string(100000, 'b')));
    assert_ok(put(1, "c", "vc"));
    assert_ok(put(1, "d", std::string(100000, 'd')));
    assert_ok(put(1, "e", std::string(100000, 'e')));

    iterator* iter = db_->newiterator(readoptions(), handles_[1]);

    iter->seektofirst();
    assert_eq(iterstatus(iter), "a->va");
    iter->next();
    assert_eq(iterstatus(iter), "b->" + std::string(100000, 'b'));
    iter->next();
    assert_eq(iterstatus(iter), "c->vc");
    iter->next();
    assert_eq(iterstatus(iter), "d->" + std::string(100000, 'd'));
    iter->next();
    assert_eq(iterstatus(iter), "e->" + std::string(100000, 'e'));
    iter->next();
    assert_eq(iterstatus(iter), "(invalid)");

    iter->seektolast();
    assert_eq(iterstatus(iter), "e->" + std::string(100000, 'e'));
    iter->prev();
    assert_eq(iterstatus(iter), "d->" + std::string(100000, 'd'));
    iter->prev();
    assert_eq(iterstatus(iter), "c->vc");
    iter->prev();
    assert_eq(iterstatus(iter), "b->" + std::string(100000, 'b'));
    iter->prev();
    assert_eq(iterstatus(iter), "a->va");
    iter->prev();
    assert_eq(iterstatus(iter), "(invalid)");

    delete iter;
  } while (changecompactoptions());
}

test(dbtest, itermultiwithdelete) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "ka", "va"));
    assert_ok(put(1, "kb", "vb"));
    assert_ok(put(1, "kc", "vc"));
    assert_ok(delete(1, "kb"));
    assert_eq("not_found", get(1, "kb"));

    iterator* iter = db_->newiterator(readoptions(), handles_[1]);
    iter->seek("kc");
    assert_eq(iterstatus(iter), "kc->vc");
    if (!currentoptions().merge_operator) {
      // todo: merge operator does not support backward iteration yet
      if (kplaintableallbytesprefix != option_config_&&
          kblockbasedtablewithwholekeyhashindex != option_config_ &&
          khashlinklist != option_config_) {
        iter->prev();
        assert_eq(iterstatus(iter), "ka->va");
      }
    }
    delete iter;
  } while (changeoptions());
}

test(dbtest, iterprevmaxskip) {
  do {
    createandreopenwithcf({"pikachu"});
    for (int i = 0; i < 2; i++) {
      assert_ok(put(1, "key1", "v1"));
      assert_ok(put(1, "key2", "v2"));
      assert_ok(put(1, "key3", "v3"));
      assert_ok(put(1, "key4", "v4"));
      assert_ok(put(1, "key5", "v5"));
    }

    verifyiterlast("key5->v5", 1);

    assert_ok(delete(1, "key5"));
    verifyiterlast("key4->v4", 1);

    assert_ok(delete(1, "key4"));
    verifyiterlast("key3->v3", 1);

    assert_ok(delete(1, "key3"));
    verifyiterlast("key2->v2", 1);

    assert_ok(delete(1, "key2"));
    verifyiterlast("key1->v1", 1);

    assert_ok(delete(1, "key1"));
    verifyiterlast("(invalid)", 1);
  } while (changeoptions(kskipmergeput | kskipnoseektolast));
}

test(dbtest, iterwithsnapshot) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "key1", "val1"));
    assert_ok(put(1, "key2", "val2"));
    assert_ok(put(1, "key3", "val3"));
    assert_ok(put(1, "key4", "val4"));
    assert_ok(put(1, "key5", "val5"));

    const snapshot *snapshot = db_->getsnapshot();
    readoptions options;
    options.snapshot = snapshot;
    iterator* iter = db_->newiterator(options, handles_[1]);

    // put more values after the snapshot
    assert_ok(put(1, "key100", "val100"));
    assert_ok(put(1, "key101", "val101"));

    iter->seek("key5");
    assert_eq(iterstatus(iter), "key5->val5");
    if (!currentoptions().merge_operator) {
      // todo: merge operator does not support backward iteration yet
      if (kplaintableallbytesprefix != option_config_&&
        kblockbasedtablewithwholekeyhashindex != option_config_ &&
        khashlinklist != option_config_) {
        iter->prev();
        assert_eq(iterstatus(iter), "key4->val4");
        iter->prev();
        assert_eq(iterstatus(iter), "key3->val3");

        iter->next();
        assert_eq(iterstatus(iter), "key4->val4");
        iter->next();
        assert_eq(iterstatus(iter), "key5->val5");
      }
      iter->next();
      assert_true(!iter->valid());
    }
    db_->releasesnapshot(snapshot);
    delete iter;
    // skip as hashcuckoorep does not support snapshot
  } while (changeoptions(kskiphashcuckoo));
}

test(dbtest, recover) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "foo", "v1"));
    assert_ok(put(1, "baz", "v5"));

    reopenwithcolumnfamilies({"default", "pikachu"});
    assert_eq("v1", get(1, "foo"));

    assert_eq("v1", get(1, "foo"));
    assert_eq("v5", get(1, "baz"));
    assert_ok(put(1, "bar", "v2"));
    assert_ok(put(1, "foo", "v3"));

    reopenwithcolumnfamilies({"default", "pikachu"});
    assert_eq("v3", get(1, "foo"));
    assert_ok(put(1, "foo", "v4"));
    assert_eq("v4", get(1, "foo"));
    assert_eq("v2", get(1, "bar"));
    assert_eq("v5", get(1, "baz"));
  } while (changeoptions());
}

test(dbtest, recoverwithtablehandle) {
  do {
    options options;
    options.create_if_missing = true;
    options.write_buffer_size = 100;
    options.disable_auto_compactions = true;
    options = currentoptions(options);
    destroyandreopen(&options);
    createandreopenwithcf({"pikachu"}, &options);

    assert_ok(put(1, "foo", "v1"));
    assert_ok(put(1, "bar", "v2"));
    assert_ok(flush(1));
    assert_ok(put(1, "foo", "v3"));
    assert_ok(put(1, "bar", "v4"));
    assert_ok(flush(1));
    assert_ok(put(1, "big", std::string(100, 'a')));
    reopenwithcolumnfamilies({"default", "pikachu"});

    std::vector<std::vector<filemetadata>> files;
    dbfull()->test_getfilesmetadata(handles_[1], &files);
    int total_files = 0;
    for (const auto& level : files) {
      total_files += level.size();
    }
    assert_eq(total_files, 3);
    for (const auto& level : files) {
      for (const auto& file : level) {
        if (kinfinitemaxopenfiles == option_config_) {
          assert_true(file.table_reader_handle != nullptr);
        } else {
          assert_true(file.table_reader_handle == nullptr);
        }
      }
    }
  } while (changeoptions());
}

test(dbtest, ignorerecoveredlog) {
  std::string backup_logs = dbname_ + "/backup_logs";

  // delete old files in backup_logs directory
  env_->createdirifmissing(backup_logs);
  std::vector<std::string> old_files;
  env_->getchildren(backup_logs, &old_files);
  for (auto& file : old_files) {
    if (file != "." && file != "..") {
      env_->deletefile(backup_logs + "/" + file);
    }
  }

  do {
    options options = currentoptions();
    options.create_if_missing = true;
    options.merge_operator = mergeoperators::createuint64addoperator();
    options.wal_dir = dbname_ + "/logs";
    destroyandreopen(&options);

    // fill up the db
    std::string one, two;
    putfixed64(&one, 1);
    putfixed64(&two, 2);
    assert_ok(db_->merge(writeoptions(), slice("foo"), slice(one)));
    assert_ok(db_->merge(writeoptions(), slice("foo"), slice(one)));
    assert_ok(db_->merge(writeoptions(), slice("bar"), slice(one)));

    // copy the logs to backup
    std::vector<std::string> logs;
    env_->getchildren(options.wal_dir, &logs);
    for (auto& log : logs) {
      if (log != ".." && log != ".") {
        copyfile(options.wal_dir + "/" + log, backup_logs + "/" + log);
      }
    }

    // recover the db
    reopen(&options);
    assert_eq(two, get("foo"));
    assert_eq(one, get("bar"));
    close();

    // copy the logs from backup back to wal dir
    for (auto& log : logs) {
      if (log != ".." && log != ".") {
        copyfile(backup_logs + "/" + log, options.wal_dir + "/" + log);
      }
    }
    // this should ignore the log files, recovery should not happen again
    // if the recovery happens, the same merge operator would be called twice,
    // leading to incorrect results
    reopen(&options);
    assert_eq(two, get("foo"));
    assert_eq(one, get("bar"));
    close();
    destroy(&options);
    reopen(&options);
    close();

    // copy the logs from backup back to wal dir
    env_->createdirifmissing(options.wal_dir);
    for (auto& log : logs) {
      if (log != ".." && log != ".") {
        copyfile(backup_logs + "/" + log, options.wal_dir + "/" + log);
      }
    }
    // assert that we successfully recovered only from logs, even though we
    // destroyed the db
    reopen(&options);
    assert_eq(two, get("foo"));
    assert_eq(one, get("bar"));

    // recovery will fail if db directory doesn't exist.
    destroy(&options);
    // copy the logs from backup back to wal dir
    env_->createdirifmissing(options.wal_dir);
    for (auto& log : logs) {
      if (log != ".." && log != ".") {
        copyfile(backup_logs + "/" + log, options.wal_dir + "/" + log);
        // we won't be needing this file no more
        env_->deletefile(backup_logs + "/" + log);
      }
    }
    status s = tryreopen(&options);
    assert_true(!s.ok());
  } while (changeoptions(kskiphashcuckoo));
}

test(dbtest, rolllog) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "foo", "v1"));
    assert_ok(put(1, "baz", "v5"));

    reopenwithcolumnfamilies({"default", "pikachu"});
    for (int i = 0; i < 10; i++) {
      reopenwithcolumnfamilies({"default", "pikachu"});
    }
    assert_ok(put(1, "foo", "v4"));
    for (int i = 0; i < 10; i++) {
      reopenwithcolumnfamilies({"default", "pikachu"});
    }
  } while (changeoptions());
}

test(dbtest, wal) {
  do {
    createandreopenwithcf({"pikachu"});
    writeoptions writeopt = writeoptions();
    writeopt.disablewal = true;
    assert_ok(dbfull()->put(writeopt, handles_[1], "foo", "v1"));
    assert_ok(dbfull()->put(writeopt, handles_[1], "bar", "v1"));

    reopenwithcolumnfamilies({"default", "pikachu"});
    assert_eq("v1", get(1, "foo"));
    assert_eq("v1", get(1, "bar"));

    writeopt.disablewal = false;
    assert_ok(dbfull()->put(writeopt, handles_[1], "bar", "v2"));
    writeopt.disablewal = true;
    assert_ok(dbfull()->put(writeopt, handles_[1], "foo", "v2"));

    reopenwithcolumnfamilies({"default", "pikachu"});
    // both value's should be present.
    assert_eq("v2", get(1, "bar"));
    assert_eq("v2", get(1, "foo"));

    writeopt.disablewal = true;
    assert_ok(dbfull()->put(writeopt, handles_[1], "bar", "v3"));
    writeopt.disablewal = false;
    assert_ok(dbfull()->put(writeopt, handles_[1], "foo", "v3"));

    reopenwithcolumnfamilies({"default", "pikachu"});
    // again both values should be present.
    assert_eq("v3", get(1, "foo"));
    assert_eq("v3", get(1, "bar"));
  } while (changecompactoptions());
}

test(dbtest, checklock) {
  do {
    db* localdb;
    options options = currentoptions();
    assert_ok(tryreopen(&options));

    // second open should fail
    assert_true(!(db::open(options, dbname_, &localdb)).ok());
  } while (changecompactoptions());
}

test(dbtest, flushmultiplememtable) {
  do {
    options options = currentoptions();
    writeoptions writeopt = writeoptions();
    writeopt.disablewal = true;
    options.max_write_buffer_number = 4;
    options.min_write_buffer_number_to_merge = 3;
    createandreopenwithcf({"pikachu"}, &options);
    assert_ok(dbfull()->put(writeopt, handles_[1], "foo", "v1"));
    assert_ok(flush(1));
    assert_ok(dbfull()->put(writeopt, handles_[1], "bar", "v1"));

    assert_eq("v1", get(1, "foo"));
    assert_eq("v1", get(1, "bar"));
    assert_ok(flush(1));
  } while (changecompactoptions());
}

test(dbtest, numimmutablememtable) {
  do {
    options options = currentoptions();
    writeoptions writeopt = writeoptions();
    writeopt.disablewal = true;
    options.max_write_buffer_number = 4;
    options.min_write_buffer_number_to_merge = 3;
    options.write_buffer_size = 1000000;
    createandreopenwithcf({"pikachu"}, &options);

    std::string big_value(1000000 * 2, 'x');
    std::string num;
    setperflevel(kenabletime);;
    assert_true(getperflevel() == kenabletime);

    assert_ok(dbfull()->put(writeopt, handles_[1], "k1", big_value));
    assert_true(dbfull()->getproperty(handles_[1],
                                      "rocksdb.num-immutable-mem-table", &num));
    assert_eq(num, "0");
    assert_true(dbfull()->getproperty(
        handles_[1], "rocksdb.num-entries-active-mem-table", &num));
    assert_eq(num, "1");
    perf_context.reset();
    get(1, "k1");
    assert_eq(1, (int) perf_context.get_from_memtable_count);

    assert_ok(dbfull()->put(writeopt, handles_[1], "k2", big_value));
    assert_true(dbfull()->getproperty(handles_[1],
                                      "rocksdb.num-immutable-mem-table", &num));
    assert_eq(num, "1");
    assert_true(dbfull()->getproperty(
        handles_[1], "rocksdb.num-entries-active-mem-table", &num));
    assert_eq(num, "1");
    assert_true(dbfull()->getproperty(
        handles_[1], "rocksdb.num-entries-imm-mem-tables", &num));
    assert_eq(num, "1");

    perf_context.reset();
    get(1, "k1");
    assert_eq(2, (int) perf_context.get_from_memtable_count);
    perf_context.reset();
    get(1, "k2");
    assert_eq(1, (int) perf_context.get_from_memtable_count);

    assert_ok(dbfull()->put(writeopt, handles_[1], "k3", big_value));
    assert_true(dbfull()->getproperty(
        handles_[1], "rocksdb.cur-size-active-mem-table", &num));
    assert_true(dbfull()->getproperty(handles_[1],
                                      "rocksdb.num-immutable-mem-table", &num));
    assert_eq(num, "2");
    assert_true(dbfull()->getproperty(
        handles_[1], "rocksdb.num-entries-active-mem-table", &num));
    assert_eq(num, "1");
    assert_true(dbfull()->getproperty(
        handles_[1], "rocksdb.num-entries-imm-mem-tables", &num));
    assert_eq(num, "2");
    perf_context.reset();
    get(1, "k2");
    assert_eq(2, (int) perf_context.get_from_memtable_count);
    perf_context.reset();
    get(1, "k3");
    assert_eq(1, (int) perf_context.get_from_memtable_count);
    perf_context.reset();
    get(1, "k1");
    assert_eq(3, (int) perf_context.get_from_memtable_count);

    assert_ok(flush(1));
    assert_true(dbfull()->getproperty(handles_[1],
                                      "rocksdb.num-immutable-mem-table", &num));
    assert_eq(num, "0");
    assert_true(dbfull()->getproperty(
        handles_[1], "rocksdb.cur-size-active-mem-table", &num));
    // "200" is the size of the metadata of an empty skiplist, this would
    // break if we change the default skiplist implementation
    assert_eq(num, "200");
    setperflevel(kdisable);
    assert_true(getperflevel() == kdisable);
  } while (changecompactoptions());
}

class sleepingbackgroundtask {
 public:
  sleepingbackgroundtask()
      : bg_cv_(&mutex_), should_sleep_(true), done_with_sleep_(false) {}
  void dosleep() {
    mutexlock l(&mutex_);
    while (should_sleep_) {
      bg_cv_.wait();
    }
    done_with_sleep_ = true;
    bg_cv_.signalall();
  }
  void wakeup() {
    mutexlock l(&mutex_);
    should_sleep_ = false;
    bg_cv_.signalall();
  }
  void waituntildone() {
    mutexlock l(&mutex_);
    while (!done_with_sleep_) {
      bg_cv_.wait();
    }
  }

  static void dosleeptask(void* arg) {
    reinterpret_cast<sleepingbackgroundtask*>(arg)->dosleep();
  }

 private:
  port::mutex mutex_;
  port::condvar bg_cv_;  // signalled when background work finishes
  bool should_sleep_;
  bool done_with_sleep_;
};

test(dbtest, getproperty) {
  // set sizes to both background thread pool to be 1 and block them.
  env_->setbackgroundthreads(1, env::high);
  env_->setbackgroundthreads(1, env::low);
  sleepingbackgroundtask sleeping_task_low;
  env_->schedule(&sleepingbackgroundtask::dosleeptask, &sleeping_task_low,
                 env::priority::low);
  sleepingbackgroundtask sleeping_task_high;
  env_->schedule(&sleepingbackgroundtask::dosleeptask, &sleeping_task_high,
                 env::priority::high);

  options options = currentoptions();
  writeoptions writeopt = writeoptions();
  writeopt.disablewal = true;
  options.compaction_style = kcompactionstyleuniversal;
  options.level0_file_num_compaction_trigger = 1;
  options.compaction_options_universal.size_ratio = 50;
  options.max_background_compactions = 1;
  options.max_background_flushes = 1;
  options.max_write_buffer_number = 10;
  options.min_write_buffer_number_to_merge = 1;
  options.write_buffer_size = 1000000;
  reopen(&options);

  std::string big_value(1000000 * 2, 'x');
  std::string num;
  uint64_t int_num;
  setperflevel(kenabletime);

  assert_true(
      dbfull()->getintproperty("rocksdb.estimate-table-readers-mem", &int_num));
  assert_eq(int_num, 0u);

  assert_ok(dbfull()->put(writeopt, "k1", big_value));
  assert_true(dbfull()->getproperty("rocksdb.num-immutable-mem-table", &num));
  assert_eq(num, "0");
  assert_true(dbfull()->getproperty("rocksdb.mem-table-flush-pending", &num));
  assert_eq(num, "0");
  assert_true(dbfull()->getproperty("rocksdb.compaction-pending", &num));
  assert_eq(num, "0");
  assert_true(dbfull()->getproperty("rocksdb.estimate-num-keys", &num));
  assert_eq(num, "1");
  perf_context.reset();

  assert_ok(dbfull()->put(writeopt, "k2", big_value));
  assert_true(dbfull()->getproperty("rocksdb.num-immutable-mem-table", &num));
  assert_eq(num, "1");
  assert_ok(dbfull()->delete(writeopt, "k-non-existing"));
  assert_ok(dbfull()->put(writeopt, "k3", big_value));
  assert_true(dbfull()->getproperty("rocksdb.num-immutable-mem-table", &num));
  assert_eq(num, "2");
  assert_true(dbfull()->getproperty("rocksdb.mem-table-flush-pending", &num));
  assert_eq(num, "1");
  assert_true(dbfull()->getproperty("rocksdb.compaction-pending", &num));
  assert_eq(num, "0");
  assert_true(dbfull()->getproperty("rocksdb.estimate-num-keys", &num));
  assert_eq(num, "4");
  // verify the same set of properties through getintproperty
  assert_true(
      dbfull()->getintproperty("rocksdb.num-immutable-mem-table", &int_num));
  assert_eq(int_num, 2u);
  assert_true(
      dbfull()->getintproperty("rocksdb.mem-table-flush-pending", &int_num));
  assert_eq(int_num, 1u);
  assert_true(dbfull()->getintproperty("rocksdb.compaction-pending", &int_num));
  assert_eq(int_num, 0u);
  assert_true(dbfull()->getintproperty("rocksdb.estimate-num-keys", &int_num));
  assert_eq(int_num, 4u);

  assert_true(
      dbfull()->getintproperty("rocksdb.estimate-table-readers-mem", &int_num));
  assert_eq(int_num, 0u);

  sleeping_task_high.wakeup();
  sleeping_task_high.waituntildone();
  dbfull()->test_waitforflushmemtable();

  assert_ok(dbfull()->put(writeopt, "k4", big_value));
  assert_ok(dbfull()->put(writeopt, "k5", big_value));
  dbfull()->test_waitforflushmemtable();
  assert_true(dbfull()->getproperty("rocksdb.mem-table-flush-pending", &num));
  assert_eq(num, "0");
  assert_true(dbfull()->getproperty("rocksdb.compaction-pending", &num));
  assert_eq(num, "1");
  assert_true(dbfull()->getproperty("rocksdb.estimate-num-keys", &num));
  assert_eq(num, "4");

  assert_true(
      dbfull()->getintproperty("rocksdb.estimate-table-readers-mem", &int_num));
  assert_gt(int_num, 0u);

  sleeping_task_low.wakeup();
  sleeping_task_low.waituntildone();

  dbfull()->test_waitforflushmemtable();
  options.max_open_files = 10;
  reopen(&options);
  // after reopening, no table reader is loaded, so no memory for table readers
  assert_true(
      dbfull()->getintproperty("rocksdb.estimate-table-readers-mem", &int_num));
  assert_eq(int_num, 0u);
  assert_true(dbfull()->getintproperty("rocksdb.estimate-num-keys", &int_num));
  assert_gt(int_num, 0u);

  // after reading a key, at least one table reader is loaded.
  get("k5");
  assert_true(
      dbfull()->getintproperty("rocksdb.estimate-table-readers-mem", &int_num));
  assert_gt(int_num, 0u);
}

test(dbtest, flush) {
  do {
    createandreopenwithcf({"pikachu"});
    writeoptions writeopt = writeoptions();
    writeopt.disablewal = true;
    setperflevel(kenabletime);;
    assert_ok(dbfull()->put(writeopt, handles_[1], "foo", "v1"));
    // this will now also flush the last 2 writes
    assert_ok(flush(1));
    assert_ok(dbfull()->put(writeopt, handles_[1], "bar", "v1"));

    perf_context.reset();
    get(1, "foo");
    assert_true((int) perf_context.get_from_output_files_time > 0);

    reopenwithcolumnfamilies({"default", "pikachu"});
    assert_eq("v1", get(1, "foo"));
    assert_eq("v1", get(1, "bar"));

    writeopt.disablewal = true;
    assert_ok(dbfull()->put(writeopt, handles_[1], "bar", "v2"));
    assert_ok(dbfull()->put(writeopt, handles_[1], "foo", "v2"));
    assert_ok(flush(1));

    reopenwithcolumnfamilies({"default", "pikachu"});
    assert_eq("v2", get(1, "bar"));
    perf_context.reset();
    assert_eq("v2", get(1, "foo"));
    assert_true((int) perf_context.get_from_output_files_time > 0);

    writeopt.disablewal = false;
    assert_ok(dbfull()->put(writeopt, handles_[1], "bar", "v3"));
    assert_ok(dbfull()->put(writeopt, handles_[1], "foo", "v3"));
    assert_ok(flush(1));

    reopenwithcolumnfamilies({"default", "pikachu"});
    // 'foo' should be there because its put
    // has wal enabled.
    assert_eq("v3", get(1, "foo"));
    assert_eq("v3", get(1, "bar"));

    setperflevel(kdisable);
  } while (changecompactoptions());
}

test(dbtest, recoverywithemptylog) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "foo", "v1"));
    assert_ok(put(1, "foo", "v2"));
    reopenwithcolumnfamilies({"default", "pikachu"});
    reopenwithcolumnfamilies({"default", "pikachu"});
    assert_ok(put(1, "foo", "v3"));
    reopenwithcolumnfamilies({"default", "pikachu"});
    assert_eq("v3", get(1, "foo"));
  } while (changeoptions());
}

// check that writes done during a memtable compaction are recovered
// if the database is shutdown during the memtable compaction.
test(dbtest, recoverduringmemtablecompaction) {
  do {
    options options;
    options.env = env_;
    options.write_buffer_size = 1000000;
    options = currentoptions(options);
    createandreopenwithcf({"pikachu"}, &options);

    // trigger a long memtable compaction and reopen the database during it
    assert_ok(put(1, "foo", "v1"));  // goes to 1st log file
    assert_ok(put(1, "big1", std::string(10000000, 'x')));  // fills memtable
    assert_ok(put(1, "big2", std::string(1000, 'y')));  // triggers compaction
    assert_ok(put(1, "bar", "v2"));                     // goes to new log file

    reopenwithcolumnfamilies({"default", "pikachu"}, &options);
    assert_eq("v1", get(1, "foo"));
    assert_eq("v2", get(1, "bar"));
    assert_eq(std::string(10000000, 'x'), get(1, "big1"));
    assert_eq(std::string(1000, 'y'), get(1, "big2"));
  } while (changeoptions());
}

test(dbtest, minorcompactionshappen) {
  do {
    options options;
    options.write_buffer_size = 10000;
    options = currentoptions(options);
    createandreopenwithcf({"pikachu"}, &options);

    const int n = 500;

    int starting_num_tables = totaltablefiles(1);
    for (int i = 0; i < n; i++) {
      assert_ok(put(1, key(i), key(i) + std::string(1000, 'v')));
    }
    int ending_num_tables = totaltablefiles(1);
    assert_gt(ending_num_tables, starting_num_tables);

    for (int i = 0; i < n; i++) {
      assert_eq(key(i) + std::string(1000, 'v'), get(1, key(i)));
    }

    reopenwithcolumnfamilies({"default", "pikachu"}, &options);

    for (int i = 0; i < n; i++) {
      assert_eq(key(i) + std::string(1000, 'v'), get(1, key(i)));
    }
  } while (changecompactoptions());
}

test(dbtest, manifestrollover) {
  do {
    options options;
    options.max_manifest_file_size = 10 ;  // 10 bytes
    options = currentoptions(options);
    createandreopenwithcf({"pikachu"}, &options);
    {
      assert_ok(put(1, "manifest_key1", std::string(1000, '1')));
      assert_ok(put(1, "manifest_key2", std::string(1000, '2')));
      assert_ok(put(1, "manifest_key3", std::string(1000, '3')));
      uint64_t manifest_before_flush = dbfull()->test_current_manifest_fileno();
      assert_ok(flush(1));  // this should trigger logandapply.
      uint64_t manifest_after_flush = dbfull()->test_current_manifest_fileno();
      assert_gt(manifest_after_flush, manifest_before_flush);
      reopenwithcolumnfamilies({"default", "pikachu"}, &options);
      assert_gt(dbfull()->test_current_manifest_fileno(), manifest_after_flush);
      // check if a new manifest file got inserted or not.
      assert_eq(std::string(1000, '1'), get(1, "manifest_key1"));
      assert_eq(std::string(1000, '2'), get(1, "manifest_key2"));
      assert_eq(std::string(1000, '3'), get(1, "manifest_key3"));
    }
  } while (changecompactoptions());
}

test(dbtest, identityacrossrestarts) {
  do {
    std::string id1;
    assert_ok(db_->getdbidentity(id1));

    options options = currentoptions();
    reopen(&options);
    std::string id2;
    assert_ok(db_->getdbidentity(id2));
    // id1 should match id2 because identity was not regenerated
    assert_eq(id1.compare(id2), 0);

    std::string idfilename = identityfilename(dbname_);
    assert_ok(env_->deletefile(idfilename));
    reopen(&options);
    std::string id3;
    assert_ok(db_->getdbidentity(id3));
    // id1 should not match id3 because identity was regenerated
    assert_ne(id1.compare(id3), 0);
  } while (changecompactoptions());
}

test(dbtest, recoverwithlargelog) {
  do {
    {
      options options = currentoptions();
      createandreopenwithcf({"pikachu"}, &options);
      assert_ok(put(1, "big1", std::string(200000, '1')));
      assert_ok(put(1, "big2", std::string(200000, '2')));
      assert_ok(put(1, "small3", std::string(10, '3')));
      assert_ok(put(1, "small4", std::string(10, '4')));
      assert_eq(numtablefilesatlevel(0, 1), 0);
    }

    // make sure that if we re-open with a small write buffer size that
    // we flush table files in the middle of a large log file.
    options options;
    options.write_buffer_size = 100000;
    options = currentoptions(options);
    reopenwithcolumnfamilies({"default", "pikachu"}, &options);
    assert_eq(numtablefilesatlevel(0, 1), 3);
    assert_eq(std::string(200000, '1'), get(1, "big1"));
    assert_eq(std::string(200000, '2'), get(1, "big2"));
    assert_eq(std::string(10, '3'), get(1, "small3"));
    assert_eq(std::string(10, '4'), get(1, "small4"));
    assert_gt(numtablefilesatlevel(0, 1), 1);
  } while (changecompactoptions());
}

test(dbtest, compactionsgeneratemultiplefiles) {
  options options;
  options.write_buffer_size = 100000000;        // large write buffer
  options = currentoptions(options);
  createandreopenwithcf({"pikachu"}, &options);

  random rnd(301);

  // write 8mb (80 values, each 100k)
  assert_eq(numtablefilesatlevel(0, 1), 0);
  std::vector<std::string> values;
  for (int i = 0; i < 80; i++) {
    values.push_back(randomstring(&rnd, 100000));
    assert_ok(put(1, key(i), values[i]));
  }

  // reopening moves updates to level-0
  reopenwithcolumnfamilies({"default", "pikachu"}, &options);
  dbfull()->test_compactrange(0, nullptr, nullptr, handles_[1]);

  assert_eq(numtablefilesatlevel(0, 1), 0);
  assert_gt(numtablefilesatlevel(1, 1), 1);
  for (int i = 0; i < 80; i++) {
    assert_eq(get(1, key(i)), values[i]);
  }
}

test(dbtest, compactiontrigger) {
  options options;
  options.write_buffer_size = 100<<10; //100kb
  options.num_levels = 3;
  options.max_mem_compaction_level = 0;
  options.level0_file_num_compaction_trigger = 3;
  options = currentoptions(options);
  createandreopenwithcf({"pikachu"}, &options);

  random rnd(301);

  for (int num = 0; num < options.level0_file_num_compaction_trigger - 1;
       num++) {
    std::vector<std::string> values;
    // write 120kb (12 values, each 10k)
    for (int i = 0; i < 12; i++) {
      values.push_back(randomstring(&rnd, 10000));
      assert_ok(put(1, key(i), values[i]));
    }
    dbfull()->test_waitforflushmemtable(handles_[1]);
    assert_eq(numtablefilesatlevel(0, 1), num + 1);
  }

  //generate one more file in level-0, and should trigger level-0 compaction
  std::vector<std::string> values;
  for (int i = 0; i < 12; i++) {
    values.push_back(randomstring(&rnd, 10000));
    assert_ok(put(1, key(i), values[i]));
  }
  dbfull()->test_waitforcompact();

  assert_eq(numtablefilesatlevel(0, 1), 0);
  assert_eq(numtablefilesatlevel(1, 1), 1);
}

namespace {
static const int kcdtvaluesize = 1000;
static const int kcdtkeysperbuffer = 4;
static const int kcdtnumlevels = 8;
options deletiontriggeroptions() {
  options options;
  options.compression = knocompression;
  options.write_buffer_size = kcdtkeysperbuffer * (kcdtvaluesize + 24);
  options.min_write_buffer_number_to_merge = 1;
  options.num_levels = kcdtnumlevels;
  options.max_mem_compaction_level = 0;
  options.level0_file_num_compaction_trigger = 1;
  options.target_file_size_base = options.write_buffer_size * 2;
  options.target_file_size_multiplier = 2;
  options.max_bytes_for_level_base =
      options.target_file_size_base * options.target_file_size_multiplier;
  options.max_bytes_for_level_multiplier = 2;
  options.disable_auto_compactions = false;
  return options;
}
}  // anonymous namespace

test(dbtest, compactiondeletiontrigger) {
  options options = deletiontriggeroptions();
  options.create_if_missing = true;

  for (int tid = 0; tid < 2; ++tid) {
    uint64_t db_size[2];

    destroyandreopen(&options);
    random rnd(301);

    const int ktestsize = kcdtkeysperbuffer * 512;
    std::vector<std::string> values;
    for (int k = 0; k < ktestsize; ++k) {
      values.push_back(randomstring(&rnd, kcdtvaluesize));
      assert_ok(put(key(k), values[k]));
    }
    dbfull()->test_waitforflushmemtable();
    dbfull()->test_waitforcompact();
    db_size[0] = size(key(0), key(ktestsize - 1));

    for (int k = 0; k < ktestsize; ++k) {
      assert_ok(delete(key(k)));
    }
    dbfull()->test_waitforflushmemtable();
    dbfull()->test_waitforcompact();
    db_size[1] = size(key(0), key(ktestsize - 1));

    // must have much smaller db size.
    assert_gt(db_size[0] / 3, db_size[1]);

    // repeat the test with universal compaction
    options.compaction_style = kcompactionstyleuniversal;
    options.num_levels = 1;
  }
}

test(dbtest, compactiondeletiontriggerreopen) {
  for (int tid = 0; tid < 2; ++tid) {
    uint64_t db_size[3];
    options options = deletiontriggeroptions();
    options.create_if_missing = true;

    destroyandreopen(&options);
    random rnd(301);

    // round 1 --- insert key/value pairs.
    const int ktestsize = kcdtkeysperbuffer * 512;
    std::vector<std::string> values;
    for (int k = 0; k < ktestsize; ++k) {
      values.push_back(randomstring(&rnd, kcdtvaluesize));
      assert_ok(put(key(k), values[k]));
    }
    dbfull()->test_waitforflushmemtable();
    dbfull()->test_waitforcompact();
    db_size[0] = size(key(0), key(ktestsize - 1));
    close();

    // round 2 --- disable auto-compactions and issue deletions.
    options.create_if_missing = false;
    options.disable_auto_compactions = true;
    reopen(&options);

    for (int k = 0; k < ktestsize; ++k) {
      assert_ok(delete(key(k)));
    }
    db_size[1] = size(key(0), key(ktestsize - 1));
    close();
    // as auto_compaction is off, we shouldn't see too much reduce
    // in db size.
    assert_lt(db_size[0] / 3, db_size[1]);

    // round 3 --- reopen db with auto_compaction on and see if
    // deletion compensation still work.
    options.disable_auto_compactions = false;
    reopen(&options);
    // insert relatively small amount of data to trigger auto compaction.
    for (int k = 0; k < ktestsize / 10; ++k) {
      assert_ok(put(key(k), values[k]));
    }
    dbfull()->test_waitforflushmemtable();
    dbfull()->test_waitforcompact();
    db_size[2] = size(key(0), key(ktestsize - 1));
    // this time we're expecting significant drop in size.
    assert_gt(db_size[0] / 3, db_size[2]);

    // repeat the test with universal compaction
    options.compaction_style = kcompactionstyleuniversal;
    options.num_levels = 1;
  }
}

// this is a static filter used for filtering
// kvs during the compaction process.
static int cfilter_count;
static std::string new_value = "newvalue";

class keepfilter : public compactionfilter {
 public:
  virtual bool filter(int level, const slice& key, const slice& value,
                      std::string* new_value, bool* value_changed) const
      override {
    cfilter_count++;
    return false;
  }

  virtual const char* name() const override { return "keepfilter"; }
};

class deletefilter : public compactionfilter {
 public:
  virtual bool filter(int level, const slice& key, const slice& value,
                      std::string* new_value, bool* value_changed) const
      override {
    cfilter_count++;
    return true;
  }

  virtual const char* name() const override { return "deletefilter"; }
};

class changefilter : public compactionfilter {
 public:
  explicit changefilter() {}

  virtual bool filter(int level, const slice& key, const slice& value,
                      std::string* new_value, bool* value_changed) const
      override {
    assert(new_value != nullptr);
    *new_value = new_value;
    *value_changed = true;
    return false;
  }

  virtual const char* name() const override { return "changefilter"; }
};

class keepfilterfactory : public compactionfilterfactory {
 public:
  explicit keepfilterfactory(bool check_context = false)
      : check_context_(check_context) {}

  virtual std::unique_ptr<compactionfilter> createcompactionfilter(
      const compactionfilter::context& context) override {
    if (check_context_) {
      assert_eq(expect_full_compaction_.load(), context.is_full_compaction);
      assert_eq(expect_manual_compaction_.load(), context.is_manual_compaction);
    }
    return std::unique_ptr<compactionfilter>(new keepfilter());
  }

  virtual const char* name() const override { return "keepfilterfactory"; }
  bool check_context_;
  std::atomic_bool expect_full_compaction_;
  std::atomic_bool expect_manual_compaction_;
};

class deletefilterfactory : public compactionfilterfactory {
 public:
  virtual std::unique_ptr<compactionfilter> createcompactionfilter(
      const compactionfilter::context& context) override {
    if (context.is_manual_compaction) {
      return std::unique_ptr<compactionfilter>(new deletefilter());
    } else {
      return std::unique_ptr<compactionfilter>(nullptr);
    }
  }

  virtual const char* name() const override { return "deletefilterfactory"; }
};

class changefilterfactory : public compactionfilterfactory {
 public:
  explicit changefilterfactory() {}

  virtual std::unique_ptr<compactionfilter> createcompactionfilter(
      const compactionfilter::context& context) override {
    return std::unique_ptr<compactionfilter>(new changefilter());
  }

  virtual const char* name() const override { return "changefilterfactory"; }
};

// todo(kailiu) the tests on universalcompaction has some issues:
//  1. a lot of magic numbers ("11" or "12").
//  2. made assumption on the memtable flush conidtions, which may change from
//     time to time.
test(dbtest, universalcompactiontrigger) {
  options options;
  options.compaction_style = kcompactionstyleuniversal;
  options.write_buffer_size = 100<<10; //100kb
  // trigger compaction if there are >= 4 files
  options.level0_file_num_compaction_trigger = 4;
  keepfilterfactory* filter = new keepfilterfactory(true);
  filter->expect_manual_compaction_.store(false);
  options.compaction_filter_factory.reset(filter);

  options = currentoptions(options);
  createandreopenwithcf({"pikachu"}, &options);

  random rnd(301);
  int key_idx = 0;

  filter->expect_full_compaction_.store(true);
  // stage 1:
  //   generate a set of files at level 0, but don't trigger level-0
  //   compaction.
  for (int num = 0; num < options.level0_file_num_compaction_trigger - 1;
       num++) {
    // write 110kb (11 values, each 10k)
    for (int i = 0; i < 12; i++) {
      assert_ok(put(1, key(key_idx), randomstring(&rnd, 10000)));
      key_idx++;
    }
    dbfull()->test_waitforflushmemtable(handles_[1]);
    assert_eq(numtablefilesatlevel(0, 1), num + 1);
  }

  // generate one more file at level-0, which should trigger level-0
  // compaction.
  for (int i = 0; i < 11; i++) {
    assert_ok(put(1, key(key_idx), randomstring(&rnd, 10000)));
    key_idx++;
  }
  dbfull()->test_waitforcompact();
  // suppose each file flushed from mem table has size 1. now we compact
  // (level0_file_num_compaction_trigger+1)=4 files and should have a big
  // file of size 4.
  assert_eq(numtablefilesatlevel(0, 1), 1);
  for (int i = 1; i < options.num_levels ; i++) {
    assert_eq(numtablefilesatlevel(i, 1), 0);
  }

  // stage 2:
  //   now we have one file at level 0, with size 4. we also have some data in
  //   mem table. let's continue generating new files at level 0, but don't
  //   trigger level-0 compaction.
  //   first, clean up memtable before inserting new data. this will generate
  //   a level-0 file, with size around 0.4 (according to previously written
  //   data amount).
  filter->expect_full_compaction_.store(false);
  assert_ok(flush(1));
  for (int num = 0; num < options.level0_file_num_compaction_trigger - 3;
       num++) {
    // write 110kb (11 values, each 10k)
    for (int i = 0; i < 11; i++) {
      assert_ok(put(1, key(key_idx), randomstring(&rnd, 10000)));
      key_idx++;
    }
    dbfull()->test_waitforflushmemtable(handles_[1]);
    assert_eq(numtablefilesatlevel(0, 1), num + 3);
  }

  // generate one more file at level-0, which should trigger level-0
  // compaction.
  for (int i = 0; i < 11; i++) {
    assert_ok(put(1, key(key_idx), randomstring(&rnd, 10000)));
    key_idx++;
  }
  dbfull()->test_waitforcompact();
  // before compaction, we have 4 files at level 0, with size 4, 0.4, 1, 1.
  // after comapction, we should have 2 files, with size 4, 2.4.
  assert_eq(numtablefilesatlevel(0, 1), 2);
  for (int i = 1; i < options.num_levels ; i++) {
    assert_eq(numtablefilesatlevel(i, 1), 0);
  }

  // stage 3:
  //   now we have 2 files at level 0, with size 4 and 2.4. continue
  //   generating new files at level 0.
  for (int num = 0; num < options.level0_file_num_compaction_trigger - 3;
       num++) {
    // write 110kb (11 values, each 10k)
    for (int i = 0; i < 11; i++) {
      assert_ok(put(1, key(key_idx), randomstring(&rnd, 10000)));
      key_idx++;
    }
    dbfull()->test_waitforflushmemtable(handles_[1]);
    assert_eq(numtablefilesatlevel(0, 1), num + 3);
  }

  // generate one more file at level-0, which should trigger level-0
  // compaction.
  for (int i = 0; i < 12; i++) {
    assert_ok(put(1, key(key_idx), randomstring(&rnd, 10000)));
    key_idx++;
  }
  dbfull()->test_waitforcompact();
  // before compaction, we have 4 files at level 0, with size 4, 2.4, 1, 1.
  // after comapction, we should have 3 files, with size 4, 2.4, 2.
  assert_eq(numtablefilesatlevel(0, 1), 3);
  for (int i = 1; i < options.num_levels ; i++) {
    assert_eq(numtablefilesatlevel(i, 1), 0);
  }

  // stage 4:
  //   now we have 3 files at level 0, with size 4, 2.4, 2. let's generate a
  //   new file of size 1.
  for (int i = 0; i < 11; i++) {
    assert_ok(put(1, key(key_idx), randomstring(&rnd, 10000)));
    key_idx++;
  }
  dbfull()->test_waitforcompact();
  // level-0 compaction is triggered, but no file will be picked up.
  assert_eq(numtablefilesatlevel(0, 1), 4);
  for (int i = 1; i < options.num_levels ; i++) {
    assert_eq(numtablefilesatlevel(i, 1), 0);
  }

  // stage 5:
  //   now we have 4 files at level 0, with size 4, 2.4, 2, 1. let's generate
  //   a new file of size 1.
  filter->expect_full_compaction_.store(true);
  for (int i = 0; i < 11; i++) {
    assert_ok(put(1, key(key_idx), randomstring(&rnd, 10000)));
    key_idx++;
  }
  dbfull()->test_waitforcompact();
  // all files at level 0 will be compacted into a single one.
  assert_eq(numtablefilesatlevel(0, 1), 1);
  for (int i = 1; i < options.num_levels ; i++) {
    assert_eq(numtablefilesatlevel(i, 1), 0);
  }
}

test(dbtest, universalcompactionsizeamplification) {
  options options;
  options.compaction_style = kcompactionstyleuniversal;
  options.write_buffer_size = 100<<10; //100kb
  options.level0_file_num_compaction_trigger = 3;
  createandreopenwithcf({"pikachu"}, &options);

  // trigger compaction if size amplification exceeds 110%
  options.compaction_options_universal.max_size_amplification_percent = 110;
  options = currentoptions(options);
  reopenwithcolumnfamilies({"default", "pikachu"}, &options);

  random rnd(301);
  int key_idx = 0;

  //   generate two files in level 0. both files are approx the same size.
  for (int num = 0; num < options.level0_file_num_compaction_trigger - 1;
       num++) {
    // write 110kb (11 values, each 10k)
    for (int i = 0; i < 11; i++) {
      assert_ok(put(1, key(key_idx), randomstring(&rnd, 10000)));
      key_idx++;
    }
    dbfull()->test_waitforflushmemtable(handles_[1]);
    assert_eq(numtablefilesatlevel(0, 1), num + 1);
  }
  assert_eq(numtablefilesatlevel(0, 1), 2);

  // flush whatever is remaining in memtable. this is typically
  // small, which should not trigger size ratio based compaction
  // but will instead trigger size amplification.
  assert_ok(flush(1));

  dbfull()->test_waitforcompact();

  // verify that size amplification did occur
  assert_eq(numtablefilesatlevel(0, 1), 1);
}

test(dbtest, universalcompactionoptions) {
  options options;
  options.compaction_style = kcompactionstyleuniversal;
  options.write_buffer_size = 100<<10; //100kb
  options.level0_file_num_compaction_trigger = 4;
  options.num_levels = 1;
  options.compaction_options_universal.compression_size_percent = -1;
  options = currentoptions(options);
  createandreopenwithcf({"pikachu"}, &options);

  random rnd(301);
  int key_idx = 0;

  for (int num = 0; num < options.level0_file_num_compaction_trigger; num++) {
    // write 110kb (11 values, each 10k)
    for (int i = 0; i < 11; i++) {
      assert_ok(put(1, key(key_idx), randomstring(&rnd, 10000)));
      key_idx++;
    }
    dbfull()->test_waitforflushmemtable(handles_[1]);

    if (num < options.level0_file_num_compaction_trigger - 1) {
      assert_eq(numtablefilesatlevel(0, 1), num + 1);
    }
  }

  dbfull()->test_waitforcompact();
  assert_eq(numtablefilesatlevel(0, 1), 1);
  for (int i = 1; i < options.num_levels ; i++) {
    assert_eq(numtablefilesatlevel(i, 1), 0);
  }
}

test(dbtest, universalcompactionstopstylesimilarsize) {
  options options = currentoptions();
  options.compaction_style = kcompactionstyleuniversal;
  options.write_buffer_size = 100<<10; //100kb
  // trigger compaction if there are >= 4 files
  options.level0_file_num_compaction_trigger = 4;
  options.compaction_options_universal.size_ratio = 10;
  options.compaction_options_universal.stop_style = kcompactionstopstylesimilarsize;
  options.num_levels=1;
  reopen(&options);

  random rnd(301);
  int key_idx = 0;

  // stage 1:
  //   generate a set of files at level 0, but don't trigger level-0
  //   compaction.
  for (int num = 0;
       num < options.level0_file_num_compaction_trigger-1;
       num++) {
    // write 110kb (11 values, each 10k)
    for (int i = 0; i < 11; i++) {
      assert_ok(put(key(key_idx), randomstring(&rnd, 10000)));
      key_idx++;
    }
    dbfull()->test_waitforflushmemtable();
    assert_eq(numtablefilesatlevel(0), num + 1);
  }

  // generate one more file at level-0, which should trigger level-0
  // compaction.
  for (int i = 0; i < 11; i++) {
    assert_ok(put(key(key_idx), randomstring(&rnd, 10000)));
    key_idx++;
  }
  dbfull()->test_waitforcompact();
  // suppose each file flushed from mem table has size 1. now we compact
  // (level0_file_num_compaction_trigger+1)=4 files and should have a big
  // file of size 4.
  assert_eq(numtablefilesatlevel(0), 1);

  // stage 2:
  //   now we have one file at level 0, with size 4. we also have some data in
  //   mem table. let's continue generating new files at level 0, but don't
  //   trigger level-0 compaction.
  //   first, clean up memtable before inserting new data. this will generate
  //   a level-0 file, with size around 0.4 (according to previously written
  //   data amount).
  dbfull()->flush(flushoptions());
  for (int num = 0;
       num < options.level0_file_num_compaction_trigger-3;
       num++) {
    // write 110kb (11 values, each 10k)
    for (int i = 0; i < 11; i++) {
      assert_ok(put(key(key_idx), randomstring(&rnd, 10000)));
      key_idx++;
    }
    dbfull()->test_waitforflushmemtable();
    assert_eq(numtablefilesatlevel(0), num + 3);
  }

  // generate one more file at level-0, which should trigger level-0
  // compaction.
  for (int i = 0; i < 11; i++) {
    assert_ok(put(key(key_idx), randomstring(&rnd, 10000)));
    key_idx++;
  }
  dbfull()->test_waitforcompact();
  // before compaction, we have 4 files at level 0, with size 4, 0.4, 1, 1.
  // after compaction, we should have 3 files, with size 4, 0.4, 2.
  assert_eq(numtablefilesatlevel(0), 3);
  // stage 3:
  //   now we have 3 files at level 0, with size 4, 0.4, 2. generate one
  //   more file at level-0, which should trigger level-0 compaction.
  for (int i = 0; i < 11; i++) {
    assert_ok(put(key(key_idx), randomstring(&rnd, 10000)));
    key_idx++;
  }
  dbfull()->test_waitforcompact();
  // level-0 compaction is triggered, but no file will be picked up.
  assert_eq(numtablefilesatlevel(0), 4);
}

#if defined(snappy)
test(dbtest, compressedcache) {
  int num_iter = 80;

  // run this test three iterations.
  // iteration 1: only a uncompressed block cache
  // iteration 2: only a compressed block cache
  // iteration 3: both block cache and compressed cache
  // iteration 4: both block cache and compressed cache, but db is not
  // compressed
  for (int iter = 0; iter < 4; iter++) {
    options options;
    options.write_buffer_size = 64*1024;        // small write buffer
    options.statistics = rocksdb::createdbstatistics();

    blockbasedtableoptions table_options;
    switch (iter) {
      case 0:
        // only uncompressed block cache
        table_options.block_cache = newlrucache(8*1024);
        table_options.block_cache_compressed = nullptr;
        options.table_factory.reset(newblockbasedtablefactory(table_options));
        break;
      case 1:
        // no block cache, only compressed cache
        table_options.no_block_cache = true;
        table_options.block_cache = nullptr;
        table_options.block_cache_compressed = newlrucache(8*1024);
        options.table_factory.reset(newblockbasedtablefactory(table_options));
        break;
      case 2:
        // both compressed and uncompressed block cache
        table_options.block_cache = newlrucache(1024);
        table_options.block_cache_compressed = newlrucache(8*1024);
        options.table_factory.reset(newblockbasedtablefactory(table_options));
        break;
      case 3:
        // both block cache and compressed cache, but db is not compressed
        // also, make block cache sizes bigger, to trigger block cache hits
        table_options.block_cache = newlrucache(1024 * 1024);
        table_options.block_cache_compressed = newlrucache(8 * 1024 * 1024);
        options.table_factory.reset(newblockbasedtablefactory(table_options));
        options.compression = knocompression;
        break;
      default:
        assert_true(false);
    }
    createandreopenwithcf({"pikachu"}, &options);
    // default column family doesn't have block cache
    options no_block_cache_opts;
    no_block_cache_opts.statistics = options.statistics;
    blockbasedtableoptions table_options_no_bc;
    table_options_no_bc.no_block_cache = true;
    no_block_cache_opts.table_factory.reset(
        newblockbasedtablefactory(table_options_no_bc));
    reopenwithcolumnfamilies({"default", "pikachu"},
                             {&no_block_cache_opts, &options});

    random rnd(301);

    // write 8mb (80 values, each 100k)
    assert_eq(numtablefilesatlevel(0, 1), 0);
    std::vector<std::string> values;
    std::string str;
    for (int i = 0; i < num_iter; i++) {
      if (i % 4 == 0) {        // high compression ratio
        str = randomstring(&rnd, 1000);
      }
      values.push_back(str);
      assert_ok(put(1, key(i), values[i]));
    }

    // flush all data from memtable so that reads are from block cache
    assert_ok(flush(1));

    for (int i = 0; i < num_iter; i++) {
      assert_eq(get(1, key(i)), values[i]);
    }

    // check that we triggered the appropriate code paths in the cache
    switch (iter) {
      case 0:
        // only uncompressed block cache
        assert_gt(testgettickercount(options, block_cache_miss), 0);
        assert_eq(testgettickercount(options, block_cache_compressed_miss), 0);
        break;
      case 1:
        // no block cache, only compressed cache
        assert_eq(testgettickercount(options, block_cache_miss), 0);
        assert_gt(testgettickercount(options, block_cache_compressed_miss), 0);
        break;
      case 2:
        // both compressed and uncompressed block cache
        assert_gt(testgettickercount(options, block_cache_miss), 0);
        assert_gt(testgettickercount(options, block_cache_compressed_miss), 0);
        break;
      case 3:
        // both compressed and uncompressed block cache
        assert_gt(testgettickercount(options, block_cache_miss), 0);
        assert_gt(testgettickercount(options, block_cache_hit), 0);
        assert_gt(testgettickercount(options, block_cache_compressed_miss), 0);
        // compressed doesn't have any hits since blocks are not compressed on
        // storage
        assert_eq(testgettickercount(options, block_cache_compressed_hit), 0);
        break;
      default:
        assert_true(false);
    }

    options.create_if_missing = true;
    destroyandreopen(&options);
  }
}

static std::string compressiblestring(random* rnd, int len) {
  std::string r;
  test::compressiblestring(rnd, 0.8, len, &r);
  return r;
}

test(dbtest, universalcompactioncompressratio1) {
  options options;
  options.compaction_style = kcompactionstyleuniversal;
  options.write_buffer_size = 100<<10; //100kb
  options.level0_file_num_compaction_trigger = 2;
  options.num_levels = 1;
  options.compaction_options_universal.compression_size_percent = 70;
  options = currentoptions(options);
  reopen(&options);

  random rnd(301);
  int key_idx = 0;

  // the first compaction (2) is compressed.
  for (int num = 0; num < 2; num++) {
    // write 110kb (11 values, each 10k)
    for (int i = 0; i < 11; i++) {
      assert_ok(put(key(key_idx), compressiblestring(&rnd, 10000)));
      key_idx++;
    }
    dbfull()->test_waitforflushmemtable();
    dbfull()->test_waitforcompact();
  }
  assert_lt((int)dbfull()->test_getlevel0totalsize(), 110000 * 2 * 0.9);

  // the second compaction (4) is compressed
  for (int num = 0; num < 2; num++) {
    // write 110kb (11 values, each 10k)
    for (int i = 0; i < 11; i++) {
      assert_ok(put(key(key_idx), compressiblestring(&rnd, 10000)));
      key_idx++;
    }
    dbfull()->test_waitforflushmemtable();
    dbfull()->test_waitforcompact();
  }
  assert_lt((int)dbfull()->test_getlevel0totalsize(), 110000 * 4 * 0.9);

  // the third compaction (2 4) is compressed since this time it is
  // (1 1 3.2) and 3.2/5.2 doesn't reach ratio.
  for (int num = 0; num < 2; num++) {
    // write 110kb (11 values, each 10k)
    for (int i = 0; i < 11; i++) {
      assert_ok(put(key(key_idx), compressiblestring(&rnd, 10000)));
      key_idx++;
    }
    dbfull()->test_waitforflushmemtable();
    dbfull()->test_waitforcompact();
  }
  assert_lt((int)dbfull()->test_getlevel0totalsize(), 110000 * 6 * 0.9);

  // when we start for the compaction up to (2 4 8), the latest
  // compressed is not compressed.
  for (int num = 0; num < 8; num++) {
    // write 110kb (11 values, each 10k)
    for (int i = 0; i < 11; i++) {
      assert_ok(put(key(key_idx), compressiblestring(&rnd, 10000)));
      key_idx++;
    }
    dbfull()->test_waitforflushmemtable();
    dbfull()->test_waitforcompact();
  }
  assert_gt((int)dbfull()->test_getlevel0totalsize(),
            110000 * 11 * 0.8 + 110000 * 2);
}

test(dbtest, universalcompactioncompressratio2) {
  options options;
  options.compaction_style = kcompactionstyleuniversal;
  options.write_buffer_size = 100<<10; //100kb
  options.level0_file_num_compaction_trigger = 2;
  options.num_levels = 1;
  options.compaction_options_universal.compression_size_percent = 95;
  options = currentoptions(options);
  reopen(&options);

  random rnd(301);
  int key_idx = 0;

  // when we start for the compaction up to (2 4 8), the latest
  // compressed is compressed given the size ratio to compress.
  for (int num = 0; num < 14; num++) {
    // write 120kb (12 values, each 10k)
    for (int i = 0; i < 12; i++) {
      assert_ok(put(key(key_idx), compressiblestring(&rnd, 10000)));
      key_idx++;
    }
    dbfull()->test_waitforflushmemtable();
    dbfull()->test_waitforcompact();
  }
  assert_lt((int)dbfull()->test_getlevel0totalsize(),
            120000 * 12 * 0.8 + 120000 * 2);
}

test(dbtest, failmoredbpaths) {
  options options;
  options.db_paths.emplace_back(dbname_, 10000000);
  options.db_paths.emplace_back(dbname_ + "_2", 1000000);
  options.db_paths.emplace_back(dbname_ + "_3", 1000000);
  options.db_paths.emplace_back(dbname_ + "_4", 1000000);
  options.db_paths.emplace_back(dbname_ + "_5", 1000000);
  assert_true(tryreopen(&options).isnotsupported());
}

test(dbtest, universalcompactionsecondpathratio) {
  options options;
  options.db_paths.emplace_back(dbname_, 500 * 1024);
  options.db_paths.emplace_back(dbname_ + "_2", 1024 * 1024 * 1024);
  options.compaction_style = kcompactionstyleuniversal;
  options.write_buffer_size = 100 << 10;  // 100kb
  options.level0_file_num_compaction_trigger = 2;
  options.num_levels = 1;
  options = currentoptions(options);

  std::vector<std::string> filenames;
  env_->getchildren(options.db_paths[1].path, &filenames);
  // delete archival files.
  for (size_t i = 0; i < filenames.size(); ++i) {
    env_->deletefile(options.db_paths[1].path + "/" + filenames[i]);
  }
  env_->deletedir(options.db_paths[1].path);
  reopen(&options);

  random rnd(301);
  int key_idx = 0;

  // first three 110kb files are not going to second path.
  // after that, (100k, 200k)
  for (int num = 0; num < 3; num++) {
    generatenewfile(&rnd, &key_idx);
  }

  // another 110kb triggers a compaction to 400k file to second path
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[1].path));

  // (1, 4)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[1].path));
  assert_eq(1, getsstfilecount(dbname_));

  // (1,1,4) -> (2, 4)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[1].path));
  assert_eq(1, getsstfilecount(dbname_));

  // (1, 2, 4)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[1].path));
  assert_eq(2, getsstfilecount(dbname_));

  // (1, 1, 2, 4) -> (8)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[1].path));
  assert_eq(0, getsstfilecount(dbname_));

  // (1, 8)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[1].path));
  assert_eq(1, getsstfilecount(dbname_));

  // (1, 1, 8) -> (2, 8)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[1].path));
  assert_eq(1, getsstfilecount(dbname_));

  // (1, 2, 8)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[1].path));
  assert_eq(2, getsstfilecount(dbname_));

  // (1, 1, 2, 8) -> (4, 8)
  generatenewfile(&rnd, &key_idx);
  assert_eq(2, getsstfilecount(options.db_paths[1].path));
  assert_eq(0, getsstfilecount(dbname_));

  // (1, 4, 8)
  generatenewfile(&rnd, &key_idx);
  assert_eq(2, getsstfilecount(options.db_paths[1].path));
  assert_eq(1, getsstfilecount(dbname_));

  for (int i = 0; i < key_idx; i++) {
    auto v = get(key(i));
    assert_ne(v, "not_found");
    assert_true(v.size() == 1 || v.size() == 10000);
  }

  reopen(&options);

  for (int i = 0; i < key_idx; i++) {
    auto v = get(key(i));
    assert_ne(v, "not_found");
    assert_true(v.size() == 1 || v.size() == 10000);
  }

  destroy(&options);
}

test(dbtest, universalcompactionfourpaths) {
  options options;
  options.db_paths.emplace_back(dbname_, 300 * 1024);
  options.db_paths.emplace_back(dbname_ + "_2", 300 * 1024);
  options.db_paths.emplace_back(dbname_ + "_3", 500 * 1024);
  options.db_paths.emplace_back(dbname_ + "_4", 1024 * 1024 * 1024);
  options.compaction_style = kcompactionstyleuniversal;
  options.write_buffer_size = 100 << 10;  // 100kb
  options.level0_file_num_compaction_trigger = 2;
  options.num_levels = 1;
  options = currentoptions(options);

  std::vector<std::string> filenames;
  env_->getchildren(options.db_paths[1].path, &filenames);
  // delete archival files.
  for (size_t i = 0; i < filenames.size(); ++i) {
    env_->deletefile(options.db_paths[1].path + "/" + filenames[i]);
  }
  env_->deletedir(options.db_paths[1].path);
  reopen(&options);

  random rnd(301);
  int key_idx = 0;

  // first three 110kb files are not going to second path.
  // after that, (100k, 200k)
  for (int num = 0; num < 3; num++) {
    generatenewfile(&rnd, &key_idx);
  }

  // another 110kb triggers a compaction to 400k file to second path
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[2].path));

  // (1, 4)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[2].path));
  assert_eq(1, getsstfilecount(dbname_));

  // (1,1,4) -> (2, 4)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[2].path));
  assert_eq(1, getsstfilecount(options.db_paths[1].path));
  assert_eq(0, getsstfilecount(dbname_));

  // (1, 2, 4)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[2].path));
  assert_eq(1, getsstfilecount(options.db_paths[1].path));
  assert_eq(1, getsstfilecount(dbname_));

  // (1, 1, 2, 4) -> (8)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[3].path));

  // (1, 8)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[3].path));
  assert_eq(1, getsstfilecount(dbname_));

  // (1, 1, 8) -> (2, 8)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[3].path));
  assert_eq(1, getsstfilecount(options.db_paths[1].path));

  // (1, 2, 8)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[3].path));
  assert_eq(1, getsstfilecount(options.db_paths[1].path));
  assert_eq(1, getsstfilecount(dbname_));

  // (1, 1, 2, 8) -> (4, 8)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[2].path));
  assert_eq(1, getsstfilecount(options.db_paths[3].path));

  // (1, 4, 8)
  generatenewfile(&rnd, &key_idx);
  assert_eq(1, getsstfilecount(options.db_paths[3].path));
  assert_eq(1, getsstfilecount(options.db_paths[2].path));
  assert_eq(1, getsstfilecount(dbname_));

  for (int i = 0; i < key_idx; i++) {
    auto v = get(key(i));
    assert_ne(v, "not_found");
    assert_true(v.size() == 1 || v.size() == 10000);
  }

  reopen(&options);

  for (int i = 0; i < key_idx; i++) {
    auto v = get(key(i));
    assert_ne(v, "not_found");
    assert_true(v.size() == 1 || v.size() == 10000);
  }

  destroy(&options);
}
#endif

test(dbtest, convertcompactionstyle) {
  random rnd(301);
  int max_key_level_insert = 200;
  int max_key_universal_insert = 600;

  // stage 1: generate a db with level compaction
  options options;
  options.write_buffer_size = 100<<10; //100kb
  options.num_levels = 4;
  options.level0_file_num_compaction_trigger = 3;
  options.max_bytes_for_level_base = 500<<10; // 500kb
  options.max_bytes_for_level_multiplier = 1;
  options.target_file_size_base = 200<<10; // 200kb
  options.target_file_size_multiplier = 1;
  options = currentoptions(options);
  createandreopenwithcf({"pikachu"}, &options);

  for (int i = 0; i <= max_key_level_insert; i++) {
    // each value is 10k
    assert_ok(put(1, key(i), randomstring(&rnd, 10000)));
  }
  assert_ok(flush(1));
  dbfull()->test_waitforcompact();

  assert_gt(totaltablefiles(1, 4), 1);
  int non_level0_num_files = 0;
  for (int i = 1; i < options.num_levels; i++) {
    non_level0_num_files += numtablefilesatlevel(i, 1);
  }
  assert_gt(non_level0_num_files, 0);

  // stage 2: reopen with universal compaction - should fail
  options = currentoptions();
  options.compaction_style = kcompactionstyleuniversal;
  options = currentoptions(options);
  status s = tryreopenwithcolumnfamilies({"default", "pikachu"}, &options);
  assert_true(s.isinvalidargument());

  // stage 3: compact into a single file and move the file to level 0
  options = currentoptions();
  options.disable_auto_compactions = true;
  options.target_file_size_base = int_max;
  options.target_file_size_multiplier = 1;
  options.max_bytes_for_level_base = int_max;
  options.max_bytes_for_level_multiplier = 1;
  options = currentoptions(options);
  reopenwithcolumnfamilies({"default", "pikachu"}, &options);

  dbfull()->compactrange(handles_[1], nullptr, nullptr, true /* reduce level */,
                         0 /* reduce to level 0 */);

  for (int i = 0; i < options.num_levels; i++) {
    int num = numtablefilesatlevel(i, 1);
    if (i == 0) {
      assert_eq(num, 1);
    } else {
      assert_eq(num, 0);
    }
  }

  // stage 4: re-open in universal compaction style and do some db operations
  options = currentoptions();
  options.compaction_style = kcompactionstyleuniversal;
  options.write_buffer_size = 100<<10; //100kb
  options.level0_file_num_compaction_trigger = 3;
  options = currentoptions(options);
  reopenwithcolumnfamilies({"default", "pikachu"}, &options);

  for (int i = max_key_level_insert / 2; i <= max_key_universal_insert; i++) {
    assert_ok(put(1, key(i), randomstring(&rnd, 10000)));
  }
  dbfull()->flush(flushoptions());
  assert_ok(flush(1));
  dbfull()->test_waitforcompact();

  for (int i = 1; i < options.num_levels; i++) {
    assert_eq(numtablefilesatlevel(i, 1), 0);
  }

  // verify keys inserted in both level compaction style and universal
  // compaction style
  std::string keys_in_db;
  iterator* iter = dbfull()->newiterator(readoptions(), handles_[1]);
  for (iter->seektofirst(); iter->valid(); iter->next()) {
    keys_in_db.append(iter->key().tostring());
    keys_in_db.push_back(',');
  }
  delete iter;

  std::string expected_keys;
  for (int i = 0; i <= max_key_universal_insert; i++) {
    expected_keys.append(key(i));
    expected_keys.push_back(',');
  }

  assert_eq(keys_in_db, expected_keys);
}

namespace {
void minlevelhelper(dbtest* self, options& options) {
  random rnd(301);

  for (int num = 0;
    num < options.level0_file_num_compaction_trigger - 1;
    num++)
  {
    std::vector<std::string> values;
    // write 120kb (12 values, each 10k)
    for (int i = 0; i < 12; i++) {
      values.push_back(randomstring(&rnd, 10000));
      assert_ok(self->put(key(i), values[i]));
    }
    self->dbfull()->test_waitforflushmemtable();
    assert_eq(self->numtablefilesatlevel(0), num + 1);
  }

  //generate one more file in level-0, and should trigger level-0 compaction
  std::vector<std::string> values;
  for (int i = 0; i < 12; i++) {
    values.push_back(randomstring(&rnd, 10000));
    assert_ok(self->put(key(i), values[i]));
  }
  self->dbfull()->test_waitforcompact();

  assert_eq(self->numtablefilesatlevel(0), 0);
  assert_eq(self->numtablefilesatlevel(1), 1);
}

// returns false if the calling-test should be skipped
bool minleveltocompress(compressiontype& type, options& options, int wbits,
                        int lev, int strategy) {
  fprintf(stderr, "test with compression options : window_bits = %d, level =  %d, strategy = %d}\n", wbits, lev, strategy);
  options.write_buffer_size = 100<<10; //100kb
  options.num_levels = 3;
  options.max_mem_compaction_level = 0;
  options.level0_file_num_compaction_trigger = 3;
  options.create_if_missing = true;

  if (snappycompressionsupported(compressionoptions(wbits, lev, strategy))) {
    type = ksnappycompression;
    fprintf(stderr, "using snappy\n");
  } else if (zlibcompressionsupported(
               compressionoptions(wbits, lev, strategy))) {
    type = kzlibcompression;
    fprintf(stderr, "using zlib\n");
  } else if (bzip2compressionsupported(
               compressionoptions(wbits, lev, strategy))) {
    type = kbzip2compression;
    fprintf(stderr, "using bzip2\n");
  } else if (lz4compressionsupported(
                 compressionoptions(wbits, lev, strategy))) {
    type = klz4compression;
    fprintf(stderr, "using lz4\n");
  } else if (lz4hccompressionsupported(
                 compressionoptions(wbits, lev, strategy))) {
    type = klz4hccompression;
    fprintf(stderr, "using lz4hc\n");
  } else {
    fprintf(stderr, "skipping test, compression disabled\n");
    return false;
  }
  options.compression_per_level.resize(options.num_levels);

  // do not compress l0
  for (int i = 0; i < 1; i++) {
    options.compression_per_level[i] = knocompression;
  }
  for (int i = 1; i < options.num_levels; i++) {
    options.compression_per_level[i] = type;
  }
  return true;
}
}  // namespace

test(dbtest, minleveltocompress1) {
  options options = currentoptions();
  compressiontype type;
  if (!minleveltocompress(type, options, -14, -1, 0)) {
    return;
  }
  reopen(&options);
  minlevelhelper(this, options);

  // do not compress l0 and l1
  for (int i = 0; i < 2; i++) {
    options.compression_per_level[i] = knocompression;
  }
  for (int i = 2; i < options.num_levels; i++) {
    options.compression_per_level[i] = type;
  }
  destroyandreopen(&options);
  minlevelhelper(this, options);
}

test(dbtest, minleveltocompress2) {
  options options = currentoptions();
  compressiontype type;
  if (!minleveltocompress(type, options, 15, -1, 0)) {
    return;
  }
  reopen(&options);
  minlevelhelper(this, options);

  // do not compress l0 and l1
  for (int i = 0; i < 2; i++) {
    options.compression_per_level[i] = knocompression;
  }
  for (int i = 2; i < options.num_levels; i++) {
    options.compression_per_level[i] = type;
  }
  destroyandreopen(&options);
  minlevelhelper(this, options);
}

test(dbtest, repeatedwritestosamekey) {
  do {
    options options;
    options.env = env_;
    options.write_buffer_size = 100000;  // small write buffer
    options = currentoptions(options);
    createandreopenwithcf({"pikachu"}, &options);

    // we must have at most one file per level except for level-0,
    // which may have up to kl0_stopwritestrigger files.
    const int kmaxfiles =
        options.num_levels + options.level0_stop_writes_trigger;

    random rnd(301);
    std::string value = randomstring(&rnd, 2 * options.write_buffer_size);
    for (int i = 0; i < 5 * kmaxfiles; i++) {
      assert_ok(put(1, "key", value));
      assert_le(totaltablefiles(1), kmaxfiles);
    }
  } while (changecompactoptions());
}

test(dbtest, inplaceupdate) {
  do {
    options options;
    options.create_if_missing = true;
    options.inplace_update_support = true;
    options.env = env_;
    options.write_buffer_size = 100000;
    options = currentoptions(options);
    createandreopenwithcf({"pikachu"}, &options);

    // update key with values of smaller size
    int numvalues = 10;
    for (int i = numvalues; i > 0; i--) {
      std::string value = dummystring(i, 'a');
      assert_ok(put(1, "key", value));
      assert_eq(value, get(1, "key"));
    }

    // only 1 instance for that key.
    validatenumberofentries(1, 1);

  } while (changecompactoptions());
}

test(dbtest, inplaceupdatelargenewvalue) {
  do {
    options options;
    options.create_if_missing = true;
    options.inplace_update_support = true;
    options.env = env_;
    options.write_buffer_size = 100000;
    options = currentoptions(options);
    createandreopenwithcf({"pikachu"}, &options);

    // update key with values of larger size
    int numvalues = 10;
    for (int i = 0; i < numvalues; i++) {
      std::string value = dummystring(i, 'a');
      assert_ok(put(1, "key", value));
      assert_eq(value, get(1, "key"));
    }

    // all 10 updates exist in the internal iterator
    validatenumberofentries(numvalues, 1);

  } while (changecompactoptions());
}


test(dbtest, inplaceupdatecallbacksmallersize) {
  do {
    options options;
    options.create_if_missing = true;
    options.inplace_update_support = true;

    options.env = env_;
    options.write_buffer_size = 100000;
    options.inplace_callback =
      rocksdb::dbtest::updateinplacesmallersize;
    options = currentoptions(options);
    createandreopenwithcf({"pikachu"}, &options);

    // update key with values of smaller size
    int numvalues = 10;
    assert_ok(put(1, "key", dummystring(numvalues, 'a')));
    assert_eq(dummystring(numvalues, 'c'), get(1, "key"));

    for (int i = numvalues; i > 0; i--) {
      assert_ok(put(1, "key", dummystring(i, 'a')));
      assert_eq(dummystring(i - 1, 'b'), get(1, "key"));
    }

    // only 1 instance for that key.
    validatenumberofentries(1, 1);

  } while (changecompactoptions());
}

test(dbtest, inplaceupdatecallbacksmallervarintsize) {
  do {
    options options;
    options.create_if_missing = true;
    options.inplace_update_support = true;

    options.env = env_;
    options.write_buffer_size = 100000;
    options.inplace_callback =
      rocksdb::dbtest::updateinplacesmallervarintsize;
    options = currentoptions(options);
    createandreopenwithcf({"pikachu"}, &options);

    // update key with values of smaller varint size
    int numvalues = 265;
    assert_ok(put(1, "key", dummystring(numvalues, 'a')));
    assert_eq(dummystring(numvalues, 'c'), get(1, "key"));

    for (int i = numvalues; i > 0; i--) {
      assert_ok(put(1, "key", dummystring(i, 'a')));
      assert_eq(dummystring(1, 'b'), get(1, "key"));
    }

    // only 1 instance for that key.
    validatenumberofentries(1, 1);

  } while (changecompactoptions());
}

test(dbtest, inplaceupdatecallbacklargenewvalue) {
  do {
    options options;
    options.create_if_missing = true;
    options.inplace_update_support = true;

    options.env = env_;
    options.write_buffer_size = 100000;
    options.inplace_callback =
      rocksdb::dbtest::updateinplacelargersize;
    options = currentoptions(options);
    createandreopenwithcf({"pikachu"}, &options);

    // update key with values of larger size
    int numvalues = 10;
    for (int i = 0; i < numvalues; i++) {
      assert_ok(put(1, "key", dummystring(i, 'a')));
      assert_eq(dummystring(i, 'c'), get(1, "key"));
    }

    // no inplace updates. all updates are puts with new seq number
    // all 10 updates exist in the internal iterator
    validatenumberofentries(numvalues, 1);

  } while (changecompactoptions());
}

test(dbtest, inplaceupdatecallbacknoaction) {
  do {
    options options;
    options.create_if_missing = true;
    options.inplace_update_support = true;

    options.env = env_;
    options.write_buffer_size = 100000;
    options.inplace_callback =
      rocksdb::dbtest::updateinplacenoaction;
    options = currentoptions(options);
    createandreopenwithcf({"pikachu"}, &options);

    // callback function requests no actions from db
    assert_ok(put(1, "key", dummystring(1, 'a')));
    assert_eq(get(1, "key"), "not_found");

  } while (changecompactoptions());
}

test(dbtest, compactionfilter) {
  options options = currentoptions();
  options.max_open_files = -1;
  options.num_levels = 3;
  options.max_mem_compaction_level = 0;
  options.compaction_filter_factory = std::make_shared<keepfilterfactory>();
  options = currentoptions(options);
  createandreopenwithcf({"pikachu"}, &options);

  // write 100k keys, these are written to a few files in l0.
  const std::string value(10, 'x');
  for (int i = 0; i < 100000; i++) {
    char key[100];
    snprintf(key, sizeof(key), "b%010d", i);
    put(1, key, value);
  }
  assert_ok(flush(1));

  // push all files to the highest level l2. verify that
  // the compaction is each level invokes the filter for
  // all the keys in that level.
  cfilter_count = 0;
  dbfull()->test_compactrange(0, nullptr, nullptr, handles_[1]);
  assert_eq(cfilter_count, 100000);
  cfilter_count = 0;
  dbfull()->test_compactrange(1, nullptr, nullptr, handles_[1]);
  assert_eq(cfilter_count, 100000);

  assert_eq(numtablefilesatlevel(0, 1), 0);
  assert_eq(numtablefilesatlevel(1, 1), 0);
  assert_ne(numtablefilesatlevel(2, 1), 0);
  cfilter_count = 0;

  // all the files are in the lowest level.
  // verify that all but the 100001st record
  // has sequence number zero. the 100001st record
  // is at the tip of this snapshot and cannot
  // be zeroed out.
  // todo: figure out sequence number squashtoo
  int count = 0;
  int total = 0;
  iterator* iter = dbfull()->test_newinternaliterator(handles_[1]);
  iter->seektofirst();
  assert_ok(iter->status());
  while (iter->valid()) {
    parsedinternalkey ikey(slice(), 0, ktypevalue);
    ikey.sequence = -1;
    assert_eq(parseinternalkey(iter->key(), &ikey), true);
    total++;
    if (ikey.sequence != 0) {
      count++;
    }
    iter->next();
  }
  assert_eq(total, 100000);
  assert_eq(count, 1);
  delete iter;

  // overwrite all the 100k keys once again.
  for (int i = 0; i < 100000; i++) {
    char key[100];
    snprintf(key, sizeof(key), "b%010d", i);
    assert_ok(put(1, key, value));
  }
  assert_ok(flush(1));

  // push all files to the highest level l2. this
  // means that all keys should pass at least once
  // via the compaction filter
  cfilter_count = 0;
  dbfull()->test_compactrange(0, nullptr, nullptr, handles_[1]);
  assert_eq(cfilter_count, 100000);
  cfilter_count = 0;
  dbfull()->test_compactrange(1, nullptr, nullptr, handles_[1]);
  assert_eq(cfilter_count, 100000);
  assert_eq(numtablefilesatlevel(0, 1), 0);
  assert_eq(numtablefilesatlevel(1, 1), 0);
  assert_ne(numtablefilesatlevel(2, 1), 0);

  // create a new database with the compaction
  // filter in such a way that it deletes all keys
  options.compaction_filter_factory = std::make_shared<deletefilterfactory>();
  options.create_if_missing = true;
  destroyandreopen(&options);
  createandreopenwithcf({"pikachu"}, &options);

  // write all the keys once again.
  for (int i = 0; i < 100000; i++) {
    char key[100];
    snprintf(key, sizeof(key), "b%010d", i);
    assert_ok(put(1, key, value));
  }
  assert_ok(flush(1));
  assert_ne(numtablefilesatlevel(0, 1), 0);
  assert_eq(numtablefilesatlevel(1, 1), 0);
  assert_eq(numtablefilesatlevel(2, 1), 0);

  // push all files to the highest level l2. this
  // triggers the compaction filter to delete all keys,
  // verify that at the end of the compaction process,
  // nothing is left.
  cfilter_count = 0;
  dbfull()->test_compactrange(0, nullptr, nullptr, handles_[1]);
  assert_eq(cfilter_count, 100000);
  cfilter_count = 0;
  dbfull()->test_compactrange(1, nullptr, nullptr, handles_[1]);
  assert_eq(cfilter_count, 0);
  assert_eq(numtablefilesatlevel(0, 1), 0);
  assert_eq(numtablefilesatlevel(1, 1), 0);

  // scan the entire database to ensure that nothing is left
  iter = db_->newiterator(readoptions(), handles_[1]);
  iter->seektofirst();
  count = 0;
  while (iter->valid()) {
    count++;
    iter->next();
  }
  assert_eq(count, 0);
  delete iter;

  // the sequence number of the remaining record
  // is not zeroed out even though it is at the
  // level lmax because this record is at the tip
  // todo: remove the following or design a different
  // test
  count = 0;
  iter = dbfull()->test_newinternaliterator(handles_[1]);
  iter->seektofirst();
  assert_ok(iter->status());
  while (iter->valid()) {
    parsedinternalkey ikey(slice(), 0, ktypevalue);
    assert_eq(parseinternalkey(iter->key(), &ikey), true);
    assert_ne(ikey.sequence, (unsigned)0);
    count++;
    iter->next();
  }
  assert_eq(count, 0);
  delete iter;
}

// tests the edge case where compaction does not produce any output -- all
// entries are deleted. the compaction should create bunch of 'deletefile'
// entries in versionedit, but none of the 'addfile's.
test(dbtest, compactionfilterdeletesall) {
  options options;
  options.compaction_filter_factory = std::make_shared<deletefilterfactory>();
  options.disable_auto_compactions = true;
  options.create_if_missing = true;
  destroyandreopen(&options);

  // put some data
  for (int table = 0; table < 4; ++table) {
    for (int i = 0; i < 10 + table; ++i) {
      put(std::to_string(table * 100 + i), "val");
    }
    flush();
  }

  // this will produce empty file (delete compaction filter)
  assert_ok(db_->compactrange(nullptr, nullptr));
  assert_eq(0, countlivefiles());

  reopen(&options);

  iterator* itr = db_->newiterator(readoptions());
  itr->seektofirst();
  // empty db
  assert_true(!itr->valid());

  delete itr;
}

test(dbtest, compactionfilterwithvaluechange) {
  do {
    options options;
    options.num_levels = 3;
    options.max_mem_compaction_level = 0;
    options.compaction_filter_factory =
      std::make_shared<changefilterfactory>();
    options = currentoptions(options);
    createandreopenwithcf({"pikachu"}, &options);

    // write 100k+1 keys, these are written to a few files
    // in l0. we do this so that the current snapshot points
    // to the 100001 key.the compaction filter is  not invoked
    // on keys that are visible via a snapshot because we
    // anyways cannot delete it.
    const std::string value(10, 'x');
    for (int i = 0; i < 100001; i++) {
      char key[100];
      snprintf(key, sizeof(key), "b%010d", i);
      put(1, key, value);
    }

    // push all files to  lower levels
    assert_ok(flush(1));
    dbfull()->test_compactrange(0, nullptr, nullptr, handles_[1]);
    dbfull()->test_compactrange(1, nullptr, nullptr, handles_[1]);

    // re-write all data again
    for (int i = 0; i < 100001; i++) {
      char key[100];
      snprintf(key, sizeof(key), "b%010d", i);
      put(1, key, value);
    }

    // push all files to  lower levels. this should
    // invoke the compaction filter for all 100000 keys.
    assert_ok(flush(1));
    dbfull()->test_compactrange(0, nullptr, nullptr, handles_[1]);
    dbfull()->test_compactrange(1, nullptr, nullptr, handles_[1]);

    // verify that all keys now have the new value that
    // was set by the compaction process.
    for (int i = 0; i < 100001; i++) {
      char key[100];
      snprintf(key, sizeof(key), "b%010d", i);
      std::string newvalue = get(1, key);
      assert_eq(newvalue.compare(new_value), 0);
    }
  } while (changecompactoptions());
}

test(dbtest, compactionfiltercontextmanual) {
  keepfilterfactory* filter = new keepfilterfactory();

  options options = currentoptions();
  options.compaction_style = kcompactionstyleuniversal;
  options.compaction_filter_factory.reset(filter);
  options.compression = knocompression;
  options.level0_file_num_compaction_trigger = 8;
  reopen(&options);
  int num_keys_per_file = 400;
  for (int j = 0; j < 3; j++) {
    // write several keys.
    const std::string value(10, 'x');
    for (int i = 0; i < num_keys_per_file; i++) {
      char key[100];
      snprintf(key, sizeof(key), "b%08d%02d", i, j);
      put(key, value);
    }
    dbfull()->test_flushmemtable();
    // make sure next file is much smaller so automatic compaction will not
    // be triggered.
    num_keys_per_file /= 2;
  }

  // force a manual compaction
  cfilter_count = 0;
  filter->expect_manual_compaction_.store(true);
  filter->expect_full_compaction_.store(false);  // manual compaction always
                                                 // set this flag.
  dbfull()->compactrange(nullptr, nullptr);
  assert_eq(cfilter_count, 700);
  assert_eq(numtablefilesatlevel(0), 1);

  // verify total number of keys is correct after manual compaction.
  int count = 0;
  int total = 0;
  iterator* iter = dbfull()->test_newinternaliterator();
  iter->seektofirst();
  assert_ok(iter->status());
  while (iter->valid()) {
    parsedinternalkey ikey(slice(), 0, ktypevalue);
    ikey.sequence = -1;
    assert_eq(parseinternalkey(iter->key(), &ikey), true);
    total++;
    if (ikey.sequence != 0) {
      count++;
    }
    iter->next();
  }
  assert_eq(total, 700);
  assert_eq(count, 1);
  delete iter;
}

class keepfilterv2 : public compactionfilterv2 {
 public:
  virtual std::vector<bool> filter(int level,
                                   const slicevector& keys,
                                   const slicevector& existing_values,
                                   std::vector<std::string>* new_values,
                                   std::vector<bool>* values_changed)
    const override {
    cfilter_count++;
    std::vector<bool> ret;
    new_values->clear();
    values_changed->clear();
    for (unsigned int i = 0; i < keys.size(); ++i) {
      values_changed->push_back(false);
      ret.push_back(false);
    }
    return ret;
  }

  virtual const char* name() const override {
    return "keepfilterv2";
  }
};

class deletefilterv2 : public compactionfilterv2 {
 public:
  virtual std::vector<bool> filter(int level,
                                   const slicevector& keys,
                                   const slicevector& existing_values,
                                   std::vector<std::string>* new_values,
                                   std::vector<bool>* values_changed)
    const override {
    cfilter_count++;
    new_values->clear();
    values_changed->clear();
    std::vector<bool> ret;
    for (unsigned int i = 0; i < keys.size(); ++i) {
      values_changed->push_back(false);
      ret.push_back(true);
    }
    return ret;
  }

  virtual const char* name() const override {
    return "deletefilterv2";
  }
};

class changefilterv2 : public compactionfilterv2 {
 public:
  virtual std::vector<bool> filter(int level,
                                   const slicevector& keys,
                                   const slicevector& existing_values,
                                   std::vector<std::string>* new_values,
                                   std::vector<bool>* values_changed)
    const override {
    std::vector<bool> ret;
    new_values->clear();
    values_changed->clear();
    for (unsigned int i = 0; i < keys.size(); ++i) {
      values_changed->push_back(true);
      new_values->push_back(new_value);
      ret.push_back(false);
    }
    return ret;
  }

  virtual const char* name() const override {
    return "changefilterv2";
  }
};

class keepfilterfactoryv2 : public compactionfilterfactoryv2 {
 public:
  explicit keepfilterfactoryv2(const slicetransform* prefix_extractor)
    : compactionfilterfactoryv2(prefix_extractor) { }

  virtual std::unique_ptr<compactionfilterv2>
  createcompactionfilterv2(
      const compactionfiltercontext& context) override {
    return std::unique_ptr<compactionfilterv2>(new keepfilterv2());
  }

  virtual const char* name() const override {
    return "keepfilterfactoryv2";
  }
};

class deletefilterfactoryv2 : public compactionfilterfactoryv2 {
 public:
  explicit deletefilterfactoryv2(const slicetransform* prefix_extractor)
    : compactionfilterfactoryv2(prefix_extractor) { }

  virtual std::unique_ptr<compactionfilterv2>
  createcompactionfilterv2(
      const compactionfiltercontext& context) override {
    return std::unique_ptr<compactionfilterv2>(new deletefilterv2());
  }

  virtual const char* name() const override {
    return "deletefilterfactoryv2";
  }
};

class changefilterfactoryv2 : public compactionfilterfactoryv2 {
 public:
  explicit changefilterfactoryv2(const slicetransform* prefix_extractor)
    : compactionfilterfactoryv2(prefix_extractor) { }

  virtual std::unique_ptr<compactionfilterv2>
  createcompactionfilterv2(
      const compactionfiltercontext& context) override {
    return std::unique_ptr<compactionfilterv2>(new changefilterv2());
  }

  virtual const char* name() const override {
    return "changefilterfactoryv2";
  }
};

test(dbtest, compactionfilterv2) {
  options options = currentoptions();
  options.num_levels = 3;
  options.max_mem_compaction_level = 0;
  // extract prefix
  std::unique_ptr<const slicetransform> prefix_extractor;
  prefix_extractor.reset(newfixedprefixtransform(8));

  options.compaction_filter_factory_v2
    = std::make_shared<keepfilterfactoryv2>(prefix_extractor.get());
  // in a testing environment, we can only flush the application
  // compaction filter buffer using universal compaction
  option_config_ = kuniversalcompaction;
  options.compaction_style = (rocksdb::compactionstyle)1;
  reopen(&options);

  // write 100k keys, these are written to a few files in l0.
  const std::string value(10, 'x');
  for (int i = 0; i < 100000; i++) {
    char key[100];
    snprintf(key, sizeof(key), "b%08d%010d", i , i);
    put(key, value);
  }

  dbfull()->test_flushmemtable();

  dbfull()->test_compactrange(0, nullptr, nullptr);
  dbfull()->test_compactrange(1, nullptr, nullptr);

  assert_eq(numtablefilesatlevel(0), 1);

  // all the files are in the lowest level.
  int count = 0;
  int total = 0;
  iterator* iter = dbfull()->test_newinternaliterator();
  iter->seektofirst();
  assert_ok(iter->status());
  while (iter->valid()) {
    parsedinternalkey ikey(slice(), 0, ktypevalue);
    ikey.sequence = -1;
    assert_eq(parseinternalkey(iter->key(), &ikey), true);
    total++;
    if (ikey.sequence != 0) {
      count++;
    }
    iter->next();
  }

  assert_eq(total, 100000);
  // 1 snapshot only. since we are using universal compacton,
  // the sequence no is cleared for better compression
  assert_eq(count, 1);
  delete iter;

  // create a new database with the compaction
  // filter in such a way that it deletes all keys
  options.compaction_filter_factory_v2 =
    std::make_shared<deletefilterfactoryv2>(prefix_extractor.get());
  options.create_if_missing = true;
  destroyandreopen(&options);

  // write all the keys once again.
  for (int i = 0; i < 100000; i++) {
    char key[100];
    snprintf(key, sizeof(key), "b%08d%010d", i, i);
    put(key, value);
  }

  dbfull()->test_flushmemtable();
  assert_ne(numtablefilesatlevel(0), 0);

  dbfull()->test_compactrange(0, nullptr, nullptr);
  dbfull()->test_compactrange(1, nullptr, nullptr);
  assert_eq(numtablefilesatlevel(1), 0);

  // scan the entire database to ensure that nothing is left
  iter = db_->newiterator(readoptions());
  iter->seektofirst();
  count = 0;
  while (iter->valid()) {
    count++;
    iter->next();
  }

  assert_eq(count, 0);
  delete iter;
}

test(dbtest, compactionfilterv2withvaluechange) {
  options options = currentoptions();
  options.num_levels = 3;
  options.max_mem_compaction_level = 0;
  std::unique_ptr<const slicetransform> prefix_extractor;
  prefix_extractor.reset(newfixedprefixtransform(8));
  options.compaction_filter_factory_v2 =
    std::make_shared<changefilterfactoryv2>(prefix_extractor.get());
  // in a testing environment, we can only flush the application
  // compaction filter buffer using universal compaction
  option_config_ = kuniversalcompaction;
  options.compaction_style = (rocksdb::compactionstyle)1;
  options = currentoptions(options);
  reopen(&options);

  // write 100k+1 keys, these are written to a few files
  // in l0. we do this so that the current snapshot points
  // to the 100001 key.the compaction filter is  not invoked
  // on keys that are visible via a snapshot because we
  // anyways cannot delete it.
  const std::string value(10, 'x');
  for (int i = 0; i < 100001; i++) {
    char key[100];
    snprintf(key, sizeof(key), "b%08d%010d", i, i);
    put(key, value);
  }

  // push all files to lower levels
  dbfull()->test_flushmemtable();
  dbfull()->test_compactrange(0, nullptr, nullptr);
  dbfull()->test_compactrange(1, nullptr, nullptr);

  // verify that all keys now have the new value that
  // was set by the compaction process.
  for (int i = 0; i < 100001; i++) {
    char key[100];
    snprintf(key, sizeof(key), "b%08d%010d", i, i);
    std::string newvalue = get(key);
    assert_eq(newvalue.compare(new_value), 0);
  }
}

test(dbtest, compactionfilterv2nullprefix) {
  options options = currentoptions();
  options.num_levels = 3;
  options.max_mem_compaction_level = 0;
  std::unique_ptr<const slicetransform> prefix_extractor;
  prefix_extractor.reset(newfixedprefixtransform(8));
  options.compaction_filter_factory_v2 =
    std::make_shared<changefilterfactoryv2>(prefix_extractor.get());
  // in a testing environment, we can only flush the application
  // compaction filter buffer using universal compaction
  option_config_ = kuniversalcompaction;
  options.compaction_style = (rocksdb::compactionstyle)1;
  reopen(&options);

  // write 100k+1 keys, these are written to a few files
  // in l0. we do this so that the current snapshot points
  // to the 100001 key.the compaction filter is  not invoked
  // on keys that are visible via a snapshot because we
  // anyways cannot delete it.
  const std::string value(10, 'x');
  char first_key[100];
  snprintf(first_key, sizeof(first_key), "%s0000%010d", "null", 1);
  put(first_key, value);
  for (int i = 1; i < 100000; i++) {
    char key[100];
    snprintf(key, sizeof(key), "%08d%010d", i, i);
    put(key, value);
  }

  char last_key[100];
  snprintf(last_key, sizeof(last_key), "%s0000%010d", "null", 2);
  put(last_key, value);

  // push all files to lower levels
  dbfull()->test_flushmemtable();
  dbfull()->test_compactrange(0, nullptr, nullptr);

  // verify that all keys now have the new value that
  // was set by the compaction process.
  std::string newvalue = get(first_key);
  assert_eq(newvalue.compare(new_value), 0);
  newvalue = get(last_key);
  assert_eq(newvalue.compare(new_value), 0);
  for (int i = 1; i < 100000; i++) {
    char key[100];
    snprintf(key, sizeof(key), "%08d%010d", i, i);
    std::string newvalue = get(key);
    assert_eq(newvalue.compare(new_value), 0);
  }
}

test(dbtest, sparsemerge) {
  do {
    options options = currentoptions();
    options.compression = knocompression;
    createandreopenwithcf({"pikachu"}, &options);

    filllevels("a", "z", 1);

    // suppose there is:
    //    small amount of data with prefix a
    //    large amount of data with prefix b
    //    small amount of data with prefix c
    // and that recent updates have made small changes to all three prefixes.
    // check that we do not do a compaction that merges all of b in one shot.
    const std::string value(1000, 'x');
    put(1, "a", "va");
    // write approximately 100mb of "b" values
    for (int i = 0; i < 100000; i++) {
      char key[100];
      snprintf(key, sizeof(key), "b%010d", i);
      put(1, key, value);
    }
    put(1, "c", "vc");
    assert_ok(flush(1));
    dbfull()->test_compactrange(0, nullptr, nullptr, handles_[1]);

    // make sparse update
    put(1, "a", "va2");
    put(1, "b100", "bvalue2");
    put(1, "c", "vc2");
    assert_ok(flush(1));

    // compactions should not cause us to create a situation where
    // a file overlaps too much data at the next level.
    assert_le(dbfull()->test_maxnextleveloverlappingbytes(handles_[1]),
              20 * 1048576);
    dbfull()->test_compactrange(0, nullptr, nullptr);
    assert_le(dbfull()->test_maxnextleveloverlappingbytes(handles_[1]),
              20 * 1048576);
    dbfull()->test_compactrange(1, nullptr, nullptr);
    assert_le(dbfull()->test_maxnextleveloverlappingbytes(handles_[1]),
              20 * 1048576);
  } while (changecompactoptions());
}

static bool between(uint64_t val, uint64_t low, uint64_t high) {
  bool result = (val >= low) && (val <= high);
  if (!result) {
    fprintf(stderr, "value %llu is not in range [%llu, %llu]\n",
            (unsigned long long)(val),
            (unsigned long long)(low),
            (unsigned long long)(high));
  }
  return result;
}

test(dbtest, approximatesizes) {
  do {
    options options;
    options.write_buffer_size = 100000000;        // large write buffer
    options.compression = knocompression;
    options = currentoptions(options);
    destroyandreopen();
    createandreopenwithcf({"pikachu"}, &options);

    assert_true(between(size("", "xyz", 1), 0, 0));
    reopenwithcolumnfamilies({"default", "pikachu"}, &options);
    assert_true(between(size("", "xyz", 1), 0, 0));

    // write 8mb (80 values, each 100k)
    assert_eq(numtablefilesatlevel(0, 1), 0);
    const int n = 80;
    static const int s1 = 100000;
    static const int s2 = 105000;  // allow some expansion from metadata
    random rnd(301);
    for (int i = 0; i < n; i++) {
      assert_ok(put(1, key(i), randomstring(&rnd, s1)));
    }

    // 0 because getapproximatesizes() does not account for memtable space
    assert_true(between(size("", key(50), 1), 0, 0));

    // check sizes across recovery by reopening a few times
    for (int run = 0; run < 3; run++) {
      reopenwithcolumnfamilies({"default", "pikachu"}, &options);

      for (int compact_start = 0; compact_start < n; compact_start += 10) {
        for (int i = 0; i < n; i += 10) {
          assert_true(between(size("", key(i), 1), s1 * i, s2 * i));
          assert_true(between(size("", key(i) + ".suffix", 1), s1 * (i + 1),
                              s2 * (i + 1)));
          assert_true(between(size(key(i), key(i + 10), 1), s1 * 10, s2 * 10));
        }
        assert_true(between(size("", key(50), 1), s1 * 50, s2 * 50));
        assert_true(
            between(size("", key(50) + ".suffix", 1), s1 * 50, s2 * 50));

        std::string cstart_str = key(compact_start);
        std::string cend_str = key(compact_start + 9);
        slice cstart = cstart_str;
        slice cend = cend_str;
        dbfull()->test_compactrange(0, &cstart, &cend, handles_[1]);
      }

      assert_eq(numtablefilesatlevel(0, 1), 0);
      assert_gt(numtablefilesatlevel(1, 1), 0);
    }
    // approximateoffsetof() is not yet implemented in plain table format.
  } while (changeoptions(kskipuniversalcompaction | kskipfifocompaction |
                         kskipplaintable | kskiphashindex));
}

test(dbtest, approximatesizes_mixofsmallandlarge) {
  do {
    options options = currentoptions();
    options.compression = knocompression;
    createandreopenwithcf({"pikachu"}, &options);

    random rnd(301);
    std::string big1 = randomstring(&rnd, 100000);
    assert_ok(put(1, key(0), randomstring(&rnd, 10000)));
    assert_ok(put(1, key(1), randomstring(&rnd, 10000)));
    assert_ok(put(1, key(2), big1));
    assert_ok(put(1, key(3), randomstring(&rnd, 10000)));
    assert_ok(put(1, key(4), big1));
    assert_ok(put(1, key(5), randomstring(&rnd, 10000)));
    assert_ok(put(1, key(6), randomstring(&rnd, 300000)));
    assert_ok(put(1, key(7), randomstring(&rnd, 10000)));

    // check sizes across recovery by reopening a few times
    for (int run = 0; run < 3; run++) {
      reopenwithcolumnfamilies({"default", "pikachu"}, &options);

      assert_true(between(size("", key(0), 1), 0, 0));
      assert_true(between(size("", key(1), 1), 10000, 11000));
      assert_true(between(size("", key(2), 1), 20000, 21000));
      assert_true(between(size("", key(3), 1), 120000, 121000));
      assert_true(between(size("", key(4), 1), 130000, 131000));
      assert_true(between(size("", key(5), 1), 230000, 231000));
      assert_true(between(size("", key(6), 1), 240000, 241000));
      assert_true(between(size("", key(7), 1), 540000, 541000));
      assert_true(between(size("", key(8), 1), 550000, 560000));

      assert_true(between(size(key(3), key(5), 1), 110000, 111000));

      dbfull()->test_compactrange(0, nullptr, nullptr, handles_[1]);
    }
    // approximateoffsetof() is not yet implemented in plain table format.
  } while (changeoptions(kskipplaintable));
}

test(dbtest, iteratorpinsref) {
  do {
    createandreopenwithcf({"pikachu"});
    put(1, "foo", "hello");

    // get iterator that will yield the current contents of the db.
    iterator* iter = db_->newiterator(readoptions(), handles_[1]);

    // write to force compactions
    put(1, "foo", "newvalue1");
    for (int i = 0; i < 100; i++) {
      // 100k values
      assert_ok(put(1, key(i), key(i) + std::string(100000, 'v')));
    }
    put(1, "foo", "newvalue2");

    iter->seektofirst();
    assert_true(iter->valid());
    assert_eq("foo", iter->key().tostring());
    assert_eq("hello", iter->value().tostring());
    iter->next();
    assert_true(!iter->valid());
    delete iter;
  } while (changecompactoptions());
}

test(dbtest, snapshot) {
  do {
    createandreopenwithcf({"pikachu"});
    put(0, "foo", "0v1");
    put(1, "foo", "1v1");
    const snapshot* s1 = db_->getsnapshot();
    put(0, "foo", "0v2");
    put(1, "foo", "1v2");
    const snapshot* s2 = db_->getsnapshot();
    put(0, "foo", "0v3");
    put(1, "foo", "1v3");
    const snapshot* s3 = db_->getsnapshot();

    put(0, "foo", "0v4");
    put(1, "foo", "1v4");
    assert_eq("0v1", get(0, "foo", s1));
    assert_eq("1v1", get(1, "foo", s1));
    assert_eq("0v2", get(0, "foo", s2));
    assert_eq("1v2", get(1, "foo", s2));
    assert_eq("0v3", get(0, "foo", s3));
    assert_eq("1v3", get(1, "foo", s3));
    assert_eq("0v4", get(0, "foo"));
    assert_eq("1v4", get(1, "foo"));

    db_->releasesnapshot(s3);
    assert_eq("0v1", get(0, "foo", s1));
    assert_eq("1v1", get(1, "foo", s1));
    assert_eq("0v2", get(0, "foo", s2));
    assert_eq("1v2", get(1, "foo", s2));
    assert_eq("0v4", get(0, "foo"));
    assert_eq("1v4", get(1, "foo"));

    db_->releasesnapshot(s1);
    assert_eq("0v2", get(0, "foo", s2));
    assert_eq("1v2", get(1, "foo", s2));
    assert_eq("0v4", get(0, "foo"));
    assert_eq("1v4", get(1, "foo"));

    db_->releasesnapshot(s2);
    assert_eq("0v4", get(0, "foo"));
    assert_eq("1v4", get(1, "foo"));
  } while (changeoptions(kskiphashcuckoo));
}

test(dbtest, hiddenvaluesareremoved) {
  do {
    createandreopenwithcf({"pikachu"});
    random rnd(301);
    filllevels("a", "z", 1);

    std::string big = randomstring(&rnd, 50000);
    put(1, "foo", big);
    put(1, "pastfoo", "v");
    const snapshot* snapshot = db_->getsnapshot();
    put(1, "foo", "tiny");
    put(1, "pastfoo2", "v2");  // advance sequence number one more

    assert_ok(flush(1));
    assert_gt(numtablefilesatlevel(0, 1), 0);

    assert_eq(big, get(1, "foo", snapshot));
    assert_true(between(size("", "pastfoo", 1), 50000, 60000));
    db_->releasesnapshot(snapshot);
    assert_eq(allentriesfor("foo", 1), "[ tiny, " + big + " ]");
    slice x("x");
    dbfull()->test_compactrange(0, nullptr, &x, handles_[1]);
    assert_eq(allentriesfor("foo", 1), "[ tiny ]");
    assert_eq(numtablefilesatlevel(0, 1), 0);
    assert_ge(numtablefilesatlevel(1, 1), 1);
    dbfull()->test_compactrange(1, nullptr, &x, handles_[1]);
    assert_eq(allentriesfor("foo", 1), "[ tiny ]");

    assert_true(between(size("", "pastfoo", 1), 0, 1000));
    // approximateoffsetof() is not yet implemented in plain table format,
    // which is used by size().
    // skip hashcuckoorep as it does not support snapshot
  } while (changeoptions(kskipuniversalcompaction | kskipfifocompaction |
                         kskipplaintable | kskiphashcuckoo));
}

test(dbtest, compactbetweensnapshots) {
  do {
    options options = currentoptions();
    options.disable_auto_compactions = true;
    createandreopenwithcf({"pikachu"});
    random rnd(301);
    filllevels("a", "z", 1);

    put(1, "foo", "first");
    const snapshot* snapshot1 = db_->getsnapshot();
    put(1, "foo", "second");
    put(1, "foo", "third");
    put(1, "foo", "fourth");
    const snapshot* snapshot2 = db_->getsnapshot();
    put(1, "foo", "fifth");
    put(1, "foo", "sixth");

    // all entries (including duplicates) exist
    // before any compaction is triggered.
    assert_ok(flush(1));
    assert_eq("sixth", get(1, "foo"));
    assert_eq("fourth", get(1, "foo", snapshot2));
    assert_eq("first", get(1, "foo", snapshot1));
    assert_eq(allentriesfor("foo", 1),
              "[ sixth, fifth, fourth, third, second, first ]");

    // after a compaction, "second", "third" and "fifth" should
    // be removed
    filllevels("a", "z", 1);
    dbfull()->compactrange(handles_[1], nullptr, nullptr);
    assert_eq("sixth", get(1, "foo"));
    assert_eq("fourth", get(1, "foo", snapshot2));
    assert_eq("first", get(1, "foo", snapshot1));
    assert_eq(allentriesfor("foo", 1), "[ sixth, fourth, first ]");

    // after we release the snapshot1, only two values left
    db_->releasesnapshot(snapshot1);
    filllevels("a", "z", 1);
    dbfull()->compactrange(handles_[1], nullptr, nullptr);

    // we have only one valid snapshot snapshot2. since snapshot1 is
    // not valid anymore, "first" should be removed by a compaction.
    assert_eq("sixth", get(1, "foo"));
    assert_eq("fourth", get(1, "foo", snapshot2));
    assert_eq(allentriesfor("foo", 1), "[ sixth, fourth ]");

    // after we release the snapshot2, only one value should be left
    db_->releasesnapshot(snapshot2);
    filllevels("a", "z", 1);
    dbfull()->compactrange(handles_[1], nullptr, nullptr);
    assert_eq("sixth", get(1, "foo"));
    assert_eq(allentriesfor("foo", 1), "[ sixth ]");
    // skip hashcuckoorep as it does not support snapshot
  } while (changeoptions(kskiphashcuckoo | kskipfifocompaction));
}

test(dbtest, deletionmarkers1) {
  createandreopenwithcf({"pikachu"});
  put(1, "foo", "v1");
  assert_ok(flush(1));
  const int last = currentoptions().max_mem_compaction_level;
  // foo => v1 is now in last level
  assert_eq(numtablefilesatlevel(last, 1), 1);

  // place a table at level last-1 to prevent merging with preceding mutation
  put(1, "a", "begin");
  put(1, "z", "end");
  flush(1);
  assert_eq(numtablefilesatlevel(last, 1), 1);
  assert_eq(numtablefilesatlevel(last - 1, 1), 1);

  delete(1, "foo");
  put(1, "foo", "v2");
  assert_eq(allentriesfor("foo", 1), "[ v2, del, v1 ]");
  assert_ok(flush(1));  // moves to level last-2
  if (currentoptions().purge_redundant_kvs_while_flush) {
    assert_eq(allentriesfor("foo", 1), "[ v2, v1 ]");
  } else {
    assert_eq(allentriesfor("foo", 1), "[ v2, del, v1 ]");
  }
  slice z("z");
  dbfull()->test_compactrange(last - 2, nullptr, &z, handles_[1]);
  // del eliminated, but v1 remains because we aren't compacting that level
  // (del can be eliminated because v2 hides v1).
  assert_eq(allentriesfor("foo", 1), "[ v2, v1 ]");
  dbfull()->test_compactrange(last - 1, nullptr, nullptr, handles_[1]);
  // merging last-1 w/ last, so we are the base level for "foo", so
  // del is removed.  (as is v1).
  assert_eq(allentriesfor("foo", 1), "[ v2 ]");
}

test(dbtest, deletionmarkers2) {
  createandreopenwithcf({"pikachu"});
  put(1, "foo", "v1");
  assert_ok(flush(1));
  const int last = currentoptions().max_mem_compaction_level;
  // foo => v1 is now in last level
  assert_eq(numtablefilesatlevel(last, 1), 1);

  // place a table at level last-1 to prevent merging with preceding mutation
  put(1, "a", "begin");
  put(1, "z", "end");
  flush(1);
  assert_eq(numtablefilesatlevel(last, 1), 1);
  assert_eq(numtablefilesatlevel(last - 1, 1), 1);

  delete(1, "foo");
  assert_eq(allentriesfor("foo", 1), "[ del, v1 ]");
  assert_ok(flush(1));  // moves to level last-2
  assert_eq(allentriesfor("foo", 1), "[ del, v1 ]");
  dbfull()->test_compactrange(last - 2, nullptr, nullptr, handles_[1]);
  // del kept: "last" file overlaps
  assert_eq(allentriesfor("foo", 1), "[ del, v1 ]");
  dbfull()->test_compactrange(last - 1, nullptr, nullptr, handles_[1]);
  // merging last-1 w/ last, so we are the base level for "foo", so
  // del is removed.  (as is v1).
  assert_eq(allentriesfor("foo", 1), "[ ]");
}

test(dbtest, overlapinlevel0) {
  do {
    createandreopenwithcf({"pikachu"});
    int tmp = currentoptions().max_mem_compaction_level;
    assert_eq(tmp, 2) << "fix test to match config";

    //fill levels 1 and 2 to disable the pushing of new memtables to levels > 0.
    assert_ok(put(1, "100", "v100"));
    assert_ok(put(1, "999", "v999"));
    flush(1);
    assert_ok(delete(1, "100"));
    assert_ok(delete(1, "999"));
    flush(1);
    assert_eq("0,1,1", filesperlevel(1));

    // make files spanning the following ranges in level-0:
    //  files[0]  200 .. 900
    //  files[1]  300 .. 500
    // note that files are sorted by smallest key.
    assert_ok(put(1, "300", "v300"));
    assert_ok(put(1, "500", "v500"));
    flush(1);
    assert_ok(put(1, "200", "v200"));
    assert_ok(put(1, "600", "v600"));
    assert_ok(put(1, "900", "v900"));
    flush(1);
    assert_eq("2,1,1", filesperlevel(1));

    // compact away the placeholder files we created initially
    dbfull()->test_compactrange(1, nullptr, nullptr, handles_[1]);
    dbfull()->test_compactrange(2, nullptr, nullptr, handles_[1]);
    assert_eq("2", filesperlevel(1));

    // do a memtable compaction.  before bug-fix, the compaction would
    // not detect the overlap with level-0 files and would incorrectly place
    // the deletion in a deeper level.
    assert_ok(delete(1, "600"));
    flush(1);
    assert_eq("3", filesperlevel(1));
    assert_eq("not_found", get(1, "600"));
  } while (changeoptions(kskipuniversalcompaction | kskipfifocompaction));
}

test(dbtest, l0_compactionbug_issue44_a) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "b", "v"));
    reopenwithcolumnfamilies({"default", "pikachu"});
    assert_ok(delete(1, "b"));
    assert_ok(delete(1, "a"));
    reopenwithcolumnfamilies({"default", "pikachu"});
    assert_ok(delete(1, "a"));
    reopenwithcolumnfamilies({"default", "pikachu"});
    assert_ok(put(1, "a", "v"));
    reopenwithcolumnfamilies({"default", "pikachu"});
    reopenwithcolumnfamilies({"default", "pikachu"});
    assert_eq("(a->v)", contents(1));
    env_->sleepformicroseconds(1000000);  // wait for compaction to finish
    assert_eq("(a->v)", contents(1));
  } while (changecompactoptions());
}

test(dbtest, l0_compactionbug_issue44_b) {
  do {
    createandreopenwithcf({"pikachu"});
    put(1, "", "");
    reopenwithcolumnfamilies({"default", "pikachu"});
    delete(1, "e");
    put(1, "", "");
    reopenwithcolumnfamilies({"default", "pikachu"});
    put(1, "c", "cv");
    reopenwithcolumnfamilies({"default", "pikachu"});
    put(1, "", "");
    reopenwithcolumnfamilies({"default", "pikachu"});
    put(1, "", "");
    env_->sleepformicroseconds(1000000);  // wait for compaction to finish
    reopenwithcolumnfamilies({"default", "pikachu"});
    put(1, "d", "dv");
    reopenwithcolumnfamilies({"default", "pikachu"});
    put(1, "", "");
    reopenwithcolumnfamilies({"default", "pikachu"});
    delete(1, "d");
    delete(1, "b");
    reopenwithcolumnfamilies({"default", "pikachu"});
    assert_eq("(->)(c->cv)", contents(1));
    env_->sleepformicroseconds(1000000);  // wait for compaction to finish
    assert_eq("(->)(c->cv)", contents(1));
  } while (changecompactoptions());
}

test(dbtest, comparatorcheck) {
  class newcomparator : public comparator {
   public:
    virtual const char* name() const { return "rocksdb.newcomparator"; }
    virtual int compare(const slice& a, const slice& b) const {
      return bytewisecomparator()->compare(a, b);
    }
    virtual void findshortestseparator(std::string* s, const slice& l) const {
      bytewisecomparator()->findshortestseparator(s, l);
    }
    virtual void findshortsuccessor(std::string* key) const {
      bytewisecomparator()->findshortsuccessor(key);
    }
  };
  options new_options, options;
  newcomparator cmp;
  do {
    createandreopenwithcf({"pikachu"});
    options = currentoptions();
    new_options = currentoptions();
    new_options.comparator = &cmp;
    // only the non-default column family has non-matching comparator
    status s = tryreopenwithcolumnfamilies({"default", "pikachu"},
                                           {&options, &new_options});
    assert_true(!s.ok());
    assert_true(s.tostring().find("comparator") != std::string::npos)
        << s.tostring();
  } while (changecompactoptions(&new_options));
}

test(dbtest, customcomparator) {
  class numbercomparator : public comparator {
   public:
    virtual const char* name() const { return "test.numbercomparator"; }
    virtual int compare(const slice& a, const slice& b) const {
      return tonumber(a) - tonumber(b);
    }
    virtual void findshortestseparator(std::string* s, const slice& l) const {
      tonumber(*s);     // check format
      tonumber(l);      // check format
    }
    virtual void findshortsuccessor(std::string* key) const {
      tonumber(*key);   // check format
    }
   private:
    static int tonumber(const slice& x) {
      // check that there are no extra characters.
      assert_true(x.size() >= 2 && x[0] == '[' && x[x.size()-1] == ']')
          << escapestring(x);
      int val;
      char ignored;
      assert_true(sscanf(x.tostring().c_str(), "[%i]%c", &val, &ignored) == 1)
          << escapestring(x);
      return val;
    }
  };
  options new_options;
  numbercomparator cmp;
  do {
    new_options = currentoptions();
    new_options.create_if_missing = true;
    new_options.comparator = &cmp;
    new_options.write_buffer_size = 1000;  // compact more often
    new_options = currentoptions(new_options);
    destroyandreopen(&new_options);
    createandreopenwithcf({"pikachu"}, &new_options);
    assert_ok(put(1, "[10]", "ten"));
    assert_ok(put(1, "[0x14]", "twenty"));
    for (int i = 0; i < 2; i++) {
      assert_eq("ten", get(1, "[10]"));
      assert_eq("ten", get(1, "[0xa]"));
      assert_eq("twenty", get(1, "[20]"));
      assert_eq("twenty", get(1, "[0x14]"));
      assert_eq("not_found", get(1, "[15]"));
      assert_eq("not_found", get(1, "[0xf]"));
      compact(1, "[0]", "[9999]");
    }

    for (int run = 0; run < 2; run++) {
      for (int i = 0; i < 1000; i++) {
        char buf[100];
        snprintf(buf, sizeof(buf), "[%d]", i*10);
        assert_ok(put(1, buf, buf));
      }
      compact(1, "[0]", "[1000000]");
    }
  } while (changecompactoptions(&new_options));
}

test(dbtest, manualcompaction) {
  createandreopenwithcf({"pikachu"});
  assert_eq(dbfull()->maxmemcompactionlevel(), 2)
      << "need to update this test to match kmaxmemcompactlevel";

  // iter - 0 with 7 levels
  // iter - 1 with 3 levels
  for (int iter = 0; iter < 2; ++iter) {
    maketables(3, "p", "q", 1);
    assert_eq("1,1,1", filesperlevel(1));

    // compaction range falls before files
    compact(1, "", "c");
    assert_eq("1,1,1", filesperlevel(1));

    // compaction range falls after files
    compact(1, "r", "z");
    assert_eq("1,1,1", filesperlevel(1));

    // compaction range overlaps files
    compact(1, "p1", "p9");
    assert_eq("0,0,1", filesperlevel(1));

    // populate a different range
    maketables(3, "c", "e", 1);
    assert_eq("1,1,2", filesperlevel(1));

    // compact just the new range
    compact(1, "b", "f");
    assert_eq("0,0,2", filesperlevel(1));

    // compact all
    maketables(1, "a", "z", 1);
    assert_eq("0,1,2", filesperlevel(1));
    db_->compactrange(handles_[1], nullptr, nullptr);
    assert_eq("0,0,1", filesperlevel(1));

    if (iter == 0) {
      options options = currentoptions();
      options.num_levels = 3;
      options.create_if_missing = true;
      destroyandreopen(&options);
      createandreopenwithcf({"pikachu"}, &options);
    }
  }

}

test(dbtest, manualcompactionoutputpathid) {
  options options = currentoptions();
  options.create_if_missing = true;
  options.db_paths.emplace_back(dbname_, 1000000000);
  options.db_paths.emplace_back(dbname_ + "_2", 1000000000);
  options.compaction_style = kcompactionstyleuniversal;
  options.level0_file_num_compaction_trigger = 10;
  destroy(&options);
  destroyandreopen(&options);
  createandreopenwithcf({"pikachu"}, &options);
  maketables(3, "p", "q", 1);
  dbfull()->test_waitforcompact();
  assert_eq("3", filesperlevel(1));
  assert_eq(3, getsstfilecount(options.db_paths[0].path));
  assert_eq(0, getsstfilecount(options.db_paths[1].path));

  // full compaction to db path 0
  db_->compactrange(handles_[1], nullptr, nullptr, false, -1, 1);
  assert_eq("1", filesperlevel(1));
  assert_eq(0, getsstfilecount(options.db_paths[0].path));
  assert_eq(1, getsstfilecount(options.db_paths[1].path));

  reopenwithcolumnfamilies({kdefaultcolumnfamilyname, "pikachu"}, &options);
  assert_eq("1", filesperlevel(1));
  assert_eq(0, getsstfilecount(options.db_paths[0].path));
  assert_eq(1, getsstfilecount(options.db_paths[1].path));

  maketables(1, "p", "q", 1);
  assert_eq("2", filesperlevel(1));
  assert_eq(1, getsstfilecount(options.db_paths[0].path));
  assert_eq(1, getsstfilecount(options.db_paths[1].path));

  reopenwithcolumnfamilies({kdefaultcolumnfamilyname, "pikachu"}, &options);
  assert_eq("2", filesperlevel(1));
  assert_eq(1, getsstfilecount(options.db_paths[0].path));
  assert_eq(1, getsstfilecount(options.db_paths[1].path));

  // full compaction to db path 0
  db_->compactrange(handles_[1], nullptr, nullptr, false, -1, 0);
  assert_eq("1", filesperlevel(1));
  assert_eq(1, getsstfilecount(options.db_paths[0].path));
  assert_eq(0, getsstfilecount(options.db_paths[1].path));

  // fail when compacting to an invalid path id
  assert_true(db_->compactrange(handles_[1], nullptr, nullptr, false, -1, 2)
                  .isinvalidargument());
}

test(dbtest, dbopen_options) {
  std::string dbname = test::tmpdir() + "/db_options_test";
  assert_ok(destroydb(dbname, options()));

  // does not exist, and create_if_missing == false: error
  db* db = nullptr;
  options opts;
  opts.create_if_missing = false;
  status s = db::open(opts, dbname, &db);
  assert_true(strstr(s.tostring().c_str(), "does not exist") != nullptr);
  assert_true(db == nullptr);

  // does not exist, and create_if_missing == true: ok
  opts.create_if_missing = true;
  s = db::open(opts, dbname, &db);
  assert_ok(s);
  assert_true(db != nullptr);

  delete db;
  db = nullptr;

  // does exist, and error_if_exists == true: error
  opts.create_if_missing = false;
  opts.error_if_exists = true;
  s = db::open(opts, dbname, &db);
  assert_true(strstr(s.tostring().c_str(), "exists") != nullptr);
  assert_true(db == nullptr);

  // does exist, and error_if_exists == false: ok
  opts.create_if_missing = true;
  opts.error_if_exists = false;
  s = db::open(opts, dbname, &db);
  assert_ok(s);
  assert_true(db != nullptr);

  delete db;
  db = nullptr;
}

test(dbtest, dbopen_change_numlevels) {
  options opts;
  opts.create_if_missing = true;
  destroyandreopen(&opts);
  assert_true(db_ != nullptr);
  createandreopenwithcf({"pikachu"}, &opts);

  assert_ok(put(1, "a", "123"));
  assert_ok(put(1, "b", "234"));
  db_->compactrange(handles_[1], nullptr, nullptr);
  close();

  opts.create_if_missing = false;
  opts.num_levels = 2;
  status s = tryreopenwithcolumnfamilies({"default", "pikachu"}, &opts);
  assert_true(strstr(s.tostring().c_str(), "invalid argument") != nullptr);
  assert_true(db_ == nullptr);
}

test(dbtest, destroydbmetadatabase) {
  std::string dbname = test::tmpdir() + "/db_meta";
  std::string metadbname = metadatabasename(dbname, 0);
  std::string metametadbname = metadatabasename(metadbname, 0);

  // destroy previous versions if they exist. using the long way.
  assert_ok(destroydb(metametadbname, options()));
  assert_ok(destroydb(metadbname, options()));
  assert_ok(destroydb(dbname, options()));

  // setup databases
  options opts;
  opts.create_if_missing = true;
  db* db = nullptr;
  assert_ok(db::open(opts, dbname, &db));
  delete db;
  db = nullptr;
  assert_ok(db::open(opts, metadbname, &db));
  delete db;
  db = nullptr;
  assert_ok(db::open(opts, metametadbname, &db));
  delete db;
  db = nullptr;

  // delete databases
  assert_ok(destroydb(dbname, options()));

  // check if deletion worked.
  opts.create_if_missing = false;
  assert_true(!(db::open(opts, dbname, &db)).ok());
  assert_true(!(db::open(opts, metadbname, &db)).ok());
  assert_true(!(db::open(opts, metametadbname, &db)).ok());
}

// check that number of files does not grow when we are out of space
test(dbtest, nospace) {
  do {
    options options = currentoptions();
    options.env = env_;
    options.paranoid_checks = false;
    reopen(&options);

    assert_ok(put("foo", "v1"));
    assert_eq("v1", get("foo"));
    compact("a", "z");
    const int num_files = countfiles();
    env_->no_space_.release_store(env_);   // force out-of-space errors
    env_->sleep_counter_.reset();
    for (int i = 0; i < 5; i++) {
      for (int level = 0; level < dbfull()->numberlevels()-1; level++) {
        dbfull()->test_compactrange(level, nullptr, nullptr);
      }
    }

    std::string property_value;
    assert_true(db_->getproperty("rocksdb.background-errors", &property_value));
    assert_eq("5", property_value);

    env_->no_space_.release_store(nullptr);
    assert_lt(countfiles(), num_files + 3);

    // check that compaction attempts slept after errors
    assert_ge(env_->sleep_counter_.read(), 5);
  } while (changecompactoptions());
}

// check background error counter bumped on flush failures.
test(dbtest, nospaceflush) {
  do {
    options options = currentoptions();
    options.env = env_;
    options.max_background_flushes = 1;
    reopen(&options);

    assert_ok(put("foo", "v1"));
    env_->no_space_.release_store(env_);  // force out-of-space errors

    std::string property_value;
    // background error count is 0 now.
    assert_true(db_->getproperty("rocksdb.background-errors", &property_value));
    assert_eq("0", property_value);

    dbfull()->test_flushmemtable(false);

    // wait 300 milliseconds or background-errors turned 1 from 0.
    int time_to_sleep_limit = 300000;
    while (time_to_sleep_limit > 0) {
      int to_sleep = (time_to_sleep_limit > 1000) ? 1000 : time_to_sleep_limit;
      time_to_sleep_limit -= to_sleep;
      env_->sleepformicroseconds(to_sleep);

      assert_true(
          db_->getproperty("rocksdb.background-errors", &property_value));
      if (property_value == "1") {
        break;
      }
    }
    assert_eq("1", property_value);

    env_->no_space_.release_store(nullptr);
  } while (changecompactoptions());
}

test(dbtest, nonwritablefilesystem) {
  do {
    options options = currentoptions();
    options.write_buffer_size = 1000;
    options.env = env_;
    reopen(&options);
    assert_ok(put("foo", "v1"));
    env_->non_writable_.release_store(env_); // force errors for new files
    std::string big(100000, 'x');
    int errors = 0;
    for (int i = 0; i < 20; i++) {
      if (!put("foo", big).ok()) {
        errors++;
        env_->sleepformicroseconds(100000);
      }
    }
    assert_gt(errors, 0);
    env_->non_writable_.release_store(nullptr);
  } while (changecompactoptions());
}

test(dbtest, manifestwriteerror) {
  // test for the following problem:
  // (a) compaction produces file f
  // (b) log record containing f is written to manifest file, but sync() fails
  // (c) gc deletes f
  // (d) after reopening db, reads fail since deleted f is named in log record

  // we iterate twice.  in the second iteration, everything is the
  // same except the log record never makes it to the manifest file.
  for (int iter = 0; iter < 2; iter++) {
    port::atomicpointer* error_type = (iter == 0)
        ? &env_->manifest_sync_error_
        : &env_->manifest_write_error_;

    // insert foo=>bar mapping
    options options = currentoptions();
    options.env = env_;
    options.create_if_missing = true;
    options.error_if_exists = false;
    destroyandreopen(&options);
    assert_ok(put("foo", "bar"));
    assert_eq("bar", get("foo"));

    // memtable compaction (will succeed)
    flush();
    assert_eq("bar", get("foo"));
    const int last = dbfull()->maxmemcompactionlevel();
    assert_eq(numtablefilesatlevel(last), 1);   // foo=>bar is now in last level

    // merging compaction (will fail)
    error_type->release_store(env_);
    dbfull()->test_compactrange(last, nullptr, nullptr);  // should fail
    assert_eq("bar", get("foo"));

    // recovery: should not lose data
    error_type->release_store(nullptr);
    reopen(&options);
    assert_eq("bar", get("foo"));
  }
}

test(dbtest, putfailsparanoid) {
  // test the following:
  // (a) a random put fails in paranoid mode (simulate by sync fail)
  // (b) all other puts have to fail, even if writes would succeed
  // (c) all of that should happen only if paranoid_checks = true

  options options = currentoptions();
  options.env = env_;
  options.create_if_missing = true;
  options.error_if_exists = false;
  options.paranoid_checks = true;
  destroyandreopen(&options);
  createandreopenwithcf({"pikachu"}, &options);
  status s;

  assert_ok(put(1, "foo", "bar"));
  assert_ok(put(1, "foo1", "bar1"));
  // simulate error
  env_->log_write_error_.release_store(env_);
  s = put(1, "foo2", "bar2");
  assert_true(!s.ok());
  env_->log_write_error_.release_store(nullptr);
  s = put(1, "foo3", "bar3");
  // the next put should fail, too
  assert_true(!s.ok());
  // but we're still able to read
  assert_eq("bar", get(1, "foo"));

  // do the same thing with paranoid checks off
  options.paranoid_checks = false;
  destroyandreopen(&options);
  createandreopenwithcf({"pikachu"}, &options);

  assert_ok(put(1, "foo", "bar"));
  assert_ok(put(1, "foo1", "bar1"));
  // simulate error
  env_->log_write_error_.release_store(env_);
  s = put(1, "foo2", "bar2");
  assert_true(!s.ok());
  env_->log_write_error_.release_store(nullptr);
  s = put(1, "foo3", "bar3");
  // the next put should not fail
  assert_true(s.ok());
}

test(dbtest, filesdeletedaftercompaction) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "foo", "v2"));
    compact(1, "a", "z");
    const int num_files = countlivefiles();
    for (int i = 0; i < 10; i++) {
      assert_ok(put(1, "foo", "v2"));
      compact(1, "a", "z");
    }
    assert_eq(countlivefiles(), num_files);
  } while (changecompactoptions());
}

test(dbtest, bloomfilter) {
  do {
    options options = currentoptions();
    env_->count_random_reads_ = true;
    options.env = env_;
    // changecompactoptions() only changes compaction style, which does not
    // trigger reset of table_factory
    blockbasedtableoptions table_options;
    table_options.no_block_cache = true;
    table_options.filter_policy.reset(newbloomfilterpolicy(10));
    options.table_factory.reset(newblockbasedtablefactory(table_options));

    createandreopenwithcf({"pikachu"}, &options);

    // populate multiple layers
    const int n = 10000;
    for (int i = 0; i < n; i++) {
      assert_ok(put(1, key(i), key(i)));
    }
    compact(1, "a", "z");
    for (int i = 0; i < n; i += 100) {
      assert_ok(put(1, key(i), key(i)));
    }
    flush(1);

    // prevent auto compactions triggered by seeks
    env_->delay_sstable_sync_.release_store(env_);

    // lookup present keys.  should rarely read from small sstable.
    env_->random_read_counter_.reset();
    for (int i = 0; i < n; i++) {
      assert_eq(key(i), get(1, key(i)));
    }
    int reads = env_->random_read_counter_.read();
    fprintf(stderr, "%d present => %d reads\n", n, reads);
    assert_ge(reads, n);
    assert_le(reads, n + 2*n/100);

    // lookup present keys.  should rarely read from either sstable.
    env_->random_read_counter_.reset();
    for (int i = 0; i < n; i++) {
      assert_eq("not_found", get(1, key(i) + ".missing"));
    }
    reads = env_->random_read_counter_.read();
    fprintf(stderr, "%d missing => %d reads\n", n, reads);
    assert_le(reads, 3*n/100);

    env_->delay_sstable_sync_.release_store(nullptr);
    close();
  } while (changecompactoptions());
}

test(dbtest, snapshotfiles) {
  do {
    options options = currentoptions();
    options.write_buffer_size = 100000000;        // large write buffer
    createandreopenwithcf({"pikachu"}, &options);

    random rnd(301);

    // write 8mb (80 values, each 100k)
    assert_eq(numtablefilesatlevel(0, 1), 0);
    std::vector<std::string> values;
    for (int i = 0; i < 80; i++) {
      values.push_back(randomstring(&rnd, 100000));
      assert_ok(put((i < 40), key(i), values[i]));
    }

    // assert that nothing makes it to disk yet.
    assert_eq(numtablefilesatlevel(0, 1), 0);

    // get a file snapshot
    uint64_t manifest_number = 0;
    uint64_t manifest_size = 0;
    std::vector<std::string> files;
    dbfull()->disablefiledeletions();
    dbfull()->getlivefiles(files, &manifest_size);

    // current, manifest, *.sst files (one for each cf)
    assert_eq(files.size(), 4u);

    uint64_t number = 0;
    filetype type;

    // copy these files to a new snapshot directory
    std::string snapdir = dbname_ + ".snapdir/";
    std::string mkdir = "mkdir -p " + snapdir;
    assert_eq(system(mkdir.c_str()), 0);

    for (unsigned int i = 0; i < files.size(); i++) {
      // our clients require that getlivefiles returns
      // files with "/" as first character!
      assert_eq(files[i][0], '/');
      std::string src = dbname_ + files[i];
      std::string dest = snapdir + files[i];

      uint64_t size;
      assert_ok(env_->getfilesize(src, &size));

      // record the number and the size of the
      // latest manifest file
      if (parsefilename(files[i].substr(1), &number, &type)) {
        if (type == kdescriptorfile) {
          if (number > manifest_number) {
            manifest_number = number;
            assert_ge(size, manifest_size);
            size = manifest_size; // copy only valid manifest data
          }
        }
      }
      copyfile(src, dest, size);
    }

    // release file snapshot
    dbfull()->disablefiledeletions();

    // overwrite one key, this key should not appear in the snapshot
    std::vector<std::string> extras;
    for (unsigned int i = 0; i < 1; i++) {
      extras.push_back(randomstring(&rnd, 100000));
      assert_ok(put(0, key(i), extras[i]));
    }

    // verify that data in the snapshot are correct
    std::vector<columnfamilydescriptor> column_families;
    column_families.emplace_back("default", columnfamilyoptions());
    column_families.emplace_back("pikachu", columnfamilyoptions());
    std::vector<columnfamilyhandle*> cf_handles;
    db* snapdb;
    dboptions opts;
    opts.create_if_missing = false;
    status stat =
        db::open(opts, snapdir, column_families, &cf_handles, &snapdb);
    assert_ok(stat);

    readoptions roptions;
    std::string val;
    for (unsigned int i = 0; i < 80; i++) {
      stat = snapdb->get(roptions, cf_handles[i < 40], key(i), &val);
      assert_eq(values[i].compare(val), 0);
    }
    for (auto cfh : cf_handles) {
      delete cfh;
    }
    delete snapdb;

    // look at the new live files after we added an 'extra' key
    // and after we took the first snapshot.
    uint64_t new_manifest_number = 0;
    uint64_t new_manifest_size = 0;
    std::vector<std::string> newfiles;
    dbfull()->disablefiledeletions();
    dbfull()->getlivefiles(newfiles, &new_manifest_size);

    // find the new manifest file. assert that this manifest file is
    // the same one as in the previous snapshot. but its size should be
    // larger because we added an extra key after taking the
    // previous shapshot.
    for (unsigned int i = 0; i < newfiles.size(); i++) {
      std::string src = dbname_ + "/" + newfiles[i];
      // record the lognumber and the size of the
      // latest manifest file
      if (parsefilename(newfiles[i].substr(1), &number, &type)) {
        if (type == kdescriptorfile) {
          if (number > new_manifest_number) {
            uint64_t size;
            new_manifest_number = number;
            assert_ok(env_->getfilesize(src, &size));
            assert_ge(size, new_manifest_size);
          }
        }
      }
    }
    assert_eq(manifest_number, new_manifest_number);
    assert_gt(new_manifest_size, manifest_size);

    // release file snapshot
    dbfull()->disablefiledeletions();
  } while (changecompactoptions());
}

test(dbtest, compactonflush) {
  do {
    options options = currentoptions();
    options.purge_redundant_kvs_while_flush = true;
    options.disable_auto_compactions = true;
    createandreopenwithcf({"pikachu"}, &options);

    put(1, "foo", "v1");
    assert_ok(flush(1));
    assert_eq(allentriesfor("foo", 1), "[ v1 ]");

    // write two new keys
    put(1, "a", "begin");
    put(1, "z", "end");
    flush(1);

    // case1: delete followed by a put
    delete(1, "foo");
    put(1, "foo", "v2");
    assert_eq(allentriesfor("foo", 1), "[ v2, del, v1 ]");

    // after the current memtable is flushed, the del should
    // have been removed
    assert_ok(flush(1));
    assert_eq(allentriesfor("foo", 1), "[ v2, v1 ]");

    dbfull()->compactrange(handles_[1], nullptr, nullptr);
    assert_eq(allentriesfor("foo", 1), "[ v2 ]");

    // case 2: delete followed by another delete
    delete(1, "foo");
    delete(1, "foo");
    assert_eq(allentriesfor("foo", 1), "[ del, del, v2 ]");
    assert_ok(flush(1));
    assert_eq(allentriesfor("foo", 1), "[ del, v2 ]");
    dbfull()->compactrange(handles_[1], nullptr, nullptr);
    assert_eq(allentriesfor("foo", 1), "[ ]");

    // case 3: put followed by a delete
    put(1, "foo", "v3");
    delete(1, "foo");
    assert_eq(allentriesfor("foo", 1), "[ del, v3 ]");
    assert_ok(flush(1));
    assert_eq(allentriesfor("foo", 1), "[ del ]");
    dbfull()->compactrange(handles_[1], nullptr, nullptr);
    assert_eq(allentriesfor("foo", 1), "[ ]");

    // case 4: put followed by another put
    put(1, "foo", "v4");
    put(1, "foo", "v5");
    assert_eq(allentriesfor("foo", 1), "[ v5, v4 ]");
    assert_ok(flush(1));
    assert_eq(allentriesfor("foo", 1), "[ v5 ]");
    dbfull()->compactrange(handles_[1], nullptr, nullptr);
    assert_eq(allentriesfor("foo", 1), "[ v5 ]");

    // clear database
    delete(1, "foo");
    dbfull()->compactrange(handles_[1], nullptr, nullptr);
    assert_eq(allentriesfor("foo", 1), "[ ]");

    // case 5: put followed by snapshot followed by another put
    // both puts should remain.
    put(1, "foo", "v6");
    const snapshot* snapshot = db_->getsnapshot();
    put(1, "foo", "v7");
    assert_ok(flush(1));
    assert_eq(allentriesfor("foo", 1), "[ v7, v6 ]");
    db_->releasesnapshot(snapshot);

    // clear database
    delete(1, "foo");
    dbfull()->compactrange(handles_[1], nullptr, nullptr);
    assert_eq(allentriesfor("foo", 1), "[ ]");

    // case 5: snapshot followed by a put followed by another put
    // only the last put should remain.
    const snapshot* snapshot1 = db_->getsnapshot();
    put(1, "foo", "v8");
    put(1, "foo", "v9");
    assert_ok(flush(1));
    assert_eq(allentriesfor("foo", 1), "[ v9 ]");
    db_->releasesnapshot(snapshot1);
  } while (changecompactoptions());
}

namespace {
std::vector<std::uint64_t> listspecificfiles(
    env* env, const std::string& path, const filetype expected_file_type) {
  std::vector<std::string> files;
  std::vector<uint64_t> log_files;
  env->getchildren(path, &files);
  uint64_t number;
  filetype type;
  for (size_t i = 0; i < files.size(); ++i) {
    if (parsefilename(files[i], &number, &type)) {
      if (type == expected_file_type) {
        log_files.push_back(number);
      }
    }
  }
  return std::move(log_files);
}

std::vector<std::uint64_t> listlogfiles(env* env, const std::string& path) {
  return listspecificfiles(env, path, klogfile);
}

std::vector<std::uint64_t> listtablefiles(env* env, const std::string& path) {
  return listspecificfiles(env, path, ktablefile);
}
}  // namespace

test(dbtest, flushonecolumnfamily) {
  options options;
  createandreopenwithcf({"pikachu", "ilya", "muromec", "dobrynia", "nikitich",
                         "alyosha", "popovich"},
                        &options);

  assert_ok(put(0, "default", "default"));
  assert_ok(put(1, "pikachu", "pikachu"));
  assert_ok(put(2, "ilya", "ilya"));
  assert_ok(put(3, "muromec", "muromec"));
  assert_ok(put(4, "dobrynia", "dobrynia"));
  assert_ok(put(5, "nikitich", "nikitich"));
  assert_ok(put(6, "alyosha", "alyosha"));
  assert_ok(put(7, "popovich", "popovich"));

  for (size_t i = 0; i < 8; ++i) {
    flush(i);
    auto tables = listtablefiles(env_, dbname_);
    assert_eq(tables.size(), i + 1u);
  }
}

test(dbtest, walarchivalttl) {
  do {
    options options = currentoptions();
    options.create_if_missing = true;
    options.wal_ttl_seconds = 1000;
    destroyandreopen(&options);

    //  test : create db with a ttl and no size limit.
    //  put some keys. count the log files present in the db just after insert.
    //  re-open db. causes deletion/archival to take place.
    //  assert that the files moved under "/archive".
    //  reopen db with small ttl.
    //  assert that archive was removed.

    std::string archivedir = archivaldirectory(dbname_);

    for (int i = 0; i < 10; ++i) {
      for (int j = 0; j < 10; ++j) {
        assert_ok(put(key(10 * i + j), dummystring(1024)));
      }

      std::vector<uint64_t> log_files = listlogfiles(env_, dbname_);

      options.create_if_missing = false;
      reopen(&options);

      std::vector<uint64_t> logs = listlogfiles(env_, archivedir);
      std::set<uint64_t> archivedfiles(logs.begin(), logs.end());

      for (auto& log : log_files) {
        assert_true(archivedfiles.find(log) != archivedfiles.end());
      }
    }

    std::vector<uint64_t> log_files = listlogfiles(env_, archivedir);
    assert_true(log_files.size() > 0);

    options.wal_ttl_seconds = 1;
    env_->sleepformicroseconds(2 * 1000 * 1000);
    reopen(&options);

    log_files = listlogfiles(env_, archivedir);
    assert_true(log_files.empty());
  } while (changecompactoptions());
}

namespace {
uint64_t getlogdirsize(std::string dir_path, specialenv* env) {
  uint64_t dir_size = 0;
  std::vector<std::string> files;
  env->getchildren(dir_path, &files);
  for (auto& f : files) {
    uint64_t number;
    filetype type;
    if (parsefilename(f, &number, &type) && type == klogfile) {
      std::string const file_path = dir_path + "/" + f;
      uint64_t file_size;
      env->getfilesize(file_path, &file_size);
      dir_size += file_size;
    }
  }
  return dir_size;
}
}  // namespace

test(dbtest, walarchivalsizelimit) {
  do {
    options options = currentoptions();
    options.create_if_missing = true;
    options.wal_ttl_seconds = 0;
    options.wal_size_limit_mb = 1000;

    // test : create db with huge size limit and no ttl.
    // put some keys. count the archived log files present in the db
    // just after insert. assert that there are many enough.
    // change size limit. re-open db.
    // assert that archive is not greater than wal_size_limit_mb.
    // set ttl and time_to_check_ to small values. re-open db.
    // assert that there are no archived logs left.

    destroyandreopen(&options);
    for (int i = 0; i < 128 * 128; ++i) {
      assert_ok(put(key(i), dummystring(1024)));
    }
    reopen(&options);

    std::string archive_dir = archivaldirectory(dbname_);
    std::vector<std::uint64_t> log_files = listlogfiles(env_, archive_dir);
    assert_true(log_files.size() > 2);

    options.wal_size_limit_mb = 8;
    reopen(&options);
    dbfull()->test_purgeobsoletetewal();

    uint64_t archive_size = getlogdirsize(archive_dir, env_);
    assert_true(archive_size <= options.wal_size_limit_mb * 1024 * 1024);

    options.wal_ttl_seconds = 1;
    dbfull()->test_setdefaulttimetocheck(1);
    env_->sleepformicroseconds(2 * 1000 * 1000);
    reopen(&options);
    dbfull()->test_purgeobsoletetewal();

    log_files = listlogfiles(env_, archive_dir);
    assert_true(log_files.empty());
  } while (changecompactoptions());
}

test(dbtest, purgeinfologs) {
  options options = currentoptions();
  options.keep_log_file_num = 5;
  options.create_if_missing = true;
  for (int mode = 0; mode <= 1; mode++) {
    if (mode == 1) {
      options.db_log_dir = dbname_ + "_logs";
      env_->createdirifmissing(options.db_log_dir);
    } else {
      options.db_log_dir = "";
    }
    for (int i = 0; i < 8; i++) {
      reopen(&options);
    }

    std::vector<std::string> files;
    env_->getchildren(options.db_log_dir.empty() ? dbname_ : options.db_log_dir,
                      &files);
    int info_log_count = 0;
    for (std::string file : files) {
      if (file.find("log") != std::string::npos) {
        info_log_count++;
      }
    }
    assert_eq(5, info_log_count);

    destroy(&options);
    // for mode (1), test destorydb() to delete all the logs under db dir.
    // for mode (2), no info log file should have been put under db dir.
    std::vector<std::string> db_files;
    env_->getchildren(dbname_, &db_files);
    for (std::string file : db_files) {
      assert_true(file.find("log") == std::string::npos);
    }

    if (mode == 1) {
      // cleaning up
      env_->getchildren(options.db_log_dir, &files);
      for (std::string file : files) {
        env_->deletefile(options.db_log_dir + "/" + file);
      }
      env_->deletedir(options.db_log_dir);
    }
  }
}

namespace {
sequencenumber readrecords(
    std::unique_ptr<transactionlogiterator>& iter,
    int& count) {
  count = 0;
  sequencenumber lastsequence = 0;
  batchresult res;
  while (iter->valid()) {
    res = iter->getbatch();
    assert_true(res.sequence > lastsequence);
    ++count;
    lastsequence = res.sequence;
    assert_ok(iter->status());
    iter->next();
  }
  return res.sequence;
}

void expectrecords(
    const int expected_no_records,
    std::unique_ptr<transactionlogiterator>& iter) {
  int num_records;
  readrecords(iter, num_records);
  assert_eq(num_records, expected_no_records);
}
}  // namespace

test(dbtest, transactionlogiterator) {
  do {
    options options = optionsforlogitertest();
    destroyandreopen(&options);
    createandreopenwithcf({"pikachu"}, &options);
    put(0, "key1", dummystring(1024));
    put(1, "key2", dummystring(1024));
    put(1, "key2", dummystring(1024));
    assert_eq(dbfull()->getlatestsequencenumber(), 3u);
    {
      auto iter = opentransactionlogiter(0);
      expectrecords(3, iter);
    }
    reopenwithcolumnfamilies({"default", "pikachu"}, &options);
    env_->sleepformicroseconds(2 * 1000 * 1000);
    {
      put(0, "key4", dummystring(1024));
      put(1, "key5", dummystring(1024));
      put(0, "key6", dummystring(1024));
    }
    {
      auto iter = opentransactionlogiter(0);
      expectrecords(6, iter);
    }
  } while (changecompactoptions());
}

#ifndef ndebug // sync point is not included with dndebug build
test(dbtest, transactionlogiteratorrace) {
  static const int log_iterator_race_test_count = 2;
  static const char* sync_points[log_iterator_race_test_count][4] =
    { { "dbimpl::getsortedwalfiles:1", "dbimpl::purgeobsoletefiles:1",
        "dbimpl::purgeobsoletefiles:2", "dbimpl::getsortedwalfiles:2" },
      { "dbimpl::getsortedwalsoftype:1", "dbimpl::purgeobsoletefiles:1",
        "dbimpl::purgeobsoletefiles:2", "dbimpl::getsortedwalsoftype:2" }};
  for (int test = 0; test < log_iterator_race_test_count; ++test) {
    // setup sync point dependency to reproduce the race condition of
    // a log file moved to archived dir, in the middle of getsortedwalfiles
    rocksdb::syncpoint::getinstance()->loaddependency(
      { { sync_points[test][0], sync_points[test][1] },
        { sync_points[test][2], sync_points[test][3] },
      });

    do {
      rocksdb::syncpoint::getinstance()->cleartrace();
      rocksdb::syncpoint::getinstance()->disableprocessing();
      options options = optionsforlogitertest();
      destroyandreopen(&options);
      put("key1", dummystring(1024));
      dbfull()->flush(flushoptions());
      put("key2", dummystring(1024));
      dbfull()->flush(flushoptions());
      put("key3", dummystring(1024));
      dbfull()->flush(flushoptions());
      put("key4", dummystring(1024));
      assert_eq(dbfull()->getlatestsequencenumber(), 4u);

      {
        auto iter = opentransactionlogiter(0);
        expectrecords(4, iter);
      }

      rocksdb::syncpoint::getinstance()->enableprocessing();
      // trigger async flush, and log move. well, log move will
      // wait until the getsortedwalfiles:1 to reproduce the race
      // condition
      flushoptions flush_options;
      flush_options.wait = false;
      dbfull()->flush(flush_options);

      // "key5" would be written in a new memtable and log
      put("key5", dummystring(1024));
      {
        // this iter would miss "key4" if not fixed
        auto iter = opentransactionlogiter(0);
        expectrecords(5, iter);
      }
    } while (changecompactoptions());
  }
}
#endif

test(dbtest, transactionlogiteratormoveoverzerofiles) {
  do {
    options options = optionsforlogitertest();
    destroyandreopen(&options);
    createandreopenwithcf({"pikachu"}, &options);
    // do a plain reopen.
    put(1, "key1", dummystring(1024));
    // two reopens should create a zero record wal file.
    reopenwithcolumnfamilies({"default", "pikachu"}, &options);
    reopenwithcolumnfamilies({"default", "pikachu"}, &options);

    put(1, "key2", dummystring(1024));

    auto iter = opentransactionlogiter(0);
    expectrecords(2, iter);
  } while (changecompactoptions());
}

test(dbtest, transactionlogiteratorstallatlastrecord) {
  do {
    options options = optionsforlogitertest();
    destroyandreopen(&options);
    put("key1", dummystring(1024));
    auto iter = opentransactionlogiter(0);
    assert_ok(iter->status());
    assert_true(iter->valid());
    iter->next();
    assert_true(!iter->valid());
    assert_ok(iter->status());
    put("key2", dummystring(1024));
    iter->next();
    assert_ok(iter->status());
    assert_true(iter->valid());
  } while (changecompactoptions());
}

test(dbtest, transactionlogiteratorjustemptyfile) {
  do {
    options options = optionsforlogitertest();
    destroyandreopen(&options);
    unique_ptr<transactionlogiterator> iter;
    status status = dbfull()->getupdatessince(0, &iter);
    // check that an empty iterator is returned
    assert_true(!iter->valid());
  } while (changecompactoptions());
}

test(dbtest, transactionlogiteratorcheckafterrestart) {
  do {
    options options = optionsforlogitertest();
    destroyandreopen(&options);
    put("key1", dummystring(1024));
    put("key2", dummystring(1023));
    dbfull()->flush(flushoptions());
    reopen(&options);
    auto iter = opentransactionlogiter(0);
    expectrecords(2, iter);
  } while (changecompactoptions());
}

test(dbtest, transactionlogiteratorcorruptedlog) {
  do {
    options options = optionsforlogitertest();
    destroyandreopen(&options);
    for (int i = 0; i < 1024; i++) {
      put("key"+std::to_string(i), dummystring(10));
    }
    dbfull()->flush(flushoptions());
    // corrupt this log to create a gap
    rocksdb::vectorlogptr wal_files;
    assert_ok(dbfull()->getsortedwalfiles(wal_files));
    const auto logfilepath = dbname_ + "/" + wal_files.front()->pathname();
    assert_eq(
      0,
      truncate(logfilepath.c_str(), wal_files.front()->sizefilebytes() / 2));
    // insert a new entry to a new log file
    put("key1025", dummystring(10));
    // try to read from the beginning. should stop before the gap and read less
    // than 1025 entries
    auto iter = opentransactionlogiter(0);
    int count;
    int last_sequence_read = readrecords(iter, count);
    assert_lt(last_sequence_read, 1025);
    // try to read past the gap, should be able to seek to key1025
    auto iter2 = opentransactionlogiter(last_sequence_read + 1);
    expectrecords(1, iter2);
  } while (changecompactoptions());
}

test(dbtest, transactionlogiteratorbatchoperations) {
  do {
    options options = optionsforlogitertest();
    destroyandreopen(&options);
    createandreopenwithcf({"pikachu"}, &options);
    writebatch batch;
    batch.put(handles_[1], "key1", dummystring(1024));
    batch.put(handles_[0], "key2", dummystring(1024));
    batch.put(handles_[1], "key3", dummystring(1024));
    batch.delete(handles_[0], "key2");
    dbfull()->write(writeoptions(), &batch);
    flush(1);
    flush(0);
    reopenwithcolumnfamilies({"default", "pikachu"}, &options);
    put(1, "key4", dummystring(1024));
    auto iter = opentransactionlogiter(3);
    expectrecords(2, iter);
  } while (changecompactoptions());
}

test(dbtest, transactionlogiteratorblobs) {
  options options = optionsforlogitertest();
  destroyandreopen(&options);
  createandreopenwithcf({"pikachu"}, &options);
  {
    writebatch batch;
    batch.put(handles_[1], "key1", dummystring(1024));
    batch.put(handles_[0], "key2", dummystring(1024));
    batch.putlogdata(slice("blob1"));
    batch.put(handles_[1], "key3", dummystring(1024));
    batch.putlogdata(slice("blob2"));
    batch.delete(handles_[0], "key2");
    dbfull()->write(writeoptions(), &batch);
    reopenwithcolumnfamilies({"default", "pikachu"}, &options);
  }

  auto res = opentransactionlogiter(0)->getbatch();
  struct handler : public writebatch::handler {
    std::string seen;
    virtual status putcf(uint32_t cf, const slice& key, const slice& value) {
      seen += "put(" + std::to_string(cf) + ", " + key.tostring() + ", " +
              std::to_string(value.size()) + ")";
      return status::ok();
    }
    virtual status mergecf(uint32_t cf, const slice& key, const slice& value) {
      seen += "merge(" + std::to_string(cf) + ", " + key.tostring() + ", " +
              std::to_string(value.size()) + ")";
      return status::ok();
    }
    virtual void logdata(const slice& blob) {
      seen += "logdata(" + blob.tostring() + ")";
    }
    virtual status deletecf(uint32_t cf, const slice& key) {
      seen += "delete(" + std::to_string(cf) + ", " + key.tostring() + ")";
      return status::ok();
    }
  } handler;
  res.writebatchptr->iterate(&handler);
  assert_eq(
      "put(1, key1, 1024)"
      "put(0, key2, 1024)"
      "logdata(blob1)"
      "put(1, key3, 1024)"
      "logdata(blob2)"
      "delete(0, key2)",
      handler.seen);
}

test(dbtest, readfirstrecordcache) {
  options options = currentoptions();
  options.env = env_;
  options.create_if_missing = true;
  destroyandreopen(&options);

  std::string path = dbname_ + "/000001.log";
  unique_ptr<writablefile> file;
  assert_ok(env_->newwritablefile(path, &file, envoptions()));

  sequencenumber s;
  assert_ok(dbfull()->test_readfirstline(path, &s));
  assert_eq(s, 0u);

  assert_ok(dbfull()->test_readfirstrecord(kalivelogfile, 1, &s));
  assert_eq(s, 0u);

  log::writer writer(std::move(file));
  writebatch batch;
  batch.put("foo", "bar");
  writebatchinternal::setsequence(&batch, 10);
  writer.addrecord(writebatchinternal::contents(&batch));

  env_->count_sequential_reads_ = true;
  // sequential_read_counter_ sanity test
  assert_eq(env_->sequential_read_counter_.read(), 0);

  assert_ok(dbfull()->test_readfirstrecord(kalivelogfile, 1, &s));
  assert_eq(s, 10u);
  // did a read
  assert_eq(env_->sequential_read_counter_.read(), 1);

  assert_ok(dbfull()->test_readfirstrecord(kalivelogfile, 1, &s));
  assert_eq(s, 10u);
  // no new reads since the value is cached
  assert_eq(env_->sequential_read_counter_.read(), 1);
}

// multi-threaded test:
namespace {

static const int kcolumnfamilies = 10;
static const int knumthreads = 10;
static const int ktestseconds = 10;
static const int knumkeys = 1000;

struct mtstate {
  dbtest* test;
  port::atomicpointer stop;
  port::atomicpointer counter[knumthreads];
  port::atomicpointer thread_done[knumthreads];
};

struct mtthread {
  mtstate* state;
  int id;
};

static void mtthreadbody(void* arg) {
  mtthread* t = reinterpret_cast<mtthread*>(arg);
  int id = t->id;
  db* db = t->state->test->db_;
  uintptr_t counter = 0;
  fprintf(stderr, "... starting thread %d\n", id);
  random rnd(1000 + id);
  char valbuf[1500];
  while (t->state->stop.acquire_load() == nullptr) {
    t->state->counter[id].release_store(reinterpret_cast<void*>(counter));

    int key = rnd.uniform(knumkeys);
    char keybuf[20];
    snprintf(keybuf, sizeof(keybuf), "%016d", key);

    if (rnd.onein(2)) {
      // write values of the form <key, my id, counter, cf, unique_id>.
      // into each of the cfs
      // we add some padding for force compactions.
      int unique_id = rnd.uniform(1000000);

      // half of the time directly use writebatch. half of the time use
      // writebatchwithindex.
      if (rnd.onein(2)) {
        writebatch batch;
        for (int cf = 0; cf < kcolumnfamilies; ++cf) {
          snprintf(valbuf, sizeof(valbuf), "%d.%d.%d.%d.%-1000d", key, id,
                   static_cast<int>(counter), cf, unique_id);
          batch.put(t->state->test->handles_[cf], slice(keybuf), slice(valbuf));
        }
        assert_ok(db->write(writeoptions(), &batch));
      } else {
        writebatchwithindex batch(db->getoptions().comparator);
        for (int cf = 0; cf < kcolumnfamilies; ++cf) {
          snprintf(valbuf, sizeof(valbuf), "%d.%d.%d.%d.%-1000d", key, id,
                   static_cast<int>(counter), cf, unique_id);
          batch.put(t->state->test->handles_[cf], slice(keybuf), slice(valbuf));
        }
        assert_ok(db->write(writeoptions(), batch.getwritebatch()));
      }
    } else {
      // read a value and verify that it matches the pattern written above
      // and that writes to all column families were atomic (unique_id is the
      // same)
      std::vector<slice> keys(kcolumnfamilies, slice(keybuf));
      std::vector<std::string> values;
      std::vector<status> statuses =
          db->multiget(readoptions(), t->state->test->handles_, keys, &values);
      status s = statuses[0];
      // all statuses have to be the same
      for (size_t i = 1; i < statuses.size(); ++i) {
        // they are either both ok or both not-found
        assert_true((s.ok() && statuses[i].ok()) ||
                    (s.isnotfound() && statuses[i].isnotfound()));
      }
      if (s.isnotfound()) {
        // key has not yet been written
      } else {
        // check that the writer thread counter is >= the counter in the value
        assert_ok(s);
        int unique_id = -1;
        for (int i = 0; i < kcolumnfamilies; ++i) {
          int k, w, c, cf, u;
          assert_eq(5, sscanf(values[i].c_str(), "%d.%d.%d.%d.%d", &k, &w,
                              &c, &cf, &u))
              << values[i];
          assert_eq(k, key);
          assert_ge(w, 0);
          assert_lt(w, knumthreads);
          assert_le((unsigned int)c, reinterpret_cast<uintptr_t>(
                                         t->state->counter[w].acquire_load()));
          assert_eq(cf, i);
          if (i == 0) {
            unique_id = u;
          } else {
            // this checks that updates across column families happened
            // atomically -- all unique ids are the same
            assert_eq(u, unique_id);
          }
        }
      }
    }
    counter++;
  }
  t->state->thread_done[id].release_store(t);
  fprintf(stderr, "... stopping thread %d after %d ops\n", id, int(counter));
}

}  // namespace

test(dbtest, multithreaded) {
  do {
    std::vector<std::string> cfs;
    for (int i = 1; i < kcolumnfamilies; ++i) {
      cfs.push_back(std::to_string(i));
    }
    createandreopenwithcf(cfs);
    // initialize state
    mtstate mt;
    mt.test = this;
    mt.stop.release_store(0);
    for (int id = 0; id < knumthreads; id++) {
      mt.counter[id].release_store(0);
      mt.thread_done[id].release_store(0);
    }

    // start threads
    mtthread thread[knumthreads];
    for (int id = 0; id < knumthreads; id++) {
      thread[id].state = &mt;
      thread[id].id = id;
      env_->startthread(mtthreadbody, &thread[id]);
    }

    // let them run for a while
    env_->sleepformicroseconds(ktestseconds * 1000000);

    // stop the threads and wait for them to finish
    mt.stop.release_store(&mt);
    for (int id = 0; id < knumthreads; id++) {
      while (mt.thread_done[id].acquire_load() == nullptr) {
        env_->sleepformicroseconds(100000);
      }
    }
    // skip as hashcuckoorep does not support snapshot
  } while (changeoptions(kskiphashcuckoo));
}

// group commit test:
namespace {

static const int kgcnumthreads = 4;
static const int kgcnumkeys = 1000;

struct gcthread {
  db* db;
  int id;
  std::atomic<bool> done;
};

static void gcthreadbody(void* arg) {
  gcthread* t = reinterpret_cast<gcthread*>(arg);
  int id = t->id;
  db* db = t->db;
  writeoptions wo;

  for (int i = 0; i < kgcnumkeys; ++i) {
    std::string kv(std::to_string(i + id * kgcnumkeys));
    assert_ok(db->put(wo, kv, kv));
  }
  t->done = true;
}

}  // namespace

test(dbtest, groupcommittest) {
  do {
    options options = currentoptions();
    options.statistics = rocksdb::createdbstatistics();
    reopen(&options);

    // start threads
    gcthread thread[kgcnumthreads];
    for (int id = 0; id < kgcnumthreads; id++) {
      thread[id].id = id;
      thread[id].db = db_;
      thread[id].done = false;
      env_->startthread(gcthreadbody, &thread[id]);
    }

    for (int id = 0; id < kgcnumthreads; id++) {
      while (thread[id].done == false) {
        env_->sleepformicroseconds(100000);
      }
    }
    assert_gt(testgettickercount(options, write_done_by_other), 0);

    std::vector<std::string> expected_db;
    for (int i = 0; i < kgcnumthreads * kgcnumkeys; ++i) {
      expected_db.push_back(std::to_string(i));
    }
    sort(expected_db.begin(), expected_db.end());

    iterator* itr = db_->newiterator(readoptions());
    itr->seektofirst();
    for (auto x : expected_db) {
      assert_true(itr->valid());
      assert_eq(itr->key().tostring(), x);
      assert_eq(itr->value().tostring(), x);
      itr->next();
    }
    assert_true(!itr->valid());
    delete itr;

  } while (changeoptions(kskipnoseektolast));
}

namespace {
typedef std::map<std::string, std::string> kvmap;
}

class modeldb: public db {
 public:
  class modelsnapshot : public snapshot {
   public:
    kvmap map_;
  };

  explicit modeldb(const options& options) : options_(options) {}
  using db::put;
  virtual status put(const writeoptions& o, columnfamilyhandle* cf,
                     const slice& k, const slice& v) {
    writebatch batch;
    batch.put(cf, k, v);
    return write(o, &batch);
  }
  using db::merge;
  virtual status merge(const writeoptions& o, columnfamilyhandle* cf,
                       const slice& k, const slice& v) {
    writebatch batch;
    batch.merge(cf, k, v);
    return write(o, &batch);
  }
  using db::delete;
  virtual status delete(const writeoptions& o, columnfamilyhandle* cf,
                        const slice& key) {
    writebatch batch;
    batch.delete(cf, key);
    return write(o, &batch);
  }
  using db::get;
  virtual status get(const readoptions& options, columnfamilyhandle* cf,
                     const slice& key, std::string* value) {
    return status::notsupported(key);
  }

  using db::multiget;
  virtual std::vector<status> multiget(
      const readoptions& options,
      const std::vector<columnfamilyhandle*>& column_family,
      const std::vector<slice>& keys, std::vector<std::string>* values) {
    std::vector<status> s(keys.size(),
                          status::notsupported("not implemented."));
    return s;
  }

  using db::getpropertiesofalltables;
  virtual status getpropertiesofalltables(columnfamilyhandle* column_family,
                                          tablepropertiescollection* props) {
    return status();
  }

  using db::keymayexist;
  virtual bool keymayexist(const readoptions& options,
                           columnfamilyhandle* column_family, const slice& key,
                           std::string* value, bool* value_found = nullptr) {
    if (value_found != nullptr) {
      *value_found = false;
    }
    return true; // not supported directly
  }
  using db::newiterator;
  virtual iterator* newiterator(const readoptions& options,
                                columnfamilyhandle* column_family) {
    if (options.snapshot == nullptr) {
      kvmap* saved = new kvmap;
      *saved = map_;
      return new modeliter(saved, true);
    } else {
      const kvmap* snapshot_state =
          &(reinterpret_cast<const modelsnapshot*>(options.snapshot)->map_);
      return new modeliter(snapshot_state, false);
    }
  }
  virtual status newiterators(
      const readoptions& options,
      const std::vector<columnfamilyhandle*>& column_family,
      std::vector<iterator*>* iterators) {
    return status::notsupported("not supported yet");
  }
  virtual const snapshot* getsnapshot() {
    modelsnapshot* snapshot = new modelsnapshot;
    snapshot->map_ = map_;
    return snapshot;
  }

  virtual void releasesnapshot(const snapshot* snapshot) {
    delete reinterpret_cast<const modelsnapshot*>(snapshot);
  }

  virtual status write(const writeoptions& options, writebatch* batch) {
    class handler : public writebatch::handler {
     public:
      kvmap* map_;
      virtual void put(const slice& key, const slice& value) {
        (*map_)[key.tostring()] = value.tostring();
      }
      virtual void merge(const slice& key, const slice& value) {
        // ignore merge for now
        //(*map_)[key.tostring()] = value.tostring();
      }
      virtual void delete(const slice& key) {
        map_->erase(key.tostring());
      }
    };
    handler handler;
    handler.map_ = &map_;
    return batch->iterate(&handler);
  }

  using db::getproperty;
  virtual bool getproperty(columnfamilyhandle* column_family,
                           const slice& property, std::string* value) {
    return false;
  }
  using db::getintproperty;
  virtual bool getintproperty(columnfamilyhandle* column_family,
                              const slice& property, uint64_t* value) override {
    return false;
  }
  using db::getapproximatesizes;
  virtual void getapproximatesizes(columnfamilyhandle* column_family,
                                   const range* range, int n, uint64_t* sizes) {
    for (int i = 0; i < n; i++) {
      sizes[i] = 0;
    }
  }
  using db::compactrange;
  virtual status compactrange(columnfamilyhandle* column_family,
                              const slice* start, const slice* end,
                              bool reduce_level, int target_level,
                              uint32_t output_path_id) {
    return status::notsupported("not supported operation.");
  }

  using db::numberlevels;
  virtual int numberlevels(columnfamilyhandle* column_family) { return 1; }

  using db::maxmemcompactionlevel;
  virtual int maxmemcompactionlevel(columnfamilyhandle* column_family) {
    return 1;
  }

  using db::level0stopwritetrigger;
  virtual int level0stopwritetrigger(columnfamilyhandle* column_family) {
    return -1;
  }

  virtual const std::string& getname() const {
    return name_;
  }

  virtual env* getenv() const {
    return nullptr;
  }

  using db::getoptions;
  virtual const options& getoptions(columnfamilyhandle* column_family) const {
    return options_;
  }

  using db::flush;
  virtual status flush(const rocksdb::flushoptions& options,
                       columnfamilyhandle* column_family) {
    status ret;
    return ret;
  }

  virtual status disablefiledeletions() {
    return status::ok();
  }
  virtual status enablefiledeletions(bool force) {
    return status::ok();
  }
  virtual status getlivefiles(std::vector<std::string>&, uint64_t* size,
                              bool flush_memtable = true) {
    return status::ok();
  }

  virtual status getsortedwalfiles(vectorlogptr& files) {
    return status::ok();
  }

  virtual status deletefile(std::string name) {
    return status::ok();
  }

  virtual status getdbidentity(std::string& identity) {
    return status::ok();
  }

  virtual sequencenumber getlatestsequencenumber() const {
    return 0;
  }
  virtual status getupdatessince(
      rocksdb::sequencenumber, unique_ptr<rocksdb::transactionlogiterator>*,
      const transactionlogiterator::readoptions&
          read_options = transactionlogiterator::readoptions()) {
    return status::notsupported("not supported in model db");
  }

  virtual columnfamilyhandle* defaultcolumnfamily() const { return nullptr; }

 private:
  class modeliter: public iterator {
   public:
    modeliter(const kvmap* map, bool owned)
        : map_(map), owned_(owned), iter_(map_->end()) {
    }
    ~modeliter() {
      if (owned_) delete map_;
    }
    virtual bool valid() const { return iter_ != map_->end(); }
    virtual void seektofirst() { iter_ = map_->begin(); }
    virtual void seektolast() {
      if (map_->empty()) {
        iter_ = map_->end();
      } else {
        iter_ = map_->find(map_->rbegin()->first);
      }
    }
    virtual void seek(const slice& k) {
      iter_ = map_->lower_bound(k.tostring());
    }
    virtual void next() { ++iter_; }
    virtual void prev() {
      if (iter_ == map_->begin()) {
        iter_ = map_->end();
        return;
      }
      --iter_;
    }

    virtual slice key() const { return iter_->first; }
    virtual slice value() const { return iter_->second; }
    virtual status status() const { return status::ok(); }
   private:
    const kvmap* const map_;
    const bool owned_;  // do we own map_
    kvmap::const_iterator iter_;
  };
  const options options_;
  kvmap map_;
  std::string name_ = "";
};

static std::string randomkey(random* rnd, int minimum = 0) {
  int len;
  do {
    len = (rnd->onein(3)
           ? 1                // short sometimes to encourage collisions
           : (rnd->onein(100) ? rnd->skewed(10) : rnd->uniform(10)));
  } while (len < minimum);
  return test::randomkey(rnd, len);
}

static bool compareiterators(int step,
                             db* model,
                             db* db,
                             const snapshot* model_snap,
                             const snapshot* db_snap) {
  readoptions options;
  options.snapshot = model_snap;
  iterator* miter = model->newiterator(options);
  options.snapshot = db_snap;
  iterator* dbiter = db->newiterator(options);
  bool ok = true;
  int count = 0;
  for (miter->seektofirst(), dbiter->seektofirst();
       ok && miter->valid() && dbiter->valid();
       miter->next(), dbiter->next()) {
    count++;
    if (miter->key().compare(dbiter->key()) != 0) {
      fprintf(stderr, "step %d: key mismatch: '%s' vs. '%s'\n",
              step,
              escapestring(miter->key()).c_str(),
              escapestring(dbiter->key()).c_str());
      ok = false;
      break;
    }

    if (miter->value().compare(dbiter->value()) != 0) {
      fprintf(stderr, "step %d: value mismatch for key '%s': '%s' vs. '%s'\n",
              step,
              escapestring(miter->key()).c_str(),
              escapestring(miter->value()).c_str(),
              escapestring(miter->value()).c_str());
      ok = false;
    }
  }

  if (ok) {
    if (miter->valid() != dbiter->valid()) {
      fprintf(stderr, "step %d: mismatch at end of iterators: %d vs. %d\n",
              step, miter->valid(), dbiter->valid());
      ok = false;
    }
  }
  delete miter;
  delete dbiter;
  return ok;
}

test(dbtest, randomized) {
  random rnd(test::randomseed());
  do {
    modeldb model(currentoptions());
    const int n = 10000;
    const snapshot* model_snap = nullptr;
    const snapshot* db_snap = nullptr;
    std::string k, v;
    for (int step = 0; step < n; step++) {
      // todo(sanjay): test get() works
      int p = rnd.uniform(100);
      int minimum = 0;
      if (option_config_ == khashskiplist ||
          option_config_ == khashlinklist ||
          option_config_ == khashcuckoo ||
          option_config_ == kplaintablefirstbyteprefix ||
          option_config_ == kblockbasedtablewithwholekeyhashindex ||
          option_config_ == kblockbasedtablewithprefixhashindex) {
        minimum = 1;
      }
      if (p < 45) {                               // put
        k = randomkey(&rnd, minimum);
        v = randomstring(&rnd,
                         rnd.onein(20)
                         ? 100 + rnd.uniform(100)
                         : rnd.uniform(8));
        assert_ok(model.put(writeoptions(), k, v));
        assert_ok(db_->put(writeoptions(), k, v));

      } else if (p < 90) {                        // delete
        k = randomkey(&rnd, minimum);
        assert_ok(model.delete(writeoptions(), k));
        assert_ok(db_->delete(writeoptions(), k));


      } else {                                    // multi-element batch
        writebatch b;
        const int num = rnd.uniform(8);
        for (int i = 0; i < num; i++) {
          if (i == 0 || !rnd.onein(10)) {
            k = randomkey(&rnd, minimum);
          } else {
            // periodically re-use the same key from the previous iter, so
            // we have multiple entries in the write batch for the same key
          }
          if (rnd.onein(2)) {
            v = randomstring(&rnd, rnd.uniform(10));
            b.put(k, v);
          } else {
            b.delete(k);
          }
        }
        assert_ok(model.write(writeoptions(), &b));
        assert_ok(db_->write(writeoptions(), &b));
      }

      if ((step % 100) == 0) {
        // for db instances that use the hash index + block-based table, the
        // iterator will be invalid right when seeking a non-existent key, right
        // than return a key that is close to it.
        if (option_config_ != kblockbasedtablewithwholekeyhashindex &&
            option_config_ != kblockbasedtablewithprefixhashindex) {
          assert_true(compareiterators(step, &model, db_, nullptr, nullptr));
          assert_true(compareiterators(step, &model, db_, model_snap, db_snap));
        }

        // save a snapshot from each db this time that we'll use next
        // time we compare things, to make sure the current state is
        // preserved with the snapshot
        if (model_snap != nullptr) model.releasesnapshot(model_snap);
        if (db_snap != nullptr) db_->releasesnapshot(db_snap);

        reopen();
        assert_true(compareiterators(step, &model, db_, nullptr, nullptr));

        model_snap = model.getsnapshot();
        db_snap = db_->getsnapshot();
      }

      if ((step % 2000) == 0) {
        fprintf(stdout,
                "dbtest.randomized, option id: %d, step: %d out of %d\n",
                option_config_, step, n);
      }
    }
    if (model_snap != nullptr) model.releasesnapshot(model_snap);
    if (db_snap != nullptr) db_->releasesnapshot(db_snap);
    // skip cuckoo hash as it does not support snapshot.
  } while (changeoptions(kskipdeletesfilterfirst | kskipnoseektolast |
                         kskiphashcuckoo));
}

test(dbtest, multigetsimple) {
  do {
    createandreopenwithcf({"pikachu"});
    assert_ok(put(1, "k1", "v1"));
    assert_ok(put(1, "k2", "v2"));
    assert_ok(put(1, "k3", "v3"));
    assert_ok(put(1, "k4", "v4"));
    assert_ok(delete(1, "k4"));
    assert_ok(put(1, "k5", "v5"));
    assert_ok(delete(1, "no_key"));

    std::vector<slice> keys({"k1", "k2", "k3", "k4", "k5", "no_key"});

    std::vector<std::string> values(20, "temporary data to be overwritten");
    std::vector<columnfamilyhandle*> cfs(keys.size(), handles_[1]);

    std::vector<status> s = db_->multiget(readoptions(), cfs, keys, &values);
    assert_eq(values.size(), keys.size());
    assert_eq(values[0], "v1");
    assert_eq(values[1], "v2");
    assert_eq(values[2], "v3");
    assert_eq(values[4], "v5");

    assert_ok(s[0]);
    assert_ok(s[1]);
    assert_ok(s[2]);
    assert_true(s[3].isnotfound());
    assert_ok(s[4]);
    assert_true(s[5].isnotfound());
  } while (changecompactoptions());
}

test(dbtest, multigetempty) {
  do {
    createandreopenwithcf({"pikachu"});
    // empty key set
    std::vector<slice> keys;
    std::vector<std::string> values;
    std::vector<columnfamilyhandle*> cfs;
    std::vector<status> s = db_->multiget(readoptions(), cfs, keys, &values);
    assert_eq(s.size(), 0u);

    // empty database, empty key set
    destroyandreopen();
    createandreopenwithcf({"pikachu"});
    s = db_->multiget(readoptions(), cfs, keys, &values);
    assert_eq(s.size(), 0u);

    // empty database, search for keys
    keys.resize(2);
    keys[0] = "a";
    keys[1] = "b";
    cfs.push_back(handles_[0]);
    cfs.push_back(handles_[1]);
    s = db_->multiget(readoptions(), cfs, keys, &values);
    assert_eq((int)s.size(), 2);
    assert_true(s[0].isnotfound() && s[1].isnotfound());
  } while (changecompactoptions());
}

namespace {
void prefixscaninit(dbtest *dbtest) {
  char buf[100];
  std::string keystr;
  const int small_range_sstfiles = 5;
  const int big_range_sstfiles = 5;

  // generate 11 sst files with the following prefix ranges.
  // group 0: [0,10]                              (level 1)
  // group 1: [1,2], [2,3], [3,4], [4,5], [5, 6]  (level 0)
  // group 2: [0,6], [0,7], [0,8], [0,9], [0,10]  (level 0)
  //
  // a seek with the previous api would do 11 random i/os (to all the
  // files).  with the new api and a prefix filter enabled, we should
  // only do 2 random i/o, to the 2 files containing the key.

  // group 0
  snprintf(buf, sizeof(buf), "%02d______:start", 0);
  keystr = std::string(buf);
  assert_ok(dbtest->put(keystr, keystr));
  snprintf(buf, sizeof(buf), "%02d______:end", 10);
  keystr = std::string(buf);
  assert_ok(dbtest->put(keystr, keystr));
  dbtest->flush();
  dbtest->dbfull()->compactrange(nullptr, nullptr); // move to level 1

  // group 1
  for (int i = 1; i <= small_range_sstfiles; i++) {
    snprintf(buf, sizeof(buf), "%02d______:start", i);
    keystr = std::string(buf);
    assert_ok(dbtest->put(keystr, keystr));
    snprintf(buf, sizeof(buf), "%02d______:end", i+1);
    keystr = std::string(buf);
    assert_ok(dbtest->put(keystr, keystr));
    dbtest->flush();
  }

  // group 2
  for (int i = 1; i <= big_range_sstfiles; i++) {
    std::string keystr;
    snprintf(buf, sizeof(buf), "%02d______:start", 0);
    keystr = std::string(buf);
    assert_ok(dbtest->put(keystr, keystr));
    snprintf(buf, sizeof(buf), "%02d______:end",
             small_range_sstfiles+i+1);
    keystr = std::string(buf);
    assert_ok(dbtest->put(keystr, keystr));
    dbtest->flush();
  }
}
}  // namespace

test(dbtest, prefixscan) {
  int count;
  slice prefix;
  slice key;
  char buf[100];
  iterator* iter;
  snprintf(buf, sizeof(buf), "03______:");
  prefix = slice(buf, 8);
  key = slice(buf, 9);
  // db configs
  env_->count_random_reads_ = true;
  options options = currentoptions();
  options.env = env_;
  options.prefix_extractor.reset(newfixedprefixtransform(8));
  options.disable_auto_compactions = true;
  options.max_background_compactions = 2;
  options.create_if_missing = true;
  options.memtable_factory.reset(newhashskiplistrepfactory(16));

  blockbasedtableoptions table_options;
  table_options.no_block_cache = true;
  table_options.filter_policy.reset(newbloomfilterpolicy(10));
  table_options.whole_key_filtering = false;
  options.table_factory.reset(newblockbasedtablefactory(table_options));

  // 11 rand i/os
  destroyandreopen(&options);
  prefixscaninit(this);
  count = 0;
  env_->random_read_counter_.reset();
  iter = db_->newiterator(readoptions());
  for (iter->seek(prefix); iter->valid(); iter->next()) {
    if (! iter->key().starts_with(prefix)) {
      break;
    }
    count++;
  }
  assert_ok(iter->status());
  delete iter;
  assert_eq(count, 2);
  assert_eq(env_->random_read_counter_.read(), 2);
  close();
}

test(dbtest, tailingiteratorsingle) {
  readoptions read_options;
  read_options.tailing = true;

  std::unique_ptr<iterator> iter(db_->newiterator(read_options));
  iter->seektofirst();
  assert_true(!iter->valid());

  // add a record and check that iter can see it
  assert_ok(db_->put(writeoptions(), "mirko", "fodor"));
  iter->seektofirst();
  assert_true(iter->valid());
  assert_eq(iter->key().tostring(), "mirko");

  iter->next();
  assert_true(!iter->valid());
}

test(dbtest, tailingiteratorkeepadding) {
  createandreopenwithcf({"pikachu"});
  readoptions read_options;
  read_options.tailing = true;

  std::unique_ptr<iterator> iter(db_->newiterator(read_options, handles_[1]));
  std::string value(1024, 'a');

  const int num_records = 10000;
  for (int i = 0; i < num_records; ++i) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%016d", i);

    slice key(buf, 16);
    assert_ok(put(1, key, value));

    iter->seek(key);
    assert_true(iter->valid());
    assert_eq(iter->key().compare(key), 0);
  }
}

test(dbtest, tailingiteratorseektonext) {
  createandreopenwithcf({"pikachu"});
  readoptions read_options;
  read_options.tailing = true;

  std::unique_ptr<iterator> iter(db_->newiterator(read_options, handles_[1]));
  std::string value(1024, 'a');

  const int num_records = 1000;
  for (int i = 1; i < num_records; ++i) {
    char buf1[32];
    char buf2[32];
    snprintf(buf1, sizeof(buf1), "00a0%016d", i * 5);

    slice key(buf1, 20);
    assert_ok(put(1, key, value));

    if (i % 100 == 99) {
      assert_ok(flush(1));
    }

    snprintf(buf2, sizeof(buf2), "00a0%016d", i * 5 - 2);
    slice target(buf2, 20);
    iter->seek(target);
    assert_true(iter->valid());
    assert_eq(iter->key().compare(key), 0);
  }
  for (int i = 2 * num_records; i > 0; --i) {
    char buf1[32];
    char buf2[32];
    snprintf(buf1, sizeof(buf1), "00a0%016d", i * 5);

    slice key(buf1, 20);
    assert_ok(put(1, key, value));

    if (i % 100 == 99) {
      assert_ok(flush(1));
    }

    snprintf(buf2, sizeof(buf2), "00a0%016d", i * 5 - 2);
    slice target(buf2, 20);
    iter->seek(target);
    assert_true(iter->valid());
    assert_eq(iter->key().compare(key), 0);
  }
}

test(dbtest, tailingiteratordeletes) {
  createandreopenwithcf({"pikachu"});
  readoptions read_options;
  read_options.tailing = true;

  std::unique_ptr<iterator> iter(db_->newiterator(read_options, handles_[1]));

  // write a single record, read it using the iterator, then delete it
  assert_ok(put(1, "0test", "test"));
  iter->seektofirst();
  assert_true(iter->valid());
  assert_eq(iter->key().tostring(), "0test");
  assert_ok(delete(1, "0test"));

  // write many more records
  const int num_records = 10000;
  std::string value(1024, 'a');

  for (int i = 0; i < num_records; ++i) {
    char buf[32];
    snprintf(buf, sizeof(buf), "1%015d", i);

    slice key(buf, 16);
    assert_ok(put(1, key, value));
  }

  // force a flush to make sure that no records are read from memtable
  assert_ok(flush(1));

  // skip "0test"
  iter->next();

  // make sure we can read all new records using the existing iterator
  int count = 0;
  for (; iter->valid(); iter->next(), ++count) ;

  assert_eq(count, num_records);
}

test(dbtest, tailingiteratorprefixseek) {
  readoptions read_options;
  read_options.tailing = true;

  options options = currentoptions();
  options.env = env_;
  options.create_if_missing = true;
  options.disable_auto_compactions = true;
  options.prefix_extractor.reset(newfixedprefixtransform(2));
  options.memtable_factory.reset(newhashskiplistrepfactory(16));
  destroyandreopen(&options);
  createandreopenwithcf({"pikachu"}, &options);

  std::unique_ptr<iterator> iter(db_->newiterator(read_options, handles_[1]));
  assert_ok(put(1, "0101", "test"));

  assert_ok(flush(1));

  assert_ok(put(1, "0202", "test"));

  // seek(0102) shouldn't find any records since 0202 has a different prefix
  iter->seek("0102");
  assert_true(!iter->valid());

  iter->seek("0202");
  assert_true(iter->valid());
  assert_eq(iter->key().tostring(), "0202");

  iter->next();
  assert_true(!iter->valid());
}

test(dbtest, tailingiteratorincomplete) {
  createandreopenwithcf({"pikachu"});
  readoptions read_options;
  read_options.tailing = true;
  read_options.read_tier = kblockcachetier;

  std::string key("key");
  std::string value("value");

  assert_ok(db_->put(writeoptions(), key, value));

  std::unique_ptr<iterator> iter(db_->newiterator(read_options));
  iter->seektofirst();
  // we either see the entry or it's not in cache
  assert_true(iter->valid() || iter->status().isincomplete());

  assert_ok(db_->compactrange(nullptr, nullptr));
  iter->seektofirst();
  // should still be true after compaction
  assert_true(iter->valid() || iter->status().isincomplete());
}

test(dbtest, tailingiteratorseektosame) {
  options options = currentoptions();
  options.compaction_style = kcompactionstyleuniversal;
  options.write_buffer_size = 1000;
  createandreopenwithcf({"pikachu"}, &options);

  readoptions read_options;
  read_options.tailing = true;

  const int nrows = 10000;
  // write rows with keys 00000, 00002, 00004 etc.
  for (int i = 0; i < nrows; ++i) {
    char buf[100];
    snprintf(buf, sizeof(buf), "%05d", 2*i);
    std::string key(buf);
    std::string value("value");
    assert_ok(db_->put(writeoptions(), key, value));
  }

  std::unique_ptr<iterator> iter(db_->newiterator(read_options));
  // seek to 00001.  we expect to find 00002.
  std::string start_key = "00001";
  iter->seek(start_key);
  assert_true(iter->valid());

  std::string found = iter->key().tostring();
  assert_eq("00002", found);

  // now seek to the same key.  the iterator should remain in the same
  // position.
  iter->seek(found);
  assert_true(iter->valid());
  assert_eq(found, iter->key().tostring());
}

test(dbtest, blockbasedtableprefixindextest) {
  // create a db with block prefix index
  blockbasedtableoptions table_options;
  options options = currentoptions();
  table_options.index_type = blockbasedtableoptions::khashsearch;
  options.table_factory.reset(newblockbasedtablefactory(table_options));
  options.prefix_extractor.reset(newfixedprefixtransform(1));


  reopen(&options);
  assert_ok(put("k1", "v1"));
  flush();
  assert_ok(put("k2", "v2"));

  // reopen it without prefix extractor, make sure everything still works.
  // rocksdb should just fall back to the binary index.
  table_options.index_type = blockbasedtableoptions::kbinarysearch;
  options.table_factory.reset(newblockbasedtablefactory(table_options));
  options.prefix_extractor.reset();

  reopen(&options);
  assert_eq("v1", get("k1"));
  assert_eq("v2", get("k2"));
}

test(dbtest, checksumtest) {
  blockbasedtableoptions table_options;
  options options = currentoptions();

  table_options.checksum = kcrc32c;
  options.table_factory.reset(newblockbasedtablefactory(table_options));
  reopen(&options);
  assert_ok(put("a", "b"));
  assert_ok(put("c", "d"));
  assert_ok(flush());  // table with crc checksum

  table_options.checksum = kxxhash;
  options.table_factory.reset(newblockbasedtablefactory(table_options));
  reopen(&options);
  assert_ok(put("e", "f"));
  assert_ok(put("g", "h"));
  assert_ok(flush());  // table with xxhash checksum

  table_options.checksum = kcrc32c;
  options.table_factory.reset(newblockbasedtablefactory(table_options));
  reopen(&options);
  assert_eq("b", get("a"));
  assert_eq("d", get("c"));
  assert_eq("f", get("e"));
  assert_eq("h", get("g"));

  table_options.checksum = kcrc32c;
  options.table_factory.reset(newblockbasedtablefactory(table_options));
  reopen(&options);
  assert_eq("b", get("a"));
  assert_eq("d", get("c"));
  assert_eq("f", get("e"));
  assert_eq("h", get("g"));
}

test(dbtest, fifocompactiontest) {
  for (int iter = 0; iter < 2; ++iter) {
    // first iteration -- auto compaction
    // second iteration -- manual compaction
    options options;
    options.compaction_style = kcompactionstylefifo;
    options.write_buffer_size = 100 << 10;                             // 100kb
    options.compaction_options_fifo.max_table_files_size = 500 << 10;  // 500kb
    options.compression = knocompression;
    options.create_if_missing = true;
    if (iter == 1) {
      options.disable_auto_compactions = true;
    }
    destroyandreopen(&options);

    random rnd(301);
    for (int i = 0; i < 6; ++i) {
      for (int j = 0; j < 100; ++j) {
        assert_ok(put(std::to_string(i * 100 + j), randomstring(&rnd, 1024)));
      }
      // flush should happen here
    }
    if (iter == 0) {
      assert_ok(dbfull()->test_waitforcompact());
    } else {
      assert_ok(db_->compactrange(nullptr, nullptr));
    }
    // only 5 files should survive
    assert_eq(numtablefilesatlevel(0), 5);
    for (int i = 0; i < 50; ++i) {
      // these keys should be deleted in previous compaction
      assert_eq("not_found", get(std::to_string(i)));
    }
  }
}

test(dbtest, simplewritetimeouttest) {
  options options;
  options.env = env_;
  options.create_if_missing = true;
  options.write_buffer_size = 100000;
  options.max_background_flushes = 0;
  options.max_write_buffer_number = 2;
  options.min_write_buffer_number_to_merge = 3;
  options.max_total_wal_size = std::numeric_limits<uint64_t>::max();
  writeoptions write_opt = writeoptions();
  write_opt.timeout_hint_us = 0;
  destroyandreopen(&options);
  // fill the two write buffer
  assert_ok(put(key(1), key(1) + std::string(100000, 'v'), write_opt));
  assert_ok(put(key(2), key(2) + std::string(100000, 'v'), write_opt));
  // as the only two write buffers are full in this moment, the third
  // put is expected to be timed-out.
  write_opt.timeout_hint_us = 50;
  assert_true(
      put(key(3), key(3) + std::string(100000, 'v'), write_opt).istimedout());
}

// multi-threaded timeout test
namespace {

static const int kvaluesize = 1000;
static const int kwritebuffersize = 100000;

struct timeoutwriterstate {
  int id;
  db* db;
  std::atomic<bool> done;
  std::map<int, std::string> success_kvs;
};

static void randomtimeoutwriter(void* arg) {
  timeoutwriterstate* state = reinterpret_cast<timeoutwriterstate*>(arg);
  static const uint64_t ktimerbias = 50;
  int thread_id = state->id;
  db* db = state->db;

  random rnd(1000 + thread_id);
  writeoptions write_opt = writeoptions();
  write_opt.timeout_hint_us = 500;
  int timeout_count = 0;
  int num_keys = knumkeys * 5;

  for (int k = 0; k < num_keys; ++k) {
    int key = k + thread_id * num_keys;
    std::string value = randomstring(&rnd, kvaluesize);
    // only the second-half is randomized
    if (k > num_keys / 2) {
      switch (rnd.next() % 5) {
        case 0:
          write_opt.timeout_hint_us = 500 * thread_id;
          break;
        case 1:
          write_opt.timeout_hint_us = num_keys - k;
          break;
        case 2:
          write_opt.timeout_hint_us = 1;
          break;
        default:
          write_opt.timeout_hint_us = 0;
          state->success_kvs.insert({key, value});
      }
    }

    uint64_t time_before_put = db->getenv()->nowmicros();
    status s = db->put(write_opt, key(key), value);
    uint64_t put_duration = db->getenv()->nowmicros() - time_before_put;
    if (write_opt.timeout_hint_us == 0 ||
        put_duration + ktimerbias < write_opt.timeout_hint_us) {
      assert_ok(s);
      std::string result;
    }
    if (s.istimedout()) {
      timeout_count++;
      assert_gt(put_duration + ktimerbias, write_opt.timeout_hint_us);
    }
  }

  state->done = true;
}

test(dbtest, mtrandomtimeouttest) {
  options options;
  options.env = env_;
  options.create_if_missing = true;
  options.max_write_buffer_number = 2;
  options.compression = knocompression;
  options.level0_slowdown_writes_trigger = 10;
  options.level0_stop_writes_trigger = 20;
  options.write_buffer_size = kwritebuffersize;
  destroyandreopen(&options);

  timeoutwriterstate thread_states[knumthreads];
  for (int tid = 0; tid < knumthreads; ++tid) {
    thread_states[tid].id = tid;
    thread_states[tid].db = db_;
    thread_states[tid].done = false;
    env_->startthread(randomtimeoutwriter, &thread_states[tid]);
  }

  for (int tid = 0; tid < knumthreads; ++tid) {
    while (thread_states[tid].done == false) {
      env_->sleepformicroseconds(100000);
    }
  }

  flush();

  for (int tid = 0; tid < knumthreads; ++tid) {
    auto& success_kvs = thread_states[tid].success_kvs;
    for (auto it = success_kvs.begin(); it != success_kvs.end(); ++it) {
      assert_eq(get(key(it->first)), it->second);
    }
  }
}

}  // anonymous namespace

/*
 * this test is not reliable enough as it heavily depends on disk behavior.
 */
test(dbtest, ratelimitingtest) {
  options options = currentoptions();
  options.write_buffer_size = 1 << 20;         // 1mb
  options.level0_file_num_compaction_trigger = 2;
  options.target_file_size_base = 1 << 20;     // 1mb
  options.max_bytes_for_level_base = 4 << 20;  // 4mb
  options.max_bytes_for_level_multiplier = 4;
  options.compression = knocompression;
  options.create_if_missing = true;
  options.env = env_;
  options.increaseparallelism(4);
  destroyandreopen(&options);

  writeoptions wo;
  wo.disablewal = true;

  // # no rate limiting
  random rnd(301);
  uint64_t start = env_->nowmicros();
  // write ~96m data
  for (int64_t i = 0; i < (96 << 10); ++i) {
    assert_ok(put(randomstring(&rnd, 32),
                  randomstring(&rnd, (1 << 10) + 1), wo));
  }
  uint64_t elapsed = env_->nowmicros() - start;
  double raw_rate = env_->bytes_written_ * 1000000 / elapsed;
  close();

  // # rate limiting with 0.7 x threshold
  options.rate_limiter.reset(
    newgenericratelimiter(static_cast<int64_t>(0.7 * raw_rate)));
  env_->bytes_written_ = 0;
  destroyandreopen(&options);

  start = env_->nowmicros();
  // write ~96m data
  for (int64_t i = 0; i < (96 << 10); ++i) {
    assert_ok(put(randomstring(&rnd, 32),
                  randomstring(&rnd, (1 << 10) + 1), wo));
  }
  elapsed = env_->nowmicros() - start;
  close();
  assert_true(options.rate_limiter->gettotalbytesthrough() ==
              env_->bytes_written_);
  double ratio = env_->bytes_written_ * 1000000 / elapsed / raw_rate;
  fprintf(stderr, "write rate ratio = %.2lf, expected 0.7\n", ratio);
  assert_true(ratio < 0.8);

  // # rate limiting with half of the raw_rate
  options.rate_limiter.reset(
    newgenericratelimiter(static_cast<int64_t>(raw_rate / 2)));
  env_->bytes_written_ = 0;
  destroyandreopen(&options);

  start = env_->nowmicros();
  // write ~96m data
  for (int64_t i = 0; i < (96 << 10); ++i) {
    assert_ok(put(randomstring(&rnd, 32),
                  randomstring(&rnd, (1 << 10) + 1), wo));
  }
  elapsed = env_->nowmicros() - start;
  close();
  assert_true(options.rate_limiter->gettotalbytesthrough() ==
              env_->bytes_written_);
  ratio = env_->bytes_written_ * 1000000 / elapsed / raw_rate;
  fprintf(stderr, "write rate ratio = %.2lf, expected 0.5\n", ratio);
  assert_true(ratio < 0.6);
}

test(dbtest, tableoptionssanitizetest) {
  options options = currentoptions();
  options.create_if_missing = true;
  destroyandreopen(&options);
  assert_eq(db_->getoptions().allow_mmap_reads, false);

  options.table_factory.reset(new plaintablefactory());
  options.prefix_extractor.reset(newnooptransform());
  destroy(&options);
  assert_true(tryreopen(&options).isnotsupported());
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
