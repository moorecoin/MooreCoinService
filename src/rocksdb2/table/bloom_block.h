//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
#pragma once

#include <vector>
#include <string>
#include "util/dynamic_bloom.h"

namespace rocksdb {
class logger;

class bloomblockbuilder {
 public:
  static const std::string kbloomblock;

  explicit bloomblockbuilder(uint32_t num_probes = 6)
      : bloom_(num_probes, nullptr) {}

  void settotalbits(arena* arena, uint32_t total_bits, uint32_t locality,
                    size_t huge_page_tlb_size, logger* logger) {
    bloom_.settotalbits(arena, total_bits, locality, huge_page_tlb_size,
                        logger);
  }

  uint32_t getnumblocks() const { return bloom_.getnumblocks(); }

  void addkeyshashes(const std::vector<uint32_t> keys_hashes);

  slice finish();

 private:
  dynamicbloom bloom_;
};

};  // namespace rocksdb
