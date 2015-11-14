// protocol buffers - google's data interchange format
// copyright 2008 google inc.  all rights reserved.
// http://code.google.com/p/protobuf/
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * neither the name of google inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// owner or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.

// author: kenton@google.com (kenton varda)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.
//
// this implementation is heavily optimized to make reads and writes
// of small values (especially varints) as fast as possible.  in
// particular, we optimize for the common case that a read or a write
// will not cross the end of the buffer, since we can avoid a lot
// of branching in this case.

#include <google/protobuf/io/coded_stream_inl.h>
#include <algorithm>
#include <limits.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stl_util.h>


namespace google {
namespace protobuf {
namespace io {

namespace {

static const int kmaxvarintbytes = 10;
static const int kmaxvarint32bytes = 5;


inline bool nextnonempty(zerocopyinputstream* input,
                         const void** data, int* size) {
  bool success;
  do {
    success = input->next(data, size);
  } while (success && *size == 0);
  return success;
}

}  // namespace

// codedinputstream ==================================================

codedinputstream::~codedinputstream() {
  if (input_ != null) {
    backupinputtocurrentposition();
  }

  if (total_bytes_warning_threshold_ == -2) {
    google_log(warning) << "the total number of bytes read was " << total_bytes_read_;
  }
}

// static.
int codedinputstream::default_recursion_limit_ = 100;


void codedinputstream::backupinputtocurrentposition() {
  int backup_bytes = buffersize() + buffer_size_after_limit_ + overflow_bytes_;
  if (backup_bytes > 0) {
    input_->backup(backup_bytes);

    // total_bytes_read_ doesn't include overflow_bytes_.
    total_bytes_read_ -= buffersize() + buffer_size_after_limit_;
    buffer_end_ = buffer_;
    buffer_size_after_limit_ = 0;
    overflow_bytes_ = 0;
  }
}

inline void codedinputstream::recomputebufferlimits() {
  buffer_end_ += buffer_size_after_limit_;
  int closest_limit = min(current_limit_, total_bytes_limit_);
  if (closest_limit < total_bytes_read_) {
    // the limit position is in the current buffer.  we must adjust
    // the buffer size accordingly.
    buffer_size_after_limit_ = total_bytes_read_ - closest_limit;
    buffer_end_ -= buffer_size_after_limit_;
  } else {
    buffer_size_after_limit_ = 0;
  }
}

codedinputstream::limit codedinputstream::pushlimit(int byte_limit) {
  // current position relative to the beginning of the stream.
  int current_position = currentposition();

  limit old_limit = current_limit_;

  // security: byte_limit is possibly evil, so check for negative values
  // and overflow.
  if (byte_limit >= 0 &&
      byte_limit <= int_max - current_position) {
    current_limit_ = current_position + byte_limit;
  } else {
    // negative or overflow.
    current_limit_ = int_max;
  }

  // we need to enforce all limits, not just the new one, so if the previous
  // limit was before the new requested limit, we continue to enforce the
  // previous limit.
  current_limit_ = min(current_limit_, old_limit);

  recomputebufferlimits();
  return old_limit;
}

void codedinputstream::poplimit(limit limit) {
  // the limit passed in is actually the *old* limit, which we returned from
  // pushlimit().
  current_limit_ = limit;
  recomputebufferlimits();

  // we may no longer be at a legitimate message end.  readtag() needs to be
  // called again to find out.
  legitimate_message_end_ = false;
}

int codedinputstream::bytesuntillimit() const {
  if (current_limit_ == int_max) return -1;
  int current_position = currentposition();

  return current_limit_ - current_position;
}

void codedinputstream::settotalbyteslimit(
    int total_bytes_limit, int warning_threshold) {
  // make sure the limit isn't already past, since this could confuse other
  // code.
  int current_position = currentposition();
  total_bytes_limit_ = max(current_position, total_bytes_limit);
  if (warning_threshold >= 0) {
    total_bytes_warning_threshold_ = warning_threshold;
  } else {
    // warning_threshold is negative
    total_bytes_warning_threshold_ = -1;
  }
  recomputebufferlimits();
}

void codedinputstream::printtotalbyteslimiterror() {
  google_log(error) << "a protocol message was rejected because it was too "
                "big (more than " << total_bytes_limit_
             << " bytes).  to increase the limit (or to disable these "
                "warnings), see codedinputstream::settotalbyteslimit() "
                "in google/protobuf/io/coded_stream.h.";
}

bool codedinputstream::skip(int count) {
  if (count < 0) return false;  // security: count is often user-supplied

  const int original_buffer_size = buffersize();

  if (count <= original_buffer_size) {
    // just skipping within the current buffer.  easy.
    advance(count);
    return true;
  }

  if (buffer_size_after_limit_ > 0) {
    // we hit a limit inside this buffer.  advance to the limit and fail.
    advance(original_buffer_size);
    return false;
  }

  count -= original_buffer_size;
  buffer_ = null;
  buffer_end_ = buffer_;

  // make sure this skip doesn't try to skip past the current limit.
  int closest_limit = min(current_limit_, total_bytes_limit_);
  int bytes_until_limit = closest_limit - total_bytes_read_;
  if (bytes_until_limit < count) {
    // we hit the limit.  skip up to it then fail.
    if (bytes_until_limit > 0) {
      total_bytes_read_ = closest_limit;
      input_->skip(bytes_until_limit);
    }
    return false;
  }

  total_bytes_read_ += count;
  return input_->skip(count);
}

bool codedinputstream::getdirectbufferpointer(const void** data, int* size) {
  if (buffersize() == 0 && !refresh()) return false;

  *data = buffer_;
  *size = buffersize();
  return true;
}

bool codedinputstream::readraw(void* buffer, int size) {
  int current_buffer_size;
  while ((current_buffer_size = buffersize()) < size) {
    // reading past end of buffer.  copy what we have, then refresh.
    memcpy(buffer, buffer_, current_buffer_size);
    buffer = reinterpret_cast<uint8*>(buffer) + current_buffer_size;
    size -= current_buffer_size;
    advance(current_buffer_size);
    if (!refresh()) return false;
  }

  memcpy(buffer, buffer_, size);
  advance(size);

  return true;
}

bool codedinputstream::readstring(string* buffer, int size) {
  if (size < 0) return false;  // security: size is often user-supplied
  return internalreadstringinline(buffer, size);
}

bool codedinputstream::readstringfallback(string* buffer, int size) {
  if (!buffer->empty()) {
    buffer->clear();
  }

  int current_buffer_size;
  while ((current_buffer_size = buffersize()) < size) {
    // some stl implementations "helpfully" crash on buffer->append(null, 0).
    if (current_buffer_size != 0) {
      // note:  string1.append(string2) is o(string2.size()) (as opposed to
      //   o(string1.size() + string2.size()), which would be bad).
      buffer->append(reinterpret_cast<const char*>(buffer_),
                     current_buffer_size);
    }
    size -= current_buffer_size;
    advance(current_buffer_size);
    if (!refresh()) return false;
  }

  buffer->append(reinterpret_cast<const char*>(buffer_), size);
  advance(size);

  return true;
}


bool codedinputstream::readlittleendian32fallback(uint32* value) {
  uint8 bytes[sizeof(*value)];

  const uint8* ptr;
  if (buffersize() >= sizeof(*value)) {
    // fast path:  enough bytes in the buffer to read directly.
    ptr = buffer_;
    advance(sizeof(*value));
  } else {
    // slow path:  had to read past the end of the buffer.
    if (!readraw(bytes, sizeof(*value))) return false;
    ptr = bytes;
  }
  readlittleendian32fromarray(ptr, value);
  return true;
}

bool codedinputstream::readlittleendian64fallback(uint64* value) {
  uint8 bytes[sizeof(*value)];

  const uint8* ptr;
  if (buffersize() >= sizeof(*value)) {
    // fast path:  enough bytes in the buffer to read directly.
    ptr = buffer_;
    advance(sizeof(*value));
  } else {
    // slow path:  had to read past the end of the buffer.
    if (!readraw(bytes, sizeof(*value))) return false;
    ptr = bytes;
  }
  readlittleendian64fromarray(ptr, value);
  return true;
}

namespace {

inline const uint8* readvarint32fromarray(
    const uint8* buffer, uint32* value) google_attribute_always_inline;
inline const uint8* readvarint32fromarray(const uint8* buffer, uint32* value) {
  // fast path:  we have enough bytes left in the buffer to guarantee that
  // this read won't cross the end, so we can skip the checks.
  const uint8* ptr = buffer;
  uint32 b;
  uint32 result;

  b = *(ptr++); result  = (b & 0x7f)      ; if (!(b & 0x80)) goto done;
  b = *(ptr++); result |= (b & 0x7f) <<  7; if (!(b & 0x80)) goto done;
  b = *(ptr++); result |= (b & 0x7f) << 14; if (!(b & 0x80)) goto done;
  b = *(ptr++); result |= (b & 0x7f) << 21; if (!(b & 0x80)) goto done;
  b = *(ptr++); result |=  b         << 28; if (!(b & 0x80)) goto done;

  // if the input is larger than 32 bits, we still need to read it all
  // and discard the high-order bits.
  for (int i = 0; i < kmaxvarintbytes - kmaxvarint32bytes; i++) {
    b = *(ptr++); if (!(b & 0x80)) goto done;
  }

  // we have overrun the maximum size of a varint (10 bytes).  assume
  // the data is corrupt.
  return null;

 done:
  *value = result;
  return ptr;
}

}  // namespace

bool codedinputstream::readvarint32slow(uint32* value) {
  uint64 result;
  // directly invoke readvarint64fallback, since we already tried to optimize
  // for one-byte varints.
  if (!readvarint64fallback(&result)) return false;
  *value = (uint32)result;
  return true;
}

bool codedinputstream::readvarint32fallback(uint32* value) {
  if (buffersize() >= kmaxvarintbytes ||
      // optimization:  if the varint ends at exactly the end of the buffer,
      // we can detect that and still use the fast path.
      (buffer_end_ > buffer_ && !(buffer_end_[-1] & 0x80))) {
    const uint8* end = readvarint32fromarray(buffer_, value);
    if (end == null) return false;
    buffer_ = end;
    return true;
  } else {
    // really slow case: we will incur the cost of an extra function call here,
    // but moving this out of line reduces the size of this function, which
    // improves the common case. in micro benchmarks, this is worth about 10-15%
    return readvarint32slow(value);
  }
}

uint32 codedinputstream::readtagslow() {
  if (buffer_ == buffer_end_) {
    // call refresh.
    if (!refresh()) {
      // refresh failed.  make sure that it failed due to eof, not because
      // we hit total_bytes_limit_, which, unlike normal limits, is not a
      // valid place to end a message.
      int current_position = total_bytes_read_ - buffer_size_after_limit_;
      if (current_position >= total_bytes_limit_) {
        // hit total_bytes_limit_.  but if we also hit the normal limit,
        // we're still ok.
        legitimate_message_end_ = current_limit_ == total_bytes_limit_;
      } else {
        legitimate_message_end_ = true;
      }
      return 0;
    }
  }

  // for the slow path, just do a 64-bit read. try to optimize for one-byte tags
  // again, since we have now refreshed the buffer.
  uint64 result = 0;
  if (!readvarint64(&result)) return 0;
  return static_cast<uint32>(result);
}

uint32 codedinputstream::readtagfallback() {
  const int buf_size = buffersize();
  if (buf_size >= kmaxvarintbytes ||
      // optimization:  if the varint ends at exactly the end of the buffer,
      // we can detect that and still use the fast path.
      (buf_size > 0 && !(buffer_end_[-1] & 0x80))) {
    uint32 tag;
    const uint8* end = readvarint32fromarray(buffer_, &tag);
    if (end == null) {
      return 0;
    }
    buffer_ = end;
    return tag;
  } else {
    // we are commonly at a limit when attempting to read tags. try to quickly
    // detect this case without making another function call.
    if ((buf_size == 0) &&
        ((buffer_size_after_limit_ > 0) ||
         (total_bytes_read_ == current_limit_)) &&
        // make sure that the limit we hit is not total_bytes_limit_, since
        // in that case we still need to call refresh() so that it prints an
        // error.
        total_bytes_read_ - buffer_size_after_limit_ < total_bytes_limit_) {
      // we hit a byte limit.
      legitimate_message_end_ = true;
      return 0;
    }
    return readtagslow();
  }
}

bool codedinputstream::readvarint64slow(uint64* value) {
  // slow path:  this read might cross the end of the buffer, so we
  // need to check and refresh the buffer if and when it does.

  uint64 result = 0;
  int count = 0;
  uint32 b;

  do {
    if (count == kmaxvarintbytes) return false;
    while (buffer_ == buffer_end_) {
      if (!refresh()) return false;
    }
    b = *buffer_;
    result |= static_cast<uint64>(b & 0x7f) << (7 * count);
    advance(1);
    ++count;
  } while (b & 0x80);

  *value = result;
  return true;
}

bool codedinputstream::readvarint64fallback(uint64* value) {
  if (buffersize() >= kmaxvarintbytes ||
      // optimization:  if the varint ends at exactly the end of the buffer,
      // we can detect that and still use the fast path.
      (buffer_end_ > buffer_ && !(buffer_end_[-1] & 0x80))) {
    // fast path:  we have enough bytes left in the buffer to guarantee that
    // this read won't cross the end, so we can skip the checks.

    const uint8* ptr = buffer_;
    uint32 b;

    // splitting into 32-bit pieces gives better performance on 32-bit
    // processors.
    uint32 part0 = 0, part1 = 0, part2 = 0;

    b = *(ptr++); part0  = (b & 0x7f)      ; if (!(b & 0x80)) goto done;
    b = *(ptr++); part0 |= (b & 0x7f) <<  7; if (!(b & 0x80)) goto done;
    b = *(ptr++); part0 |= (b & 0x7f) << 14; if (!(b & 0x80)) goto done;
    b = *(ptr++); part0 |= (b & 0x7f) << 21; if (!(b & 0x80)) goto done;
    b = *(ptr++); part1  = (b & 0x7f)      ; if (!(b & 0x80)) goto done;
    b = *(ptr++); part1 |= (b & 0x7f) <<  7; if (!(b & 0x80)) goto done;
    b = *(ptr++); part1 |= (b & 0x7f) << 14; if (!(b & 0x80)) goto done;
    b = *(ptr++); part1 |= (b & 0x7f) << 21; if (!(b & 0x80)) goto done;
    b = *(ptr++); part2  = (b & 0x7f)      ; if (!(b & 0x80)) goto done;
    b = *(ptr++); part2 |= (b & 0x7f) <<  7; if (!(b & 0x80)) goto done;

    // we have overrun the maximum size of a varint (10 bytes).  the data
    // must be corrupt.
    return false;

   done:
    advance(ptr - buffer_);
    *value = (static_cast<uint64>(part0)      ) |
             (static_cast<uint64>(part1) << 28) |
             (static_cast<uint64>(part2) << 56);
    return true;
  } else {
    return readvarint64slow(value);
  }
}

bool codedinputstream::refresh() {
  google_dcheck_eq(0, buffersize());

  if (buffer_size_after_limit_ > 0 || overflow_bytes_ > 0 ||
      total_bytes_read_ == current_limit_) {
    // we've hit a limit.  stop.
    int current_position = total_bytes_read_ - buffer_size_after_limit_;

    if (current_position >= total_bytes_limit_ &&
        total_bytes_limit_ != current_limit_) {
      // hit total_bytes_limit_.
      printtotalbyteslimiterror();
    }

    return false;
  }

  if (total_bytes_warning_threshold_ >= 0 &&
      total_bytes_read_ >= total_bytes_warning_threshold_) {
      google_log(warning) << "reading dangerously large protocol message.  if the "
                      "message turns out to be larger than "
                   << total_bytes_limit_ << " bytes, parsing will be halted "
                      "for security reasons.  to increase the limit (or to "
                      "disable these warnings), see "
                      "codedinputstream::settotalbyteslimit() in "
                      "google/protobuf/io/coded_stream.h.";

    // don't warn again for this stream, and print total size at the end.
    total_bytes_warning_threshold_ = -2;
  }

  const void* void_buffer;
  int buffer_size;
  if (nextnonempty(input_, &void_buffer, &buffer_size)) {
    buffer_ = reinterpret_cast<const uint8*>(void_buffer);
    buffer_end_ = buffer_ + buffer_size;
    google_check_ge(buffer_size, 0);

    if (total_bytes_read_ <= int_max - buffer_size) {
      total_bytes_read_ += buffer_size;
    } else {
      // overflow.  reset buffer_end_ to not include the bytes beyond int_max.
      // we can't get that far anyway, because total_bytes_limit_ is guaranteed
      // to be less than it.  we need to keep track of the number of bytes
      // we discarded, though, so that we can call input_->backup() to back
      // up over them on destruction.

      // the following line is equivalent to:
      //   overflow_bytes_ = total_bytes_read_ + buffer_size - int_max;
      // except that it avoids overflows.  signed integer overflow has
      // undefined results according to the c standard.
      overflow_bytes_ = total_bytes_read_ - (int_max - buffer_size);
      buffer_end_ -= overflow_bytes_;
      total_bytes_read_ = int_max;
    }

    recomputebufferlimits();
    return true;
  } else {
    buffer_ = null;
    buffer_end_ = null;
    return false;
  }
}

// codedoutputstream =================================================

codedoutputstream::codedoutputstream(zerocopyoutputstream* output)
  : output_(output),
    buffer_(null),
    buffer_size_(0),
    total_bytes_(0),
    had_error_(false) {
  // eagerly refresh() so buffer space is immediately available.
  refresh();
  // the refresh() may have failed. if the client doesn't write any data,
  // though, don't consider this an error. if the client does write data, then
  // another refresh() will be attempted and it will set the error once again.
  had_error_ = false;
}

codedoutputstream::~codedoutputstream() {
  if (buffer_size_ > 0) {
    output_->backup(buffer_size_);
  }
}

bool codedoutputstream::skip(int count) {
  if (count < 0) return false;

  while (count > buffer_size_) {
    count -= buffer_size_;
    if (!refresh()) return false;
  }

  advance(count);
  return true;
}

bool codedoutputstream::getdirectbufferpointer(void** data, int* size) {
  if (buffer_size_ == 0 && !refresh()) return false;

  *data = buffer_;
  *size = buffer_size_;
  return true;
}

void codedoutputstream::writeraw(const void* data, int size) {
  while (buffer_size_ < size) {
    memcpy(buffer_, data, buffer_size_);
    size -= buffer_size_;
    data = reinterpret_cast<const uint8*>(data) + buffer_size_;
    if (!refresh()) return;
  }

  memcpy(buffer_, data, size);
  advance(size);
}

uint8* codedoutputstream::writerawtoarray(
    const void* data, int size, uint8* target) {
  memcpy(target, data, size);
  return target + size;
}


void codedoutputstream::writelittleendian32(uint32 value) {
  uint8 bytes[sizeof(value)];

  bool use_fast = buffer_size_ >= sizeof(value);
  uint8* ptr = use_fast ? buffer_ : bytes;

  writelittleendian32toarray(value, ptr);

  if (use_fast) {
    advance(sizeof(value));
  } else {
    writeraw(bytes, sizeof(value));
  }
}

void codedoutputstream::writelittleendian64(uint64 value) {
  uint8 bytes[sizeof(value)];

  bool use_fast = buffer_size_ >= sizeof(value);
  uint8* ptr = use_fast ? buffer_ : bytes;

  writelittleendian64toarray(value, ptr);

  if (use_fast) {
    advance(sizeof(value));
  } else {
    writeraw(bytes, sizeof(value));
  }
}

inline uint8* codedoutputstream::writevarint32fallbacktoarrayinline(
    uint32 value, uint8* target) {
  target[0] = static_cast<uint8>(value | 0x80);
  if (value >= (1 << 7)) {
    target[1] = static_cast<uint8>((value >>  7) | 0x80);
    if (value >= (1 << 14)) {
      target[2] = static_cast<uint8>((value >> 14) | 0x80);
      if (value >= (1 << 21)) {
        target[3] = static_cast<uint8>((value >> 21) | 0x80);
        if (value >= (1 << 28)) {
          target[4] = static_cast<uint8>(value >> 28);
          return target + 5;
        } else {
          target[3] &= 0x7f;
          return target + 4;
        }
      } else {
        target[2] &= 0x7f;
        return target + 3;
      }
    } else {
      target[1] &= 0x7f;
      return target + 2;
    }
  } else {
    target[0] &= 0x7f;
    return target + 1;
  }
}

void codedoutputstream::writevarint32(uint32 value) {
  if (buffer_size_ >= kmaxvarint32bytes) {
    // fast path:  we have enough bytes left in the buffer to guarantee that
    // this write won't cross the end, so we can skip the checks.
    uint8* target = buffer_;
    uint8* end = writevarint32fallbacktoarrayinline(value, target);
    int size = end - target;
    advance(size);
  } else {
    // slow path:  this write might cross the end of the buffer, so we
    // compose the bytes first then use writeraw().
    uint8 bytes[kmaxvarint32bytes];
    int size = 0;
    while (value > 0x7f) {
      bytes[size++] = (static_cast<uint8>(value) & 0x7f) | 0x80;
      value >>= 7;
    }
    bytes[size++] = static_cast<uint8>(value) & 0x7f;
    writeraw(bytes, size);
  }
}

uint8* codedoutputstream::writevarint32fallbacktoarray(
    uint32 value, uint8* target) {
  return writevarint32fallbacktoarrayinline(value, target);
}

inline uint8* codedoutputstream::writevarint64toarrayinline(
    uint64 value, uint8* target) {
  // splitting into 32-bit pieces gives better performance on 32-bit
  // processors.
  uint32 part0 = static_cast<uint32>(value      );
  uint32 part1 = static_cast<uint32>(value >> 28);
  uint32 part2 = static_cast<uint32>(value >> 56);

  int size;

  // here we can't really optimize for small numbers, since the value is
  // split into three parts.  cheking for numbers < 128, for instance,
  // would require three comparisons, since you'd have to make sure part1
  // and part2 are zero.  however, if the caller is using 64-bit integers,
  // it is likely that they expect the numbers to often be very large, so
  // we probably don't want to optimize for small numbers anyway.  thus,
  // we end up with a hardcoded binary search tree...
  if (part2 == 0) {
    if (part1 == 0) {
      if (part0 < (1 << 14)) {
        if (part0 < (1 << 7)) {
          size = 1; goto size1;
        } else {
          size = 2; goto size2;
        }
      } else {
        if (part0 < (1 << 21)) {
          size = 3; goto size3;
        } else {
          size = 4; goto size4;
        }
      }
    } else {
      if (part1 < (1 << 14)) {
        if (part1 < (1 << 7)) {
          size = 5; goto size5;
        } else {
          size = 6; goto size6;
        }
      } else {
        if (part1 < (1 << 21)) {
          size = 7; goto size7;
        } else {
          size = 8; goto size8;
        }
      }
    }
  } else {
    if (part2 < (1 << 7)) {
      size = 9; goto size9;
    } else {
      size = 10; goto size10;
    }
  }

  google_log(fatal) << "can't get here.";

  size10: target[9] = static_cast<uint8>((part2 >>  7) | 0x80);
  size9 : target[8] = static_cast<uint8>((part2      ) | 0x80);
  size8 : target[7] = static_cast<uint8>((part1 >> 21) | 0x80);
  size7 : target[6] = static_cast<uint8>((part1 >> 14) | 0x80);
  size6 : target[5] = static_cast<uint8>((part1 >>  7) | 0x80);
  size5 : target[4] = static_cast<uint8>((part1      ) | 0x80);
  size4 : target[3] = static_cast<uint8>((part0 >> 21) | 0x80);
  size3 : target[2] = static_cast<uint8>((part0 >> 14) | 0x80);
  size2 : target[1] = static_cast<uint8>((part0 >>  7) | 0x80);
  size1 : target[0] = static_cast<uint8>((part0      ) | 0x80);

  target[size-1] &= 0x7f;
  return target + size;
}

void codedoutputstream::writevarint64(uint64 value) {
  if (buffer_size_ >= kmaxvarintbytes) {
    // fast path:  we have enough bytes left in the buffer to guarantee that
    // this write won't cross the end, so we can skip the checks.
    uint8* target = buffer_;

    uint8* end = writevarint64toarrayinline(value, target);
    int size = end - target;
    advance(size);
  } else {
    // slow path:  this write might cross the end of the buffer, so we
    // compose the bytes first then use writeraw().
    uint8 bytes[kmaxvarintbytes];
    int size = 0;
    while (value > 0x7f) {
      bytes[size++] = (static_cast<uint8>(value) & 0x7f) | 0x80;
      value >>= 7;
    }
    bytes[size++] = static_cast<uint8>(value) & 0x7f;
    writeraw(bytes, size);
  }
}

uint8* codedoutputstream::writevarint64toarray(
    uint64 value, uint8* target) {
  return writevarint64toarrayinline(value, target);
}

bool codedoutputstream::refresh() {
  void* void_buffer;
  if (output_->next(&void_buffer, &buffer_size_)) {
    buffer_ = reinterpret_cast<uint8*>(void_buffer);
    total_bytes_ += buffer_size_;
    return true;
  } else {
    buffer_ = null;
    buffer_size_ = 0;
    had_error_ = true;
    return false;
  }
}

int codedoutputstream::varintsize32fallback(uint32 value) {
  if (value < (1 << 7)) {
    return 1;
  } else if (value < (1 << 14)) {
    return 2;
  } else if (value < (1 << 21)) {
    return 3;
  } else if (value < (1 << 28)) {
    return 4;
  } else {
    return 5;
  }
}

int codedoutputstream::varintsize64(uint64 value) {
  if (value < (1ull << 35)) {
    if (value < (1ull << 7)) {
      return 1;
    } else if (value < (1ull << 14)) {
      return 2;
    } else if (value < (1ull << 21)) {
      return 3;
    } else if (value < (1ull << 28)) {
      return 4;
    } else {
      return 5;
    }
  } else {
    if (value < (1ull << 42)) {
      return 6;
    } else if (value < (1ull << 49)) {
      return 7;
    } else if (value < (1ull << 56)) {
      return 8;
    } else if (value < (1ull << 63)) {
      return 9;
    } else {
      return 10;
    }
  }
}

}  // namespace io
}  // namespace protobuf
}  // namespace google
