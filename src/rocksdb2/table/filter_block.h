//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// a filter block is stored near the end of a table file.  it contains
// filters (e.g., bloom filters) for all data blocks in the table combined
// into a single filter block.

#pragma once

#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/table.h"
#include "util/hash.h"

namespace rocksdb {

class filterpolicy;

// a filterblockbuilder is used to construct all of the filters for a
// particular table.  it generates a single string which is stored as
// a special block in the table.
//
// the sequence of calls to filterblockbuilder must match the regexp:
//      (startblock addkey*)* finish
class filterblockbuilder {
 public:
  explicit filterblockbuilder(const options& opt,
                              const blockbasedtableoptions& table_opt,
                              const comparator* internal_comparator);

  void startblock(uint64_t block_offset);
  void addkey(const slice& key);
  slice finish();

 private:
  bool sameprefix(const slice &key1, const slice &key2) const;
  void generatefilter();

  // important: all of these might point to invalid addresses
  // at the time of destruction of this filter block. destructor
  // should not dereference them.
  const filterpolicy* policy_;
  const slicetransform* prefix_extractor_;
  bool whole_key_filtering_;
  const comparator* comparator_;

  std::string entries_;         // flattened entry contents
  std::vector<size_t> start_;   // starting index in entries_ of each entry
  std::string result_;          // filter data computed so far
  std::vector<slice> tmp_entries_; // policy_->createfilter() argument
  std::vector<uint32_t> filter_offsets_;

  // no copying allowed
  filterblockbuilder(const filterblockbuilder&);
  void operator=(const filterblockbuilder&);
};

class filterblockreader {
 public:
 // requires: "contents" and *policy must stay live while *this is live.
  filterblockreader(
    const options& opt,
    const blockbasedtableoptions& table_opt,
    const slice& contents,
    bool delete_contents_after_use = false);
  bool keymaymatch(uint64_t block_offset, const slice& key);
  bool prefixmaymatch(uint64_t block_offset, const slice& prefix);
  size_t approximatememoryusage() const;

 private:
  const filterpolicy* policy_;
  const slicetransform* prefix_extractor_;
  bool whole_key_filtering_;
  const char* data_;    // pointer to filter data (at block-start)
  const char* offset_;  // pointer to beginning of offset array (at block-end)
  size_t num_;          // number of entries in offset array
  size_t base_lg_;      // encoding parameter (see kfilterbaselg in .cc file)
  std::unique_ptr<const char[]> filter_data;


  bool maymatch(uint64_t block_offset, const slice& entry);
};

}
