//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#ifndef rocksdb_lite
#include "table/plain_table_key_coding.h"

#include "table/plain_table_factory.h"
#include "db/dbformat.h"

namespace rocksdb {

namespace {

enum entrytype : unsigned char {
  kfullkey = 0,
  kprefixfrompreviouskey = 1,
  kkeysuffix = 2,
};

// control byte:
// first two bits indicate type of entry
// other bytes are inlined sizes. if all bits are 1 (0x03f), overflow bytes
// are used. key_size-0x3f will be encoded as a variint32 after this bytes.

const unsigned char ksizeinlinelimit = 0x3f;

// return 0 for error
size_t encodesize(entrytype type, uint32_t key_size, char* out_buffer) {
  out_buffer[0] = type << 6;

  if (key_size < 0x3f) {
    // size inlined
    out_buffer[0] |= static_cast<char>(key_size);
    return 1;
  } else {
    out_buffer[0] |= ksizeinlinelimit;
    char* ptr = encodevarint32(out_buffer + 1, key_size - ksizeinlinelimit);
    return ptr - out_buffer;
  }
}

// return position after the size byte(s). nullptr means error
const char* decodesize(const char* offset, const char* limit,
                       entrytype* entry_type, size_t* key_size) {
  assert(offset < limit);
  *entry_type = static_cast<entrytype>(
      (static_cast<unsigned char>(offset[0]) & ~ksizeinlinelimit) >> 6);
  char inline_key_size = offset[0] & ksizeinlinelimit;
  if (inline_key_size < ksizeinlinelimit) {
    *key_size = inline_key_size;
    return offset + 1;
  } else {
    uint32_t extra_size;
    const char* ptr = getvarint32ptr(offset + 1, limit, &extra_size);
    if (ptr == nullptr) {
      return nullptr;
    }
    *key_size = ksizeinlinelimit + extra_size;
    return ptr;
  }
}
}  // namespace

status plaintablekeyencoder::appendkey(const slice& key, writablefile* file,
                                       uint64_t* offset, char* meta_bytes_buf,
                                       size_t* meta_bytes_buf_size) {
  parsedinternalkey parsed_key;
  if (!parseinternalkey(key, &parsed_key)) {
    return status::corruption(slice());
  }

  slice key_to_write = key;  // portion of internal key to write out.

  size_t user_key_size = fixed_user_key_len_;
  if (encoding_type_ == kplain) {
    if (fixed_user_key_len_ == kplaintablevariablelength) {
      user_key_size = key.size() - 8;
      // write key length
      char key_size_buf[5];  // tmp buffer for key size as varint32
      char* ptr = encodevarint32(key_size_buf, user_key_size);
      assert(ptr <= key_size_buf + sizeof(key_size_buf));
      auto len = ptr - key_size_buf;
      status s = file->append(slice(key_size_buf, len));
      if (!s.ok()) {
        return s;
      }
      *offset += len;
    }
  } else {
    assert(encoding_type_ == kprefix);
    char size_bytes[12];
    size_t size_bytes_pos = 0;

    user_key_size = key.size() - 8;

    slice prefix =
        prefix_extractor_->transform(slice(key.data(), user_key_size));
    if (key_count_for_prefix == 0 || prefix != pre_prefix_.getkey() ||
        key_count_for_prefix % index_sparseness_ == 0) {
      key_count_for_prefix = 1;
      pre_prefix_.setkey(prefix);
      size_bytes_pos += encodesize(kfullkey, user_key_size, size_bytes);
      status s = file->append(slice(size_bytes, size_bytes_pos));
      if (!s.ok()) {
        return s;
      }
      *offset += size_bytes_pos;
    } else {
      key_count_for_prefix++;
      if (key_count_for_prefix == 2) {
        // for second key within a prefix, need to encode prefix length
        size_bytes_pos +=
            encodesize(kprefixfrompreviouskey, pre_prefix_.getkey().size(),
                       size_bytes + size_bytes_pos);
      }
      size_t prefix_len = pre_prefix_.getkey().size();
      size_bytes_pos += encodesize(kkeysuffix, user_key_size - prefix_len,
                                   size_bytes + size_bytes_pos);
      status s = file->append(slice(size_bytes, size_bytes_pos));
      if (!s.ok()) {
        return s;
      }
      *offset += size_bytes_pos;
      key_to_write = slice(key.data() + prefix_len, key.size() - prefix_len);
    }
  }

  // encode full key
  // for value size as varint32 (up to 5 bytes).
  // if the row is of value type with seqid 0, flush the special flag together
  // in this buffer to safe one file append call, which takes 1 byte.
  if (parsed_key.sequence == 0 && parsed_key.type == ktypevalue) {
    status s =
        file->append(slice(key_to_write.data(), key_to_write.size() - 8));
    if (!s.ok()) {
      return s;
    }
    *offset += key_to_write.size() - 8;
    meta_bytes_buf[*meta_bytes_buf_size] = plaintablefactory::kvaluetypeseqid0;
    *meta_bytes_buf_size += 1;
  } else {
    file->append(key_to_write);
    *offset += key_to_write.size();
  }

  return status::ok();
}

namespace {
status readinternalkey(const char* key_ptr, const char* limit,
                       uint32_t user_key_size, parsedinternalkey* parsed_key,
                       size_t* bytes_read, bool* internal_key_valid,
                       slice* internal_key) {
  if (key_ptr + user_key_size + 1 >= limit) {
    return status::corruption("unexpected eof when reading the next key");
  }
  if (*(key_ptr + user_key_size) == plaintablefactory::kvaluetypeseqid0) {
    // special encoding for the row with seqid=0
    parsed_key->user_key = slice(key_ptr, user_key_size);
    parsed_key->sequence = 0;
    parsed_key->type = ktypevalue;
    *bytes_read += user_key_size + 1;
    *internal_key_valid = false;
  } else {
    if (key_ptr + user_key_size + 8 >= limit) {
      return status::corruption(
          "unexpected eof when reading internal bytes of the next key");
    }
    *internal_key_valid = true;
    *internal_key = slice(key_ptr, user_key_size + 8);
    if (!parseinternalkey(*internal_key, parsed_key)) {
      return status::corruption(
          slice("incorrect value type found when reading the next key"));
    }
    *bytes_read += user_key_size + 8;
  }
  return status::ok();
}
}  // namespace

status plaintablekeydecoder::nextplainencodingkey(
    const char* start, const char* limit, parsedinternalkey* parsed_key,
    slice* internal_key, size_t* bytes_read, bool* seekable) {
  const char* key_ptr = start;
  size_t user_key_size = 0;
  if (fixed_user_key_len_ != kplaintablevariablelength) {
    user_key_size = fixed_user_key_len_;
    key_ptr = start;
  } else {
    uint32_t tmp_size = 0;
    key_ptr = getvarint32ptr(start, limit, &tmp_size);
    if (key_ptr == nullptr) {
      return status::corruption(
          "unexpected eof when reading the next key's size");
    }
    user_key_size = static_cast<size_t>(tmp_size);
    *bytes_read = key_ptr - start;
  }
  // dummy initial value to avoid compiler complain
  bool decoded_internal_key_valid = true;
  slice decoded_internal_key;
  status s =
      readinternalkey(key_ptr, limit, user_key_size, parsed_key, bytes_read,
                      &decoded_internal_key_valid, &decoded_internal_key);
  if (!s.ok()) {
    return s;
  }
  if (internal_key != nullptr) {
    if (decoded_internal_key_valid) {
      *internal_key = decoded_internal_key;
    } else {
      // need to copy out the internal key
      cur_key_.setinternalkey(*parsed_key);
      *internal_key = cur_key_.getkey();
    }
  }
  return status::ok();
}

status plaintablekeydecoder::nextprefixencodingkey(
    const char* start, const char* limit, parsedinternalkey* parsed_key,
    slice* internal_key, size_t* bytes_read, bool* seekable) {
  const char* key_ptr = start;
  entrytype entry_type;

  bool expect_suffix = false;
  do {
    size_t size = 0;
    // dummy initial value to avoid compiler complain
    bool decoded_internal_key_valid = true;
    const char* pos = decodesize(key_ptr, limit, &entry_type, &size);
    if (pos == nullptr) {
      return status::corruption("unexpected eof when reading size of the key");
    }
    *bytes_read += pos - key_ptr;
    key_ptr = pos;

    switch (entry_type) {
      case kfullkey: {
        expect_suffix = false;
        slice decoded_internal_key;
        status s =
            readinternalkey(key_ptr, limit, size, parsed_key, bytes_read,
                            &decoded_internal_key_valid, &decoded_internal_key);
        if (!s.ok()) {
          return s;
        }
        saved_user_key_ = parsed_key->user_key;
        if (internal_key != nullptr) {
          if (decoded_internal_key_valid) {
            *internal_key = decoded_internal_key;
          } else {
            cur_key_.setinternalkey(*parsed_key);
            *internal_key = cur_key_.getkey();
          }
        }
        break;
      }
      case kprefixfrompreviouskey: {
        if (seekable != nullptr) {
          *seekable = false;
        }
        prefix_len_ = size;
        assert(prefix_extractor_ == nullptr ||
               prefix_extractor_->transform(saved_user_key_).size() ==
                   prefix_len_);
        // need read another size flag for suffix
        expect_suffix = true;
        break;
      }
      case kkeysuffix: {
        expect_suffix = false;
        if (seekable != nullptr) {
          *seekable = false;
        }
        assert(prefix_len_ >= 0);
        cur_key_.reserve(prefix_len_ + size);

        slice tmp_slice;
        status s = readinternalkey(key_ptr, limit, size, parsed_key, bytes_read,
                                   &decoded_internal_key_valid, &tmp_slice);
        if (!s.ok()) {
          return s;
        }
        cur_key_.setinternalkey(slice(saved_user_key_.data(), prefix_len_),
                                *parsed_key);
        assert(
            prefix_extractor_ == nullptr ||
            prefix_extractor_->transform(extractuserkey(cur_key_.getkey())) ==
                slice(saved_user_key_.data(), prefix_len_));
        parsed_key->user_key = extractuserkey(cur_key_.getkey());
        if (internal_key != nullptr) {
          *internal_key = cur_key_.getkey();
        }
        break;
      }
      default:
        return status::corruption("identified size flag.");
    }
  } while (expect_suffix);  // another round if suffix is expected.
  return status::ok();
}

status plaintablekeydecoder::nextkey(const char* start, const char* limit,
                                     parsedinternalkey* parsed_key,
                                     slice* internal_key, size_t* bytes_read,
                                     bool* seekable) {
  *bytes_read = 0;
  if (seekable != nullptr) {
    *seekable = true;
  }
  if (encoding_type_ == kplain) {
    return nextplainencodingkey(start, limit, parsed_key, internal_key,
                                bytes_read, seekable);
  } else {
    assert(encoding_type_ == kprefix);
    return nextprefixencodingkey(start, limit, parsed_key, internal_key,
                                 bytes_read, seekable);
  }
}

}  // namespace rocksdb
#endif  // rocksdb_lite
