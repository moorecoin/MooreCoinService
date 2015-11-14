// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_hyperleveldb_db_db_impl_h_
#define storage_hyperleveldb_db_db_impl_h_

#include <deque>
#include <list>
#include <set>
#include <memory>

#include "dbformat.h"
#include "log_writer.h"
#include "snapshot.h"
#include "replay_iterator.h"
#include "../hyperleveldb/db.h"
#include "../hyperleveldb/env.h"
#include "../port/port.h"
#include "../port/thread_annotations.h"

namespace hyperleveldb {

class memtable;
class tablecache;
class version;
class versionedit;
class versionset;

class dbimpl : public db {
 public:
  dbimpl(const options& options, const std::string& dbname);
  virtual ~dbimpl();

  // implementations of the db interface
  virtual status put(const writeoptions&, const slice& key, const slice& value);
  virtual status delete(const writeoptions&, const slice& key);
  virtual status write(const writeoptions& options, writebatch* updates);
  virtual status get(const readoptions& options,
                     const slice& key,
                     std::string* value);
  virtual iterator* newiterator(const readoptions&);
  virtual void getreplaytimestamp(std::string* timestamp);
  virtual void allowgarbagecollectbeforetimestamp(const std::string& timestamp);
  virtual bool validatetimestamp(const std::string& timestamp);
  virtual int comparetimestamps(const std::string& lhs, const std::string& rhs);
  virtual status getreplayiterator(const std::string& timestamp,
                                   replayiterator** iter);
  virtual void releasereplayiterator(replayiterator* iter);
  virtual const snapshot* getsnapshot();
  virtual void releasesnapshot(const snapshot* snapshot);
  virtual bool getproperty(const slice& property, std::string* value);
  virtual void getapproximatesizes(const range* range, int n, uint64_t* sizes);
  virtual void compactrange(const slice* begin, const slice* end);
  virtual status livebackup(const slice& name);

  // extra methods (for testing) that are not in the public db interface

  // compact any files in the named level that overlap [*begin,*end]
  void test_compactrange(int level, const slice* begin, const slice* end);

  // force current memtable contents to be compacted.
  status test_compactmemtable();

  // return an internal iterator over the current state of the database.
  // the keys of this iterator are internal keys (see format.h).
  // the returned iterator should be deleted when no longer needed.
  iterator* test_newinternaliterator();

  // return the maximum overlapping data (in bytes) at next level for any
  // file at a level >= 1.
  int64_t test_maxnextleveloverlappingbytes();

  // record a sample of bytes read at the specified internal key.
  // samples are taken approximately once every config::kreadbytesperiod
  // bytes.
  void recordreadsample(slice key);

  // peek at the last sequence;
  // requres: mutex_ not held
  sequencenumber lastsequence();

 private:
  friend class db;
  struct compactionstate;
  struct writer;

  iterator* newinternaliterator(const readoptions&, uint64_t number,
                                sequencenumber* latest_snapshot,
                                uint32_t* seed, bool external_sync);

  status newdb();

  // recover the descriptor from persistent storage.  may do a significant
  // amount of work to recover recently logged updates.  any changes to
  // be made to the descriptor are added to *edit.
  status recover(versionedit* edit) exclusive_locks_required(mutex_);

  void maybeignoreerror(status* s) const;

  // delete any unneeded files and stale in-memory entries.
  void deleteobsoletefiles();

  // a background thread to compact the in-memory write buffer to disk.
  // switches to a new log-file/memtable and writes a new descriptor iff
  // successful.
  static void compactmemtablewrapper(void* db)
  { reinterpret_cast<dbimpl*>(db)->compactmemtablethread(); }
  void compactmemtablethread();

  status recoverlogfile(uint64_t log_number,
                        versionedit* edit,
                        sequencenumber* max_sequence)
      exclusive_locks_required(mutex_);

  status writelevel0table(memtable* mem, versionedit* edit, version* base, uint64_t* number)
      exclusive_locks_required(mutex_);

  status sequencewritebegin(writer* w, writebatch* updates)
      exclusive_locks_required(mutex_);
  void sequencewriteend(writer* w)
      exclusive_locks_required(mutex_);

  static void compactlevelwrapper(void* db)
  { reinterpret_cast<dbimpl*>(db)->compactlevelthread(); }
  void compactlevelthread();
  status backgroundcompaction() exclusive_locks_required(mutex_);

  static void compactoptimisticwrapper(void* db)
  { reinterpret_cast<dbimpl*>(db)->compactoptimisticthread(); }
  void compactoptimisticthread();
  status optimisticcompaction() exclusive_locks_required(mutex_);

  void cleanupcompaction(compactionstate* compact)
      exclusive_locks_required(mutex_);
  status docompactionwork(compactionstate* compact)
      exclusive_locks_required(mutex_);
  status opencompactionoutputfile(compactionstate* compact);
  status finishcompactionoutputfile(compactionstate* compact, iterator* input);
  status installcompactionresults(compactionstate* compact)
      exclusive_locks_required(mutex_);

  // constant after construction
  env* const env_;
  const internalkeycomparator internal_comparator_;
  const internalfilterpolicy internal_filter_policy_;
  const options options_;  // options_.comparator == &internal_comparator_
  bool owns_info_log_;
  bool owns_cache_;
  const std::string dbname_;

  // table_cache_ provides its own synchronization
  tablecache* table_cache_;

  // lock over the persistent db state.  non-null iff successfully acquired.
  filelock* db_lock_;

  // state below is protected by mutex_
  port::mutex mutex_;
  port::atomicpointer shutting_down_;
  memtable* mem_;
  memtable* imm_;                // memtable being compacted
  port::atomicpointer has_imm_;  // so bg thread can detect non-null imm_
  std::shared_ptr<writablefile> logfile_;
  uint64_t logfile_number_;
  std::shared_ptr<log::writer> log_;
  uint32_t seed_;                // for sampling.

  // synchronize writers
  uint64_t __attribute__ ((aligned (8))) writers_lower_;
  uint64_t __attribute__ ((aligned (8))) writers_upper_;

  snapshotlist snapshots_;

  // set of table files to protect from deletion because they are
  // part of ongoing compactions.
  std::set<uint64_t> pending_outputs_;

  bool allow_background_activity_;
  bool levels_locked_[config::knumlevels];
  int num_bg_threads_;
  // tell the foreground that background has done something of note
  port::condvar bg_fg_cv_;
  // communicate with compaction background thread
  port::condvar bg_compaction_cv_;
  // communicate with memtable->l0 background thread
  port::condvar bg_memtable_cv_;
  // communicate with the optimistic background thread
  bool bg_optimistic_trip_;
  port::condvar bg_optimistic_cv_;
  // mutual exlusion protecting the logandapply func
  port::condvar bg_log_cv_;
  bool bg_log_occupied_;

  // information for a manual compaction
  struct manualcompaction {
    int level;
    bool done;
    const internalkey* begin;   // null means beginning of key range
    const internalkey* end;     // null means end of key range
    internalkey tmp_storage;    // used to keep track of compaction progress
  };
  manualcompaction* manual_compaction_;

  // where have we pinned tombstones?
  sequencenumber manual_garbage_cutoff_;

  // replay iterators
  std::list<replayiteratorimpl*> replay_iters_;

  // how many reads have we done in a row, uninterrupted by writes
  uint64_t straight_reads_;

  versionset* versions_;

  // information for ongoing backup processes
  port::condvar backup_cv_;
  port::atomicpointer backup_in_progress_; // non-null in progress
  bool backup_deferred_delete_; // deleteobsoletefiles delayed by backup; protect with mutex_

  // have we encountered a background error in paranoid mode?
  status bg_error_;
  int consecutive_compaction_errors_;

  // per level compaction stats.  stats_[level] stores the stats for
  // compactions that produced data for the specified "level".
  struct compactionstats {
    int64_t micros;
    int64_t bytes_read;
    int64_t bytes_written;

    compactionstats() : micros(0), bytes_read(0), bytes_written(0) { }

    void add(const compactionstats& c) {
      this->micros += c.micros;
      this->bytes_read += c.bytes_read;
      this->bytes_written += c.bytes_written;
    }
  };
  compactionstats stats_[config::knumlevels];

  // no copying allowed
  dbimpl(const dbimpl&);
  void operator=(const dbimpl&);

  const comparator* user_comparator() const {
    return internal_comparator_.user_comparator();
  }
};

// sanitize db options.  the caller should delete result.info_log if
// it is not equal to src.info_log.
extern options sanitizeoptions(const std::string& db,
                               const internalkeycomparator* icmp,
                               const internalfilterpolicy* ipolicy,
                               const options& src);

}  // namespace hyperleveldb

#endif  // storage_hyperleveldb_db_db_impl_h_
