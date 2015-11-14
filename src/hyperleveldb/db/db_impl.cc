// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db_impl.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <vector>

#include "builder.h"
#include "db_iter.h"
#include "dbformat.h"
#include "filename.h"
#include "log_reader.h"
#include "log_writer.h"
#include "memtable.h"
#include "replay_iterator.h"
#include "table_cache.h"
#include "version_set.h"
#include "write_batch_internal.h"
#include "../hyperleveldb/db.h"
#include "../hyperleveldb/env.h"
#include "../hyperleveldb/replay_iterator.h"
#include "../hyperleveldb/status.h"
#include "../hyperleveldb/table.h"
#include "../hyperleveldb/table_builder.h"
#include "../port/port.h"
#include "../table/block.h"
#include "../table/merger.h"
#include "../table/two_level_iterator.h"
#include "../util/coding.h"
#include "../util/logging.h"
#include "../util/mutexlock.h"

namespace hyperleveldb {

const int kstraightreads = 50;

const int knumnontablecachefiles = 10;

struct dbimpl::compactionstate {
  compaction* const compaction;

  // sequence numbers < smallest_snapshot are not significant since we
  // will never have to service a snapshot below smallest_snapshot.
  // therefore if we have seen a sequence number s <= smallest_snapshot,
  // we can drop all entries for the same key with sequence numbers < s.
  sequencenumber smallest_snapshot;

  // files produced by compaction
  struct output {
    uint64_t number;
    uint64_t file_size;
    internalkey smallest, largest;
  };
  std::vector<output> outputs;

  // state kept for output being generated
  writablefile* outfile;
  tablebuilder* builder;

  uint64_t total_bytes;

  output* current_output() { return &outputs[outputs.size()-1]; }

  explicit compactionstate(compaction* c)
      : compaction(c),
        outfile(null),
        builder(null),
        total_bytes(0) {
  }
};

// fix user-supplied options to be reasonable
template <class t,class v>
static void cliptorange(t* ptr, v minvalue, v maxvalue) {
  if (static_cast<v>(*ptr) > maxvalue) *ptr = maxvalue;
  if (static_cast<v>(*ptr) < minvalue) *ptr = minvalue;
}
options sanitizeoptions(const std::string& dbname,
                        const internalkeycomparator* icmp,
                        const internalfilterpolicy* ipolicy,
                        const options& src) {
  options result = src;
  result.comparator = icmp;
  result.filter_policy = (src.filter_policy != null) ? ipolicy : null;
  cliptorange(&result.max_open_files,    64 + knumnontablecachefiles, 50000);
  cliptorange(&result.write_buffer_size, 64<<10,                      1<<30);
  cliptorange(&result.block_size,        1<<10,                       4<<20);
  if (result.info_log == null) {
    // open a log file in the same directory as the db
    src.env->createdir(dbname);  // in case it does not exist
    src.env->renamefile(infologfilename(dbname), oldinfologfilename(dbname));
    status s = src.env->newlogger(infologfilename(dbname), &result.info_log);
    if (!s.ok()) {
      // no place suitable for logging
      result.info_log = null;
    }
  }
  if (result.block_cache == null) {
    result.block_cache = newlrucache(8 << 20);
  }
  return result;
}

dbimpl::dbimpl(const options& raw_options, const std::string& dbname)
    : env_(raw_options.env),
      internal_comparator_(raw_options.comparator),
      internal_filter_policy_(raw_options.filter_policy),
      options_(sanitizeoptions(dbname, &internal_comparator_,
                               &internal_filter_policy_, raw_options)),
      owns_info_log_(options_.info_log != raw_options.info_log),
      owns_cache_(options_.block_cache != raw_options.block_cache),
      dbname_(dbname),
      db_lock_(null),
      shutting_down_(null),
      mem_(new memtable(internal_comparator_)),
      imm_(null),
      logfile_(),
      logfile_number_(0),
      log_(),
      seed_(0),
      writers_lower_(0),
      writers_upper_(0),
      bg_fg_cv_(&mutex_),
      allow_background_activity_(false),
      num_bg_threads_(0),
      bg_compaction_cv_(&mutex_),
      bg_memtable_cv_(&mutex_),
      bg_optimistic_trip_(false),
      bg_optimistic_cv_(&mutex_),
      bg_log_cv_(&mutex_),
      bg_log_occupied_(false),
      manual_compaction_(null),
      manual_garbage_cutoff_(raw_options.manual_garbage_collection ?
                             sequencenumber(0) : kmaxsequencenumber),
      straight_reads_(0),
      backup_cv_(&mutex_),
      backup_in_progress_(),
      backup_deferred_delete_(),
      consecutive_compaction_errors_(0) {
  mutex_.lock();
  mem_->ref();
  has_imm_.release_store(null);
  backup_in_progress_.release_store(null);
  env_->startthread(&dbimpl::compactmemtablewrapper, this);
  env_->startthread(&dbimpl::compactoptimisticwrapper, this);
  env_->startthread(&dbimpl::compactlevelwrapper, this);
  num_bg_threads_ = 3;

  // reserve ten files or so for other uses and give the rest to tablecache.
  const int table_cache_size = options_.max_open_files - knumnontablecachefiles;
  table_cache_ = new tablecache(dbname_, &options_, table_cache_size);
  versions_ = new versionset(dbname_, &options_, table_cache_,
                             &internal_comparator_);

  for (int i = 0; i < config::knumlevels; ++i) {
    levels_locked_[i] = false;
  }
  mutex_.unlock();
}

dbimpl::~dbimpl() {
  // wait for background work to finish
  mutex_.lock();
  shutting_down_.release_store(this);  // any non-null value is ok
  bg_optimistic_cv_.signalall();
  bg_compaction_cv_.signalall();
  bg_memtable_cv_.signalall();
  while (num_bg_threads_ > 0) {
    bg_fg_cv_.wait();
  }
  mutex_.unlock();

  if (db_lock_ != null) {
    env_->unlockfile(db_lock_);
  }

  delete versions_;
  if (mem_ != null) mem_->unref();
  if (imm_ != null) imm_->unref();
  log_.reset();
  logfile_.reset();
  delete table_cache_;

  if (owns_info_log_) {
    delete options_.info_log;
  }
  if (owns_cache_) {
    delete options_.block_cache;
  }
}

status dbimpl::newdb() {
  versionedit new_db;
  new_db.setcomparatorname(user_comparator()->name());
  new_db.setlognumber(0);
  new_db.setnextfile(2);
  new_db.setlastsequence(0);

  const std::string manifest = descriptorfilename(dbname_, 1);
  writablefile* file;
  status s = env_->newwritablefile(manifest, &file);
  if (!s.ok()) {
    return s;
  }
  {
    log::writer log(file);
    std::string record;
    new_db.encodeto(&record);
    s = log.addrecord(record);
    if (s.ok()) {
      s = file->close();
    }
  }
  delete file;
  if (s.ok()) {
    // make "current" file that points to the new manifest file.
    s = setcurrentfile(env_, dbname_, 1);
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

void dbimpl::deleteobsoletefiles() {
  // defer if there's background activity
  mutex_.assertheld();
  if (backup_in_progress_.acquire_load() != null) {
    backup_deferred_delete_ = true;
    return;
  }

  // if you ever release mutex_ in this function, you'll need to do more work in
  // livebackup

  // make a set of all of the live files
  std::set<uint64_t> live = pending_outputs_;
  versions_->addlivefiles(&live);

  std::vector<std::string> filenames;
  env_->getchildren(dbname_, &filenames); // ignoring errors on purpose
  uint64_t number;
  filetype type;
  for (size_t i = 0; i < filenames.size(); i++) {
    if (parsefilename(filenames[i], &number, &type)) {
      bool keep = true;
      switch (type) {
        case klogfile:
          keep = ((number >= versions_->lognumber()) ||
                  (number == versions_->prevlognumber()));
          break;
        case kdescriptorfile:
          // keep my manifest file, and any newer incarnations'
          // (in case there is a race that allows other incarnations)
          keep = (number >= versions_->manifestfilenumber());
          break;
        case ktablefile:
          keep = (live.find(number) != live.end());
          break;
        case ktempfile:
          // any temp files that are currently being written to must
          // be recorded in pending_outputs_, which is inserted into "live"
          keep = (live.find(number) != live.end());
          break;
        case kcurrentfile:
        case kdblockfile:
        case kinfologfile:
          keep = true;
          break;
      }

      if (!keep) {
        if (type == ktablefile) {
          table_cache_->evict(number);
        }
        log(options_.info_log, "delete type=%d #%lld\n",
            int(type),
            static_cast<unsigned long long>(number));
        env_->deletefile(dbname_ + "/" + filenames[i]);
      }
    }
  }
}

status dbimpl::recover(versionedit* edit) {
  mutex_.assertheld();

  // ignore error from createdir since the creation of the db is
  // committed only when the descriptor is created, and this directory
  // may already exist from a previous failed creation attempt.
  env_->createdir(dbname_);
  assert(db_lock_ == null);
  status s = env_->lockfile(lockfilename(dbname_), &db_lock_);
  if (!s.ok()) {
    return s;
  }

  if (!env_->fileexists(currentfilename(dbname_))) {
    if (options_.create_if_missing) {
      s = newdb();
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

  s = versions_->recover();
  if (s.ok()) {
    sequencenumber max_sequence(0);

    // recover from all newer log files than the ones named in the
    // descriptor (new log files may have been added by the previous
    // incarnation without registering them in the descriptor).
    //
    // note that prevlognumber() is no longer used, but we pay
    // attention to it in case we are recovering a database
    // produced by an older version of leveldb.
    const uint64_t min_log = versions_->lognumber();
    const uint64_t prev_log = versions_->prevlognumber();
    std::vector<std::string> filenames;
    s = env_->getchildren(dbname_, &filenames);
    if (!s.ok()) {
      return s;
    }
    std::set<uint64_t> expected;
    versions_->addlivefiles(&expected);
    uint64_t number;
    filetype type;
    std::vector<uint64_t> logs;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (parsefilename(filenames[i], &number, &type)) {
        expected.erase(number);
        if (type == klogfile && ((number >= min_log) || (number == prev_log)))
          logs.push_back(number);
      }
    }
    if (!expected.empty()) {
      char buf[50];
      snprintf(buf, sizeof(buf), "%d missing files; e.g.",
               static_cast<int>(expected.size()));
      return status::corruption(buf, tablefilename(dbname_, *(expected.begin())));
    }

    // recover in the order in which the logs were generated
    std::sort(logs.begin(), logs.end());
    for (size_t i = 0; i < logs.size(); i++) {
      s = recoverlogfile(logs[i], edit, &max_sequence);

      // the previous incarnation may not have written any manifest
      // records after allocating this log number.  so we manually
      // update the file number allocation counter in versionset.
      versions_->markfilenumberused(logs[i]);
    }

    if (s.ok()) {
      if (versions_->lastsequence() < max_sequence) {
        versions_->setlastsequence(max_sequence);
      }
    }
  }

  return s;
}

status dbimpl::recoverlogfile(uint64_t log_number,
                              versionedit* edit,
                              sequencenumber* max_sequence) {
  struct logreporter : public log::reader::reporter {
    env* env;
    logger* info_log;
    const char* fname;
    status* status;  // null if options_.paranoid_checks==false
    virtual void corruption(size_t bytes, const status& s) {
      log(info_log, "%s%s: dropping %d bytes; %s",
          (this->status == null ? "(ignoring error) " : ""),
          fname, static_cast<int>(bytes), s.tostring().c_str());
      if (this->status != null && this->status->ok()) *this->status = s;
    }
  };

  mutex_.assertheld();

  // open the log file
  std::string fname = logfilename(dbname_, log_number);
  sequentialfile* file;
  status status = env_->newsequentialfile(fname, &file);
  if (!status.ok()) {
    maybeignoreerror(&status);
    return status;
  }

  // create the log reader.
  logreporter reporter;
  reporter.env = env_;
  reporter.info_log = options_.info_log;
  reporter.fname = fname.c_str();
  reporter.status = (options_.paranoid_checks ? &status : null);
  // we intentially make log::reader do checksumming even if
  // paranoid_checks==false so that corruptions cause entire commits
  // to be skipped instead of propagating bad information (like overly
  // large sequence numbers).
  log::reader reader(file, &reporter, true/*checksum*/,
                     0/*initial_offset*/);
  log(options_.info_log, "recovering log #%llu",
      (unsigned long long) log_number);

  // read all the records and add to a memtable
  std::string scratch;
  slice record;
  writebatch batch;
  memtable* mem = null;
  while (reader.readrecord(&record, &scratch) &&
         status.ok()) {
    if (record.size() < 12) {
      reporter.corruption(
          record.size(), status::corruption("log record too small"));
      continue;
    }
    writebatchinternal::setcontents(&batch, record);

    if (mem == null) {
      mem = new memtable(internal_comparator_);
      mem->ref();
    }
    status = writebatchinternal::insertinto(&batch, mem);
    maybeignoreerror(&status);
    if (!status.ok()) {
      break;
    }
    const sequencenumber last_seq =
        writebatchinternal::sequence(&batch) +
        writebatchinternal::count(&batch) - 1;
    if (last_seq > *max_sequence) {
      *max_sequence = last_seq;
    }

    if (mem->approximatememoryusage() > options_.write_buffer_size) {
      status = writelevel0table(mem, edit, null, null);
      if (!status.ok()) {
        // reflect errors immediately so that conditions like full
        // file-systems cause the db::open() to fail.
        break;
      }
      mem->unref();
      mem = null;
    }
  }

  if (status.ok() && mem != null) {
    status = writelevel0table(mem, edit, null, null);
    // reflect errors immediately so that conditions like full
    // file-systems cause the db::open() to fail.
  }

  if (mem != null) mem->unref();
  delete file;
  return status;
}

status dbimpl::writelevel0table(memtable* mem, versionedit* edit,
                                version* base, uint64_t* number) {
  mutex_.assertheld();
  const uint64_t start_micros = env_->nowmicros();
  filemetadata meta;
  meta.number = versions_->newfilenumber();
  if (number) {
    *number = meta.number;
  }
  pending_outputs_.insert(meta.number);
  iterator* iter = mem->newiterator();
  log(options_.info_log, "level-0 table #%llu: started",
      (unsigned long long) meta.number);

  status s;
  {
    mutex_.unlock();
    s = buildtable(dbname_, env_, options_, table_cache_, iter, &meta);
    mutex_.lock();
  }

  log(options_.info_log, "level-0 table #%llu: %lld bytes %s",
      (unsigned long long) meta.number,
      (unsigned long long) meta.file_size,
      s.tostring().c_str());
  delete iter;

  // note that if file_size is zero, the file has been deleted and
  // should not be added to the manifest.
  int level = 0;
  if (s.ok() && meta.file_size > 0) {
    const slice min_user_key = meta.smallest.user_key();
    const slice max_user_key = meta.largest.user_key();
    if (base != null) {
      level = base->picklevelformemtableoutput(min_user_key, max_user_key);
      while (level > 0 && levels_locked_[level]) {
        --level;
      }
    }
    edit->addfile(level, meta.number, meta.file_size,
                  meta.smallest, meta.largest);
  }

  compactionstats stats;
  stats.micros = env_->nowmicros() - start_micros;
  stats.bytes_written = meta.file_size;
  stats_[level].add(stats);
  return s;
}

void dbimpl::compactmemtablethread() {
  mutexlock l(&mutex_);
  while (!shutting_down_.acquire_load() && !allow_background_activity_) {
    bg_memtable_cv_.wait();
  }
  while (!shutting_down_.acquire_load()) {
    while (!shutting_down_.acquire_load() && imm_ == null) {
      bg_memtable_cv_.wait();
    }
    if (shutting_down_.acquire_load()) {
      break;
    }

    // save the contents of the memtable as a new table
    versionedit edit;
    version* base = versions_->current();
    base->ref();
    uint64_t number;
    status s = writelevel0table(imm_, &edit, base, &number);
    base->unref(); base = null;

    if (s.ok() && shutting_down_.acquire_load()) {
      s = status::ioerror("deleting db during memtable compaction");
    }

    // replace immutable memtable with the generated table
    if (s.ok()) {
      edit.setprevlognumber(0);
      edit.setlognumber(logfile_number_);  // earlier logs no longer needed
      s = versions_->logandapply(&edit, &mutex_, &bg_log_cv_, &bg_log_occupied_);
    }

    pending_outputs_.erase(number);

    if (s.ok()) {
      // commit to the new state
      imm_->unref();
      imm_ = null;
      has_imm_.release_store(null);
      bg_fg_cv_.signalall();
      bg_compaction_cv_.signal();
      deleteobsoletefiles();
    }

    if (!shutting_down_.acquire_load() && !s.ok()) {
      // wait a little bit before retrying background compaction in
      // case this is an environmental problem and we do not want to
      // chew up resources for failed compactions for the duration of
      // the problem.
      bg_fg_cv_.signalall();  // in case a waiter can proceed despite the error
      log(options_.info_log, "waiting after memtable compaction error: %s",
          s.tostring().c_str());
      mutex_.unlock();
      env_->sleepformicroseconds(1000000);
      mutex_.lock();
    }

    assert(config::kl0_slowdownwritestrigger > 0);
    if (versions_->numlevelfiles(0) >= config::kl0_slowdownwritestrigger - 1) {
      bg_optimistic_trip_ = true;
      bg_optimistic_cv_.signal();
    }
  }
  log(options_.info_log, "cleaning up compactmemtablethread");
  num_bg_threads_ -= 1;
  bg_fg_cv_.signalall();
}

void dbimpl::compactrange(const slice* begin, const slice* end) {
  int max_level_with_files = 1;
  {
    mutexlock l(&mutex_);
    version* base = versions_->current();
    for (int level = 1; level < config::knumlevels; level++) {
      if (base->overlapinlevel(level, begin, end)) {
        max_level_with_files = level;
      }
    }
  }
  test_compactmemtable(); // todo(sanjay): skip if memtable does not overlap
  for (int level = 0; level < max_level_with_files; level++) {
    test_compactrange(level, begin, end);
  }
}

void dbimpl::test_compactrange(int level, const slice* begin,const slice* end) {
  assert(level >= 0);
  assert(level + 1 < config::knumlevels);

  internalkey begin_storage, end_storage;

  manualcompaction manual;
  manual.level = level;
  manual.done = false;
  if (begin == null) {
    manual.begin = null;
  } else {
    begin_storage = internalkey(*begin, kmaxsequencenumber, kvaluetypeforseek);
    manual.begin = &begin_storage;
  }
  if (end == null) {
    manual.end = null;
  } else {
    end_storage = internalkey(*end, 0, static_cast<valuetype>(0));
    manual.end = &end_storage;
  }

  mutexlock l(&mutex_);
  while (!manual.done) {
    while (manual_compaction_ != null) {
      bg_fg_cv_.wait();
    }
    manual_compaction_ = &manual;
    bg_compaction_cv_.signal();
    bg_memtable_cv_.signal();
    while (manual_compaction_ == &manual) {
      bg_fg_cv_.wait();
    }
  }
}

status dbimpl::test_compactmemtable() {
  // null batch means just wait for earlier writes to be done
  status s = write(writeoptions(), null);
  if (s.ok()) {
    // wait until the compaction completes
    mutexlock l(&mutex_);
    while (imm_ != null && bg_error_.ok()) {
      bg_fg_cv_.wait();
    }
    if (imm_ != null) {
      s = bg_error_;
    }
  }
  return s;
}

void dbimpl::compactlevelthread() {
  mutexlock l(&mutex_);
  while (!shutting_down_.acquire_load() && !allow_background_activity_) {
    bg_compaction_cv_.wait();
  }
  while (!shutting_down_.acquire_load()) {
    while (!shutting_down_.acquire_load() &&
           manual_compaction_ == null &&
           !versions_->needscompaction(levels_locked_, straight_reads_ > kstraightreads)) {
      bg_compaction_cv_.wait();
    }
    if (shutting_down_.acquire_load()) {
      break;
    }

    assert(manual_compaction_ == null || num_bg_threads_ == 3);
    status s = backgroundcompaction();
    bg_fg_cv_.signalall(); // before the backoff in case a waiter 
                           // can proceed despite the error

    if (s.ok()) {
      // success
      consecutive_compaction_errors_ = 0;
    } else if (shutting_down_.acquire_load()) {
      // error most likely due to shutdown; do not wait
    } else {
      // wait a little bit before retrying background compaction in
      // case this is an environmental problem and we do not want to
      // chew up resources for failed compactions for the duration of
      // the problem.
      log(options_.info_log, "waiting after background compaction error: %s",
          s.tostring().c_str());
      mutex_.unlock();
      ++consecutive_compaction_errors_;
      int seconds_to_sleep = 1;
      for (int i = 0; i < 3 && i < consecutive_compaction_errors_ - 1; ++i) {
        seconds_to_sleep *= 2;
      }
      env_->sleepformicroseconds(seconds_to_sleep * 1000000);
      mutex_.lock();
    }
  }
  log(options_.info_log, "cleaning up compactlevelthread");
  num_bg_threads_ -= 1;
  bg_fg_cv_.signalall();
}

status dbimpl::backgroundcompaction() {
  mutex_.assertheld();
  compaction* c = null;
  bool is_manual = (manual_compaction_ != null);
  internalkey manual_end;
  if (is_manual) {
    manualcompaction* m = manual_compaction_;
    c = versions_->compactrange(m->level, m->begin, m->end);
    m->done = (c == null);
    if (c != null) {
      manual_end = c->input(0, c->num_input_files(0) - 1)->largest;
    }
    log(options_.info_log,
        "manual compaction at level-%d from %s .. %s; will stop at %s\n",
        m->level,
        (m->begin ? m->begin->debugstring().c_str() : "(begin)"),
        (m->end ? m->end->debugstring().c_str() : "(end)"),
        (m->done ? "(end)" : manual_end.debugstring().c_str()));
  } else {
    int level = versions_->pickcompactionlevel(levels_locked_, straight_reads_ > kstraightreads);
    if (level != config::knumlevels) {
      c = versions_->pickcompaction(versions_->current(), level);
    }
    if (c) {
      assert(!levels_locked_[c->level() + 0]);
      assert(!levels_locked_[c->level() + 1]);
      levels_locked_[c->level() + 0] = true;
      levels_locked_[c->level() + 1] = true;
    }
  }

  status status;

  if (c == null) {
    // nothing to do
  } else if (!is_manual && c->istrivialmove() && c->level() > 0) {
    // move file to next level
    for (size_t i = 0; i < c->num_input_files(0); ++i) {
      filemetadata* f = c->input(0, i);
      c->edit()->deletefile(c->level(), f->number);
      c->edit()->addfile(c->level() + 1, f->number, f->file_size,
                         f->smallest, f->largest);
    }
    status = versions_->logandapply(c->edit(), &mutex_, &bg_log_cv_, &bg_log_occupied_);
    versionset::levelsummarystorage tmp;
    for (size_t i = 0; i < c->num_input_files(0); ++i) {
      filemetadata* f = c->input(0, i);
      log(options_.info_log, "moved #%lld to level-%d %lld bytes %s: %s\n",
          static_cast<unsigned long long>(f->number),
          c->level() + 1,
          static_cast<unsigned long long>(f->file_size),
          status.tostring().c_str(),
          versions_->levelsummary(&tmp));
    }
  } else {
    compactionstate* compact = new compactionstate(c);
    status = docompactionwork(compact);
    cleanupcompaction(compact);
    c->releaseinputs();
    deleteobsoletefiles();
  }

  if (c) {
    levels_locked_[c->level() + 0] = false;
    levels_locked_[c->level() + 1] = false;
    delete c;
  }

  if (status.ok()) {
    // done
  } else if (shutting_down_.acquire_load()) {
    // ignore compaction errors found during shutting down
  } else {
    log(options_.info_log,
        "compaction error: %s", status.tostring().c_str());
    if (options_.paranoid_checks && bg_error_.ok()) {
      bg_error_ = status;
    }
  }

  if (is_manual) {
    manualcompaction* m = manual_compaction_;
    if (!status.ok()) {
      m->done = true;
    }
    if (!m->done) {
      // we only compacted part of the requested range.  update *m
      // to the range that is left to be compacted.
      m->tmp_storage = manual_end;
      m->begin = &m->tmp_storage;
    }
    manual_compaction_ = null;
  }
  return status;
}

void dbimpl::compactoptimisticthread() {
  mutexlock l(&mutex_);
  while (!shutting_down_.acquire_load() && !allow_background_activity_) {
    bg_optimistic_cv_.wait();
  }
  while (!shutting_down_.acquire_load()) {
    while (!shutting_down_.acquire_load() && !bg_optimistic_trip_) {
      bg_optimistic_cv_.wait();
    }
    if (shutting_down_.acquire_load()) {
      break;
    }
    bg_optimistic_trip_ = false;
    status s = optimisticcompaction();

    if (!shutting_down_.acquire_load() && !s.ok()) {
      // wait a little bit before retrying background compaction in
      // case this is an environmental problem and we do not want to
      // chew up resources for failed compactions for the duration of
      // the problem.
      log(options_.info_log, "waiting after optimistic compaction error: %s",
          s.tostring().c_str());
      mutex_.unlock();
      env_->sleepformicroseconds(1000000);
      mutex_.lock();
    }
  }
  log(options_.info_log, "cleaning up optimisticcompactthread");
  num_bg_threads_ -= 1;
  bg_fg_cv_.signalall();
}

status dbimpl::optimisticcompaction() {
  mutex_.assertheld();
  log(options_.info_log, "optimistic compaction started");
  bool did_compaction = true;
  uint64_t iters = 0;
  while (did_compaction) {
    ++iters;
    did_compaction = false;
    compaction* c = null;
    for (size_t level = 1; level + 1 < config::knumlevels; ++level) {
      if (levels_locked_[level] || levels_locked_[level + 1]) {
        continue;
      }
      compaction* tmp = versions_->pickcompaction(versions_->current(), level);
      if (tmp && tmp->istrivialmove()) {
        if (c) {
          delete c;
        }
        c = tmp;
        break;
      } else if (c && tmp && c->ratio() < tmp->ratio()) {
        delete c;
        c = tmp;
      } else if (!c) {
        c = tmp;
      } else {
        delete tmp;
      }
    }
    if (!c) {
      continue;
    }
    if (!c->istrivialmove() && c->ratio() < .90) {
      delete c;
      continue;
    }
    assert(!levels_locked_[c->level() + 0]);
    assert(!levels_locked_[c->level() + 1]);
    levels_locked_[c->level() + 0] = true;
    levels_locked_[c->level() + 1] = true;

    did_compaction = true;
    status status;

    if (c->istrivialmove() && c->level() > 0) {
      // move file to next level
      for (size_t i = 0; i < c->num_input_files(0); ++i) {
        filemetadata* f = c->input(0, i);
        c->edit()->deletefile(c->level(), f->number);
        c->edit()->addfile(c->level() + 1, f->number, f->file_size,
                           f->smallest, f->largest);
      }
      status = versions_->logandapply(c->edit(), &mutex_, &bg_log_cv_, &bg_log_occupied_);
      versionset::levelsummarystorage tmp;
      for (size_t i = 0; i < c->num_input_files(0); ++i) {
        filemetadata* f = c->input(0, i);
        log(options_.info_log, "moved #%lld to level-%d %lld bytes %s: %s\n",
            static_cast<unsigned long long>(f->number),
            c->level() + 1,
            static_cast<unsigned long long>(f->file_size),
            status.tostring().c_str(),
            versions_->levelsummary(&tmp));
      }
    } else {
      compactionstate* compact = new compactionstate(c);
      status = docompactionwork(compact);
      cleanupcompaction(compact);
      c->releaseinputs();
      deleteobsoletefiles();
    }

    levels_locked_[c->level() + 0] = false;
    levels_locked_[c->level() + 1] = false;
    delete c;

    if (status.ok()) {
      // done
    } else if (shutting_down_.acquire_load()) {
      // ignore compaction errors found during shutting down
      break;
    } else {
      log(options_.info_log,
          "compaction error: %s", status.tostring().c_str());
      if (options_.paranoid_checks && bg_error_.ok()) {
        bg_error_ = status;
      }
      break;
    }
  }
  log(options_.info_log, "optimistic compaction ended after %ld iterations", iters);
  return status::ok();
}

void dbimpl::cleanupcompaction(compactionstate* compact) {
  mutex_.assertheld();
  if (compact->builder != null) {
    // may happen if we get a shutdown call in the middle of compaction
    compact->builder->abandon();
    delete compact->builder;
  } else {
    assert(compact->outfile == null);
  }
  delete compact->outfile;
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    const compactionstate::output& out = compact->outputs[i];
    pending_outputs_.erase(out.number);
  }
  delete compact;
}

status dbimpl::opencompactionoutputfile(compactionstate* compact) {
  assert(compact != null);
  assert(compact->builder == null);
  uint64_t file_number;
  {
    mutex_.lock();
    file_number = versions_->newfilenumber();
    pending_outputs_.insert(file_number);
    compactionstate::output out;
    out.number = file_number;
    out.smallest.clear();
    out.largest.clear();
    compact->outputs.push_back(out);
    mutex_.unlock();
  }

  // make the output file
  std::string fname = tablefilename(dbname_, file_number);
  status s = env_->newwritablefile(fname, &compact->outfile);
  if (s.ok()) {
    compact->builder = new tablebuilder(options_, compact->outfile);
  }
  return s;
}

status dbimpl::finishcompactionoutputfile(compactionstate* compact,
                                          iterator* input) {
  assert(compact != null);
  assert(compact->outfile != null);
  assert(compact->builder != null);

  const uint64_t output_number = compact->current_output()->number;
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
  delete compact->builder;
  compact->builder = null;

  // finish and check for file errors
  if (s.ok()) {
    s = compact->outfile->sync();
  }
  if (s.ok()) {
    s = compact->outfile->close();
  }
  delete compact->outfile;
  compact->outfile = null;

  if (s.ok() && current_entries > 0) {
    // verify that the table is usable
    iterator* iter = table_cache_->newiterator(readoptions(),
                                               output_number,
                                               current_bytes);
    s = iter->status();
    delete iter;
    if (s.ok()) {
      log(options_.info_log,
          "generated table #%llu: %lld keys, %lld bytes",
          (unsigned long long) output_number,
          (unsigned long long) current_entries,
          (unsigned long long) current_bytes);
    }
  }
  return s;
}


status dbimpl::installcompactionresults(compactionstate* compact) {
  mutex_.assertheld();
  log(options_.info_log,  "compacted %d@%d + %d@%d files => %lld bytes",
      compact->compaction->num_input_files(0),
      compact->compaction->level(),
      compact->compaction->num_input_files(1),
      compact->compaction->level() + 1,
      static_cast<long long>(compact->total_bytes));

  // add compaction outputs
  compact->compaction->addinputdeletions(compact->compaction->edit());
  const int level = compact->compaction->level();
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    const compactionstate::output& out = compact->outputs[i];
    compact->compaction->edit()->addfile(
        level + 1,
        out.number, out.file_size, out.smallest, out.largest);
  }
  return versions_->logandapply(compact->compaction->edit(), &mutex_, &bg_log_cv_, &bg_log_occupied_);
}

status dbimpl::docompactionwork(compactionstate* compact) {
  const uint64_t start_micros = env_->nowmicros();
  int64_t imm_micros = 0;  // micros spent doing imm_ compactions

  log(options_.info_log,  "compacting %d@%d + %d@%d files",
      compact->compaction->num_input_files(0),
      compact->compaction->level(),
      compact->compaction->num_input_files(1),
      compact->compaction->level() + 1);

  assert(versions_->numlevelfiles(compact->compaction->level()) > 0);
  assert(compact->builder == null);
  assert(compact->outfile == null);
  if (snapshots_.empty()) {
    compact->smallest_snapshot = versions_->lastsequence();
  } else {
    compact->smallest_snapshot = snapshots_.oldest()->number_;
  }

  // release mutex while we're actually doing the compaction work
  mutex_.unlock();

  iterator* input = versions_->makeinputiterator(compact->compaction);
  input->seektofirst();
  status status;
  parsedinternalkey ikey;
  std::string current_user_key;
  bool has_current_user_key = false;
  sequencenumber last_sequence_for_key = kmaxsequencenumber;
  uint64_t i = 0;
  for (; input->valid() && !shutting_down_.acquire_load(); ) {
    slice key = input->key();
    // handle key/value, add to state, etc.
    bool drop = false;
    if (!parseinternalkey(key, &ikey)) {
      // do not hide error keys
      current_user_key.clear();
      has_current_user_key = false;
      last_sequence_for_key = kmaxsequencenumber;
    } else {
      if (!has_current_user_key ||
          user_comparator()->compare(ikey.user_key,
                                     slice(current_user_key)) != 0) {
        // first occurrence of this user key
        current_user_key.assign(ikey.user_key.data(), ikey.user_key.size());
        has_current_user_key = true;
        last_sequence_for_key = kmaxsequencenumber;
      }

      // just remember that last_sequence_for_key is decreasing over time, and
      // all of this makes sense.

      if (last_sequence_for_key <= compact->smallest_snapshot) {
        // hidden by an newer entry for same user key
        drop = true;    // (a)
      } else if (ikey.type == ktypedeletion &&
                 ikey.sequence <= compact->smallest_snapshot &&
                 compact->compaction->isbaselevelforkey(ikey.user_key)) {
        // for this user key:
        // (1) there is no data in higher levels
        // (2) data in lower levels will have larger sequence numbers
        // (3) data in layers that are being compacted here and have
        //     smaller sequence numbers will be dropped in the next
        //     few iterations of this loop (by rule (a) above).
        // therefore this deletion marker is obsolete and can be dropped.
        drop = true;
      }

      // if we're going to drop this key, and there was no previous version of
      // this key, and it was written at or after the garbage cutoff, we keep
      // it.
      if (drop &&
          last_sequence_for_key == kmaxsequencenumber  &&
          ikey.sequence >= manual_garbage_cutoff_) {
        drop = false;
      }

      last_sequence_for_key = ikey.sequence;
    }

    if (!drop) {
      // open output file if necessary
      if (compact->builder == null) {
        status = opencompactionoutputfile(compact);
        if (!status.ok()) {
          break;
        }
      }
      if (compact->builder->numentries() == 0) {
        compact->current_output()->smallest.decodefrom(key);
      }
      compact->current_output()->largest.decodefrom(key);
      compact->builder->add(key, input->value());

      // close output file if it is big enough
      if (compact->builder->filesize() >=
          compact->compaction->maxoutputfilesize()) {
        status = finishcompactionoutputfile(compact, input);
        if (!status.ok()) {
          break;
        }
      }
    }

    input->next();
  }

  if (status.ok() && shutting_down_.acquire_load()) {
    status = status::ioerror("deleting db during compaction");
  }
  if (status.ok() && compact->builder != null) {
    status = finishcompactionoutputfile(compact, input);
  }
  if (status.ok()) {
    status = input->status();
  }
  delete input;
  input = null;

  compactionstats stats;
  stats.micros = env_->nowmicros() - start_micros - imm_micros;
  for (int which = 0; which < 2; which++) {
    for (int i = 0; i < compact->compaction->num_input_files(which); i++) {
      stats.bytes_read += compact->compaction->input(which, i)->file_size;
    }
  }
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    stats.bytes_written += compact->outputs[i].file_size;
  }

  mutex_.lock();
  stats_[compact->compaction->level() + 1].add(stats);

  if (status.ok()) {
    status = installcompactionresults(compact);
  }
  versionset::levelsummarystorage tmp;
  log(options_.info_log,
      "compacted to: %s", versions_->levelsummary(&tmp));
  return status;
}

namespace {
struct iterstate {
  port::mutex* mu;
  version* version;
  memtable* mem;
  memtable* imm;
};

static void cleanupiteratorstate(void* arg1, void* arg2) {
  iterstate* state = reinterpret_cast<iterstate*>(arg1);
  state->mu->lock();
  state->mem->unref();
  if (state->imm != null) state->imm->unref();
  state->version->unref();
  state->mu->unlock();
  delete state;
}
}  // namespace

iterator* dbimpl::newinternaliterator(const readoptions& options, uint64_t number,
                                      sequencenumber* latest_snapshot,
                                      uint32_t* seed, bool external_sync) {
  iterstate* cleanup = new iterstate;
  if (!external_sync) {
    mutex_.lock();
  }
  ++straight_reads_;
  *latest_snapshot = versions_->lastsequence();

  // collect together all needed child iterators
  std::vector<iterator*> list;
  list.push_back(mem_->newiterator());
  mem_->ref();
  if (imm_ != null) {
    list.push_back(imm_->newiterator());
    imm_->ref();
  }
  versions_->current()->addsomeiterators(options, number, &list);
  iterator* internal_iter =
      newmergingiterator(&internal_comparator_, &list[0], list.size());
  versions_->current()->ref();

  cleanup->mu = &mutex_;
  cleanup->mem = mem_;
  cleanup->imm = imm_;
  cleanup->version = versions_->current();
  internal_iter->registercleanup(cleanupiteratorstate, cleanup, null);

  *seed = ++seed_;
  if (!external_sync) {
    mutex_.unlock();
  }
  return internal_iter;
}

iterator* dbimpl::test_newinternaliterator() {
  sequencenumber ignored;
  uint32_t ignored_seed;
  return newinternaliterator(readoptions(), 0, &ignored, &ignored_seed, false);
}

int64_t dbimpl::test_maxnextleveloverlappingbytes() {
  mutexlock l(&mutex_);
  return versions_->maxnextleveloverlappingbytes();
}

status dbimpl::get(const readoptions& options,
                   const slice& key,
                   std::string* value) {
  status s;
  mutexlock l(&mutex_);
  sequencenumber snapshot;
  if (options.snapshot != null) {
    snapshot = reinterpret_cast<const snapshotimpl*>(options.snapshot)->number_;
  } else {
    snapshot = versions_->lastsequence();
  }

  memtable* mem = mem_;
  memtable* imm = imm_;
  version* current = versions_->current();
  mem->ref();
  if (imm != null) imm->ref();
  current->ref();

  bool have_stat_update = false;
  version::getstats stats;

  // unlock while reading from files and memtables
  {
    mutex_.unlock();
    // first look in the memtable, then in the immutable memtable (if any).
    lookupkey lkey(key, snapshot);
    if (mem->get(lkey, value, &s)) {
      // done
    } else if (imm != null && imm->get(lkey, value, &s)) {
      // done
    } else {
      s = current->get(options, lkey, value, &stats);
      have_stat_update = true;
    }
    mutex_.lock();
  }

  if (have_stat_update && current->updatestats(stats)) {
    bg_compaction_cv_.signal();
  }
  ++straight_reads_;
  mem->unref();
  if (imm != null) imm->unref();
  current->unref();
  return s;
}

iterator* dbimpl::newiterator(const readoptions& options) {
  sequencenumber latest_snapshot;
  uint32_t seed;
  iterator* iter = newinternaliterator(options, 0, &latest_snapshot, &seed, false);
  return newdbiterator(
      this, user_comparator(), iter,
      (options.snapshot != null
       ? reinterpret_cast<const snapshotimpl*>(options.snapshot)->number_
       : latest_snapshot),
      seed);
}

void dbimpl::getreplaytimestamp(std::string* timestamp) {
  uint64_t file = 0;
  uint64_t seqno = 0;

  {
    mutexlock l(&mutex_);
    file = versions_->newfilenumber();
    versions_->reusefilenumber(file);
    seqno = versions_->lastsequence();
  }

  timestamp->clear();
  putvarint64(timestamp, file);
  putvarint64(timestamp, seqno);
}

void dbimpl::allowgarbagecollectbeforetimestamp(const std::string& timestamp) {
  slice ts_slice(timestamp);
  uint64_t file = 0;
  uint64_t seqno = 0;

  if (timestamp == "all") {
    // keep zeroes
  } else if (timestamp == "now") {
    mutexlock l(&mutex_);
    seqno = versions_->lastsequence();
    if (manual_garbage_cutoff_ < seqno) {
      manual_garbage_cutoff_ = seqno;
    }
  } else if (getvarint64(&ts_slice, &file) &&
             getvarint64(&ts_slice, &seqno)) {
    mutexlock l(&mutex_);
    if (manual_garbage_cutoff_ < seqno) {
      manual_garbage_cutoff_ = seqno;
    }
  }
}

bool dbimpl::validatetimestamp(const std::string& ts) {
  uint64_t file = 0;
  uint64_t seqno = 0;
  slice ts_slice(ts);
  return ts == "all" || ts == "now" ||
         (getvarint64(&ts_slice, &file) &&
          getvarint64(&ts_slice, &seqno));
}

int dbimpl::comparetimestamps(const std::string& lhs, const std::string& rhs) {
  uint64_t now = 0;
  uint64_t lhs_seqno = 0;
  uint64_t rhs_seqno = 0;
  uint64_t tmp;
  if (lhs == "now" || rhs == "now") {
    mutexlock l(&mutex_);
    now = versions_->lastsequence();
  }
  if (lhs == "all") {
    lhs_seqno = 0;
  } else if (lhs == "now") {
    lhs_seqno = now;
  } else {
    slice lhs_slice(lhs);
    getvarint64(&lhs_slice, &tmp);
    getvarint64(&lhs_slice, &lhs_seqno);
  }
  if (rhs == "all") {
    rhs_seqno = 0;
  } else if (rhs == "now") {
    rhs_seqno = now;
  } else {
    slice rhs_slice(rhs);
    getvarint64(&rhs_slice, &tmp);
    getvarint64(&rhs_slice, &rhs_seqno);
  }

  if (lhs_seqno < rhs_seqno) {
    return -1;
  } else if (lhs_seqno > rhs_seqno) {
    return 1;
  } else {
    return 0;
  }
}

status dbimpl::getreplayiterator(const std::string& timestamp,
                                 replayiterator** iter) {
  *iter = null;
  slice ts_slice(timestamp);
  uint64_t file = 0;
  uint64_t seqno = 0;

  if (timestamp == "all") {
    seqno = 0;
  } else if (timestamp == "now") {
    mutexlock l(&mutex_);
    file = versions_->newfilenumber();
    versions_->reusefilenumber(file);
    seqno = versions_->lastsequence();
  } else if (!getvarint64(&ts_slice, &file) ||
             !getvarint64(&ts_slice, &seqno)) {
    return status::invalidargument("timestamp is not valid");
  }

  readoptions options;
  sequencenumber latest_snapshot;
  uint32_t seed;
  mutexlock l(&mutex_);
  iterator* internal_iter = newinternaliterator(options, file, &latest_snapshot, &seed, true);
  internal_iter->seektofirst();
  replayiteratorimpl* iterimpl;
  iterimpl = new replayiteratorimpl(
      this, &mutex_, user_comparator(), internal_iter, mem_, sequencenumber(seqno));
  *iter = iterimpl;
  replay_iters_.push_back(iterimpl);
  return status::ok();
}

void dbimpl::releasereplayiterator(replayiterator* _iter) {
  mutexlock l(&mutex_);
  replayiteratorimpl* iter = reinterpret_cast<replayiteratorimpl*>(_iter);
  for (std::list<replayiteratorimpl*>::iterator it = replay_iters_.begin();
      it != replay_iters_.end(); ++it) {
    if (*it == iter) {
      iter->cleanup(); // calls delete
      replay_iters_.erase(it);
      return;
    }
  }
}

void dbimpl::recordreadsample(slice key) {
  mutexlock l(&mutex_);
  ++straight_reads_;
  if (versions_->current()->recordreadsample(key)) {
    bg_compaction_cv_.signal();
  }
}

sequencenumber dbimpl::lastsequence() {
  mutexlock l(&mutex_);
  return versions_->lastsequence();
}

const snapshot* dbimpl::getsnapshot() {
  mutexlock l(&mutex_);
  return snapshots_.new(versions_->lastsequence());
}

void dbimpl::releasesnapshot(const snapshot* s) {
  mutexlock l(&mutex_);
  snapshots_.delete(reinterpret_cast<const snapshotimpl*>(s));
}

// convenience methods
status dbimpl::put(const writeoptions& o, const slice& key, const slice& val) {
  return db::put(o, key, val);
}

status dbimpl::delete(const writeoptions& options, const slice& key) {
  return db::delete(options, key);
}

// information kept for every waiting writer
struct dbimpl::writer {
  port::mutex mtx;
  port::condvar cv;
  bool linked;
  writer* next;
  uint64_t start_sequence;
  uint64_t end_sequence;
  std::shared_ptr<writablefile> logfile;
  std::shared_ptr<log::writer> log;
  memtable* mem;
  std::shared_ptr<writablefile> old_logfile;
  std::shared_ptr<log::writer> old_log;

  explicit writer()
    : mtx(),
      cv(&mtx),
      linked(false),
      next(null),
      start_sequence(0),
      end_sequence(0),
      logfile(),
      log(),
      mem(null),
      old_logfile(),
      old_log() {
  }
  ~writer() throw () {
  }
};

status dbimpl::write(const writeoptions& options, writebatch* updates) {
  writer w;
  status s;
  s = sequencewritebegin(&w, updates);

  if (s.ok() && updates != null) { // null batch is for compactions
    writebatchinternal::setsequence(updates, w.start_sequence);

    // add to log and apply to memtable.  we do this without holding the lock
    // because both the log and the memtable are safe for concurrent access.
    // the synchronization with readers occurs with sequencewriteend.
    s = w.log->addrecord(writebatchinternal::contents(updates));
    if (s.ok()) {
      s = writebatchinternal::insertinto(updates, w.mem);
    }
  }

  if (s.ok() && options.sync) {
    s = w.logfile->sync();
  }

  sequencewriteend(&w);
  return s;
}

status dbimpl::sequencewritebegin(writer* w, writebatch* updates) {
  status s;
  mutexlock l(&mutex_);
  straight_reads_ = 0;
  bool force = updates == null;
  bool allow_delay = !force;
  bool enqueue_mem = false;
  w->old_log.reset();
  w->old_logfile.reset();

  while (true) {
    if (!bg_error_.ok()) {
      // yield previous error
      s = bg_error_;
      break;
    } else if (!force &&
               (mem_->approximatememoryusage() <= options_.write_buffer_size)) {
      // there is room in current memtable
      // note that this is a sloppy check.  we can overfill a memtable by the
      // amount of concurrently written data.
      break;
    } else if (imm_ != null) {
      // we have filled up the current memtable, but the previous
      // one is still being compacted, so we wait.
      bg_compaction_cv_.signal();
      bg_memtable_cv_.signal();
      bg_fg_cv_.wait();
    } else {
      // attempt to switch to a new memtable and trigger compaction of old
      assert(versions_->prevlognumber() == 0);
      uint64_t new_log_number = versions_->newfilenumber();
      writablefile* lfile = null;
      s = env_->newwritablefile(logfilename(dbname_, new_log_number), &lfile);
      if (!s.ok()) {
        // avoid chewing through file number space in a tight loop.
        versions_->reusefilenumber(new_log_number);
        break;
      }
      w->old_log = log_;
      w->old_logfile = logfile_;
      logfile_.reset(lfile);
      logfile_number_ = new_log_number;
      log_.reset(new log::writer(lfile));
      imm_ = mem_;
      has_imm_.release_store(imm_);
      mem_ = new memtable(internal_comparator_);
      mem_->ref();
      force = false;   // do not force another compaction if have room
      enqueue_mem = true;
      break;
    }
  }

  if (s.ok()) {
    w->linked = true;
    w->next = null;
    uint64_t diff = updates ? writebatchinternal::count(updates) : 0;
    uint64_t ticket = __sync_add_and_fetch(&writers_upper_, 1 + diff);
    w->start_sequence = ticket - diff;
    w->end_sequence = ticket;
    w->logfile = logfile_;
    w->log = log_;
    w->mem = mem_;
    w->mem->ref();
  }

  if (enqueue_mem) {
    for (std::list<replayiteratorimpl*>::iterator it = replay_iters_.begin();
        it != replay_iters_.end(); ++it) {
      (*it)->enqueue(mem_, w->start_sequence);
    }
  }

  return s;
}

void dbimpl::sequencewriteend(writer* w) {
  if (!w->linked) {
    return;
  }

  // wait until we are next
  while (__sync_fetch_and_add(&writers_lower_, 0) < w->start_sequence)
    ;

  // swizzle state to make ours visible
  {
    mutexlock l(&mutex_);
    versions_->setlastsequence(w->end_sequence);
  }

  // signal the next writer
  __sync_fetch_and_add(&writers_lower_, 1 + w->end_sequence - w->start_sequence);

  // must do in order: log, logfile
  if (w->old_log) {
    assert(w->old_logfile);
    w->old_log.reset();
    w->old_logfile.reset();
    bg_memtable_cv_.signal();
  }

  // safe because unref is synchronized internally
  if (w->mem) {
    w->mem->unref();
  }
}

bool dbimpl::getproperty(const slice& property, std::string* value) {
  value->clear();

  mutexlock l(&mutex_);
  slice in = property;
  slice prefix("leveldb.");
  if (!in.starts_with(prefix)) return false;
  in.remove_prefix(prefix.size());

  if (in.starts_with("num-files-at-level")) {
    in.remove_prefix(strlen("num-files-at-level"));
    uint64_t level;
    bool ok = consumedecimalnumber(&in, &level) && in.empty();
    if (!ok || level >= config::knumlevels) {
      return false;
    } else {
      char buf[100];
      snprintf(buf, sizeof(buf), "%d",
               versions_->numlevelfiles(static_cast<int>(level)));
      *value = buf;
      return true;
    }
  } else if (in == "stats") {
    char buf[200];
    snprintf(buf, sizeof(buf),
             "                               compactions\n"
             "level  files size(mb) time(sec) read(mb) write(mb)\n"
             "--------------------------------------------------\n"
             );
    value->append(buf);
    for (int level = 0; level < config::knumlevels; level++) {
      int files = versions_->numlevelfiles(level);
      if (stats_[level].micros > 0 || files > 0) {
        snprintf(
            buf, sizeof(buf),
            "%3d %8d %8.0f %9.0f %8.0f %9.0f\n",
            level,
            files,
            versions_->numlevelbytes(level) / 1048576.0,
            stats_[level].micros / 1e6,
            stats_[level].bytes_read / 1048576.0,
            stats_[level].bytes_written / 1048576.0);
        value->append(buf);
      }
    }
    return true;
  } else if (in == "sstables") {
    *value = versions_->current()->debugstring();
    return true;
  }

  return false;
}

void dbimpl::getapproximatesizes(
    const range* range, int n,
    uint64_t* sizes) {
  // todo(opt): better implementation
  version* v;
  {
    mutexlock l(&mutex_);
    versions_->current()->ref();
    v = versions_->current();
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

status dbimpl::livebackup(const slice& _name) {
  slice name = _name;
  size_t name_sz = 0;

  for (; name_sz < name.size() && name.data()[name_sz] != '\0'; ++name_sz)
      ;

  name = slice(name.data(), name_sz);
  std::set<uint64_t> live;
  uint64_t ticket = __sync_add_and_fetch(&writers_upper_, 1);

  while (__sync_fetch_and_add(&writers_lower_, 0) < ticket)
    ;

  {
    mutexlock l(&mutex_);
    versions_->setlastsequence(ticket);
    while (backup_in_progress_.acquire_load() != null) {
      backup_cv_.wait();
    }
    backup_in_progress_.release_store(this);
    while (bg_log_occupied_) {
      bg_log_cv_.wait();
    }
    bg_log_occupied_ = true;
    // note that this logic assumes that deleteobsoletefiles never releases
    // mutex_, so that once we release at this brace, we'll guarantee that it
    // will see backup_in_progress_.  if you change deleteobsoletefiles to
    // release mutex_, you'll need to add some sort of synchronization in place
    // of this text block.
    versions_->addlivefiles(&live);
    __sync_fetch_and_add(&writers_lower_, 1);
  }

  status s;
  std::vector<std::string> filenames;
  s = env_->getchildren(dbname_, &filenames);
  std::string backup_dir = dbname_ + "/backup-" + name.tostring() + "/";

  if (s.ok()) {
    s = env_->createdir(backup_dir);
  }

  uint64_t number;
  filetype type;

  for (size_t i = 0; i < filenames.size(); i++) {
    if (!s.ok()) {
      continue;
    }
    if (parsefilename(filenames[i], &number, &type)) {
      std::string src = dbname_ + "/" + filenames[i];
      std::string target = backup_dir + "/" + filenames[i];
      switch (type) {
        case klogfile:
        case kdescriptorfile:
        case kcurrentfile:
        case kinfologfile:
          s = env_->copyfile(src, target);
          break;
        case ktablefile:
          // if it's a file referenced by a version, we have logged that version
          // and applied it.  our manifest will reflect that, and the file
          // number assigned to new files will be greater or equal, ensuring
          // that they aren't overwritten.  any file not in "live" either exists
          // past the current manifest (output of ongoing compaction) or so far
          // in the past we don't care (we're going to delete it at the end of
          // this backup).  i'd rather play safe than sorry.
          //
          // under no circumstances should you collapse this to a single
          // linkfile without the conditional as it has implications for backups
          // that share hardlinks.  opening an older backup that has files
          // hardlinked with newer backups will overwrite "immutable" files in
          // the newer backups because they aren't in our manifest, and we do an
          // open/write rather than a creat/rename.  we avoid linking these
          // files.
          if (live.find(number) != live.end()) {
            s = env_->linkfile(src, target);
          }
          break;
        case ktempfile:
        case kdblockfile:
          break;
      }
    }
  }

  {
    mutexlock l(&mutex_);
    backup_in_progress_.release_store(null);
    if (s.ok() && backup_deferred_delete_) {
      deleteobsoletefiles();
    }
    backup_deferred_delete_ = false;
    bg_log_occupied_ = false;
    bg_log_cv_.signal();
    backup_cv_.signal();
  }
  return s;
}

// default implementations of convenience methods that subclasses of db
// can call if they wish
status db::put(const writeoptions& opt, const slice& key, const slice& value) {
  writebatch batch;
  batch.put(key, value);
  return write(opt, &batch);
}

status db::delete(const writeoptions& opt, const slice& key) {
  writebatch batch;
  batch.delete(key);
  return write(opt, &batch);
}

db::~db() { }

status db::open(const options& options, const std::string& dbname,
                db** dbptr) {
  *dbptr = null;

  dbimpl* impl = new dbimpl(options, dbname);
  impl->mutex_.lock();
  versionedit edit;
  status s = impl->recover(&edit); // handles create_if_missing, error_if_exists
  if (s.ok()) {
    uint64_t new_log_number = impl->versions_->newfilenumber();
    writablefile* lfile;
    s = options.env->newwritablefile(logfilename(dbname, new_log_number),
                                     &lfile);
    if (s.ok()) {
      edit.setlognumber(new_log_number);
      impl->logfile_.reset(lfile);
      impl->logfile_number_ = new_log_number;
      impl->log_.reset(new log::writer(lfile));
      s = impl->versions_->logandapply(&edit, &impl->mutex_, &impl->bg_log_cv_, &impl->bg_log_occupied_);
    }
    if (s.ok()) {
      impl->deleteobsoletefiles();
      impl->bg_optimistic_cv_.signal();
      impl->bg_compaction_cv_.signal();
      impl->bg_memtable_cv_.signal();
    }
  }
  impl->pending_outputs_.clear();
  impl->allow_background_activity_ = true;
  impl->bg_optimistic_cv_.signalall();
  impl->bg_compaction_cv_.signalall();
  impl->bg_memtable_cv_.signalall();
  impl->mutex_.unlock();
  if (s.ok()) {
    *dbptr = impl;
  } else {
    delete impl;
  }
  impl->writers_upper_ = impl->versions_->lastsequence();
  impl->writers_lower_ = impl->writers_upper_ + 1;
  return s;
}

snapshot::~snapshot() {
}

status destroydb(const std::string& dbname, const options& options) {
  env* env = options.env;
  std::vector<std::string> filenames;
  // ignore error in case directory does not exist
  env->getchildren(dbname, &filenames);
  if (filenames.empty()) {
    return status::ok();
  }

  filelock* lock;
  const std::string lockname = lockfilename(dbname);
  status result = env->lockfile(lockname, &lock);
  if (result.ok()) {
    uint64_t number;
    filetype type;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (parsefilename(filenames[i], &number, &type) &&
          type != kdblockfile) {  // lock file will be deleted at end
        status del = env->deletefile(dbname + "/" + filenames[i]);
        if (result.ok() && !del.ok()) {
          result = del;
        }
      }
    }
    env->unlockfile(lock);  // ignore error since state is already gone
    env->deletefile(lockname);
    env->deletedir(dbname);  // ignore error in case dir contains other files
  }
  return result;
}

}  // namespace hyperleveldb
