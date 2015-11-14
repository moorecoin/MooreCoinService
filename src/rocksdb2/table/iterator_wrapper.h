//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once

#include "rocksdb/iterator.h"

namespace rocksdb {

// a internal wrapper class with an interface similar to iterator that
// caches the valid() and key() results for an underlying iterator.
// this can help avoid virtual function calls and also gives better
// cache locality.
class iteratorwrapper {
 public:
  iteratorwrapper(): iter_(nullptr), valid_(false) { }
  explicit iteratorwrapper(iterator* iter): iter_(nullptr) {
    set(iter);
  }
  ~iteratorwrapper() {}
  iterator* iter() const { return iter_; }

  // takes ownership of "iter" and will delete it when destroyed, or
  // when set() is invoked again.
  void set(iterator* iter) {
    delete iter_;
    iter_ = iter;
    if (iter_ == nullptr) {
      valid_ = false;
    } else {
      update();
    }
  }

  void deleteiter(bool is_arena_mode) {
    if (!is_arena_mode) {
      delete iter_;
    } else {
      iter_->~iterator();
    }
  }

  // iterator interface methods
  bool valid() const        { return valid_; }
  slice key() const         { assert(valid()); return key_; }
  slice value() const       { assert(valid()); return iter_->value(); }
  // methods below require iter() != nullptr
  status status() const     { assert(iter_); return iter_->status(); }
  void next()               { assert(iter_); iter_->next();        update(); }
  void prev()               { assert(iter_); iter_->prev();        update(); }
  void seek(const slice& k) { assert(iter_); iter_->seek(k);       update(); }
  void seektofirst()        { assert(iter_); iter_->seektofirst(); update(); }
  void seektolast()         { assert(iter_); iter_->seektolast();  update(); }

 private:
  void update() {
    valid_ = iter_->valid();
    if (valid_) {
      key_ = iter_->key();
    }
  }

  iterator* iter_;
  bool valid_;
  slice key_;
};

class arena;
// return an empty iterator (yields nothing) allocated from arena.
extern iterator* newemptyiterator(arena* arena);

// return an empty iterator with the specified status, allocated arena.
extern iterator* newerroriterator(const status& status, arena* arena);

}  // namespace rocksdb
