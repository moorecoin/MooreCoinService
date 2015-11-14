//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/db_impl.h"

#define __stdc_format_macros
#include <inttypes.h>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "db/builder.h"
#include "db/db_iter.h"
#include "db/dbformat.h"
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/memtable_list.h"
#include "db/merge_context.h"
#include "db/merge_helper.h"
#include "db/table_cache.h"
#include "db/table_properties_collector.h"
#include "db/forward_iterator.h"
#include "db/transaction_log_impl.h"
#include "db/version_set.h"
#include "db/write_batch_internal.h"
#include "port/port.h"
#include "rocksdb/cache.h"
#include "port/likely.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/merge_operator.h"
#include "rocksdb/statistics.h"
#include "rocksdb/status.h"
#include "rocksdb/table.h"
#include "table/block.h"
#include "table/block_based_table_factory.h"
#include "table/merger.h"
#include "table/table_builder.h"
#include "table/two_level_iterator.h"
#include "util/auto_roll_logger.h"
#include "util/autovector.h"
#include "util/build_version.h"
#include "util/coding.h"
#include "util/hash_skiplist_rep.h"
#include "util/hash_linklist_rep.h"
#include "util/logging.h"
#include "util/log_buffer.h"
#include "util/mutexlock.h"
#include "util/perf_context_imp.h"
#include "util/iostats_context_imp.h"
#include "util/stop_watch.h"
#include "util/sync_point.h"

namespace rocksdb {

const std::string kdefaultcolumnfamilyname("default");

void dumpleveldbbuildversion(logger * log);

// information kept for every waiting writer
struct dbimpl::writer {
  status status;
  writebatch* batch;
  bool sync;
  bool disablewal;
  bool in_batch_group;
  bool done;
  uint64_t timeout_hint_us;
  port::condvar cv;

  explicit writer(port::mutex* mu) : cv(mu) { }
};

struct dbimpl::writecontext {
  autovector<superversion*> superversions_to_free_;
  autovector<log::writer*> logs_to_free_;

  ~writecontext() {
    for (auto& sv : superversions_to_free_) {
      delete sv;
    }
    for (auto& log : logs_to_free_) {
      delete log;
    }
  }
};

struct dbimpl::compactionstate {
  compaction* const compaction;

  // if there were two snapshots with seq numbers s1 and
  // s2 and s1 < s2, and if we find two instances of a key k1 then lies
  // entirely within s1 and s2, then the earlier version of k1 can be safely
  // deleted because that version is not visible in any snapshot.
  std::vector<sequencenumber> existing_snapshots;

  // files produced by compaction
  struct output {
    uint64_t number;
    uint32_t path_id;
    uint64_t file_size;
    internalkey smallest, largest;
    sequencenumber smallest_seqno, largest_seqno;
  };
  std::vector<output> outputs;
  std::list<uint64_t> allocated_file_numbers;

  // state kept for output being generated
  unique_ptr<writablefile> outfile;
  unique_ptr<tablebuilder> builder;

  uint64_t total_bytes;

  output* current_output() { return &outputs[outputs.size()-1]; }

  explicit compactionstate(compaction* c)
      : compaction(c),
        total_bytes(0) {
  }

  // create a client visible context of this compaction
  compactionfilter::context getfiltercontextv1() {
    compactionfilter::context context;
    context.is_full_compaction = compaction->isfullcompaction();
    context.is_manual_compaction = compaction->ismanualcompaction();
    return context;
  }

  // create a client visible context of this compaction
  compactionfiltercontext getfiltercontext() {
    compactionfiltercontext context;
    context.is_full_compaction = compaction->isfullcompaction();
    context.is_manual_compaction = compaction->ismanualcompaction();
    return context;
  }

  std::vector<std::string> key_str_buf_;
  std::vector<std::string> existing_value_str_buf_;
  // new_value_buf_ will only be appended if a value changes
  std::vector<std::string> new_value_buf_;
  // if values_changed_buf_[i] is true
  // new_value_buf_ will add a new entry with the changed value
  std::vector<bool> value_changed_buf_;
  // to_delete_buf_[i] is true iff key_buf_[i] is deleted
  std::vector<bool> to_delete_buf_;

  std::vector<std::string> other_key_str_buf_;
  std::vector<std::string> other_value_str_buf_;

  std::vector<slice> combined_key_buf_;
  std::vector<slice> combined_value_buf_;

  std::string cur_prefix_;

  // buffers the kv-pair that will be run through compaction filter v2
  // in the future.
  void bufferkeyvalueslices(const slice& key, const slice& value) {
    key_str_buf_.emplace_back(key.tostring());
    existing_value_str_buf_.emplace_back(value.tostring());
  }

  // buffers the kv-pair that will not be run through compaction filter v2
  // in the future.
  void bufferotherkeyvalueslices(const slice& key, const slice& value) {
    other_key_str_buf_.emplace_back(key.tostring());
    other_value_str_buf_.emplace_back(value.tostring());
  }

  // add a kv-pair to the combined buffer
  void addtocombinedkeyvalueslices(const slice& key, const slice& value) {
    // the real strings are stored in the batch buffers
    combined_key_buf_.emplace_back(key);
    combined_value_buf_.emplace_back(value);
  }

  // merging the two buffers
  void mergekeyvalueslicebuffer(const internalkeycomparator* comparator) {
    size_t i = 0;
    size_t j = 0;
    size_t total_size = key_str_buf_.size() + other_key_str_buf_.size();
    combined_key_buf_.reserve(total_size);
    combined_value_buf_.reserve(total_size);

    while (i + j < total_size) {
      int comp_res = 0;
      if (i < key_str_buf_.size() && j < other_key_str_buf_.size()) {
        comp_res = comparator->compare(key_str_buf_[i], other_key_str_buf_[j]);
      } else if (i >= key_str_buf_.size() && j < other_key_str_buf_.size()) {
        comp_res = 1;
      } else if (j >= other_key_str_buf_.size() && i < key_str_buf_.size()) {
        comp_res = -1;
      }
      if (comp_res > 0) {
        addtocombinedkeyvalueslices(other_key_str_buf_[j], other_value_str_buf_[j]);
        j++;
      } else if (comp_res < 0) {
        addtocombinedkeyvalueslices(key_str_buf_[i], existing_value_str_buf_[i]);
        i++;
      }
    }
  }

  void cleanupbatchbuffer() {
    to_delete_buf_.clear();
    key_str_buf_.clear();
    existing_value_str_buf_.clear();
    new_value_buf_.clear();
    value_changed_buf_.clear();

    to_delete_buf_.shrink_to_fit();
    key_str_buf_.shrink_to_fit();
    existing_value_str_buf_.shrink_to_fit();
    new_value_buf_.shrink_to_fit();
    value_changed_buf_.shrink_to_fit();

    other_key_str_buf_.clear();
    other_value_str_buf_.clear();
    other_key_str_buf_.shrink_to_fit();
    other_value_str_buf_.shrink_to_fit();
  }

  void cleanupmergedbuffer() {
    combined_key_buf_.clear();
    combined_value_buf_.clear();
    combined_key_buf_.shrink_to_fit();
    combined_value_buf_.shrink_to_fit();
  }
};

options sanitizeoptions(const std::string& dbname,
                        const internalkeycomparator* icmp,
                        const options& src) {
  auto db_options = sanitizeoptions(dbname, dboptions(src));
  auto cf_options = sanitizeoptions(icmp, columnfamilyoptions(src));
  return options(db_options, cf_options);
}

dboptions sanitizeoptions(const std::string& dbname, const dboptions& src) {
  dboptions result = src;

  // result.max_open_files means an "infinite" open files.
  if (result.max_open_files != -1) {
    cliptorange(&result.max_open_files, 20, 1000000);
  }

  if (result.info_log == nullptr) {
    status s = createloggerfromoptions(dbname, result.db_log_dir, src.env,
                                       result, &result.info_log);
    if (!s.ok()) {
      // no place suitable for logging
      result.info_log = nullptr;
    }
  }

  if (!result.rate_limiter) {
    if (result.bytes_per_sync == 0) {
      result.bytes_per_sync = 1024 * 1024;
    }
  }

  if (result.wal_dir.empty()) {
    // use dbname as default
    result.wal_dir = dbname;
  }
  if (result.wal_dir.back() == '/') {
    result.wal_dir = result.wal_dir.substr(0, result.wal_dir.size() - 1);
  }

  if (result.db_paths.size() == 0) {
    result.db_paths.emplace_back(dbname, std::numeric_limits<uint64_t>::max());
  }

  return result;
}

namespace {

status sanitizedboptionsbycfoptions(
    const dboptions* db_opts,
    const std::vector<columnfamilydescriptor>& column_families) {
  status s;
  for (auto cf : column_families) {
    s = cf.options.table_factory->sanitizedboptions(db_opts);
    if (!s.ok()) {
      return s;
    }
  }
  return status::ok();
}

compressiontype getcompressionflush(const options& options) {
  // compressing memtable flushes might not help unless the sequential load
  // optimization is used for leveled compaction. otherwise the cpu and
  // latency overhead is not offset by saving much space.

  bool can_compress;

  if (options.compaction_style == kcompactionstyleuniversal) {
    can_compress =
        (options.compaction_options_universal.compression_size_percent < 0);
  } else {
    // for leveled compress when min_level_to_compress == 0.
    can_compress = options.compression_per_level.empty() ||
                   options.compression_per_level[0] != knocompression;
  }

  if (can_compress) {
    return options.compression;
  } else {
    return knocompression;
  }
}
}  // namespace

dbimpl::dbimpl(const dboptions& options, const std::string& dbname)
    : env_(options.env),
      dbname_(dbname),
      options_(sanitizeoptions(dbname, options)),
      stats_(options_.statistics.get()),
      db_lock_(nullptr),
      mutex_(options.use_adaptive_mutex),
      shutting_down_(nullptr),
      bg_cv_(&mutex_),
      logfile_number_(0),
      log_empty_(true),
      default_cf_handle_(nullptr),
      total_log_size_(0),
      max_total_in_memory_state_(0),
      tmp_batch_(),
      bg_schedule_needed_(false),
      bg_compaction_scheduled_(0),
      bg_manual_only_(0),
      bg_flush_scheduled_(0),
      manual_compaction_(nullptr),
      disable_delete_obsolete_files_(0),
      delete_obsolete_files_last_run_(options.env->nowmicros()),
      purge_wal_files_last_run_(0),
      last_stats_dump_time_microsec_(0),
      default_interval_to_delete_obsolete_wal_(600),
      flush_on_destroy_(false),
      delayed_writes_(0),
      storage_options_(options),
      bg_work_gate_closed_(false),
      refitting_level_(false),
      opened_successfully_(false) {
  env_->getabsolutepath(dbname, &db_absolute_path_);

  // reserve ten files or so for other uses and give the rest to tablecache.
  // give a large number for setting of "infinite" open files.
  const int table_cache_size =
      (options_.max_open_files == -1) ? 4194304 : options_.max_open_files - 10;
  // reserve ten files or so for other uses and give the rest to tablecache.
  table_cache_ =
      newlrucache(table_cache_size, options_.table_cache_numshardbits,
                  options_.table_cache_remove_scan_count_limit);

  versions_.reset(
      new versionset(dbname_, &options_, storage_options_, table_cache_.get()));
  column_family_memtables_.reset(
      new columnfamilymemtablesimpl(versions_->getcolumnfamilyset()));

  dumpleveldbbuildversion(options_.info_log.get());
  dumpdbfilesummary(options_, dbname_);
  options_.dump(options_.info_log.get());

  logflush(options_.info_log);
}

dbimpl::~dbimpl() {
  mutex_.lock();
  if (flush_on_destroy_) {
    for (auto cfd : *versions_->getcolumnfamilyset()) {
      if (cfd->mem()->getfirstsequencenumber() != 0) {
        cfd->ref();
        mutex_.unlock();
        flushmemtable(cfd, flushoptions());
        mutex_.lock();
        cfd->unref();
      }
    }
    versions_->getcolumnfamilyset()->freedeadcolumnfamilies();
  }

  // wait for background work to finish
  shutting_down_.release_store(this);  // any non-nullptr value is ok
  while (bg_compaction_scheduled_ || bg_flush_scheduled_) {
    bg_cv_.wait();
  }

  if (default_cf_handle_ != nullptr) {
    // we need to delete handle outside of lock because it does its own locking
    mutex_.unlock();
    delete default_cf_handle_;
    mutex_.lock();
  }

  if (options_.allow_thread_local) {
    // clean up obsolete files due to superversion release.
    // (1) need to delete to obsolete files before closing because repairdb()
    // scans all existing files in the file system and builds manifest file.
    // keeping obsolete files confuses the repair process.
    // (2) need to check if we open()/recover() the db successfully before
    // deleting because if versionset recover fails (may be due to corrupted
    // manifest file), it is not able to identify live files correctly. as a
    // result, all "live" files can get deleted by accident. however, corrupted
    // manifest is recoverable by repairdb().
    if (opened_successfully_) {
      deletionstate deletion_state;
      findobsoletefiles(deletion_state, true);
      // manifest number starting from 2
      deletion_state.manifest_file_number = 1;
      if (deletion_state.havesomethingtodelete()) {
        purgeobsoletefiles(deletion_state);
      }
    }
  }

  // versions need to be destroyed before table_cache since it can hold
  // references to table_cache.
  versions_.reset();
  mutex_.unlock();
  if (db_lock_ != nullptr) {
    env_->unlockfile(db_lock_);
  }

  logflush(options_.info_log);
}

status dbimpl::newdb() {
  versionedit new_db;
  new_db.setlognumber(0);
  new_db.setnextfile(2);
  new_db.setlastsequence(0);

  log(options_.info_log, "creating manifest 1 \n");
  const std::string manifest = descriptorfilename(dbname_, 1);
  unique_ptr<writablefile> file;
  status s = env_->newwritablefile(
      manifest, &file, env_->optimizeformanifestwrite(storage_options_));
  if (!s.ok()) {
    return s;
  }
  file->setpreallocationblocksize(options_.manifest_preallocation_size);
  {
    log::writer log(std::move(file));
    std::string record;
    new_db.encodeto(&record);
    s = log.addrecord(record);
  }
  if (s.ok()) {
    // make "current" file that points to the new manifest file.
    s = setcurrentfile(env_, dbname_, 1, db_directory_.get());
  } else {
    env_->deletefile(manifest);
  }
  return s;
}

void dbimpl::maybeignoreerror(status* s) const {
  if (s->ok() || options_.paranoid_checks) {
    // no change needed
  } else {
    log(options_.info_log, "ignoring error %s", s->tostring().c_str());
    *s = status::ok();
  }
}

const status dbimpl::createarchivaldirectory() {
  if (options_.wal_ttl_seconds > 0 || options_.wal_size_limit_mb > 0) {
    std::string archivalpath = archivaldirectory(options_.wal_dir);
    return env_->createdirifmissing(archivalpath);
  }
  return status::ok();
}

void dbimpl::printstatistics() {
  auto dbstats = options_.statistics.get();
  if (dbstats) {
    log(options_.info_log,
        "statistcs:\n %s",
        dbstats->tostring().c_str());
  }
}

void dbimpl::maybedumpstats() {
  if (options_.stats_dump_period_sec == 0) return;

  const uint64_t now_micros = env_->nowmicros();

  if (last_stats_dump_time_microsec_ +
      options_.stats_dump_period_sec * 1000000
      <= now_micros) {
    // multiple threads could race in here simultaneously.
    // however, the last one will update last_stats_dump_time_microsec_
    // atomically. we could see more than one dump during one dump
    // period in rare cases.
    last_stats_dump_time_microsec_ = now_micros;

    bool tmp1 = false;
    bool tmp2 = false;
    dbpropertytype cf_property_type =
        getpropertytype("rocksdb.cfstats", &tmp1, &tmp2);
    dbpropertytype db_property_type =
        getpropertytype("rocksdb.dbstats", &tmp1, &tmp2);
    std::string stats;
    {
      mutexlock l(&mutex_);
      for (auto cfd : *versions_->getcolumnfamilyset()) {
        cfd->internal_stats()->getstringproperty(cf_property_type,
                                                 "rocksdb.cfstats", &stats);
      }
      default_cf_internal_stats_->getstringproperty(db_property_type,
                                                    "rocksdb.dbstats", &stats);
    }
    log(options_.info_log, "------- dumping stats -------");
    log(options_.info_log, "%s", stats.c_str());

    printstatistics();
  }
}

// returns the list of live files in 'sst_live' and the list
// of all files in the filesystem in 'candidate_files'.
// no_full_scan = true -- never do the full scan using getchildren()
// force = false -- don't force the full scan, except every
//  options_.delete_obsolete_files_period_micros
// force = true -- force the full scan
void dbimpl::findobsoletefiles(deletionstate& deletion_state,
                               bool force,
                               bool no_full_scan) {
  mutex_.assertheld();

  // if deletion is disabled, do nothing
  if (disable_delete_obsolete_files_ > 0) {
    return;
  }

  bool doing_the_full_scan = false;

  // logic for figurint out if we're doing the full scan
  if (no_full_scan) {
    doing_the_full_scan = false;
  } else if (force || options_.delete_obsolete_files_period_micros == 0) {
    doing_the_full_scan = true;
  } else {
    const uint64_t now_micros = env_->nowmicros();
    if (delete_obsolete_files_last_run_ +
        options_.delete_obsolete_files_period_micros < now_micros) {
      doing_the_full_scan = true;
      delete_obsolete_files_last_run_ = now_micros;
    }
  }

  // get obsolete files
  versions_->getobsoletefiles(&deletion_state.sst_delete_files);

  // store the current filenum, lognum, etc
  deletion_state.manifest_file_number = versions_->manifestfilenumber();
  deletion_state.pending_manifest_file_number =
      versions_->pendingmanifestfilenumber();
  deletion_state.log_number = versions_->minlognumber();
  deletion_state.prev_log_number = versions_->prevlognumber();

  if (!doing_the_full_scan && !deletion_state.havesomethingtodelete()) {
    // avoid filling up sst_live if we're sure that we
    // are not going to do the full scan and that we don't have
    // anything to delete at the moment
    return;
  }

  // don't delete live files
  for (auto pair : pending_outputs_) {
    deletion_state.sst_live.emplace_back(pair.first, pair.second, 0);
  }
  /*  deletion_state.sst_live.insert(pending_outputs_.begin(),
                                   pending_outputs_.end());*/
  versions_->addlivefiles(&deletion_state.sst_live);

  if (doing_the_full_scan) {
    for (uint32_t path_id = 0; path_id < options_.db_paths.size(); path_id++) {
      // set of all files in the directory. we'll exclude files that are still
      // alive in the subsequent processings.
      std::vector<std::string> files;
      env_->getchildren(options_.db_paths[path_id].path,
                        &files);  // ignore errors
      for (std::string file : files) {
        deletion_state.candidate_files.emplace_back(file, path_id);
      }
    }

    //add log files in wal_dir
    if (options_.wal_dir != dbname_) {
      std::vector<std::string> log_files;
      env_->getchildren(options_.wal_dir, &log_files); // ignore errors
      for (std::string log_file : log_files) {
        deletion_state.candidate_files.emplace_back(log_file, 0);
      }
    }
    // add info log files in db_log_dir
    if (!options_.db_log_dir.empty() && options_.db_log_dir != dbname_) {
      std::vector<std::string> info_log_files;
      env_->getchildren(options_.db_log_dir, &info_log_files);  // ignore errors
      for (std::string log_file : info_log_files) {
        deletion_state.candidate_files.emplace_back(log_file, 0);
      }
    }
  }
}

namespace {
bool comparecandidatefile(const rocksdb::dbimpl::candidatefileinfo& first,
                          const rocksdb::dbimpl::candidatefileinfo& second) {
  if (first.file_name > second.file_name) {
    return true;
  } else if (first.file_name < second.file_name) {
    return false;
  } else {
    return (first.path_id > second.path_id);
  }
}
};  // namespace

// diffs the files listed in filenames and those that do not
// belong to live files are posibly removed. also, removes all the
// files in sst_delete_files and log_delete_files.
// it is not necessary to hold the mutex when invoking this method.
void dbimpl::purgeobsoletefiles(deletionstate& state) {
  // we'd better have sth to delete
  assert(state.havesomethingtodelete());

  // this checks if findobsoletefiles() was run before. if not, don't do
  // purgeobsoletefiles(). if findobsoletefiles() was run, we need to also
  // run purgeobsoletefiles(), even if disable_delete_obsolete_files_ is true
  if (state.manifest_file_number == 0) {
    return;
  }

  // now, convert live list to an unordered map, without mutex held;
  // set is slow.
  std::unordered_map<uint64_t, const filedescriptor*> sst_live_map;
  for (filedescriptor& fd : state.sst_live) {
    sst_live_map[fd.getnumber()] = &fd;
  }

  auto& candidate_files = state.candidate_files;
  candidate_files.reserve(
      candidate_files.size() +
      state.sst_delete_files.size() +
      state.log_delete_files.size());
  // we may ignore the dbname when generating the file names.
  const char* kdumbdbname = "";
  for (auto file : state.sst_delete_files) {
    candidate_files.emplace_back(
        maketablefilename(kdumbdbname, file->fd.getnumber()),
        file->fd.getpathid());
    delete file;
  }

  for (auto file_num : state.log_delete_files) {
    if (file_num > 0) {
      candidate_files.emplace_back(logfilename(kdumbdbname, file_num).substr(1),
                                   0);
    }
  }

  // dedup state.candidate_files so we don't try to delete the same
  // file twice
  sort(candidate_files.begin(), candidate_files.end(), comparecandidatefile);
  candidate_files.erase(unique(candidate_files.begin(), candidate_files.end()),
                        candidate_files.end());

  std::vector<std::string> old_info_log_files;
  infologprefix info_log_prefix(!options_.db_log_dir.empty(), dbname_);
  for (const auto& candidate_file : candidate_files) {
    std::string to_delete = candidate_file.file_name;
    uint32_t path_id = candidate_file.path_id;
    uint64_t number;
    filetype type;
    // ignore file if we cannot recognize it.
    if (!parsefilename(to_delete, &number, info_log_prefix.prefix, &type)) {
      continue;
    }

    bool keep = true;
    switch (type) {
      case klogfile:
        keep = ((number >= state.log_number) ||
                (number == state.prev_log_number));
        break;
      case kdescriptorfile:
        // keep my manifest file, and any newer incarnations'
        // (can happen during manifest roll)
        keep = (number >= state.manifest_file_number);
        break;
      case ktablefile:
        keep = (sst_live_map.find(number) != sst_live_map.end());
        break;
      case ktempfile:
        // any temp files that are currently being written to must
        // be recorded in pending_outputs_, which is inserted into "live".
        // also, setcurrentfile creates a temp file when writing out new
        // manifest, which is equal to state.pending_manifest_file_number. we
        // should not delete that file
        keep = (sst_live_map.find(number) != sst_live_map.end()) ||
               (number == state.pending_manifest_file_number);
        break;
      case kinfologfile:
        keep = true;
        if (number != 0) {
          old_info_log_files.push_back(to_delete);
        }
        break;
      case kcurrentfile:
      case kdblockfile:
      case kidentityfile:
      case kmetadatabase:
        keep = true;
        break;
    }

    if (keep) {
      continue;
    }

    std::string fname;
    if (type == ktablefile) {
      // evict from cache
      tablecache::evict(table_cache_.get(), number);
      fname = tablefilename(options_.db_paths, number, path_id);
    } else {
      fname =
          ((type == klogfile) ? options_.wal_dir : dbname_) + "/" + to_delete;
    }

    if (type == klogfile &&
        (options_.wal_ttl_seconds > 0 || options_.wal_size_limit_mb > 0)) {
      auto archived_log_name = archivedlogfilename(options_.wal_dir, number);
      // the sync point below is used in (dbtest,transactionlogiteratorrace)
      test_sync_point("dbimpl::purgeobsoletefiles:1");
      status s = env_->renamefile(fname, archived_log_name);
      // the sync point below is used in (dbtest,transactionlogiteratorrace)
      test_sync_point("dbimpl::purgeobsoletefiles:2");
      log(options_.info_log,
          "move log file %s to %s -- %s\n",
          fname.c_str(), archived_log_name.c_str(), s.tostring().c_str());
    } else {
      status s = env_->deletefile(fname);
      log(options_.info_log, "delete %s type=%d #%" priu64 " -- %s\n",
          fname.c_str(), type, number, s.tostring().c_str());
    }
  }

  // delete old info log files.
  size_t old_info_log_file_count = old_info_log_files.size();
  if (old_info_log_file_count >= options_.keep_log_file_num) {
    std::sort(old_info_log_files.begin(), old_info_log_files.end());
    size_t end = old_info_log_file_count - options_.keep_log_file_num;
    for (unsigned int i = 0; i <= end; i++) {
      std::string& to_delete = old_info_log_files.at(i);
      std::string full_path_to_delete =
          (options_.db_log_dir.empty() ? dbname_ : options_.db_log_dir) + "/" +
          to_delete;
      log(options_.info_log, "delete info log file %s\n",
          full_path_to_delete.c_str());
      status s = env_->deletefile(full_path_to_delete);
      if (!s.ok()) {
        log(options_.info_log, "delete info log file %s failed -- %s\n",
            to_delete.c_str(), s.tostring().c_str());
      }
    }
  }
  purgeobsoletewalfiles();
  logflush(options_.info_log);
}

void dbimpl::deleteobsoletefiles() {
  mutex_.assertheld();
  deletionstate deletion_state;
  findobsoletefiles(deletion_state, true);
  if (deletion_state.havesomethingtodelete()) {
    purgeobsoletefiles(deletion_state);
  }
}

#ifndef rocksdb_lite
// 1. go through all archived files and
//    a. if ttl is enabled, delete outdated files
//    b. if archive size limit is enabled, delete empty files,
//        compute file number and size.
// 2. if size limit is enabled:
//    a. compute how many files should be deleted
//    b. get sorted non-empty archived logs
//    c. delete what should be deleted
void dbimpl::purgeobsoletewalfiles() {
  bool const ttl_enabled = options_.wal_ttl_seconds > 0;
  bool const size_limit_enabled =  options_.wal_size_limit_mb > 0;
  if (!ttl_enabled && !size_limit_enabled) {
    return;
  }

  int64_t current_time;
  status s = env_->getcurrenttime(&current_time);
  if (!s.ok()) {
    log(options_.info_log, "can't get current time: %s", s.tostring().c_str());
    assert(false);
    return;
  }
  uint64_t const now_seconds = static_cast<uint64_t>(current_time);
  uint64_t const time_to_check = (ttl_enabled && !size_limit_enabled) ?
    options_.wal_ttl_seconds / 2 : default_interval_to_delete_obsolete_wal_;

  if (purge_wal_files_last_run_ + time_to_check > now_seconds) {
    return;
  }

  purge_wal_files_last_run_ = now_seconds;

  std::string archival_dir = archivaldirectory(options_.wal_dir);
  std::vector<std::string> files;
  s = env_->getchildren(archival_dir, &files);
  if (!s.ok()) {
    log(options_.info_log, "can't get archive files: %s", s.tostring().c_str());
    assert(false);
    return;
  }

  size_t log_files_num = 0;
  uint64_t log_file_size = 0;

  for (auto& f : files) {
    uint64_t number;
    filetype type;
    if (parsefilename(f, &number, &type) && type == klogfile) {
      std::string const file_path = archival_dir + "/" + f;
      if (ttl_enabled) {
        uint64_t file_m_time;
        status const s = env_->getfilemodificationtime(file_path,
          &file_m_time);
        if (!s.ok()) {
          log(options_.info_log, "can't get file mod time: %s: %s",
              file_path.c_str(), s.tostring().c_str());
          continue;
        }
        if (now_seconds - file_m_time > options_.wal_ttl_seconds) {
          status const s = env_->deletefile(file_path);
          if (!s.ok()) {
            log(options_.info_log, "can't delete file: %s: %s",
                file_path.c_str(), s.tostring().c_str());
            continue;
          } else {
            mutexlock l(&read_first_record_cache_mutex_);
            read_first_record_cache_.erase(number);
          }
          continue;
        }
      }

      if (size_limit_enabled) {
        uint64_t file_size;
        status const s = env_->getfilesize(file_path, &file_size);
        if (!s.ok()) {
          log(options_.info_log, "can't get file size: %s: %s",
              file_path.c_str(), s.tostring().c_str());
          return;
        } else {
          if (file_size > 0) {
            log_file_size = std::max(log_file_size, file_size);
            ++log_files_num;
          } else {
            status s = env_->deletefile(file_path);
            if (!s.ok()) {
              log(options_.info_log, "can't delete file: %s: %s",
                  file_path.c_str(), s.tostring().c_str());
              continue;
            } else {
              mutexlock l(&read_first_record_cache_mutex_);
              read_first_record_cache_.erase(number);
            }
          }
        }
      }
    }
  }

  if (0 == log_files_num || !size_limit_enabled) {
    return;
  }

  size_t const files_keep_num = options_.wal_size_limit_mb *
    1024 * 1024 / log_file_size;
  if (log_files_num <= files_keep_num) {
    return;
  }

  size_t files_del_num = log_files_num - files_keep_num;
  vectorlogptr archived_logs;
  getsortedwalsoftype(archival_dir, archived_logs, karchivedlogfile);

  if (files_del_num > archived_logs.size()) {
    log(options_.info_log, "trying to delete more archived log files than "
        "exist. deleting all");
    files_del_num = archived_logs.size();
  }

  for (size_t i = 0; i < files_del_num; ++i) {
    std::string const file_path = archived_logs[i]->pathname();
    status const s = deletefile(file_path);
    if (!s.ok()) {
      log(options_.info_log, "can't delete file: %s: %s",
          file_path.c_str(), s.tostring().c_str());
      continue;
    } else {
      mutexlock l(&read_first_record_cache_mutex_);
      read_first_record_cache_.erase(archived_logs[i]->lognumber());
    }
  }
}

namespace {
struct comparelogbypointer {
  bool operator()(const unique_ptr<logfile>& a, const unique_ptr<logfile>& b) {
    logfileimpl* a_impl = dynamic_cast<logfileimpl*>(a.get());
    logfileimpl* b_impl = dynamic_cast<logfileimpl*>(b.get());
    return *a_impl < *b_impl;
  }
};
}

status dbimpl::getsortedwalsoftype(const std::string& path,
                                   vectorlogptr& log_files,
                                   walfiletype log_type) {
  std::vector<std::string> all_files;
  const status status = env_->getchildren(path, &all_files);
  if (!status.ok()) {
    return status;
  }
  log_files.reserve(all_files.size());
  for (const auto& f : all_files) {
    uint64_t number;
    filetype type;
    if (parsefilename(f, &number, &type) && type == klogfile) {
      sequencenumber sequence;
      status s = readfirstrecord(log_type, number, &sequence);
      if (!s.ok()) {
        return s;
      }
      if (sequence == 0) {
        // empty file
        continue;
      }

      // reproduce the race condition where a log file is moved
      // to archived dir, between these two sync points, used in
      // (dbtest,transactionlogiteratorrace)
      test_sync_point("dbimpl::getsortedwalsoftype:1");
      test_sync_point("dbimpl::getsortedwalsoftype:2");

      uint64_t size_bytes;
      s = env_->getfilesize(logfilename(path, number), &size_bytes);
      // re-try in case the alive log file has been moved to archive.
      if (!s.ok() && log_type == kalivelogfile &&
          env_->fileexists(archivedlogfilename(path, number))) {
        s = env_->getfilesize(archivedlogfilename(path, number), &size_bytes);
      }
      if (!s.ok()) {
        return s;
      }

      log_files.push_back(std::move(unique_ptr<logfile>(
          new logfileimpl(number, log_type, sequence, size_bytes))));
    }
  }
  comparelogbypointer compare_log_files;
  std::sort(log_files.begin(), log_files.end(), compare_log_files);
  return status;
}

status dbimpl::retainprobablewalfiles(vectorlogptr& all_logs,
                                      const sequencenumber target) {
  int64_t start = 0;  // signed to avoid overflow when target is < first file.
  int64_t end = static_cast<int64_t>(all_logs.size()) - 1;
  // binary search. avoid opening all files.
  while (end >= start) {
    int64_t mid = start + (end - start) / 2;  // avoid overflow.
    sequencenumber current_seq_num = all_logs.at(mid)->startsequence();
    if (current_seq_num == target) {
      end = mid;
      break;
    } else if (current_seq_num < target) {
      start = mid + 1;
    } else {
      end = mid - 1;
    }
  }
  // end could be -ve.
  size_t start_index = std::max(static_cast<int64_t>(0), end);
  // the last wal file is always included
  all_logs.erase(all_logs.begin(), all_logs.begin() + start_index);
  return status::ok();
}

status dbimpl::readfirstrecord(const walfiletype type, const uint64_t number,
                               sequencenumber* sequence) {
  if (type != kalivelogfile && type != karchivedlogfile) {
    return status::notsupported("file type not known " + std::to_string(type));
  }
  {
    mutexlock l(&read_first_record_cache_mutex_);
    auto itr = read_first_record_cache_.find(number);
    if (itr != read_first_record_cache_.end()) {
      *sequence = itr->second;
      return status::ok();
    }
  }
  status s;
  if (type == kalivelogfile) {
    std::string fname = logfilename(options_.wal_dir, number);
    s = readfirstline(fname, sequence);
    if (env_->fileexists(fname) && !s.ok()) {
      // return any error that is not caused by non-existing file
      return s;
    }
  }

  if (type == karchivedlogfile || !s.ok()) {
    //  check if the file got moved to archive.
    std::string archived_file = archivedlogfilename(options_.wal_dir, number);
    s = readfirstline(archived_file, sequence);
  }

  if (s.ok() && *sequence != 0) {
    mutexlock l(&read_first_record_cache_mutex_);
    read_first_record_cache_.insert({number, *sequence});
  }
  return s;
}

// the function returns status.ok() and sequence == 0 if the file exists, but is
// empty
status dbimpl::readfirstline(const std::string& fname,
                             sequencenumber* sequence) {
  struct logreporter : public log::reader::reporter {
    env* env;
    logger* info_log;
    const char* fname;

    status* status;
    bool ignore_error;  // true if options_.paranoid_checks==false
    virtual void corruption(size_t bytes, const status& s) {
      log(info_log, "%s%s: dropping %d bytes; %s",
          (this->ignore_error ? "(ignoring error) " : ""), fname,
          static_cast<int>(bytes), s.tostring().c_str());
      if (this->status->ok()) {
        // only keep the first error
        *this->status = s;
      }
    }
  };

  unique_ptr<sequentialfile> file;
  status status = env_->newsequentialfile(fname, &file, storage_options_);

  if (!status.ok()) {
    return status;
  }

  logreporter reporter;
  reporter.env = env_;
  reporter.info_log = options_.info_log.get();
  reporter.fname = fname.c_str();
  reporter.status = &status;
  reporter.ignore_error = !options_.paranoid_checks;
  log::reader reader(std::move(file), &reporter, true /*checksum*/,
                     0 /*initial_offset*/);
  std::string scratch;
  slice record;

  if (reader.readrecord(&record, &scratch) &&
      (status.ok() || !options_.paranoid_checks)) {
    if (record.size() < 12) {
      reporter.corruption(record.size(),
                          status::corruption("log record too small"));
      // todo read record's till the first no corrupt entry?
    } else {
      writebatch batch;
      writebatchinternal::setcontents(&batch, record);
      *sequence = writebatchinternal::sequence(&batch);
      return status::ok();
    }
  }

  // readrecord returns false on eof, which means that the log file is empty. we
  // return status.ok() in that case and set sequence number to 0
  *sequence = 0;
  return status;
}

#endif  // rocksdb_lite

status dbimpl::recover(
    const std::vector<columnfamilydescriptor>& column_families, bool read_only,
    bool error_if_log_file_exist) {
  mutex_.assertheld();

  bool is_new_db = false;
  assert(db_lock_ == nullptr);
  if (!read_only) {
    // we call createdirifmissing() as the directory may already exist (if we
    // are reopening a db), when this happens we don't want creating the
    // directory to cause an error. however, we need to check if creating the
    // directory fails or else we may get an obscure message about the lock
    // file not existing. one real-world example of this occurring is if
    // env->createdirifmissing() doesn't create intermediate directories, e.g.
    // when dbname_ is "dir/db" but when "dir" doesn't exist.
    status s = env_->createdirifmissing(dbname_);
    if (!s.ok()) {
      return s;
    }

    for (auto& db_path : options_.db_paths) {
      s = env_->createdirifmissing(db_path.path);
      if (!s.ok()) {
        return s;
      }
    }

    s = env_->newdirectory(dbname_, &db_directory_);
    if (!s.ok()) {
      return s;
    }

    s = env_->lockfile(lockfilename(dbname_), &db_lock_);
    if (!s.ok()) {
      return s;
    }

    if (!env_->fileexists(currentfilename(dbname_))) {
      if (options_.create_if_missing) {
        s = newdb();
        is_new_db = true;
        if (!s.ok()) {
          return s;
        }
      } else {
        return status::invalidargument(
            dbname_, "does not exist (create_if_missing is false)");
      }
    } else {
      if (options_.error_if_exists) {
        return status::invalidargument(
            dbname_, "exists (error_if_exists is true)");
      }
    }
    // check for the identity file and create it if not there
    if (!env_->fileexists(identityfilename(dbname_))) {
      s = setidentityfile(env_, dbname_);
      if (!s.ok()) {
        return s;
      }
    }
  }

  status s = versions_->recover(column_families, read_only);
  if (options_.paranoid_checks && s.ok()) {
    s = checkconsistency();
  }
  if (s.ok()) {
    sequencenumber max_sequence(0);
    default_cf_handle_ = new columnfamilyhandleimpl(
        versions_->getcolumnfamilyset()->getdefault(), this, &mutex_);
    default_cf_internal_stats_ = default_cf_handle_->cfd()->internal_stats();
    single_column_family_mode_ =
        versions_->getcolumnfamilyset()->numberofcolumnfamilies() == 1;

    // recover from all newer log files than the ones named in the
    // descriptor (new log files may have been added by the previous
    // incarnation without registering them in the descriptor).
    //
    // note that prevlognumber() is no longer used, but we pay
    // attention to it in case we are recovering a database
    // produced by an older version of rocksdb.
    const uint64_t min_log = versions_->minlognumber();
    const uint64_t prev_log = versions_->prevlognumber();
    std::vector<std::string> filenames;
    s = env_->getchildren(options_.wal_dir, &filenames);
    if (!s.ok()) {
      return s;
    }

    std::vector<uint64_t> logs;
    for (size_t i = 0; i < filenames.size(); i++) {
      uint64_t number;
      filetype type;
      if (parsefilename(filenames[i], &number, &type) && type == klogfile) {
        if (is_new_db) {
          return status::corruption(
              "while creating a new db, wal_dir contains "
              "existing log file: ",
              filenames[i]);
        } else if ((number >= min_log) || (number == prev_log)) {
          logs.push_back(number);
        }
      }
    }

    if (logs.size() > 0 && error_if_log_file_exist) {
      return status::corruption(""
          "the db was opened in readonly mode with error_if_log_file_exist"
          "flag but a log file already exists");
    }

    // recover in the order in which the logs were generated
    std::sort(logs.begin(), logs.end());
    for (const auto& log : logs) {
      // the previous incarnation may not have written any manifest
      // records after allocating this log number.  so we manually
      // update the file number allocation counter in versionset.
      versions_->markfilenumberused(log);
      s = recoverlogfile(log, &max_sequence, read_only);
    }
    settickercount(stats_, sequence_number, versions_->lastsequence());
  }

  for (auto cfd : *versions_->getcolumnfamilyset()) {
    max_total_in_memory_state_ += cfd->options()->write_buffer_size *
                                  cfd->options()->max_write_buffer_number;
  }

  return s;
}

status dbimpl::recoverlogfile(uint64_t log_number, sequencenumber* max_sequence,
                              bool read_only) {
  struct logreporter : public log::reader::reporter {
    env* env;
    logger* info_log;
    const char* fname;
    status* status;  // nullptr if options_.paranoid_checks==false or
                     //            options_.skip_log_error_on_recovery==true
    virtual void corruption(size_t bytes, const status& s) {
      log(info_log, "%s%s: dropping %d bytes; %s",
          (this->status == nullptr ? "(ignoring error) " : ""),
          fname, static_cast<int>(bytes), s.tostring().c_str());
      if (this->status != nullptr && this->status->ok()) *this->status = s;
    }
  };

  mutex_.assertheld();

  std::unordered_map<int, versionedit> version_edits;
  // no need to refcount because iteration is under mutex
  for (auto cfd : *versions_->getcolumnfamilyset()) {
    versionedit edit;
    edit.setcolumnfamily(cfd->getid());
    version_edits.insert({cfd->getid(), edit});
  }

  // open the log file
  std::string fname = logfilename(options_.wal_dir, log_number);
  unique_ptr<sequentialfile> file;
  status status = env_->newsequentialfile(fname, &file, storage_options_);
  if (!status.ok()) {
    maybeignoreerror(&status);
    return status;
  }

  // create the log reader.
  logreporter reporter;
  reporter.env = env_;
  reporter.info_log = options_.info_log.get();
  reporter.fname = fname.c_str();
  reporter.status = (options_.paranoid_checks &&
                     !options_.skip_log_error_on_recovery ? &status : nullptr);
  // we intentially make log::reader do checksumming even if
  // paranoid_checks==false so that corruptions cause entire commits
  // to be skipped instead of propagating bad information (like overly
  // large sequence numbers).
  log::reader reader(std::move(file), &reporter, true/*checksum*/,
                     0/*initial_offset*/);
  log(options_.info_log, "recovering log #%" priu64 "", log_number);

  // read all the records and add to a memtable
  std::string scratch;
  slice record;
  writebatch batch;
  while (reader.readrecord(&record, &scratch)) {
    if (record.size() < 12) {
      reporter.corruption(record.size(),
                          status::corruption("log record too small"));
      continue;
    }
    writebatchinternal::setcontents(&batch, record);

    // if column family was not found, it might mean that the wal write
    // batch references to the column family that was dropped after the
    // insert. we don't want to fail the whole write batch in that case -- we
    // just ignore the update. that's why we set ignore missing column families
    // to true
    status = writebatchinternal::insertinto(
        &batch, column_family_memtables_.get(),
        true /* ignore missing column families */, log_number);

    maybeignoreerror(&status);
    if (!status.ok()) {
      return status;
    }
    const sequencenumber last_seq =
        writebatchinternal::sequence(&batch) +
        writebatchinternal::count(&batch) - 1;
    if (last_seq > *max_sequence) {
      *max_sequence = last_seq;
    }

    if (!read_only) {
      // no need to refcount since client still doesn't have access
      // to the db and can not drop column families while we iterate
      for (auto cfd : *versions_->getcolumnfamilyset()) {
        if (cfd->mem()->shouldflush()) {
          // if this asserts, it means that insertinto failed in
          // filtering updates to already-flushed column families
          assert(cfd->getlognumber() <= log_number);
          auto iter = version_edits.find(cfd->getid());
          assert(iter != version_edits.end());
          versionedit* edit = &iter->second;
          status = writelevel0tableforrecovery(cfd, cfd->mem(), edit);
          // we still want to clear the memtable, even if the recovery failed
          cfd->createnewmemtable();
          if (!status.ok()) {
            // reflect errors immediately so that conditions like full
            // file-systems cause the db::open() to fail.
            return status;
          }
        }
      }
    }
  }

  if (versions_->lastsequence() < *max_sequence) {
    versions_->setlastsequence(*max_sequence);
  }

  if (!read_only) {
    // no need to refcount since client still doesn't have access
    // to the db and can not drop column families while we iterate
    for (auto cfd : *versions_->getcolumnfamilyset()) {
      auto iter = version_edits.find(cfd->getid());
      assert(iter != version_edits.end());
      versionedit* edit = &iter->second;

      if (cfd->getlognumber() > log_number) {
        // column family cfd has already flushed the data
        // from log_number. memtable has to be empty because
        // we filter the updates based on log_number
        // (in writebatch::insertinto)
        assert(cfd->mem()->getfirstsequencenumber() == 0);
        assert(edit->numentries() == 0);
        continue;
      }

      // flush the final memtable (if non-empty)
      if (cfd->mem()->getfirstsequencenumber() != 0) {
        status = writelevel0tableforrecovery(cfd, cfd->mem(), edit);
      }
      // we still want to clear the memtable, even if the recovery failed
      cfd->createnewmemtable();
      if (!status.ok()) {
        return status;
      }

      // write manifest with update
      // writing log number in the manifest means that any log file
      // with number strongly less than (log_number + 1) is already
      // recovered and should be ignored on next reincarnation.
      // since we already recovered log_number, we want all logs
      // with numbers `<= log_number` (includes this one) to be ignored
      edit->setlognumber(log_number + 1);
      // we must mark the next log number as used, even though it's
      // not actually used. that is because versionset assumes
      // versionset::next_file_number_ always to be strictly greater than any
      // log number
      versions_->markfilenumberused(log_number + 1);
      status = versions_->logandapply(cfd, edit, &mutex_);
      if (!status.ok()) {
        return status;
      }
    }
  }

  return status;
}

status dbimpl::writelevel0tableforrecovery(columnfamilydata* cfd, memtable* mem,
                                           versionedit* edit) {
  mutex_.assertheld();
  const uint64_t start_micros = env_->nowmicros();
  filemetadata meta;
  meta.fd = filedescriptor(versions_->newfilenumber(), 0, 0);
  pending_outputs_[meta.fd.getnumber()] = 0;  // path 0 for level 0 file.
  readoptions ro;
  ro.total_order_seek = true;
  iterator* iter = mem->newiterator(ro);
  const sequencenumber newest_snapshot = snapshots_.getnewest();
  const sequencenumber earliest_seqno_in_memtable =
    mem->getfirstsequencenumber();
  log(options_.info_log, "[%s] level-0 table #%" priu64 ": started",
      cfd->getname().c_str(), meta.fd.getnumber());

  status s;
  {
    mutex_.unlock();
    s = buildtable(dbname_, env_, *cfd->options(), storage_options_,
                   cfd->table_cache(), iter, &meta, cfd->internal_comparator(),
                   newest_snapshot, earliest_seqno_in_memtable,
                   getcompressionflush(*cfd->options()), env::io_high);
    logflush(options_.info_log);
    mutex_.lock();
  }

  log(options_.info_log,
      "[%s] level-0 table #%" priu64 ": %" priu64 " bytes %s",
      cfd->getname().c_str(), meta.fd.getnumber(), meta.fd.getfilesize(),
      s.tostring().c_str());
  delete iter;

  pending_outputs_.erase(meta.fd.getnumber());

  // note that if file_size is zero, the file has been deleted and
  // should not be added to the manifest.
  int level = 0;
  if (s.ok() && meta.fd.getfilesize() > 0) {
    edit->addfile(level, meta.fd.getnumber(), meta.fd.getpathid(),
                  meta.fd.getfilesize(), meta.smallest, meta.largest,
                  meta.smallest_seqno, meta.largest_seqno);
  }

  internalstats::compactionstats stats(1);
  stats.micros = env_->nowmicros() - start_micros;
  stats.bytes_written = meta.fd.getfilesize();
  stats.files_out_levelnp1 = 1;
  cfd->internal_stats()->addcompactionstats(level, stats);
  cfd->internal_stats()->addcfstats(
      internalstats::bytes_flushed, meta.fd.getfilesize());
  recordtick(stats_, compact_write_bytes, meta.fd.getfilesize());
  return s;
}

status dbimpl::writelevel0table(columnfamilydata* cfd,
                                autovector<memtable*>& mems, versionedit* edit,
                                uint64_t* filenumber, logbuffer* log_buffer) {
  mutex_.assertheld();
  const uint64_t start_micros = env_->nowmicros();
  filemetadata meta;

  meta.fd = filedescriptor(versions_->newfilenumber(), 0, 0);
  *filenumber = meta.fd.getnumber();
  pending_outputs_[meta.fd.getnumber()] = 0;  // path 0 for level 0 file.

  const sequencenumber newest_snapshot = snapshots_.getnewest();
  const sequencenumber earliest_seqno_in_memtable =
    mems[0]->getfirstsequencenumber();
  version* base = cfd->current();
  base->ref();          // it is likely that we do not need this reference
  status s;
  {
    mutex_.unlock();
    log_buffer->flushbuffertolog();
    std::vector<iterator*> memtables;
    readoptions ro;
    ro.total_order_seek = true;
    for (memtable* m : mems) {
      log(options_.info_log,
          "[%s] flushing memtable with next log file: %" priu64 "\n",
          cfd->getname().c_str(), m->getnextlognumber());
      memtables.push_back(m->newiterator(ro));
    }
    iterator* iter = newmergingiterator(&cfd->internal_comparator(),
                                        &memtables[0], memtables.size());
    log(options_.info_log, "[%s] level-0 flush table #%" priu64 ": started",
        cfd->getname().c_str(), meta.fd.getnumber());

    s = buildtable(dbname_, env_, *cfd->options(), storage_options_,
                   cfd->table_cache(), iter, &meta, cfd->internal_comparator(),
                   newest_snapshot, earliest_seqno_in_memtable,
                   getcompressionflush(*cfd->options()), env::io_high);
    logflush(options_.info_log);
    delete iter;
    log(options_.info_log,
        "[%s] level-0 flush table #%" priu64 ": %" priu64 " bytes %s",
        cfd->getname().c_str(), meta.fd.getnumber(), meta.fd.getfilesize(),
        s.tostring().c_str());

    if (!options_.disabledatasync) {
      db_directory_->fsync();
    }
    mutex_.lock();
  }
  base->unref();

  // re-acquire the most current version
  base = cfd->current();

  // there could be multiple threads writing to its own level-0 file.
  // the pending_outputs cannot be cleared here, otherwise this newly
  // created file might not be considered as a live-file by another
  // compaction thread that is concurrently deleting obselete files.
  // the pending_outputs can be cleared only after the new version is
  // committed so that other threads can recognize this file as a
  // valid one.
  // pending_outputs_.erase(meta.number);

  // note that if file_size is zero, the file has been deleted and
  // should not be added to the manifest.
  int level = 0;
  if (s.ok() && meta.fd.getfilesize() > 0) {
    const slice min_user_key = meta.smallest.user_key();
    const slice max_user_key = meta.largest.user_key();
    // if we have more than 1 background thread, then we cannot
    // insert files directly into higher levels because some other
    // threads could be concurrently producing compacted files for
    // that key range.
    if (base != nullptr && options_.max_background_compactions <= 1 &&
        cfd->options()->compaction_style == kcompactionstylelevel) {
      level = base->picklevelformemtableoutput(min_user_key, max_user_key);
    }
    edit->addfile(level, meta.fd.getnumber(), meta.fd.getpathid(),
                  meta.fd.getfilesize(), meta.smallest, meta.largest,
                  meta.smallest_seqno, meta.largest_seqno);
  }

  internalstats::compactionstats stats(1);
  stats.micros = env_->nowmicros() - start_micros;
  stats.bytes_written = meta.fd.getfilesize();
  cfd->internal_stats()->addcompactionstats(level, stats);
  cfd->internal_stats()->addcfstats(
      internalstats::bytes_flushed, meta.fd.getfilesize());
  recordtick(stats_, compact_write_bytes, meta.fd.getfilesize());
  return s;
}

status dbimpl::flushmemtabletooutputfile(columnfamilydata* cfd,
                                         bool* madeprogress,
                                         deletionstate& deletion_state,
                                         logbuffer* log_buffer) {
  mutex_.assertheld();
  assert(cfd->imm()->size() != 0);
  assert(cfd->imm()->isflushpending());

  // save the contents of the earliest memtable as a new table
  uint64_t file_number;
  autovector<memtable*> mems;
  cfd->imm()->pickmemtablestoflush(&mems);
  if (mems.empty()) {
    logtobuffer(log_buffer, "[%s] nothing in memtable to flush",
                cfd->getname().c_str());
    return status::ok();
  }

  // record the logfile_number_ before we release the mutex
  // entries mems are (implicitly) sorted in ascending order by their created
  // time. we will use the first memtable's `edit` to keep the meta info for
  // this flush.
  memtable* m = mems[0];
  versionedit* edit = m->getedits();
  edit->setprevlognumber(0);
  // setlognumber(log_num) indicates logs with number smaller than log_num
  // will no longer be picked up for recovery.
  edit->setlognumber(mems.back()->getnextlognumber());
  edit->setcolumnfamily(cfd->getid());

  // this will release and re-acquire the mutex.
  status s = writelevel0table(cfd, mems, edit, &file_number, log_buffer);

  if (s.ok() && shutting_down_.acquire_load() && cfd->isdropped()) {
    s = status::shutdowninprogress(
        "database shutdown or column family drop during flush");
  }

  if (!s.ok()) {
    cfd->imm()->rollbackmemtableflush(mems, file_number, &pending_outputs_);
  } else {
    // replace immutable memtable with the generated table
    s = cfd->imm()->installmemtableflushresults(
        cfd, mems, versions_.get(), &mutex_, options_.info_log.get(),
        file_number, &pending_outputs_, &deletion_state.memtables_to_free,
        db_directory_.get(), log_buffer);
  }

  if (s.ok()) {
    installsuperversion(cfd, deletion_state);
    if (madeprogress) {
      *madeprogress = 1;
    }
    version::levelsummarystorage tmp;
    logtobuffer(log_buffer, "[%s] level summary: %s\n", cfd->getname().c_str(),
                cfd->current()->levelsummary(&tmp));

    if (disable_delete_obsolete_files_ == 0) {
      // add to deletion state
      while (alive_log_files_.size() &&
             alive_log_files_.begin()->number < versions_->minlognumber()) {
        const auto& earliest = *alive_log_files_.begin();
        deletion_state.log_delete_files.push_back(earliest.number);
        total_log_size_ -= earliest.size;
        alive_log_files_.pop_front();
      }
    }
  }

  if (!s.ok() && !s.isshutdowninprogress() && options_.paranoid_checks &&
      bg_error_.ok()) {
    // if a bad error happened (not shutdowninprogress) and paranoid_checks is
    // true, mark db read-only
    bg_error_ = s;
  }
  recordflushiostats();
  return s;
}

status dbimpl::compactrange(columnfamilyhandle* column_family,
                            const slice* begin, const slice* end,
                            bool reduce_level, int target_level,
                            uint32_t target_path_id) {
  if (target_path_id >= options_.db_paths.size()) {
    return status::invalidargument("invalid target path id");
  }

  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  auto cfd = cfh->cfd();

  status s = flushmemtable(cfd, flushoptions());
  if (!s.ok()) {
    logflush(options_.info_log);
    return s;
  }

  int max_level_with_files = 0;
  {
    mutexlock l(&mutex_);
    version* base = cfd->current();
    for (int level = 1; level < cfd->numberlevels(); level++) {
      if (base->overlapinlevel(level, begin, end)) {
        max_level_with_files = level;
      }
    }
  }
  for (int level = 0; level <= max_level_with_files; level++) {
    // in case the compaction is unversal or if we're compacting the
    // bottom-most level, the output level will be the same as input one.
    // level 0 can never be the bottommost level (i.e. if all files are in level
    // 0, we will compact to level 1)
    if (cfd->options()->compaction_style == kcompactionstyleuniversal ||
        cfd->options()->compaction_style == kcompactionstylefifo ||
        (level == max_level_with_files && level > 0)) {
      s = runmanualcompaction(cfd, level, level, target_path_id, begin, end);
    } else {
      s = runmanualcompaction(cfd, level, level + 1, target_path_id, begin,
                              end);
    }
    if (!s.ok()) {
      logflush(options_.info_log);
      return s;
    }
  }

  if (reduce_level) {
    s = refitlevel(cfd, max_level_with_files, target_level);
  }
  logflush(options_.info_log);

  {
    mutexlock l(&mutex_);
    // an automatic compaction that has been scheduled might have been
    // preempted by the manual compactions. need to schedule it back.
    maybescheduleflushorcompaction();
  }

  return s;
}

// return the same level if it cannot be moved
int dbimpl::findminimumemptylevelfitting(columnfamilydata* cfd, int level) {
  mutex_.assertheld();
  version* current = cfd->current();
  int minimum_level = level;
  for (int i = level - 1; i > 0; --i) {
    // stop if level i is not empty
    if (current->numlevelfiles(i) > 0) break;
    // stop if level i is too small (cannot fit the level files)
    if (cfd->compaction_picker()->maxbytesforlevel(i) <
        current->numlevelbytes(level)) {
      break;
    }

    minimum_level = i;
  }
  return minimum_level;
}

status dbimpl::refitlevel(columnfamilydata* cfd, int level, int target_level) {
  assert(level < cfd->numberlevels());

  superversion* superversion_to_free = nullptr;
  superversion* new_superversion = new superversion();

  mutex_.lock();

  // only allow one thread refitting
  if (refitting_level_) {
    mutex_.unlock();
    log(options_.info_log, "refitlevel: another thread is refitting");
    delete new_superversion;
    return status::notsupported("another thread is refitting");
  }
  refitting_level_ = true;

  // wait for all background threads to stop
  bg_work_gate_closed_ = true;
  while (bg_compaction_scheduled_ > 0 || bg_flush_scheduled_) {
    log(options_.info_log,
        "refitlevel: waiting for background threads to stop: %d %d",
        bg_compaction_scheduled_, bg_flush_scheduled_);
    bg_cv_.wait();
  }

  // move to a smaller level
  int to_level = target_level;
  if (target_level < 0) {
    to_level = findminimumemptylevelfitting(cfd, level);
  }

  assert(to_level <= level);

  status status;
  if (to_level < level) {
    log(options_.info_log, "[%s] before refitting:\n%s", cfd->getname().c_str(),
        cfd->current()->debugstring().data());

    versionedit edit;
    edit.setcolumnfamily(cfd->getid());
    for (const auto& f : cfd->current()->files_[level]) {
      edit.deletefile(level, f->fd.getnumber());
      edit.addfile(to_level, f->fd.getnumber(), f->fd.getpathid(),
                   f->fd.getfilesize(), f->smallest, f->largest,
                   f->smallest_seqno, f->largest_seqno);
    }
    log(options_.info_log, "[%s] apply version edit:\n%s",
        cfd->getname().c_str(), edit.debugstring().data());

    status = versions_->logandapply(cfd, &edit, &mutex_, db_directory_.get());
    superversion_to_free = cfd->installsuperversion(new_superversion, &mutex_);
    new_superversion = nullptr;

    log(options_.info_log, "[%s] logandapply: %s\n", cfd->getname().c_str(),
        status.tostring().data());

    if (status.ok()) {
      log(options_.info_log, "[%s] after refitting:\n%s",
          cfd->getname().c_str(), cfd->current()->debugstring().data());
    }
  }

  refitting_level_ = false;
  bg_work_gate_closed_ = false;

  mutex_.unlock();
  delete superversion_to_free;
  delete new_superversion;
  return status;
}

int dbimpl::numberlevels(columnfamilyhandle* column_family) {
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  return cfh->cfd()->numberlevels();
}

int dbimpl::maxmemcompactionlevel(columnfamilyhandle* column_family) {
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  return cfh->cfd()->options()->max_mem_compaction_level;
}

int dbimpl::level0stopwritetrigger(columnfamilyhandle* column_family) {
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  return cfh->cfd()->options()->level0_stop_writes_trigger;
}

status dbimpl::flush(const flushoptions& options,
                     columnfamilyhandle* column_family) {
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  return flushmemtable(cfh->cfd(), options);
}

sequencenumber dbimpl::getlatestsequencenumber() const {
  return versions_->lastsequence();
}

status dbimpl::runmanualcompaction(columnfamilydata* cfd, int input_level,
                                   int output_level, uint32_t output_path_id,
                                   const slice* begin, const slice* end) {
  assert(input_level >= 0);

  internalkey begin_storage, end_storage;

  manualcompaction manual;
  manual.cfd = cfd;
  manual.input_level = input_level;
  manual.output_level = output_level;
  manual.output_path_id = output_path_id;
  manual.done = false;
  manual.in_progress = false;
  // for universal compaction, we enforce every manual compaction to compact
  // all files.
  if (begin == nullptr ||
      cfd->options()->compaction_style == kcompactionstyleuniversal ||
      cfd->options()->compaction_style == kcompactionstylefifo) {
    manual.begin = nullptr;
  } else {
    begin_storage = internalkey(*begin, kmaxsequencenumber, kvaluetypeforseek);
    manual.begin = &begin_storage;
  }
  if (end == nullptr ||
      cfd->options()->compaction_style == kcompactionstyleuniversal ||
      cfd->options()->compaction_style == kcompactionstylefifo) {
    manual.end = nullptr;
  } else {
    end_storage = internalkey(*end, 0, static_cast<valuetype>(0));
    manual.end = &end_storage;
  }

  mutexlock l(&mutex_);

  // when a manual compaction arrives, temporarily disable scheduling of
  // non-manual compactions and wait until the number of scheduled compaction
  // jobs drops to zero. this is needed to ensure that this manual compaction
  // can compact any range of keys/files.
  //
  // bg_manual_only_ is non-zero when at least one thread is inside
  // runmanualcompaction(), i.e. during that time no other compaction will
  // get scheduled (see maybescheduleflushorcompaction).
  //
  // note that the following loop doesn't stop more that one thread calling
  // runmanualcompaction() from getting to the second while loop below.
  // however, only one of them will actually schedule compaction, while
  // others will wait on a condition variable until it completes.

  ++bg_manual_only_;
  while (bg_compaction_scheduled_ > 0) {
    log(options_.info_log,
        "[%s] manual compaction waiting for all other scheduled background "
        "compactions to finish",
        cfd->getname().c_str());
    bg_cv_.wait();
  }

  log(options_.info_log, "[%s] manual compaction starting",
      cfd->getname().c_str());

  while (!manual.done && !shutting_down_.acquire_load() && bg_error_.ok()) {
    assert(bg_manual_only_ > 0);
    if (manual_compaction_ != nullptr) {
      // running either this or some other manual compaction
      bg_cv_.wait();
    } else {
      manual_compaction_ = &manual;
      assert(bg_compaction_scheduled_ == 0);
      bg_compaction_scheduled_++;
      env_->schedule(&dbimpl::bgworkcompaction, this, env::priority::low);
    }
  }

  assert(!manual.in_progress);
  assert(bg_manual_only_ > 0);
  --bg_manual_only_;
  return manual.status;
}

status dbimpl::flushmemtable(columnfamilydata* cfd,
                             const flushoptions& options) {
  writer w(&mutex_);
  w.batch = nullptr;
  w.sync = false;
  w.disablewal = false;
  w.in_batch_group = false;
  w.done = false;
  w.timeout_hint_us = knotimeout;

  status s;
  {
    writecontext context;
    mutexlock guard_lock(&mutex_);
    s = beginwrite(&w, 0);
    assert(s.ok() && !w.done);  // no timeout and nobody should do our job

    // setnewmemtableandnewlogfile() will release and reacquire mutex
    // during execution
    s = setnewmemtableandnewlogfile(cfd, &context);
    cfd->imm()->flushrequested();
    maybescheduleflushorcompaction();

    assert(!writers_.empty());
    assert(writers_.front() == &w);
    endwrite(&w, &w, s);
  }


  if (s.ok() && options.wait) {
    // wait until the compaction completes
    s = waitforflushmemtable(cfd);
  }
  return s;
}

status dbimpl::waitforflushmemtable(columnfamilydata* cfd) {
  status s;
  // wait until the compaction completes
  mutexlock l(&mutex_);
  while (cfd->imm()->size() > 0 && bg_error_.ok()) {
    bg_cv_.wait();
  }
  if (!bg_error_.ok()) {
    s = bg_error_;
  }
  return s;
}

void dbimpl::maybescheduleflushorcompaction() {
  mutex_.assertheld();
  bg_schedule_needed_ = false;
  if (bg_work_gate_closed_) {
    // gate closed for backgrond work
  } else if (shutting_down_.acquire_load()) {
    // db is being deleted; no more background compactions
  } else {
    bool is_flush_pending = false;
    // no need to refcount since we're under a mutex
    for (auto cfd : *versions_->getcolumnfamilyset()) {
      if (cfd->imm()->isflushpending()) {
        is_flush_pending = true;
      }
    }
    if (is_flush_pending) {
      // memtable flush needed
      if (bg_flush_scheduled_ < options_.max_background_flushes) {
        bg_flush_scheduled_++;
        env_->schedule(&dbimpl::bgworkflush, this, env::priority::high);
      } else if (options_.max_background_flushes > 0) {
        bg_schedule_needed_ = true;
      }
    }
    bool is_compaction_needed = false;
    // no need to refcount since we're under a mutex
    for (auto cfd : *versions_->getcolumnfamilyset()) {
      if (cfd->current()->needscompaction()) {
        is_compaction_needed = true;
        break;
      }
    }

    // schedule bgworkcompaction if there's a compaction pending (or a memtable
    // flush, but the high pool is not enabled)
    // do it only if max_background_compactions hasn't been reached and
    // bg_manual_only_ == 0
    if (!bg_manual_only_ &&
        (is_compaction_needed ||
         (is_flush_pending && options_.max_background_flushes == 0))) {
      if (bg_compaction_scheduled_ < options_.max_background_compactions) {
        bg_compaction_scheduled_++;
        env_->schedule(&dbimpl::bgworkcompaction, this, env::priority::low);
      } else {
        bg_schedule_needed_ = true;
      }
    }
  }
}

void dbimpl::recordflushiostats() {
  recordtick(stats_, flush_write_bytes, iostats(bytes_written));
  iostats_reset(bytes_written);
}

void dbimpl::recordcompactioniostats() {
  recordtick(stats_, compact_read_bytes, iostats(bytes_read));
  iostats_reset(bytes_read);
  recordtick(stats_, compact_write_bytes, iostats(bytes_written));
  iostats_reset(bytes_written);
}

void dbimpl::bgworkflush(void* db) {
  iostats_set_thread_pool_id(env::priority::high);
  reinterpret_cast<dbimpl*>(db)->backgroundcallflush();
}

void dbimpl::bgworkcompaction(void* db) {
  iostats_set_thread_pool_id(env::priority::low);
  reinterpret_cast<dbimpl*>(db)->backgroundcallcompaction();
}

status dbimpl::backgroundflush(bool* madeprogress,
                               deletionstate& deletion_state,
                               logbuffer* log_buffer) {
  mutex_.assertheld();
  // call_status is failure if at least one flush was a failure. even if
  // flushing one column family reports a failure, we will continue flushing
  // other column families. however, call_status will be a failure in that case.
  status call_status;
  // refcounting in iteration
  for (auto cfd : *versions_->getcolumnfamilyset()) {
    cfd->ref();
    status flush_status;
    while (flush_status.ok() && cfd->imm()->isflushpending()) {
      logtobuffer(
          log_buffer,
          "backgroundcallflush doing flushmemtabletooutputfile with column "
          "family [%s], flush slots available %d",
          cfd->getname().c_str(),
          options_.max_background_flushes - bg_flush_scheduled_);
      flush_status = flushmemtabletooutputfile(cfd, madeprogress,
                                               deletion_state, log_buffer);
    }
    if (call_status.ok() && !flush_status.ok()) {
      call_status = flush_status;
    }
    cfd->unref();
  }
  versions_->getcolumnfamilyset()->freedeadcolumnfamilies();
  return call_status;
}

void dbimpl::backgroundcallflush() {
  bool madeprogress = false;
  deletionstate deletion_state(true);
  assert(bg_flush_scheduled_);

  logbuffer log_buffer(infologlevel::info_level, options_.info_log.get());
  {
    mutexlock l(&mutex_);

    status s;
    if (!shutting_down_.acquire_load()) {
      s = backgroundflush(&madeprogress, deletion_state, &log_buffer);
      if (!s.ok()) {
        // wait a little bit before retrying background compaction in
        // case this is an environmental problem and we do not want to
        // chew up resources for failed compactions for the duration of
        // the problem.
        uint64_t error_cnt =
          default_cf_internal_stats_->bumpandgetbackgrounderrorcount();
        bg_cv_.signalall();  // in case a waiter can proceed despite the error
        mutex_.unlock();
        log(options_.info_log,
            "waiting after background flush error: %s"
            "accumulated background error counts: %" priu64,
            s.tostring().c_str(), error_cnt);
        log_buffer.flushbuffertolog();
        logflush(options_.info_log);
        env_->sleepformicroseconds(1000000);
        mutex_.lock();
      }
    }

    // if !s.ok(), this means that flush failed. in that case, we want
    // to delete all obsolete files and we force findobsoletefiles()
    findobsoletefiles(deletion_state, !s.ok());
    // delete unnecessary files if any, this is done outside the mutex
    if (deletion_state.havesomethingtodelete() || !log_buffer.isempty()) {
      mutex_.unlock();
      // have to flush the info logs before bg_flush_scheduled_--
      // because if bg_flush_scheduled_ becomes 0 and the lock is
      // released, the deconstructor of db can kick in and destroy all the
      // states of db so info_log might not be available after that point.
      // it also applies to access other states that db owns.
      log_buffer.flushbuffertolog();
      if (deletion_state.havesomethingtodelete()) {
        purgeobsoletefiles(deletion_state);
      }
      mutex_.lock();
    }

    bg_flush_scheduled_--;
    // any time the mutex is released after finding the work to do, another
    // thread might execute maybescheduleflushorcompaction(). it is possible
    // that there is a pending job but it is not scheduled because of the
    // max thread limit.
    if (madeprogress || bg_schedule_needed_) {
      maybescheduleflushorcompaction();
    }
    recordflushiostats();
    bg_cv_.signalall();
    // important: there should be no code after calling signalall. this call may
    // signal the db destructor that it's ok to proceed with destruction. in
    // that case, all db variables will be dealloacated and referencing them
    // will cause trouble.
  }
}

void dbimpl::backgroundcallcompaction() {
  bool madeprogress = false;
  deletionstate deletion_state(true);

  maybedumpstats();
  logbuffer log_buffer(infologlevel::info_level, options_.info_log.get());
  {
    mutexlock l(&mutex_);
    assert(bg_compaction_scheduled_);
    status s;
    if (!shutting_down_.acquire_load()) {
      s = backgroundcompaction(&madeprogress, deletion_state, &log_buffer);
      if (!s.ok()) {
        // wait a little bit before retrying background compaction in
        // case this is an environmental problem and we do not want to
        // chew up resources for failed compactions for the duration of
        // the problem.
        uint64_t error_cnt =
          default_cf_internal_stats_->bumpandgetbackgrounderrorcount();
        bg_cv_.signalall();  // in case a waiter can proceed despite the error
        mutex_.unlock();
        log_buffer.flushbuffertolog();
        log(options_.info_log,
            "waiting after background compaction error: %s, "
            "accumulated background error counts: %" priu64,
            s.tostring().c_str(), error_cnt);
        logflush(options_.info_log);
        env_->sleepformicroseconds(1000000);
        mutex_.lock();
      }
    }

    // if !s.ok(), this means that compaction failed. in that case, we want
    // to delete all obsolete files we might have created and we force
    // findobsoletefiles(). this is because deletion_state does not catch
    // all created files if compaction failed.
    findobsoletefiles(deletion_state, !s.ok());

    // delete unnecessary files if any, this is done outside the mutex
    if (deletion_state.havesomethingtodelete() || !log_buffer.isempty()) {
      mutex_.unlock();
      // have to flush the info logs before bg_compaction_scheduled_--
      // because if bg_flush_scheduled_ becomes 0 and the lock is
      // released, the deconstructor of db can kick in and destroy all the
      // states of db so info_log might not be available after that point.
      // it also applies to access other states that db owns.
      log_buffer.flushbuffertolog();
      if (deletion_state.havesomethingtodelete()) {
        purgeobsoletefiles(deletion_state);
      }
      mutex_.lock();
    }

    bg_compaction_scheduled_--;

    versions_->getcolumnfamilyset()->freedeadcolumnfamilies();

    // previous compaction may have produced too many files in a level,
    // so reschedule another compaction if we made progress in the
    // last compaction.
    //
    // also, any time the mutex is released after finding the work to do,
    // another thread might execute maybescheduleflushorcompaction(). it is
    // possible  that there is a pending job but it is not scheduled because of
    // the max thread limit.
    if (madeprogress || bg_schedule_needed_) {
      maybescheduleflushorcompaction();
    }
    if (madeprogress || bg_compaction_scheduled_ == 0 || bg_manual_only_ > 0) {
      // signal if
      // * madeprogress -- need to wakeup makeroomforwrite
      // * bg_compaction_scheduled_ == 0 -- need to wakeup ~dbimpl
      // * bg_manual_only_ > 0 -- need to wakeup runmanualcompaction
      // if none of this is true, there is no need to signal since nobody is
      // waiting for it
      bg_cv_.signalall();
    }
    // important: there should be no code after calling signalall. this call may
    // signal the db destructor that it's ok to proceed with destruction. in
    // that case, all db variables will be dealloacated and referencing them
    // will cause trouble.
  }
}

status dbimpl::backgroundcompaction(bool* madeprogress,
                                    deletionstate& deletion_state,
                                    logbuffer* log_buffer) {
  *madeprogress = false;
  mutex_.assertheld();

  bool is_manual = (manual_compaction_ != nullptr) &&
                   (manual_compaction_->in_progress == false);

  if (is_manual) {
    // another thread cannot pick up the same work
    manual_compaction_->in_progress = true;
  } else if (manual_compaction_ != nullptr) {
    // there should be no automatic compactions running when manual compaction
    // is running
    return status::ok();
  }

  // flush preempts compaction
  status flush_stat;
  for (auto cfd : *versions_->getcolumnfamilyset()) {
    while (cfd->imm()->isflushpending()) {
      logtobuffer(
          log_buffer,
          "backgroundcompaction doing flushmemtabletooutputfile, "
          "compaction slots available %d",
          options_.max_background_compactions - bg_compaction_scheduled_);
      cfd->ref();
      flush_stat = flushmemtabletooutputfile(cfd, madeprogress, deletion_state,
                                             log_buffer);
      cfd->unref();
      if (!flush_stat.ok()) {
        if (is_manual) {
          manual_compaction_->status = flush_stat;
          manual_compaction_->done = true;
          manual_compaction_->in_progress = false;
          manual_compaction_ = nullptr;
        }
        return flush_stat;
      }
    }
  }

  unique_ptr<compaction> c;
  internalkey manual_end_storage;
  internalkey* manual_end = &manual_end_storage;
  if (is_manual) {
    manualcompaction* m = manual_compaction_;
    assert(m->in_progress);
    c.reset(m->cfd->compactrange(m->input_level, m->output_level,
                                 m->output_path_id, m->begin, m->end,
                                 &manual_end));
    if (!c) {
      m->done = true;
    }
    logtobuffer(log_buffer,
                "[%s] manual compaction from level-%d to level-%d from %s .. "
                "%s; will stop at %s\n",
                m->cfd->getname().c_str(), m->input_level, m->output_level,
                (m->begin ? m->begin->debugstring().c_str() : "(begin)"),
                (m->end ? m->end->debugstring().c_str() : "(end)"),
                ((m->done || manual_end == nullptr)
                     ? "(end)"
                     : manual_end->debugstring().c_str()));
  } else {
    // no need to refcount in iteration since it's always under a mutex
    for (auto cfd : *versions_->getcolumnfamilyset()) {
      if (!cfd->options()->disable_auto_compactions) {
        c.reset(cfd->pickcompaction(log_buffer));
        if (c != nullptr) {
          // update statistics
          measuretime(stats_, num_files_in_single_compaction,
                      c->inputs(0)->size());
          break;
        }
      }
    }
  }

  status status;
  if (!c) {
    // nothing to do
    logtobuffer(log_buffer, "compaction nothing to do");
  } else if (c->isdeletioncompaction()) {
    // todo(icanadi) do we want to honor snapshots here? i.e. not delete old
    // file if there is alive snapshot pointing to it
    assert(c->num_input_files(1) == 0);
    assert(c->level() == 0);
    assert(c->column_family_data()->options()->compaction_style ==
           kcompactionstylefifo);
    for (const auto& f : *c->inputs(0)) {
      c->edit()->deletefile(c->level(), f->fd.getnumber());
    }
    status = versions_->logandapply(c->column_family_data(), c->edit(), &mutex_,
                                    db_directory_.get());
    installsuperversion(c->column_family_data(), deletion_state);
    logtobuffer(log_buffer, "[%s] deleted %d files\n",
                c->column_family_data()->getname().c_str(),
                c->num_input_files(0));
    c->releasecompactionfiles(status);
    *madeprogress = true;
  } else if (!is_manual && c->istrivialmove()) {
    // move file to next level
    assert(c->num_input_files(0) == 1);
    filemetadata* f = c->input(0, 0);
    c->edit()->deletefile(c->level(), f->fd.getnumber());
    c->edit()->addfile(c->level() + 1, f->fd.getnumber(), f->fd.getpathid(),
                       f->fd.getfilesize(), f->smallest, f->largest,
                       f->smallest_seqno, f->largest_seqno);
    status = versions_->logandapply(c->column_family_data(), c->edit(), &mutex_,
                                    db_directory_.get());
    installsuperversion(c->column_family_data(), deletion_state);

    version::levelsummarystorage tmp;
    logtobuffer(
        log_buffer, "[%s] moved #%lld to level-%d %lld bytes %s: %s\n",
        c->column_family_data()->getname().c_str(),
        static_cast<unsigned long long>(f->fd.getnumber()), c->level() + 1,
        static_cast<unsigned long long>(f->fd.getfilesize()),
        status.tostring().c_str(), c->input_version()->levelsummary(&tmp));
    c->releasecompactionfiles(status);
    *madeprogress = true;
  } else {
    maybescheduleflushorcompaction(); // do more compaction work in parallel.
    compactionstate* compact = new compactionstate(c.get());
    status = docompactionwork(compact, deletion_state, log_buffer);
    cleanupcompaction(compact, status);
    c->releasecompactionfiles(status);
    c->releaseinputs();
    *madeprogress = true;
  }
  c.reset();

  if (status.ok()) {
    // done
  } else if (status.isshutdowninprogress()) {
    // ignore compaction errors found during shutting down
  } else {
    log(infologlevel::warn_level, options_.info_log, "compaction error: %s",
        status.tostring().c_str());
    if (options_.paranoid_checks && bg_error_.ok()) {
      bg_error_ = status;
    }
  }

  if (is_manual) {
    manualcompaction* m = manual_compaction_;
    if (!status.ok()) {
      m->status = status;
      m->done = true;
    }
    // for universal compaction:
    //   because universal compaction always happens at level 0, so one
    //   compaction will pick up all overlapped files. no files will be
    //   filtered out due to size limit and left for a successive compaction.
    //   so we can safely conclude the current compaction.
    //
    //   also note that, if we don't stop here, then the current compaction
    //   writes a new file back to level 0, which will be used in successive
    //   compaction. hence the manual compaction will never finish.
    //
    // stop the compaction if manual_end points to nullptr -- this means
    // that we compacted the whole range. manual_end should always point
    // to nullptr in case of universal compaction
    if (manual_end == nullptr) {
      m->done = true;
    }
    if (!m->done) {
      // we only compacted part of the requested range.  update *m
      // to the range that is left to be compacted.
      // universal and fifo compactions should always compact the whole range
      assert(m->cfd->options()->compaction_style != kcompactionstyleuniversal);
      assert(m->cfd->options()->compaction_style != kcompactionstylefifo);
      m->tmp_storage = *manual_end;
      m->begin = &m->tmp_storage;
    }
    m->in_progress = false; // not being processed anymore
    manual_compaction_ = nullptr;
  }
  return status;
}

void dbimpl::cleanupcompaction(compactionstate* compact, status status) {
  mutex_.assertheld();
  if (compact->builder != nullptr) {
    // may happen if we get a shutdown call in the middle of compaction
    compact->builder->abandon();
    compact->builder.reset();
  } else {
    assert(compact->outfile == nullptr);
  }
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    const compactionstate::output& out = compact->outputs[i];
    pending_outputs_.erase(out.number);

    // if this file was inserted into the table cache then remove
    // them here because this compaction was not committed.
    if (!status.ok()) {
      tablecache::evict(table_cache_.get(), out.number);
    }
  }
  delete compact;
}

// allocate the file numbers for the output file. we allocate as
// many output file numbers as there are files in level+1 (at least one)
// insert them into pending_outputs so that they do not get deleted.
void dbimpl::allocatecompactionoutputfilenumbers(compactionstate* compact) {
  mutex_.assertheld();
  assert(compact != nullptr);
  assert(compact->builder == nullptr);
  int filesneeded = compact->compaction->num_input_files(1);
  for (int i = 0; i < std::max(filesneeded, 1); i++) {
    uint64_t file_number = versions_->newfilenumber();
    pending_outputs_[file_number] = compact->compaction->getoutputpathid();
    compact->allocated_file_numbers.push_back(file_number);
  }
}

// frees up unused file number.
void dbimpl::releasecompactionunusedfilenumbers(compactionstate* compact) {
  mutex_.assertheld();
  for (const auto file_number : compact->allocated_file_numbers) {
    pending_outputs_.erase(file_number);
  }
}

status dbimpl::opencompactionoutputfile(compactionstate* compact) {
  assert(compact != nullptr);
  assert(compact->builder == nullptr);
  uint64_t file_number;
  // if we have not yet exhausted the pre-allocated file numbers,
  // then use the one from the front. otherwise, we have to acquire
  // the heavyweight lock and allocate a new file number.
  if (!compact->allocated_file_numbers.empty()) {
    file_number = compact->allocated_file_numbers.front();
    compact->allocated_file_numbers.pop_front();
  } else {
    mutex_.lock();
    file_number = versions_->newfilenumber();
    pending_outputs_[file_number] = compact->compaction->getoutputpathid();
    mutex_.unlock();
  }
  compactionstate::output out;
  out.number = file_number;
  out.path_id = compact->compaction->getoutputpathid();
  out.smallest.clear();
  out.largest.clear();
  out.smallest_seqno = out.largest_seqno = 0;
  compact->outputs.push_back(out);

  // make the output file
  std::string fname = tablefilename(options_.db_paths, file_number,
                                    compact->compaction->getoutputpathid());
  status s = env_->newwritablefile(fname, &compact->outfile, storage_options_);

  if (s.ok()) {
    compact->outfile->setiopriority(env::io_low);
    compact->outfile->setpreallocationblocksize(
        compact->compaction->outputfilepreallocationsize());

    columnfamilydata* cfd = compact->compaction->column_family_data();
    compact->builder.reset(newtablebuilder(
        *cfd->options(), cfd->internal_comparator(), compact->outfile.get(),
        compact->compaction->outputcompressiontype()));
  }
  logflush(options_.info_log);
  return s;
}

status dbimpl::finishcompactionoutputfile(compactionstate* compact,
                                          iterator* input) {
  assert(compact != nullptr);
  assert(compact->outfile);
  assert(compact->builder != nullptr);

  const uint64_t output_number = compact->current_output()->number;
  const uint32_t output_path_id = compact->current_output()->path_id;
  assert(output_number != 0);

  // check for iterator errors
  status s = input->status();
  const uint64_t current_entries = compact->builder->numentries();
  if (s.ok()) {
    s = compact->builder->finish();
  } else {
    compact->builder->abandon();
  }
  const uint64_t current_bytes = compact->builder->filesize();
  compact->current_output()->file_size = current_bytes;
  compact->total_bytes += current_bytes;
  compact->builder.reset();

  // finish and check for file errors
  if (s.ok() && !options_.disabledatasync) {
    if (options_.use_fsync) {
      stopwatch sw(env_, stats_, compaction_outfile_sync_micros);
      s = compact->outfile->fsync();
    } else {
      stopwatch sw(env_, stats_, compaction_outfile_sync_micros);
      s = compact->outfile->sync();
    }
  }
  if (s.ok()) {
    s = compact->outfile->close();
  }
  compact->outfile.reset();

  if (s.ok() && current_entries > 0) {
    // verify that the table is usable
    columnfamilydata* cfd = compact->compaction->column_family_data();
    filedescriptor fd(output_number, output_path_id, current_bytes);
    iterator* iter = cfd->table_cache()->newiterator(
        readoptions(), storage_options_, cfd->internal_comparator(), fd);
    s = iter->status();
    delete iter;
    if (s.ok()) {
      log(options_.info_log, "[%s] generated table #%" priu64 ": %" priu64
                             " keys, %" priu64 " bytes",
          cfd->getname().c_str(), output_number, current_entries,
          current_bytes);
    }
  }
  return s;
}


status dbimpl::installcompactionresults(compactionstate* compact,
                                        logbuffer* log_buffer) {
  mutex_.assertheld();

  // paranoia: verify that the files that we started with
  // still exist in the current version and in the same original level.
  // this ensures that a concurrent compaction did not erroneously
  // pick the same files to compact.
  if (!versions_->verifycompactionfileconsistency(compact->compaction)) {
    log(options_.info_log, "[%s] compaction %d@%d + %d@%d files aborted",
        compact->compaction->column_family_data()->getname().c_str(),
        compact->compaction->num_input_files(0), compact->compaction->level(),
        compact->compaction->num_input_files(1),
        compact->compaction->output_level());
    return status::corruption("compaction input files inconsistent");
  }

  logtobuffer(log_buffer, "[%s] compacted %d@%d + %d@%d files => %lld bytes",
              compact->compaction->column_family_data()->getname().c_str(),
              compact->compaction->num_input_files(0),
              compact->compaction->level(),
              compact->compaction->num_input_files(1),
              compact->compaction->output_level(),
              static_cast<long long>(compact->total_bytes));

  // add compaction outputs
  compact->compaction->addinputdeletions(compact->compaction->edit());
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    const compactionstate::output& out = compact->outputs[i];
    compact->compaction->edit()->addfile(compact->compaction->output_level(),
                                         out.number, out.path_id, out.file_size,
                                         out.smallest, out.largest,
                                         out.smallest_seqno, out.largest_seqno);
  }
  return versions_->logandapply(compact->compaction->column_family_data(),
                                compact->compaction->edit(), &mutex_,
                                db_directory_.get());
}

// given a sequence number, return the sequence number of the
// earliest snapshot that this sequence number is visible in.
// the snapshots themselves are arranged in ascending order of
// sequence numbers.
// employ a sequential search because the total number of
// snapshots are typically small.
inline sequencenumber dbimpl::findearliestvisiblesnapshot(
  sequencenumber in, std::vector<sequencenumber>& snapshots,
  sequencenumber* prev_snapshot) {
  sequencenumber prev __attribute__((unused)) = 0;
  for (const auto cur : snapshots) {
    assert(prev <= cur);
    if (cur >= in) {
      *prev_snapshot = prev;
      return cur;
    }
    prev = cur; // assignment
    assert(prev);
  }
  log(options_.info_log,
      "looking for seqid %" priu64 " but maxseqid is %" priu64 "", in,
      snapshots[snapshots.size() - 1]);
  assert(0);
  return 0;
}

uint64_t dbimpl::callflushduringcompaction(columnfamilydata* cfd,
                                           deletionstate& deletion_state,
                                           logbuffer* log_buffer) {
  if (options_.max_background_flushes > 0) {
    // flush thread will take care of this
    return 0;
  }
  if (cfd->imm()->imm_flush_needed.nobarrier_load() != nullptr) {
    const uint64_t imm_start = env_->nowmicros();
    mutex_.lock();
    if (cfd->imm()->isflushpending()) {
      cfd->ref();
      flushmemtabletooutputfile(cfd, nullptr, deletion_state, log_buffer);
      cfd->unref();
      bg_cv_.signalall();  // wakeup makeroomforwrite() if necessary
    }
    mutex_.unlock();
    log_buffer->flushbuffertolog();
    return env_->nowmicros() - imm_start;
  }
  return 0;
}

status dbimpl::processkeyvaluecompaction(
    bool is_snapshot_supported,
    sequencenumber visible_at_tip,
    sequencenumber earliest_snapshot,
    sequencenumber latest_snapshot,
    deletionstate& deletion_state,
    bool bottommost_level,
    int64_t& imm_micros,
    iterator* input,
    compactionstate* compact,
    bool is_compaction_v2,
    logbuffer* log_buffer) {
  size_t combined_idx = 0;
  status status;
  std::string compaction_filter_value;
  parsedinternalkey ikey;
  iterkey current_user_key;
  bool has_current_user_key = false;
  iterkey delete_key;
  sequencenumber last_sequence_for_key __attribute__((unused)) =
    kmaxsequencenumber;
  sequencenumber visible_in_snapshot = kmaxsequencenumber;
  columnfamilydata* cfd = compact->compaction->column_family_data();
  mergehelper merge(
      cfd->user_comparator(), cfd->options()->merge_operator.get(),
      options_.info_log.get(), cfd->options()->min_partial_merge_operands,
      false /* internal key corruption is expected */);
  auto compaction_filter = cfd->options()->compaction_filter;
  std::unique_ptr<compactionfilter> compaction_filter_from_factory = nullptr;
  if (!compaction_filter) {
    auto context = compact->getfiltercontextv1();
    compaction_filter_from_factory =
        cfd->options()->compaction_filter_factory->createcompactionfilter(
            context);
    compaction_filter = compaction_filter_from_factory.get();
  }

  int64_t key_drop_user = 0;
  int64_t key_drop_newer_entry = 0;
  int64_t key_drop_obsolete = 0;
  int64_t loop_cnt = 0;
  while (input->valid() && !shutting_down_.acquire_load() &&
         !cfd->isdropped()) {
    if (++loop_cnt > 1000) {
      if (key_drop_user > 0) {
        recordtick(stats_, compaction_key_drop_user, key_drop_user);
        key_drop_user = 0;
      }
      if (key_drop_newer_entry > 0) {
        recordtick(stats_, compaction_key_drop_newer_entry,
                   key_drop_newer_entry);
        key_drop_newer_entry = 0;
      }
      if (key_drop_obsolete > 0) {
        recordtick(stats_, compaction_key_drop_obsolete, key_drop_obsolete);
        key_drop_obsolete = 0;
      }
      recordcompactioniostats();
      loop_cnt = 0;
    }
    // flush preempts compaction
    // todo(icanadi) this currently only checks if flush is necessary on
    // compacting column family. we should also check if flush is necessary on
    // other column families, too
    imm_micros += callflushduringcompaction(cfd, deletion_state, log_buffer);

    slice key;
    slice value;
    // if is_compaction_v2 is on, kv-pairs are reset to the prefix batch.
    // this prefix batch should contain results after calling
    // compaction_filter_v2.
    //
    // if is_compaction_v2 is off, this function will go through all the
    // kv-pairs in input.
    if (!is_compaction_v2) {
      key = input->key();
      value = input->value();
    } else {
      if (combined_idx >= compact->combined_key_buf_.size()) {
        break;
      }
      assert(combined_idx < compact->combined_key_buf_.size());
      key = compact->combined_key_buf_[combined_idx];
      value = compact->combined_value_buf_[combined_idx];

      ++combined_idx;
    }

    if (compact->compaction->shouldstopbefore(key) &&
        compact->builder != nullptr) {
      status = finishcompactionoutputfile(compact, input);
      if (!status.ok()) {
        break;
      }
    }

    // handle key/value, add to state, etc.
    bool drop = false;
    bool current_entry_is_merging = false;
    if (!parseinternalkey(key, &ikey)) {
      // do not hide error keys
      // todo: error key stays in db forever? figure out the intention/rationale
      // v10 error v8 : we cannot hide v8 even though it's pretty obvious.
      current_user_key.clear();
      has_current_user_key = false;
      last_sequence_for_key = kmaxsequencenumber;
      visible_in_snapshot = kmaxsequencenumber;
    } else {
      if (!has_current_user_key ||
          cfd->user_comparator()->compare(ikey.user_key,
                                          current_user_key.getkey()) != 0) {
        // first occurrence of this user key
        current_user_key.setkey(ikey.user_key);
        has_current_user_key = true;
        last_sequence_for_key = kmaxsequencenumber;
        visible_in_snapshot = kmaxsequencenumber;
        // apply the compaction filter to the first occurrence of the user key
        if (compaction_filter && !is_compaction_v2 &&
            ikey.type == ktypevalue &&
            (visible_at_tip || ikey.sequence > latest_snapshot)) {
          // if the user has specified a compaction filter and the sequence
          // number is greater than any external snapshot, then invoke the
          // filter.
          // if the return value of the compaction filter is true, replace
          // the entry with a delete marker.
          bool value_changed = false;
          compaction_filter_value.clear();
          bool to_delete = compaction_filter->filter(
              compact->compaction->level(), ikey.user_key, value,
              &compaction_filter_value, &value_changed);
          if (to_delete) {
            // make a copy of the original key and convert it to a delete
            delete_key.setinternalkey(extractuserkey(key), ikey.sequence,
                                      ktypedeletion);
            // anchor the key again
            key = delete_key.getkey();
            // needed because ikey is backed by key
            parseinternalkey(key, &ikey);
            // no value associated with delete
            value.clear();
            ++key_drop_user;
          } else if (value_changed) {
            value = compaction_filter_value;
          }
        }
      }

      // if there are no snapshots, then this kv affect visibility at tip.
      // otherwise, search though all existing snapshots to find
      // the earlist snapshot that is affected by this kv.
      sequencenumber prev_snapshot = 0; // 0 means no previous snapshot
      sequencenumber visible = visible_at_tip ? visible_at_tip :
        is_snapshot_supported ?  findearliestvisiblesnapshot(ikey.sequence,
                                  compact->existing_snapshots, &prev_snapshot)
                              : 0;

      if (visible_in_snapshot == visible) {
        // if the earliest snapshot is which this key is visible in
        // is the same as the visibily of a previous instance of the
        // same key, then this kv is not visible in any snapshot.
        // hidden by an newer entry for same user key
        // todo: why not > ?
        assert(last_sequence_for_key >= ikey.sequence);
        drop = true;    // (a)
        ++key_drop_newer_entry;
      } else if (ikey.type == ktypedeletion &&
          ikey.sequence <= earliest_snapshot &&
          compact->compaction->keynotexistsbeyondoutputlevel(ikey.user_key)) {
        // for this user key:
        // (1) there is no data in higher levels
        // (2) data in lower levels will have larger sequence numbers
        // (3) data in layers that are being compacted here and have
        //     smaller sequence numbers will be dropped in the next
        //     few iterations of this loop (by rule (a) above).
        // therefore this deletion marker is obsolete and can be dropped.
        drop = true;
        ++key_drop_obsolete;
      } else if (ikey.type == ktypemerge) {
        if (!merge.hasoperator()) {
          logtobuffer(log_buffer, "options::merge_operator is null.");
          status = status::invalidargument(
              "merge_operator is not properly initialized.");
          break;
        }
        // we know the merge type entry is not hidden, otherwise we would
        // have hit (a)
        // we encapsulate the merge related state machine in a different
        // object to minimize change to the existing flow. turn out this
        // logic could also be nicely re-used for memtable flush purge
        // optimization in buildtable.
        int steps = 0;
        merge.mergeuntil(input, prev_snapshot, bottommost_level,
            options_.statistics.get(), &steps);
        // skip the merge ops
        combined_idx = combined_idx - 1 + steps;

        current_entry_is_merging = true;
        if (merge.issuccess()) {
          // successfully found put/delete/(end-of-key-range) while merging
          // get the merge result
          key = merge.key();
          parseinternalkey(key, &ikey);
          value = merge.value();
        } else {
          // did not find a put/delete/(end-of-key-range) while merging
          // we now have some stack of merge operands to write out.
          // note: key,value, and ikey are now referring to old entries.
          //       these will be correctly set below.
          assert(!merge.keys().empty());
          assert(merge.keys().size() == merge.values().size());

          // hack to make sure last_sequence_for_key is correct
          parseinternalkey(merge.keys().front(), &ikey);
        }
      }

      last_sequence_for_key = ikey.sequence;
      visible_in_snapshot = visible;
    }

    if (!drop) {
      // we may write a single key (e.g.: for put/delete or successful merge).
      // or we may instead have to write a sequence/list of keys.
      // we have to write a sequence iff we have an unsuccessful merge
      bool has_merge_list = current_entry_is_merging && !merge.issuccess();
      const std::deque<std::string>* keys = nullptr;
      const std::deque<std::string>* values = nullptr;
      std::deque<std::string>::const_reverse_iterator key_iter;
      std::deque<std::string>::const_reverse_iterator value_iter;
      if (has_merge_list) {
        keys = &merge.keys();
        values = &merge.values();
        key_iter = keys->rbegin();    // the back (*rbegin()) is the first key
        value_iter = values->rbegin();

        key = slice(*key_iter);
        value = slice(*value_iter);
      }

      // if we have a list of keys to write, traverse the list.
      // if we have a single key to write, simply write that key.
      while (true) {
        // invariant: key,value,ikey will always be the next entry to write
        char* kptr = (char*)key.data();
        std::string kstr;

        // zeroing out the sequence number leads to better compression.
        // if this is the bottommost level (no files in lower levels)
        // and the earliest snapshot is larger than this seqno
        // then we can squash the seqno to zero.
        if (bottommost_level && ikey.sequence < earliest_snapshot &&
            ikey.type != ktypemerge) {
          assert(ikey.type != ktypedeletion);
          // make a copy because updating in place would cause problems
          // with the priority queue that is managing the input key iterator
          kstr.assign(key.data(), key.size());
          kptr = (char *)kstr.c_str();
          updateinternalkey(kptr, key.size(), (uint64_t)0, ikey.type);
        }

        slice newkey(kptr, key.size());
        assert((key.clear(), 1)); // we do not need 'key' anymore

        // open output file if necessary
        if (compact->builder == nullptr) {
          status = opencompactionoutputfile(compact);
          if (!status.ok()) {
            break;
          }
        }

        sequencenumber seqno = getinternalkeyseqno(newkey);
        if (compact->builder->numentries() == 0) {
          compact->current_output()->smallest.decodefrom(newkey);
          compact->current_output()->smallest_seqno = seqno;
        } else {
          compact->current_output()->smallest_seqno =
            std::min(compact->current_output()->smallest_seqno, seqno);
        }
        compact->current_output()->largest.decodefrom(newkey);
        compact->builder->add(newkey, value);
        compact->current_output()->largest_seqno =
          std::max(compact->current_output()->largest_seqno, seqno);

        // close output file if it is big enough
        if (compact->builder->filesize() >=
            compact->compaction->maxoutputfilesize()) {
          status = finishcompactionoutputfile(compact, input);
          if (!status.ok()) {
            break;
          }
        }

        // if we have a list of entries, move to next element
        // if we only had one entry, then break the loop.
        if (has_merge_list) {
          ++key_iter;
          ++value_iter;

          // if at end of list
          if (key_iter == keys->rend() || value_iter == values->rend()) {
            // sanity check: if one ends, then both end
            assert(key_iter == keys->rend() && value_iter == values->rend());
            break;
          }

          // otherwise not at end of list. update key, value, and ikey.
          key = slice(*key_iter);
          value = slice(*value_iter);
          parseinternalkey(key, &ikey);

        } else{
          // only had one item to begin with (put/delete)
          break;
        }
      }
    }

    // mergeuntil has moved input to the next entry
    if (!current_entry_is_merging) {
      input->next();
    }
  }
  if (key_drop_user > 0) {
    recordtick(stats_, compaction_key_drop_user, key_drop_user);
  }
  if (key_drop_newer_entry > 0) {
    recordtick(stats_, compaction_key_drop_newer_entry, key_drop_newer_entry);
  }
  if (key_drop_obsolete > 0) {
    recordtick(stats_, compaction_key_drop_obsolete, key_drop_obsolete);
  }
  recordcompactioniostats();

  return status;
}

void dbimpl::callcompactionfilterv2(compactionstate* compact,
  compactionfilterv2* compaction_filter_v2) {
  if (compact == nullptr || compaction_filter_v2 == nullptr) {
    return;
  }

  // assemble slice vectors for user keys and existing values.
  // we also keep track of our parsed internal key structs because
  // we may need to access the sequence number in the event that
  // keys are garbage collected during the filter process.
  std::vector<parsedinternalkey> ikey_buf;
  std::vector<slice> user_key_buf;
  std::vector<slice> existing_value_buf;

  for (const auto& key : compact->key_str_buf_) {
    parsedinternalkey ikey;
    parseinternalkey(slice(key), &ikey);
    ikey_buf.emplace_back(ikey);
    user_key_buf.emplace_back(ikey.user_key);
  }
  for (const auto& value : compact->existing_value_str_buf_) {
    existing_value_buf.emplace_back(slice(value));
  }

  // if the user has specified a compaction filter and the sequence
  // number is greater than any external snapshot, then invoke the
  // filter.
  // if the return value of the compaction filter is true, replace
  // the entry with a delete marker.
  compact->to_delete_buf_ = compaction_filter_v2->filter(
      compact->compaction->level(),
      user_key_buf, existing_value_buf,
      &compact->new_value_buf_,
      &compact->value_changed_buf_);

  // new_value_buf_.size() <= to_delete__buf_.size(). "=" iff all
  // kv-pairs in this compaction run needs to be deleted.
  assert(compact->to_delete_buf_.size() ==
      compact->key_str_buf_.size());
  assert(compact->to_delete_buf_.size() ==
      compact->existing_value_str_buf_.size());
  assert(compact->to_delete_buf_.size() ==
      compact->value_changed_buf_.size());

  int new_value_idx = 0;
  for (unsigned int i = 0; i < compact->to_delete_buf_.size(); ++i) {
    if (compact->to_delete_buf_[i]) {
      // update the string buffer directly
      // the slice buffer points to the updated buffer
      updateinternalkey(&compact->key_str_buf_[i][0],
                        compact->key_str_buf_[i].size(),
                        ikey_buf[i].sequence,
                        ktypedeletion);

      // no value associated with delete
      compact->existing_value_str_buf_[i].clear();
      recordtick(stats_, compaction_key_drop_user);
    } else if (compact->value_changed_buf_[i]) {
      compact->existing_value_str_buf_[i] =
          compact->new_value_buf_[new_value_idx++];
    }
  }  // for
}

status dbimpl::docompactionwork(compactionstate* compact,
                                deletionstate& deletion_state,
                                logbuffer* log_buffer) {
  assert(compact);
  compact->cleanupbatchbuffer();
  compact->cleanupmergedbuffer();
  bool prefix_initialized = false;

  // generate file_levels_ for compaction berfore making iterator
  compact->compaction->generatefilelevels();
  int64_t imm_micros = 0;  // micros spent doing imm_ compactions
  columnfamilydata* cfd = compact->compaction->column_family_data();
  logtobuffer(
      log_buffer,
      "[%s] compacting %d@%d + %d@%d files, score %.2f slots available %d",
      cfd->getname().c_str(), compact->compaction->num_input_files(0),
      compact->compaction->level(), compact->compaction->num_input_files(1),
      compact->compaction->output_level(), compact->compaction->score(),
      options_.max_background_compactions - bg_compaction_scheduled_);
  char scratch[2345];
  compact->compaction->summary(scratch, sizeof(scratch));
  logtobuffer(log_buffer, "[%s] compaction start summary: %s\n",
              cfd->getname().c_str(), scratch);

  assert(cfd->current()->numlevelfiles(compact->compaction->level()) > 0);
  assert(compact->builder == nullptr);
  assert(!compact->outfile);

  sequencenumber visible_at_tip = 0;
  sequencenumber earliest_snapshot;
  sequencenumber latest_snapshot = 0;
  snapshots_.getall(compact->existing_snapshots);
  if (compact->existing_snapshots.size() == 0) {
    // optimize for fast path if there are no snapshots
    visible_at_tip = versions_->lastsequence();
    earliest_snapshot = visible_at_tip;
  } else {
    latest_snapshot = compact->existing_snapshots.back();
    // add the current seqno as the 'latest' virtual
    // snapshot to the end of this list.
    compact->existing_snapshots.push_back(versions_->lastsequence());
    earliest_snapshot = compact->existing_snapshots[0];
  }

  // is this compaction producing files at the bottommost level?
  bool bottommost_level = compact->compaction->bottommostlevel();

  // allocate the output file numbers before we release the lock
  allocatecompactionoutputfilenumbers(compact);

  bool is_snapshot_supported = issnapshotsupported();
  // release mutex while we're actually doing the compaction work
  mutex_.unlock();
  log_buffer->flushbuffertolog();

  const uint64_t start_micros = env_->nowmicros();
  unique_ptr<iterator> input(versions_->makeinputiterator(compact->compaction));
  input->seektofirst();
  shared_ptr<iterator> backup_input(
      versions_->makeinputiterator(compact->compaction));
  backup_input->seektofirst();

  status status;
  parsedinternalkey ikey;
  std::unique_ptr<compactionfilterv2> compaction_filter_from_factory_v2
    = nullptr;
  auto context = compact->getfiltercontext();
  compaction_filter_from_factory_v2 =
      cfd->options()->compaction_filter_factory_v2->createcompactionfilterv2(
          context);
  auto compaction_filter_v2 =
    compaction_filter_from_factory_v2.get();

  // temp_backup_input always point to the start of the current buffer
  // temp_backup_input = backup_input;
  // iterate through input,
  // 1) buffer ineligible keys and value keys into 2 separate buffers;
  // 2) send value_buffer to compaction filter and alternate the values;
  // 3) merge value_buffer with ineligible_value_buffer;
  // 4) run the modified "compaction" using the old for loop.
  if (compaction_filter_v2) {
    while (backup_input->valid() && !shutting_down_.acquire_load() &&
           !cfd->isdropped()) {
      // flush preempts compaction
      // todo(icanadi) this currently only checks if flush is necessary on
      // compacting column family. we should also check if flush is necessary on
      // other column families, too
      imm_micros += callflushduringcompaction(cfd, deletion_state, log_buffer);

      slice key = backup_input->key();
      slice value = backup_input->value();

      if (!parseinternalkey(key, &ikey)) {
        // log error
        log(options_.info_log, "[%s] failed to parse key: %s",
            cfd->getname().c_str(), key.tostring().c_str());
        continue;
      } else {
        const slicetransform* transformer =
            cfd->options()->compaction_filter_factory_v2->getprefixextractor();
        const auto key_prefix = transformer->transform(ikey.user_key);
        if (!prefix_initialized) {
          compact->cur_prefix_ = key_prefix.tostring();
          prefix_initialized = true;
        }
        // if the prefix remains the same, keep buffering
        if (key_prefix.compare(slice(compact->cur_prefix_)) == 0) {
          // apply the compaction filter v2 to all the kv pairs sharing
          // the same prefix
          if (ikey.type == ktypevalue &&
              (visible_at_tip || ikey.sequence > latest_snapshot)) {
            // buffer all keys sharing the same prefix for compactionfilterv2
            // iterate through keys to check prefix
            compact->bufferkeyvalueslices(key, value);
          } else {
            // buffer ineligible keys
            compact->bufferotherkeyvalueslices(key, value);
          }
          backup_input->next();
          continue;
          // finish changing values for eligible keys
        } else {
          // now prefix changes, this batch is done.
          // call compaction filter on the buffered values to change the value
          if (compact->key_str_buf_.size() > 0) {
            callcompactionfilterv2(compact, compaction_filter_v2);
          }
          compact->cur_prefix_ = key_prefix.tostring();
        }
      }

      // merge this batch of data (values + ineligible keys)
      compact->mergekeyvalueslicebuffer(&cfd->internal_comparator());

      // done buffering for the current prefix. spit it out to disk
      // now just iterate through all the kv-pairs
      status = processkeyvaluecompaction(
          is_snapshot_supported,
          visible_at_tip,
          earliest_snapshot,
          latest_snapshot,
          deletion_state,
          bottommost_level,
          imm_micros,
          input.get(),
          compact,
          true,
          log_buffer);

      if (!status.ok()) {
        break;
      }

      // after writing the kv-pairs, we can safely remove the reference
      // to the string buffer and clean them up
      compact->cleanupbatchbuffer();
      compact->cleanupmergedbuffer();
      // buffer the key that triggers the mismatch in prefix
      if (ikey.type == ktypevalue &&
        (visible_at_tip || ikey.sequence > latest_snapshot)) {
        compact->bufferkeyvalueslices(key, value);
      } else {
        compact->bufferotherkeyvalueslices(key, value);
      }
      backup_input->next();
      if (!backup_input->valid()) {
        // if this is the single last value, we need to merge it.
        if (compact->key_str_buf_.size() > 0) {
          callcompactionfilterv2(compact, compaction_filter_v2);
        }
        compact->mergekeyvalueslicebuffer(&cfd->internal_comparator());

        status = processkeyvaluecompaction(
            is_snapshot_supported,
            visible_at_tip,
            earliest_snapshot,
            latest_snapshot,
            deletion_state,
            bottommost_level,
            imm_micros,
            input.get(),
            compact,
            true,
            log_buffer);

        compact->cleanupbatchbuffer();
        compact->cleanupmergedbuffer();
      }
    }  // done processing all prefix batches
    // finish the last batch
    if (compact->key_str_buf_.size() > 0) {
      callcompactionfilterv2(compact, compaction_filter_v2);
    }
    compact->mergekeyvalueslicebuffer(&cfd->internal_comparator());
    status = processkeyvaluecompaction(
        is_snapshot_supported,
        visible_at_tip,
        earliest_snapshot,
        latest_snapshot,
        deletion_state,
        bottommost_level,
        imm_micros,
        input.get(),
        compact,
        true,
        log_buffer);
  }  // checking for compaction filter v2

  if (!compaction_filter_v2) {
    status = processkeyvaluecompaction(
      is_snapshot_supported,
      visible_at_tip,
      earliest_snapshot,
      latest_snapshot,
      deletion_state,
      bottommost_level,
      imm_micros,
      input.get(),
      compact,
      false,
      log_buffer);
  }

  if (status.ok() && (shutting_down_.acquire_load() || cfd->isdropped())) {
    status = status::shutdowninprogress(
        "database shutdown or column family drop during compaction");
  }
  if (status.ok() && compact->builder != nullptr) {
    status = finishcompactionoutputfile(compact, input.get());
  }
  if (status.ok()) {
    status = input->status();
  }
  input.reset();

  if (!options_.disabledatasync) {
    db_directory_->fsync();
  }

  internalstats::compactionstats stats(1);
  stats.micros = env_->nowmicros() - start_micros - imm_micros;
  stats.files_in_leveln = compact->compaction->num_input_files(0);
  stats.files_in_levelnp1 = compact->compaction->num_input_files(1);
  measuretime(stats_, compaction_time, stats.micros);

  int num_output_files = compact->outputs.size();
  if (compact->builder != nullptr) {
    // an error occurred so ignore the last output.
    assert(num_output_files > 0);
    --num_output_files;
  }
  stats.files_out_levelnp1 = num_output_files;

  for (int i = 0; i < compact->compaction->num_input_files(0); i++) {
    stats.bytes_readn += compact->compaction->input(0, i)->fd.getfilesize();
  }

  for (int i = 0; i < compact->compaction->num_input_files(1); i++) {
    stats.bytes_readnp1 += compact->compaction->input(1, i)->fd.getfilesize();
  }

  for (int i = 0; i < num_output_files; i++) {
    stats.bytes_written += compact->outputs[i].file_size;
  }

  recordcompactioniostats();

  logflush(options_.info_log);
  mutex_.lock();
  cfd->internal_stats()->addcompactionstats(
      compact->compaction->output_level(), stats);

  // if there were any unused file number (mostly in case of
  // compaction error), free up the entry from pending_putputs
  releasecompactionunusedfilenumbers(compact);

  if (status.ok()) {
    status = installcompactionresults(compact, log_buffer);
    installsuperversion(cfd, deletion_state);
  }
  version::levelsummarystorage tmp;
  logtobuffer(
      log_buffer,
      "[%s] compacted to: %s, %.1f mb/sec, level %d, files in(%d, %d) out(%d) "
      "mb in(%.1f, %.1f) out(%.1f), read-write-amplify(%.1f) "
      "write-amplify(%.1f) %s\n",
      cfd->getname().c_str(), cfd->current()->levelsummary(&tmp),
      (stats.bytes_readn + stats.bytes_readnp1 + stats.bytes_written) /
          (double)stats.micros,
      compact->compaction->output_level(), stats.files_in_leveln,
      stats.files_in_levelnp1, stats.files_out_levelnp1,
      stats.bytes_readn / 1048576.0, stats.bytes_readnp1 / 1048576.0,
      stats.bytes_written / 1048576.0,
      (stats.bytes_written + stats.bytes_readnp1 + stats.bytes_readn) /
          (double)stats.bytes_readn,
      stats.bytes_written / (double)stats.bytes_readn,
      status.tostring().c_str());

  return status;
}

namespace {
struct iterstate {
  iterstate(dbimpl* db, port::mutex* mu, superversion* super_version)
      : db(db), mu(mu), super_version(super_version) {}

  dbimpl* db;
  port::mutex* mu;
  superversion* super_version;
};

static void cleanupiteratorstate(void* arg1, void* arg2) {
  iterstate* state = reinterpret_cast<iterstate*>(arg1);

  if (state->super_version->unref()) {
    dbimpl::deletionstate deletion_state;

    state->mu->lock();
    state->super_version->cleanup();
    state->db->findobsoletefiles(deletion_state, false, true);
    state->mu->unlock();

    delete state->super_version;
    if (deletion_state.havesomethingtodelete()) {
      state->db->purgeobsoletefiles(deletion_state);
    }
  }

  delete state;
}
}  // namespace

iterator* dbimpl::newinternaliterator(const readoptions& options,
                                      columnfamilydata* cfd,
                                      superversion* super_version,
                                      arena* arena) {
  iterator* internal_iter;
  if (arena != nullptr) {
    // need to create internal iterator from the arena.
    mergeiteratorbuilder merge_iter_builder(&cfd->internal_comparator(), arena);
    // collect iterator for mutable mem
    merge_iter_builder.additerator(
        super_version->mem->newiterator(options, arena));
    // collect all needed child iterators for immutable memtables
    super_version->imm->additerators(options, &merge_iter_builder);
    // collect iterators for files in l0 - ln
    super_version->current->additerators(options, storage_options_,
                                         &merge_iter_builder);
    internal_iter = merge_iter_builder.finish();
  } else {
    // need to create internal iterator using malloc.
    std::vector<iterator*> iterator_list;
    // collect iterator for mutable mem
    iterator_list.push_back(super_version->mem->newiterator(options));
    // collect all needed child iterators for immutable memtables
    super_version->imm->additerators(options, &iterator_list);
    // collect iterators for files in l0 - ln
    super_version->current->additerators(options, storage_options_,
                                         &iterator_list);
    internal_iter = newmergingiterator(&cfd->internal_comparator(),
                                       &iterator_list[0], iterator_list.size());
  }
  iterstate* cleanup = new iterstate(this, &mutex_, super_version);
  internal_iter->registercleanup(cleanupiteratorstate, cleanup, nullptr);

  return internal_iter;
}

columnfamilyhandle* dbimpl::defaultcolumnfamily() const {
  return default_cf_handle_;
}

status dbimpl::get(const readoptions& options,
                   columnfamilyhandle* column_family, const slice& key,
                   std::string* value) {
  return getimpl(options, column_family, key, value);
}

// deletionstate gets created and destructed outside of the lock -- we
// use this convinently to:
// * malloc one superversion() outside of the lock -- new_superversion
// * delete superversion()s outside of the lock -- superversions_to_free
//
// however, if installsuperversion() gets called twice with the same,
// deletion_state, we can't reuse the superversion() that got malloced because
// first call already used it. in that rare case, we take a hit and create a
// new superversion() inside of the mutex. we do similar thing
// for superversion_to_free
void dbimpl::installsuperversion(columnfamilydata* cfd,
                                 deletionstate& deletion_state) {
  mutex_.assertheld();
  // if new_superversion == nullptr, it means somebody already used it
  superversion* new_superversion =
    (deletion_state.new_superversion != nullptr) ?
    deletion_state.new_superversion : new superversion();
  superversion* old_superversion =
      cfd->installsuperversion(new_superversion, &mutex_);
  deletion_state.new_superversion = nullptr;
  deletion_state.superversions_to_free.push_back(old_superversion);
}

status dbimpl::getimpl(const readoptions& options,
                       columnfamilyhandle* column_family, const slice& key,
                       std::string* value, bool* value_found) {
  stopwatch sw(env_, stats_, db_get);
  perf_timer_guard(get_snapshot_time);

  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  auto cfd = cfh->cfd();

  sequencenumber snapshot;
  if (options.snapshot != nullptr) {
    snapshot = reinterpret_cast<const snapshotimpl*>(options.snapshot)->number_;
  } else {
    snapshot = versions_->lastsequence();
  }

  // acquire superversion
  superversion* sv = getandrefsuperversion(cfd);

  // prepare to store a list of merge operations if merge occurs.
  mergecontext merge_context;

  status s;
  // first look in the memtable, then in the immutable memtable (if any).
  // s is both in/out. when in, s could either be ok or mergeinprogress.
  // merge_operands will contain the sequence of merges in the latter case.
  lookupkey lkey(key, snapshot);
  perf_timer_stop(get_snapshot_time);

  if (sv->mem->get(lkey, value, &s, merge_context, *cfd->options())) {
    // done
    recordtick(stats_, memtable_hit);
  } else if (sv->imm->get(lkey, value, &s, merge_context, *cfd->options())) {
    // done
    recordtick(stats_, memtable_hit);
  } else {
    perf_timer_guard(get_from_output_files_time);
    sv->current->get(options, lkey, value, &s, &merge_context, value_found);
    recordtick(stats_, memtable_miss);
  }

  {
    perf_timer_guard(get_post_process_time);

    returnandcleanupsuperversion(cfd, sv);

    recordtick(stats_, number_keys_read);
    recordtick(stats_, bytes_read, value->size());
  }
  return s;
}

std::vector<status> dbimpl::multiget(
    const readoptions& options,
    const std::vector<columnfamilyhandle*>& column_family,
    const std::vector<slice>& keys, std::vector<std::string>* values) {

  stopwatch sw(env_, stats_, db_multiget);
  perf_timer_guard(get_snapshot_time);

  sequencenumber snapshot;

  struct multigetcolumnfamilydata {
    columnfamilydata* cfd;
    superversion* super_version;
  };
  std::unordered_map<uint32_t, multigetcolumnfamilydata*> multiget_cf_data;
  // fill up and allocate outside of mutex
  for (auto cf : column_family) {
    auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(cf);
    auto cfd = cfh->cfd();
    if (multiget_cf_data.find(cfd->getid()) == multiget_cf_data.end()) {
      auto mgcfd = new multigetcolumnfamilydata();
      mgcfd->cfd = cfd;
      multiget_cf_data.insert({cfd->getid(), mgcfd});
    }
  }

  mutex_.lock();
  if (options.snapshot != nullptr) {
    snapshot = reinterpret_cast<const snapshotimpl*>(options.snapshot)->number_;
  } else {
    snapshot = versions_->lastsequence();
  }
  for (auto mgd_iter : multiget_cf_data) {
    mgd_iter.second->super_version =
        mgd_iter.second->cfd->getsuperversion()->ref();
  }
  mutex_.unlock();

  // contain a list of merge operations if merge occurs.
  mergecontext merge_context;

  // note: this always resizes the values array
  size_t num_keys = keys.size();
  std::vector<status> stat_list(num_keys);
  values->resize(num_keys);

  // keep track of bytes that we read for statistics-recording later
  uint64_t bytes_read = 0;
  perf_timer_stop(get_snapshot_time);

  // for each of the given keys, apply the entire "get" process as follows:
  // first look in the memtable, then in the immutable memtable (if any).
  // s is both in/out. when in, s could either be ok or mergeinprogress.
  // merge_operands will contain the sequence of merges in the latter case.
  for (size_t i = 0; i < num_keys; ++i) {
    merge_context.clear();
    status& s = stat_list[i];
    std::string* value = &(*values)[i];

    lookupkey lkey(keys[i], snapshot);
    auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family[i]);
    auto mgd_iter = multiget_cf_data.find(cfh->cfd()->getid());
    assert(mgd_iter != multiget_cf_data.end());
    auto mgd = mgd_iter->second;
    auto super_version = mgd->super_version;
    auto cfd = mgd->cfd;
    if (super_version->mem->get(lkey, value, &s, merge_context,
                                *cfd->options())) {
      // done
    } else if (super_version->imm->get(lkey, value, &s, merge_context,
                                       *cfd->options())) {
      // done
    } else {
      super_version->current->get(options, lkey, value, &s, &merge_context);
    }

    if (s.ok()) {
      bytes_read += value->size();
    }
  }

  // post processing (decrement reference counts and record statistics)
  perf_timer_guard(get_post_process_time);
  autovector<superversion*> superversions_to_delete;

  // todo(icanadi) do we need lock here or just around cleanup()?
  mutex_.lock();
  for (auto mgd_iter : multiget_cf_data) {
    auto mgd = mgd_iter.second;
    if (mgd->super_version->unref()) {
      mgd->super_version->cleanup();
      superversions_to_delete.push_back(mgd->super_version);
    }
  }
  mutex_.unlock();

  for (auto td : superversions_to_delete) {
    delete td;
  }
  for (auto mgd : multiget_cf_data) {
    delete mgd.second;
  }

  recordtick(stats_, number_multiget_calls);
  recordtick(stats_, number_multiget_keys_read, num_keys);
  recordtick(stats_, number_multiget_bytes_read, bytes_read);
  perf_timer_stop(get_post_process_time);

  return stat_list;
}

status dbimpl::createcolumnfamily(const columnfamilyoptions& options,
                                  const std::string& column_family_name,
                                  columnfamilyhandle** handle) {
  *handle = nullptr;
  mutexlock l(&mutex_);

  if (versions_->getcolumnfamilyset()->getcolumnfamily(column_family_name) !=
      nullptr) {
    return status::invalidargument("column family already exists");
  }
  versionedit edit;
  edit.addcolumnfamily(column_family_name);
  uint32_t new_id = versions_->getcolumnfamilyset()->getnextcolumnfamilyid();
  edit.setcolumnfamily(new_id);
  edit.setlognumber(logfile_number_);
  edit.setcomparatorname(options.comparator->name());

  // logandapply will both write the creation in manifest and create
  // columnfamilydata object
  status s = versions_->logandapply(nullptr, &edit, &mutex_,
                                    db_directory_.get(), false, &options);
  if (s.ok()) {
    single_column_family_mode_ = false;
    auto cfd =
        versions_->getcolumnfamilyset()->getcolumnfamily(column_family_name);
    assert(cfd != nullptr);
    delete cfd->installsuperversion(new superversion(), &mutex_);
    *handle = new columnfamilyhandleimpl(cfd, this, &mutex_);
    log(options_.info_log, "created column family [%s] (id %u)",
        column_family_name.c_str(), (unsigned)cfd->getid());
    max_total_in_memory_state_ += cfd->options()->write_buffer_size *
                                  cfd->options()->max_write_buffer_number;
  } else {
    log(options_.info_log, "creating column family [%s] failed -- %s",
        column_family_name.c_str(), s.tostring().c_str());
  }
  return s;
}

status dbimpl::dropcolumnfamily(columnfamilyhandle* column_family) {
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  auto cfd = cfh->cfd();
  if (cfd->getid() == 0) {
    return status::invalidargument("can't drop default column family");
  }

  versionedit edit;
  edit.dropcolumnfamily();
  edit.setcolumnfamily(cfd->getid());

  status s;
  {
    mutexlock l(&mutex_);
    if (cfd->isdropped()) {
      s = status::invalidargument("column family already dropped!\n");
    }
    if (s.ok()) {
      s = versions_->logandapply(cfd, &edit, &mutex_);
    }
  }

  if (s.ok()) {
    assert(cfd->isdropped());
    max_total_in_memory_state_ -= cfd->options()->write_buffer_size *
                                  cfd->options()->max_write_buffer_number;
    log(options_.info_log, "dropped column family with id %u\n", cfd->getid());
  } else {
    log(options_.info_log, "dropping column family with id %u failed -- %s\n",
        cfd->getid(), s.tostring().c_str());
  }

  return s;
}

bool dbimpl::keymayexist(const readoptions& options,
                         columnfamilyhandle* column_family, const slice& key,
                         std::string* value, bool* value_found) {
  if (value_found != nullptr) {
    // falsify later if key-may-exist but can't fetch value
    *value_found = true;
  }
  readoptions roptions = options;
  roptions.read_tier = kblockcachetier; // read from block cache only
  auto s = getimpl(roptions, column_family, key, value, value_found);

  // if block_cache is enabled and the index block of the table didn't
  // not present in block_cache, the return value will be status::incomplete.
  // in this case, key may still exist in the table.
  return s.ok() || s.isincomplete();
}

iterator* dbimpl::newiterator(const readoptions& options,
                              columnfamilyhandle* column_family) {
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  auto cfd = cfh->cfd();

  if (options.tailing) {
#ifdef rocksdb_lite
    // not supported in lite version
    return nullptr;
#else
    // todo(ljin): remove tailing iterator
    auto iter = new forwarditerator(this, options, cfd);
    return newdbiterator(env_, *cfd->options(), cfd->user_comparator(), iter,
                         kmaxsequencenumber);
// return new tailingiterator(env_, this, options, cfd);
#endif
  } else {
    sequencenumber latest_snapshot = versions_->lastsequence();
    superversion* sv = nullptr;
    sv = cfd->getreferencedsuperversion(&mutex_);

    auto snapshot =
        options.snapshot != nullptr
            ? reinterpret_cast<const snapshotimpl*>(options.snapshot)->number_
            : latest_snapshot;

    // try to generate a db iterator tree in continuous memory area to be
    // cache friendly. here is an example of result:
    // +-------------------------------+
    // |                               |
    // | arenawrappeddbiter            |
    // |  +                            |
    // |  +---> inner iterator   ------------+
    // |  |                            |     |
    // |  |    +-- -- -- -- -- -- -- --+     |
    // |  +--- | arena                 |     |
    // |       |                       |     |
    // |          allocated memory:    |     |
    // |       |   +-------------------+     |
    // |       |   | dbiter            | <---+
    // |           |  +                |
    // |       |   |  +-> iter_  ------------+
    // |       |   |                   |     |
    // |       |   +-------------------+     |
    // |       |   | mergingiterator   | <---+
    // |           |  +                |
    // |       |   |  +->child iter1  ------------+
    // |       |   |  |                |          |
    // |           |  +->child iter2  ----------+ |
    // |       |   |  |                |        | |
    // |       |   |  +->child iter3  --------+ | |
    // |           |                   |      | | |
    // |       |   +-------------------+      | | |
    // |       |   | iterator1         | <--------+
    // |       |   +-------------------+      | |
    // |       |   | iterator2         | <------+
    // |       |   +-------------------+      |
    // |       |   | iterator3         | <----+
    // |       |   +-------------------+
    // |       |                       |
    // +-------+-----------------------+
    //
    // arenawrappeddbiter inlines an arena area where all the iterartor in the
    // the iterator tree is allocated in the order of being accessed when
    // querying.
    // laying out the iterators in the order of being accessed makes it more
    // likely that any iterator pointer is close to the iterator it points to so
    // that they are likely to be in the same cache line and/or page.
    arenawrappeddbiter* db_iter = newarenawrappeddbiterator(
        env_, *cfd->options(), cfd->user_comparator(), snapshot);
    iterator* internal_iter =
        newinternaliterator(options, cfd, sv, db_iter->getarena());
    db_iter->setiterunderdbiter(internal_iter);

    return db_iter;
  }
}

status dbimpl::newiterators(
    const readoptions& options,
    const std::vector<columnfamilyhandle*>& column_families,
    std::vector<iterator*>* iterators) {
  iterators->clear();
  iterators->reserve(column_families.size());
  sequencenumber latest_snapshot = 0;
  std::vector<superversion*> super_versions;
  super_versions.reserve(column_families.size());

  if (!options.tailing) {
    mutex_.lock();
    latest_snapshot = versions_->lastsequence();
    for (auto cfh : column_families) {
      auto cfd = reinterpret_cast<columnfamilyhandleimpl*>(cfh)->cfd();
      super_versions.push_back(cfd->getsuperversion()->ref());
    }
    mutex_.unlock();
  }

  if (options.tailing) {
#ifdef rocksdb_lite
    return status::invalidargument(
        "tailing interator not supported in rocksdb lite");
#else
    for (auto cfh : column_families) {
      auto cfd = reinterpret_cast<columnfamilyhandleimpl*>(cfh)->cfd();
      auto iter = new forwarditerator(this, options, cfd);
      iterators->push_back(
          newdbiterator(env_, *cfd->options(), cfd->user_comparator(), iter,
                        kmaxsequencenumber));
    }
#endif
  } else {
    for (size_t i = 0; i < column_families.size(); ++i) {
      auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_families[i]);
      auto cfd = cfh->cfd();

      auto snapshot =
          options.snapshot != nullptr
              ? reinterpret_cast<const snapshotimpl*>(options.snapshot)->number_
              : latest_snapshot;

      auto iter = newinternaliterator(options, cfd, super_versions[i]);
      iter = newdbiterator(env_, *cfd->options(),
                           cfd->user_comparator(), iter, snapshot);
      iterators->push_back(iter);
    }
  }

  return status::ok();
}

bool dbimpl::issnapshotsupported() const {
  for (auto cfd : *versions_->getcolumnfamilyset()) {
    if (!cfd->mem()->issnapshotsupported()) {
      return false;
    }
  }
  return true;
}

const snapshot* dbimpl::getsnapshot() {
  mutexlock l(&mutex_);
  // returns null if the underlying memtable does not support snapshot.
  if (!issnapshotsupported()) return nullptr;
  return snapshots_.new(versions_->lastsequence());
}

void dbimpl::releasesnapshot(const snapshot* s) {
  mutexlock l(&mutex_);
  snapshots_.delete(reinterpret_cast<const snapshotimpl*>(s));
}

// convenience methods
status dbimpl::put(const writeoptions& o, columnfamilyhandle* column_family,
                   const slice& key, const slice& val) {
  return db::put(o, column_family, key, val);
}

status dbimpl::merge(const writeoptions& o, columnfamilyhandle* column_family,
                     const slice& key, const slice& val) {
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  if (!cfh->cfd()->options()->merge_operator) {
    return status::notsupported("provide a merge_operator when opening db");
  } else {
    return db::merge(o, column_family, key, val);
  }
}

status dbimpl::delete(const writeoptions& options,
                      columnfamilyhandle* column_family, const slice& key) {
  return db::delete(options, column_family, key);
}

// requires: mutex_ is held
status dbimpl::beginwrite(writer* w, uint64_t expiration_time) {
  // the following code block pushes the current writer "w" into the writer
  // queue "writers_" and wait until one of the following conditions met:
  // 1. the job of "w" has been done by some other writers.
  // 2. "w" becomes the first writer in "writers_"
  // 3. "w" timed-out.
  mutex_.assertheld();
  writers_.push_back(w);

  bool timed_out = false;
  while (!w->done && w != writers_.front()) {
    if (expiration_time == 0) {
      w->cv.wait();
    } else if (w->cv.timedwait(expiration_time)) {
      if (w->in_batch_group) {
        // then it means the front writer is currently doing the
        // write on behalf of this "timed-out" writer.  then it
        // should wait until the write completes.
        expiration_time = 0;
      } else {
        timed_out = true;
        break;
      }
    }
  }

  if (timed_out) {
#ifndef ndebug
    bool found = false;
#endif
    for (auto iter = writers_.begin(); iter != writers_.end(); iter++) {
      if (*iter == w) {
        writers_.erase(iter);
#ifndef ndebug
        found = true;
#endif
        break;
      }
    }
#ifndef ndebug
    assert(found);
#endif
    // writers_.front() might still be in cond_wait without a time-out.
    // as a result, we need to signal it to wake it up.  otherwise no
    // one else will wake him up, and rocksdb will hang.
    if (!writers_.empty()) {
      writers_.front()->cv.signal();
    }
    return status::timedout();
  }
  return status::ok();
}

// requires: mutex_ is held
void dbimpl::endwrite(writer* w, writer* last_writer, status status) {
  // pop out the current writer and all writers being pushed before the
  // current writer from the writer queue.
  mutex_.assertheld();
  while (!writers_.empty()) {
    writer* ready = writers_.front();
    writers_.pop_front();
    if (ready != w) {
      ready->status = status;
      ready->done = true;
      ready->cv.signal();
    }
    if (ready == last_writer) break;
  }

  // notify new head of write queue
  if (!writers_.empty()) {
    writers_.front()->cv.signal();
  }
}

status dbimpl::write(const writeoptions& options, writebatch* my_batch) {
  if (my_batch == nullptr) {
    return status::corruption("batch is nullptr!");
  }
  perf_timer_guard(write_pre_and_post_process_time);
  writer w(&mutex_);
  w.batch = my_batch;
  w.sync = options.sync;
  w.disablewal = options.disablewal;
  w.in_batch_group = false;
  w.done = false;
  w.timeout_hint_us = options.timeout_hint_us;

  uint64_t expiration_time = 0;
  if (w.timeout_hint_us == 0) {
    w.timeout_hint_us = knotimeout;
  } else {
    expiration_time = env_->nowmicros() + w.timeout_hint_us;
  }

  if (!options.disablewal) {
    recordtick(stats_, write_with_wal);
    default_cf_internal_stats_->adddbstats(internalstats::write_with_wal, 1);
  }

  writecontext context;
  mutex_.lock();
  status status = beginwrite(&w, expiration_time);
  assert(status.ok() || status.istimedout());
  if (status.istimedout()) {
    mutex_.unlock();
    recordtick(stats_, write_timedout);
    return status::timedout();
  }
  if (w.done) {  // write was done by someone else
    default_cf_internal_stats_->adddbstats(internalstats::write_done_by_other,
                                           1);
    mutex_.unlock();
    recordtick(stats_, write_done_by_other);
    return w.status;
  }

  recordtick(stats_, write_done_by_self);
  default_cf_internal_stats_->adddbstats(internalstats::write_done_by_self, 1);

  // once reaches this point, the current writer "w" will try to do its write
  // job.  it may also pick up some of the remaining writers in the "writers_"
  // when it finds suitable, and finish them in the same write batch.
  // this is how a write job could be done by the other writer.
  assert(!single_column_family_mode_ ||
         versions_->getcolumnfamilyset()->numberofcolumnfamilies() == 1);

  uint64_t flush_column_family_if_log_file = 0;
  uint64_t max_total_wal_size = (options_.max_total_wal_size == 0)
                                    ? 4 * max_total_in_memory_state_
                                    : options_.max_total_wal_size;
  if (unlikely(!single_column_family_mode_) &&
      alive_log_files_.begin()->getting_flushed == false &&
      total_log_size_ > max_total_wal_size) {
    flush_column_family_if_log_file = alive_log_files_.begin()->number;
    alive_log_files_.begin()->getting_flushed = true;
    log(options_.info_log,
        "flushing all column families with data in wal number %" priu64
        ". total log size is %" priu64 " while max_total_wal_size is %" priu64,
        flush_column_family_if_log_file, total_log_size_, max_total_wal_size);
  }

  if (likely(single_column_family_mode_)) {
    // fast path
    status = makeroomforwrite(default_cf_handle_->cfd(),
                              &context, expiration_time);
  } else {
    // refcounting cfd in iteration
    bool dead_cfd = false;
    for (auto cfd : *versions_->getcolumnfamilyset()) {
      cfd->ref();
      if (flush_column_family_if_log_file != 0 &&
          cfd->getlognumber() <= flush_column_family_if_log_file) {
        // log size excedded limit and we need to do flush
        // setnewmemtableandnewlogfie may temporarily unlock and wait
        status = setnewmemtableandnewlogfile(cfd, &context);
        cfd->imm()->flushrequested();
        maybescheduleflushorcompaction();
      } else {
        // may temporarily unlock and wait.
        status = makeroomforwrite(cfd, &context, expiration_time);
      }

      if (cfd->unref()) {
        dead_cfd = true;
      }
      if (!status.ok()) {
        break;
      }
    }
    if (dead_cfd) {
      versions_->getcolumnfamilyset()->freedeadcolumnfamilies();
    }
  }

  uint64_t last_sequence = versions_->lastsequence();
  writer* last_writer = &w;
  if (status.ok()) {
    autovector<writebatch*> write_batch_group;
    buildbatchgroup(&last_writer, &write_batch_group);

    // add to log and apply to memtable.  we can release the lock
    // during this phase since &w is currently responsible for logging
    // and protects against concurrent loggers and concurrent writes
    // into memtables
    {
      mutex_.unlock();
      writebatch* updates = nullptr;
      if (write_batch_group.size() == 1) {
        updates = write_batch_group[0];
      } else {
        updates = &tmp_batch_;
        for (size_t i = 0; i < write_batch_group.size(); ++i) {
          writebatchinternal::append(updates, write_batch_group[i]);
        }
      }

      const sequencenumber current_sequence = last_sequence + 1;
      writebatchinternal::setsequence(updates, current_sequence);
      int my_batch_count = writebatchinternal::count(updates);
      last_sequence += my_batch_count;
      const uint64_t batch_size = writebatchinternal::bytesize(updates);
      // record statistics
      recordtick(stats_, number_keys_written, my_batch_count);
      recordtick(stats_, bytes_written, writebatchinternal::bytesize(updates));
      if (options.disablewal) {
        flush_on_destroy_ = true;
      }
      perf_timer_stop(write_pre_and_post_process_time);

      uint64_t log_size = 0;
      if (!options.disablewal) {
        perf_timer_guard(write_wal_time);
        slice log_entry = writebatchinternal::contents(updates);
        status = log_->addrecord(log_entry);
        total_log_size_ += log_entry.size();
        alive_log_files_.back().addsize(log_entry.size());
        log_empty_ = false;
        log_size = log_entry.size();
        recordtick(stats_, wal_file_synced);
        recordtick(stats_, wal_file_bytes, log_size);
        if (status.ok() && options.sync) {
          if (options_.use_fsync) {
            stopwatch(env_, stats_, wal_file_sync_micros);
            status = log_->file()->fsync();
          } else {
            stopwatch(env_, stats_, wal_file_sync_micros);
            status = log_->file()->sync();
          }
        }
      }
      if (status.ok()) {
        perf_timer_guard(write_memtable_time);

        status = writebatchinternal::insertinto(
            updates, column_family_memtables_.get(),
            options.ignore_missing_column_families, 0, this, false);
        // a non-ok status here indicates iteration failure (either in-memory
        // writebatch corruption (very bad), or the client specified invalid
        // column family).  this will later on trigger bg_error_.
        //
        // note that existing logic was not sound. any partial failure writing
        // into the memtable would result in a state that some write ops might
        // have succeeded in memtable but status reports error for all writes.

        settickercount(stats_, sequence_number, last_sequence);
      }
      perf_timer_start(write_pre_and_post_process_time);
      if (updates == &tmp_batch_) {
        tmp_batch_.clear();
      }
      mutex_.lock();
      // internal stats
      default_cf_internal_stats_->adddbstats(
          internalstats::bytes_written, batch_size);
      if (!options.disablewal) {
        default_cf_internal_stats_->adddbstats(
            internalstats::wal_file_synced, 1);
        default_cf_internal_stats_->adddbstats(
            internalstats::wal_file_bytes, log_size);
      }
      if (status.ok()) {
        versions_->setlastsequence(last_sequence);
      }
    }
  }
  if (options_.paranoid_checks && !status.ok() &&
      !status.istimedout() && bg_error_.ok()) {
    bg_error_ = status; // stop compaction & fail any further writes
  }

  endwrite(&w, last_writer, status);
  mutex_.unlock();

  if (status.istimedout()) {
    recordtick(stats_, write_timedout);
  }

  return status;
}

// this function will be called only when the first writer succeeds.
// all writers in the to-be-built batch group will be processed.
//
// requires: writer list must be non-empty
// requires: first writer must have a non-nullptr batch
void dbimpl::buildbatchgroup(writer** last_writer,
                             autovector<writebatch*>* write_batch_group) {
  assert(!writers_.empty());
  writer* first = writers_.front();
  assert(first->batch != nullptr);

  size_t size = writebatchinternal::bytesize(first->batch);
  write_batch_group->push_back(first->batch);

  // allow the group to grow up to a maximum size, but if the
  // original write is small, limit the growth so we do not slow
  // down the small write too much.
  size_t max_size = 1 << 20;
  if (size <= (128<<10)) {
    max_size = size + (128<<10);
  }

  *last_writer = first;
  std::deque<writer*>::iterator iter = writers_.begin();
  ++iter;  // advance past "first"
  for (; iter != writers_.end(); ++iter) {
    writer* w = *iter;
    if (w->sync && !first->sync) {
      // do not include a sync write into a batch handled by a non-sync write.
      break;
    }

    if (!w->disablewal && first->disablewal) {
      // do not include a write that needs wal into a batch that has
      // wal disabled.
      break;
    }

    if (w->timeout_hint_us < first->timeout_hint_us) {
      // do not include those writes with shorter timeout.  otherwise, we might
      // execute a write that should instead be aborted because of timeout.
      break;
    }

    if (w->batch == nullptr) {
      // do not include those writes with nullptr batch. those are not writes,
      // those are something else. they want to be alone
      break;
    }

    size += writebatchinternal::bytesize(w->batch);
    if (size > max_size) {
      // do not make batch too big
      break;
    }

    write_batch_group->push_back(w->batch);
    w->in_batch_group = true;
    *last_writer = w;
  }
}

// this function computes the amount of time in microseconds by which a write
// should be delayed based on the number of level-0 files according to the
// following formula:
// if n < bottom, return 0;
// if n >= top, return 1000;
// otherwise, let r = (n - bottom) /
//                    (top - bottom)
//  and return r^2 * 1000.
// the goal of this formula is to gradually increase the rate at which writes
// are slowed. we also tried linear delay (r * 1000), but it seemed to do
// slightly worse. there is no other particular reason for choosing quadratic.
uint64_t dbimpl::slowdownamount(int n, double bottom, double top) {
  uint64_t delay;
  if (n >= top) {
    delay = 1000;
  }
  else if (n < bottom) {
    delay = 0;
  }
  else {
    // if we are here, we know that:
    //   level0_start_slowdown <= n < level0_slowdown
    // since the previous two conditions are false.
    double how_much =
      (double) (n - bottom) /
              (top - bottom);
    delay = std::max(how_much * how_much * 1000, 100.0);
  }
  assert(delay <= 1000);
  return delay;
}

// requires: mutex_ is held
// requires: this thread is currently at the front of the writer queue
status dbimpl::makeroomforwrite(columnfamilydata* cfd,
                                writecontext* context,
                                uint64_t expiration_time) {
  mutex_.assertheld();
  assert(!writers_.empty());
  bool allow_delay = true;
  bool allow_hard_rate_limit_delay = true;
  bool allow_soft_rate_limit_delay = true;
  uint64_t rate_limit_delay_millis = 0;
  status s;
  double score;
  // once we schedule background work, we shouldn't schedule it again, since it
  // might generate a tight feedback loop, constantly scheduling more background
  // work, even if additional background work is not needed
  bool schedule_background_work = true;
  bool has_timeout = (expiration_time > 0);

  while (true) {
    if (!bg_error_.ok()) {
      // yield previous error
      s = bg_error_;
      break;
    } else if (has_timeout && env_->nowmicros() > expiration_time) {
      s = status::timedout();
      break;
    } else if (allow_delay && cfd->needslowdownfornumlevel0files()) {
      // we are getting close to hitting a hard limit on the number of
      // l0 files.  rather than delaying a single write by several
      // seconds when we hit the hard limit, start delaying each
      // individual write by 0-1ms to reduce latency variance.  also,
      // this delay hands over some cpu to the compaction thread in
      // case it is sharing the same core as the writer.
      uint64_t slowdown =
          slowdownamount(cfd->current()->numlevelfiles(0),
                         cfd->options()->level0_slowdown_writes_trigger,
                         cfd->options()->level0_stop_writes_trigger);
      mutex_.unlock();
      uint64_t delayed;
      {
        stopwatch sw(env_, stats_, stall_l0_slowdown_count, &delayed);
        env_->sleepformicroseconds(slowdown);
      }
      recordtick(stats_, stall_l0_slowdown_micros, delayed);
      allow_delay = false;  // do not delay a single write more than once
      mutex_.lock();
      cfd->internal_stats()->addcfstats(
          internalstats::level0_slowdown, delayed);
      delayed_writes_++;
    } else if (!cfd->mem()->shouldflush()) {
      // there is room in current memtable
      if (allow_delay) {
        delayloggingandreset();
      }
      break;
    } else if (cfd->needwaitfornummemtables()) {
      // we have filled up the current memtable, but the previous
      // ones are still being flushed, so we wait.
      delayloggingandreset();
      log(options_.info_log, "[%s] wait for memtable flush...\n",
          cfd->getname().c_str());
      if (schedule_background_work) {
        maybescheduleflushorcompaction();
        schedule_background_work = false;
      }
      uint64_t stall;
      {
        stopwatch sw(env_, stats_, stall_memtable_compaction_count, &stall);
        if (!has_timeout) {
          bg_cv_.wait();
        } else {
          bg_cv_.timedwait(expiration_time);
        }
      }
      recordtick(stats_, stall_memtable_compaction_micros, stall);
      cfd->internal_stats()->addcfstats(
          internalstats::memtable_compaction, stall);
    } else if (cfd->needwaitfornumlevel0files()) {
      delayloggingandreset();
      log(options_.info_log, "[%s] wait for fewer level0 files...\n",
          cfd->getname().c_str());
      uint64_t stall;
      {
        stopwatch sw(env_, stats_, stall_l0_num_files_count, &stall);
        if (!has_timeout) {
          bg_cv_.wait();
        } else {
          bg_cv_.timedwait(expiration_time);
        }
      }
      recordtick(stats_, stall_l0_num_files_micros, stall);
      cfd->internal_stats()->addcfstats(
          internalstats::level0_num_files, stall);
    } else if (allow_hard_rate_limit_delay && cfd->exceedshardratelimit()) {
      // delay a write when the compaction score for any level is too large.
      const int max_level = cfd->current()->maxcompactionscorelevel();
      score = cfd->current()->maxcompactionscore();
      mutex_.unlock();
      uint64_t delayed;
      {
        stopwatch sw(env_, stats_, hard_rate_limit_delay_count, &delayed);
        env_->sleepformicroseconds(1000);
      }
      // make sure the following value doesn't round to zero.
      uint64_t rate_limit = std::max((delayed / 1000), (uint64_t) 1);
      rate_limit_delay_millis += rate_limit;
      recordtick(stats_, rate_limit_delay_millis, rate_limit);
      if (cfd->options()->rate_limit_delay_max_milliseconds > 0 &&
          rate_limit_delay_millis >=
              (unsigned)cfd->options()->rate_limit_delay_max_milliseconds) {
        allow_hard_rate_limit_delay = false;
      }
      mutex_.lock();
      cfd->internal_stats()->recordlevelnslowdown(max_level, delayed, false);
    } else if (allow_soft_rate_limit_delay && cfd->exceedssoftratelimit()) {
      const int max_level = cfd->current()->maxcompactionscorelevel();
      score = cfd->current()->maxcompactionscore();
      // delay a write when the compaction score for any level is too large.
      // todo: add statistics
      uint64_t slowdown = slowdownamount(score, cfd->options()->soft_rate_limit,
                                         cfd->options()->hard_rate_limit);
      uint64_t elapsed = 0;
      mutex_.unlock();
      {
        stopwatch sw(env_, stats_, soft_rate_limit_delay_count, &elapsed);
        env_->sleepformicroseconds(slowdown);
        rate_limit_delay_millis += slowdown;
      }
      allow_soft_rate_limit_delay = false;
      mutex_.lock();
      cfd->internal_stats()->recordlevelnslowdown(max_level, elapsed, true);
    } else {
      s = setnewmemtableandnewlogfile(cfd, context);
      if (!s.ok()) {
        break;
      }
      maybescheduleflushorcompaction();
    }
  }
  return s;
}

// requires: mutex_ is held
// requires: this thread is currently at the front of the writer queue
status dbimpl::setnewmemtableandnewlogfile(columnfamilydata* cfd,
                                           writecontext* context) {
  mutex_.assertheld();
  unique_ptr<writablefile> lfile;
  log::writer* new_log = nullptr;
  memtable* new_mem = nullptr;

  // attempt to switch to a new memtable and trigger flush of old.
  // do this without holding the dbmutex lock.
  assert(versions_->prevlognumber() == 0);
  bool creating_new_log = !log_empty_;
  uint64_t new_log_number =
      creating_new_log ? versions_->newfilenumber() : logfile_number_;
  superversion* new_superversion = nullptr;
  mutex_.unlock();
  status s;
  {
    delayloggingandreset();
    if (creating_new_log) {
      s = env_->newwritablefile(logfilename(options_.wal_dir, new_log_number),
                                &lfile,
                                env_->optimizeforlogwrite(storage_options_));
      if (s.ok()) {
        // our final size should be less than write_buffer_size
        // (compression, etc) but err on the side of caution.
        lfile->setpreallocationblocksize(1.1 *
                                         cfd->options()->write_buffer_size);
        new_log = new log::writer(std::move(lfile));
      }
    }

    if (s.ok()) {
      new_mem = new memtable(cfd->internal_comparator(), *cfd->options());
      new_superversion = new superversion();
    }
  }
  mutex_.lock();
  if (!s.ok()) {
    // how do we fail if we're not creating new log?
    assert(creating_new_log);
    // avoid chewing through file number space in a tight loop.
    versions_->reuselogfilenumber(new_log_number);
    assert(!new_mem);
    assert(!new_log);
    return s;
  }
  if (creating_new_log) {
    logfile_number_ = new_log_number;
    assert(new_log != nullptr);
    context->logs_to_free_.push_back(log_.release());
    log_.reset(new_log);
    log_empty_ = true;
    alive_log_files_.push_back(logfilenumbersize(logfile_number_));
    for (auto cfd : *versions_->getcolumnfamilyset()) {
      // all this is just optimization to delete logs that
      // are no longer needed -- if cf is empty, that means it
      // doesn't need that particular log to stay alive, so we just
      // advance the log number. no need to persist this in the manifest
      if (cfd->mem()->getfirstsequencenumber() == 0 &&
          cfd->imm()->size() == 0) {
        cfd->setlognumber(logfile_number_);
      }
    }
  }
  cfd->mem()->setnextlognumber(logfile_number_);
  cfd->imm()->add(cfd->mem());
  new_mem->ref();
  cfd->setmemtable(new_mem);
  log(options_.info_log,
      "[%s] new memtable created with log file: #%" priu64 "\n",
      cfd->getname().c_str(), logfile_number_);
  context->superversions_to_free_.push_back(
      cfd->installsuperversion(new_superversion, &mutex_));
  return s;
}

#ifndef rocksdb_lite
status dbimpl::getpropertiesofalltables(columnfamilyhandle* column_family,
                                        tablepropertiescollection* props) {
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  auto cfd = cfh->cfd();

  // increment the ref count
  mutex_.lock();
  auto version = cfd->current();
  version->ref();
  mutex_.unlock();

  auto s = version->getpropertiesofalltables(props);

  // decrement the ref count
  mutex_.lock();
  version->unref();
  mutex_.unlock();

  return s;
}
#endif  // rocksdb_lite

const std::string& dbimpl::getname() const {
  return dbname_;
}

env* dbimpl::getenv() const {
  return env_;
}

const options& dbimpl::getoptions(columnfamilyhandle* column_family) const {
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  return *cfh->cfd()->options();
}

bool dbimpl::getproperty(columnfamilyhandle* column_family,
                         const slice& property, std::string* value) {
  bool is_int_property = false;
  bool need_out_of_mutex = false;
  dbpropertytype property_type =
      getpropertytype(property, &is_int_property, &need_out_of_mutex);

  value->clear();
  if (is_int_property) {
    uint64_t int_value;
    bool ret_value = getintpropertyinternal(column_family, property_type,
                                            need_out_of_mutex, &int_value);
    if (ret_value) {
      *value = std::to_string(int_value);
    }
    return ret_value;
  } else {
    auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
    auto cfd = cfh->cfd();
    mutexlock l(&mutex_);
    return cfd->internal_stats()->getstringproperty(property_type, property,
                                                    value);
  }
}

bool dbimpl::getintproperty(columnfamilyhandle* column_family,
                            const slice& property, uint64_t* value) {
  bool is_int_property = false;
  bool need_out_of_mutex = false;
  dbpropertytype property_type =
      getpropertytype(property, &is_int_property, &need_out_of_mutex);
  if (!is_int_property) {
    return false;
  }
  return getintpropertyinternal(column_family, property_type, need_out_of_mutex,
                                value);
}

bool dbimpl::getintpropertyinternal(columnfamilyhandle* column_family,
                                    dbpropertytype property_type,
                                    bool need_out_of_mutex, uint64_t* value) {
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  auto cfd = cfh->cfd();

  if (!need_out_of_mutex) {
    mutexlock l(&mutex_);
    return cfd->internal_stats()->getintproperty(property_type, value, this);
  } else {
    superversion* sv = getandrefsuperversion(cfd);

    bool ret = cfd->internal_stats()->getintpropertyoutofmutex(
        property_type, sv->current, value);

    returnandcleanupsuperversion(cfd, sv);

    return ret;
  }
}

superversion* dbimpl::getandrefsuperversion(columnfamilydata* cfd) {
  // todo(ljin): consider using getreferencedsuperversion() directly
  if (likely(options_.allow_thread_local)) {
    return cfd->getthreadlocalsuperversion(&mutex_);
  } else {
    mutexlock l(&mutex_);
    return cfd->getsuperversion()->ref();
  }
}

void dbimpl::returnandcleanupsuperversion(columnfamilydata* cfd,
                                          superversion* sv) {
  bool unref_sv = true;
  if (likely(options_.allow_thread_local)) {
    unref_sv = !cfd->returnthreadlocalsuperversion(sv);
  }

  if (unref_sv) {
    // release superversion
    if (sv->unref()) {
      {
        mutexlock l(&mutex_);
        sv->cleanup();
      }
      delete sv;
      recordtick(stats_, number_superversion_cleanups);
    }
    recordtick(stats_, number_superversion_releases);
  }
}

void dbimpl::getapproximatesizes(columnfamilyhandle* column_family,
                                 const range* range, int n, uint64_t* sizes) {
  // todo(opt): better implementation
  version* v;
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  auto cfd = cfh->cfd();
  {
    mutexlock l(&mutex_);
    v = cfd->current();
    v->ref();
  }

  for (int i = 0; i < n; i++) {
    // convert user_key into a corresponding internal key.
    internalkey k1(range[i].start, kmaxsequencenumber, kvaluetypeforseek);
    internalkey k2(range[i].limit, kmaxsequencenumber, kvaluetypeforseek);
    uint64_t start = versions_->approximateoffsetof(v, k1);
    uint64_t limit = versions_->approximateoffsetof(v, k2);
    sizes[i] = (limit >= start ? limit - start : 0);
  }

  {
    mutexlock l(&mutex_);
    v->unref();
  }
}

inline void dbimpl::delayloggingandreset() {
  if (delayed_writes_ > 0) {
    log(options_.info_log, "delayed %d write...\n", delayed_writes_ );
    delayed_writes_ = 0;
  }
}

#ifndef rocksdb_lite
status dbimpl::getupdatessince(
    sequencenumber seq, unique_ptr<transactionlogiterator>* iter,
    const transactionlogiterator::readoptions& read_options) {

  recordtick(stats_, get_updates_since_calls);
  if (seq > versions_->lastsequence()) {
    return status::notfound("requested sequence not yet written in the db");
  }
  //  get all sorted wal files.
  //  do binary search and open files and find the seq number.

  std::unique_ptr<vectorlogptr> wal_files(new vectorlogptr);
  status s = getsortedwalfiles(*wal_files);
  if (!s.ok()) {
    return s;
  }

  s = retainprobablewalfiles(*wal_files, seq);
  if (!s.ok()) {
    return s;
  }
  iter->reset(new transactionlogiteratorimpl(options_.wal_dir, &options_,
                                             read_options, storage_options_,
                                             seq, std::move(wal_files), this));
  return (*iter)->status();
}

status dbimpl::deletefile(std::string name) {
  uint64_t number;
  filetype type;
  walfiletype log_type;
  if (!parsefilename(name, &number, &type, &log_type) ||
      (type != ktablefile && type != klogfile)) {
    log(options_.info_log, "deletefile %s failed.\n", name.c_str());
    return status::invalidargument("invalid file name");
  }

  status status;
  if (type == klogfile) {
    // only allow deleting archived log files
    if (log_type != karchivedlogfile) {
      log(options_.info_log, "deletefile %s failed - not archived log.\n",
          name.c_str());
      return status::notsupported("delete only supported for archived logs");
    }
    status = env_->deletefile(options_.wal_dir + "/" + name.c_str());
    if (!status.ok()) {
      log(options_.info_log, "deletefile %s failed -- %s.\n",
          name.c_str(), status.tostring().c_str());
    }
    return status;
  }

  int level;
  filemetadata* metadata;
  columnfamilydata* cfd;
  versionedit edit;
  deletionstate deletion_state(true);
  {
    mutexlock l(&mutex_);
    status = versions_->getmetadataforfile(number, &level, &metadata, &cfd);
    if (!status.ok()) {
      log(options_.info_log, "deletefile %s failed. file not found\n",
                             name.c_str());
      return status::invalidargument("file not found");
    }
    assert((level > 0) && (level < cfd->numberlevels()));

    // if the file is being compacted no need to delete.
    if (metadata->being_compacted) {
      log(options_.info_log,
          "deletefile %s skipped. file about to be compacted\n", name.c_str());
      return status::ok();
    }

    // only the files in the last level can be deleted externally.
    // this is to make sure that any deletion tombstones are not
    // lost. check that the level passed is the last level.
    for (int i = level + 1; i < cfd->numberlevels(); i++) {
      if (cfd->current()->numlevelfiles(i) != 0) {
        log(options_.info_log,
            "deletefile %s failed. file not in last level\n", name.c_str());
        return status::invalidargument("file not in last level");
      }
    }
    edit.deletefile(level, number);
    status = versions_->logandapply(cfd, &edit, &mutex_, db_directory_.get());
    if (status.ok()) {
      installsuperversion(cfd, deletion_state);
    }
    findobsoletefiles(deletion_state, false);
  } // lock released here
  logflush(options_.info_log);
  // remove files outside the db-lock
  if (deletion_state.havesomethingtodelete()) {
    purgeobsoletefiles(deletion_state);
  }
  {
    mutexlock l(&mutex_);
    // schedule flush if file deletion means we freed the space for flushes to
    // continue
    maybescheduleflushorcompaction();
  }
  return status;
}

void dbimpl::getlivefilesmetadata(std::vector<livefilemetadata>* metadata) {
  mutexlock l(&mutex_);
  versions_->getlivefilesmetadata(metadata);
}
#endif  // rocksdb_lite

status dbimpl::checkconsistency() {
  mutex_.assertheld();
  std::vector<livefilemetadata> metadata;
  versions_->getlivefilesmetadata(&metadata);

  std::string corruption_messages;
  for (const auto& md : metadata) {
    std::string file_path = md.db_path + "/" + md.name;

    uint64_t fsize = 0;
    status s = env_->getfilesize(file_path, &fsize);
    if (!s.ok()) {
      corruption_messages +=
          "can't access " + md.name + ": " + s.tostring() + "\n";
    } else if (fsize != md.size) {
      corruption_messages += "sst file size mismatch: " + file_path +
                             ". size recorded in manifest " +
                             std::to_string(md.size) + ", actual size " +
                             std::to_string(fsize) + "\n";
    }
  }
  if (corruption_messages.size() == 0) {
    return status::ok();
  } else {
    return status::corruption(corruption_messages);
  }
}

status dbimpl::getdbidentity(std::string& identity) {
  std::string idfilename = identityfilename(dbname_);
  unique_ptr<sequentialfile> idfile;
  const envoptions soptions;
  status s = env_->newsequentialfile(idfilename, &idfile, soptions);
  if (!s.ok()) {
    return s;
  }
  uint64_t file_size;
  s = env_->getfilesize(idfilename, &file_size);
  if (!s.ok()) {
    return s;
  }
  char buffer[file_size];
  slice id;
  s = idfile->read(file_size, &id, buffer);
  if (!s.ok()) {
    return s;
  }
  identity.assign(id.tostring());
  // if last character is '\n' remove it from identity
  if (identity.size() > 0 && identity.back() == '\n') {
    identity.pop_back();
  }
  return s;
}

// default implementations of convenience methods that subclasses of db
// can call if they wish
status db::put(const writeoptions& opt, columnfamilyhandle* column_family,
               const slice& key, const slice& value) {
  // pre-allocate size of write batch conservatively.
  // 8 bytes are taken by header, 4 bytes for count, 1 byte for type,
  // and we allocate 11 extra bytes for key length, as well as value length.
  writebatch batch(key.size() + value.size() + 24);
  batch.put(column_family, key, value);
  return write(opt, &batch);
}

status db::delete(const writeoptions& opt, columnfamilyhandle* column_family,
                  const slice& key) {
  writebatch batch;
  batch.delete(column_family, key);
  return write(opt, &batch);
}

status db::merge(const writeoptions& opt, columnfamilyhandle* column_family,
                 const slice& key, const slice& value) {
  writebatch batch;
  batch.merge(column_family, key, value);
  return write(opt, &batch);
}

// default implementation -- returns not supported status
status db::createcolumnfamily(const columnfamilyoptions& options,
                              const std::string& column_family_name,
                              columnfamilyhandle** handle) {
  return status::notsupported("");
}
status db::dropcolumnfamily(columnfamilyhandle* column_family) {
  return status::notsupported("");
}

db::~db() { }

status db::open(const options& options, const std::string& dbname, db** dbptr) {
  dboptions db_options(options);
  columnfamilyoptions cf_options(options);
  std::vector<columnfamilydescriptor> column_families;
  column_families.push_back(
      columnfamilydescriptor(kdefaultcolumnfamilyname, cf_options));
  std::vector<columnfamilyhandle*> handles;
  status s = db::open(db_options, dbname, column_families, &handles, dbptr);
  if (s.ok()) {
    assert(handles.size() == 1);
    // i can delete the handle since dbimpl is always holding a reference to
    // default column family
    delete handles[0];
  }
  return s;
}

status db::open(const dboptions& db_options, const std::string& dbname,
                const std::vector<columnfamilydescriptor>& column_families,
                std::vector<columnfamilyhandle*>* handles, db** dbptr) {
  status s = sanitizedboptionsbycfoptions(&db_options, column_families);
  if (!s.ok()) {
    return s;
  }
  if (db_options.db_paths.size() > 1) {
    for (auto& cfd : column_families) {
      if (cfd.options.compaction_style != kcompactionstyleuniversal) {
        return status::notsupported(
            "more than one db paths are only supported in "
            "universal compaction style. ");
      }
    }

    if (db_options.db_paths.size() > 4) {
      return status::notsupported(
        "more than four db paths are not supported yet. ");
    }
  }

  *dbptr = nullptr;
  handles->clear();

  size_t max_write_buffer_size = 0;
  for (auto cf : column_families) {
    max_write_buffer_size =
        std::max(max_write_buffer_size, cf.options.write_buffer_size);
  }

  dbimpl* impl = new dbimpl(db_options, dbname);
  s = impl->env_->createdirifmissing(impl->options_.wal_dir);
  if (s.ok()) {
    for (auto db_path : impl->options_.db_paths) {
      s = impl->env_->createdirifmissing(db_path.path);
      if (!s.ok()) {
        break;
      }
    }
  }

  if (!s.ok()) {
    delete impl;
    return s;
  }

  s = impl->createarchivaldirectory();
  if (!s.ok()) {
    delete impl;
    return s;
  }
  impl->mutex_.lock();
  // handles create_if_missing, error_if_exists
  s = impl->recover(column_families);
  if (s.ok()) {
    uint64_t new_log_number = impl->versions_->newfilenumber();
    unique_ptr<writablefile> lfile;
    envoptions soptions(db_options);
    s = impl->options_.env->newwritablefile(
        logfilename(impl->options_.wal_dir, new_log_number), &lfile,
        impl->options_.env->optimizeforlogwrite(soptions));
    if (s.ok()) {
      lfile->setpreallocationblocksize(1.1 * max_write_buffer_size);
      impl->logfile_number_ = new_log_number;
      impl->log_.reset(new log::writer(std::move(lfile)));

      // set column family handles
      for (auto cf : column_families) {
        auto cfd =
            impl->versions_->getcolumnfamilyset()->getcolumnfamily(cf.name);
        if (cfd != nullptr) {
          handles->push_back(
              new columnfamilyhandleimpl(cfd, impl, &impl->mutex_));
        } else {
          if (db_options.create_missing_column_families) {
            // missing column family, create it
            columnfamilyhandle* handle;
            impl->mutex_.unlock();
            s = impl->createcolumnfamily(cf.options, cf.name, &handle);
            impl->mutex_.lock();
            if (s.ok()) {
              handles->push_back(handle);
            } else {
              break;
            }
          } else {
            s = status::invalidargument("column family not found: ", cf.name);
            break;
          }
        }
      }
    }
    if (s.ok()) {
      for (auto cfd : *impl->versions_->getcolumnfamilyset()) {
        delete cfd->installsuperversion(new superversion(), &impl->mutex_);
      }
      impl->alive_log_files_.push_back(
          dbimpl::logfilenumbersize(impl->logfile_number_));
      impl->deleteobsoletefiles();
      impl->maybescheduleflushorcompaction();
      s = impl->db_directory_->fsync();
    }
  }

  if (s.ok()) {
    for (auto cfd : *impl->versions_->getcolumnfamilyset()) {
      if (cfd->options()->compaction_style == kcompactionstyleuniversal ||
          cfd->options()->compaction_style == kcompactionstylefifo) {
        version* current = cfd->current();
        for (int i = 1; i < current->numberlevels(); ++i) {
          int num_files = current->numlevelfiles(i);
          if (num_files > 0) {
            s = status::invalidargument(
                "not all files are at level 0. cannot "
                "open with universal or fifo compaction style.");
            break;
          }
        }
      }
      if (cfd->options()->merge_operator != nullptr &&
          !cfd->mem()->ismergeoperatorsupported()) {
        s = status::invalidargument(
            "the memtable of column family %s does not support merge operator "
            "its options.merge_operator is non-null", cfd->getname().c_str());
      }
      if (!s.ok()) {
        break;
      }
    }
  }

  impl->mutex_.unlock();

  if (s.ok()) {
    impl->opened_successfully_ = true;
    *dbptr = impl;
  } else {
    for (auto h : *handles) {
      delete h;
    }
    handles->clear();
    delete impl;
  }
  return s;
}

status db::listcolumnfamilies(const dboptions& db_options,
                              const std::string& name,
                              std::vector<std::string>* column_families) {
  return versionset::listcolumnfamilies(column_families, name, db_options.env);
}

snapshot::~snapshot() {
}

status destroydb(const std::string& dbname, const options& options) {
  const internalkeycomparator comparator(options.comparator);
  const options& soptions(sanitizeoptions(dbname, &comparator, options));
  env* env = soptions.env;
  std::vector<std::string> filenames;
  std::vector<std::string> archivefiles;

  std::string archivedir = archivaldirectory(dbname);
  // ignore error in case directory does not exist
  env->getchildren(dbname, &filenames);

  if (dbname != soptions.wal_dir) {
    std::vector<std::string> logfilenames;
    env->getchildren(soptions.wal_dir, &logfilenames);
    filenames.insert(filenames.end(), logfilenames.begin(), logfilenames.end());
    archivedir = archivaldirectory(soptions.wal_dir);
  }

  if (filenames.empty()) {
    return status::ok();
  }

  filelock* lock;
  const std::string lockname = lockfilename(dbname);
  status result = env->lockfile(lockname, &lock);
  if (result.ok()) {
    uint64_t number;
    filetype type;
    infologprefix info_log_prefix(!options.db_log_dir.empty(), dbname);
    for (size_t i = 0; i < filenames.size(); i++) {
      if (parsefilename(filenames[i], &number, info_log_prefix.prefix, &type) &&
          type != kdblockfile) {  // lock file will be deleted at end
        status del;
        if (type == kmetadatabase) {
          del = destroydb(dbname + "/" + filenames[i], options);
        } else if (type == klogfile) {
          del = env->deletefile(soptions.wal_dir + "/" + filenames[i]);
        } else {
          del = env->deletefile(dbname + "/" + filenames[i]);
        }
        if (result.ok() && !del.ok()) {
          result = del;
        }
      }
    }

    for (auto& db_path : options.db_paths) {
      env->getchildren(db_path.path, &filenames);
      uint64_t number;
      filetype type;
      for (size_t i = 0; i < filenames.size(); i++) {
        if (parsefilename(filenames[i], &number, &type) &&
            type == ktablefile) {  // lock file will be deleted at end
          status del = env->deletefile(db_path.path + "/" + filenames[i]);
          if (result.ok() && !del.ok()) {
            result = del;
          }
        }
      }
    }

    env->getchildren(archivedir, &archivefiles);
    // delete archival files.
    for (size_t i = 0; i < archivefiles.size(); ++i) {
      if (parsefilename(archivefiles[i], &number, &type) &&
          type == klogfile) {
        status del = env->deletefile(archivedir + "/" + archivefiles[i]);
        if (result.ok() && !del.ok()) {
          result = del;
        }
      }
    }
    // ignore case where no archival directory is present.
    env->deletedir(archivedir);

    env->unlockfile(lock);  // ignore error since state is already gone
    env->deletefile(lockname);
    env->deletedir(dbname);  // ignore error in case dir contains other files
    env->deletedir(soptions.wal_dir);
  }
  return result;
}

//
// a global method that can dump out the build version
void dumpleveldbbuildversion(logger * log) {
#if !defined(ios_cross_compile)
  // if we compile with xcode, we don't run build_detect_vesion, so we don't generate util/build_version.cc
  log(log, "git sha %s", rocksdb_build_git_sha);
  log(log, "compile time %s %s",
      rocksdb_build_compile_time, rocksdb_build_compile_date);
#endif
}

}  // namespace rocksdb
