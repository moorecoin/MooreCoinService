//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// simple hash function used for internal data structures

#pragma once
#include <stddef.h>
#include <stdint.h>

namespace rocksdb {

extern uint32_t hash(const char* data, size_t n, uint32_t seed);

inline uint32_t bloomhash(const slice& key) {
  return hash(key.data(), key.size(), 0xbc9f1d34);
}

inline uint32_t getslicehash(const slice& s) {
  return hash(s.data(), s.size(), 397);
}
}
