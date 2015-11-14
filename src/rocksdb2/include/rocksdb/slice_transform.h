// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// class for specifying user-defined functions which perform a
// transformation on a slice.  it is not required that every slice
// belong to the domain and/or range of a function.  subclasses should
// define indomain and inrange to determine which slices are in either
// of these sets respectively.

#ifndef storage_rocksdb_include_slice_transform_h_
#define storage_rocksdb_include_slice_transform_h_

#include <string>

namespace rocksdb {

class slice;

class slicetransform {
 public:
  virtual ~slicetransform() {};

  // return the name of this transformation.
  virtual const char* name() const = 0;

  // transform a src in domain to a dst in the range
  virtual slice transform(const slice& src) const = 0;

  // determine whether this is a valid src upon the function applies
  virtual bool indomain(const slice& src) const = 0;

  // determine whether dst=transform(src) for some src
  virtual bool inrange(const slice& dst) const = 0;
};

extern const slicetransform* newfixedprefixtransform(size_t prefix_len);

extern const slicetransform* newnooptransform();

}

#endif  // storage_rocksdb_include_slice_transform_h_
