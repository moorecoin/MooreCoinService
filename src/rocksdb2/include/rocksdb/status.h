// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// a status encapsulates the result of an operation.  it may indicate success,
// or it may indicate an error with an associated error message.
//
// multiple threads can invoke const methods on a status without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same status must use
// external synchronization.

#ifndef storage_rocksdb_include_status_h_
#define storage_rocksdb_include_status_h_

#include <string>
#include "rocksdb/slice.h"

namespace rocksdb {

class status {
 public:
  // create a success status.
  status() : code_(kok), state_(nullptr) { }
  ~status() { delete[] state_; }

  // copy the specified status.
  status(const status& s);
  void operator=(const status& s);

  // return a success status.
  static status ok() { return status(); }

  // return error status of an appropriate type.
  static status notfound(const slice& msg, const slice& msg2 = slice()) {
    return status(knotfound, msg, msg2);
  }
  // fast path for not found without malloc;
  static status notfound() {
    return status(knotfound);
  }
  static status corruption(const slice& msg, const slice& msg2 = slice()) {
    return status(kcorruption, msg, msg2);
  }
  static status notsupported(const slice& msg, const slice& msg2 = slice()) {
    return status(knotsupported, msg, msg2);
  }
  static status invalidargument(const slice& msg, const slice& msg2 = slice()) {
    return status(kinvalidargument, msg, msg2);
  }
  static status ioerror(const slice& msg, const slice& msg2 = slice()) {
    return status(kioerror, msg, msg2);
  }
  static status mergeinprogress(const slice& msg, const slice& msg2 = slice()) {
    return status(kmergeinprogress, msg, msg2);
  }
  static status incomplete(const slice& msg, const slice& msg2 = slice()) {
    return status(kincomplete, msg, msg2);
  }
  static status shutdowninprogress(const slice& msg,
                                   const slice& msg2 = slice()) {
    return status(kshutdowninprogress, msg, msg2);
  }
  static status timedout() {
    return status(ktimedout);
  }
  static status timedout(const slice& msg, const slice& msg2 = slice()) {
    return status(ktimedout, msg, msg2);
  }

  // returns true iff the status indicates success.
  bool ok() const { return code() == kok; }

  // returns true iff the status indicates a notfound error.
  bool isnotfound() const { return code() == knotfound; }

  // returns true iff the status indicates a corruption error.
  bool iscorruption() const { return code() == kcorruption; }

  // returns true iff the status indicates a notsupported error.
  bool isnotsupported() const { return code() == knotsupported; }

  // returns true iff the status indicates an invalidargument error.
  bool isinvalidargument() const { return code() == kinvalidargument; }

  // returns true iff the status indicates an ioerror.
  bool isioerror() const { return code() == kioerror; }

  // returns true iff the status indicates an mergeinprogress.
  bool ismergeinprogress() const { return code() == kmergeinprogress; }

  // returns true iff the status indicates incomplete
  bool isincomplete() const { return code() == kincomplete; }

  // returns true iff the status indicates shutdown in progress
  bool isshutdowninprogress() const { return code() == kshutdowninprogress; }

  bool istimedout() const { return code() == ktimedout; }

  // return a string representation of this status suitable for printing.
  // returns the string "ok" for success.
  std::string tostring() const;

  enum code {
    kok = 0,
    knotfound = 1,
    kcorruption = 2,
    knotsupported = 3,
    kinvalidargument = 4,
    kioerror = 5,
    kmergeinprogress = 6,
    kincomplete = 7,
    kshutdowninprogress = 8,
    ktimedout = 9
  };

  code code() const {
    return code_;
  }
 private:
  // a nullptr state_ (which is always the case for ok) means the message
  // is empty.
  // of the following form:
  //    state_[0..3] == length of message
  //    state_[4..]  == message
  code code_;
  const char* state_;

  explicit status(code code) : code_(code), state_(nullptr) { }
  status(code code, const slice& msg, const slice& msg2);
  static const char* copystate(const char* s);
};

inline status::status(const status& s) {
  code_ = s.code_;
  state_ = (s.state_ == nullptr) ? nullptr : copystate(s.state_);
}
inline void status::operator=(const status& s) {
  // the following condition catches both aliasing (when this == &s),
  // and the common case where both s and *this are ok.
  code_ = s.code_;
  if (state_ != s.state_) {
    delete[] state_;
    state_ = (s.state_ == nullptr) ? nullptr : copystate(s.state_);
  }
}

}  // namespace rocksdb

#endif  // storage_rocksdb_include_status_h_
