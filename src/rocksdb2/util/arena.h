//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

// arena is an implementation of arena class. for a request of small size,
// it allocates a block with pre-defined block size. for a request of big
// size, it uses malloc to directly get the requested size.

#pragma once
#include <cstddef>
#include <cerrno>
#include <vector>
#include <assert.h>
#include <stdint.h>
#include "util/arena.h"

namespace rocksdb {

class logger;

const size_t kinlinesize = 2048;

class arena {
 public:
  // no copying allowed
  arena(const arena&) = delete;
  void operator=(const arena&) = delete;

  static const size_t kinlinesize = 2048;
  static const size_t kminblocksize;
  static const size_t kmaxblocksize;

  explicit arena(size_t block_size = kminblocksize);
  ~arena();

  char* allocate(size_t bytes);

  // huge_page_size: if >0, will try to allocate from huage page tlb.
  // the argument will be the size of the page size for huge page tlb. bytes
  // will be rounded up to multiple of the page size to allocate through mmap
  // anonymous option with huge page on. the extra  space allocated will be
  // wasted. if allocation fails, will fall back to normal case. to enable it,
  // need to reserve huge pages for it to be allocated, like:
  //     sysctl -w vm.nr_hugepages=20
  // see linux doc documentation/vm/hugetlbpage.txt for details.
  // huge page allocation can fail. in this case it will fail back to
  // normal cases. the messages will be logged to logger. so when calling with
  // huge_page_tlb_size > 0, we highly recommend a logger is passed in.
  // otherwise, the error message will be printed out to stderr directly.
  char* allocatealigned(size_t bytes, size_t huge_page_size = 0,
                        logger* logger = nullptr);

  // returns an estimate of the total memory usage of data allocated
  // by the arena (exclude the space allocated but not yet used for future
  // allocations).
  size_t approximatememoryusage() const {
    return blocks_memory_ + blocks_.capacity() * sizeof(char*) -
           alloc_bytes_remaining_;
  }

  size_t memoryallocatedbytes() const { return blocks_memory_; }

  size_t allocatedandunused() const { return alloc_bytes_remaining_; }

  // if an allocation is too big, we'll allocate an irregular block with the
  // same size of that allocation.
  virtual size_t irregularblocknum() const { return irregular_block_num; }

  size_t blocksize() const { return kblocksize; }

 private:
  char inline_block_[kinlinesize];
  // number of bytes allocated in one block
  const size_t kblocksize;
  // array of new[] allocated memory blocks
  typedef std::vector<char*> blocks;
  blocks blocks_;

  struct mmapinfo {
    void* addr_;
    size_t length_;

    mmapinfo(void* addr, size_t length) : addr_(addr), length_(length) {}
  };
  std::vector<mmapinfo> huge_blocks_;
  size_t irregular_block_num = 0;

  // stats for current active block.
  // for each block, we allocate aligned memory chucks from one end and
  // allocate unaligned memory chucks from the other end. otherwise the
  // memory waste for alignment will be higher if we allocate both types of
  // memory from one direction.
  char* unaligned_alloc_ptr_ = nullptr;
  char* aligned_alloc_ptr_ = nullptr;
  // how many bytes left in currently active block?
  size_t alloc_bytes_remaining_ = 0;

  char* allocatefallback(size_t bytes, bool aligned);
  char* allocatenewblock(size_t block_bytes);

  // bytes of memory in blocks allocated so far
  size_t blocks_memory_ = 0;
};

inline char* arena::allocate(size_t bytes) {
  // the semantics of what to return are a bit messy if we allow
  // 0-byte allocations, so we disallow them here (we don't need
  // them for our internal use).
  assert(bytes > 0);
  if (bytes <= alloc_bytes_remaining_) {
    unaligned_alloc_ptr_ -= bytes;
    alloc_bytes_remaining_ -= bytes;
    return unaligned_alloc_ptr_;
  }
  return allocatefallback(bytes, false /* unaligned */);
}

// check and adjust the block_size so that the return value is
//  1. in the range of [kminblocksize, kmaxblocksize].
//  2. the multiple of align unit.
extern size_t optimizeblocksize(size_t block_size);

}  // namespace rocksdb
