//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef rocksdb_lite
#include "table/cuckoo_table_reader.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include "rocksdb/iterator.h"
#include "table/meta_blocks.h"
#include "table/cuckoo_table_factory.h"
#include "util/arena.h"
#include "util/coding.h"

namespace rocksdb {
namespace {
  static const uint64_t cache_line_mask = ~((uint64_t)cache_line_size - 1);
}

extern const uint64_t kcuckootablemagicnumber;

cuckootablereader::cuckootablereader(
    const options& options,
    std::unique_ptr<randomaccessfile>&& file,
    uint64_t file_size,
    const comparator* comparator,
    uint64_t (*get_slice_hash)(const slice&, uint32_t, uint64_t))
    : file_(std::move(file)),
      ucomp_(comparator),
      get_slice_hash_(get_slice_hash) {
  if (!options.allow_mmap_reads) {
    status_ = status::invalidargument("file is not mmaped");
  }
  tableproperties* props = nullptr;
  status_ = readtableproperties(file_.get(), file_size, kcuckootablemagicnumber,
      options.env, options.info_log.get(), &props);
  if (!status_.ok()) {
    return;
  }
  table_props_.reset(props);
  auto& user_props = props->user_collected_properties;
  auto hash_funs = user_props.find(cuckootablepropertynames::knumhashfunc);
  if (hash_funs == user_props.end()) {
    status_ = status::invalidargument("number of hash functions not found");
    return;
  }
  num_hash_func_ = *reinterpret_cast<const uint32_t*>(hash_funs->second.data());
  auto unused_key = user_props.find(cuckootablepropertynames::kemptykey);
  if (unused_key == user_props.end()) {
    status_ = status::invalidargument("empty bucket value not found");
    return;
  }
  unused_key_ = unused_key->second;

  key_length_ = props->fixed_key_len;
  auto value_length = user_props.find(cuckootablepropertynames::kvaluelength);
  if (value_length == user_props.end()) {
    status_ = status::invalidargument("value length not found");
    return;
  }
  value_length_ = *reinterpret_cast<const uint32_t*>(
      value_length->second.data());
  bucket_length_ = key_length_ + value_length_;

  auto hash_table_size = user_props.find(
      cuckootablepropertynames::khashtablesize);
  if (hash_table_size == user_props.end()) {
    status_ = status::invalidargument("hash table size not found");
    return;
  }
  table_size_minus_one_ = *reinterpret_cast<const uint64_t*>(
      hash_table_size->second.data()) - 1;
  auto is_last_level = user_props.find(cuckootablepropertynames::kislastlevel);
  if (is_last_level == user_props.end()) {
    status_ = status::invalidargument("is last level not found");
    return;
  }
  is_last_level_ = *reinterpret_cast<const bool*>(is_last_level->second.data());
  auto cuckoo_block_size = user_props.find(
      cuckootablepropertynames::kcuckooblocksize);
  if (cuckoo_block_size == user_props.end()) {
    status_ = status::invalidargument("cuckoo block size not found");
    return;
  }
  cuckoo_block_size_ = *reinterpret_cast<const uint32_t*>(
      cuckoo_block_size->second.data());
  cuckoo_block_bytes_minus_one_ = cuckoo_block_size_ * bucket_length_ - 1;
  status_ = file_->read(0, file_size, &file_data_, nullptr);
}

status cuckootablereader::get(
    const readoptions& readoptions, const slice& key, void* handle_context,
    bool (*result_handler)(void* arg, const parsedinternalkey& k,
                           const slice& v),
    void (*mark_key_may_exist_handler)(void* handle_context)) {
  assert(key.size() == key_length_ + (is_last_level_ ? 8 : 0));
  slice user_key = extractuserkey(key);
  for (uint32_t hash_cnt = 0; hash_cnt < num_hash_func_; ++hash_cnt) {
    uint64_t offset = bucket_length_ * cuckoohash(
        user_key, hash_cnt, table_size_minus_one_, get_slice_hash_);
    const char* bucket = &file_data_.data()[offset];
    for (uint32_t block_idx = 0; block_idx < cuckoo_block_size_;
        ++block_idx, bucket += bucket_length_) {
      if (ucomp_->compare(slice(unused_key_.data(), user_key.size()),
            slice(bucket, user_key.size())) == 0) {
        return status::ok();
      }
      // here, we compare only the user key part as we support only one entry
      // per user key and we don't support sanpshot.
      if (ucomp_->compare(user_key, slice(bucket, user_key.size())) == 0) {
        slice value = slice(&bucket[key_length_], value_length_);
        if (is_last_level_) {
          parsedinternalkey found_ikey(
              slice(bucket, key_length_), 0, ktypevalue);
          result_handler(handle_context, found_ikey, value);
        } else {
          slice full_key(bucket, key_length_);
          parsedinternalkey found_ikey;
          parseinternalkey(full_key, &found_ikey);
          result_handler(handle_context, found_ikey, value);
        }
        // we don't support merge operations. so, we return here.
        return status::ok();
      }
    }
  }
  return status::ok();
}

void cuckootablereader::prepare(const slice& key) {
  // prefetch the first cuckoo block.
  slice user_key = extractuserkey(key);
  uint64_t addr = reinterpret_cast<uint64_t>(file_data_.data()) +
    bucket_length_ * cuckoohash(user_key, 0, table_size_minus_one_, nullptr);
  uint64_t end_addr = addr + cuckoo_block_bytes_minus_one_;
  for (addr &= cache_line_mask; addr < end_addr; addr += cache_line_size) {
    prefetch(reinterpret_cast<const char*>(addr), 0, 3);
  }
}

class cuckootableiterator : public iterator {
 public:
  explicit cuckootableiterator(cuckootablereader* reader);
  ~cuckootableiterator() {}
  bool valid() const override;
  void seektofirst() override;
  void seektolast() override;
  void seek(const slice& target) override;
  void next() override;
  void prev() override;
  slice key() const override;
  slice value() const override;
  status status() const override { return status_; }
  void loadkeysfromreader();

 private:
  struct comparekeys {
    comparekeys(const comparator* ucomp, const bool last_level)
      : ucomp_(ucomp),
        is_last_level_(last_level) {}
    bool operator()(const std::pair<slice, uint32_t>& first,
        const std::pair<slice, uint32_t>& second) const {
      if (is_last_level_) {
        return ucomp_->compare(first.first, second.first) < 0;
      } else {
        return ucomp_->compare(extractuserkey(first.first),
            extractuserkey(second.first)) < 0;
      }
    }

   private:
    const comparator* ucomp_;
    const bool is_last_level_;
  };
  const comparekeys comparator_;
  void preparekvatcurridx();
  cuckootablereader* reader_;
  status status_;
  // contains a map of keys to bucket_id sorted in key order.
  std::vector<std::pair<slice, uint32_t>> key_to_bucket_id_;
  // we assume that the number of items can be stored in uint32 (4 billion).
  uint32_t curr_key_idx_;
  slice curr_value_;
  iterkey curr_key_;
  // no copying allowed
  cuckootableiterator(const cuckootableiterator&) = delete;
  void operator=(const iterator&) = delete;
};

cuckootableiterator::cuckootableiterator(cuckootablereader* reader)
  : comparator_(reader->ucomp_, reader->is_last_level_),
    reader_(reader),
    curr_key_idx_(std::numeric_limits<int32_t>::max()) {
  key_to_bucket_id_.clear();
  curr_value_.clear();
  curr_key_.clear();
}

void cuckootableiterator::loadkeysfromreader() {
  key_to_bucket_id_.reserve(reader_->gettableproperties()->num_entries);
  uint64_t num_buckets = reader_->table_size_minus_one_ +
    reader_->cuckoo_block_size_;
  for (uint32_t bucket_id = 0; bucket_id < num_buckets; bucket_id++) {
    slice read_key;
    status_ = reader_->file_->read(bucket_id * reader_->bucket_length_,
        reader_->key_length_, &read_key, nullptr);
    if (read_key != slice(reader_->unused_key_)) {
      key_to_bucket_id_.push_back(std::make_pair(read_key, bucket_id));
    }
  }
  assert(key_to_bucket_id_.size() ==
      reader_->gettableproperties()->num_entries);
  std::sort(key_to_bucket_id_.begin(), key_to_bucket_id_.end(), comparator_);
  curr_key_idx_ = key_to_bucket_id_.size();
}

void cuckootableiterator::seektofirst() {
  curr_key_idx_ = 0;
  preparekvatcurridx();
}

void cuckootableiterator::seektolast() {
  curr_key_idx_ = key_to_bucket_id_.size() - 1;
  preparekvatcurridx();
}

void cuckootableiterator::seek(const slice& target) {
  // we assume that the target is an internal key. if this is last level file,
  // we need to take only the user key part to seek.
  slice target_to_search = reader_->is_last_level_ ?
    extractuserkey(target) : target;
  auto seek_it = std::lower_bound(key_to_bucket_id_.begin(),
      key_to_bucket_id_.end(),
      std::make_pair(target_to_search, 0),
      comparator_);
  curr_key_idx_ = std::distance(key_to_bucket_id_.begin(), seek_it);
  preparekvatcurridx();
}

bool cuckootableiterator::valid() const {
  return curr_key_idx_ < key_to_bucket_id_.size();
}

void cuckootableiterator::preparekvatcurridx() {
  if (!valid()) {
    curr_value_.clear();
    curr_key_.clear();
    return;
  }
  uint64_t offset = ((uint64_t) key_to_bucket_id_[curr_key_idx_].second
      * reader_->bucket_length_) + reader_->key_length_;
  status_ = reader_->file_->read(offset, reader_->value_length_,
      &curr_value_, nullptr);
  if (reader_->is_last_level_) {
    // always return internal key.
    curr_key_.setinternalkey(
        key_to_bucket_id_[curr_key_idx_].first, 0, ktypevalue);
  }
}

void cuckootableiterator::next() {
  if (!valid()) {
    curr_value_.clear();
    curr_key_.clear();
    return;
  }
  ++curr_key_idx_;
  preparekvatcurridx();
}

void cuckootableiterator::prev() {
  if (curr_key_idx_ == 0) {
    curr_key_idx_ = key_to_bucket_id_.size();
  }
  if (!valid()) {
    curr_value_.clear();
    curr_key_.clear();
    return;
  }
  --curr_key_idx_;
  preparekvatcurridx();
}

slice cuckootableiterator::key() const {
  assert(valid());
  if (reader_->is_last_level_) {
    return curr_key_.getkey();
  } else {
    return key_to_bucket_id_[curr_key_idx_].first;
  }
}

slice cuckootableiterator::value() const {
  assert(valid());
  return curr_value_;
}

extern iterator* newerroriterator(const status& status, arena* arena);

iterator* cuckootablereader::newiterator(
    const readoptions& read_options, arena* arena) {
  if (!status().ok()) {
    return newerroriterator(
        status::corruption("cuckootablereader status is not okay."), arena);
  }
  if (read_options.total_order_seek) {
    return newerroriterator(
        status::invalidargument("total_order_seek is not supported."), arena);
  }
  cuckootableiterator* iter;
  if (arena == nullptr) {
    iter = new cuckootableiterator(this);
  } else {
    auto iter_mem = arena->allocatealigned(sizeof(cuckootableiterator));
    iter = new (iter_mem) cuckootableiterator(this);
  }
  if (iter->status().ok()) {
    iter->loadkeysfromreader();
  }
  return iter;
}

size_t cuckootablereader::approximatememoryusage() const { return 0; }

}  // namespace rocksdb
#endif
