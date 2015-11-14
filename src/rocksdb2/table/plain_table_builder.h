//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#pragma once
#ifndef rocksdb_lite
#include <stdint.h>
#include <vector>
#include "rocksdb/options.h"
#include "rocksdb/status.h"
#include "table/table_builder.h"
#include "table/plain_table_key_coding.h"
#include "rocksdb/table.h"
#include "rocksdb/table_properties.h"
#include "table/bloom_block.h"
#include "table/plain_table_index.h"

namespace rocksdb {

class blockbuilder;
class blockhandle;
class writablefile;
class tablebuilder;

class plaintablebuilder: public tablebuilder {
 public:
  // create a builder that will store the contents of the table it is
  // building in *file.  does not close the file.  it is up to the
  // caller to close the file after calling finish(). the output file
  // will be part of level specified by 'level'.  a value of -1 means
  // that the caller does not know which level the output file will reside.
  plaintablebuilder(const options& options, writablefile* file,
                    uint32_t user_key_size, encodingtype encoding_type,
                    size_t index_sparseness, uint32_t bloom_bits_per_key,
                    uint32_t num_probes = 6, size_t huge_page_tlb_size = 0,
                    double hash_table_ratio = 0,
                    bool store_index_in_file = false);

  // requires: either finish() or abandon() has been called.
  ~plaintablebuilder();

  // add key,value to the table being constructed.
  // requires: key is after any previously added key according to comparator.
  // requires: finish(), abandon() have not been called
  void add(const slice& key, const slice& value) override;

  // return non-ok iff some error has been detected.
  status status() const override;

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

  bool saveindexinfile() const { return store_index_in_file_; }

 private:
  arena arena_;
  options options_;
  std::vector<std::unique_ptr<tablepropertiescollector>>
      table_properties_collectors_;

  bloomblockbuilder bloom_block_;
  std::unique_ptr<plaintableindexbuilder> index_builder_;

  writablefile* file_;
  uint64_t offset_ = 0;
  uint32_t bloom_bits_per_key_;
  uint32_t huge_page_tlb_size_;
  status status_;
  tableproperties properties_;
  plaintablekeyencoder encoder_;

  bool store_index_in_file_;

  std::vector<uint32_t> keys_or_prefixes_hashes_;
  bool closed_ = false;  // either finish() or abandon() has been called.

  const slicetransform* prefix_extractor_;

  slice getprefix(const slice& target) const {
    assert(target.size() >= 8);  // target is internal key
    return getprefixfromuserkey(getuserkey(target));
  }

  slice getprefix(const parsedinternalkey& target) const {
    return getprefixfromuserkey(target.user_key);
  }

  slice getuserkey(const slice& key) const {
    return slice(key.data(), key.size() - 8);
  }

  slice getprefixfromuserkey(const slice& user_key) const {
    if (!istotalordermode()) {
      return prefix_extractor_->transform(user_key);
    } else {
      // use empty slice as prefix if prefix_extractor is not set.
      // in that case,
      // it falls back to pure binary search and
      // total iterator seek is supported.
      return slice();
    }
  }

  bool istotalordermode() const { return (prefix_extractor_ == nullptr); }

  // no copying allowed
  plaintablebuilder(const plaintablebuilder&) = delete;
  void operator=(const plaintablebuilder&) = delete;
};

}  // namespace rocksdb

#endif  // rocksdb_lite
