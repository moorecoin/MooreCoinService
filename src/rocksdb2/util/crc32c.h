//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include <stddef.h>
#include <stdint.h>

namespace rocksdb {
namespace crc32c {

// return the crc32c of concat(a, data[0,n-1]) where init_crc is the
// crc32c of some string a.  extend() is often used to maintain the
// crc32c of a stream of data.
extern uint32_t extend(uint32_t init_crc, const char* data, size_t n);

// return the crc32c of data[0,n-1]
inline uint32_t value(const char* data, size_t n) {
  return extend(0, data, n);
}

static const uint32_t kmaskdelta = 0xa282ead8ul;

// return a masked representation of crc.
//
// motivation: it is problematic to compute the crc of a string that
// contains embedded crcs.  therefore we recommend that crcs stored
// somewhere (e.g., in files) should be masked before being stored.
inline uint32_t mask(uint32_t crc) {
  // rotate right by 15 bits and add a constant.
  return ((crc >> 15) | (crc << 17)) + kmaskdelta;
}

// return the crc whose masked representation is masked_crc.
inline uint32_t unmask(uint32_t masked_crc) {
  uint32_t rot = masked_crc - kmaskdelta;
  return ((rot >> 17) | (rot << 15));
}

}  // namespace crc32c
}  // namespace rocksdb
