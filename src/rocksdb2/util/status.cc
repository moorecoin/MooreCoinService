//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <stdio.h>
#include "port/port.h"
#include "rocksdb/status.h"

namespace rocksdb {

const char* status::copystate(const char* state) {
  uint32_t size;
  memcpy(&size, state, sizeof(size));
  char* result = new char[size + 4];
  memcpy(result, state, size + 4);
  return result;
}

status::status(code code, const slice& msg, const slice& msg2) :
    code_(code) {
  assert(code != kok);
  const uint32_t len1 = msg.size();
  const uint32_t len2 = msg2.size();
  const uint32_t size = len1 + (len2 ? (2 + len2) : 0);
  char* result = new char[size + 4];
  memcpy(result, &size, sizeof(size));
  memcpy(result + 4, msg.data(), len1);
  if (len2) {
    result[4 + len1] = ':';
    result[5 + len1] = ' ';
    memcpy(result + 6 + len1, msg2.data(), len2);
  }
  state_ = result;
}

std::string status::tostring() const {
  char tmp[30];
  const char* type;
  switch (code_) {
    case kok:
      return "ok";
    case knotfound:
      type = "notfound: ";
      break;
    case kcorruption:
      type = "corruption: ";
      break;
    case knotsupported:
      type = "not implemented: ";
      break;
    case kinvalidargument:
      type = "invalid argument: ";
      break;
    case kioerror:
      type = "io error: ";
      break;
    case kmergeinprogress:
      type = "merge in progress: ";
      break;
    case kincomplete:
      type = "result incomplete: ";
      break;
    case kshutdowninprogress:
      type = "shutdown in progress: ";
      break;
    case ktimedout:
      type = "operation timed out: ";
      break;
    default:
      snprintf(tmp, sizeof(tmp), "unknown code(%d): ",
               static_cast<int>(code()));
      type = tmp;
      break;
  }
  std::string result(type);
  if (state_ != nullptr) {
    uint32_t length;
    memcpy(&length, state_, sizeof(length));
    result.append(state_ + 4, length);
  }
  return result;
}

}  // namespace rocksdb
