//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// endian-neutral encoding:
// * fixed-length numbers are encoded with least-significant byte first
// * in addition we support variable length "varint" encoding
// * strings are encoded prefixed by their length in varint format

#pragma once
#include <algorithm>
#include <stdint.h>
#include <string.h>
#include <string>

#include "rocksdb/write_batch.h"
#include "port/port.h"

namespace rocksdb {

// the maximum length of a varint in bytes for 32 and 64 bits respectively.
const unsigned int kmaxvarint32length = 5;
const unsigned int kmaxvarint64length = 10;

// standard put... routines append to a string
extern void putfixed32(std::string* dst, uint32_t value);
extern void putfixed64(std::string* dst, uint64_t value);
extern void putvarint32(std::string* dst, uint32_t value);
extern void putvarint64(std::string* dst, uint64_t value);
extern void putlengthprefixedslice(std::string* dst, const slice& value);
extern void putlengthprefixedsliceparts(std::string* dst,
                                        const sliceparts& slice_parts);

// standard get... routines parse a value from the beginning of a slice
// and advance the slice past the parsed value.
extern bool getfixed64(slice* input, uint64_t* value);
extern bool getvarint32(slice* input, uint32_t* value);
extern bool getvarint64(slice* input, uint64_t* value);
extern bool getlengthprefixedslice(slice* input, slice* result);
// this function assumes data is well-formed.
extern slice getlengthprefixedslice(const char* data);

extern slice getsliceuntil(slice* slice, char delimiter);

// pointer-based variants of getvarint...  these either store a value
// in *v and return a pointer just past the parsed value, or return
// nullptr on error.  these routines only look at bytes in the range
// [p..limit-1]
extern const char* getvarint32ptr(const char* p,const char* limit, uint32_t* v);
extern const char* getvarint64ptr(const char* p,const char* limit, uint64_t* v);

// returns the length of the varint32 or varint64 encoding of "v"
extern int varintlength(uint64_t v);

// lower-level versions of put... that write directly into a character buffer
// requires: dst has enough space for the value being written
extern void encodefixed32(char* dst, uint32_t value);
extern void encodefixed64(char* dst, uint64_t value);

// lower-level versions of put... that write directly into a character buffer
// and return a pointer just past the last byte written.
// requires: dst has enough space for the value being written
extern char* encodevarint32(char* dst, uint32_t value);
extern char* encodevarint64(char* dst, uint64_t value);

// lower-level versions of get... that read directly from a character buffer
// without any bounds checking.

inline uint32_t decodefixed32(const char* ptr) {
  if (port::klittleendian) {
    // load the raw bytes
    uint32_t result;
    memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
    return result;
  } else {
    return ((static_cast<uint32_t>(static_cast<unsigned char>(ptr[0])))
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[1])) << 8)
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[2])) << 16)
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[3])) << 24));
  }
}

inline uint64_t decodefixed64(const char* ptr) {
  if (port::klittleendian) {
    // load the raw bytes
    uint64_t result;
    memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
    return result;
  } else {
    uint64_t lo = decodefixed32(ptr);
    uint64_t hi = decodefixed32(ptr + 4);
    return (hi << 32) | lo;
  }
}

// internal routine for use by fallback path of getvarint32ptr
extern const char* getvarint32ptrfallback(const char* p,
                                          const char* limit,
                                          uint32_t* value);
inline const char* getvarint32ptr(const char* p,
                                  const char* limit,
                                  uint32_t* value) {
  if (p < limit) {
    uint32_t result = *(reinterpret_cast<const unsigned char*>(p));
    if ((result & 128) == 0) {
      *value = result;
      return p + 1;
    }
  }
  return getvarint32ptrfallback(p, limit, value);
}

// -- implementation of the functions declared above
inline void encodefixed32(char* buf, uint32_t value) {
#if __byte_order == __little_endian
  memcpy(buf, &value, sizeof(value));
#else
  buf[0] = value & 0xff;
  buf[1] = (value >> 8) & 0xff;
  buf[2] = (value >> 16) & 0xff;
  buf[3] = (value >> 24) & 0xff;
#endif
}

inline void encodefixed64(char* buf, uint64_t value) {
#if __byte_order == __little_endian
  memcpy(buf, &value, sizeof(value));
#else
  buf[0] = value & 0xff;
  buf[1] = (value >> 8) & 0xff;
  buf[2] = (value >> 16) & 0xff;
  buf[3] = (value >> 24) & 0xff;
  buf[4] = (value >> 32) & 0xff;
  buf[5] = (value >> 40) & 0xff;
  buf[6] = (value >> 48) & 0xff;
  buf[7] = (value >> 56) & 0xff;
#endif
}

inline void putfixed32(std::string* dst, uint32_t value) {
  char buf[sizeof(value)];
  encodefixed32(buf, value);
  dst->append(buf, sizeof(buf));
}

inline void putfixed64(std::string* dst, uint64_t value) {
  char buf[sizeof(value)];
  encodefixed64(buf, value);
  dst->append(buf, sizeof(buf));
}

inline void putvarint32(std::string* dst, uint32_t v) {
  char buf[5];
  char* ptr = encodevarint32(buf, v);
  dst->append(buf, ptr - buf);
}

inline char* encodevarint64(char* dst, uint64_t v) {
  static const unsigned int b = 128;
  unsigned char* ptr = reinterpret_cast<unsigned char*>(dst);
  while (v >= b) {
    *(ptr++) = (v & (b - 1)) | b;
    v >>= 7;
  }
  *(ptr++) = static_cast<unsigned char>(v);
  return reinterpret_cast<char*>(ptr);
}

inline void putvarint64(std::string* dst, uint64_t v) {
  char buf[10];
  char* ptr = encodevarint64(buf, v);
  dst->append(buf, ptr - buf);
}

inline void putlengthprefixedslice(std::string* dst, const slice& value) {
  putvarint32(dst, value.size());
  dst->append(value.data(), value.size());
}

inline void putlengthprefixedsliceparts(std::string* dst,
                                        const sliceparts& slice_parts) {
  uint32_t total_bytes = 0;
  for (int i = 0; i < slice_parts.num_parts; ++i) {
    total_bytes += slice_parts.parts[i].size();
  }
  putvarint32(dst, total_bytes);
  for (int i = 0; i < slice_parts.num_parts; ++i) {
    dst->append(slice_parts.parts[i].data(), slice_parts.parts[i].size());
  }
}

inline int varintlength(uint64_t v) {
  int len = 1;
  while (v >= 128) {
    v >>= 7;
    len++;
  }
  return len;
}

inline bool getfixed64(slice* input, uint64_t* value) {
  if (input->size() < sizeof(uint64_t)) {
    return false;
  }
  *value = decodefixed64(input->data());
  input->remove_prefix(sizeof(uint64_t));
  return true;
}

inline bool getvarint32(slice* input, uint32_t* value) {
  const char* p = input->data();
  const char* limit = p + input->size();
  const char* q = getvarint32ptr(p, limit, value);
  if (q == nullptr) {
    return false;
  } else {
    *input = slice(q, limit - q);
    return true;
  }
}

inline bool getvarint64(slice* input, uint64_t* value) {
  const char* p = input->data();
  const char* limit = p + input->size();
  const char* q = getvarint64ptr(p, limit, value);
  if (q == nullptr) {
    return false;
  } else {
    *input = slice(q, limit - q);
    return true;
  }
}

inline bool getlengthprefixedslice(slice* input, slice* result) {
  uint32_t len = 0;
  if (getvarint32(input, &len) && input->size() >= len) {
    *result = slice(input->data(), len);
    input->remove_prefix(len);
    return true;
  } else {
    return false;
  }
}

inline slice getlengthprefixedslice(const char* data) {
  uint32_t len = 0;
  // +5: we assume "data" is not corrupted
  auto p = getvarint32ptr(data, data + 5 /* limit */, &len);
  return slice(p, len);
}

inline slice getsliceuntil(slice* slice, char delimiter) {
  uint32_t len = 0;
  for (len = 0; len < slice->size() && slice->data()[len] != delimiter; ++len) {
    // nothing
  }

  slice ret(slice->data(), len);
  slice->remove_prefix(len + ((len < slice->size()) ? 1 : 0));
  return ret;
}

}  // namespace rocksdb
