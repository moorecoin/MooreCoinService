//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#pragma once

#include <string>
#include <vector>

#include "db/dbformat.h"
#include "rocksdb/options.h"
#include "util/murmurhash.h"
#include "util/hash.h"
#include "util/arena.h"
#include "util/histogram.h"

namespace rocksdb {

// plaintableindex contains buckets size of index_size_, each is a
// 32-bit integer. the lower 31 bits contain an offset value (explained below)
// and the first bit of the integer indicates type of the offset.
//
// +--------------+------------------------------------------------------+
// | flag (1 bit) | offset to binary search buffer or file (31 bits)     +
// +--------------+------------------------------------------------------+
//
// explanation for the "flag bit":
//
// 0 indicates that the bucket contains only one prefix (no conflict when
//   hashing this prefix), whose first row starts from this offset of the
// file.
// 1 indicates that the bucket contains more than one prefixes, or there
//   are too many rows for one prefix so we need a binary search for it. in
//   this case, the offset indicates the offset of sub_index_ holding the
//   binary search indexes of keys for those rows. those binary search indexes
//   are organized in this way:
//
// the first 4 bytes, indicate how many indexes (n) are stored after it. after
// it, there are n 32-bit integers, each points of an offset of the file,
// which
// points to starting of a row. those offsets need to be guaranteed to be in
// ascending order so the keys they are pointing to are also in ascending
// order
// to make sure we can use them to do binary searches. below is visual
// presentation of a bucket.
//
// <begin>
//   number_of_records:  varint32
//   record 1 file offset:  fixedint32
//   record 2 file offset:  fixedint32
//    ....
//   record n file offset:  fixedint32
// <end>
class plaintableindex {
 public:
  enum indexsearchresult {
    knoprefixforbucket = 0,
    kdirecttofile = 1,
    ksubindex = 2
  };

  explicit plaintableindex(slice data) { initfromrawdata(data); }

  plaintableindex()
      : index_size_(0),
        sub_index_size_(0),
        num_prefixes_(0),
        index_(nullptr),
        sub_index_(nullptr) {}

  indexsearchresult getoffset(uint32_t prefix_hash,
                              uint32_t* bucket_value) const;

  status initfromrawdata(slice data);

  const char* getsubindexbaseptrandupperbound(uint32_t offset,
                                              uint32_t* upper_bound) const {
    const char* index_ptr = &sub_index_[offset];
    return getvarint32ptr(index_ptr, index_ptr + 4, upper_bound);
  }

  uint32_t getindexsize() const { return index_size_; }

  uint32_t getsubindexsize() const { return sub_index_size_; }

  uint32_t getnumprefixes() const { return num_prefixes_; }

  static const uint64_t kmaxfilesize = (1u << 31) - 1;
  static const uint32_t ksubindexmask = 0x80000000;
  static const size_t koffsetlen = sizeof(uint32_t);

 private:
  uint32_t index_size_;
  size_t sub_index_size_;
  uint32_t num_prefixes_;

  uint32_t* index_;
  char* sub_index_;
};

// plaintableindexbuilder is used to create plain table index.
// after calling finish(), it returns slice, which is usually
// used either to initialize plaintableindex or
// to save index to sst file.
// for more details about the  index, please refer to:
// https://github.com/facebook/rocksdb/wiki/plaintable-format
// #wiki-in-memory-index-format
class plaintableindexbuilder {
 public:
  plaintableindexbuilder(arena* arena, const options& options,
                         uint32_t index_sparseness, double hash_table_ratio,
                         double huge_page_tlb_size)
      : arena_(arena),
        options_(options),
        record_list_(krecordspergroup),
        is_first_record_(true),
        due_index_(false),
        num_prefixes_(0),
        num_keys_per_prefix_(0),
        prev_key_prefix_hash_(0),
        index_sparseness_(index_sparseness),
        prefix_extractor_(options.prefix_extractor.get()),
        hash_table_ratio_(hash_table_ratio),
        huge_page_tlb_size_(huge_page_tlb_size) {}

  void addkeyprefix(slice key_prefix_slice, uint64_t key_offset);

  slice finish();

  uint32_t gettotalsize() const {
    return varintlength(index_size_) + varintlength(num_prefixes_) +
           plaintableindex::koffsetlen * index_size_ + sub_index_size_;
  }

  static const std::string kplaintableindexblock;

 private:
  struct indexrecord {
    uint32_t hash;    // hash of the prefix
    uint32_t offset;  // offset of a row
    indexrecord* next;
  };

  // helper class to track all the index records
  class indexrecordlist {
   public:
    explicit indexrecordlist(size_t num_records_per_group)
        : knumrecordspergroup(num_records_per_group),
          current_group_(nullptr),
          num_records_in_current_group_(num_records_per_group) {}

    ~indexrecordlist() {
      for (size_t i = 0; i < groups_.size(); i++) {
        delete[] groups_[i];
      }
    }

    void addrecord(murmur_t hash, uint32_t offset);

    size_t getnumrecords() const {
      return (groups_.size() - 1) * knumrecordspergroup +
             num_records_in_current_group_;
    }
    indexrecord* at(size_t index) {
      return &(groups_[index / knumrecordspergroup]
                      [index % knumrecordspergroup]);
    }

   private:
    indexrecord* allocatenewgroup() {
      indexrecord* result = new indexrecord[knumrecordspergroup];
      groups_.push_back(result);
      return result;
    }

    // each group in `groups_` contains fix-sized records (determined by
    // knumrecordspergroup). which can help us minimize the cost if resizing
    // occurs.
    const size_t knumrecordspergroup;
    indexrecord* current_group_;
    // list of arrays allocated
    std::vector<indexrecord*> groups_;
    size_t num_records_in_current_group_;
  };

  void allocateindex();

  // internal helper function to bucket index record list to hash buckets.
  void bucketizeindexes(std::vector<indexrecord*>* hash_to_offsets,
                        std::vector<uint32_t>* entries_per_bucket);

  // internal helper class to fill the indexes and bloom filters to internal
  // data structures.
  slice fillindexes(const std::vector<indexrecord*>& hash_to_offsets,
                    const std::vector<uint32_t>& entries_per_bucket);

  arena* arena_;
  options options_;
  histogramimpl keys_per_prefix_hist_;
  indexrecordlist record_list_;
  bool is_first_record_;
  bool due_index_;
  uint32_t num_prefixes_;
  uint32_t num_keys_per_prefix_;

  uint32_t prev_key_prefix_hash_;
  uint32_t index_sparseness_;
  uint32_t index_size_;
  size_t sub_index_size_;

  const slicetransform* prefix_extractor_;
  double hash_table_ratio_;
  double huge_page_tlb_size_;

  std::string prev_key_prefix_;

  static const size_t krecordspergroup = 256;
};

};  // namespace rocksdb
