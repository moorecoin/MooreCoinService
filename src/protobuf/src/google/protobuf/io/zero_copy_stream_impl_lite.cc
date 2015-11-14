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

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
namespace protobuf {
namespace io {

namespace {

// default block size for copying{in,out}putstreamadaptor.
static const int kdefaultblocksize = 8192;

}  // namespace

// ===================================================================

arrayinputstream::arrayinputstream(const void* data, int size,
                                   int block_size)
  : data_(reinterpret_cast<const uint8*>(data)),
    size_(size),
    block_size_(block_size > 0 ? block_size : size),
    position_(0),
    last_returned_size_(0) {
}

arrayinputstream::~arrayinputstream() {
}

bool arrayinputstream::next(const void** data, int* size) {
  if (position_ < size_) {
    last_returned_size_ = min(block_size_, size_ - position_);
    *data = data_ + position_;
    *size = last_returned_size_;
    position_ += last_returned_size_;
    return true;
  } else {
    // we're at the end of the array.
    last_returned_size_ = 0;   // don't let caller back up.
    return false;
  }
}

void arrayinputstream::backup(int count) {
  google_check_gt(last_returned_size_, 0)
      << "backup() can only be called after a successful next().";
  google_check_le(count, last_returned_size_);
  google_check_ge(count, 0);
  position_ -= count;
  last_returned_size_ = 0;  // don't let caller back up further.
}

bool arrayinputstream::skip(int count) {
  google_check_ge(count, 0);
  last_returned_size_ = 0;   // don't let caller back up.
  if (count > size_ - position_) {
    position_ = size_;
    return false;
  } else {
    position_ += count;
    return true;
  }
}

int64 arrayinputstream::bytecount() const {
  return position_;
}


// ===================================================================

arrayoutputstream::arrayoutputstream(void* data, int size, int block_size)
  : data_(reinterpret_cast<uint8*>(data)),
    size_(size),
    block_size_(block_size > 0 ? block_size : size),
    position_(0),
    last_returned_size_(0) {
}

arrayoutputstream::~arrayoutputstream() {
}

bool arrayoutputstream::next(void** data, int* size) {
  if (position_ < size_) {
    last_returned_size_ = min(block_size_, size_ - position_);
    *data = data_ + position_;
    *size = last_returned_size_;
    position_ += last_returned_size_;
    return true;
  } else {
    // we're at the end of the array.
    last_returned_size_ = 0;   // don't let caller back up.
    return false;
  }
}

void arrayoutputstream::backup(int count) {
  google_check_gt(last_returned_size_, 0)
      << "backup() can only be called after a successful next().";
  google_check_le(count, last_returned_size_);
  google_check_ge(count, 0);
  position_ -= count;
  last_returned_size_ = 0;  // don't let caller back up further.
}

int64 arrayoutputstream::bytecount() const {
  return position_;
}

// ===================================================================

stringoutputstream::stringoutputstream(string* target)
  : target_(target) {
}

stringoutputstream::~stringoutputstream() {
}

bool stringoutputstream::next(void** data, int* size) {
  int old_size = target_->size();

  // grow the string.
  if (old_size < target_->capacity()) {
    // resize the string to match its capacity, since we can get away
    // without a memory allocation this way.
    stlstringresizeuninitialized(target_, target_->capacity());
  } else {
    // size has reached capacity, so double the size.  also make sure
    // that the new size is at least kminimumsize.
    stlstringresizeuninitialized(
      target_,
      max(old_size * 2,
          kminimumsize + 0));  // "+ 0" works around gcc4 weirdness.
  }

  *data = string_as_array(target_) + old_size;
  *size = target_->size() - old_size;
  return true;
}

void stringoutputstream::backup(int count) {
  google_check_ge(count, 0);
  google_check_le(count, target_->size());
  target_->resize(target_->size() - count);
}

int64 stringoutputstream::bytecount() const {
  return target_->size();
}

// ===================================================================

copyinginputstream::~copyinginputstream() {}

int copyinginputstream::skip(int count) {
  char junk[4096];
  int skipped = 0;
  while (skipped < count) {
    int bytes = read(junk, min(count - skipped,
                               implicit_cast<int>(sizeof(junk))));
    if (bytes <= 0) {
      // eof or read error.
      return skipped;
    }
    skipped += bytes;
  }
  return skipped;
}

copyinginputstreamadaptor::copyinginputstreamadaptor(
    copyinginputstream* copying_stream, int block_size)
  : copying_stream_(copying_stream),
    owns_copying_stream_(false),
    failed_(false),
    position_(0),
    buffer_size_(block_size > 0 ? block_size : kdefaultblocksize),
    buffer_used_(0),
    backup_bytes_(0) {
}

copyinginputstreamadaptor::~copyinginputstreamadaptor() {
  if (owns_copying_stream_) {
    delete copying_stream_;
  }
}

bool copyinginputstreamadaptor::next(const void** data, int* size) {
  if (failed_) {
    // already failed on a previous read.
    return false;
  }

  allocatebufferifneeded();

  if (backup_bytes_ > 0) {
    // we have data left over from a previous backup(), so just return that.
    *data = buffer_.get() + buffer_used_ - backup_bytes_;
    *size = backup_bytes_;
    backup_bytes_ = 0;
    return true;
  }

  // read new data into the buffer.
  buffer_used_ = copying_stream_->read(buffer_.get(), buffer_size_);
  if (buffer_used_ <= 0) {
    // eof or read error.  we don't need the buffer anymore.
    if (buffer_used_ < 0) {
      // read error (not eof).
      failed_ = true;
    }
    freebuffer();
    return false;
  }
  position_ += buffer_used_;

  *size = buffer_used_;
  *data = buffer_.get();
  return true;
}

void copyinginputstreamadaptor::backup(int count) {
  google_check(backup_bytes_ == 0 && buffer_.get() != null)
    << " backup() can only be called after next().";
  google_check_le(count, buffer_used_)
    << " can't back up over more bytes than were returned by the last call"
       " to next().";
  google_check_ge(count, 0)
    << " parameter to backup() can't be negative.";

  backup_bytes_ = count;
}

bool copyinginputstreamadaptor::skip(int count) {
  google_check_ge(count, 0);

  if (failed_) {
    // already failed on a previous read.
    return false;
  }

  // first skip any bytes left over from a previous backup().
  if (backup_bytes_ >= count) {
    // we have more data left over than we're trying to skip.  just chop it.
    backup_bytes_ -= count;
    return true;
  }

  count -= backup_bytes_;
  backup_bytes_ = 0;

  int skipped = copying_stream_->skip(count);
  position_ += skipped;
  return skipped == count;
}

int64 copyinginputstreamadaptor::bytecount() const {
  return position_ - backup_bytes_;
}

void copyinginputstreamadaptor::allocatebufferifneeded() {
  if (buffer_.get() == null) {
    buffer_.reset(new uint8[buffer_size_]);
  }
}

void copyinginputstreamadaptor::freebuffer() {
  google_check_eq(backup_bytes_, 0);
  buffer_used_ = 0;
  buffer_.reset();
}

// ===================================================================

copyingoutputstream::~copyingoutputstream() {}

copyingoutputstreamadaptor::copyingoutputstreamadaptor(
    copyingoutputstream* copying_stream, int block_size)
  : copying_stream_(copying_stream),
    owns_copying_stream_(false),
    failed_(false),
    position_(0),
    buffer_size_(block_size > 0 ? block_size : kdefaultblocksize),
    buffer_used_(0) {
}

copyingoutputstreamadaptor::~copyingoutputstreamadaptor() {
  writebuffer();
  if (owns_copying_stream_) {
    delete copying_stream_;
  }
}

bool copyingoutputstreamadaptor::flush() {
  return writebuffer();
}

bool copyingoutputstreamadaptor::next(void** data, int* size) {
  if (buffer_used_ == buffer_size_) {
    if (!writebuffer()) return false;
  }

  allocatebufferifneeded();

  *data = buffer_.get() + buffer_used_;
  *size = buffer_size_ - buffer_used_;
  buffer_used_ = buffer_size_;
  return true;
}

void copyingoutputstreamadaptor::backup(int count) {
  google_check_ge(count, 0);
  google_check_eq(buffer_used_, buffer_size_)
    << " backup() can only be called after next().";
  google_check_le(count, buffer_used_)
    << " can't back up over more bytes than were returned by the last call"
       " to next().";

  buffer_used_ -= count;
}

int64 copyingoutputstreamadaptor::bytecount() const {
  return position_ + buffer_used_;
}

bool copyingoutputstreamadaptor::writebuffer() {
  if (failed_) {
    // already failed on a previous write.
    return false;
  }

  if (buffer_used_ == 0) return true;

  if (copying_stream_->write(buffer_.get(), buffer_used_)) {
    position_ += buffer_used_;
    buffer_used_ = 0;
    return true;
  } else {
    failed_ = true;
    freebuffer();
    return false;
  }
}

void copyingoutputstreamadaptor::allocatebufferifneeded() {
  if (buffer_ == null) {
    buffer_.reset(new uint8[buffer_size_]);
  }
}

void copyingoutputstreamadaptor::freebuffer() {
  buffer_used_ = 0;
  buffer_.reset();
}

// ===================================================================

}  // namespace io
}  // namespace protobuf
}  // namespace google
