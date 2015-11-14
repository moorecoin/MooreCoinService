//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#pragma once

#include <string>
#include <list>
#include <vector>
#include <set>
#include <deque>
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/iterator.h"

#include "db/dbformat.h"
#include "db/filename.h"
#include "db/skiplist.h"
#include "db/memtable.h"
#include "rocksdb/db.h"
#include "rocksdb/iterator.h"
#include "rocksdb/options.h"
#include "util/autovector.h"
#include "util/log_buffer.h"

namespace rocksdb {

class columnfamilydata;
class internalkeycomparator;
class mutex;
class mergeiteratorbuilder;

// keeps a list of immutable memtables in a vector. the list is immutable
// if refcount is bigger than one. it is used as a state for get() and
// iterator code paths
class memtablelistversion {
 public:
  explicit memtablelistversion(memtablelistversion* old = nullptr);

  void ref();
  void unref(autovector<memtable*>* to_delete = nullptr);

  int size() const;

  // search all the memtables starting from the most recent one.
  // return the most recent value found, if any.
  bool get(const lookupkey& key, std::string* value, status* s,
           mergecontext& merge_context, const options& options);

  void additerators(const readoptions& options,
                    std::vector<iterator*>* iterator_list);

  void additerators(const readoptions& options,
                    mergeiteratorbuilder* merge_iter_builder);

  uint64_t gettotalnumentries() const;

 private:
  // require: m is mutable memtable
  void add(memtable* m);
  // require: m is mutable memtable
  void remove(memtable* m);

  friend class memtablelist;
  std::list<memtable*> memlist_;
  int size_ = 0;
  int refs_ = 0;
};

// this class stores references to all the immutable memtables.
// the memtables are flushed to l0 as soon as possible and in
// any order. if there are more than one immutable memtable, their
// flushes can occur concurrently.  however, they are 'committed'
// to the manifest in fifo order to maintain correctness and
// recoverability from a crash.
class memtablelist {
 public:
  // a list of memtables.
  explicit memtablelist(int min_write_buffer_number_to_merge)
      : min_write_buffer_number_to_merge_(min_write_buffer_number_to_merge),
        current_(new memtablelistversion()),
        num_flush_not_started_(0),
        commit_in_progress_(false),
        flush_requested_(false) {
    imm_flush_needed.release_store(nullptr);
    current_->ref();
  }
  ~memtablelist() {}

  memtablelistversion* current() { return current_; }

  // so that background threads can detect non-nullptr pointer to
  // determine whether there is anything more to start flushing.
  port::atomicpointer imm_flush_needed;

  // returns the total number of memtables in the list
  int size() const;

  // returns true if there is at least one memtable on which flush has
  // not yet started.
  bool isflushpending() const;

  // returns the earliest memtables that needs to be flushed. the returned
  // memtables are guaranteed to be in the ascending order of created time.
  void pickmemtablestoflush(autovector<memtable*>* mems);

  // reset status of the given memtable list back to pending state so that
  // they can get picked up again on the next round of flush.
  void rollbackmemtableflush(const autovector<memtable*>& mems,
                             uint64_t file_number,
                             filenumtopathidmap* pending_outputs);

  // commit a successful flush in the manifest file
  status installmemtableflushresults(
      columnfamilydata* cfd, const autovector<memtable*>& m, versionset* vset,
      port::mutex* mu, logger* info_log, uint64_t file_number,
      filenumtopathidmap* pending_outputs, autovector<memtable*>* to_delete,
      directory* db_directory, logbuffer* log_buffer);

  // new memtables are inserted at the front of the list.
  // takes ownership of the referenced held on *m by the caller of add().
  void add(memtable* m);

  // returns an estimate of the number of bytes of data in use.
  size_t approximatememoryusage();

  // request a flush of all existing memtables to storage
  void flushrequested() { flush_requested_ = true; }

  // copying allowed
  // memtablelist(const memtablelist&);
  // void operator=(const memtablelist&);

 private:
  // db mutex held
  void installnewversion();

  int min_write_buffer_number_to_merge_;

  memtablelistversion* current_;

  // the number of elements that still need flushing
  int num_flush_not_started_;

  // committing in progress
  bool commit_in_progress_;

  // requested a flush of all memtables to storage
  bool flush_requested_;

};

}  // namespace rocksdb
