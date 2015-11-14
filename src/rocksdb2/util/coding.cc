//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/coding.h"

#include <algorithm>
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"

namespace rocksdb {

char* encodevarint32(char* dst, uint32_t v) {
  // operate on characters as unsigneds
  unsigned char* ptr = reinterpret_cast<unsigned char*>(dst);
  static const int b = 128;
  if (v < (1 << 7)) {
    *(ptr++) = v;
  } else if (v < (1 << 14)) {
    *(ptr++) = v | b;
    *(ptr++) = v >> 7;
  } else if (v < (1 << 21)) {
    *(ptr++) = v | b;
    *(ptr++) = (v >> 7) | b;
    *(ptr++) = v >> 14;
  } else if (v < (1 << 28)) {
    *(ptr++) = v | b;
    *(ptr++) = (v >> 7) | b;
    *(ptr++) = (v >> 14) | b;
    *(ptr++) = v >> 21;
  } else {
    *(ptr++) = v | b;
    *(ptr++) = (v >> 7) | b;
    *(ptr++) = (v >> 14) | b;
    *(ptr++) = (v >> 21) | b;
    *(ptr++) = v >> 28;
  }
  return reinterpret_cast<char*>(ptr);
}

const char* getvarint32ptrfallback(const char* p, const char* limit,
                                   uint32_t* value) {
  uint32_t result = 0;
  for (uint32_t shift = 0; shift <= 28 && p < limit; shift += 7) {
    uint32_t byte = *(reinterpret_cast<const unsigned char*>(p));
    p++;
    if (byte & 128) {
      // more bytes are present
      result |= ((byte & 127) << shift);
    } else {
      result |= (byte << shift);
      *value = result;
      return reinterpret_cast<const char*>(p);
    }
  }
  return nullptr;
}

const char* getvarint64ptr(const char* p, const char* limit, uint64_t* value) {
  uint64_t result = 0;
  for (uint32_t shift = 0; shift <= 63 && p < limit; shift += 7) {
    uint64_t byte = *(reinterpret_cast<const unsigned char*>(p));
    p++;
    if (byte & 128) {
      // more bytes are present
      result |= ((byte & 127) << shift);
    } else {
      result |= (byte << shift);
      *value = result;
      return reinterpret_cast<const char*>(p);
    }
  }
  return nullptr;
}

}  // namespace rocksdb
