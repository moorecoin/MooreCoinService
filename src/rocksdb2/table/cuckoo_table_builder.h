//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#pragma once
#ifndef rocksdb_lite
#include <stdint.h>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include "rocksdb/status.h"
#include "table/table_builder.h"
#include "rocksdb/table.h"
#include "rocksdb/table_properties.h"
#include "util/autovector.h"

namespace rocksdb {

class cuckootablebuilder: public tablebuilder {
 public:
  cuckootablebuilder(
      writablefile* file, double max_hash_table_ratio,
      uint32_t max_num_hash_func, uint32_t max_search_depth,
      const comparator* user_comparator, uint32_t cuckoo_block_size,
      uint64_t (*get_slice_hash)(const slice&, uint32_t, uint64_t));

  // requires: either finish() or abandon() has been called.
  ~cuckootablebuilder() {}

  // add key,value to the table being constructed.
  // requires: key is after any previously added key according to comparator.
  // requires: finish(), abandon() have not been called
  void add(const slice& key, const slice& value) override;

  // return non-ok iff some error has been detected.
  status status() const override { return status_; }

  // finish building the table.  stops using the file passed to the
  // constructor after this function returns.
  // requires: finish(), abandon() have not been called
  status finish() override;

  // indicate that the contents of this builder should be abandoned.  stops
  // using the file passed to the constructor after this function returns.
  // if the caller is not going to call finish(), it must call abandon()
  // before destroying this builder.
  // requires: finish(), abandon() have not been called
  void abandon() override;

  // number of calls to add() so far.
  uint64_t numentries() const override;

  // size of the file generated so far.  if invoked after a successful
  // finish() call, returns the size of the final generated file.
  uint64_t filesize() const override;

 private:
  struct cuckoobucket {
    cuckoobucket()
      : vector_idx(kmaxvectoridx), make_space_for_key_call_id(0) {}
    uint32_t vector_idx;
    // this number will not exceed kvs_.size() + max_num_hash_func_.
    // we assume number of items is <= 2^32.
    uint32_t make_space_for_key_call_id;
  };
  static const uint32_t kmaxvectoridx = std::numeric_limits<int32_t>::max();

  bool makespaceforkey(
      const autovector<uint64_t>& hash_vals,
      const uint64_t call_id,
      std::vector<cuckoobucket>* buckets,
      uint64_t* bucket_id);
  status makehashtable(std::vector<cuckoobucket>* buckets);

  uint32_t num_hash_func_;
  writablefile* file_;
  const double max_hash_table_ratio_;
  const uint32_t max_num_hash_func_;
  const uint32_t max_search_depth_;
  const uint32_t cuckoo_block_size_;
  uint64_t hash_table_size_;
  bool is_last_level_file_;
  status status_;
  std::vector<std::pair<std::string, std::string>> kvs_;
  tableproperties properties_;
  bool has_seen_first_key_;
  const comparator* ucomp_;
  uint64_t (*get_slice_hash_)(const slice& s, uint32_t index,
    uint64_t max_num_buckets);
  std::string largest_user_key_ = "";
  std::string smallest_user_key_ = "";

  bool closed_;  // either finish() or abandon() has been called.

  // no copying allowed
  cuckootablebuilder(const cuckootablebuilder&) = delete;
  void operator=(const cuckootablebuilder&) = delete;
};

}  // namespace rocksdb

#endif  // rocksdb_lite
