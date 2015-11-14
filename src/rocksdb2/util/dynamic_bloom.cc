// copyright (c) 2013, facebook, inc. all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#include "dynamic_bloom.h"

#include <algorithm>

#include "port/port.h"
#include "rocksdb/slice.h"
#include "util/hash.h"

namespace rocksdb {

namespace {

uint32_t gettotalbitsforlocality(uint32_t total_bits) {
  uint32_t num_blocks =
      (total_bits + cache_line_size * 8 - 1) / (cache_line_size * 8);

  // make num_blocks an odd number to make sure more bits are involved
  // when determining which block.
  if (num_blocks % 2 == 0) {
    num_blocks++;
  }

  return num_blocks * (cache_line_size * 8);
}
}

dynamicbloom::dynamicbloom(arena* arena, uint32_t total_bits, uint32_t locality,
                           uint32_t num_probes,
                           uint32_t (*hash_func)(const slice& key),
                           size_t huge_page_tlb_size,
                           logger* logger)
    : dynamicbloom(num_probes, hash_func) {
  settotalbits(arena, total_bits, locality, huge_page_tlb_size, logger);
}

dynamicbloom::dynamicbloom(uint32_t num_probes,
                           uint32_t (*hash_func)(const slice& key))
    : ktotalbits(0),
      knumblocks(0),
      knumprobes(num_probes),
      hash_func_(hash_func == nullptr ? &bloomhash : hash_func) {}

void dynamicbloom::setrawdata(unsigned char* raw_data, uint32_t total_bits,
                              uint32_t num_blocks) {
  data_ = raw_data;
  ktotalbits = total_bits;
  knumblocks = num_blocks;
}

void dynamicbloom::settotalbits(arena* arena,
                                uint32_t total_bits, uint32_t locality,
                                size_t huge_page_tlb_size,
                                logger* logger) {
  ktotalbits = (locality > 0) ? gettotalbitsforlocality(total_bits)
                              : (total_bits + 7) / 8 * 8;
  knumblocks = (locality > 0) ? (ktotalbits / (cache_line_size * 8)) : 0;

  assert(knumblocks > 0 || ktotalbits > 0);
  assert(knumprobes > 0);

  uint32_t sz = ktotalbits / 8;
  if (knumblocks > 0) {
    sz += cache_line_size - 1;
  }
  assert(arena);
  raw_ = reinterpret_cast<unsigned char*>(
      arena->allocatealigned(sz, huge_page_tlb_size, logger));
  memset(raw_, 0, sz);
  if (knumblocks > 0 && (reinterpret_cast<uint64_t>(raw_) % cache_line_size)) {
    data_ = raw_ + cache_line_size -
            reinterpret_cast<uint64_t>(raw_) % cache_line_size;
  } else {
    data_ = raw_;
  }
}

}  // rocksdb
