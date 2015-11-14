// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once

#ifndef rocksdb_lite
#include <memory>
#include <string>
#include <stdint.h>

#include "rocksdb/options.h"
#include "rocksdb/table.h"

namespace rocksdb {

struct options;
struct envoptions;

using std::unique_ptr;
class status;
class randomaccessfile;
class writablefile;
class table;
class tablebuilder;

// indexedtable requires fixed length key, configured as a constructor
// parameter of the factory class. output file format:
// +-------------+-----------------+
// | version     | user_key_length |
// +------------++------------+-----------------+  <= key1 offset
// |  encoded key1            | value_size  |   |
// +------------+-------------+-------------+   |
// | value1                                     |
// |                                            |
// +--------------------------+-------------+---+  <= key2 offset
// | encoded key2             | value_size  |   |
// +------------+-------------+-------------+   |
// | value2                                     |
// |                                            |
// |        ......                              |
// +-----------------+--------------------------+
//
// when the key encoding type is kplain. key part is encoded as:
// +------------+--------------------+
// | [key_size] |  internal key      |
// +------------+--------------------+
// for the case of user_key_len = kplaintablevariablelength case,
// and simply:
// +----------------------+
// |  internal key        |
// +----------------------+
// for user_key_len != kplaintablevariablelength case.
//
// if key encoding type is kprefix. keys are encoding in this format.
// there are three ways to encode a key:
// (1) full key
// +---------------+---------------+-------------------+
// | full key flag | full key size | full internal key |
// +---------------+---------------+-------------------+
// which simply encodes a full key
//
// (2) a key shared the same prefix as the previous key, which is encoded as
//     format of (1).
// +-------------+-------------+-------------+-------------+------------+
// | prefix flag | prefix size | suffix flag | suffix size | key suffix |
// +-------------+-------------+-------------+-------------+------------+
// where key is the suffix part of the key, including the internal bytes.
// the actual key will be constructed by concatenating prefix part of the
// previous key, with the suffix part of the key here, with sizes given here.
//
// (3) a key shared the same prefix as the previous key, which is encoded as
//     the format of (2).
// +-----------------+-----------------+------------------------+
// | key suffix flag | key suffix size | suffix of internal key |
// +-----------------+-----------------+------------------------+
// the key will be constructed by concatenating previous key's prefix (which is
// also a prefix which the last key encoded in the format of (1)) and the
// key given here.
//
// for example, we for following keys (prefix and suffix are separated by
// spaces):
//   0000 0001
//   0000 00021
//   0000 0002
//   00011 00
//   0002 0001
// will be encoded like this:
//   fk 8 00000001
//   pf 4 sf 5 00021
//   sf 4 0002
//   fk 7 0001100
//   fk 8 00020001
// (where fk means full key flag, pf means prefix flag and sf means suffix flag)
//
// all those "key flag + key size" shown above are in this format:
// the 8 bits of the first byte:
// +----+----+----+----+----+----+----+----+
// |  type   |            size             |
// +----+----+----+----+----+----+----+----+
// type indicates: full key, prefix, or suffix.
// the last 6 bits are for size. if the size bits are not all 1, it means the
// size of the key. otherwise, varint32 is read after this byte. this varint
// value + 0x3f (the value of all 1) will be the key size.
//
// for example, full key with length 16 will be encoded as (binary):
//     00 010000
// (00 means full key)
// and a prefix with 100 bytes will be encoded as:
//     01 111111    00100101
//         (63)       (37)
// (01 means key suffix)
//
// all the internal keys above (including kplain and kprefix) are encoded in
// this format:
// there are two types:
// (1) normal internal key format
// +----------- ...... -------------+----+---+---+---+---+---+---+---+
// |       user key                 |type|      sequence id          |
// +----------- ..... --------------+----+---+---+---+---+---+---+---+
// (2) special case for keys whose sequence id is 0 and is value type
// +----------- ...... -------------+----+
// |       user key                 |0x80|
// +----------- ..... --------------+----+
// to save 7 bytes for the special case where sequence id = 0.
//
//
class plaintablefactory : public tablefactory {
 public:
  ~plaintablefactory() {}
  // user_key_size is the length of the user key. if it is set to be
  // kplaintablevariablelength, then it means variable length. otherwise, all
  // the keys need to have the fix length of this value. bloom_bits_per_key is
  // number of bits used for bloom filer per key. hash_table_ratio is
  // the desired utilization of the hash table used for prefix hashing.
  // hash_table_ratio = number of prefixes / #buckets in the hash table
  // hash_table_ratio = 0 means skip hash table but only replying on binary
  // search.
  // index_sparseness determines index interval for keys
  // inside the same prefix. it will be the maximum number of linear search
  // required after hash and binary search.
  // index_sparseness = 0 means index for every key.
  // huge_page_tlb_size determines whether to allocate hash indexes from huge
  // page tlb and the page size if allocating from there. see comments of
  // arena::allocatealigned() for details.
  explicit plaintablefactory(const plaintableoptions& options =
                                 plaintableoptions())
      : user_key_len_(options.user_key_len),
        bloom_bits_per_key_(options.bloom_bits_per_key),
        hash_table_ratio_(options.hash_table_ratio),
        index_sparseness_(options.index_sparseness),
        huge_page_tlb_size_(options.huge_page_tlb_size),
        encoding_type_(options.encoding_type),
        full_scan_mode_(options.full_scan_mode),
        store_index_in_file_(options.store_index_in_file) {}
  const char* name() const override { return "plaintable"; }
  status newtablereader(const options& options, const envoptions& soptions,
                        const internalkeycomparator& internal_comparator,
                        unique_ptr<randomaccessfile>&& file, uint64_t file_size,
                        unique_ptr<tablereader>* table) const override;
  tablebuilder* newtablebuilder(const options& options,
                                const internalkeycomparator& icomparator,
                                writablefile* file,
                                compressiontype compression_type) const
      override;

  std::string getprintabletableoptions() const override;

  static const char kvaluetypeseqid0 = 0xff;

  // sanitizes the specified db options.
  status sanitizedboptions(const dboptions* db_opts) const override {
    if (db_opts->allow_mmap_reads == false) {
      return status::notsupported(
          "plaintable with allow_mmap_reads == false is not supported.");
    }
    return status::ok();
  }

 private:
  uint32_t user_key_len_;
  int bloom_bits_per_key_;
  double hash_table_ratio_;
  size_t index_sparseness_;
  size_t huge_page_tlb_size_;
  encodingtype encoding_type_;
  bool full_scan_mode_;
  bool store_index_in_file_;
};

}  // namespace rocksdb
#endif  // rocksdb_lite
