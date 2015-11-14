//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "rocksdb/iterator.h"
#include "table/iterator_wrapper.h"
#include "util/arena.h"

namespace rocksdb {

iterator::iterator() {
  cleanup_.function = nullptr;
  cleanup_.next = nullptr;
}

iterator::~iterator() {
  if (cleanup_.function != nullptr) {
    (*cleanup_.function)(cleanup_.arg1, cleanup_.arg2);
    for (cleanup* c = cleanup_.next; c != nullptr; ) {
      (*c->function)(c->arg1, c->arg2);
      cleanup* next = c->next;
      delete c;
      c = next;
    }
  }
}

void iterator::registercleanup(cleanupfunction func, void* arg1, void* arg2) {
  assert(func != nullptr);
  cleanup* c;
  if (cleanup_.function == nullptr) {
    c = &cleanup_;
  } else {
    c = new cleanup;
    c->next = cleanup_.next;
    cleanup_.next = c;
  }
  c->function = func;
  c->arg1 = arg1;
  c->arg2 = arg2;
}

namespace {
class emptyiterator : public iterator {
 public:
  explicit emptyiterator(const status& s) : status_(s) { }
  virtual bool valid() const { return false; }
  virtual void seek(const slice& target) { }
  virtual void seektofirst() { }
  virtual void seektolast() { }
  virtual void next() { assert(false); }
  virtual void prev() { assert(false); }
  slice key() const { assert(false); return slice(); }
  slice value() const { assert(false); return slice(); }
  virtual status status() const { return status_; }
 private:
  status status_;
};
}  // namespace

iterator* newemptyiterator() {
  return new emptyiterator(status::ok());
}

iterator* newemptyiterator(arena* arena) {
  if (arena == nullptr) {
    return newemptyiterator();
  } else {
    auto mem = arena->allocatealigned(sizeof(emptyiterator));
    return new (mem) emptyiterator(status::ok());
  }
}

iterator* newerroriterator(const status& status) {
  return new emptyiterator(status);
}

iterator* newerroriterator(const status& status, arena* arena) {
  if (arena == nullptr) {
    return newerroriterator(status);
  } else {
    auto mem = arena->allocatealigned(sizeof(emptyiterator));
    return new (mem) emptyiterator(status);
  }
}

}  // namespace rocksdb
