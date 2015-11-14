// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
//
// this file contains the interface that must be implemented by any collection
// to be used as the backing store for a memtable. such a collection must
// satisfy the following properties:
//  (1) it does not store duplicate items.
//  (2) it uses memtablerep::keycomparator to compare items for iteration and
//     equality.
//  (3) it can be accessed concurrently by multiple readers and can support
//     during reads. however, it needn't support multiple concurrent writes.
//  (4) items are never deleted.
// the liberal use of assertions is encouraged to enforce (1).
//
// the factory will be passed an arena object when a new memtablerep is
// requested. the api for this object is in rocksdb/arena.h.
//
// users can implement their own memtable representations. we include three
// types built in:
//  - skiplistrep: this is the default; it is backed by a skip list.
//  - hashskiplistrep: the memtable rep that is best used for keys that are
//  structured like "prefix:suffix" where iteration within a prefix is
//  common and iteration across different prefixes is rare. it is backed by
//  a hash map where each bucket is a skip list.
//  - vectorrep: this is backed by an unordered std::vector. on iteration, the
// vector is sorted. it is intelligent about sorting; once the markreadonly()
// has been called, the vector will only be sorted once. it is optimized for
// random-write-heavy workloads.
//
// the last four implementations are designed for situations in which
// iteration over the entire collection is rare since doing so requires all the
// keys to be copied into a sorted data structure.

#pragma once

#include <memory>
#include <stdint.h>

namespace rocksdb {

class arena;
class lookupkey;
class slice;
class slicetransform;
class logger;

typedef void* keyhandle;

class memtablerep {
 public:
  // keycomparator provides a means to compare keys, which are internal keys
  // concatenated with values.
  class keycomparator {
   public:
    // compare a and b. return a negative value if a is less than b, 0 if they
    // are equal, and a positive value if a is greater than b
    virtual int operator()(const char* prefix_len_key1,
                           const char* prefix_len_key2) const = 0;

    virtual int operator()(const char* prefix_len_key,
                           const slice& key) const = 0;

    virtual ~keycomparator() { }
  };

  explicit memtablerep(arena* arena) : arena_(arena) {}

  // allocate a buf of len size for storing key. the idea is that a specific
  // memtable representation knows its underlying data structure better. by
  // allowing it to allocate memory, it can possibly put correlated stuff
  // in consecutive memory area to make processor prefetching more efficient.
  virtual keyhandle allocate(const size_t len, char** buf);

  // insert key into the collection. (the caller will pack key and value into a
  // single buffer and pass that in as the parameter to insert).
  // requires: nothing that compares equal to key is currently in the
  // collection.
  virtual void insert(keyhandle handle) = 0;

  // returns true iff an entry that compares equal to key is in the collection.
  virtual bool contains(const char* key) const = 0;

  // notify this table rep that it will no longer be added to. by default, does
  // nothing.
  virtual void markreadonly() { }

  // look up key from the mem table, since the first key in the mem table whose
  // user_key matches the one given k, call the function callback_func(), with
  // callback_args directly forwarded as the first parameter, and the mem table
  // key as the second parameter. if the return value is false, then terminates.
  // otherwise, go through the next key.
  // it's safe for get() to terminate after having finished all the potential
  // key for the k.user_key(), or not.
  //
  // default:
  // get() function with a default value of dynamically construct an iterator,
  // seek and call the call back function.
  virtual void get(const lookupkey& k, void* callback_args,
                   bool (*callback_func)(void* arg, const char* entry));

  // report an approximation of how much memory has been used other than memory
  // that was allocated through the arena.
  virtual size_t approximatememoryusage() = 0;

  virtual ~memtablerep() { }

  // iteration over the contents of a skip collection
  class iterator {
   public:
    // initialize an iterator over the specified collection.
    // the returned iterator is not valid.
    // explicit iterator(const memtablerep* collection);
    virtual ~iterator() {}

    // returns true iff the iterator is positioned at a valid node.
    virtual bool valid() const = 0;

    // returns the key at the current position.
    // requires: valid()
    virtual const char* key() const = 0;

    // advances to the next position.
    // requires: valid()
    virtual void next() = 0;

    // advances to the previous position.
    // requires: valid()
    virtual void prev() = 0;

    // advance to the first entry with a key >= target
    virtual void seek(const slice& internal_key, const char* memtable_key) = 0;

    // position at the first entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektofirst() = 0;

    // position at the last entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektolast() = 0;
  };

  // return an iterator over the keys in this representation.
  // arena: if not null, the arena needs to be used to allocate the iterator.
  //        when destroying the iterator, the caller will not call "delete"
  //        but iterator::~iterator() directly. the destructor needs to destroy
  //        all the states but those allocated in arena.
  virtual iterator* getiterator(arena* arena = nullptr) = 0;

  // return an iterator that has a special seek semantics. the result of
  // a seek might only include keys with the same prefix as the target key.
  // arena: if not null, the arena needs to be used to allocate the iterator.
  //        when destroying the iterator, the caller will not call "delete"
  //        but iterator::~iterator() directly. the destructor needs to destroy
  //        all the states but those allocated in arena.
  virtual iterator* getdynamicprefixiterator(arena* arena = nullptr) {
    return getiterator(arena);
  }

  // return true if the current memtablerep supports merge operator.
  // default: true
  virtual bool ismergeoperatorsupported() const { return true; }

  // return true if the current memtablerep supports snapshot
  // default: true
  virtual bool issnapshotsupported() const { return true; }

 protected:
  // when *key is an internal key concatenated with the value, returns the
  // user key.
  virtual slice userkey(const char* key) const;

  arena* arena_;
};

// this is the base class for all factories that are used by rocksdb to create
// new memtablerep objects
class memtablerepfactory {
 public:
  virtual ~memtablerepfactory() {}
  virtual memtablerep* creatememtablerep(const memtablerep::keycomparator&,
                                         arena*, const slicetransform*,
                                         logger* logger) = 0;
  virtual const char* name() const = 0;
};

// this uses a skip list to store keys. it is the default.
class skiplistfactory : public memtablerepfactory {
 public:
  virtual memtablerep* creatememtablerep(const memtablerep::keycomparator&,
                                         arena*, const slicetransform*,
                                         logger* logger) override;
  virtual const char* name() const override { return "skiplistfactory"; }
};

#ifndef rocksdb_lite
// this creates memtablereps that are backed by an std::vector. on iteration,
// the vector is sorted. this is useful for workloads where iteration is very
// rare and writes are generally not issued after reads begin.
//
// parameters:
//   count: passed to the constructor of the underlying std::vector of each
//     vectorrep. on initialization, the underlying array will be at least count
//     bytes reserved for usage.
class vectorrepfactory : public memtablerepfactory {
  const size_t count_;

 public:
  explicit vectorrepfactory(size_t count = 0) : count_(count) { }
  virtual memtablerep* creatememtablerep(const memtablerep::keycomparator&,
                                         arena*, const slicetransform*,
                                         logger* logger) override;
  virtual const char* name() const override {
    return "vectorrepfactory";
  }
};

// this class contains a fixed array of buckets, each
// pointing to a skiplist (null if the bucket is empty).
// bucket_count: number of fixed array buckets
// skiplist_height: the max height of the skiplist
// skiplist_branching_factor: probabilistic size ratio between adjacent
//                            link lists in the skiplist
extern memtablerepfactory* newhashskiplistrepfactory(
    size_t bucket_count = 1000000, int32_t skiplist_height = 4,
    int32_t skiplist_branching_factor = 4
);

// the factory is to create memtables based on a hash table:
// it contains a fixed array of buckets, each pointing to either a linked list
// or a skip list if number of entries inside the bucket exceeds
// threshold_use_skiplist.
// @bucket_count: number of fixed array buckets
// @huge_page_tlb_size: if <=0, allocate the hash table bytes from malloc.
//                      otherwise from huge page tlb. the user needs to reserve
//                      huge pages for it to be allocated, like:
//                          sysctl -w vm.nr_hugepages=20
//                      see linux doc documentation/vm/hugetlbpage.txt
// @bucket_entries_logging_threshold: if number of entries in one bucket
//                                    exceeds this number, log about it.
// @if_log_bucket_dist_when_flash: if true, log distribution of number of
//                                 entries when flushing.
// @threshold_use_skiplist: a bucket switches to skip list if number of
//                          entries exceed this parameter.
extern memtablerepfactory* newhashlinklistrepfactory(
    size_t bucket_count = 50000, size_t huge_page_tlb_size = 0,
    int bucket_entries_logging_threshold = 4096,
    bool if_log_bucket_dist_when_flash = true,
    uint32_t threshold_use_skiplist = 256);

// this factory creates a cuckoo-hashing based mem-table representation.
// cuckoo-hash is a closed-hash strategy, in which all key/value pairs
// are stored in the bucket array itself intead of in some data structures
// external to the bucket array.  in addition, each key in cuckoo hash
// has a constant number of possible buckets in the bucket array.  these
// two properties together makes cuckoo hash more memory efficient and
// a constant worst-case read time.  cuckoo hash is best suitable for
// point-lookup workload.
//
// when inserting a key / value, it first checks whether one of its possible
// buckets is empty.  if so, the key / value will be inserted to that vacant
// bucket.  otherwise, one of the keys originally stored in one of these
// possible buckets will be "kicked out" and move to one of its possible
// buckets (and possibly kicks out another victim.)  in the current
// implementation, such "kick-out" path is bounded.  if it cannot find a
// "kick-out" path for a specific key, this key will be stored in a backup
// structure, and the current memtable to be forced to immutable.
//
// note that currently this mem-table representation does not support
// snapshot (i.e., it only queries latest state) and iterators.  in addition,
// multiget operation might also lose its atomicity due to the lack of
// snapshot support.
//
// parameters:
//   write_buffer_size: the write buffer size in bytes.
//   average_data_size: the average size of key + value in bytes.  this value
//     together with write_buffer_size will be used to compute the number
//     of buckets.
//   hash_function_count: the number of hash functions that will be used by
//     the cuckoo-hash.  the number also equals to the number of possible
//     buckets each key will have.
extern memtablerepfactory* newhashcuckoorepfactory(
    size_t write_buffer_size, size_t average_data_size = 64,
    unsigned int hash_function_count = 4);
#endif  // rocksdb_lite
}  // namespace rocksdb
