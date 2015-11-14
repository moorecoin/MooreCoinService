//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once

namespace rocksdb {

class slice;
class status;

// tablebuilder provides the interface used to build a table
// (an immutable and sorted map from keys to values).
//
// multiple threads can invoke const methods on a tablebuilder without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same tablebuilder must use
// external synchronization.
class tablebuilder {
 public:
  // requires: either finish() or abandon() has been called.
  virtual ~tablebuilder() {}

  // add key,value to the table being constructed.
  // requires: key is after any previously added key according to comparator.
  // requires: finish(), abandon() have not been called
  virtual void add(const slice& key, const slice& value) = 0;

  // return non-ok iff some error has been detected.
  virtual status status() const = 0;

  // finish building the table.
  // requires: finish(), abandon() have not been called
  virtual status finish() = 0;

  // indicate that the contents of this builder should be abandoned.
  // if the caller is not going to call finish(), it must call abandon()
  // before destroying this builder.
  // requires: finish(), abandon() have not been called
  virtual void abandon() = 0;

  // number of calls to add() so far.
  virtual uint64_t numentries() const = 0;

  // size of the file generated so far.  if invoked after a successful
  // finish() call, returns the size of the final generated file.
  virtual uint64_t filesize() const = 0;
};

}  // namespace rocksdb
