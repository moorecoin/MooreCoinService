//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "rocksdb/slice_transform.h"
#include "rocksdb/slice.h"

namespace rocksdb {

namespace {

class fixedprefixtransform : public slicetransform {
 private:
  size_t prefix_len_;
  std::string name_;

 public:
  explicit fixedprefixtransform(size_t prefix_len)
      : prefix_len_(prefix_len),
        name_("rocksdb.fixedprefix." + std::to_string(prefix_len_)) {}

  virtual const char* name() const { return name_.c_str(); }

  virtual slice transform(const slice& src) const {
    assert(indomain(src));
    return slice(src.data(), prefix_len_);
  }

  virtual bool indomain(const slice& src) const {
    return (src.size() >= prefix_len_);
  }

  virtual bool inrange(const slice& dst) const {
    return (dst.size() == prefix_len_);
  }
};

class nooptransform : public slicetransform {
 public:
  explicit nooptransform() { }

  virtual const char* name() const {
    return "rocksdb.noop";
  }

  virtual slice transform(const slice& src) const {
    return src;
  }

  virtual bool indomain(const slice& src) const {
    return true;
  }

  virtual bool inrange(const slice& dst) const {
    return true;
  }
};

}

const slicetransform* newfixedprefixtransform(size_t prefix_len) {
  return new fixedprefixtransform(prefix_len);
}

const slicetransform* newnooptransform() {
  return new nooptransform;
}

}  // namespace rocksdb
