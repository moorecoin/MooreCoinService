// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// a cache is an interface that maps keys to values.  it has internal
// synchronization and may be safely accessed concurrently from
// multiple threads.  it may automatically evict entries to make room
// for new entries.  values have a specified charge against the cache
// capacity.  for example, a cache where the values are variable
// length strings, may use the length of the string as the charge for
// the string.
//
// a builtin cache implementation with a least-recently-used eviction
// policy is provided.  clients may use their own implementations if
// they want something more sophisticated (like scan-resistance, a
// custom eviction policy, variable cache sizing, etc.)

#ifndef storage_rocksdb_include_cache_h_
#define storage_rocksdb_include_cache_h_

#include <memory>
#include <stdint.h>
#include "rocksdb/slice.h"

namespace rocksdb {

using std::shared_ptr;

class cache;

// create a new cache with a fixed size capacity. the cache is sharded
// to 2^numshardbits shards, by hash of the key. the total capacity
// is divided and evenly assigned to each shard. inside each shard,
// the eviction is done in two passes: first try to free spaces by
// evicting entries that are among the most least used removescancountlimit
// entries and do not have reference other than by the cache itself, in
// the least-used order. if not enough space is freed, further free the
// entries in least used order.
//
// the functions without parameter numshardbits and/or removescancountlimit
// use default values. removescancountlimit's default value is 0, which
// means a strict lru order inside each shard.
extern shared_ptr<cache> newlrucache(size_t capacity);
extern shared_ptr<cache> newlrucache(size_t capacity, int numshardbits);
extern shared_ptr<cache> newlrucache(size_t capacity, int numshardbits,
                                     int removescancountlimit);

class cache {
 public:
  cache() { }

  // destroys all existing entries by calling the "deleter"
  // function that was passed to the constructor.
  virtual ~cache();

  // opaque handle to an entry stored in the cache.
  struct handle { };

  // insert a mapping from key->value into the cache and assign it
  // the specified charge against the total cache capacity.
  //
  // returns a handle that corresponds to the mapping.  the caller
  // must call this->release(handle) when the returned mapping is no
  // longer needed.
  //
  // when the inserted entry is no longer needed, the key and
  // value will be passed to "deleter".
  virtual handle* insert(const slice& key, void* value, size_t charge,
                         void (*deleter)(const slice& key, void* value)) = 0;

  // if the cache has no mapping for "key", returns nullptr.
  //
  // else return a handle that corresponds to the mapping.  the caller
  // must call this->release(handle) when the returned mapping is no
  // longer needed.
  virtual handle* lookup(const slice& key) = 0;

  // release a mapping returned by a previous lookup().
  // requires: handle must not have been released yet.
  // requires: handle must have been returned by a method on *this.
  virtual void release(handle* handle) = 0;

  // return the value encapsulated in a handle returned by a
  // successful lookup().
  // requires: handle must not have been released yet.
  // requires: handle must have been returned by a method on *this.
  virtual void* value(handle* handle) = 0;

  // if the cache contains entry for key, erase it.  note that the
  // underlying entry will be kept around until all existing handles
  // to it have been released.
  virtual void erase(const slice& key) = 0;

  // return a new numeric id.  may be used by multiple clients who are
  // sharing the same cache to partition the key space.  typically the
  // client will allocate a new id at startup and prepend the id to
  // its cache keys.
  virtual uint64_t newid() = 0;

  // returns the maximum configured capacity of the cache
  virtual size_t getcapacity() const = 0;

  // returns the memory size for the entries residing in the cache.
  virtual size_t getusage() const = 0;

  // call this on shutdown if you want to speed it up. cache will disown
  // any underlying data and will not free it on delete. this call will leak
  // memory - call this only if you're shutting down the process.
  // any attempts of using cache after this call will fail terribly.
  // always delete the db object before calling this method!
  virtual void disowndata() {
    // default implementation is noop
  };

  // apply callback to all entries in the cache
  // if thread_safe is true, it will also lock the accesses. otherwise, it will
  // access the cache without the lock held
  virtual void applytoallcacheentries(void (*callback)(void*, size_t),
                                      bool thread_safe) = 0;

 private:
  void lru_remove(handle* e);
  void lru_append(handle* e);
  void unref(handle* e);

  struct rep;
  rep* rep_;

  // no copying allowed
  cache(const cache&);
  void operator=(const cache&);
};

}  // namespace rocksdb

#endif  // storage_rocksdb_util_cache_h_
