// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef rocksdb_lite
#pragma once
#include "rocksdb/slice_transform.h"
#include "rocksdb/memtablerep.h"

namespace rocksdb {

class hashskiplistrepfactory : public memtablerepfactory {
 public:
  explicit hashskiplistrepfactory(
    size_t bucket_count,
    int32_t skiplist_height,
    int32_t skiplist_branching_factor)
      : bucket_count_(bucket_count),
        skiplist_height_(skiplist_height),
        skiplist_branching_factor_(skiplist_branching_factor) { }

  virtual ~hashskiplistrepfactory() {}

  virtual memtablerep* creatememtablerep(
      const memtablerep::keycomparator& compare, arena* arena,
      const slicetransform* transform, logger* logger) override;

  virtual const char* name() const override {
    return "hashskiplistrepfactory";
  }

 private:
  const size_t bucket_count_;
  const int32_t skiplist_height_;
  const int32_t skiplist_branching_factor_;
};

}
#endif  // rocksdb_lite
