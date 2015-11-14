//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "table/filter_block.h"

#include "db/dbformat.h"
#include "rocksdb/filter_policy.h"
#include "util/coding.h"

namespace rocksdb {

// see doc/table_format.txt for an explanation of the filter block format.

// generate new filter every 2kb of data
static const size_t kfilterbaselg = 11;
static const size_t kfilterbase = 1 << kfilterbaselg;

filterblockbuilder::filterblockbuilder(const options& opt,
                                       const blockbasedtableoptions& table_opt,
                                       const comparator* internal_comparator)
    : policy_(table_opt.filter_policy.get()),
      prefix_extractor_(opt.prefix_extractor.get()),
      whole_key_filtering_(table_opt.whole_key_filtering),
      comparator_(internal_comparator) {}

void filterblockbuilder::startblock(uint64_t block_offset) {
  uint64_t filter_index = (block_offset / kfilterbase);
  assert(filter_index >= filter_offsets_.size());
  while (filter_index > filter_offsets_.size()) {
    generatefilter();
  }
}

bool filterblockbuilder::sameprefix(const slice &key1,
                                    const slice &key2) const {
  if (!prefix_extractor_->indomain(key1) &&
      !prefix_extractor_->indomain(key2)) {
    return true;
  } else if (!prefix_extractor_->indomain(key1) ||
             !prefix_extractor_->indomain(key2)) {
    return false;
  } else {
    return (prefix_extractor_->transform(key1) ==
            prefix_extractor_->transform(key2));
  }
}

void filterblockbuilder::addkey(const slice& key) {
  // get slice for most recently added entry
  slice prev;
  size_t added_to_start = 0;

  // add key to filter if needed
  if (whole_key_filtering_) {
    start_.push_back(entries_.size());
    ++added_to_start;
    entries_.append(key.data(), key.size());
  }

  if (start_.size() > added_to_start) {
    size_t prev_start = start_[start_.size() - 1 - added_to_start];
    const char* base = entries_.data() + prev_start;
    size_t length = entries_.size() - prev_start;
    prev = slice(base, length);
  }

  // add prefix to filter if needed
  if (prefix_extractor_ && prefix_extractor_->indomain(key)) {
    // this assumes prefix(prefix(key)) == prefix(key), as the last
    // entry in entries_ may be either a key or prefix, and we use
    // prefix(last entry) to get the prefix of the last key.
    if (prev.size() == 0 || !sameprefix(key, prev)) {
      slice prefix = prefix_extractor_->transform(key);
      start_.push_back(entries_.size());
      entries_.append(prefix.data(), prefix.size());
    }
  }
}

slice filterblockbuilder::finish() {
  if (!start_.empty()) {
    generatefilter();
  }

  // append array of per-filter offsets
  const uint32_t array_offset = result_.size();
  for (size_t i = 0; i < filter_offsets_.size(); i++) {
    putfixed32(&result_, filter_offsets_[i]);
  }

  putfixed32(&result_, array_offset);
  result_.push_back(kfilterbaselg);  // save encoding parameter in result
  return slice(result_);
}

void filterblockbuilder::generatefilter() {
  const size_t num_entries = start_.size();
  if (num_entries == 0) {
    // fast path if there are no keys for this filter
    filter_offsets_.push_back(result_.size());
    return;
  }

  // make list of keys from flattened key structure
  start_.push_back(entries_.size());  // simplify length computation
  tmp_entries_.resize(num_entries);
  for (size_t i = 0; i < num_entries; i++) {
    const char* base = entries_.data() + start_[i];
    size_t length = start_[i+1] - start_[i];
    tmp_entries_[i] = slice(base, length);
  }

  // generate filter for current set of keys and append to result_.
  filter_offsets_.push_back(result_.size());
  policy_->createfilter(&tmp_entries_[0], num_entries, &result_);

  tmp_entries_.clear();
  entries_.clear();
  start_.clear();
}

filterblockreader::filterblockreader(
    const options& opt, const blockbasedtableoptions& table_opt,
    const slice& contents, bool delete_contents_after_use)
    : policy_(table_opt.filter_policy.get()),
      prefix_extractor_(opt.prefix_extractor.get()),
      whole_key_filtering_(table_opt.whole_key_filtering),
      data_(nullptr),
      offset_(nullptr),
      num_(0),
      base_lg_(0) {
  size_t n = contents.size();
  if (n < 5) return;  // 1 byte for base_lg_ and 4 for start of offset array
  base_lg_ = contents[n-1];
  uint32_t last_word = decodefixed32(contents.data() + n - 5);
  if (last_word > n - 5) return;
  data_ = contents.data();
  offset_ = data_ + last_word;
  num_ = (n - 5 - last_word) / 4;
  if (delete_contents_after_use) {
    filter_data.reset(contents.data());
  }
}

bool filterblockreader::keymaymatch(uint64_t block_offset,
                                    const slice& key) {
  if (!whole_key_filtering_) {
    return true;
  }
  return maymatch(block_offset, key);
}

bool filterblockreader::prefixmaymatch(uint64_t block_offset,
                                       const slice& prefix) {
  if (!prefix_extractor_) {
    return true;
  }
  return maymatch(block_offset, prefix);
}

bool filterblockreader::maymatch(uint64_t block_offset, const slice& entry) {
  uint64_t index = block_offset >> base_lg_;
  if (index < num_) {
    uint32_t start = decodefixed32(offset_ + index*4);
    uint32_t limit = decodefixed32(offset_ + index*4 + 4);
    if (start <= limit && limit <= (uint32_t)(offset_ - data_)) {
      slice filter = slice(data_ + start, limit - start);
      return policy_->keymaymatch(entry, filter);
    } else if (start == limit) {
      // empty filters do not match any entries
      return false;
    }
  }
  return true;  // errors are treated as potential matches
}

size_t filterblockreader::approximatememoryusage() const {
  return num_ * 4 + 5 + (offset_ - data_);
}
}
