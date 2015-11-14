// copyright 2008 google inc. all rights reserved.
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * neither the name of google inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// owner or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.
//
// internals shared between the snappy implementation and its unittest.

#ifndef util_snappy_snappy_internal_h_
#define util_snappy_snappy_internal_h_

#include "snappy-stubs-internal.h"

namespace snappy {
namespace internal {

class workingmemory {
 public:
  workingmemory() : large_table_(null) { }
  ~workingmemory() { delete[] large_table_; }

  // allocates and clears a hash table using memory in "*this",
  // stores the number of buckets in "*table_size" and returns a pointer to
  // the base of the hash table.
  uint16* gethashtable(size_t input_size, int* table_size);

 private:
  uint16 small_table_[1<<10];    // 2kb
  uint16* large_table_;          // allocated only when needed

  disallow_copy_and_assign(workingmemory);
};

// flat array compression that does not emit the "uncompressed length"
// prefix. compresses "input" string to the "*op" buffer.
//
// requires: "input_length <= kblocksize"
// requires: "op" points to an array of memory that is at least
// "maxcompressedlength(input_length)" in size.
// requires: all elements in "table[0..table_size-1]" are initialized to zero.
// requires: "table_size" is a power of two
//
// returns an "end" pointer into "op" buffer.
// "end - op" is the compressed size of "input".
char* compressfragment(const char* input,
                       size_t input_length,
                       char* op,
                       uint16* table,
                       const int table_size);

// return the largest n such that
//
//   s1[0,n-1] == s2[0,n-1]
//   and n <= (s2_limit - s2).
//
// does not read *s2_limit or beyond.
// does not read *(s1 + (s2_limit - s2)) or beyond.
// requires that s2_limit >= s2.
//
// separate implementation for x86_64, for speed.  uses the fact that
// x86_64 is little endian.
#if defined(arch_k8)
static inline int findmatchlength(const char* s1,
                                  const char* s2,
                                  const char* s2_limit) {
  assert(s2_limit >= s2);
  int matched = 0;

  // find out how long the match is. we loop over the data 64 bits at a
  // time until we find a 64-bit block that doesn't match; then we find
  // the first non-matching bit and use that to calculate the total
  // length of the match.
  while (predict_true(s2 <= s2_limit - 8)) {
    if (predict_false(unaligned_load64(s2) == unaligned_load64(s1 + matched))) {
      s2 += 8;
      matched += 8;
    } else {
      // on current (mid-2008) opteron models there is a 3% more
      // efficient code sequence to find the first non-matching byte.
      // however, what follows is ~10% better on intel core 2 and newer,
      // and we expect amd's bsf instruction to improve.
      uint64 x = unaligned_load64(s2) ^ unaligned_load64(s1 + matched);
      int matching_bits = bits::findlsbsetnonzero64(x);
      matched += matching_bits >> 3;
      return matched;
    }
  }
  while (predict_true(s2 < s2_limit)) {
    if (predict_true(s1[matched] == *s2)) {
      ++s2;
      ++matched;
    } else {
      return matched;
    }
  }
  return matched;
}
#else
static inline int findmatchlength(const char* s1,
                                  const char* s2,
                                  const char* s2_limit) {
  // implementation based on the x86-64 version, above.
  assert(s2_limit >= s2);
  int matched = 0;

  while (s2 <= s2_limit - 4 &&
         unaligned_load32(s2) == unaligned_load32(s1 + matched)) {
    s2 += 4;
    matched += 4;
  }
  if (littleendian::islittleendian() && s2 <= s2_limit - 4) {
    uint32 x = unaligned_load32(s2) ^ unaligned_load32(s1 + matched);
    int matching_bits = bits::findlsbsetnonzero(x);
    matched += matching_bits >> 3;
  } else {
    while ((s2 < s2_limit) && (s1[matched] == *s2)) {
      ++s2;
      ++matched;
    }
  }
  return matched;
}
#endif

}  // end namespace internal
}  // end namespace snappy

#endif  // util_snappy_snappy_internal_h_
