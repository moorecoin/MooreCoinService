//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include <string>
#include <stdint.h>
#include "rocksdb/slice.h"
#include "rocksdb/status.h"
#include "rocksdb/options.h"
#include "rocksdb/table.h"

namespace rocksdb {

class block;
class randomaccessfile;
struct readoptions;

// the length of the magic number in bytes.
const int kmagicnumberlengthbyte = 8;

// blockhandle is a pointer to the extent of a file that stores a data
// block or a meta block.
class blockhandle {
 public:
  blockhandle();
  blockhandle(uint64_t offset, uint64_t size);

  // the offset of the block in the file.
  uint64_t offset() const { return offset_; }
  void set_offset(uint64_t offset) { offset_ = offset; }

  // the size of the stored block
  uint64_t size() const { return size_; }
  void set_size(uint64_t size) { size_ = size; }

  void encodeto(std::string* dst) const;
  status decodefrom(slice* input);

  // if the block handle's offset and size are both "0", we will view it
  // as a null block handle that points to no where.
  bool isnull() const {
    return offset_ == 0 && size_ == 0;
  }

  static const blockhandle& nullblockhandle() {
    return knullblockhandle;
  }

  // maximum encoding length of a blockhandle
  enum { kmaxencodedlength = 10 + 10 };

 private:
  uint64_t offset_ = 0;
  uint64_t size_ = 0;

  static const blockhandle knullblockhandle;
};

// footer encapsulates the fixed information stored at the tail
// end of every table file.
class footer {
 public:
  // constructs a footer without specifying its table magic number.
  // in such case, the table magic number of such footer should be
  // initialized via @readfooterfromfile().
  footer() : footer(kinvalidtablemagicnumber) {}

  // @table_magic_number serves two purposes:
  //  1. identify different types of the tables.
  //  2. help us to identify if a given file is a valid sst.
  explicit footer(uint64_t table_magic_number);

  // the version of the footer in this file
  uint32_t version() const { return version_; }

  // the checksum type used in this file
  checksumtype checksum() const { return checksum_; }
  void set_checksum(const checksumtype c) { checksum_ = c; }

  // the block handle for the metaindex block of the table
  const blockhandle& metaindex_handle() const { return metaindex_handle_; }
  void set_metaindex_handle(const blockhandle& h) { metaindex_handle_ = h; }

  // the block handle for the index block of the table
  const blockhandle& index_handle() const { return index_handle_; }

  void set_index_handle(const blockhandle& h) { index_handle_ = h; }

  uint64_t table_magic_number() const { return table_magic_number_; }

  // the version of footer we encode
  enum {
    klegacyfooter = 0,
    kfooterversion = 1,
  };

  void encodeto(std::string* dst) const;

  // set the current footer based on the input slice.  if table_magic_number_
  // is not set (i.e., hasinitializedtablemagicnumber() is true), then this
  // function will also initialize table_magic_number_.  otherwise, this
  // function will verify whether the magic number specified in the input
  // slice matches table_magic_number_ and update the current footer only
  // when the test passes.
  status decodefrom(slice* input);

  // encoded length of a footer.  note that the serialization of a footer will
  // always occupy at least kminencodedlength bytes.  if fields are changed
  // the version number should be incremented and kmaxencodedlength should be
  // increased accordingly.
  enum {
    // footer version 0 (legacy) will always occupy exactly this many bytes.
    // it consists of two block handles, padding, and a magic number.
    kversion0encodedlength = 2 * blockhandle::kmaxencodedlength + 8,
    // footer version 1 will always occupy exactly this many bytes.
    // it consists of the checksum type, two block handles, padding,
    // a version number, and a magic number
    kversion1encodedlength = 1 + 2 * blockhandle::kmaxencodedlength + 4 + 8,

    kminencodedlength = kversion0encodedlength,
    kmaxencodedlength = kversion1encodedlength
  };

  static const uint64_t kinvalidtablemagicnumber = 0;

 private:
  // requires: magic number wasn't initialized.
  void set_table_magic_number(uint64_t magic_number) {
    assert(!hasinitializedtablemagicnumber());
    table_magic_number_ = magic_number;
  }

  // return true if @table_magic_number_ is set to a value different
  // from @kinvalidtablemagicnumber.
  bool hasinitializedtablemagicnumber() const {
    return (table_magic_number_ != kinvalidtablemagicnumber);
  }

  uint32_t version_;
  checksumtype checksum_;
  blockhandle metaindex_handle_;
  blockhandle index_handle_;
  uint64_t table_magic_number_ = 0;
};

// read the footer from file
status readfooterfromfile(randomaccessfile* file,
                          uint64_t file_size,
                          footer* footer);

// 1-byte type + 32-bit crc
static const size_t kblocktrailersize = 5;

struct blockcontents {
  slice data;           // actual contents of data
  bool cachable;        // true iff data can be cached
  bool heap_allocated;  // true iff caller should delete[] data.data()
  compressiontype compression_type;
};

// read the block identified by "handle" from "file".  on failure
// return non-ok.  on success fill *result and return ok.
extern status readblockcontents(randomaccessfile* file,
                                const footer& footer,
                                const readoptions& options,
                                const blockhandle& handle,
                                blockcontents* result,
                                env* env,
                                bool do_uncompress);

// the 'data' points to the raw block contents read in from file.
// this method allocates a new heap buffer and the raw block
// contents are uncompresed into this buffer. this buffer is
// returned via 'result' and it is upto the caller to
// free this buffer.
extern status uncompressblockcontents(const char* data,
                                      size_t n,
                                      blockcontents* result);

// implementation details follow.  clients should ignore,

inline blockhandle::blockhandle()
    : blockhandle(~static_cast<uint64_t>(0),
                  ~static_cast<uint64_t>(0)) {
}

inline blockhandle::blockhandle(uint64_t offset, uint64_t size)
    : offset_(offset),
      size_(size) {
}

}  // namespace rocksdb
