//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/arena.h"
#include <sys/mman.h>
#include <algorithm>
#include "rocksdb/env.h"

namespace rocksdb {

const size_t arena::kinlinesize;
const size_t arena::kminblocksize = 4096;
const size_t arena::kmaxblocksize = 2 << 30;
static const int kalignunit = sizeof(void*);

size_t optimizeblocksize(size_t block_size) {
  // make sure block_size is in optimal range
  block_size = std::max(arena::kminblocksize, block_size);
  block_size = std::min(arena::kmaxblocksize, block_size);

  // make sure block_size is the multiple of kalignunit
  if (block_size % kalignunit != 0) {
    block_size = (1 + block_size / kalignunit) * kalignunit;
  }

  return block_size;
}

arena::arena(size_t block_size) : kblocksize(optimizeblocksize(block_size)) {
  assert(kblocksize >= kminblocksize && kblocksize <= kmaxblocksize &&
         kblocksize % kalignunit == 0);
  alloc_bytes_remaining_ = sizeof(inline_block_);
  blocks_memory_ += alloc_bytes_remaining_;
  aligned_alloc_ptr_ = inline_block_;
  unaligned_alloc_ptr_ = inline_block_ + alloc_bytes_remaining_;
}

arena::~arena() {
  for (const auto& block : blocks_) {
    delete[] block;
  }
  for (const auto& mmap_info : huge_blocks_) {
    auto ret = munmap(mmap_info.addr_, mmap_info.length_);
    if (ret != 0) {
      // todo(sdong): better handling
    }
  }
}

char* arena::allocatefallback(size_t bytes, bool aligned) {
  if (bytes > kblocksize / 4) {
    ++irregular_block_num;
    // object is more than a quarter of our block size.  allocate it separately
    // to avoid wasting too much space in leftover bytes.
    return allocatenewblock(bytes);
  }

  // we waste the remaining space in the current block.
  auto block_head = allocatenewblock(kblocksize);
  alloc_bytes_remaining_ = kblocksize - bytes;

  if (aligned) {
    aligned_alloc_ptr_ = block_head + bytes;
    unaligned_alloc_ptr_ = block_head + kblocksize;
    return block_head;
  } else {
    aligned_alloc_ptr_ = block_head;
    unaligned_alloc_ptr_ = block_head + kblocksize - bytes;
    return unaligned_alloc_ptr_;
  }
}

char* arena::allocatealigned(size_t bytes, size_t huge_page_size,
                             logger* logger) {
  assert((kalignunit & (kalignunit - 1)) ==
         0);  // pointer size should be a power of 2

#ifdef map_hugetlb
  if (huge_page_size > 0 && bytes > 0) {
    // allocate from a huge page tbl table.
    assert(logger != nullptr);  // logger need to be passed in.
    size_t reserved_size =
        ((bytes - 1u) / huge_page_size + 1u) * huge_page_size;
    assert(reserved_size >= bytes);
    void* addr = mmap(nullptr, reserved_size, (prot_read | prot_write),
                      (map_private | map_anonymous | map_hugetlb), 0, 0);

    if (addr == map_failed) {
      warn(logger, "allocatealigned fail to allocate huge tlb pages: %s",
           strerror(errno));
      // fail back to malloc
    } else {
      blocks_memory_ += reserved_size;
      huge_blocks_.push_back(mmapinfo(addr, reserved_size));
      return reinterpret_cast<char*>(addr);
    }
  }
#endif

  size_t current_mod =
      reinterpret_cast<uintptr_t>(aligned_alloc_ptr_) & (kalignunit - 1);
  size_t slop = (current_mod == 0 ? 0 : kalignunit - current_mod);
  size_t needed = bytes + slop;
  char* result;
  if (needed <= alloc_bytes_remaining_) {
    result = aligned_alloc_ptr_ + slop;
    aligned_alloc_ptr_ += needed;
    alloc_bytes_remaining_ -= needed;
  } else {
    // allocatefallback always returned aligned memory
    result = allocatefallback(bytes, true /* aligned */);
  }
  assert((reinterpret_cast<uintptr_t>(result) & (kalignunit - 1)) == 0);
  return result;
}

char* arena::allocatenewblock(size_t block_bytes) {
  char* block = new char[block_bytes];
  blocks_memory_ += block_bytes;
  blocks_.push_back(block);
  return block;
}

}  // namespace rocksdb
