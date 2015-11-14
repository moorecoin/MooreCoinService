//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "rocksdb/cache.h"
#include "port/port.h"
#include "util/autovector.h"
#include "util/hash.h"
#include "util/mutexlock.h"

namespace rocksdb {

cache::~cache() {
}

namespace {

// lru cache implementation

// an entry is a variable length heap-allocated structure.  entries
// are kept in a circular doubly linked list ordered by access time.
struct lruhandle {
  void* value;
  void (*deleter)(const slice&, void* value);
  lruhandle* next_hash;
  lruhandle* next;
  lruhandle* prev;
  size_t charge;      // todo(opt): only allow uint32_t?
  size_t key_length;
  uint32_t refs;
  uint32_t hash;      // hash of key(); used for fast sharding and comparisons
  char key_data[1];   // beginning of key

  slice key() const {
    // for cheaper lookups, we allow a temporary handle object
    // to store a pointer to a key in "value".
    if (next == this) {
      return *(reinterpret_cast<slice*>(value));
    } else {
      return slice(key_data, key_length);
    }
  }
};

// we provide our own simple hash table since it removes a whole bunch
// of porting hacks and is also faster than some of the built-in hash
// table implementations in some of the compiler/runtime combinations
// we have tested.  e.g., readrandom speeds up by ~5% over the g++
// 4.4.3's builtin hashtable.
class handletable {
 public:
  handletable() : length_(0), elems_(0), list_(nullptr) { resize(); }
  ~handletable() { delete[] list_; }

  lruhandle* lookup(const slice& key, uint32_t hash) {
    return *findpointer(key, hash);
  }

  lruhandle* insert(lruhandle* h) {
    lruhandle** ptr = findpointer(h->key(), h->hash);
    lruhandle* old = *ptr;
    h->next_hash = (old == nullptr ? nullptr : old->next_hash);
    *ptr = h;
    if (old == nullptr) {
      ++elems_;
      if (elems_ > length_) {
        // since each cache entry is fairly large, we aim for a small
        // average linked list length (<= 1).
        resize();
      }
    }
    return old;
  }

  lruhandle* remove(const slice& key, uint32_t hash) {
    lruhandle** ptr = findpointer(key, hash);
    lruhandle* result = *ptr;
    if (result != nullptr) {
      *ptr = result->next_hash;
      --elems_;
    }
    return result;
  }

 private:
  // the table consists of an array of buckets where each bucket is
  // a linked list of cache entries that hash into the bucket.
  uint32_t length_;
  uint32_t elems_;
  lruhandle** list_;

  // return a pointer to slot that points to a cache entry that
  // matches key/hash.  if there is no such cache entry, return a
  // pointer to the trailing slot in the corresponding linked list.
  lruhandle** findpointer(const slice& key, uint32_t hash) {
    lruhandle** ptr = &list_[hash & (length_ - 1)];
    while (*ptr != nullptr &&
           ((*ptr)->hash != hash || key != (*ptr)->key())) {
      ptr = &(*ptr)->next_hash;
    }
    return ptr;
  }

  void resize() {
    uint32_t new_length = 16;
    while (new_length < elems_ * 1.5) {
      new_length *= 2;
    }
    lruhandle** new_list = new lruhandle*[new_length];
    memset(new_list, 0, sizeof(new_list[0]) * new_length);
    uint32_t count = 0;
    for (uint32_t i = 0; i < length_; i++) {
      lruhandle* h = list_[i];
      while (h != nullptr) {
        lruhandle* next = h->next_hash;
        uint32_t hash = h->hash;
        lruhandle** ptr = &new_list[hash & (new_length - 1)];
        h->next_hash = *ptr;
        *ptr = h;
        h = next;
        count++;
      }
    }
    assert(elems_ == count);
    delete[] list_;
    list_ = new_list;
    length_ = new_length;
  }
};

// a single shard of sharded cache.
class lrucache {
 public:
  lrucache();
  ~lrucache();

  // separate from constructor so caller can easily make an array of lrucache
  void setcapacity(size_t capacity) { capacity_ = capacity; }
  void setremovescancountlimit(size_t remove_scan_count_limit) {
    remove_scan_count_limit_ = remove_scan_count_limit;
  }

  // like cache methods, but with an extra "hash" parameter.
  cache::handle* insert(const slice& key, uint32_t hash,
                        void* value, size_t charge,
                        void (*deleter)(const slice& key, void* value));
  cache::handle* lookup(const slice& key, uint32_t hash);
  void release(cache::handle* handle);
  void erase(const slice& key, uint32_t hash);
  // although in some platforms the update of size_t is atomic, to make sure
  // getusage() works correctly under any platforms, we'll protect this
  // function with mutex.
  size_t getusage() const {
    mutexlock l(&mutex_);
    return usage_;
  }

  void applytoallcacheentries(void (*callback)(void*, size_t),
                              bool thread_safe);

 private:
  void lru_remove(lruhandle* e);
  void lru_append(lruhandle* e);
  // just reduce the reference count by 1.
  // return true if last reference
  bool unref(lruhandle* e);
  // call deleter and free
  void freeentry(lruhandle* e);

  // initialized before use.
  size_t capacity_;
  uint32_t remove_scan_count_limit_;

  // mutex_ protects the following state.
  // we don't count mutex_ as the cache's internal state so semantically we
  // don't mind mutex_ invoking the non-const actions.
  mutable port::mutex mutex_;
  size_t usage_;

  // dummy head of lru list.
  // lru.prev is newest entry, lru.next is oldest entry.
  lruhandle lru_;

  handletable table_;
};

lrucache::lrucache()
    : usage_(0) {
  // make empty circular linked list
  lru_.next = &lru_;
  lru_.prev = &lru_;
}

lrucache::~lrucache() {
  for (lruhandle* e = lru_.next; e != &lru_; ) {
    lruhandle* next = e->next;
    assert(e->refs == 1);  // error if caller has an unreleased handle
    if (unref(e)) {
      freeentry(e);
    }
    e = next;
  }
}

bool lrucache::unref(lruhandle* e) {
  assert(e->refs > 0);
  e->refs--;
  return e->refs == 0;
}

void lrucache::freeentry(lruhandle* e) {
  assert(e->refs == 0);
  (*e->deleter)(e->key(), e->value);
  free(e);
}

void lrucache::applytoallcacheentries(void (*callback)(void*, size_t),
                                      bool thread_safe) {
  if (thread_safe) {
    mutex_.lock();
  }
  for (auto e = lru_.next; e != &lru_; e = e->next) {
    callback(e->value, e->charge);
  }
  if (thread_safe) {
    mutex_.unlock();
  }
}

void lrucache::lru_remove(lruhandle* e) {
  e->next->prev = e->prev;
  e->prev->next = e->next;
  usage_ -= e->charge;
}

void lrucache::lru_append(lruhandle* e) {
  // make "e" newest entry by inserting just before lru_
  e->next = &lru_;
  e->prev = lru_.prev;
  e->prev->next = e;
  e->next->prev = e;
  usage_ += e->charge;
}

cache::handle* lrucache::lookup(const slice& key, uint32_t hash) {
  mutexlock l(&mutex_);
  lruhandle* e = table_.lookup(key, hash);
  if (e != nullptr) {
    e->refs++;
    lru_remove(e);
    lru_append(e);
  }
  return reinterpret_cast<cache::handle*>(e);
}

void lrucache::release(cache::handle* handle) {
  lruhandle* e = reinterpret_cast<lruhandle*>(handle);
  bool last_reference = false;
  {
    mutexlock l(&mutex_);
    last_reference = unref(e);
  }
  if (last_reference) {
    freeentry(e);
  }
}

cache::handle* lrucache::insert(
    const slice& key, uint32_t hash, void* value, size_t charge,
    void (*deleter)(const slice& key, void* value)) {

  lruhandle* e = reinterpret_cast<lruhandle*>(
      malloc(sizeof(lruhandle)-1 + key.size()));
  autovector<lruhandle*> last_reference_list;

  e->value = value;
  e->deleter = deleter;
  e->charge = charge;
  e->key_length = key.size();
  e->hash = hash;
  e->refs = 2;  // one from lrucache, one for the returned handle
  memcpy(e->key_data, key.data(), key.size());

  {
    mutexlock l(&mutex_);

    lru_append(e);

    lruhandle* old = table_.insert(e);
    if (old != nullptr) {
      lru_remove(old);
      if (unref(old)) {
        last_reference_list.push_back(old);
      }
    }

    if (remove_scan_count_limit_ > 0) {
      // try to free the space by evicting the entries that are only
      // referenced by the cache first.
      lruhandle* cur = lru_.next;
      for (unsigned int scancount = 0;
           usage_ > capacity_ && cur != &lru_
           && scancount < remove_scan_count_limit_; scancount++) {
        lruhandle* next = cur->next;
        if (cur->refs <= 1) {
          lru_remove(cur);
          table_.remove(cur->key(), cur->hash);
          if (unref(cur)) {
            last_reference_list.push_back(cur);
          }
        }
        cur = next;
      }
    }

    // free the space following strict lru policy until enough space
    // is freed.
    while (usage_ > capacity_ && lru_.next != &lru_) {
      lruhandle* old = lru_.next;
      lru_remove(old);
      table_.remove(old->key(), old->hash);
      if (unref(old)) {
        last_reference_list.push_back(old);
      }
    }
  }

  // we free the entries here outside of mutex for
  // performance reasons
  for (auto entry : last_reference_list) {
    freeentry(entry);
  }

  return reinterpret_cast<cache::handle*>(e);
}

void lrucache::erase(const slice& key, uint32_t hash) {
  lruhandle* e;
  bool last_reference = false;
  {
    mutexlock l(&mutex_);
    e = table_.remove(key, hash);
    if (e != nullptr) {
      lru_remove(e);
      last_reference = unref(e);
    }
  }
  // mutex not held here
  // last_reference will only be true if e != nullptr
  if (last_reference) {
    freeentry(e);
  }
}

static int knumshardbits = 4;          // default values, can be overridden
static int kremovescancountlimit = 0; // default values, can be overridden

class shardedlrucache : public cache {
 private:
  lrucache* shards_;
  port::mutex id_mutex_;
  uint64_t last_id_;
  int num_shard_bits_;
  size_t capacity_;

  static inline uint32_t hashslice(const slice& s) {
    return hash(s.data(), s.size(), 0);
  }

  uint32_t shard(uint32_t hash) {
    // note, hash >> 32 yields hash in gcc, not the zero we expect!
    return (num_shard_bits_ > 0) ? (hash >> (32 - num_shard_bits_)) : 0;
  }

  void init(size_t capacity, int numbits, int removescancountlimit) {
    num_shard_bits_ = numbits;
    capacity_ = capacity;
    int num_shards = 1 << num_shard_bits_;
    shards_ = new lrucache[num_shards];
    const size_t per_shard = (capacity + (num_shards - 1)) / num_shards;
    for (int s = 0; s < num_shards; s++) {
      shards_[s].setcapacity(per_shard);
      shards_[s].setremovescancountlimit(removescancountlimit);
    }
  }

 public:
  explicit shardedlrucache(size_t capacity)
      : last_id_(0) {
    init(capacity, knumshardbits, kremovescancountlimit);
  }
  shardedlrucache(size_t capacity, int num_shard_bits,
                  int removescancountlimit)
     : last_id_(0) {
    init(capacity, num_shard_bits, removescancountlimit);
  }
  virtual ~shardedlrucache() {
    delete[] shards_;
  }
  virtual handle* insert(const slice& key, void* value, size_t charge,
                         void (*deleter)(const slice& key, void* value)) {
    const uint32_t hash = hashslice(key);
    return shards_[shard(hash)].insert(key, hash, value, charge, deleter);
  }
  virtual handle* lookup(const slice& key) {
    const uint32_t hash = hashslice(key);
    return shards_[shard(hash)].lookup(key, hash);
  }
  virtual void release(handle* handle) {
    lruhandle* h = reinterpret_cast<lruhandle*>(handle);
    shards_[shard(h->hash)].release(handle);
  }
  virtual void erase(const slice& key) {
    const uint32_t hash = hashslice(key);
    shards_[shard(hash)].erase(key, hash);
  }
  virtual void* value(handle* handle) {
    return reinterpret_cast<lruhandle*>(handle)->value;
  }
  virtual uint64_t newid() {
    mutexlock l(&id_mutex_);
    return ++(last_id_);
  }
  virtual size_t getcapacity() const {
    return capacity_;
  }

  virtual size_t getusage() const {
    // we will not lock the cache when getting the usage from shards.
    // for (size_t i = 0; i < num_shard_bits_; ++i)
    int num_shards = 1 << num_shard_bits_;
    size_t usage = 0;
    for (int s = 0; s < num_shards; s++) {
      usage += shards_[s].getusage();
    }
    return usage;
  }

  virtual void disowndata() {
    shards_ = nullptr;
  }

  virtual void applytoallcacheentries(void (*callback)(void*, size_t),
                                      bool thread_safe) override {
    int num_shards = 1 << num_shard_bits_;
    for (int s = 0; s < num_shards; s++) {
      shards_[s].applytoallcacheentries(callback, thread_safe);
    }
  }
};

}  // end anonymous namespace

shared_ptr<cache> newlrucache(size_t capacity) {
  return newlrucache(capacity, knumshardbits);
}

shared_ptr<cache> newlrucache(size_t capacity, int num_shard_bits) {
  return newlrucache(capacity, num_shard_bits, kremovescancountlimit);
}

shared_ptr<cache> newlrucache(size_t capacity, int num_shard_bits,
                              int removescancountlimit) {
  if (num_shard_bits >= 20) {
    return nullptr;  // the cache cannot be sharded into too many fine pieces
  }
  return std::make_shared<shardedlrucache>(capacity,
                                           num_shard_bits,
                                           removescancountlimit);
}

}  // namespace rocksdb
