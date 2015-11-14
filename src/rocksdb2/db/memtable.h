//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include <string>
#include <memory>
#include <deque>
#include "db/dbformat.h"
#include "db/skiplist.h"
#include "db/version_edit.h"
#include "rocksdb/db.h"
#include "rocksdb/memtablerep.h"
#include "util/arena.h"
#include "util/dynamic_bloom.h"

namespace rocksdb {

class arena;
class mutex;
class memtableiterator;
class mergecontext;

class memtable {
 public:
  struct keycomparator : public memtablerep::keycomparator {
    const internalkeycomparator comparator;
    explicit keycomparator(const internalkeycomparator& c) : comparator(c) { }
    virtual int operator()(const char* prefix_len_key1,
                           const char* prefix_len_key2) const;
    virtual int operator()(const char* prefix_len_key,
                           const slice& key) const override;
  };

  // memtables are reference counted.  the initial reference count
  // is zero and the caller must call ref() at least once.
  explicit memtable(const internalkeycomparator& comparator,
                    const options& options);

  ~memtable();

  // increase reference count.
  void ref() { ++refs_; }

  // drop reference count.
  // if the refcount goes to zero return this memtable, otherwise return null
  memtable* unref() {
    --refs_;
    assert(refs_ >= 0);
    if (refs_ <= 0) {
      return this;
    }
    return nullptr;
  }

  // returns an estimate of the number of bytes of data in use by this
  // data structure.
  //
  // requires: external synchronization to prevent simultaneous
  // operations on the same memtable.
  size_t approximatememoryusage();

  // this method heuristically determines if the memtable should continue to
  // host more data.
  bool shouldflush() const { return should_flush_; }

  // return an iterator that yields the contents of the memtable.
  //
  // the caller must ensure that the underlying memtable remains live
  // while the returned iterator is live.  the keys returned by this
  // iterator are internal keys encoded by appendinternalkey in the
  // db/dbformat.{h,cc} module.
  //
  // by default, it returns an iterator for prefix seek if prefix_extractor
  // is configured in options.
  // arena: if not null, the arena needs to be used to allocate the iterator.
  //        calling ~iterator of the iterator will destroy all the states but
  //        those allocated in arena.
  iterator* newiterator(const readoptions& options,
                        arena* arena = nullptr);

  // add an entry into memtable that maps key to value at the
  // specified sequence number and with the specified type.
  // typically value will be empty if type==ktypedeletion.
  void add(sequencenumber seq, valuetype type,
           const slice& key,
           const slice& value);

  // if memtable contains a value for key, store it in *value and return true.
  // if memtable contains a deletion for key, store a notfound() error
  // in *status and return true.
  // if memtable contains merge operation as the most recent entry for a key,
  //   and the merge process does not stop (not reaching a value or delete),
  //   prepend the current merge operand to *operands.
  //   store mergeinprogress in s, and return false.
  // else, return false.
  bool get(const lookupkey& key, std::string* value, status* s,
           mergecontext& merge_context, const options& options);

  // attempts to update the new_value inplace, else does normal add
  // pseudocode
  //   if key exists in current memtable && prev_value is of type ktypevalue
  //     if new sizeof(new_value) <= sizeof(prev_value)
  //       update inplace
  //     else add(key, new_value)
  //   else add(key, new_value)
  void update(sequencenumber seq,
              const slice& key,
              const slice& value);

  // if prev_value for key exits, attempts to update it inplace.
  // else returns false
  // pseudocode
  //   if key exists in current memtable && prev_value is of type ktypevalue
  //     new_value = delta(prev_value)
  //     if sizeof(new_value) <= sizeof(prev_value)
  //       update inplace
  //     else add(key, new_value)
  //   else return false
  bool updatecallback(sequencenumber seq,
                      const slice& key,
                      const slice& delta,
                      const options& options);

  // returns the number of successive merge entries starting from the newest
  // entry for the key up to the last non-merge entry or last entry for the
  // key in the memtable.
  size_t countsuccessivemergeentries(const lookupkey& key);

  // get total number of entries in the mem table.
  uint64_t getnumentries() const { return num_entries_; }

  // returns the edits area that is needed for flushing the memtable
  versionedit* getedits() { return &edit_; }

  // returns the sequence number of the first element that was inserted
  // into the memtable
  sequencenumber getfirstsequencenumber() { return first_seqno_; }

  // returns the next active logfile number when this memtable is about to
  // be flushed to storage
  uint64_t getnextlognumber() { return mem_next_logfile_number_; }

  // sets the next active logfile number when this memtable is about to
  // be flushed to storage
  void setnextlognumber(uint64_t num) { mem_next_logfile_number_ = num; }

  // notify the underlying storage that no more items will be added
  void markimmutable() { table_->markreadonly(); }

  // return true if the current memtablerep supports merge operator.
  bool ismergeoperatorsupported() const {
    return table_->ismergeoperatorsupported();
  }

  // return true if the current memtablerep supports snapshots.
  bool issnapshotsupported() const { return table_->issnapshotsupported(); }

  // get the lock associated for the key
  port::rwmutex* getlock(const slice& key);

  const internalkeycomparator& getinternalkeycomparator() const {
    return comparator_.comparator;
  }

  const arena& test_getarena() const { return arena_; }

 private:
  // dynamically check if we can add more incoming entries.
  bool shouldflushnow() const;

  friend class memtableiterator;
  friend class memtablebackwarditerator;
  friend class memtablelist;

  keycomparator comparator_;
  int refs_;
  const size_t karenablocksize;
  const size_t kwritebuffersize;
  arena arena_;
  unique_ptr<memtablerep> table_;

  uint64_t num_entries_;

  // these are used to manage memtable flushes to storage
  bool flush_in_progress_; // started the flush
  bool flush_completed_;   // finished the flush
  uint64_t file_number_;    // filled up after flush is complete

  // the updates to be applied to the transaction log when this
  // memtable is flushed to storage.
  versionedit edit_;

  // the sequence number of the kv that was inserted first
  sequencenumber first_seqno_;

  // the log files earlier than this number can be deleted.
  uint64_t mem_next_logfile_number_;

  // rw locks for inplace updates
  std::vector<port::rwmutex> locks_;

  // no copying allowed
  memtable(const memtable&);
  void operator=(const memtable&);

  const slicetransform* const prefix_extractor_;
  std::unique_ptr<dynamicbloom> prefix_bloom_;

  // a flag indicating if a memtable has met the criteria to flush
  bool should_flush_;
};

extern const char* encodekey(std::string* scratch, const slice& target);

}  // namespace rocksdb
