//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
#pragma once

#include <atomic>
#include <deque>
#include <limits>
#include <set>
#include <utility>
#include <vector>
#include <string>

#include "db/dbformat.h"
#include "db/log_writer.h"
#include "db/snapshot.h"
#include "db/column_family.h"
#include "db/version_edit.h"
#include "memtable_list.h"
#include "port/port.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/memtablerep.h"
#include "rocksdb/transaction_log.h"
#include "util/autovector.h"
#include "util/stop_watch.h"
#include "util/thread_local.h"
#include "db/internal_stats.h"

namespace rocksdb {

class memtable;
class tablecache;
class version;
class versionedit;
class versionset;
class compactionfilterv2;
class arena;

class dbimpl : public db {
 public:
  dbimpl(const dboptions& options, const std::string& dbname);
  virtual ~dbimpl();

  // implementations of the db interface
  using db::put;
  virtual status put(const writeoptions& options,
                     columnfamilyhandle* column_family, const slice& key,
                     const slice& value);
  using db::merge;
  virtual status merge(const writeoptions& options,
                       columnfamilyhandle* column_family, const slice& key,
                       const slice& value);
  using db::delete;
  virtual status delete(const writeoptions& options,
                        columnfamilyhandle* column_family, const slice& key);
  using db::write;
  virtual status write(const writeoptions& options, writebatch* updates);
  using db::get;
  virtual status get(const readoptions& options,
                     columnfamilyhandle* column_family, const slice& key,
                     std::string* value);
  using db::multiget;
  virtual std::vector<status> multiget(
      const readoptions& options,
      const std::vector<columnfamilyhandle*>& column_family,
      const std::vector<slice>& keys, std::vector<std::string>* values);

  virtual status createcolumnfamily(const columnfamilyoptions& options,
                                    const std::string& column_family,
                                    columnfamilyhandle** handle);
  virtual status dropcolumnfamily(columnfamilyhandle* column_family);

  // returns false if key doesn't exist in the database and true if it may.
  // if value_found is not passed in as null, then return the value if found in
  // memory. on return, if value was found, then value_found will be set to true
  // , otherwise false.
  using db::keymayexist;
  virtual bool keymayexist(const readoptions& options,
                           columnfamilyhandle* column_family, const slice& key,
                           std::string* value, bool* value_found = nullptr);
  using db::newiterator;
  virtual iterator* newiterator(const readoptions& options,
                                columnfamilyhandle* column_family);
  virtual status newiterators(
      const readoptions& options,
      const std::vector<columnfamilyhandle*>& column_families,
      std::vector<iterator*>* iterators);
  virtual const snapshot* getsnapshot();
  virtual void releasesnapshot(const snapshot* snapshot);
  using db::getproperty;
  virtual bool getproperty(columnfamilyhandle* column_family,
                           const slice& property, std::string* value);
  using db::getintproperty;
  virtual bool getintproperty(columnfamilyhandle* column_family,
                              const slice& property, uint64_t* value) override;
  using db::getapproximatesizes;
  virtual void getapproximatesizes(columnfamilyhandle* column_family,
                                   const range* range, int n, uint64_t* sizes);
  using db::compactrange;
  virtual status compactrange(columnfamilyhandle* column_family,
                              const slice* begin, const slice* end,
                              bool reduce_level = false, int target_level = -1,
                              uint32_t target_path_id = 0);

  using db::numberlevels;
  virtual int numberlevels(columnfamilyhandle* column_family);
  using db::maxmemcompactionlevel;
  virtual int maxmemcompactionlevel(columnfamilyhandle* column_family);
  using db::level0stopwritetrigger;
  virtual int level0stopwritetrigger(columnfamilyhandle* column_family);
  virtual const std::string& getname() const;
  virtual env* getenv() const;
  using db::getoptions;
  virtual const options& getoptions(columnfamilyhandle* column_family) const;
  using db::flush;
  virtual status flush(const flushoptions& options,
                       columnfamilyhandle* column_family);

  virtual sequencenumber getlatestsequencenumber() const;

#ifndef rocksdb_lite
  virtual status disablefiledeletions();
  virtual status enablefiledeletions(bool force);
  virtual int isfiledeletionsenabled() const;
  // all the returned filenames start with "/"
  virtual status getlivefiles(std::vector<std::string>&,
                              uint64_t* manifest_file_size,
                              bool flush_memtable = true);
  virtual status getsortedwalfiles(vectorlogptr& files);

  virtual status getupdatessince(
      sequencenumber seq_number, unique_ptr<transactionlogiterator>* iter,
      const transactionlogiterator::readoptions&
          read_options = transactionlogiterator::readoptions());
  virtual status deletefile(std::string name);

  virtual void getlivefilesmetadata(std::vector<livefilemetadata>* metadata);
#endif  // rocksdb_lite

  // checks if all live files exist on file system and that their file sizes
  // match to our in-memory records
  virtual status checkconsistency();

  virtual status getdbidentity(std::string& identity);

  status runmanualcompaction(columnfamilydata* cfd, int input_level,
                             int output_level, uint32_t output_path_id,
                             const slice* begin, const slice* end);

#ifndef rocksdb_lite
  // extra methods (for testing) that are not in the public db interface
  // implemented in db_impl_debug.cc

  // compact any files in the named level that overlap [*begin, *end]
  status test_compactrange(int level, const slice* begin, const slice* end,
                           columnfamilyhandle* column_family = nullptr);

  // force current memtable contents to be flushed.
  status test_flushmemtable(bool wait = true);

  // wait for memtable compaction
  status test_waitforflushmemtable(columnfamilyhandle* column_family = nullptr);

  // wait for any compaction
  status test_waitforcompact();

  // return an internal iterator over the current state of the database.
  // the keys of this iterator are internal keys (see format.h).
  // the returned iterator should be deleted when no longer needed.
  iterator* test_newinternaliterator(columnfamilyhandle* column_family =
                                         nullptr);

  // return the maximum overlapping data (in bytes) at next level for any
  // file at a level >= 1.
  int64_t test_maxnextleveloverlappingbytes(columnfamilyhandle* column_family =
                                                nullptr);

  // return the current manifest file no.
  uint64_t test_current_manifest_fileno();

  // trigger's a background call for testing.
  void test_purgeobsoletetewal();

  // get total level0 file size. only for testing.
  uint64_t test_getlevel0totalsize();

  void test_setdefaulttimetocheck(uint64_t default_interval_to_delete_obsolete_wal)
  {
    default_interval_to_delete_obsolete_wal_ = default_interval_to_delete_obsolete_wal;
  }

  void test_getfilesmetadata(columnfamilyhandle* column_family,
                             std::vector<std::vector<filemetadata>>* metadata);

  status test_readfirstrecord(const walfiletype type, const uint64_t number,
                              sequencenumber* sequence);

  status test_readfirstline(const std::string& fname, sequencenumber* sequence);
#endif  // ndebug

  // structure to store information for candidate files to delete.
  struct candidatefileinfo {
    std::string file_name;
    uint32_t path_id;
    candidatefileinfo(std::string name, uint32_t path)
        : file_name(name), path_id(path) {}
    bool operator==(const candidatefileinfo& other) const {
      return file_name == other.file_name && path_id == other.path_id;
    }
  };

  // needed for cleanupiteratorstate
  struct deletionstate {
    inline bool havesomethingtodelete() const {
      return  candidate_files.size() ||
        sst_delete_files.size() ||
        log_delete_files.size();
    }

    // a list of all files that we'll consider deleting
    // (every once in a while this is filled up with all files
    // in the db directory)
    std::vector<candidatefileinfo> candidate_files;

    // the list of all live sst files that cannot be deleted
    std::vector<filedescriptor> sst_live;

    // a list of sst files that we need to delete
    std::vector<filemetadata*> sst_delete_files;

    // a list of log files that we need to delete
    std::vector<uint64_t> log_delete_files;

    // a list of memtables to be free
    autovector<memtable*> memtables_to_free;

    autovector<superversion*> superversions_to_free;

    superversion* new_superversion;  // if nullptr no new superversion

    // the current manifest_file_number, log_number and prev_log_number
    // that corresponds to the set of files in 'live'.
    uint64_t manifest_file_number, pending_manifest_file_number, log_number,
        prev_log_number;

    explicit deletionstate(bool create_superversion = false) {
      manifest_file_number = 0;
      pending_manifest_file_number = 0;
      log_number = 0;
      prev_log_number = 0;
      new_superversion = create_superversion ? new superversion() : nullptr;
    }

    ~deletionstate() {
      // free pending memtables
      for (auto m : memtables_to_free) {
        delete m;
      }
      // free superversions
      for (auto s : superversions_to_free) {
        delete s;
      }
      // if new_superversion was not used, it will be non-nullptr and needs
      // to be freed here
      delete new_superversion;
    }
  };

  // returns the list of live files in 'live' and the list
  // of all files in the filesystem in 'candidate_files'.
  // if force == false and the last call was less than
  // options_.delete_obsolete_files_period_micros microseconds ago,
  // it will not fill up the deletion_state
  void findobsoletefiles(deletionstate& deletion_state,
                         bool force,
                         bool no_full_scan = false);

  // diffs the files listed in filenames and those that do not
  // belong to live files are posibly removed. also, removes all the
  // files in sst_delete_files and log_delete_files.
  // it is not necessary to hold the mutex when invoking this method.
  void purgeobsoletefiles(deletionstate& deletion_state);

  columnfamilyhandle* defaultcolumnfamily() const;

 protected:
  env* const env_;
  const std::string dbname_;
  unique_ptr<versionset> versions_;
  const dboptions options_;
  statistics* stats_;

  iterator* newinternaliterator(const readoptions&, columnfamilydata* cfd,
                                superversion* super_version,
                                arena* arena = nullptr);

 private:
  friend class db;
  friend class internalstats;
#ifndef rocksdb_lite
  friend class tailingiterator;
  friend class forwarditerator;
#endif
  friend struct superversion;
  struct compactionstate;
  struct writer;
  struct writecontext;

  status newdb();

  // recover the descriptor from persistent storage.  may do a significant
  // amount of work to recover recently logged updates.  any changes to
  // be made to the descriptor are added to *edit.
  status recover(const std::vector<columnfamilydescriptor>& column_families,
                 bool read_only = false, bool error_if_log_file_exist = false);

  void maybeignoreerror(status* s) const;

  const status createarchivaldirectory();

  // delete any unneeded files and stale in-memory entries.
  void deleteobsoletefiles();

  // flush the in-memory write buffer to storage.  switches to a new
  // log-file/memtable and writes a new descriptor iff successful.
  status flushmemtabletooutputfile(columnfamilydata* cfd, bool* madeprogress,
                                   deletionstate& deletion_state,
                                   logbuffer* log_buffer);

  status recoverlogfile(uint64_t log_number, sequencenumber* max_sequence,
                        bool read_only);

  // the following two methods are used to flush a memtable to
  // storage. the first one is used atdatabase recoverytime (when the
  // database is opened) and is heavyweight because it holds the mutex
  // for the entire period. the second method writelevel0table supports
  // concurrent flush memtables to storage.
  status writelevel0tableforrecovery(columnfamilydata* cfd, memtable* mem,
                                     versionedit* edit);
  status writelevel0table(columnfamilydata* cfd, autovector<memtable*>& mems,
                          versionedit* edit, uint64_t* filenumber,
                          logbuffer* log_buffer);

  uint64_t slowdownamount(int n, double bottom, double top);

  // before applying write operation (such as dbimpl::write, dbimpl::flush)
  // thread should grab the mutex_ and be the first on writers queue.
  // beginwrite is used for it.
  // be aware! writer's job can be done by other thread (see dbimpl::write
  // for examples), so check it via w.done before applying changes.
  //
  // writer* w:                writer to be placed in the queue
  // uint64_t expiration_time: maximum time to be in the queue
  // see also: endwrite
  status beginwrite(writer* w, uint64_t expiration_time);

  // after doing write job, we need to remove already used writers from
  // writers_ queue and notify head of the queue about it.
  // endwrite is used for this.
  //
  // writer* w:           writer, that was added by beginwrite function
  // writer* last_writer: since we can join a few writers (as dbimpl::write
  //                      does)
  //                      we should pass last_writer as a parameter to
  //                      endwrite
  //                      (if you don't touch other writers, just pass w)
  // status status:       status of write operation
  // see also: beginwrite
  void endwrite(writer* w, writer* last_writer, status status);

  status makeroomforwrite(columnfamilydata* cfd,
                          writecontext* context,
                          uint64_t expiration_time);

  status setnewmemtableandnewlogfile(columnfamilydata* cfd,
                                     writecontext* context);

  void buildbatchgroup(writer** last_writer,
                       autovector<writebatch*>* write_batch_group);

  // force current memtable contents to be flushed.
  status flushmemtable(columnfamilydata* cfd, const flushoptions& options);

  // wait for memtable flushed
  status waitforflushmemtable(columnfamilydata* cfd);

  void recordflushiostats();
  void recordcompactioniostats();

  void maybescheduleflushorcompaction();
  static void bgworkcompaction(void* db);
  static void bgworkflush(void* db);
  void backgroundcallcompaction();
  void backgroundcallflush();
  status backgroundcompaction(bool* madeprogress, deletionstate& deletion_state,
                              logbuffer* log_buffer);
  status backgroundflush(bool* madeprogress, deletionstate& deletion_state,
                         logbuffer* log_buffer);
  void cleanupcompaction(compactionstate* compact, status status);
  status docompactionwork(compactionstate* compact,
                          deletionstate& deletion_state,
                          logbuffer* log_buffer);

  // this function is called as part of compaction. it enables flush process to
  // preempt compaction, since it's higher prioirty
  // returns: micros spent executing
  uint64_t callflushduringcompaction(columnfamilydata* cfd,
                                     deletionstate& deletion_state,
                                     logbuffer* log_buffer);

  // call compaction filter if is_compaction_v2 is not true. then iterate
  // through input and compact the kv-pairs
  status processkeyvaluecompaction(
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
    logbuffer* log_buffer);

  // call compaction_filter_v2->filter() on kv-pairs in compact
  void callcompactionfilterv2(compactionstate* compact,
    compactionfilterv2* compaction_filter_v2);

  status opencompactionoutputfile(compactionstate* compact);
  status finishcompactionoutputfile(compactionstate* compact, iterator* input);
  status installcompactionresults(compactionstate* compact,
                                  logbuffer* log_buffer);
  void allocatecompactionoutputfilenumbers(compactionstate* compact);
  void releasecompactionunusedfilenumbers(compactionstate* compact);

#ifdef rocksdb_lite
  void purgeobsoletewalfiles() {
    // this function is used for archiving wal files. we don't need this in
    // rocksdb_lite
  }
#else
  void purgeobsoletewalfiles();

  status getsortedwalsoftype(const std::string& path,
                             vectorlogptr& log_files,
                             walfiletype type);

  // requires: all_logs should be sorted with earliest log file first
  // retains all log files in all_logs which contain updates with seq no.
  // greater than or equal to the requested sequencenumber.
  status retainprobablewalfiles(vectorlogptr& all_logs,
                                const sequencenumber target);

  status readfirstrecord(const walfiletype type, const uint64_t number,
                         sequencenumber* sequence);

  status readfirstline(const std::string& fname, sequencenumber* sequence);
#endif  // rocksdb_lite

  void printstatistics();

  // dump rocksdb.stats to log
  void maybedumpstats();

  // return true if the current db supports snapshot.  if the current
  // db does not support snapshot, then calling getsnapshot() will always
  // return nullptr.
  //
  // @see getsnapshot()
  virtual bool issnapshotsupported() const;

  // return the minimum empty level that could hold the total data in the
  // input level. return the input level, if such level could not be found.
  int findminimumemptylevelfitting(columnfamilydata* cfd, int level);

  // move the files in the input level to the target level.
  // if target_level < 0, automatically calculate the minimum level that could
  // hold the data set.
  status refitlevel(columnfamilydata* cfd, int level, int target_level = -1);

  // table_cache_ provides its own synchronization
  std::shared_ptr<cache> table_cache_;

  // lock over the persistent db state.  non-nullptr iff successfully acquired.
  filelock* db_lock_;

  // state below is protected by mutex_
  port::mutex mutex_;
  port::atomicpointer shutting_down_;
  // this condition variable is signaled on these conditions:
  // * whenever bg_compaction_scheduled_ goes down to 0
  // * if bg_manual_only_ > 0, whenever a compaction finishes, even if it hasn't
  // made any progress
  // * whenever a compaction made any progress
  // * whenever bg_flush_scheduled_ value decreases (i.e. whenever a flush is
  // done, even if it didn't make any progress)
  // * whenever there is an error in background flush or compaction
  port::condvar bg_cv_;
  uint64_t logfile_number_;
  unique_ptr<log::writer> log_;
  bool log_empty_;
  columnfamilyhandleimpl* default_cf_handle_;
  internalstats* default_cf_internal_stats_;
  unique_ptr<columnfamilymemtablesimpl> column_family_memtables_;
  struct logfilenumbersize {
    explicit logfilenumbersize(uint64_t _number)
        : number(_number), size(0), getting_flushed(false) {}
    void addsize(uint64_t new_size) { size += new_size; }
    uint64_t number;
    uint64_t size;
    bool getting_flushed;
  };
  std::deque<logfilenumbersize> alive_log_files_;
  uint64_t total_log_size_;
  // only used for dynamically adjusting max_total_wal_size. it is a sum of
  // [write_buffer_size * max_write_buffer_number] over all column families
  uint64_t max_total_in_memory_state_;
  // if true, we have only one (default) column family. we use this to optimize
  // some code-paths
  bool single_column_family_mode_;

  std::unique_ptr<directory> db_directory_;

  // queue of writers.
  std::deque<writer*> writers_;
  writebatch tmp_batch_;

  snapshotlist snapshots_;

  // cache for readfirstrecord() calls
  std::unordered_map<uint64_t, sequencenumber> read_first_record_cache_;
  port::mutex read_first_record_cache_mutex_;

  // set of table files to protect from deletion because they are
  // part of ongoing compactions.
  // map from pending file number id to their path ids.
  filenumtopathidmap pending_outputs_;

  // at least one compaction or flush job is pending but not yet scheduled
  // because of the max background thread limit.
  bool bg_schedule_needed_;

  // count how many background compactions are running or have been scheduled
  int bg_compaction_scheduled_;

  // if non-zero, maybescheduleflushorcompaction() will only schedule manual
  // compactions (if manual_compaction_ is not null). this mechanism enables
  // manual compactions to wait until all other compactions are finished.
  int bg_manual_only_;

  // number of background memtable flush jobs, submitted to the high pool
  int bg_flush_scheduled_;

  // information for a manual compaction
  struct manualcompaction {
    columnfamilydata* cfd;
    int input_level;
    int output_level;
    uint32_t output_path_id;
    bool done;
    status status;
    bool in_progress;           // compaction request being processed?
    const internalkey* begin;   // nullptr means beginning of key range
    const internalkey* end;     // nullptr means end of key range
    internalkey tmp_storage;    // used to keep track of compaction progress
  };
  manualcompaction* manual_compaction_;

  // have we encountered a background error in paranoid mode?
  status bg_error_;

  // shall we disable deletion of obsolete files
  // if 0 the deletion is enabled.
  // if non-zero, files will not be getting deleted
  // this enables two different threads to call
  // enablefiledeletions() and disablefiledeletions()
  // without any synchronization
  int disable_delete_obsolete_files_;

  // last time when deleteobsoletefiles was invoked
  uint64_t delete_obsolete_files_last_run_;

  // last time when purgeobsoletewalfiles ran.
  uint64_t purge_wal_files_last_run_;

  // last time stats were dumped to log
  std::atomic<uint64_t> last_stats_dump_time_microsec_;

  // obsolete files will be deleted every this seconds if ttl deletion is
  // enabled and archive size_limit is disabled.
  uint64_t default_interval_to_delete_obsolete_wal_;

  bool flush_on_destroy_; // used when disablewal is true.

  static const int keep_log_file_num = 1000;
  static const uint64_t knotimeout = std::numeric_limits<uint64_t>::max();
  std::string db_absolute_path_;

  // count of the number of contiguous delaying writes
  int delayed_writes_;

  // the options to access storage files
  const envoptions storage_options_;

  // a value of true temporarily disables scheduling of background work
  bool bg_work_gate_closed_;

  // guard against multiple concurrent refitting
  bool refitting_level_;

  // indicate db was opened successfully
  bool opened_successfully_;

  // no copying allowed
  dbimpl(const dbimpl&);
  void operator=(const dbimpl&);

  // dump the delayed_writes_ to the log file and reset counter.
  void delayloggingandreset();

  // return the earliest snapshot where seqno is visible.
  // store the snapshot right before that, if any, in prev_snapshot
  inline sequencenumber findearliestvisiblesnapshot(
    sequencenumber in,
    std::vector<sequencenumber>& snapshots,
    sequencenumber* prev_snapshot);

  // background threads call this function, which is just a wrapper around
  // the cfd->installsuperversion() function. background threads carry
  // deletion_state which can have new_superversion already allocated.
  void installsuperversion(columnfamilydata* cfd,
                           deletionstate& deletion_state);

  // find super version and reference it. based on options, it might return
  // the thread local cached one.
  inline superversion* getandrefsuperversion(columnfamilydata* cfd);

  // un-reference the super version and return it to thread local cache if
  // needed. if it is the last reference of the super version. clean it up
  // after un-referencing it.
  inline void returnandcleanupsuperversion(columnfamilydata* cfd,
                                           superversion* sv);

#ifndef rocksdb_lite
  using db::getpropertiesofalltables;
  virtual status getpropertiesofalltables(columnfamilyhandle* column_family,
                                          tablepropertiescollection* props)
      override;
#endif  // rocksdb_lite

  // function that get and keymayexist call with no_io true or false
  // note: 'value_found' from keymayexist propagates here
  status getimpl(const readoptions& options, columnfamilyhandle* column_family,
                 const slice& key, std::string* value,
                 bool* value_found = nullptr);

  bool getintpropertyinternal(columnfamilyhandle* column_family,
                              dbpropertytype property_type,
                              bool need_out_of_mutex, uint64_t* value);
};

// sanitize db options.  the caller should delete result.info_log if
// it is not equal to src.info_log.
extern options sanitizeoptions(const std::string& db,
                               const internalkeycomparator* icmp,
                               const options& src);
extern dboptions sanitizeoptions(const std::string& db, const dboptions& src);

// fix user-supplied options to be reasonable
template <class t, class v>
static void cliptorange(t* ptr, v minvalue, v maxvalue) {
  if (static_cast<v>(*ptr) > maxvalue) *ptr = maxvalue;
  if (static_cast<v>(*ptr) < minvalue) *ptr = minvalue;
}

// dump db file summary, implemented in util/
extern void dumpdbfilesummary(const dboptions& options,
                              const std::string& dbname);

}  // namespace rocksdb
