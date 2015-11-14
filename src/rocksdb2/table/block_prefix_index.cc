// copyright (c) 2014, facebook, inc. all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#include "table/block_prefix_index.h"

#include <vector>

#include "rocksdb/comparator.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "util/arena.h"
#include "util/coding.h"
#include "util/hash.h"

namespace rocksdb {

inline uint32_t hash(const slice& s) {
  return rocksdb::hash(s.data(), s.size(), 0);
}

inline uint32_t prefixtobucket(const slice& prefix, uint32_t num_buckets) {
  return hash(prefix) % num_buckets;
}

// the prefix block index is simply a bucket array, with each entry pointing to
// the blocks that span the prefixes hashed to this bucket.
//
// to reduce memory footprint, if there is only one block per bucket, the entry
// stores the block id directly. if there are more than one blocks per bucket,
// because of hash collision or a single prefix spanning multiple blocks,
// the entry points to an array of block ids. the block array is an array of
// uint32_t's. the first uint32_t indicates the total number of blocks, followed
// by the block ids.
//
// to differentiate the two cases, the high order bit of the entry indicates
// whether it is a 'pointer' into a separate block array.
// 0x7fffffff is reserved for empty bucket.

const uint32_t knoneblock = 0x7fffffff;
const uint32_t kblockarraymask = 0x80000000;

inline bool isnone(uint32_t block_id) {
  return block_id == knoneblock;
}

inline bool isblockid(uint32_t block_id) {
  return (block_id & kblockarraymask) == 0;
}

inline uint32_t decodeindex(uint32_t block_id) {
  uint32_t index = block_id ^ kblockarraymask;
  assert(index < kblockarraymask);
  return index;
}

inline uint32_t encodeindex(uint32_t index) {
  assert(index < kblockarraymask);
  return index | kblockarraymask;
}

// temporary storage for prefix information during index building
struct prefixrecord {
  slice prefix;
  uint32_t start_block;
  uint32_t end_block;
  uint32_t num_blocks;
  prefixrecord* next;
};

class blockprefixindex::builder {
 public:
  explicit builder(const slicetransform* internal_prefix_extractor)
      : internal_prefix_extractor_(internal_prefix_extractor) {}

  void add(const slice& key_prefix, uint32_t start_block,
           uint32_t num_blocks) {
    prefixrecord* record = reinterpret_cast<prefixrecord*>(
      arena_.allocatealigned(sizeof(prefixrecord)));
    record->prefix = key_prefix;
    record->start_block = start_block;
    record->end_block = start_block + num_blocks - 1;
    record->num_blocks = num_blocks;
    prefixes_.push_back(record);
  }

  blockprefixindex* finish() {
    // for now, use roughly 1:1 prefix to bucket ratio.
    uint32_t num_buckets = prefixes_.size() + 1;

    // collect prefix records that hash to the same bucket, into a single
    // linklist.
    std::vector<prefixrecord*> prefixes_per_bucket(num_buckets, nullptr);
    std::vector<uint32_t> num_blocks_per_bucket(num_buckets, 0);
    for (prefixrecord* current : prefixes_) {
      uint32_t bucket = prefixtobucket(current->prefix, num_buckets);
      // merge the prefix block span if the first block of this prefix is
      // connected to the last block of the previous prefix.
      prefixrecord* prev = prefixes_per_bucket[bucket];
      if (prev) {
        assert(current->start_block >= prev->end_block);
        auto distance = current->start_block - prev->end_block;
        if (distance <= 1) {
          prev->end_block = current->end_block;
          prev->num_blocks = prev->end_block - prev->start_block + 1;
          num_blocks_per_bucket[bucket] += (current->num_blocks + distance - 1);
          continue;
        }
      }
      current->next = prev;
      prefixes_per_bucket[bucket] = current;
      num_blocks_per_bucket[bucket] += current->num_blocks;
    }

    // calculate the block array buffer size
    uint32_t total_block_array_entries = 0;
    for (uint32_t i = 0; i < num_buckets; i++) {
      uint32_t num_blocks = num_blocks_per_bucket[i];
      if (num_blocks > 1) {
        total_block_array_entries += (num_blocks + 1);
      }
    }

    // populate the final prefix block index
    uint32_t* block_array_buffer = new uint32_t[total_block_array_entries];
    uint32_t* buckets = new uint32_t[num_buckets];
    uint32_t offset = 0;
    for (uint32_t i = 0; i < num_buckets; i++) {
      uint32_t num_blocks = num_blocks_per_bucket[i];
      if (num_blocks == 0) {
        assert(prefixes_per_bucket[i] == nullptr);
        buckets[i] = knoneblock;
      } else if (num_blocks == 1) {
        assert(prefixes_per_bucket[i] != nullptr);
        assert(prefixes_per_bucket[i]->next == nullptr);
        buckets[i] = prefixes_per_bucket[i]->start_block;
      } else {
        assert(prefixes_per_bucket[i] != nullptr);
        buckets[i] = encodeindex(offset);
        block_array_buffer[offset] = num_blocks;
        uint32_t* last_block = &block_array_buffer[offset + num_blocks];
        auto current = prefixes_per_bucket[i];
        // populate block ids from largest to smallest
        while (current != nullptr) {
          for (uint32_t i = 0; i < current->num_blocks; i++) {
            *last_block = current->end_block - i;
            last_block--;
          }
          current = current->next;
        }
        assert(last_block == &block_array_buffer[offset]);
        offset += (num_blocks + 1);
      }
    }

    assert(offset == total_block_array_entries);

    return new blockprefixindex(internal_prefix_extractor_, num_buckets,
                                buckets, total_block_array_entries,
                                block_array_buffer);
  }

 private:
  const slicetransform* internal_prefix_extractor_;

  std::vector<prefixrecord*> prefixes_;
  arena arena_;
};


status blockprefixindex::create(const slicetransform* internal_prefix_extractor,
                                const slice& prefixes, const slice& prefix_meta,
                                blockprefixindex** prefix_index) {
  uint64_t pos = 0;
  auto meta_pos = prefix_meta;
  status s;
  builder builder(internal_prefix_extractor);

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
    if (pos + prefix_size > prefixes.size()) {
      s = status::corruption(
        "corrupted prefix meta block: size inconsistency.");
      break;
    }
    slice prefix(prefixes.data() + pos, prefix_size);
    builder.add(prefix, entry_index, num_blocks);

    pos += prefix_size;
  }

  if (s.ok() && pos != prefixes.size()) {
    s = status::corruption("corrupted prefix meta block");
  }

  if (s.ok()) {
    *prefix_index = builder.finish();
  }

  return s;
}

const uint32_t blockprefixindex::getblocks(const slice& key,
                                           uint32_t** blocks) {
  slice prefix = internal_prefix_extractor_->transform(key);

  uint32_t bucket = prefixtobucket(prefix, num_buckets_);
  uint32_t block_id = buckets_[bucket];

  if (isnone(block_id)) {
    return 0;
  } else if (isblockid(block_id)) {
    *blocks = &buckets_[bucket];
    return 1;
  } else {
    uint32_t index = decodeindex(block_id);
    assert(index < num_block_array_buffer_entries_);
    *blocks = &block_array_buffer_[index+1];
    uint32_t num_blocks = block_array_buffer_[index];
    assert(num_blocks > 1);
    assert(index + num_blocks < num_block_array_buffer_entries_);
    return num_blocks;
  }
}

}  // namespace rocksdb
