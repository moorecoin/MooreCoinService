//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//

#ifndef rocksdb_lite
#include "util/hash_skiplist_rep.h"

#include "rocksdb/memtablerep.h"
#include "util/arena.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "port/port.h"
#include "port/atomic_pointer.h"
#include "util/murmurhash.h"
#include "db/memtable.h"
#include "db/skiplist.h"

namespace rocksdb {
namespace {

class hashskiplistrep : public memtablerep {
 public:
  hashskiplistrep(const memtablerep::keycomparator& compare, arena* arena,
                  const slicetransform* transform, size_t bucket_size,
                  int32_t skiplist_height, int32_t skiplist_branching_factor);

  virtual void insert(keyhandle handle) override;

  virtual bool contains(const char* key) const override;

  virtual size_t approximatememoryusage() override;

  virtual void get(const lookupkey& k, void* callback_args,
                   bool (*callback_func)(void* arg,
                                         const char* entry)) override;

  virtual ~hashskiplistrep();

  virtual memtablerep::iterator* getiterator(arena* arena = nullptr) override;

  virtual memtablerep::iterator* getdynamicprefixiterator(
      arena* arena = nullptr) override;

 private:
  friend class dynamiciterator;
  typedef skiplist<const char*, const memtablerep::keycomparator&> bucket;

  size_t bucket_size_;

  const int32_t skiplist_height_;
  const int32_t skiplist_branching_factor_;

  // maps slices (which are transformed user keys) to buckets of keys sharing
  // the same transform.
  port::atomicpointer* buckets_;

  // the user-supplied transform whose domain is the user keys.
  const slicetransform* transform_;

  const memtablerep::keycomparator& compare_;
  // immutable after construction
  arena* const arena_;

  inline size_t gethash(const slice& slice) const {
    return murmurhash(slice.data(), slice.size(), 0) % bucket_size_;
  }
  inline bucket* getbucket(size_t i) const {
    return static_cast<bucket*>(buckets_[i].acquire_load());
  }
  inline bucket* getbucket(const slice& slice) const {
    return getbucket(gethash(slice));
  }
  // get a bucket from buckets_. if the bucket hasn't been initialized yet,
  // initialize it before returning.
  bucket* getinitializedbucket(const slice& transformed);

  class iterator : public memtablerep::iterator {
   public:
    explicit iterator(bucket* list, bool own_list = true,
                      arena* arena = nullptr)
        : list_(list), iter_(list), own_list_(own_list), arena_(arena) {}

    virtual ~iterator() {
      // if we own the list, we should also delete it
      if (own_list_) {
        assert(list_ != nullptr);
        delete list_;
      }
    }

    // returns true iff the iterator is positioned at a valid node.
    virtual bool valid() const {
      return list_ != nullptr && iter_.valid();
    }

    // returns the key at the current position.
    // requires: valid()
    virtual const char* key() const {
      assert(valid());
      return iter_.key();
    }

    // advances to the next position.
    // requires: valid()
    virtual void next() {
      assert(valid());
      iter_.next();
    }

    // advances to the previous position.
    // requires: valid()
    virtual void prev() {
      assert(valid());
      iter_.prev();
    }

    // advance to the first entry with a key >= target
    virtual void seek(const slice& internal_key, const char* memtable_key) {
      if (list_ != nullptr) {
        const char* encoded_key =
            (memtable_key != nullptr) ?
                memtable_key : encodekey(&tmp_, internal_key);
        iter_.seek(encoded_key);
      }
    }

    // position at the first entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektofirst() {
      if (list_ != nullptr) {
        iter_.seektofirst();
      }
    }

    // position at the last entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektolast() {
      if (list_ != nullptr) {
        iter_.seektolast();
      }
    }
   protected:
    void reset(bucket* list) {
      if (own_list_) {
        assert(list_ != nullptr);
        delete list_;
      }
      list_ = list;
      iter_.setlist(list);
      own_list_ = false;
    }
   private:
    // if list_ is nullptr, we should never call any methods on iter_
    // if list_ is nullptr, this iterator is not valid()
    bucket* list_;
    bucket::iterator iter_;
    // here we track if we own list_. if we own it, we are also
    // responsible for it's cleaning. this is a poor man's shared_ptr
    bool own_list_;
    std::unique_ptr<arena> arena_;
    std::string tmp_;       // for passing to encodekey
  };

  class dynamiciterator : public hashskiplistrep::iterator {
   public:
    explicit dynamiciterator(const hashskiplistrep& memtable_rep)
      : hashskiplistrep::iterator(nullptr, false),
        memtable_rep_(memtable_rep) {}

    // advance to the first entry with a key >= target
    virtual void seek(const slice& k, const char* memtable_key) {
      auto transformed = memtable_rep_.transform_->transform(extractuserkey(k));
      reset(memtable_rep_.getbucket(transformed));
      hashskiplistrep::iterator::seek(k, memtable_key);
    }

    // position at the first entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektofirst() {
      // prefix iterator does not support total order.
      // we simply set the iterator to invalid state
      reset(nullptr);
    }

    // position at the last entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektolast() {
      // prefix iterator does not support total order.
      // we simply set the iterator to invalid state
      reset(nullptr);
    }
   private:
    // the underlying memtable
    const hashskiplistrep& memtable_rep_;
  };

  class emptyiterator : public memtablerep::iterator {
    // this is used when there wasn't a bucket. it is cheaper than
    // instantiating an empty bucket over which to iterate.
   public:
    emptyiterator() { }
    virtual bool valid() const {
      return false;
    }
    virtual const char* key() const {
      assert(false);
      return nullptr;
    }
    virtual void next() { }
    virtual void prev() { }
    virtual void seek(const slice& internal_key,
                      const char* memtable_key) { }
    virtual void seektofirst() { }
    virtual void seektolast() { }
   private:
  };
};

hashskiplistrep::hashskiplistrep(const memtablerep::keycomparator& compare,
                                 arena* arena, const slicetransform* transform,
                                 size_t bucket_size, int32_t skiplist_height,
                                 int32_t skiplist_branching_factor)
    : memtablerep(arena),
      bucket_size_(bucket_size),
      skiplist_height_(skiplist_height),
      skiplist_branching_factor_(skiplist_branching_factor),
      transform_(transform),
      compare_(compare),
      arena_(arena) {
  auto mem =
      arena->allocatealigned(sizeof(port::atomicpointer) * bucket_size);
  buckets_ = new (mem) port::atomicpointer[bucket_size];

  for (size_t i = 0; i < bucket_size_; ++i) {
    buckets_[i].nobarrier_store(nullptr);
  }
}

hashskiplistrep::~hashskiplistrep() {
}

hashskiplistrep::bucket* hashskiplistrep::getinitializedbucket(
    const slice& transformed) {
  size_t hash = gethash(transformed);
  auto bucket = getbucket(hash);
  if (bucket == nullptr) {
    auto addr = arena_->allocatealigned(sizeof(bucket));
    bucket = new (addr) bucket(compare_, arena_, skiplist_height_,
                               skiplist_branching_factor_);
    buckets_[hash].release_store(static_cast<void*>(bucket));
  }
  return bucket;
}

void hashskiplistrep::insert(keyhandle handle) {
  auto* key = static_cast<char*>(handle);
  assert(!contains(key));
  auto transformed = transform_->transform(userkey(key));
  auto bucket = getinitializedbucket(transformed);
  bucket->insert(key);
}

bool hashskiplistrep::contains(const char* key) const {
  auto transformed = transform_->transform(userkey(key));
  auto bucket = getbucket(transformed);
  if (bucket == nullptr) {
    return false;
  }
  return bucket->contains(key);
}

size_t hashskiplistrep::approximatememoryusage() {
  return 0;
}

void hashskiplistrep::get(const lookupkey& k, void* callback_args,
                          bool (*callback_func)(void* arg, const char* entry)) {
  auto transformed = transform_->transform(k.user_key());
  auto bucket = getbucket(transformed);
  if (bucket != nullptr) {
    bucket::iterator iter(bucket);
    for (iter.seek(k.memtable_key().data());
         iter.valid() && callback_func(callback_args, iter.key());
         iter.next()) {
    }
  }
}

memtablerep::iterator* hashskiplistrep::getiterator(arena* arena) {
  // allocate a new arena of similar size to the one currently in use
  arena* new_arena = new arena(arena_->blocksize());
  auto list = new bucket(compare_, new_arena);
  for (size_t i = 0; i < bucket_size_; ++i) {
    auto bucket = getbucket(i);
    if (bucket != nullptr) {
      bucket::iterator itr(bucket);
      for (itr.seektofirst(); itr.valid(); itr.next()) {
        list->insert(itr.key());
      }
    }
  }
  if (arena == nullptr) {
    return new iterator(list, true, new_arena);
  } else {
    auto mem = arena->allocatealigned(sizeof(iterator));
    return new (mem) iterator(list, true, new_arena);
  }
}

memtablerep::iterator* hashskiplistrep::getdynamicprefixiterator(arena* arena) {
  if (arena == nullptr) {
    return new dynamiciterator(*this);
  } else {
    auto mem = arena->allocatealigned(sizeof(dynamiciterator));
    return new (mem) dynamiciterator(*this);
  }
}

} // anon namespace

memtablerep* hashskiplistrepfactory::creatememtablerep(
    const memtablerep::keycomparator& compare, arena* arena,
    const slicetransform* transform, logger* logger) {
  return new hashskiplistrep(compare, arena, transform, bucket_count_,
                             skiplist_height_, skiplist_branching_factor_);
}

memtablerepfactory* newhashskiplistrepfactory(
    size_t bucket_count, int32_t skiplist_height,
    int32_t skiplist_branching_factor) {
  return new hashskiplistrepfactory(bucket_count, skiplist_height,
      skiplist_branching_factor);
}

} // namespace rocksdb
#endif  // rocksdb_lite
