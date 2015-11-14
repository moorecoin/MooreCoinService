// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once

#ifndef rocksdb_lite
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include <stdint.h>

#include "db/dbformat.h"
#include "rocksdb/env.h"
#include "rocksdb/iterator.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/table.h"
#include "rocksdb/table_properties.h"
#include "table/table_reader.h"
#include "table/plain_table_factory.h"
#include "table/plain_table_index.h"
#include "util/arena.h"
#include "util/dynamic_bloom.h"

namespace rocksdb {

class block;
struct blockcontents;
class blockhandle;
class footer;
struct options;
class randomaccessfile;
struct readoptions;
class tablecache;
class tablereader;
class internalkeycomparator;
class plaintablekeydecoder;

using std::unique_ptr;
using std::unordered_map;
using std::vector;
extern const uint32_t kplaintablevariablelength;

// based on following output file format shown in plain_table_factory.h
// when opening the output file, indexedtablereader creates a hash table
// from key prefixes to offset of the output file. indexedtable will decide
// whether it points to the data offset of the first key with the key prefix
// or the offset of it. if there are too many keys share this prefix, it will
// create a binary search-able index from the suffix to offset on disk.
//
// the implementation of indexedtablereader requires output file is mmaped
class plaintablereader: public tablereader {
 public:
  static status open(const options& options, const envoptions& soptions,
                     const internalkeycomparator& internal_comparator,
                     unique_ptr<randomaccessfile>&& file, uint64_t file_size,
                     unique_ptr<tablereader>* table,
                     const int bloom_bits_per_key, double hash_table_ratio,
                     size_t index_sparseness, size_t huge_page_tlb_size,
                     bool full_scan_mode);

  iterator* newiterator(const readoptions&, arena* arena = nullptr) override;

  void prepare(const slice& target);

  status get(const readoptions&, const slice& key, void* arg,
             bool (*result_handler)(void* arg, const parsedinternalkey& k,
                                    const slice& v),
             void (*mark_key_may_exist)(void*) = nullptr);

  uint64_t approximateoffsetof(const slice& key);

  uint32_t getindexsize() const { return index_.getindexsize(); }
  void setupforcompaction();

  std::shared_ptr<const tableproperties> gettableproperties() const {
    return table_properties_;
  }

  virtual size_t approximatememoryusage() const override {
    return arena_.memoryallocatedbytes();
  }

  plaintablereader(const options& options, unique_ptr<randomaccessfile>&& file,
                   const envoptions& storage_options,
                   const internalkeycomparator& internal_comparator,
                   encodingtype encoding_type, uint64_t file_size,
                   const tableproperties* table_properties);
  virtual ~plaintablereader();

 protected:
  // check bloom filter to see whether it might contain this prefix.
  // the hash of the prefix is given, since it can be reused for index lookup
  // too.
  virtual bool matchbloom(uint32_t hash) const;

  // populateindex() builds index of keys. it must be called before any query
  // to the table.
  //
  // props: the table properties object that need to be stored. ownership of
  //        the object will be passed.
  //

  status populateindex(tableproperties* props, int bloom_bits_per_key,
                       double hash_table_ratio, size_t index_sparseness,
                       size_t huge_page_tlb_size);

  status mmapdatafile();

 private:
  const internalkeycomparator internal_comparator_;
  encodingtype encoding_type_;
  // represents plain table's current status.
  status status_;
  slice file_data_;

  plaintableindex index_;
  bool full_scan_mode_;

  // data_start_offset_ and data_end_offset_ defines the range of the
  // sst file that stores data.
  const uint32_t data_start_offset_ = 0;
  const uint32_t data_end_offset_;
  const size_t user_key_len_;
  const slicetransform* prefix_extractor_;

  static const size_t knuminternalbytes = 8;

  // bloom filter is used to rule out non-existent key
  bool enable_bloom_;
  dynamicbloom bloom_;
  arena arena_;

  const options& options_;
  unique_ptr<randomaccessfile> file_;
  uint32_t file_size_;
  std::shared_ptr<const tableproperties> table_properties_;

  bool isfixedlength() const {
    return user_key_len_ != kplaintablevariablelength;
  }

  size_t getfixedinternalkeylength() const {
    return user_key_len_ + knuminternalbytes;
  }

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

  friend class tablecache;
  friend class plaintableiterator;

  // internal helper function to generate an indexrecordlist object from all
  // the rows, which contains index records as a list.
  // if bloom_ is not null, all the keys' full-key hash will be added to the
  // bloom filter.
  status populateindexrecordlist(plaintableindexbuilder* index_builder,
                                 vector<uint32_t>* prefix_hashes);

  // internal helper function to allocate memory for bloom filter and fill it
  void allocateandfillbloom(int bloom_bits_per_key, int num_prefixes,
                            size_t huge_page_tlb_size,
                            vector<uint32_t>* prefix_hashes);

  void fillbloom(vector<uint32_t>* prefix_hashes);

  // read the key and value at `offset` to parameters for keys, the and
  // `seekable`.
  // on success, `offset` will be updated as the offset for the next key.
  // `parsed_key` will be key in parsed format.
  // if `internal_key` is not empty, it will be filled with key with slice
  // format.
  // if `seekable` is not null, it will return whether we can directly read
  // data using this offset.
  status next(plaintablekeydecoder* decoder, uint32_t* offset,
              parsedinternalkey* parsed_key, slice* internal_key, slice* value,
              bool* seekable = nullptr) const;
  // get file offset for key target.
  // return value prefix_matched is set to true if the offset is confirmed
  // for a key with the same prefix as target.
  status getoffset(const slice& target, const slice& prefix,
                   uint32_t prefix_hash, bool& prefix_matched,
                   uint32_t* offset) const;

  bool istotalordermode() const { return (prefix_extractor_ == nullptr); }

  // no copying allowed
  explicit plaintablereader(const tablereader&) = delete;
  void operator=(const tablereader&) = delete;
};
}  // namespace rocksdb
#endif  // rocksdb_lite
