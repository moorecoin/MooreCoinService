// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_hyperleveldb_db_memtable_h_
#define storage_hyperleveldb_db_memtable_h_

#include <string>
#include "../hyperleveldb/db.h"
#include "dbformat.h"
#include "skiplist.h"
#include "../util/arena.h"

namespace hyperleveldb {

class internalkeycomparator;
class mutex;
class memtableiterator;

class memtable {
 public:
  // memtables are reference counted.  the initial reference count
  // is zero and the caller must call ref() at least once.
  explicit memtable(const internalkeycomparator& comparator);

  // increase reference count.
  // xxx use a release increment if not using gcc intrinsics
  void ref() { __sync_add_and_fetch(&refs_, 1); }

  // drop reference count.  delete if no more references exist.
  // xxx use an acquire decrement if not using gcc intrinsics
  void unref() {
    int refs = __sync_sub_and_fetch(&refs_, 1);
    assert(refs >= 0);
    if (refs <= 0) {
      delete this;
    }
  }

  // returns an estimate of the number of bytes of data in use by this
  // data structure.
  //
  // requires: external synchronization to prevent simultaneous
  // operations on the same memtable.
  size_t approximatememoryusage();

  // return an iterator that yields the contents of the memtable.
  //
  // the caller must ensure that the underlying memtable remains live
  // while the returned iterator is live.  the keys returned by this
  // iterator are internal keys encoded by appendinternalkey in the
  // db/format.{h,cc} module.
  iterator* newiterator();

  // add an entry into memtable that maps key to value at the
  // specified sequence number and with the specified type.
  // typically value will be empty if type==ktypedeletion.
  void add(sequencenumber seq, valuetype type,
           const slice& key,
           const slice& value);

  // if memtable contains a value for key, store it in *value and return true.
  // if memtable contains a deletion for key, store a notfound() error
  // in *status and return true.
  // else, return false.
  bool get(const lookupkey& key, std::string* value, status* s);

 private:
  ~memtable();  // private since only unref() should be used to delete it
  typedef std::pair<uint64_t, const char*> tablekey;

  struct keycomparator {
    const internalkeycomparator comparator;
    explicit keycomparator(const internalkeycomparator& c) : comparator(c) { }
    int operator()(tablekey a, tablekey b) const;
  };
  friend class memtableiterator;
  friend class memtablebackwarditerator;

  typedef skiplist<tablekey, keycomparator> table;

  keycomparator comparator_;
  int refs_;
  port::mutex mtx_;
  arena arena_;
  table table_;

  // no copying allowed
  memtable(const memtable&);
  void operator=(const memtable&);
};

}  // namespace hyperleveldb

#endif  // storage_hyperleveldb_db_memtable_h_
