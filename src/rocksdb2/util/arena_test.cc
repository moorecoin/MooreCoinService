//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/arena.h"
#include "util/random.h"
#include "util/testharness.h"

namespace rocksdb {

class arenatest {};

test(arenatest, empty) { arena arena0; }

test(arenatest, memoryallocatedbytes) {
  const int n = 17;
  size_t req_sz;  // requested size
  size_t bsz = 8192;  // block size
  size_t expected_memory_allocated;

  arena arena(bsz);

  // requested size > quarter of a block:
  //   allocate requested size separately
  req_sz = 3001;
  for (int i = 0; i < n; i++) {
    arena.allocate(req_sz);
  }
  expected_memory_allocated = req_sz * n + arena::kinlinesize;
  assert_eq(arena.memoryallocatedbytes(), expected_memory_allocated);

  arena.allocate(arena::kinlinesize - 1);

  // requested size < quarter of a block:
  //   allocate a block with the default size, then try to use unused part
  //   of the block. so one new block will be allocated for the first
  //   allocate(99) call. all the remaining calls won't lead to new allocation.
  req_sz = 99;
  for (int i = 0; i < n; i++) {
    arena.allocate(req_sz);
  }
  expected_memory_allocated += bsz;
  assert_eq(arena.memoryallocatedbytes(), expected_memory_allocated);

  // requested size > quarter of a block:
  //   allocate requested size separately
  req_sz = 99999999;
  for (int i = 0; i < n; i++) {
    arena.allocate(req_sz);
  }
  expected_memory_allocated += req_sz * n;
  assert_eq(arena.memoryallocatedbytes(), expected_memory_allocated);
}

// make sure we didn't count the allocate but not used memory space in
// arena::approximatememoryusage()
test(arenatest, approximatememoryusagetest) {
  const size_t kblocksize = 4096;
  const size_t kentrysize = kblocksize / 8;
  const size_t kzero = 0;
  arena arena(kblocksize);
  assert_eq(kzero, arena.approximatememoryusage());

  // allocate inline bytes
  arena.allocatealigned(8);
  arena.allocatealigned(arena::kinlinesize / 2 - 16);
  arena.allocatealigned(arena::kinlinesize / 2);
  assert_eq(arena.approximatememoryusage(), arena::kinlinesize - 8);
  assert_eq(arena.memoryallocatedbytes(), arena::kinlinesize);

  auto num_blocks = kblocksize / kentrysize;

  // first allocation
  arena.allocatealigned(kentrysize);
  auto mem_usage = arena.memoryallocatedbytes();
  assert_eq(mem_usage, kblocksize + arena::kinlinesize);
  auto usage = arena.approximatememoryusage();
  assert_lt(usage, mem_usage);
  for (size_t i = 1; i < num_blocks; ++i) {
    arena.allocatealigned(kentrysize);
    assert_eq(mem_usage, arena.memoryallocatedbytes());
    assert_eq(arena.approximatememoryusage(), usage + kentrysize);
    usage = arena.approximatememoryusage();
  }
  assert_gt(usage, mem_usage);
}

test(arenatest, simple) {
  std::vector<std::pair<size_t, char*>> allocated;
  arena arena;
  const int n = 100000;
  size_t bytes = 0;
  random rnd(301);
  for (int i = 0; i < n; i++) {
    size_t s;
    if (i % (n / 10) == 0) {
      s = i;
    } else {
      s = rnd.onein(4000)
              ? rnd.uniform(6000)
              : (rnd.onein(10) ? rnd.uniform(100) : rnd.uniform(20));
    }
    if (s == 0) {
      // our arena disallows size 0 allocations.
      s = 1;
    }
    char* r;
    if (rnd.onein(10)) {
      r = arena.allocatealigned(s);
    } else {
      r = arena.allocate(s);
    }

    for (unsigned int b = 0; b < s; b++) {
      // fill the "i"th allocation with a known bit pattern
      r[b] = i % 256;
    }
    bytes += s;
    allocated.push_back(std::make_pair(s, r));
    assert_ge(arena.approximatememoryusage(), bytes);
    if (i > n / 10) {
      assert_le(arena.approximatememoryusage(), bytes * 1.10);
    }
  }
  for (unsigned int i = 0; i < allocated.size(); i++) {
    size_t num_bytes = allocated[i].first;
    const char* p = allocated[i].second;
    for (unsigned int b = 0; b < num_bytes; b++) {
      // check the "i"th allocation for the known bit pattern
      assert_eq(int(p[b]) & 0xff, (int)(i % 256));
    }
  }
}

}  // namespace rocksdb

int main(int argc, char** argv) { return rocksdb::test::runalltests(); }
