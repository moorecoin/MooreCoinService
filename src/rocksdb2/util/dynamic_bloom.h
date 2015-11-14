// copyright (c) 2013, facebook, inc. all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#pragma once

#include <string>

#include "rocksdb/slice.h"

#include <util/arena.h>
#include <port/port_posix.h>

#include <atomic>
#include <memory>

namespace rocksdb {

class slice;
class logger;

class dynamicbloom {
 public:
  // arena: pass arena to bloom filter, hence trace the usage of memory
  // total_bits: fixed total bits for the bloom
  // num_probes: number of hash probes for a single key
  // locality:  if positive, optimize for cache line locality, 0 otherwise.
  // hash_func:  customized hash function
  // huge_page_tlb_size:  if >0, try to allocate bloom bytes from huge page tlb
  //                      withi this page size. need to reserve huge pages for
  //                      it to be allocated, like:
  //                         sysctl -w vm.nr_hugepages=20
  //                     see linux doc documentation/vm/hugetlbpage.txt
  explicit dynamicbloom(arena* arena,
                        uint32_t total_bits, uint32_t locality = 0,
                        uint32_t num_probes = 6,
                        uint32_t (*hash_func)(const slice& key) = nullptr,
                        size_t huge_page_tlb_size = 0,
                        logger* logger = nullptr);

  explicit dynamicbloom(uint32_t num_probes = 6,
                        uint32_t (*hash_func)(const slice& key) = nullptr);

  void settotalbits(arena* arena, uint32_t total_bits, uint32_t locality,
                    size_t huge_page_tlb_size, logger* logger);

  ~dynamicbloom() {}

  // assuming single threaded access to this function.
  void add(const slice& key);

  // assuming single threaded access to this function.
  void addhash(uint32_t hash);

  // multithreaded access to this function is ok
  bool maycontain(const slice& key) const;

  // multithreaded access to this function is ok
  bool maycontainhash(uint32_t hash) const;

  void prefetch(uint32_t h);

  uint32_t getnumblocks() const { return knumblocks; }

  slice getrawdata() const {
    return slice(reinterpret_cast<char*>(data_), gettotalbits() / 8);
  }

  void setrawdata(unsigned char* raw_data, uint32_t total_bits,
                  uint32_t num_blocks = 0);

  uint32_t gettotalbits() const { return ktotalbits; }

  bool isinitialized() const { return knumblocks > 0 || ktotalbits > 0; }

 private:
  uint32_t ktotalbits;
  uint32_t knumblocks;
  const uint32_t knumprobes;

  uint32_t (*hash_func_)(const slice& key);
  unsigned char* data_;
  unsigned char* raw_;
};

inline void dynamicbloom::add(const slice& key) { addhash(hash_func_(key)); }

inline bool dynamicbloom::maycontain(const slice& key) const {
  return (maycontainhash(hash_func_(key)));
}

inline void dynamicbloom::prefetch(uint32_t h) {
  if (knumblocks != 0) {
    uint32_t b = ((h >> 11 | (h << 21)) % knumblocks) * (cache_line_size * 8);
    prefetch(&(data_[b]), 0, 3);
  }
}

inline bool dynamicbloom::maycontainhash(uint32_t h) const {
  assert(isinitialized());
  const uint32_t delta = (h >> 17) | (h << 15);  // rotate right 17 bits
  if (knumblocks != 0) {
    uint32_t b = ((h >> 11 | (h << 21)) % knumblocks) * (cache_line_size * 8);
    for (uint32_t i = 0; i < knumprobes; ++i) {
      // since cache_line_size is defined as 2^n, this line will be optimized
      //  to a simple and operation by compiler.
      const uint32_t bitpos = b + (h % (cache_line_size * 8));
      if (((data_[bitpos / 8]) & (1 << (bitpos % 8))) == 0) {
        return false;
      }
      // rotate h so that we don't reuse the same bytes.
      h = h / (cache_line_size * 8) +
          (h % (cache_line_size * 8)) * (0x20000000u / cache_line_size);
      h += delta;
    }
  } else {
    for (uint32_t i = 0; i < knumprobes; ++i) {
      const uint32_t bitpos = h % ktotalbits;
      if (((data_[bitpos / 8]) & (1 << (bitpos % 8))) == 0) {
        return false;
      }
      h += delta;
    }
  }
  return true;
}

inline void dynamicbloom::addhash(uint32_t h) {
  assert(isinitialized());
  const uint32_t delta = (h >> 17) | (h << 15);  // rotate right 17 bits
  if (knumblocks != 0) {
    uint32_t b = ((h >> 11 | (h << 21)) % knumblocks) * (cache_line_size * 8);
    for (uint32_t i = 0; i < knumprobes; ++i) {
      // since cache_line_size is defined as 2^n, this line will be optimized
      // to a simple and operation by compiler.
      const uint32_t bitpos = b + (h % (cache_line_size * 8));
      data_[bitpos / 8] |= (1 << (bitpos % 8));
      // rotate h so that we don't reuse the same bytes.
      h = h / (cache_line_size * 8) +
          (h % (cache_line_size * 8)) * (0x20000000u / cache_line_size);
      h += delta;
    }
  } else {
    for (uint32_t i = 0; i < knumprobes; ++i) {
      const uint32_t bitpos = h % ktotalbits;
      data_[bitpos / 8] |= (1 << (bitpos % 8));
      h += delta;
    }
  }
}

}  // rocksdb
