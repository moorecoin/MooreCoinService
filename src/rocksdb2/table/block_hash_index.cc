// copyright (c) 2013, facebook, inc. all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#include "table/block_hash_index.h"

#include <algorithm>

#include "rocksdb/comparator.h"
#include "rocksdb/iterator.h"
#include "rocksdb/slice_transform.h"
#include "util/coding.h"

namespace rocksdb {

status createblockhashindex(const slicetransform* hash_key_extractor,
                            const slice& prefixes, const slice& prefix_meta,
                            blockhashindex** hash_index) {
  uint64_t pos = 0;
  auto meta_pos = prefix_meta;
  status s;
  *hash_index = new blockhashindex(
      hash_key_extractor,
      false /* external module manages memory space for prefixes */);

  while (!meta_pos.empty()) {
    uint32_t prefix_size = 0;
    uint32_t entry_index = 0;
    uint32_t num_blocks = 0;
    if (!getvarint32(&meta_pos, &prefix_size) ||
        !getvarint32(&meta_pos, &entry_index) ||
        !getvarint32(&meta_pos, &num_blocks)) {
      s = status::corruption(
          "corrupted prefix meta block: unable to read from it.");
      break;
    }
    slice prefix(prefixes.data() + pos, prefix_size);
    (*hash_index)->add(prefix, entry_index, num_blocks);

    pos += prefix_size;
  }

  if (s.ok() && pos != prefixes.size()) {
    s = status::corruption("corrupted prefix meta block");
  }

  if (!s.ok()) {
    delete *hash_index;
  }

  return s;
}

blockhashindex* createblockhashindexonthefly(
    iterator* index_iter, iterator* data_iter, const uint32_t num_restarts,
    const comparator* comparator, const slicetransform* hash_key_extractor) {
  assert(hash_key_extractor);
  auto hash_index = new blockhashindex(
      hash_key_extractor,
      true /* hash_index will copy prefix when add() is called */);
  uint64_t current_restart_index = 0;

  std::string pending_entry_prefix;
  // pending_block_num == 0 also implies there is no entry inserted at all.
  uint32_t pending_block_num = 0;
  uint32_t pending_entry_index = 0;

  // scan all the entries and create a hash index based on their prefixes.
  data_iter->seektofirst();
  for (index_iter->seektofirst();
       index_iter->valid() && current_restart_index < num_restarts;
       index_iter->next()) {
    slice last_key_in_block = index_iter->key();
    assert(data_iter->valid() && data_iter->status().ok());

    // scan through all entries within a data block.
    while (data_iter->valid() &&
           comparator->compare(data_iter->key(), last_key_in_block) <= 0) {
      auto key_prefix = hash_key_extractor->transform(data_iter->key());
      bool is_first_entry = pending_block_num == 0;

      // keys may share the prefix
      if (is_first_entry || pending_entry_prefix != key_prefix) {
        if (!is_first_entry) {
          bool succeeded = hash_index->add(
              pending_entry_prefix, pending_entry_index, pending_block_num);
          if (!succeeded) {
            delete hash_index;
            return nullptr;
          }
        }

        // update the status.
        // needs a hard copy otherwise the underlying data changes all the time.
        pending_entry_prefix = key_prefix.tostring();
        pending_block_num = 1;
        pending_entry_index = current_restart_index;
      } else {
        // entry number increments when keys share the prefix reside in
        // differnt data blocks.
        auto last_restart_index = pending_entry_index + pending_block_num - 1;
        assert(last_restart_index <= current_restart_index);
        if (last_restart_index != current_restart_index) {
          ++pending_block_num;
        }
      }
      data_iter->next();
    }

    ++current_restart_index;
  }

  // make sure all entries has been scaned.
  assert(!index_iter->valid());
  assert(!data_iter->valid());

  if (pending_block_num > 0) {
    auto succeeded = hash_index->add(pending_entry_prefix, pending_entry_index,
                                     pending_block_num);
    if (!succeeded) {
      delete hash_index;
      return nullptr;
    }
  }

  return hash_index;
}

bool blockhashindex::add(const slice& prefix, uint32_t restart_index,
                         uint32_t num_blocks) {
  auto prefix_to_insert = prefix;
  if (kownprefixes) {
    auto prefix_ptr = arena_.allocate(prefix.size());
    std::copy(prefix.data() /* begin */,
              prefix.data() + prefix.size() /* end */,
              prefix_ptr /* destination */);
    prefix_to_insert = slice(prefix_ptr, prefix.size());
  }
  auto result = restart_indices_.insert(
      {prefix_to_insert, restartindex(restart_index, num_blocks)});
  return result.second;
}

const blockhashindex::restartindex* blockhashindex::getrestartindex(
    const slice& key) {
  auto key_prefix = hash_key_extractor_->transform(key);

  auto pos = restart_indices_.find(key_prefix);
  if (pos == restart_indices_.end()) {
    return nullptr;
  }

  return &pos->second;
}

}  // namespace rocksdb
