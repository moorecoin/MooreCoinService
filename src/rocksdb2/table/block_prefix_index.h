// copyright (c) 2013, facebook, inc. all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
#pragma once

#include "rocksdb/status.h"

namespace rocksdb {

class comparator;
class iterator;
class slice;
class slicetransform;

// build a hash-based index to speed up the lookup for "index block".
// blockhashindex accepts a key and, if found, returns its restart index within
// that index block.
class blockprefixindex {
 public:

  // maps a key to a list of data blocks that could potentially contain
  // the key, based on the prefix.
  // returns the total number of relevant blocks, 0 means the key does
  // not exist.
  const uint32_t getblocks(const slice& key, uint32_t** blocks);

  size_t approximatememoryusage() const {
    return sizeof(blockprefixindex) +
      (num_block_array_buffer_entries_ + num_buckets_) * sizeof(uint32_t);
  }

  // create hash index by reading from the metadata blocks.
  // @params prefixes: a sequence of prefixes.
  // @params prefix_meta: contains the "metadata" to of the prefixes.
  static status create(const slicetransform* hash_key_extractor,
                       const slice& prefixes, const slice& prefix_meta,
                       blockprefixindex** prefix_index);

  ~blockprefixindex() {
    delete[] buckets_;
    delete[] block_array_buffer_;
  }

 private:
  class builder;
  friend builder;

  blockprefixindex(const slicetransform* internal_prefix_extractor,
                   uint32_t num_buckets,
                   uint32_t* buckets,
                   uint32_t num_block_array_buffer_entries,
                   uint32_t* block_array_buffer)
      : internal_prefix_extractor_(internal_prefix_extractor),
        num_buckets_(num_buckets),
        num_block_array_buffer_entries_(num_block_array_buffer_entries),
        buckets_(buckets),
        block_array_buffer_(block_array_buffer) {}

  const slicetransform* internal_prefix_extractor_;
  uint32_t num_buckets_;
  uint32_t num_block_array_buffer_entries_;
  uint32_t* buckets_;
  uint32_t* block_array_buffer_;
};

}  // namespace rocksdb
