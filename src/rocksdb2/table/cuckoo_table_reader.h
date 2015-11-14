//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#ifndef rocksdb_lite
#include <string>
#include <memory>
#include <utility>
#include <vector>

#include "db/dbformat.h"
#include "rocksdb/env.h"
#include "table/table_reader.h"

namespace rocksdb {

class arena;
class tablereader;

class cuckootablereader: public tablereader {
 public:
  cuckootablereader(
      const options& options,
      std::unique_ptr<randomaccessfile>&& file,
      uint64_t file_size,
      const comparator* user_comparator,
      uint64_t (*get_slice_hash)(const slice&, uint32_t, uint64_t));
  ~cuckootablereader() {}

  std::shared_ptr<const tableproperties> gettableproperties() const override {
    return table_props_;
  }

  status status() const { return status_; }

  status get(
      const readoptions& readoptions, const slice& key, void* handle_context,
      bool (*result_handler)(void* arg, const parsedinternalkey& k,
                             const slice& v),
      void (*mark_key_may_exist_handler)(void* handle_context) = nullptr)
    override;

  iterator* newiterator(const readoptions&, arena* arena = nullptr) override;
  void prepare(const slice& target) override;

  // report an approximation of how much memory has been used.
  size_t approximatememoryusage() const override;

  // following methods are not implemented for cuckoo table reader
  uint64_t approximateoffsetof(const slice& key) override { return 0; }
  void setupforcompaction() override {}
  // end of methods not implemented.

 private:
  friend class cuckootableiterator;
  void loadallkeys(std::vector<std::pair<slice, uint32_t>>* key_to_bucket_id);
  std::unique_ptr<randomaccessfile> file_;
  slice file_data_;
  bool is_last_level_;
  std::shared_ptr<const tableproperties> table_props_;
  status status_;
  uint32_t num_hash_func_;
  std::string unused_key_;
  uint32_t key_length_;
  uint32_t value_length_;
  uint32_t bucket_length_;
  uint32_t cuckoo_block_size_;
  uint32_t cuckoo_block_bytes_minus_one_;
  uint64_t table_size_minus_one_;
  const comparator* ucomp_;
  uint64_t (*get_slice_hash_)(const slice& s, uint32_t index,
      uint64_t max_num_buckets);
};

}  // namespace rocksdb
#endif  // rocksdb_lite
