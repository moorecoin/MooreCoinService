//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include "table/plain_table_index.h"
#include "util/coding.h"
#include "util/hash.h"

namespace rocksdb {

namespace {
inline uint32_t getbucketidfromhash(uint32_t hash, uint32_t num_buckets) {
  assert(num_buckets > 0);
  return hash % num_buckets;
}
}

status plaintableindex::initfromrawdata(slice data) {
  if (!getvarint32(&data, &index_size_)) {
    return status::corruption("couldn't read the index size!");
  }
  assert(index_size_ > 0);
  if (!getvarint32(&data, &num_prefixes_)) {
    return status::corruption("couldn't read the index size!");
  }
  sub_index_size_ = data.size() - index_size_ * koffsetlen;

  char* index_data_begin = const_cast<char*>(data.data());
  index_ = reinterpret_cast<uint32_t*>(index_data_begin);
  sub_index_ = reinterpret_cast<char*>(index_ + index_size_);
  return status::ok();
}

plaintableindex::indexsearchresult plaintableindex::getoffset(
    uint32_t prefix_hash, uint32_t* bucket_value) const {
  int bucket = getbucketidfromhash(prefix_hash, index_size_);
  *bucket_value = index_[bucket];
  if ((*bucket_value & ksubindexmask) == ksubindexmask) {
    *bucket_value ^= ksubindexmask;
    return ksubindex;
  }
  if (*bucket_value >= kmaxfilesize) {
    return knoprefixforbucket;
  } else {
    // point directly to the file
    return kdirecttofile;
  }
}

void plaintableindexbuilder::indexrecordlist::addrecord(murmur_t hash,
                                                        uint32_t offset) {
  if (num_records_in_current_group_ == knumrecordspergroup) {
    current_group_ = allocatenewgroup();
    num_records_in_current_group_ = 0;
  }
  auto& new_record = current_group_[num_records_in_current_group_++];
  new_record.hash = hash;
  new_record.offset = offset;
  new_record.next = nullptr;
}

void plaintableindexbuilder::addkeyprefix(slice key_prefix_slice,
                                          uint64_t key_offset) {
  if (is_first_record_ || prev_key_prefix_ != key_prefix_slice.tostring()) {
    ++num_prefixes_;
    if (!is_first_record_) {
      keys_per_prefix_hist_.add(num_keys_per_prefix_);
    }
    num_keys_per_prefix_ = 0;
    prev_key_prefix_ = key_prefix_slice.tostring();
    prev_key_prefix_hash_ = getslicehash(key_prefix_slice);
    due_index_ = true;
  }

  if (due_index_) {
    // add an index key for every kindexintervalforsameprefixkeys keys
    record_list_.addrecord(prev_key_prefix_hash_, key_offset);
    due_index_ = false;
  }

  num_keys_per_prefix_++;
  if (index_sparseness_ == 0 || num_keys_per_prefix_ % index_sparseness_ == 0) {
    due_index_ = true;
  }
  is_first_record_ = false;
}

slice plaintableindexbuilder::finish() {
  allocateindex();
  std::vector<indexrecord*> hash_to_offsets(index_size_, nullptr);
  std::vector<uint32_t> entries_per_bucket(index_size_, 0);
  bucketizeindexes(&hash_to_offsets, &entries_per_bucket);

  keys_per_prefix_hist_.add(num_keys_per_prefix_);
  log(options_.info_log, "number of keys per prefix histogram: %s",
      keys_per_prefix_hist_.tostring().c_str());

  // from the temp data structure, populate indexes.
  return fillindexes(hash_to_offsets, entries_per_bucket);
}

void plaintableindexbuilder::allocateindex() {
  if (prefix_extractor_ == nullptr || hash_table_ratio_ <= 0) {
    // fall back to pure binary search if the user fails to specify a prefix
    // extractor.
    index_size_ = 1;
  } else {
    double hash_table_size_multipier = 1.0 / hash_table_ratio_;
    index_size_ = num_prefixes_ * hash_table_size_multipier + 1;
    assert(index_size_ > 0);
  }
}

void plaintableindexbuilder::bucketizeindexes(
    std::vector<indexrecord*>* hash_to_offsets,
    std::vector<uint32_t>* entries_per_bucket) {
  bool first = true;
  uint32_t prev_hash = 0;
  size_t num_records = record_list_.getnumrecords();
  for (size_t i = 0; i < num_records; i++) {
    indexrecord* index_record = record_list_.at(i);
    uint32_t cur_hash = index_record->hash;
    if (first || prev_hash != cur_hash) {
      prev_hash = cur_hash;
      first = false;
    }
    uint32_t bucket = getbucketidfromhash(cur_hash, index_size_);
    indexrecord* prev_bucket_head = (*hash_to_offsets)[bucket];
    index_record->next = prev_bucket_head;
    (*hash_to_offsets)[bucket] = index_record;
    (*entries_per_bucket)[bucket]++;
  }

  sub_index_size_ = 0;
  for (auto entry_count : *entries_per_bucket) {
    if (entry_count <= 1) {
      continue;
    }
    // only buckets with more than 1 entry will have subindex.
    sub_index_size_ += varintlength(entry_count);
    // total bytes needed to store these entries' in-file offsets.
    sub_index_size_ += entry_count * plaintableindex::koffsetlen;
  }
}

slice plaintableindexbuilder::fillindexes(
    const std::vector<indexrecord*>& hash_to_offsets,
    const std::vector<uint32_t>& entries_per_bucket) {
  log(options_.info_log, "reserving %zu bytes for plain table's sub_index",
      sub_index_size_);
  auto total_allocate_size = gettotalsize();
  char* allocated = arena_->allocatealigned(
      total_allocate_size, huge_page_tlb_size_, options_.info_log.get());

  auto temp_ptr = encodevarint32(allocated, index_size_);
  uint32_t* index =
      reinterpret_cast<uint32_t*>(encodevarint32(temp_ptr, num_prefixes_));
  char* sub_index = reinterpret_cast<char*>(index + index_size_);

  size_t sub_index_offset = 0;
  for (uint32_t i = 0; i < index_size_; i++) {
    uint32_t num_keys_for_bucket = entries_per_bucket[i];
    switch (num_keys_for_bucket) {
      case 0:
        // no key for bucket
        index[i] = plaintableindex::kmaxfilesize;
        break;
      case 1:
        // point directly to the file offset
        index[i] = hash_to_offsets[i]->offset;
        break;
      default:
        // point to second level indexes.
        index[i] = sub_index_offset | plaintableindex::ksubindexmask;
        char* prev_ptr = &sub_index[sub_index_offset];
        char* cur_ptr = encodevarint32(prev_ptr, num_keys_for_bucket);
        sub_index_offset += (cur_ptr - prev_ptr);
        char* sub_index_pos = &sub_index[sub_index_offset];
        indexrecord* record = hash_to_offsets[i];
        int j;
        for (j = num_keys_for_bucket - 1; j >= 0 && record;
             j--, record = record->next) {
          encodefixed32(sub_index_pos + j * sizeof(uint32_t), record->offset);
        }
        assert(j == -1 && record == nullptr);
        sub_index_offset += plaintableindex::koffsetlen * num_keys_for_bucket;
        assert(sub_index_offset <= sub_index_size_);
        break;
    }
  }
  assert(sub_index_offset == sub_index_size_);

  log(options_.info_log, "hash table size: %d, suffix_map length %zu",
      index_size_, sub_index_size_);
  return slice(allocated, gettotalsize());
}

const std::string plaintableindexbuilder::kplaintableindexblock =
    "plaintableindexblock";
};  // namespace rocksdb
