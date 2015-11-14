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

#ifdef _msc_ver
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif
#include <errno.h>
#include <iostream>
#include <algorithm>

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stl_util.h>


namespace google {
namespace protobuf {
namespace io {

#ifdef _win32
// win32 lseek is broken:  if invoked on a non-seekable file descriptor, its
// return value is undefined.  we re-define it to always produce an error.
#define lseek(fd, offset, origin) ((off_t)-1)
#endif

namespace {

// eintr sucks.
int close_no_eintr(int fd) {
  int result;
  do {
    result = close(fd);
  } while (result < 0 && errno == eintr);
  return result;
}

}  // namespace


// ===================================================================

fileinputstream::fileinputstream(int file_descriptor, int block_size)
  : copying_input_(file_descriptor),
    impl_(&copying_input_, block_size) {
}

fileinputstream::~fileinputstream() {}

bool fileinputstream::close() {
  return copying_input_.close();
}

bool fileinputstream::next(const void** data, int* size) {
  return impl_.next(data, size);
}

void fileinputstream::backup(int count) {
  impl_.backup(count);
}

bool fileinputstream::skip(int count) {
  return impl_.skip(count);
}

int64 fileinputstream::bytecount() const {
  return impl_.bytecount();
}

fileinputstream::copyingfileinputstream::copyingfileinputstream(
    int file_descriptor)
  : file_(file_descriptor),
    close_on_delete_(false),
    is_closed_(false),
    errno_(0),
    previous_seek_failed_(false) {
}

fileinputstream::copyingfileinputstream::~copyingfileinputstream() {
  if (close_on_delete_) {
    if (!close()) {
      google_log(error) << "close() failed: " << strerror(errno_);
    }
  }
}

bool fileinputstream::copyingfileinputstream::close() {
  google_check(!is_closed_);

  is_closed_ = true;
  if (close_no_eintr(file_) != 0) {
    // the docs on close() do not specify whether a file descriptor is still
    // open after close() fails with eio.  however, the glibc source code
    // seems to indicate that it is not.
    errno_ = errno;
    return false;
  }

  return true;
}

int fileinputstream::copyingfileinputstream::read(void* buffer, int size) {
  google_check(!is_closed_);

  int result;
  do {
    result = read(file_, buffer, size);
  } while (result < 0 && errno == eintr);

  if (result < 0) {
    // read error (not eof).
    errno_ = errno;
  }

  return result;
}

int fileinputstream::copyingfileinputstream::skip(int count) {
  google_check(!is_closed_);

  if (!previous_seek_failed_ &&
      lseek(file_, count, seek_cur) != (off_t)-1) {
    // seek succeeded.
    return count;
  } else {
    // failed to seek.

    // note to self:  don't seek again.  this file descriptor doesn't
    // support it.
    previous_seek_failed_ = true;

    // use the default implementation.
    return copyinginputstream::skip(count);
  }
}

// ===================================================================

fileoutputstream::fileoutputstream(int file_descriptor, int block_size)
  : copying_output_(file_descriptor),
    impl_(&copying_output_, block_size) {
}

fileoutputstream::~fileoutputstream() {
  impl_.flush();
}

bool fileoutputstream::close() {
  bool flush_succeeded = impl_.flush();
  return copying_output_.close() && flush_succeeded;
}

bool fileoutputstream::flush() {
  return impl_.flush();
}

bool fileoutputstream::next(void** data, int* size) {
  return impl_.next(data, size);
}

void fileoutputstream::backup(int count) {
  impl_.backup(count);
}

int64 fileoutputstream::bytecount() const {
  return impl_.bytecount();
}

fileoutputstream::copyingfileoutputstream::copyingfileoutputstream(
    int file_descriptor)
  : file_(file_descriptor),
    close_on_delete_(false),
    is_closed_(false),
    errno_(0) {
}

fileoutputstream::copyingfileoutputstream::~copyingfileoutputstream() {
  if (close_on_delete_) {
    if (!close()) {
      google_log(error) << "close() failed: " << strerror(errno_);
    }
  }
}

bool fileoutputstream::copyingfileoutputstream::close() {
  google_check(!is_closed_);

  is_closed_ = true;
  if (close_no_eintr(file_) != 0) {
    // the docs on close() do not specify whether a file descriptor is still
    // open after close() fails with eio.  however, the glibc source code
    // seems to indicate that it is not.
    errno_ = errno;
    return false;
  }

  return true;
}

bool fileoutputstream::copyingfileoutputstream::write(
    const void* buffer, int size) {
  google_check(!is_closed_);
  int total_written = 0;

  const uint8* buffer_base = reinterpret_cast<const uint8*>(buffer);

  while (total_written < size) {
    int bytes;
    do {
      bytes = write(file_, buffer_base + total_written, size - total_written);
    } while (bytes < 0 && errno == eintr);

    if (bytes <= 0) {
      // write error.

      // fixme(kenton):  according to the man page, if write() returns zero,
      //   there was no error; write() simply did not write anything.  it's
      //   unclear under what circumstances this might happen, but presumably
      //   errno won't be set in this case.  i am confused as to how such an
      //   event should be handled.  for now i'm treating it as an error, since
      //   retrying seems like it could lead to an infinite loop.  i suspect
      //   this never actually happens anyway.

      if (bytes < 0) {
        errno_ = errno;
      }
      return false;
    }
    total_written += bytes;
  }

  return true;
}

// ===================================================================

istreaminputstream::istreaminputstream(istream* input, int block_size)
  : copying_input_(input),
    impl_(&copying_input_, block_size) {
}

istreaminputstream::~istreaminputstream() {}

bool istreaminputstream::next(const void** data, int* size) {
  return impl_.next(data, size);
}

void istreaminputstream::backup(int count) {
  impl_.backup(count);
}

bool istreaminputstream::skip(int count) {
  return impl_.skip(count);
}

int64 istreaminputstream::bytecount() const {
  return impl_.bytecount();
}

istreaminputstream::copyingistreaminputstream::copyingistreaminputstream(
    istream* input)
  : input_(input) {
}

istreaminputstream::copyingistreaminputstream::~copyingistreaminputstream() {}

int istreaminputstream::copyingistreaminputstream::read(
    void* buffer, int size) {
  input_->read(reinterpret_cast<char*>(buffer), size);
  int result = input_->gcount();
  if (result == 0 && input_->fail() && !input_->eof()) {
    return -1;
  }
  return result;
}

// ===================================================================

ostreamoutputstream::ostreamoutputstream(ostream* output, int block_size)
  : copying_output_(output),
    impl_(&copying_output_, block_size) {
}

ostreamoutputstream::~ostreamoutputstream() {
  impl_.flush();
}

bool ostreamoutputstream::next(void** data, int* size) {
  return impl_.next(data, size);
}

void ostreamoutputstream::backup(int count) {
  impl_.backup(count);
}

int64 ostreamoutputstream::bytecount() const {
  return impl_.bytecount();
}

ostreamoutputstream::copyingostreamoutputstream::copyingostreamoutputstream(
    ostream* output)
  : output_(output) {
}

ostreamoutputstream::copyingostreamoutputstream::~copyingostreamoutputstream() {
}

bool ostreamoutputstream::copyingostreamoutputstream::write(
    const void* buffer, int size) {
  output_->write(reinterpret_cast<const char*>(buffer), size);
  return output_->good();
}

// ===================================================================

concatenatinginputstream::concatenatinginputstream(
    zerocopyinputstream* const streams[], int count)
  : streams_(streams), stream_count_(count), bytes_retired_(0) {
}

concatenatinginputstream::~concatenatinginputstream() {
}

bool concatenatinginputstream::next(const void** data, int* size) {
  while (stream_count_ > 0) {
    if (streams_[0]->next(data, size)) return true;

    // that stream is done.  advance to the next one.
    bytes_retired_ += streams_[0]->bytecount();
    ++streams_;
    --stream_count_;
  }

  // no more streams.
  return false;
}

void concatenatinginputstream::backup(int count) {
  if (stream_count_ > 0) {
    streams_[0]->backup(count);
  } else {
    google_log(dfatal) << "can't backup() after failed next().";
  }
}

bool concatenatinginputstream::skip(int count) {
  while (stream_count_ > 0) {
    // assume that bytecount() can be used to find out how much we actually
    // skipped when skip() fails.
    int64 target_byte_count = streams_[0]->bytecount() + count;
    if (streams_[0]->skip(count)) return true;

    // hit the end of the stream.  figure out how many more bytes we still have
    // to skip.
    int64 final_byte_count = streams_[0]->bytecount();
    google_dcheck_lt(final_byte_count, target_byte_count);
    count = target_byte_count - final_byte_count;

    // that stream is done.  advance to the next one.
    bytes_retired_ += final_byte_count;
    ++streams_;
    --stream_count_;
  }

  return false;
}

int64 concatenatinginputstream::bytecount() const {
  if (stream_count_ == 0) {
    return bytes_retired_;
  } else {
    return bytes_retired_ + streams_[0]->bytecount();
  }
}


// ===================================================================

limitinginputstream::limitinginputstream(zerocopyinputstream* input,
                                         int64 limit)
  : input_(input), limit_(limit) {}

limitinginputstream::~limitinginputstream() {
  // if we overshot the limit, back up.
  if (limit_ < 0) input_->backup(-limit_);
}

bool limitinginputstream::next(const void** data, int* size) {
  if (limit_ <= 0) return false;
  if (!input_->next(data, size)) return false;

  limit_ -= *size;
  if (limit_ < 0) {
    // we overshot the limit.  reduce *size to hide the rest of the buffer.
    *size += limit_;
  }
  return true;
}

void limitinginputstream::backup(int count) {
  if (limit_ < 0) {
    input_->backup(count - limit_);
    limit_ = count;
  } else {
    input_->backup(count);
    limit_ += count;
  }
}

bool limitinginputstream::skip(int count) {
  if (count > limit_) {
    if (limit_ < 0) return false;
    input_->skip(limit_);
    limit_ = 0;
    return false;
  } else {
    if (!input_->skip(count)) return false;
    limit_ -= count;
    return true;
  }
}

int64 limitinginputstream::bytecount() const {
  if (limit_ < 0) {
    return input_->bytecount() + limit_;
  } else {
    return input_->bytecount();
  }
}


// ===================================================================

}  // namespace io
}  // namespace protobuf
}  // namespace google
