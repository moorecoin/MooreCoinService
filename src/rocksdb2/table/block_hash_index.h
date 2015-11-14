// copyright (c) 2013, facebook, inc. all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
#pragma once

#include <string>
#include <unordered_map>

#include "rocksdb/status.h"
#include "util/arena.h"
#include "util/murmurhash.h"

namespace rocksdb {

class comparator;
class iterator;
class slice;
class slicetransform;

// build a hash-based index to speed up the lookup for "index block".
// blockhashindex accepts a key and, if found, returns its restart index within
// that index block.
class blockhashindex {
 public:
  // represents a restart index in the index block's restart array.
  struct restartindex {
    explicit restartindex(uint32_t first_index, uint32_t num_blocks = 1)
        : first_index(first_index), num_blocks(num_blocks) {}

    // for a given prefix, what is the restart index for the first data block
    // that contains it.
    uint32_t first_index = 0;

    // how many data blocks contains this prefix?
    uint32_t num_blocks = 1;
  };

  // @params own_prefixes indicate if we should take care the memory space for
  // the `key_prefix`
  // passed by add()
  explicit blockhashindex(const slicetransform* hash_key_extractor,
                          bool own_prefixes)
      : hash_key_extractor_(hash_key_extractor), kownprefixes(own_prefixes) {}

  // maps a key to its restart first_index.
  // returns nullptr if the restart first_index is found
  const restartindex* getrestartindex(const slice& key);

  bool add(const slice& key_prefix, uint32_t restart_index,
           uint32_t num_blocks);

  size_t approximatememoryusage() const {
    return arena_.approximatememoryusage();
  }

 private:
  const slicetransform* hash_key_extractor_;
  std::unordered_map<slice, restartindex, murmur_hash> restart_indices_;

  arena arena_;
  bool kownprefixes;
};

// create hash index by reading from the metadata blocks.
// @params prefixes: a sequence of prefixes.
// @params prefix_meta: contains the "metadata" to of the prefixes.
status createblockhashindex(const slicetransform* hash_key_extractor,
                            const slice& prefixes, const slice& prefix_meta,
                            blockhashindex** hash_index);

// create hash index by scanning the entries in index as well as the whole
// dataset.
// @params index_iter: an iterator with the pointer to the first entry in a
//                     block.
// @params data_iter: an iterator that can scan all the entries reside in a
//                     table.
// @params num_restarts: used for correctness verification.
// @params hash_key_extractor: extract the hashable part of a given key.
// on error, nullptr will be returned.
blockhashindex* createblockhashindexonthefly(
    iterator* index_iter, iterator* data_iter, const uint32_t num_restarts,
    const comparator* comparator, const slicetransform* hash_key_extractor);

}  // namespace rocksdb
