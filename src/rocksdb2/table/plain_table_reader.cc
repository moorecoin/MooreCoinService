// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef rocksdb_lite

#include "table/plain_table_reader.h"

#include <string>
#include <vector>

#include "db/dbformat.h"

#include "rocksdb/cache.h"
#include "rocksdb/comparator.h"
#include "rocksdb/env.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/options.h"
#include "rocksdb/statistics.h"

#include "table/block.h"
#include "table/bloom_block.h"
#include "table/filter_block.h"
#include "table/format.h"
#include "table/meta_blocks.h"
#include "table/two_level_iterator.h"
#include "table/plain_table_factory.h"
#include "table/plain_table_key_coding.h"

#include "util/arena.h"
#include "util/coding.h"
#include "util/dynamic_bloom.h"
#include "util/hash.h"
#include "util/histogram.h"
#include "util/murmurhash.h"
#include "util/perf_context_imp.h"
#include "util/stop_watch.h"


namespace rocksdb {

namespace {

// safely getting a uint32_t element from a char array, where, starting from
// `base`, every 4 bytes are considered as an fixed 32 bit integer.
inline uint32_t getfixed32element(const char* base, size_t offset) {
  return decodefixed32(base + offset * sizeof(uint32_t));
}
}  // namespace

// iterator to iterate indexedtable
class plaintableiterator : public iterator {
 public:
  explicit plaintableiterator(plaintablereader* table, bool use_prefix_seek);
  ~plaintableiterator();

  bool valid() const;

  void seektofirst();

  void seektolast();

  void seek(const slice& target);

  void next();

  void prev();

  slice key() const;

  slice value() const;

  status status() const;

 private:
  plaintablereader* table_;
  plaintablekeydecoder decoder_;
  bool use_prefix_seek_;
  uint32_t offset_;
  uint32_t next_offset_;
  slice key_;
  slice value_;
  status status_;
  // no copying allowed
  plaintableiterator(const plaintableiterator&) = delete;
  void operator=(const iterator&) = delete;
};

extern const uint64_t kplaintablemagicnumber;
plaintablereader::plaintablereader(const options& options,
                                   unique_ptr<randomaccessfile>&& file,
                                   const envoptions& storage_options,
                                   const internalkeycomparator& icomparator,
                                   encodingtype encoding_type,
                                   uint64_t file_size,
                                   const tableproperties* table_properties)
    : internal_comparator_(icomparator),
      encoding_type_(encoding_type),
      full_scan_mode_(false),
      data_end_offset_(table_properties->data_size),
      user_key_len_(table_properties->fixed_key_len),
      prefix_extractor_(options.prefix_extractor.get()),
      enable_bloom_(false),
      bloom_(6, nullptr),
      options_(options),
      file_(std::move(file)),
      file_size_(file_size),
      table_properties_(nullptr) {}

plaintablereader::~plaintablereader() {
}

status plaintablereader::open(const options& options,
                              const envoptions& soptions,
                              const internalkeycomparator& internal_comparator,
                              unique_ptr<randomaccessfile>&& file,
                              uint64_t file_size,
                              unique_ptr<tablereader>* table_reader,
                              const int bloom_bits_per_key,
                              double hash_table_ratio, size_t index_sparseness,
                              size_t huge_page_tlb_size, bool full_scan_mode) {
  assert(options.allow_mmap_reads);
  if (file_size > plaintableindex::kmaxfilesize) {
    return status::notsupported("file is too large for plaintablereader!");
  }

  tableproperties* props = nullptr;
  auto s = readtableproperties(file.get(), file_size, kplaintablemagicnumber,
                               options.env, options.info_log.get(), &props);
  if (!s.ok()) {
    return s;
  }

  assert(hash_table_ratio >= 0.0);
  auto& user_props = props->user_collected_properties;
  auto prefix_extractor_in_file =
      user_props.find(plaintablepropertynames::kprefixextractorname);

  if (!full_scan_mode && prefix_extractor_in_file != user_props.end()) {
    if (!options.prefix_extractor) {
      return status::invalidargument(
          "prefix extractor is missing when opening a plaintable built "
          "using a prefix extractor");
    } else if (prefix_extractor_in_file->second.compare(
                   options.prefix_extractor->name()) != 0) {
      return status::invalidargument(
          "prefix extractor given doesn't match the one used to build "
          "plaintable");
    }
  }

  encodingtype encoding_type = kplain;
  auto encoding_type_prop =
      user_props.find(plaintablepropertynames::kencodingtype);
  if (encoding_type_prop != user_props.end()) {
    encoding_type = static_cast<encodingtype>(
        decodefixed32(encoding_type_prop->second.c_str()));
  }

  std::unique_ptr<plaintablereader> new_reader(new plaintablereader(
      options, std::move(file), soptions, internal_comparator, encoding_type,
      file_size, props));

  s = new_reader->mmapdatafile();
  if (!s.ok()) {
    return s;
  }

  if (!full_scan_mode) {
    s = new_reader->populateindex(props, bloom_bits_per_key, hash_table_ratio,
                                  index_sparseness, huge_page_tlb_size);
    if (!s.ok()) {
      return s;
    }
  } else {
    // flag to indicate it is a full scan mode so that none of the indexes
    // can be used.
    new_reader->full_scan_mode_ = true;
  }

  *table_reader = std::move(new_reader);
  return s;
}

void plaintablereader::setupforcompaction() {
}

iterator* plaintablereader::newiterator(const readoptions& options,
                                        arena* arena) {
  if (options.total_order_seek && !istotalordermode()) {
    return newerroriterator(
        status::invalidargument("total_order_seek not supported"), arena);
  }
  if (arena == nullptr) {
    return new plaintableiterator(this, prefix_extractor_ != nullptr);
  } else {
    auto mem = arena->allocatealigned(sizeof(plaintableiterator));
    return new (mem) plaintableiterator(this, prefix_extractor_ != nullptr);
  }
}

status plaintablereader::populateindexrecordlist(
    plaintableindexbuilder* index_builder, vector<uint32_t>* prefix_hashes) {
  slice prev_key_prefix_slice;
  uint32_t pos = data_start_offset_;

  bool is_first_record = true;
  slice key_prefix_slice;
  plaintablekeydecoder decoder(encoding_type_, user_key_len_,
                               options_.prefix_extractor.get());
  while (pos < data_end_offset_) {
    uint32_t key_offset = pos;
    parsedinternalkey key;
    slice value_slice;
    bool seekable = false;
    status s = next(&decoder, &pos, &key, nullptr, &value_slice, &seekable);
    if (!s.ok()) {
      return s;
    }

    key_prefix_slice = getprefix(key);
    if (enable_bloom_) {
      bloom_.addhash(getslicehash(key.user_key));
    } else {
      if (is_first_record || prev_key_prefix_slice != key_prefix_slice) {
        if (!is_first_record) {
          prefix_hashes->push_back(getslicehash(prev_key_prefix_slice));
        }
        prev_key_prefix_slice = key_prefix_slice;
      }
    }

    index_builder->addkeyprefix(getprefix(key), key_offset);

    if (!seekable && is_first_record) {
      return status::corruption("key for a prefix is not seekable");
    }

    is_first_record = false;
  }

  prefix_hashes->push_back(getslicehash(key_prefix_slice));
  auto s = index_.initfromrawdata(index_builder->finish());
  return s;
}

void plaintablereader::allocateandfillbloom(int bloom_bits_per_key,
                                            int num_prefixes,
                                            size_t huge_page_tlb_size,
                                            vector<uint32_t>* prefix_hashes) {
  if (!istotalordermode()) {
    uint32_t bloom_total_bits = num_prefixes * bloom_bits_per_key;
    if (bloom_total_bits > 0) {
      enable_bloom_ = true;
      bloom_.settotalbits(&arena_, bloom_total_bits, options_.bloom_locality,
                          huge_page_tlb_size, options_.info_log.get());
      fillbloom(prefix_hashes);
    }
  }
}

void plaintablereader::fillbloom(vector<uint32_t>* prefix_hashes) {
  assert(bloom_.isinitialized());
  for (auto prefix_hash : *prefix_hashes) {
    bloom_.addhash(prefix_hash);
  }
}

status plaintablereader::mmapdatafile() {
  // get mmapped memory to file_data_.
  return file_->read(0, file_size_, &file_data_, nullptr);
}

status plaintablereader::populateindex(tableproperties* props,
                                       int bloom_bits_per_key,
                                       double hash_table_ratio,
                                       size_t index_sparseness,
                                       size_t huge_page_tlb_size) {
  assert(props != nullptr);
  table_properties_.reset(props);

  blockcontents bloom_block_contents;
  auto s = readmetablock(file_.get(), file_size_, kplaintablemagicnumber,
                         options_.env, bloomblockbuilder::kbloomblock,
                         &bloom_block_contents);
  bool index_in_file = s.ok();

  blockcontents index_block_contents;
  s = readmetablock(file_.get(), file_size_, kplaintablemagicnumber,
                    options_.env, plaintableindexbuilder::kplaintableindexblock,
                    &index_block_contents);

  index_in_file &= s.ok();

  slice* bloom_block;
  if (index_in_file) {
    bloom_block = &bloom_block_contents.data;
  } else {
    bloom_block = nullptr;
  }

  // index_in_file == true only if there are kbloomblock and
  // kplaintableindexblock
  // in file

  slice* index_block;
  if (index_in_file) {
    index_block = &index_block_contents.data;
  } else {
    index_block = nullptr;
  }

  if ((options_.prefix_extractor.get() == nullptr) && (hash_table_ratio != 0)) {
  // options.prefix_extractor is requried for a hash-based look-up.
    return status::notsupported(
        "plaintable requires a prefix extractor enable prefix hash mode.");
  }

  // first, read the whole file, for every kindexintervalforsameprefixkeys rows
  // for a prefix (starting from the first one), generate a record of (hash,
  // offset) and append it to indexrecordlist, which is a data structure created
  // to store them.

  if (!index_in_file) {
    // allocate bloom filter here for total order mode.
    if (istotalordermode()) {
      uint32_t num_bloom_bits =
          table_properties_->num_entries * bloom_bits_per_key;
      if (num_bloom_bits > 0) {
        enable_bloom_ = true;
        bloom_.settotalbits(&arena_, num_bloom_bits, options_.bloom_locality,
                            huge_page_tlb_size, options_.info_log.get());
      }
    }
  } else {
    enable_bloom_ = true;
    auto num_blocks_property = props->user_collected_properties.find(
        plaintablepropertynames::knumbloomblocks);

    uint32_t num_blocks = 0;
    if (num_blocks_property != props->user_collected_properties.end()) {
      slice temp_slice(num_blocks_property->second);
      if (!getvarint32(&temp_slice, &num_blocks)) {
        num_blocks = 0;
      }
    }
    // cast away const qualifier, because bloom_ won't be changed
    bloom_.setrawdata(
        const_cast<unsigned char*>(
            reinterpret_cast<const unsigned char*>(bloom_block->data())),
        bloom_block->size() * 8, num_blocks);
  }

  plaintableindexbuilder index_builder(&arena_, options_, index_sparseness,
                                       hash_table_ratio, huge_page_tlb_size);

  std::vector<uint32_t> prefix_hashes;
  if (!index_in_file) {
    status s = populateindexrecordlist(&index_builder, &prefix_hashes);
    if (!s.ok()) {
      return s;
    }
  } else {
    status s = index_.initfromrawdata(*index_block);
    if (!s.ok()) {
      return s;
    }
  }

  if (!index_in_file) {
    // calculated bloom filter size and allocate memory for
    // bloom filter based on the number of prefixes, then fill it.
    allocateandfillbloom(bloom_bits_per_key, index_.getnumprefixes(),
                         huge_page_tlb_size, &prefix_hashes);
  }

  // fill two table properties.
  if (!index_in_file) {
    props->user_collected_properties["plain_table_hash_table_size"] =
        std::to_string(index_.getindexsize() * plaintableindex::koffsetlen);
    props->user_collected_properties["plain_table_sub_index_size"] =
        std::to_string(index_.getsubindexsize());
  } else {
    props->user_collected_properties["plain_table_hash_table_size"] =
        std::to_string(0);
    props->user_collected_properties["plain_table_sub_index_size"] =
        std::to_string(0);
  }

  return status::ok();
}

status plaintablereader::getoffset(const slice& target, const slice& prefix,
                                   uint32_t prefix_hash, bool& prefix_matched,
                                   uint32_t* offset) const {
  prefix_matched = false;
  uint32_t prefix_index_offset;
  auto res = index_.getoffset(prefix_hash, &prefix_index_offset);
  if (res == plaintableindex::knoprefixforbucket) {
    *offset = data_end_offset_;
    return status::ok();
  } else if (res == plaintableindex::kdirecttofile) {
    *offset = prefix_index_offset;
    return status::ok();
  }

  // point to sub-index, need to do a binary search
  uint32_t upper_bound;
  const char* base_ptr =
      index_.getsubindexbaseptrandupperbound(prefix_index_offset, &upper_bound);
  uint32_t low = 0;
  uint32_t high = upper_bound;
  parsedinternalkey mid_key;
  parsedinternalkey parsed_target;
  if (!parseinternalkey(target, &parsed_target)) {
    return status::corruption(slice());
  }

  // the key is between [low, high). do a binary search between it.
  while (high - low > 1) {
    uint32_t mid = (high + low) / 2;
    uint32_t file_offset = getfixed32element(base_ptr, mid);
    size_t tmp;
    status s = plaintablekeydecoder(encoding_type_, user_key_len_,
                                    options_.prefix_extractor.get())
                   .nextkey(file_data_.data() + file_offset,
                            file_data_.data() + data_end_offset_, &mid_key,
                            nullptr, &tmp);
    if (!s.ok()) {
      return s;
    }
    int cmp_result = internal_comparator_.compare(mid_key, parsed_target);
    if (cmp_result < 0) {
      low = mid;
    } else {
      if (cmp_result == 0) {
        // happen to have found the exact key or target is smaller than the
        // first key after base_offset.
        prefix_matched = true;
        *offset = file_offset;
        return status::ok();
      } else {
        high = mid;
      }
    }
  }
  // both of the key at the position low or low+1 could share the same
  // prefix as target. we need to rule out one of them to avoid to go
  // to the wrong prefix.
  parsedinternalkey low_key;
  size_t tmp;
  uint32_t low_key_offset = getfixed32element(base_ptr, low);
  status s = plaintablekeydecoder(encoding_type_, user_key_len_,
                                  options_.prefix_extractor.get())
                 .nextkey(file_data_.data() + low_key_offset,
                          file_data_.data() + data_end_offset_, &low_key,
                          nullptr, &tmp);
  if (!s.ok()) {
    return s;
  }

  if (getprefix(low_key) == prefix) {
    prefix_matched = true;
    *offset = low_key_offset;
  } else if (low + 1 < upper_bound) {
    // there is possible a next prefix, return it
    prefix_matched = false;
    *offset = getfixed32element(base_ptr, low + 1);
  } else {
    // target is larger than a key of the last prefix in this bucket
    // but with a different prefix. key does not exist.
    *offset = data_end_offset_;
  }
  return status::ok();
}

bool plaintablereader::matchbloom(uint32_t hash) const {
  return !enable_bloom_ || bloom_.maycontainhash(hash);
}


status plaintablereader::next(plaintablekeydecoder* decoder, uint32_t* offset,
                              parsedinternalkey* parsed_key,
                              slice* internal_key, slice* value,
                              bool* seekable) const {
  if (*offset == data_end_offset_) {
    *offset = data_end_offset_;
    return status::ok();
  }

  if (*offset > data_end_offset_) {
    return status::corruption("offset is out of file size");
  }

  const char* start = file_data_.data() + *offset;
  size_t bytes_for_key;
  status s =
      decoder->nextkey(start, file_data_.data() + data_end_offset_, parsed_key,
                       internal_key, &bytes_for_key, seekable);
  if (!s.ok()) {
    return s;
  }
  uint32_t value_size;
  const char* value_ptr = getvarint32ptr(
      start + bytes_for_key, file_data_.data() + data_end_offset_, &value_size);
  if (value_ptr == nullptr) {
    return status::corruption(
        "unexpected eof when reading the next value's size.");
  }
  *offset = *offset + (value_ptr - start) + value_size;
  if (*offset > data_end_offset_) {
    return status::corruption("unexpected eof when reading the next value. ");
  }
  *value = slice(value_ptr, value_size);

  return status::ok();
}

void plaintablereader::prepare(const slice& target) {
  if (enable_bloom_) {
    uint32_t prefix_hash = getslicehash(getprefix(target));
    bloom_.prefetch(prefix_hash);
  }
}

status plaintablereader::get(const readoptions& ro, const slice& target,
                             void* arg,
                             bool (*saver)(void*, const parsedinternalkey&,
                                           const slice&),
                             void (*mark_key_may_exist)(void*)) {
  // check bloom filter first.
  slice prefix_slice;
  uint32_t prefix_hash;
  if (istotalordermode()) {
    if (full_scan_mode_) {
      status_ =
          status::invalidargument("get() is not allowed in full scan mode.");
    }
    // match whole user key for bloom filter check.
    if (!matchbloom(getslicehash(getuserkey(target)))) {
      return status::ok();
    }
    // in total order mode, there is only one bucket 0, and we always use empty
    // prefix.
    prefix_slice = slice();
    prefix_hash = 0;
  } else {
    prefix_slice = getprefix(target);
    prefix_hash = getslicehash(prefix_slice);
    if (!matchbloom(prefix_hash)) {
      return status::ok();
    }
  }
  uint32_t offset;
  bool prefix_match;
  status s =
      getoffset(target, prefix_slice, prefix_hash, prefix_match, &offset);
  if (!s.ok()) {
    return s;
  }
  parsedinternalkey found_key;
  parsedinternalkey parsed_target;
  if (!parseinternalkey(target, &parsed_target)) {
    return status::corruption(slice());
  }
  slice found_value;
  plaintablekeydecoder decoder(encoding_type_, user_key_len_,
                               options_.prefix_extractor.get());
  while (offset < data_end_offset_) {
    status s = next(&decoder, &offset, &found_key, nullptr, &found_value);
    if (!s.ok()) {
      return s;
    }
    if (!prefix_match) {
      // need to verify prefix for the first key found if it is not yet
      // checked.
      if (getprefix(found_key) != prefix_slice) {
        return status::ok();
      }
      prefix_match = true;
    }
    if (internal_comparator_.compare(found_key, parsed_target) >= 0) {
      if (!(*saver)(arg, found_key, found_value)) {
        break;
      }
    }
  }
  return status::ok();
}

uint64_t plaintablereader::approximateoffsetof(const slice& key) {
  return 0;
}

plaintableiterator::plaintableiterator(plaintablereader* table,
                                       bool use_prefix_seek)
    : table_(table),
      decoder_(table_->encoding_type_, table_->user_key_len_,
               table_->prefix_extractor_),
      use_prefix_seek_(use_prefix_seek) {
  next_offset_ = offset_ = table_->data_end_offset_;
}

plaintableiterator::~plaintableiterator() {
}

bool plaintableiterator::valid() const {
  return offset_ < table_->data_end_offset_
      && offset_ >= table_->data_start_offset_;
}

void plaintableiterator::seektofirst() {
  next_offset_ = table_->data_start_offset_;
  if (next_offset_ >= table_->data_end_offset_) {
    next_offset_ = offset_ = table_->data_end_offset_;
  } else {
    next();
  }
}

void plaintableiterator::seektolast() {
  assert(false);
  status_ = status::notsupported("seektolast() is not supported in plaintable");
}

void plaintableiterator::seek(const slice& target) {
  // if the user doesn't set prefix seek option and we are not able to do a
  // total seek(). assert failure.
  if (!use_prefix_seek_) {
    if (table_->full_scan_mode_) {
      status_ =
          status::invalidargument("seek() is not allowed in full scan mode.");
      offset_ = next_offset_ = table_->data_end_offset_;
      return;
    } else if (table_->getindexsize() > 1) {
      assert(false);
      status_ = status::notsupported(
          "plaintable cannot issue non-prefix seek unless in total order "
          "mode.");
      offset_ = next_offset_ = table_->data_end_offset_;
      return;
    }
  }

  slice prefix_slice = table_->getprefix(target);
  uint32_t prefix_hash = 0;
  // bloom filter is ignored in total-order mode.
  if (!table_->istotalordermode()) {
    prefix_hash = getslicehash(prefix_slice);
    if (!table_->matchbloom(prefix_hash)) {
      offset_ = next_offset_ = table_->data_end_offset_;
      return;
    }
  }
  bool prefix_match;
  status_ = table_->getoffset(target, prefix_slice, prefix_hash, prefix_match,
                              &next_offset_);
  if (!status_.ok()) {
    offset_ = next_offset_ = table_->data_end_offset_;
    return;
  }

  if (next_offset_ < table_-> data_end_offset_) {
    for (next(); status_.ok() && valid(); next()) {
      if (!prefix_match) {
        // need to verify the first key's prefix
        if (table_->getprefix(key()) != prefix_slice) {
          offset_ = next_offset_ = table_->data_end_offset_;
          break;
        }
        prefix_match = true;
      }
      if (table_->internal_comparator_.compare(key(), target) >= 0) {
        break;
      }
    }
  } else {
    offset_ = table_->data_end_offset_;
  }
}

void plaintableiterator::next() {
  offset_ = next_offset_;
  if (offset_ < table_->data_end_offset_) {
    slice tmp_slice;
    parsedinternalkey parsed_key;
    status_ =
        table_->next(&decoder_, &next_offset_, &parsed_key, &key_, &value_);
    if (!status_.ok()) {
      offset_ = next_offset_ = table_->data_end_offset_;
    }
  }
}

void plaintableiterator::prev() {
  assert(false);
}

slice plaintableiterator::key() const {
  assert(valid());
  return key_;
}

slice plaintableiterator::value() const {
  assert(valid());
  return value_;
}

status plaintableiterator::status() const {
  return status_;
}

}  // namespace rocksdb
#endif  // rocksdb_lite
