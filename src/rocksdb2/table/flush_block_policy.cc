//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include "rocksdb/options.h"
#include "rocksdb/flush_block_policy.h"
#include "rocksdb/slice.h"
#include "table/block_builder.h"

#include <cassert>

namespace rocksdb {

// flush block by size
class flushblockbysizepolicy : public flushblockpolicy {
 public:
  // @params block_size:           approximate size of user data packed per
  //                               block.
  // @params block_size_deviation: this is used to close a block before it
  //                               reaches the configured
  flushblockbysizepolicy(const uint64_t block_size,
                         const uint64_t block_size_deviation,
                         const blockbuilder& data_block_builder) :
      block_size_(block_size),
      block_size_deviation_(block_size_deviation),
      data_block_builder_(data_block_builder) {
  }

  virtual bool update(const slice& key,
                      const slice& value) override {
    // it makes no sense to flush when the data block is empty
    if (data_block_builder_.empty()) {
      return false;
    }

    auto curr_size = data_block_builder_.currentsizeestimate();

    // do flush if one of the below two conditions is true:
    // 1) if the current estimated size already exceeds the block size,
    // 2) block_size_deviation is set and the estimated size after appending
    // the kv will exceed the block size and the current size is under the
    // the deviation.
    return curr_size >= block_size_ || blockalmostfull(key, value);
  }

 private:
  bool blockalmostfull(const slice& key, const slice& value) const {
    const auto curr_size = data_block_builder_.currentsizeestimate();
    const auto estimated_size_after =
      data_block_builder_.estimatesizeafterkv(key, value);

    return
      estimated_size_after > block_size_ &&
      block_size_deviation_ > 0 &&
      curr_size * 100 > block_size_ * (100 - block_size_deviation_);
  }

  const uint64_t block_size_;
  const uint64_t block_size_deviation_;
  const blockbuilder& data_block_builder_;
};

flushblockpolicy* flushblockbysizepolicyfactory::newflushblockpolicy(
    const blockbasedtableoptions& table_options,
    const blockbuilder& data_block_builder) const {
  return new flushblockbysizepolicy(
      table_options.block_size, table_options.block_size_deviation,
      data_block_builder);
}

}  // namespace rocksdb
