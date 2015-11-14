//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
/*
  murmurhash from http://sites.google.com/site/murmurhash/

  all code is released to the public domain. for business purposes, murmurhash is
  under the mit license.
*/
#pragma once
#include <stdint.h>
#include "rocksdb/slice.h"

#if defined(__x86_64__)
#define murmur_hash murmurhash64a
uint64_t murmurhash64a ( const void * key, int len, unsigned int seed );
#define murmurhash murmurhash64a
typedef uint64_t murmur_t;

#elif defined(__i386__)
#define murmur_hash murmurhash2
unsigned int murmurhash2 ( const void * key, int len, unsigned int seed );
#define murmurhash murmurhash2
typedef unsigned int murmur_t;

#else
#define murmur_hash murmurhashneutral2
unsigned int murmurhashneutral2 ( const void * key, int len, unsigned int seed );
#define murmurhash murmurhashneutral2
typedef unsigned int murmur_t;
#endif

// allow slice to be hashable by murmur hash.
namespace rocksdb {
struct murmur_hash {
  size_t operator()(const slice& slice) const {
    return murmurhash(slice.data(), slice.size(), 0);
  }
};
}  // rocksdb
