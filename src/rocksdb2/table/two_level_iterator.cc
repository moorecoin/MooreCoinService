//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "table/two_level_iterator.h"

#include "rocksdb/options.h"
#include "rocksdb/table.h"
#include "table/block.h"
#include "table/format.h"
#include "util/arena.h"

namespace rocksdb {

namespace {

class twoleveliterator: public iterator {
 public:
  explicit twoleveliterator(twoleveliteratorstate* state,
      iterator* first_level_iter);

  virtual ~twoleveliterator() {
    first_level_iter_.deleteiter(false);
    second_level_iter_.deleteiter(false);
  }

  virtual void seek(const slice& target);
  virtual void seektofirst();
  virtual void seektolast();
  virtual void next();
  virtual void prev();

  virtual bool valid() const {
    return second_level_iter_.valid();
  }
  virtual slice key() const {
    assert(valid());
    return second_level_iter_.key();
  }
  virtual slice value() const {
    assert(valid());
    return second_level_iter_.value();
  }
  virtual status status() const {
    // it'd be nice if status() returned a const status& instead of a status
    if (!first_level_iter_.status().ok()) {
      return first_level_iter_.status();
    } else if (second_level_iter_.iter() != nullptr &&
               !second_level_iter_.status().ok()) {
      return second_level_iter_.status();
    } else {
      return status_;
    }
  }

 private:
  void saveerror(const status& s) {
    if (status_.ok() && !s.ok()) status_ = s;
  }
  void skipemptydatablocksforward();
  void skipemptydatablocksbackward();
  void setsecondleveliterator(iterator* iter);
  void initdatablock();

  std::unique_ptr<twoleveliteratorstate> state_;
  iteratorwrapper first_level_iter_;
  iteratorwrapper second_level_iter_;  // may be nullptr
  status status_;
  // if second_level_iter is non-nullptr, then "data_block_handle_" holds the
  // "index_value" passed to block_function_ to create the second_level_iter.
  std::string data_block_handle_;
};

twoleveliterator::twoleveliterator(twoleveliteratorstate* state,
    iterator* first_level_iter)
  : state_(state), first_level_iter_(first_level_iter) {}

void twoleveliterator::seek(const slice& target) {
  if (state_->check_prefix_may_match &&
      !state_->prefixmaymatch(target)) {
    setsecondleveliterator(nullptr);
    return;
  }
  first_level_iter_.seek(target);

  initdatablock();
  if (second_level_iter_.iter() != nullptr) {
    second_level_iter_.seek(target);
  }
  skipemptydatablocksforward();
}

void twoleveliterator::seektofirst() {
  first_level_iter_.seektofirst();
  initdatablock();
  if (second_level_iter_.iter() != nullptr) {
    second_level_iter_.seektofirst();
  }
  skipemptydatablocksforward();
}

void twoleveliterator::seektolast() {
  first_level_iter_.seektolast();
  initdatablock();
  if (second_level_iter_.iter() != nullptr) {
    second_level_iter_.seektolast();
  }
  skipemptydatablocksbackward();
}

void twoleveliterator::next() {
  assert(valid());
  second_level_iter_.next();
  skipemptydatablocksforward();
}

void twoleveliterator::prev() {
  assert(valid());
  second_level_iter_.prev();
  skipemptydatablocksbackward();
}


void twoleveliterator::skipemptydatablocksforward() {
  while (second_level_iter_.iter() == nullptr ||
         (!second_level_iter_.valid() &&
         !second_level_iter_.status().isincomplete())) {
    // move to next block
    if (!first_level_iter_.valid()) {
      setsecondleveliterator(nullptr);
      return;
    }
    first_level_iter_.next();
    initdatablock();
    if (second_level_iter_.iter() != nullptr) {
      second_level_iter_.seektofirst();
    }
  }
}

void twoleveliterator::skipemptydatablocksbackward() {
  while (second_level_iter_.iter() == nullptr ||
         (!second_level_iter_.valid() &&
         !second_level_iter_.status().isincomplete())) {
    // move to next block
    if (!first_level_iter_.valid()) {
      setsecondleveliterator(nullptr);
      return;
    }
    first_level_iter_.prev();
    initdatablock();
    if (second_level_iter_.iter() != nullptr) {
      second_level_iter_.seektolast();
    }
  }
}

void twoleveliterator::setsecondleveliterator(iterator* iter) {
  if (second_level_iter_.iter() != nullptr) {
    saveerror(second_level_iter_.status());
  }
  second_level_iter_.set(iter);
}

void twoleveliterator::initdatablock() {
  if (!first_level_iter_.valid()) {
    setsecondleveliterator(nullptr);
  } else {
    slice handle = first_level_iter_.value();
    if (second_level_iter_.iter() != nullptr &&
        !second_level_iter_.status().isincomplete() &&
        handle.compare(data_block_handle_) == 0) {
      // second_level_iter is already constructed with this iterator, so
      // no need to change anything
    } else {
      iterator* iter = state_->newsecondaryiterator(handle);
      data_block_handle_.assign(handle.data(), handle.size());
      setsecondleveliterator(iter);
    }
  }
}

}  // namespace

iterator* newtwoleveliterator(twoleveliteratorstate* state,
                              iterator* first_level_iter, arena* arena) {
  if (arena == nullptr) {
    return new twoleveliterator(state, first_level_iter);
  } else {
    auto mem = arena->allocatealigned(sizeof(twoleveliterator));
    return new (mem) twoleveliterator(state, first_level_iter);
  }
}

}  // namespace rocksdb
