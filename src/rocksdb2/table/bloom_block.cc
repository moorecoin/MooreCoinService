//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include "table/bloom_block.h"

#include <string>
#include "rocksdb/slice.h"
#include "util/dynamic_bloom.h"

namespace rocksdb {

void bloomblockbuilder::addkeyshashes(const std::vector<uint32_t> keys_hashes) {
  for (auto hash : keys_hashes) {
    bloom_.addhash(hash);
  }
}

slice bloomblockbuilder::finish() { return bloom_.getrawdata(); }

const std::string bloomblockbuilder::kbloomblock = "kbloomblock";
}  // namespace rocksdb
