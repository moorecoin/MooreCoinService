// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef rocksdb_lite
#pragma once
#include "rocksdb/env.h"
#include "rocksdb/status.h"
#include "port/port.h"
#include "util/mutexlock.h"
#include "util/coding.h"

#include <list>
#include <deque>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstdio>

namespace rocksdb {

struct blobchunk {
  uint32_t bucket_id;
  uint32_t offset; // in blocks
  uint32_t size; // in blocks
  blobchunk() {}
  blobchunk(uint32_t bucket_id, uint32_t offset, uint32_t size) :
    bucket_id(bucket_id), offset(offset), size(size) {}

  // returns true if it's immediately before chunk
  bool immediatelybefore(const blobchunk& chunk) const;
  // returns true if chunks overlap
  bool overlap(const blobchunk &chunk) const;
};

// we represent each blob as a string in format:
// bucket_id offset size|bucket_id offset size...
// the string can be used to reference the blob stored on external
// device/file
// not thread-safe!
struct blob {
  // generates the string
  std::string tostring() const;
  // parses the previously generated string
  explicit blob(const std::string& blob);
  // creates unfragmented blob
  blob(uint32_t bucket_id, uint32_t offset, uint32_t size) {
    setonechunk(bucket_id, offset, size);
  }
  blob() {}

  void setonechunk(uint32_t bucket_id, uint32_t offset, uint32_t size) {
    chunks.clear();
    chunks.push_back(blobchunk(bucket_id, offset, size));
  }

  uint32_t size() const { // in blocks
    uint32_t ret = 0;
    for (auto chunk : chunks) {
      ret += chunk.size;
    }
    assert(ret > 0);
    return ret;
  }

  // bucket_id, offset, size
  std::vector<blobchunk> chunks;
};

// keeps a list of free chunks
// not thread-safe. externally synchronized
class freelist {
 public:
  freelist() :
    free_blocks_(0) {}
  ~freelist() {}

  // allocates a a blob. stores the allocated blob in
  // 'blob'. returns non-ok status if it failed to allocate.
  // thread-safe
  status allocate(uint32_t blocks, blob* blob);
  // frees the blob for reuse. thread-safe
  status free(const blob& blob);

  // returns true if blob is overlapping with any of the
  // chunks stored in free list
  bool overlap(const blob &blob) const;

 private:
  std::deque<blobchunk> fifo_free_chunks_;
  uint32_t free_blocks_;
  mutable port::mutex mutex_;
};

// thread-safe
class blobstore {
 public:
   // directory - wherever the blobs should be stored. it will be created
   //   if missing
   // block_size - self explanatory
   // blocks_per_bucket - how many blocks we want to keep in one bucket.
   //   bucket is a device or a file that we use to store the blobs.
   //   if we don't have enough blocks to allocate a new blob, we will
   //   try to create a new file or device.
   // max_buckets - maximum number of buckets blobstore will create
   //   blobstore max size in bytes is
   //     max_buckets * blocks_per_bucket * block_size
   // env - env for creating new files
  blobstore(const std::string& directory,
            uint64_t block_size,
            uint32_t blocks_per_bucket,
            uint32_t max_buckets,
            env* env);
  ~blobstore();

  // allocates space for value.size bytes (rounded up to be multiple of
  // block size) and writes value.size bytes from value.data to a backing store.
  // sets blob blob that can than be used for addressing the
  // stored value. returns non-ok status on error.
  status put(const slice& value, blob* blob);
  // value needs to have enough space to store all the loaded stuff.
  // this function is thread safe!
  status get(const blob& blob, std::string* value) const;
  // frees the blob for reuse, but does not delete the data
  // on the backing store.
  status delete(const blob& blob);
  // sync all opened files that are modified
  status sync();

 private:
  const std::string directory_;
  // block_size_ is uint64_t because when we multiply with
  // blocks_size_ we want the result to be uint64_t or
  // we risk overflowing
  const uint64_t block_size_;
  const uint32_t blocks_per_bucket_;
  env* env_;
  envoptions storage_options_;
  // protected by free_list_mutex_
  freelist free_list_;
  // free_list_mutex_ is locked before buckets_mutex_
  mutable port::mutex free_list_mutex_;
  // protected by buckets_mutex_
  // array of buckets
  unique_ptr<randomrwfile>* buckets_;
  // number of buckets in the array
  uint32_t buckets_size_;
  uint32_t max_buckets_;
  mutable port::mutex buckets_mutex_;

  // calls freelist allocate. if free list can't allocate
  // new blob, creates new bucket and tries again
  // thread-safe
  status allocate(uint32_t blocks, blob* blob);

  // creates a new backing store and adds all the blocks
  // from the new backing store to the free list
  status createnewbucket();
};

} // namespace rocksdb
#endif  // rocksdb_lite
