//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include <vector>

#include <stdint.h>
#include "rocksdb/slice.h"

namespace rocksdb {

class comparator;

class blockbuilder {
 public:
  blockbuilder(const blockbuilder&) = delete;
  void operator=(const blockbuilder&) = delete;
  
  explicit blockbuilder(int block_restart_interval);
  
  // reset the contents as if the blockbuilder was just constructed.
  void reset();

  // requires: finish() has not been callled since the last call to reset().
  // requires: key is larger than any previously added key
  void add(const slice& key, const slice& value);

  // finish building the block and return a slice that refers to the
  // block contents.  the returned slice will remain valid for the
  // lifetime of this builder or until reset() is called.
  slice finish();

  // returns an estimate of the current (uncompressed) size of the block
  // we are building.
  size_t currentsizeestimate() const;

  // returns an estimated block size after appending key and value.
  size_t estimatesizeafterkv(const slice& key, const slice& value) const;

  // return true iff no entries have been added since the last reset()
  bool empty() const {
    return buffer_.empty();
  }

 private:
  const int          block_restart_interval_;

  std::string           buffer_;    // destination buffer
  std::vector<uint32_t> restarts_;  // restart points
  int                   counter_;   // number of entries emitted since restart
  bool                  finished_;  // has finish() been called?
  std::string           last_key_;
};

}  // namespace rocksdb
