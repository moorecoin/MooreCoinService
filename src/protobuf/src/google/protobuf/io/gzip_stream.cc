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

// author: brianolson@google.com (brian olson)
//
// this file contains the implementation of classes gzipinputstream and
// gzipoutputstream.

#include "config.h"

#if have_zlib
#include <google/protobuf/io/gzip_stream.h>

#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
namespace io {

static const int kdefaultbuffersize = 65536;

gzipinputstream::gzipinputstream(
    zerocopyinputstream* sub_stream, format format, int buffer_size)
    : format_(format), sub_stream_(sub_stream), zerror_(z_ok) {
  zcontext_.zalloc = z_null;
  zcontext_.zfree = z_null;
  zcontext_.opaque = z_null;
  zcontext_.total_out = 0;
  zcontext_.next_in = null;
  zcontext_.avail_in = 0;
  zcontext_.total_in = 0;
  zcontext_.msg = null;
  if (buffer_size == -1) {
    output_buffer_length_ = kdefaultbuffersize;
  } else {
    output_buffer_length_ = buffer_size;
  }
  output_buffer_ = operator new(output_buffer_length_);
  google_check(output_buffer_ != null);
  zcontext_.next_out = static_cast<bytef*>(output_buffer_);
  zcontext_.avail_out = output_buffer_length_;
  output_position_ = output_buffer_;
}
gzipinputstream::~gzipinputstream() {
  operator delete(output_buffer_);
  zerror_ = inflateend(&zcontext_);
}

static inline int internalinflateinit2(
    z_stream* zcontext, gzipinputstream::format format) {
  int windowbitsformat = 0;
  switch (format) {
    case gzipinputstream::gzip: windowbitsformat = 16; break;
    case gzipinputstream::auto: windowbitsformat = 32; break;
    case gzipinputstream::zlib: windowbitsformat = 0; break;
  }
  return inflateinit2(zcontext, /* windowbits */15 | windowbitsformat);
}

int gzipinputstream::inflate(int flush) {
  if ((zerror_ == z_ok) && (zcontext_.avail_out == 0)) {
    // previous inflate filled output buffer. don't change input params yet.
  } else if (zcontext_.avail_in == 0) {
    const void* in;
    int in_size;
    bool first = zcontext_.next_in == null;
    bool ok = sub_stream_->next(&in, &in_size);
    if (!ok) {
      zcontext_.next_out = null;
      zcontext_.avail_out = 0;
      return z_stream_end;
    }
    zcontext_.next_in = static_cast<bytef*>(const_cast<void*>(in));
    zcontext_.avail_in = in_size;
    if (first) {
      int error = internalinflateinit2(&zcontext_, format_);
      if (error != z_ok) {
        return error;
      }
    }
  }
  zcontext_.next_out = static_cast<bytef*>(output_buffer_);
  zcontext_.avail_out = output_buffer_length_;
  output_position_ = output_buffer_;
  int error = inflate(&zcontext_, flush);
  return error;
}

void gzipinputstream::donextoutput(const void** data, int* size) {
  *data = output_position_;
  *size = ((uintptr_t)zcontext_.next_out) - ((uintptr_t)output_position_);
  output_position_ = zcontext_.next_out;
}

// implements zerocopyinputstream ----------------------------------
bool gzipinputstream::next(const void** data, int* size) {
  bool ok = (zerror_ == z_ok) || (zerror_ == z_stream_end)
      || (zerror_ == z_buf_error);
  if ((!ok) || (zcontext_.next_out == null)) {
    return false;
  }
  if (zcontext_.next_out != output_position_) {
    donextoutput(data, size);
    return true;
  }
  if (zerror_ == z_stream_end) {
    if (zcontext_.next_out != null) {
      // sub_stream_ may have concatenated streams to follow
      zerror_ = inflateend(&zcontext_);
      if (zerror_ != z_ok) {
        return false;
      }
      zerror_ = internalinflateinit2(&zcontext_, format_);
      if (zerror_ != z_ok) {
        return false;
      }
    } else {
      *data = null;
      *size = 0;
      return false;
    }
  }
  zerror_ = inflate(z_no_flush);
  if ((zerror_ == z_stream_end) && (zcontext_.next_out == null)) {
    // the underlying stream's next returned false inside inflate.
    return false;
  }
  ok = (zerror_ == z_ok) || (zerror_ == z_stream_end)
      || (zerror_ == z_buf_error);
  if (!ok) {
    return false;
  }
  donextoutput(data, size);
  return true;
}
void gzipinputstream::backup(int count) {
  output_position_ = reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(output_position_) - count);
}
bool gzipinputstream::skip(int count) {
  const void* data;
  int size;
  bool ok = next(&data, &size);
  while (ok && (size < count)) {
    count -= size;
    ok = next(&data, &size);
  }
  if (size > count) {
    backup(size - count);
  }
  return ok;
}
int64 gzipinputstream::bytecount() const {
  return zcontext_.total_out +
    (((uintptr_t)zcontext_.next_out) - ((uintptr_t)output_position_));
}

// =========================================================================

gzipoutputstream::options::options()
    : format(gzip),
      buffer_size(kdefaultbuffersize),
      compression_level(z_default_compression),
      compression_strategy(z_default_strategy) {}

gzipoutputstream::gzipoutputstream(zerocopyoutputstream* sub_stream) {
  init(sub_stream, options());
}

gzipoutputstream::gzipoutputstream(zerocopyoutputstream* sub_stream,
                                   const options& options) {
  init(sub_stream, options);
}

void gzipoutputstream::init(zerocopyoutputstream* sub_stream,
                            const options& options) {
  sub_stream_ = sub_stream;
  sub_data_ = null;
  sub_data_size_ = 0;

  input_buffer_length_ = options.buffer_size;
  input_buffer_ = operator new(input_buffer_length_);
  google_check(input_buffer_ != null);

  zcontext_.zalloc = z_null;
  zcontext_.zfree = z_null;
  zcontext_.opaque = z_null;
  zcontext_.next_out = null;
  zcontext_.avail_out = 0;
  zcontext_.total_out = 0;
  zcontext_.next_in = null;
  zcontext_.avail_in = 0;
  zcontext_.total_in = 0;
  zcontext_.msg = null;
  // default to gzip format
  int windowbitsformat = 16;
  if (options.format == zlib) {
    windowbitsformat = 0;
  }
  zerror_ = deflateinit2(
      &zcontext_,
      options.compression_level,
      z_deflated,
      /* windowbits */15 | windowbitsformat,
      /* memlevel (default) */8,
      options.compression_strategy);
}

gzipoutputstream::~gzipoutputstream() {
  close();
  if (input_buffer_ != null) {
    operator delete(input_buffer_);
  }
}

// private
int gzipoutputstream::deflate(int flush) {
  int error = z_ok;
  do {
    if ((sub_data_ == null) || (zcontext_.avail_out == 0)) {
      bool ok = sub_stream_->next(&sub_data_, &sub_data_size_);
      if (!ok) {
        sub_data_ = null;
        sub_data_size_ = 0;
        return z_buf_error;
      }
      google_check_gt(sub_data_size_, 0);
      zcontext_.next_out = static_cast<bytef*>(sub_data_);
      zcontext_.avail_out = sub_data_size_;
    }
    error = deflate(&zcontext_, flush);
  } while (error == z_ok && zcontext_.avail_out == 0);
  if ((flush == z_full_flush) || (flush == z_finish)) {
    // notify lower layer of data.
    sub_stream_->backup(zcontext_.avail_out);
    // we don't own the buffer anymore.
    sub_data_ = null;
    sub_data_size_ = 0;
  }
  return error;
}

// implements zerocopyoutputstream ---------------------------------
bool gzipoutputstream::next(void** data, int* size) {
  if ((zerror_ != z_ok) && (zerror_ != z_buf_error)) {
    return false;
  }
  if (zcontext_.avail_in != 0) {
    zerror_ = deflate(z_no_flush);
    if (zerror_ != z_ok) {
      return false;
    }
  }
  if (zcontext_.avail_in == 0) {
    // all input was consumed. reset the buffer.
    zcontext_.next_in = static_cast<bytef*>(input_buffer_);
    zcontext_.avail_in = input_buffer_length_;
    *data = input_buffer_;
    *size = input_buffer_length_;
  } else {
    // the loop in deflate should consume all avail_in
    google_log(dfatal) << "deflate left bytes unconsumed";
  }
  return true;
}
void gzipoutputstream::backup(int count) {
  google_check_ge(zcontext_.avail_in, count);
  zcontext_.avail_in -= count;
}
int64 gzipoutputstream::bytecount() const {
  return zcontext_.total_in + zcontext_.avail_in;
}

bool gzipoutputstream::flush() {
  zerror_ = deflate(z_full_flush);
  // return true if the flush succeeded or if it was a no-op.
  return  (zerror_ == z_ok) ||
      (zerror_ == z_buf_error && zcontext_.avail_in == 0 &&
       zcontext_.avail_out != 0);
}

bool gzipoutputstream::close() {
  if ((zerror_ != z_ok) && (zerror_ != z_buf_error)) {
    return false;
  }
  do {
    zerror_ = deflate(z_finish);
  } while (zerror_ == z_ok);
  zerror_ = deflateend(&zcontext_);
  bool ok = zerror_ == z_ok;
  zerror_ = z_stream_end;
  return ok;
}

}  // namespace io
}  // namespace protobuf
}  // namespace google

#endif  // have_zlib
