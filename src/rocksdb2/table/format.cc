//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "table/format.h"

#include <string>
#include <inttypes.h>

#include "port/port.h"
#include "rocksdb/env.h"
#include "table/block.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include "util/perf_context_imp.h"
#include "util/xxhash.h"

namespace rocksdb {

extern const uint64_t klegacyblockbasedtablemagicnumber;
extern const uint64_t kblockbasedtablemagicnumber;

#ifndef rocksdb_lite
extern const uint64_t klegacyplaintablemagicnumber;
extern const uint64_t kplaintablemagicnumber;
#else
// rocksdb_lite doesn't have plain table
const uint64_t klegacyplaintablemagicnumber = 0;
const uint64_t kplaintablemagicnumber = 0;
#endif
const uint32_t defaultstackbuffersize = 5000;

void blockhandle::encodeto(std::string* dst) const {
  // sanity check that all fields have been set
  assert(offset_ != ~static_cast<uint64_t>(0));
  assert(size_ != ~static_cast<uint64_t>(0));
  putvarint64(dst, offset_);
  putvarint64(dst, size_);
}

status blockhandle::decodefrom(slice* input) {
  if (getvarint64(input, &offset_) &&
      getvarint64(input, &size_)) {
    return status::ok();
  } else {
    return status::corruption("bad block handle");
  }
}
const blockhandle blockhandle::knullblockhandle(0, 0);

// legacy footer format:
//    metaindex handle (varint64 offset, varint64 size)
//    index handle     (varint64 offset, varint64 size)
//    <padding> to make the total size 2 * blockhandle::kmaxencodedlength
//    table_magic_number (8 bytes)
// new footer format:
//    checksum (char, 1 byte)
//    metaindex handle (varint64 offset, varint64 size)
//    index handle     (varint64 offset, varint64 size)
//    <padding> to make the total size 2 * blockhandle::kmaxencodedlength + 1
//    footer version (4 bytes)
//    table_magic_number (8 bytes)
void footer::encodeto(std::string* dst) const {
  if (version() == klegacyfooter) {
    // has to be default checksum with legacy footer
    assert(checksum_ == kcrc32c);
    const size_t original_size = dst->size();
    metaindex_handle_.encodeto(dst);
    index_handle_.encodeto(dst);
    dst->resize(original_size + 2 * blockhandle::kmaxencodedlength);  // padding
    putfixed32(dst, static_cast<uint32_t>(table_magic_number() & 0xffffffffu));
    putfixed32(dst, static_cast<uint32_t>(table_magic_number() >> 32));
    assert(dst->size() == original_size + kversion0encodedlength);
  } else {
    const size_t original_size = dst->size();
    dst->push_back(static_cast<char>(checksum_));
    metaindex_handle_.encodeto(dst);
    index_handle_.encodeto(dst);
    dst->resize(original_size + kversion1encodedlength - 12);  // padding
    putfixed32(dst, kfooterversion);
    putfixed32(dst, static_cast<uint32_t>(table_magic_number() & 0xffffffffu));
    putfixed32(dst, static_cast<uint32_t>(table_magic_number() >> 32));
    assert(dst->size() == original_size + kversion1encodedlength);
  }
}

namespace {
inline bool islegacyfooterformat(uint64_t magic_number) {
  return magic_number == klegacyblockbasedtablemagicnumber ||
         magic_number == klegacyplaintablemagicnumber;
}

inline uint64_t upconvertlegacyfooterformat(uint64_t magic_number) {
  if (magic_number == klegacyblockbasedtablemagicnumber) {
    return kblockbasedtablemagicnumber;
  }
  if (magic_number == klegacyplaintablemagicnumber) {
    return kplaintablemagicnumber;
  }
  assert(false);
  return 0;
}
}  // namespace

footer::footer(uint64_t table_magic_number)
    : version_(islegacyfooterformat(table_magic_number) ? klegacyfooter
                                                        : kfooterversion),
      checksum_(kcrc32c),
      table_magic_number_(table_magic_number) {}

status footer::decodefrom(slice* input) {
  assert(input != nullptr);
  assert(input->size() >= kminencodedlength);

  const char *magic_ptr =
      input->data() + input->size() - kmagicnumberlengthbyte;
  const uint32_t magic_lo = decodefixed32(magic_ptr);
  const uint32_t magic_hi = decodefixed32(magic_ptr + 4);
  uint64_t magic = ((static_cast<uint64_t>(magic_hi) << 32) |
                    (static_cast<uint64_t>(magic_lo)));

  // we check for legacy formats here and silently upconvert them
  bool legacy = islegacyfooterformat(magic);
  if (legacy) {
    magic = upconvertlegacyfooterformat(magic);
  }
  if (hasinitializedtablemagicnumber()) {
    if (magic != table_magic_number()) {
      char buffer[80];
      snprintf(buffer, sizeof(buffer) - 1,
               "not an sstable (bad magic number --- %lx)",
               (long)magic);
      return status::invalidargument(buffer);
    }
  } else {
    set_table_magic_number(magic);
  }

  if (legacy) {
    // the size is already asserted to be at least kminencodedlength
    // at the beginning of the function
    input->remove_prefix(input->size() - kversion0encodedlength);
    version_ = klegacyfooter;
    checksum_ = kcrc32c;
  } else {
    version_ = decodefixed32(magic_ptr - 4);
    if (version_ != kfooterversion) {
      return status::corruption("bad footer version");
    }
    // footer version 1 will always occupy exactly this many bytes.
    // it consists of the checksum type, two block handles, padding,
    // a version number, and a magic number
    if (input->size() < kversion1encodedlength) {
      return status::invalidargument("input is too short to be an sstable");
    } else {
      input->remove_prefix(input->size() - kversion1encodedlength);
    }
    uint32_t checksum;
    if (!getvarint32(input, &checksum)) {
      return status::corruption("bad checksum type");
    }
    checksum_ = static_cast<checksumtype>(checksum);
  }

  status result = metaindex_handle_.decodefrom(input);
  if (result.ok()) {
    result = index_handle_.decodefrom(input);
  }
  if (result.ok()) {
    // we skip over any leftover data (just padding for now) in "input"
    const char* end = magic_ptr + kmagicnumberlengthbyte;
    *input = slice(end, input->data() + input->size() - end);
  }
  return result;
}

status readfooterfromfile(randomaccessfile* file,
                          uint64_t file_size,
                          footer* footer) {
  if (file_size < footer::kminencodedlength) {
    return status::invalidargument("file is too short to be an sstable");
  }

  char footer_space[footer::kmaxencodedlength];
  slice footer_input;
  size_t read_offset = (file_size > footer::kmaxencodedlength)
                           ? (file_size - footer::kmaxencodedlength)
                           : 0;
  status s = file->read(read_offset, footer::kmaxencodedlength, &footer_input,
                        footer_space);
  if (!s.ok()) return s;

  // check that we actually read the whole footer from the file. it may be
  // that size isn't correct.
  if (footer_input.size() < footer::kminencodedlength) {
    return status::invalidargument("file is too short to be an sstable");
  }

  return footer->decodefrom(&footer_input);
}

// read a block and check its crc
// contents is the result of reading.
// according to the implementation of file->read, contents may not point to buf
status readblock(randomaccessfile* file, const footer& footer,
                  const readoptions& options, const blockhandle& handle,
                  slice* contents,  /* result of reading */ char* buf) {
  size_t n = static_cast<size_t>(handle.size());
  status s;

  {
    perf_timer_guard(block_read_time);
    s = file->read(handle.offset(), n + kblocktrailersize, contents, buf);
  }

  perf_counter_add(block_read_count, 1);
  perf_counter_add(block_read_byte, n + kblocktrailersize);

  if (!s.ok()) {
    return s;
  }
  if (contents->size() != n + kblocktrailersize) {
    return status::corruption("truncated block read");
  }

  // check the crc of the type and the block contents
  const char* data = contents->data();  // pointer to where read put the data
  if (options.verify_checksums) {
    perf_timer_guard(block_checksum_time);
    uint32_t value = decodefixed32(data + n + 1);
    uint32_t actual = 0;
    switch (footer.checksum()) {
      case kcrc32c:
        value = crc32c::unmask(value);
        actual = crc32c::value(data, n + 1);
        break;
      case kxxhash:
        actual = xxh32(data, n + 1, 0);
        break;
      default:
        s = status::corruption("unknown checksum type");
    }
    if (s.ok() && actual != value) {
      s = status::corruption("block checksum mismatch");
    }
    if (!s.ok()) {
      return s;
    }
  }
  return s;
}

// decompress a block according to params
// may need to malloc a space for cache usage
status decompressblock(blockcontents* result, size_t block_size,
                          bool do_uncompress, const char* buf,
                          const slice& contents, bool use_stack_buf) {
  status s;
  size_t n = block_size;
  const char* data = contents.data();

  result->data = slice();
  result->cachable = false;
  result->heap_allocated = false;

  perf_timer_guard(block_decompress_time);
  rocksdb::compressiontype compression_type =
      static_cast<rocksdb::compressiontype>(data[n]);
  // if the caller has requested that the block not be uncompressed
  if (!do_uncompress || compression_type == knocompression) {
    if (data != buf) {
      // file implementation gave us pointer to some other data.
      // use it directly under the assumption that it will be live
      // while the file is open.
      result->data = slice(data, n);
      result->heap_allocated = false;
      result->cachable = false;  // do not double-cache
    } else {
      if (use_stack_buf) {
        // need to allocate space in heap for cache usage
        char* new_buf = new char[n];
        memcpy(new_buf, buf, n);
        result->data = slice(new_buf, n);
      } else {
        result->data = slice(buf, n);
      }

      result->heap_allocated = true;
      result->cachable = true;
    }
    result->compression_type = compression_type;
    s = status::ok();
  } else {
    s = uncompressblockcontents(data, n, result);
  }
  return s;
}

// read and decompress block
// use buf in stack as temp reading buffer
status readanddecompressfast(randomaccessfile* file, const footer& footer,
                             const readoptions& options,
                             const blockhandle& handle, blockcontents* result,
                             env* env, bool do_uncompress) {
  status s;
  slice contents;
  size_t n = static_cast<size_t>(handle.size());
  char buf[defaultstackbuffersize];

  s = readblock(file, footer, options, handle, &contents, buf);
  if (!s.ok()) {
    return s;
  }
  s = decompressblock(result, n, do_uncompress, buf, contents, true);
  if (!s.ok()) {
    return s;
  }
  return s;
}

// read and decompress block
// use buf in heap as temp reading buffer
status readanddecompress(randomaccessfile* file, const footer& footer,
                         const readoptions& options, const blockhandle& handle,
                         blockcontents* result, env* env, bool do_uncompress) {
  status s;
  slice contents;
  size_t n = static_cast<size_t>(handle.size());
  char* buf = new char[n + kblocktrailersize];

  s = readblock(file, footer, options, handle, &contents, buf);
  if (!s.ok()) {
    delete[] buf;
    return s;
  }
  s = decompressblock(result, n, do_uncompress, buf, contents, false);
  if (!s.ok()) {
    delete[] buf;
    return s;
  }

  if (result->data.data() != buf) {
    delete[] buf;
  }
  return s;
}

status readblockcontents(randomaccessfile* file, const footer& footer,
                         const readoptions& options, const blockhandle& handle,
                         blockcontents* result, env* env, bool do_uncompress) {
  size_t n = static_cast<size_t>(handle.size());
  if (do_uncompress && n + kblocktrailersize < defaultstackbuffersize) {
    return readanddecompressfast(file, footer, options, handle, result, env,
                                 do_uncompress);
  } else {
    return readanddecompress(file, footer, options, handle, result, env,
                             do_uncompress);
  }
}

//
// the 'data' points to the raw block contents that was read in from file.
// this method allocates a new heap buffer and the raw block
// contents are uncompresed into this buffer. this
// buffer is returned via 'result' and it is upto the caller to
// free this buffer.
status uncompressblockcontents(const char* data, size_t n,
                               blockcontents* result) {
  char* ubuf = nullptr;
  int decompress_size = 0;
  assert(data[n] != knocompression);
  switch (data[n]) {
    case ksnappycompression: {
      size_t ulength = 0;
      static char snappy_corrupt_msg[] =
        "snappy not supported or corrupted snappy compressed block contents";
      if (!port::snappy_getuncompressedlength(data, n, &ulength)) {
        return status::corruption(snappy_corrupt_msg);
      }
      ubuf = new char[ulength];
      if (!port::snappy_uncompress(data, n, ubuf)) {
        delete[] ubuf;
        return status::corruption(snappy_corrupt_msg);
      }
      result->data = slice(ubuf, ulength);
      result->heap_allocated = true;
      result->cachable = true;
      break;
    }
    case kzlibcompression:
      ubuf = port::zlib_uncompress(data, n, &decompress_size);
      static char zlib_corrupt_msg[] =
        "zlib not supported or corrupted zlib compressed block contents";
      if (!ubuf) {
        return status::corruption(zlib_corrupt_msg);
      }
      result->data = slice(ubuf, decompress_size);
      result->heap_allocated = true;
      result->cachable = true;
      break;
    case kbzip2compression:
      ubuf = port::bzip2_uncompress(data, n, &decompress_size);
      static char bzip2_corrupt_msg[] =
        "bzip2 not supported or corrupted bzip2 compressed block contents";
      if (!ubuf) {
        return status::corruption(bzip2_corrupt_msg);
      }
      result->data = slice(ubuf, decompress_size);
      result->heap_allocated = true;
      result->cachable = true;
      break;
    case klz4compression:
      ubuf = port::lz4_uncompress(data, n, &decompress_size);
      static char lz4_corrupt_msg[] =
          "lz4 not supported or corrupted lz4 compressed block contents";
      if (!ubuf) {
        return status::corruption(lz4_corrupt_msg);
      }
      result->data = slice(ubuf, decompress_size);
      result->heap_allocated = true;
      result->cachable = true;
      break;
    case klz4hccompression:
      ubuf = port::lz4_uncompress(data, n, &decompress_size);
      static char lz4hc_corrupt_msg[] =
          "lz4hc not supported or corrupted lz4hc compressed block contents";
      if (!ubuf) {
        return status::corruption(lz4hc_corrupt_msg);
      }
      result->data = slice(ubuf, decompress_size);
      result->heap_allocated = true;
      result->cachable = true;
      break;
    default:
      return status::corruption("bad block type");
  }
  result->compression_type = knocompression;  // not compressed any more
  return status::ok();
}

}  // namespace rocksdb
