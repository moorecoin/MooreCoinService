//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#pragma once
#ifndef rocksdb_lite

#include "rocksdb/slice.h"
#include "db/dbformat.h"

namespace rocksdb {

class writablefile;
struct parsedinternalkey;

// helper class to write out a key to an output file
// actual data format of the key is documented in plain_table_factory.h
class plaintablekeyencoder {
 public:
  explicit plaintablekeyencoder(encodingtype encoding_type,
                                uint32_t user_key_len,
                                const slicetransform* prefix_extractor,
                                size_t index_sparseness)
      : encoding_type_((prefix_extractor != nullptr) ? encoding_type : kplain),
        fixed_user_key_len_(user_key_len),
        prefix_extractor_(prefix_extractor),
        index_sparseness_((index_sparseness > 1) ? index_sparseness : 1),
        key_count_for_prefix(0) {}
  // key: the key to write out, in the format of internal key.
  // file: the output file to write out
  // offset: offset in the file. needs to be updated after appending bytes
  //         for the key
  // meta_bytes_buf: buffer for extra meta bytes
  // meta_bytes_buf_size: offset to append extra meta bytes. will be updated
  //                      if meta_bytes_buf is updated.
  status appendkey(const slice& key, writablefile* file, uint64_t* offset,
                   char* meta_bytes_buf, size_t* meta_bytes_buf_size);

  // return actual encoding type to be picked
  encodingtype getencodingtype() { return encoding_type_; }

 private:
  encodingtype encoding_type_;
  uint32_t fixed_user_key_len_;
  const slicetransform* prefix_extractor_;
  const size_t index_sparseness_;
  size_t key_count_for_prefix;
  iterkey pre_prefix_;
};

// a helper class to decode keys from input buffer
// actual data format of the key is documented in plain_table_factory.h
class plaintablekeydecoder {
 public:
  explicit plaintablekeydecoder(encodingtype encoding_type,
                                uint32_t user_key_len,
                                const slicetransform* prefix_extractor)
      : encoding_type_(encoding_type),
        prefix_len_(0),
        fixed_user_key_len_(user_key_len),
        prefix_extractor_(prefix_extractor),
        in_prefix_(false) {}
  // find the next key.
  // start: char array where the key starts.
  // limit: boundary of the char array
  // parsed_key: the output of the result key
  // internal_key: if not null, fill with the output of the result key in
  //               un-parsed format
  // bytes_read: how many bytes read from start. output
  // seekable: whether key can be read from this place. used when building
  //           indexes. output.
  status nextkey(const char* start, const char* limit,
                 parsedinternalkey* parsed_key, slice* internal_key,
                 size_t* bytes_read, bool* seekable = nullptr);
  encodingtype encoding_type_;
  uint32_t prefix_len_;
  uint32_t fixed_user_key_len_;
  slice saved_user_key_;
  iterkey cur_key_;
  const slicetransform* prefix_extractor_;
  bool in_prefix_;

 private:
  status nextplainencodingkey(const char* start, const char* limit,
                              parsedinternalkey* parsed_key,
                              slice* internal_key, size_t* bytes_read,
                              bool* seekable = nullptr);
  status nextprefixencodingkey(const char* start, const char* limit,
                               parsedinternalkey* parsed_key,
                               slice* internal_key, size_t* bytes_read,
                               bool* seekable = nullptr);
};

}  // namespace rocksdb

#endif  // rocksdb_lite
