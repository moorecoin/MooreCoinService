//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// the representation of a dbimpl consists of a set of versions.  the
// newest version is called "current".  older versions may be kept
// around to provide a consistent view to live iterators.
//
// each version keeps track of a set of table files per level.  the
// entire set of versions is maintained in a versionset.
//
// version,versionset are thread-compatible, but require external
// synchronization on all accesses.

#pragma once
#include <map>
#include <memory>
#include <set>
#include <vector>
#include <deque>
#include <atomic>
#include <limits>
#include "db/dbformat.h"
#include "db/version_edit.h"
#include "port/port.h"
#include "db/table_cache.h"
#include "db/compaction.h"
#include "db/compaction_picker.h"
#include "db/column_family.h"
#include "db/log_reader.h"
#include "db/file_indexer.h"

namespace rocksdb {

namespace log { class writer; }

class compaction;
class compactionpicker;
class iterator;
class logbuffer;
class lookupkey;
class memtable;
class version;
class versionset;
class mergecontext;
class columnfamilydata;
class columnfamilyset;
class tablecache;
class mergeiteratorbuilder;

// return the smallest index i such that file_level.files[i]->largest >= key.
// return file_level.num_files if there is no such file.
// requires: "file_level.files" contains a sorted list of
// non-overlapping files.
extern int findfile(const internalkeycomparator& icmp,
                    const filelevel& file_level,
                    const slice& key);

// returns true iff some file in "files" overlaps the user key range
// [*smallest,*largest].
// smallest==nullptr represents a key smaller than all keys in the db.
// largest==nullptr represents a key largest than all keys in the db.
// requires: if disjoint_sorted_files, file_level.files[]
// contains disjoint ranges in sorted order.
extern bool somefileoverlapsrange(
    const internalkeycomparator& icmp,
    bool disjoint_sorted_files,
    const filelevel& file_level,
    const slice* smallest_user_key,
    const slice* largest_user_key);

// generate filelevel from vector<fdwithkeyrange*>
// would copy smallest_key and largest_key data to sequential memory
// arena: arena used to allocate the memory
extern void dogeneratefilelevel(filelevel* file_level,
        const std::vector<filemetadata*>& files,
        arena* arena);

class version {
 public:
  // append to *iters a sequence of iterators that will
  // yield the contents of this version when merged together.
  // requires: this version has been saved (see versionset::saveto)
  void additerators(const readoptions&, const envoptions& soptions,
                    std::vector<iterator*>* iters);

  void additerators(const readoptions&, const envoptions& soptions,
                    mergeiteratorbuilder* merger_iter_builder);

  // lookup the value for key.  if found, store it in *val and
  // return ok.  else return a non-ok status.
  // uses *operands to store merge_operator operations to apply later
  // requires: lock is not held
  void get(const readoptions&, const lookupkey& key, std::string* val,
           status* status, mergecontext* merge_context,
           bool* value_found = nullptr);

  // updates internal structures that keep track of compaction scores
  // we use compaction scores to figure out which compaction to do next
  // requires: if version is not yet saved to current_, it can be called without
  // a lock. once a version is saved to current_, call only with mutex held
  void computecompactionscore(std::vector<uint64_t>& size_being_compacted);

  // generate file_levels_ from files_
  void generatefilelevels();

  // update scores, pre-calculated variables. it needs to be called before
  // applying the version to the version set.
  void prepareapply(std::vector<uint64_t>& size_being_compacted);

  // reference count management (so versions do not disappear out from
  // under live iterators)
  void ref();
  // decrease reference count. delete the object if no reference left
  // and return true. otherwise, return false.
  bool unref();

  // returns true iff some level needs a compaction.
  bool needscompaction() const;

  // returns the maxmimum compaction score for levels 1 to max
  double maxcompactionscore() const { return max_compaction_score_; }

  // see field declaration
  int maxcompactionscorelevel() const { return max_compaction_score_level_; }

  void getoverlappinginputs(
      int level,
      const internalkey* begin,         // nullptr means before all keys
      const internalkey* end,           // nullptr means after all keys
      std::vector<filemetadata*>* inputs,
      int hint_index = -1,              // index of overlap file
      int* file_index = nullptr);          // return index of overlap file

  void getoverlappinginputsbinarysearch(
      int level,
      const slice& begin,         // nullptr means before all keys
      const slice& end,           // nullptr means after all keys
      std::vector<filemetadata*>* inputs,
      int hint_index,             // index of overlap file
      int* file_index);           // return index of overlap file

  void extendoverlappinginputs(
      int level,
      const slice& begin,         // nullptr means before all keys
      const slice& end,           // nullptr means after all keys
      std::vector<filemetadata*>* inputs,
      unsigned int index);                 // start extending from this index

  // returns true iff some file in the specified level overlaps
  // some part of [*smallest_user_key,*largest_user_key].
  // smallest_user_key==null represents a key smaller than all keys in the db.
  // largest_user_key==null represents a key largest than all keys in the db.
  bool overlapinlevel(int level,
                      const slice* smallest_user_key,
                      const slice* largest_user_key);

  // returns true iff the first or last file in inputs contains
  // an overlapping user key to the file "just outside" of it (i.e.
  // just after the last file, or just before the first file)
  // requires: "*inputs" is a sorted list of non-overlapping files
  bool hasoverlappinguserkey(const std::vector<filemetadata*>* inputs,
                             int level);


  // return the level at which we should place a new memtable compaction
  // result that covers the range [smallest_user_key,largest_user_key].
  int picklevelformemtableoutput(const slice& smallest_user_key,
                                 const slice& largest_user_key);

  int numberlevels() const { return num_levels_; }

  // requires: lock is held
  int numlevelfiles(int level) const { return files_[level].size(); }

  // return the combined file size of all files at the specified level.
  int64_t numlevelbytes(int level) const;

  // return a human-readable short (single-line) summary of the number
  // of files per level.  uses *scratch as backing store.
  struct levelsummarystorage {
    char buffer[100];
  };
  struct filesummarystorage {
    char buffer[1000];
  };
  const char* levelsummary(levelsummarystorage* scratch) const;
  // return a human-readable short (single-line) summary of files
  // in a specified level.  uses *scratch as backing store.
  const char* levelfilesummary(filesummarystorage* scratch, int level) const;

  // return the maximum overlapping data (in bytes) at next level for any
  // file at a level >= 1.
  int64_t maxnextleveloverlappingbytes();

  // add all files listed in the current version to *live.
  void addlivefiles(std::vector<filedescriptor>* live);

  // return a human readable string that describes this version's contents.
  std::string debugstring(bool hex = false) const;

  // returns the version nuber of this version
  uint64_t getversionnumber() const { return version_number_; }

  uint64_t getaveragevaluesize() const {
    if (num_non_deletions_ == 0) {
      return 0;
    }
    assert(total_raw_key_size_ + total_raw_value_size_ > 0);
    assert(total_file_size_ > 0);
    return total_raw_value_size_ / num_non_deletions_ * total_file_size_ /
           (total_raw_key_size_ + total_raw_value_size_);
  }

  // requires: lock is held
  // on success, "tp" will contains the table properties of the file
  // specified in "file_meta".  if the file name of "file_meta" is
  // known ahread, passing it by a non-null "fname" can save a
  // file-name conversion.
  status gettableproperties(std::shared_ptr<const tableproperties>* tp,
                            const filemetadata* file_meta,
                            const std::string* fname = nullptr);

  // requires: lock is held
  // on success, *props will be populated with all sstables' table properties.
  // the keys of `props` are the sst file name, the values of `props` are the
  // tables' propertis, represented as shared_ptr.
  status getpropertiesofalltables(tablepropertiescollection* props);

  uint64_t getestimatedactivekeys();

  size_t getmemoryusagebytablereaders();

  // used to sort files by size
  struct fsize {
    int index;
    filemetadata* file;
  };

 private:
  friend class compaction;
  friend class versionset;
  friend class dbimpl;
  friend class columnfamilydata;
  friend class compactionpicker;
  friend class levelcompactionpicker;
  friend class universalcompactionpicker;
  friend class fifocompactionpicker;
  friend class forwarditerator;
  friend class internalstats;

  class levelfilenumiterator;
  class levelfileiteratorstate;

  bool prefixmaymatch(const readoptions& options, iterator* level_iter,
                      const slice& internal_prefix) const;

  // update num_non_empty_levels_.
  void updatenumnonemptylevels();

  // the helper function of updatetemporarystats, which may fill the missing
  // fields of file_mata from its associated tableproperties.
  // returns true if it does initialize filemetadata.
  bool maybeinitializefilemetadata(filemetadata* file_meta);

  // update the temporary stats associated with the current version.
  // this temporary stats will be used in compaction.
  void updatetemporarystats();

  // sort all files for this version based on their file size and
  // record results in files_by_size_. the largest files are listed first.
  void updatefilesbysize();

  columnfamilydata* cfd_;  // columnfamilydata to which this version belongs
  const internalkeycomparator* internal_comparator_;
  const comparator* user_comparator_;
  tablecache* table_cache_;
  const mergeoperator* merge_operator_;

  autovector<filelevel> file_levels_;   // a copy of list of files per level
  logger* info_log_;
  statistics* db_statistics_;
  int num_levels_;              // number of levels
  int num_non_empty_levels_;    // number of levels. any level larger than it
                                // is guaranteed to be empty.
  fileindexer file_indexer_;
  versionset* vset_;            // versionset to which this version belongs
  arena arena_;                 // used to allocate space for file_levels_
  version* next_;               // next version in linked list
  version* prev_;               // previous version in linked list
  int refs_;                    // number of live refs to this version

  // list of files per level, files in each level are arranged
  // in increasing order of keys
  std::vector<filemetadata*>* files_;

  // a list for the same set of files that are stored in files_,
  // but files in each level are now sorted based on file
  // size. the file with the largest size is at the front.
  // this vector stores the index of the file from files_.
  std::vector<std::vector<int>> files_by_size_;

  // an index into files_by_size_ that specifies the first
  // file that is not yet compacted
  std::vector<int> next_file_to_compact_by_size_;

  // only the first few entries of files_by_size_ are sorted.
  // there is no need to sort all the files because it is likely
  // that on a running system, we need to look at only the first
  // few largest files because a new version is created every few
  // seconds/minutes (because of concurrent compactions).
  static const size_t number_of_files_to_sort_ = 50;

  // level that should be compacted next and its compaction score.
  // score < 1 means compaction is not strictly needed.  these fields
  // are initialized by finalize().
  // the most critical level to be compacted is listed first
  // these are used to pick the best compaction level
  std::vector<double> compaction_score_;
  std::vector<int> compaction_level_;
  double max_compaction_score_; // max score in l1 to ln-1
  int max_compaction_score_level_; // level on which max score occurs

  // a version number that uniquely represents this version. this is
  // used for debugging and logging purposes only.
  uint64_t version_number_;

  version(columnfamilydata* cfd, versionset* vset, uint64_t version_number = 0);

  // total file size
  uint64_t total_file_size_;
  // the total size of all raw keys.
  uint64_t total_raw_key_size_;
  // the total size of all raw values.
  uint64_t total_raw_value_size_;
  // total number of non-deletion entries
  uint64_t num_non_deletions_;
  // total number of deletion entries
  uint64_t num_deletions_;

  ~version();

  // re-initializes the index that is used to offset into files_by_size_
  // to find the next compaction candidate file.
  void resetnextcompactionindex(int level) {
    next_file_to_compact_by_size_[level] = 0;
  }

  // no copying allowed
  version(const version&);
  void operator=(const version&);
};

class versionset {
 public:
  versionset(const std::string& dbname, const dboptions* options,
             const envoptions& storage_options, cache* table_cache);
  ~versionset();

  // apply *edit to the current version to form a new descriptor that
  // is both saved to persistent state and installed as the new
  // current version.  will release *mu while actually writing to the file.
  // column_family_options has to be set if edit is column family add
  // requires: *mu is held on entry.
  // requires: no other thread concurrently calls logandapply()
  status logandapply(columnfamilydata* column_family_data, versionedit* edit,
                     port::mutex* mu, directory* db_directory = nullptr,
                     bool new_descriptor_log = false,
                     const columnfamilyoptions* column_family_options =
                         nullptr);

  // recover the last saved descriptor from persistent storage.
  // if read_only == true, recover() will not complain if some column families
  // are not opened
  status recover(const std::vector<columnfamilydescriptor>& column_families,
                 bool read_only = false);

  // reads a manifest file and returns a list of column families in
  // column_families.
  static status listcolumnfamilies(std::vector<std::string>* column_families,
                                   const std::string& dbname, env* env);

#ifndef rocksdb_lite
  // try to reduce the number of levels. this call is valid when
  // only one level from the new max level to the old
  // max level containing files.
  // the call is static, since number of levels is immutable during
  // the lifetime of a rocksdb instance. it reduces number of levels
  // in a db by applying changes to manifest.
  // for example, a db currently has 7 levels [0-6], and a call to
  // to reduce to 5 [0-4] can only be executed when only one level
  // among [4-6] contains files.
  static status reducenumberoflevels(const std::string& dbname,
                                     const options* options,
                                     const envoptions& storage_options,
                                     int new_levels);

  // printf contents (for debugging)
  status dumpmanifest(options& options, std::string& manifestfilename,
                      bool verbose, bool hex = false);

#endif  // rocksdb_lite

  // return the current manifest file number
  uint64_t manifestfilenumber() const { return manifest_file_number_; }

  uint64_t pendingmanifestfilenumber() const {
    return pending_manifest_file_number_;
  }

  // allocate and return a new file number
  uint64_t newfilenumber() { return next_file_number_++; }

  // arrange to reuse "file_number" unless a newer file number has
  // already been allocated.
  // requires: "file_number" was returned by a call to newfilenumber().
  void reuselogfilenumber(uint64_t file_number) {
    if (next_file_number_ == file_number + 1) {
      next_file_number_ = file_number;
    }
  }

  // return the last sequence number.
  uint64_t lastsequence() const {
    return last_sequence_.load(std::memory_order_acquire);
  }

  // set the last sequence number to s.
  void setlastsequence(uint64_t s) {
    assert(s >= last_sequence_);
    last_sequence_.store(s, std::memory_order_release);
  }

  // mark the specified file number as used.
  void markfilenumberused(uint64_t number);

  // return the log file number for the log file that is currently
  // being compacted, or zero if there is no such log file.
  uint64_t prevlognumber() const { return prev_log_number_; }

  // returns the minimum log number such that all
  // log numbers less than or equal to it can be deleted
  uint64_t minlognumber() const {
    uint64_t min_log_num = std::numeric_limits<uint64_t>::max();
    for (auto cfd : *column_family_set_) {
      if (min_log_num > cfd->getlognumber()) {
        min_log_num = cfd->getlognumber();
      }
    }
    return min_log_num;
  }

  // create an iterator that reads over the compaction inputs for "*c".
  // the caller should delete the iterator when no longer needed.
  iterator* makeinputiterator(compaction* c);

  // add all files listed in any live version to *live.
  void addlivefiles(std::vector<filedescriptor>* live_list);

  // return the approximate offset in the database of the data for
  // "key" as of version "v".
  uint64_t approximateoffsetof(version* v, const internalkey& key);

  // return the size of the current manifest file
  uint64_t manifestfilesize() const { return manifest_file_size_; }

  // verify that the files that we started with for a compaction
  // still exist in the current version and in the same original level.
  // this ensures that a concurrent compaction did not erroneously
  // pick the same files to compact.
  bool verifycompactionfileconsistency(compaction* c);

  status getmetadataforfile(uint64_t number, int* filelevel,
                            filemetadata** metadata, columnfamilydata** cfd);

  void getlivefilesmetadata(
    std::vector<livefilemetadata> *metadata);

  void getobsoletefiles(std::vector<filemetadata*>* files);

  columnfamilyset* getcolumnfamilyset() { return column_family_set_.get(); }

 private:
  class builder;
  struct manifestwriter;

  friend class version;

  struct logreporter : public log::reader::reporter {
    status* status;
    virtual void corruption(size_t bytes, const status& s) {
      if (this->status->ok()) *this->status = s;
    }
  };

  // save current contents to *log
  status writesnapshot(log::writer* log);

  void appendversion(columnfamilydata* column_family_data, version* v);

  bool manifestcontains(uint64_t manifest_file_number,
                        const std::string& record) const;

  columnfamilydata* createcolumnfamily(const columnfamilyoptions& options,
                                       versionedit* edit);

  std::unique_ptr<columnfamilyset> column_family_set_;

  env* const env_;
  const std::string dbname_;
  const dboptions* const options_;
  uint64_t next_file_number_;
  uint64_t manifest_file_number_;
  uint64_t pending_manifest_file_number_;
  std::atomic<uint64_t> last_sequence_;
  uint64_t prev_log_number_;  // 0 or backing store for memtable being compacted

  // opened lazily
  unique_ptr<log::writer> descriptor_log_;

  // generates a increasing version number for every new version
  uint64_t current_version_number_;

  // queue of writers to the manifest file
  std::deque<manifestwriter*> manifest_writers_;

  // current size of manifest file
  uint64_t manifest_file_size_;

  std::vector<filemetadata*> obsolete_files_;

  // storage options for all reads and writes except compactions
  const envoptions& storage_options_;

  // storage options used for compactions. this is a copy of
  // storage_options_ but with readaheads set to readahead_compactions_.
  const envoptions storage_options_compactions_;

  // no copying allowed
  versionset(const versionset&);
  void operator=(const versionset&);

  void logandapplycfhelper(versionedit* edit);
  void logandapplyhelper(columnfamilydata* cfd, builder* b, version* v,
                         versionedit* edit, port::mutex* mu);
};

}  // namespace rocksdb
