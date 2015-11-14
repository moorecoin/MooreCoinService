//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include <stdint.h>
#include "rocksdb/db.h"
#include "db/dbformat.h"
#include "util/arena.h"
#include "util/autovector.h"

namespace rocksdb {

class arena;
class dbiter;

// return a new iterator that converts internal keys (yielded by
// "*internal_iter") that were live at the specified "sequence" number
// into appropriate user keys.
extern iterator* newdbiterator(
    env* env,
    const options& options,
    const comparator *user_key_comparator,
    iterator* internal_iter,
    const sequencenumber& sequence);

// a wrapper iterator which wraps db iterator and the arena, with which the db
// iterator is supposed be allocated. this class is used as an entry point of
// a iterator hierarchy whose memory can be allocated inline. in that way,
// accessing the iterator tree can be more cache friendly. it is also faster
// to allocate.
class arenawrappeddbiter : public iterator {
 public:
  virtual ~arenawrappeddbiter();

  // get the arena to be used to allocate memory for dbiter to be wrapped,
  // as well as child iterators in it.
  virtual arena* getarena() { return &arena_; }

  // set the db iterator to be wrapped

  virtual void setdbiter(dbiter* iter);

  // set the internal iterator wrapped inside the db iterator. usually it is
  // a merging iterator.
  virtual void setiterunderdbiter(iterator* iter);
  virtual bool valid() const override;
  virtual void seektofirst() override;
  virtual void seektolast() override;
  virtual void seek(const slice& target) override;
  virtual void next() override;
  virtual void prev() override;
  virtual slice key() const override;
  virtual slice value() const override;
  virtual status status() const override;
  void registercleanup(cleanupfunction function, void* arg1, void* arg2);

 private:
  dbiter* db_iter_;
  arena arena_;
};

// generate the arena wrapped iterator class.
extern arenawrappeddbiter* newarenawrappeddbiterator(
    env* env, const options& options, const comparator* user_key_comparator,
    const sequencenumber& sequence);

}  // namespace rocksdb
