//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// must not be included from any .h files to avoid polluting the namespace
// with macros.

#pragma once
#include <stdio.h>
#include <stdint.h>
#include <string>
#include "port/port.h"

namespace rocksdb {

class slice;
class writablefile;

// append a human-readable size in bytes
int appendhumanbytes(uint64_t bytes, char* output, int len);

// append a human-readable printout of "num" to *str
extern void appendnumberto(std::string* str, uint64_t num);

// append a human-readable printout of "value" to *str.
// escapes any non-printable characters found in "value".
extern void appendescapedstringto(std::string* str, const slice& value);

// return a human-readable printout of "num"
extern std::string numbertostring(uint64_t num);

// return a human-readable version of "value".
// escapes any non-printable characters found in "value".
extern std::string escapestring(const slice& value);

// parse a human-readable number from "*in" into *value.  on success,
// advances "*in" past the consumed number and sets "*val" to the
// numeric value.  otherwise, returns false and leaves *in in an
// unspecified state.
extern bool consumedecimalnumber(slice* in, uint64_t* val);

}  // namespace rocksdb
