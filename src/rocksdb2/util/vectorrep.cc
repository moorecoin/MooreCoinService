//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#ifndef rocksdb_lite
#include "rocksdb/memtablerep.h"

#include <unordered_set>
#include <set>
#include <memory>
#include <algorithm>
#include <type_traits>

#include "util/arena.h"
#include "db/memtable.h"
#include "port/port.h"
#include "util/mutexlock.h"
#include "util/stl_wrappers.h"

namespace rocksdb {
namespace {

using namespace stl_wrappers;

class vectorrep : public memtablerep {
 public:
  vectorrep(const keycomparator& compare, arena* arena, size_t count);

  // insert key into the collection. (the caller will pack key and value into a
  // single buffer and pass that in as the parameter to insert)
  // requires: nothing that compares equal to key is currently in the
  // collection.
  virtual void insert(keyhandle handle) override;

  // returns true iff an entry that compares equal to key is in the collection.
  virtual bool contains(const char* key) const override;

  virtual void markreadonly() override;

  virtual size_t approximatememoryusage() override;

  virtual void get(const lookupkey& k, void* callback_args,
                   bool (*callback_func)(void* arg,
                                         const char* entry)) override;

  virtual ~vectorrep() override { }

  class iterator : public memtablerep::iterator {
    class vectorrep* vrep_;
    std::shared_ptr<std::vector<const char*>> bucket_;
    typename std::vector<const char*>::const_iterator mutable cit_;
    const keycomparator& compare_;
    std::string tmp_;       // for passing to encodekey
    bool mutable sorted_;
    void dosort() const;
   public:
    explicit iterator(class vectorrep* vrep,
      std::shared_ptr<std::vector<const char*>> bucket,
      const keycomparator& compare);

    // initialize an iterator over the specified collection.
    // the returned iterator is not valid.
    // explicit iterator(const memtablerep* collection);
    virtual ~iterator() override { };

    // returns true iff the iterator is positioned at a valid node.
    virtual bool valid() const override;

    // returns the key at the current position.
    // requires: valid()
    virtual const char* key() const override;

    // advances to the next position.
    // requires: valid()
    virtual void next() override;

    // advances to the previous position.
    // requires: valid()
    virtual void prev() override;

    // advance to the first entry with a key >= target
    virtual void seek(const slice& user_key, const char* memtable_key) override;

    // position at the first entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektofirst() override;

    // position at the last entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektolast() override;
  };

  // return an iterator over the keys in this representation.
  virtual memtablerep::iterator* getiterator(arena* arena) override;

 private:
  friend class iterator;
  typedef std::vector<const char*> bucket;
  std::shared_ptr<bucket> bucket_;
  mutable port::rwmutex rwlock_;
  bool immutable_;
  bool sorted_;
  const keycomparator& compare_;
};

void vectorrep::insert(keyhandle handle) {
  auto* key = static_cast<char*>(handle);
  writelock l(&rwlock_);
  assert(!immutable_);
  bucket_->push_back(key);
}

// returns true iff an entry that compares equal to key is in the collection.
bool vectorrep::contains(const char* key) const {
  readlock l(&rwlock_);
  return std::find(bucket_->begin(), bucket_->end(), key) != bucket_->end();
}

void vectorrep::markreadonly() {
  writelock l(&rwlock_);
  immutable_ = true;
}

size_t vectorrep::approximatememoryusage() {
  return
    sizeof(bucket_) + sizeof(*bucket_) +
    bucket_->size() *
    sizeof(
      std::remove_reference<decltype(*bucket_)>::type::value_type
    );
}

vectorrep::vectorrep(const keycomparator& compare, arena* arena, size_t count)
  : memtablerep(arena),
    bucket_(new bucket()),
    immutable_(false),
    sorted_(false),
    compare_(compare) { bucket_.get()->reserve(count); }

vectorrep::iterator::iterator(class vectorrep* vrep,
                   std::shared_ptr<std::vector<const char*>> bucket,
                   const keycomparator& compare)
: vrep_(vrep),
  bucket_(bucket),
  cit_(bucket_->end()),
  compare_(compare),
  sorted_(false) { }

void vectorrep::iterator::dosort() const {
  // vrep is non-null means that we are working on an immutable memtable
  if (!sorted_ && vrep_ != nullptr) {
    writelock l(&vrep_->rwlock_);
    if (!vrep_->sorted_) {
      std::sort(bucket_->begin(), bucket_->end(), compare(compare_));
      cit_ = bucket_->begin();
      vrep_->sorted_ = true;
    }
    sorted_ = true;
  }
  if (!sorted_) {
    std::sort(bucket_->begin(), bucket_->end(), compare(compare_));
    cit_ = bucket_->begin();
    sorted_ = true;
  }
  assert(sorted_);
  assert(vrep_ == nullptr || vrep_->sorted_);
}

// returns true iff the iterator is positioned at a valid node.
bool vectorrep::iterator::valid() const {
  dosort();
  return cit_ != bucket_->end();
}

// returns the key at the current position.
// requires: valid()
const char* vectorrep::iterator::key() const {
  assert(valid());
  return *cit_;
}

// advances to the next position.
// requires: valid()
void vectorrep::iterator::next() {
  assert(valid());
  if (cit_ == bucket_->end()) {
    return;
  }
  ++cit_;
}

// advances to the previous position.
// requires: valid()
void vectorrep::iterator::prev() {
  assert(valid());
  if (cit_ == bucket_->begin()) {
    // if you try to go back from the first element, the iterator should be
    // invalidated. so we set it to past-the-end. this means that you can
    // treat the container circularly.
    cit_ = bucket_->end();
  } else {
    --cit_;
  }
}

// advance to the first entry with a key >= target
void vectorrep::iterator::seek(const slice& user_key,
                               const char* memtable_key) {
  dosort();
  // do binary search to find first value not less than the target
  const char* encoded_key =
      (memtable_key != nullptr) ? memtable_key : encodekey(&tmp_, user_key);
  cit_ = std::equal_range(bucket_->begin(),
                          bucket_->end(),
                          encoded_key,
                          [this] (const char* a, const char* b) {
                            return compare_(a, b) < 0;
                          }).first;
}

// position at the first entry in collection.
// final state of iterator is valid() iff collection is not empty.
void vectorrep::iterator::seektofirst() {
  dosort();
  cit_ = bucket_->begin();
}

// position at the last entry in collection.
// final state of iterator is valid() iff collection is not empty.
void vectorrep::iterator::seektolast() {
  dosort();
  cit_ = bucket_->end();
  if (bucket_->size() != 0) {
    --cit_;
  }
}

void vectorrep::get(const lookupkey& k, void* callback_args,
                    bool (*callback_func)(void* arg, const char* entry)) {
  rwlock_.readlock();
  vectorrep* vector_rep;
  std::shared_ptr<bucket> bucket;
  if (immutable_) {
    vector_rep = this;
  } else {
    vector_rep = nullptr;
    bucket.reset(new bucket(*bucket_));  // make a copy
  }
  vectorrep::iterator iter(vector_rep, immutable_ ? bucket_ : bucket, compare_);
  rwlock_.readunlock();

  for (iter.seek(k.user_key(), k.memtable_key().data());
       iter.valid() && callback_func(callback_args, iter.key()); iter.next()) {
  }
}

memtablerep::iterator* vectorrep::getiterator(arena* arena) {
  char* mem = nullptr;
  if (arena != nullptr) {
    mem = arena->allocatealigned(sizeof(iterator));
  }
  readlock l(&rwlock_);
  // do not sort here. the sorting would be done the first time
  // a seek is performed on the iterator.
  if (immutable_) {
    if (arena == nullptr) {
      return new iterator(this, bucket_, compare_);
    } else {
      return new (mem) iterator(this, bucket_, compare_);
    }
  } else {
    std::shared_ptr<bucket> tmp;
    tmp.reset(new bucket(*bucket_)); // make a copy
    if (arena == nullptr) {
      return new iterator(nullptr, tmp, compare_);
    } else {
      return new (mem) iterator(nullptr, tmp, compare_);
    }
  }
}
} // anon namespace

memtablerep* vectorrepfactory::creatememtablerep(
    const memtablerep::keycomparator& compare, arena* arena,
    const slicetransform*, logger* logger) {
  return new vectorrep(compare, arena, count_);
}
} // namespace rocksdb
#endif  // rocksdb_lite
