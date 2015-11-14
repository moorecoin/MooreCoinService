// copyright (c) 2014, facebook, inc. all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#pragma once
#ifndef rocksdb_lite

#include <string>
#include "rocksdb/table.h"
#include "util/murmurhash.h"

namespace rocksdb {

const uint32_t kcuckoomurmurseedmultiplier = 816922183;
static inline uint64_t cuckoohash(
    const slice& user_key, uint32_t hash_cnt, uint64_t table_size_minus_one,
    uint64_t (*get_slice_hash)(const slice&, uint32_t, uint64_t)) {
#ifndef ndebug
  // this part is used only in unit tests.
  if (get_slice_hash != nullptr) {
    return get_slice_hash(user_key, hash_cnt, table_size_minus_one + 1);
  }
#endif
  return murmurhash(user_key.data(), user_key.size(),
      kcuckoomurmurseedmultiplier * hash_cnt) & table_size_minus_one;
}

// cuckoo table is designed for applications that require fast point lookups
// but not fast range scans.
//
// some assumptions:
// - key length and value length are fixed.
// - does not support snapshot.
// - does not support merge operations.
class cuckootablefactory : public tablefactory {
 public:
  cuckootablefactory(double hash_table_ratio, uint32_t max_search_depth,
      uint32_t cuckoo_block_size)
    : hash_table_ratio_(hash_table_ratio),
      max_search_depth_(max_search_depth),
      cuckoo_block_size_(cuckoo_block_size) {}
  ~cuckootablefactory() {}

  const char* name() const override { return "cuckootable"; }

  status newtablereader(
      const options& options, const envoptions& soptions,
      const internalkeycomparator& internal_comparator,
      unique_ptr<randomaccessfile>&& file, uint64_t file_size,
      unique_ptr<tablereader>* table) const override;

  tablebuilder* newtablebuilder(const options& options,
      const internalkeycomparator& icomparator, writablefile* file,
      compressiontype compression_type) const override;

  // sanitizes the specified db options.
  status sanitizedboptions(const dboptions* db_opts) const override {
    return status::ok();
  }

  std::string getprintabletableoptions() const override;

 private:
  const double hash_table_ratio_;
  const uint32_t max_search_depth_;
  const uint32_t cuckoo_block_size_;
};

}  // namespace rocksdb
#endif  // rocksdb_lite
