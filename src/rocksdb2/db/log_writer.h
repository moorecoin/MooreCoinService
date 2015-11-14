//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include <memory>
#include <stdint.h>
#include "db/log_format.h"
#include "rocksdb/slice.h"
#include "rocksdb/status.h"

namespace rocksdb {

class writablefile;

using std::unique_ptr;

namespace log {

class writer {
 public:
  // create a writer that will append data to "*dest".
  // "*dest" must be initially empty.
  // "*dest" must remain live while this writer is in use.
  explicit writer(unique_ptr<writablefile>&& dest);
  ~writer();

  status addrecord(const slice& slice);

  writablefile* file() { return dest_.get(); }
  const writablefile* file() const { return dest_.get(); }

 private:
  unique_ptr<writablefile> dest_;
  int block_offset_;       // current offset in block

  // crc32c values for all supported record types.  these are
  // pre-computed to reduce the overhead of computing the crc of the
  // record type stored in the header.
  uint32_t type_crc_[kmaxrecordtype + 1];

  status emitphysicalrecord(recordtype type, const char* ptr, size_t length);

  // no copying allowed
  writer(const writer&);
  void operator=(const writer&);
};

}  // namespace log
}  // namespace rocksdb
