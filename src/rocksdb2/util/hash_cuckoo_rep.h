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

class hashcuckoorepfactory : public memtablerepfactory {
 public:
  // maxinum number of hash functions used in the cuckoo hash.
  static const unsigned int kmaxhashcount = 10;

  explicit hashcuckoorepfactory(size_t write_buffer_size,
                                size_t average_data_size,
                                unsigned int hash_function_count)
      : write_buffer_size_(write_buffer_size),
        average_data_size_(average_data_size),
        hash_function_count_(hash_function_count) {}

  virtual ~hashcuckoorepfactory() {}

  virtual memtablerep* creatememtablerep(
      const memtablerep::keycomparator& compare, arena* arena,
      const slicetransform* transform, logger* logger) override;

  virtual const char* name() const override { return "hashcuckoorepfactory"; }

 private:
  size_t write_buffer_size_;
  size_t average_data_size_;
  const unsigned int hash_function_count_;
};
}  // namespace rocksdb
#endif  // rocksdb_lite
