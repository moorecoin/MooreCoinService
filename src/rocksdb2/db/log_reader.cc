//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/log_reader.h"

#include <stdio.h>
#include "rocksdb/env.h"
#include "util/coding.h"
#include "util/crc32c.h"

namespace rocksdb {
namespace log {

reader::reporter::~reporter() {
}

reader::reader(unique_ptr<sequentialfile>&& file, reporter* reporter,
               bool checksum, uint64_t initial_offset)
    : file_(std::move(file)),
      reporter_(reporter),
      checksum_(checksum),
      backing_store_(new char[kblocksize]),
      buffer_(),
      eof_(false),
      read_error_(false),
      eof_offset_(0),
      last_record_offset_(0),
      end_of_buffer_offset_(0),
      initial_offset_(initial_offset) {
}

reader::~reader() {
  delete[] backing_store_;
}

bool reader::skiptoinitialblock() {
  size_t offset_in_block = initial_offset_ % kblocksize;
  uint64_t block_start_location = initial_offset_ - offset_in_block;

  // don't search a block if we'd be in the trailer
  if (offset_in_block > kblocksize - 6) {
    offset_in_block = 0;
    block_start_location += kblocksize;
  }

  end_of_buffer_offset_ = block_start_location;

  // skip to start of first block that can contain the initial record
  if (block_start_location > 0) {
    status skip_status = file_->skip(block_start_location);
    if (!skip_status.ok()) {
      reportdrop(block_start_location, skip_status);
      return false;
    }
  }

  return true;
}

bool reader::readrecord(slice* record, std::string* scratch) {
  if (last_record_offset_ < initial_offset_) {
    if (!skiptoinitialblock()) {
      return false;
    }
  }

  scratch->clear();
  record->clear();
  bool in_fragmented_record = false;
  // record offset of the logical record that we're reading
  // 0 is a dummy value to make compilers happy
  uint64_t prospective_record_offset = 0;

  slice fragment;
  while (true) {
    uint64_t physical_record_offset = end_of_buffer_offset_ - buffer_.size();
    const unsigned int record_type = readphysicalrecord(&fragment);
    switch (record_type) {
      case kfulltype:
        if (in_fragmented_record) {
          // handle bug in earlier versions of log::writer where
          // it could emit an empty kfirsttype record at the tail end
          // of a block followed by a kfulltype or kfirsttype record
          // at the beginning of the next block.
          if (scratch->empty()) {
            in_fragmented_record = false;
          } else {
            reportcorruption(scratch->size(), "partial record without end(1)");
          }
        }
        prospective_record_offset = physical_record_offset;
        scratch->clear();
        *record = fragment;
        last_record_offset_ = prospective_record_offset;
        return true;

      case kfirsttype:
        if (in_fragmented_record) {
          // handle bug in earlier versions of log::writer where
          // it could emit an empty kfirsttype record at the tail end
          // of a block followed by a kfulltype or kfirsttype record
          // at the beginning of the next block.
          if (scratch->empty()) {
            in_fragmented_record = false;
          } else {
            reportcorruption(scratch->size(), "partial record without end(2)");
          }
        }
        prospective_record_offset = physical_record_offset;
        scratch->assign(fragment.data(), fragment.size());
        in_fragmented_record = true;
        break;

      case kmiddletype:
        if (!in_fragmented_record) {
          reportcorruption(fragment.size(),
                           "missing start of fragmented record(1)");
        } else {
          scratch->append(fragment.data(), fragment.size());
        }
        break;

      case klasttype:
        if (!in_fragmented_record) {
          reportcorruption(fragment.size(),
                           "missing start of fragmented record(2)");
        } else {
          scratch->append(fragment.data(), fragment.size());
          *record = slice(*scratch);
          last_record_offset_ = prospective_record_offset;
          return true;
        }
        break;

      case keof:
        if (in_fragmented_record) {
          // this can be caused by the writer dying immediately after
          //  writing a physical record but before completing the next; don't
          //  treat it as a corruption, just ignore the entire logical record.
          scratch->clear();
        }
        return false;

      case kbadrecord:
        if (in_fragmented_record) {
          reportcorruption(scratch->size(), "error in middle of record");
          in_fragmented_record = false;
          scratch->clear();
        }
        break;

      default: {
        char buf[40];
        snprintf(buf, sizeof(buf), "unknown record type %u", record_type);
        reportcorruption(
            (fragment.size() + (in_fragmented_record ? scratch->size() : 0)),
            buf);
        in_fragmented_record = false;
        scratch->clear();
        break;
      }
    }
  }
  return false;
}

uint64_t reader::lastrecordoffset() {
  return last_record_offset_;
}

void reader::unmarkeof() {
  if (read_error_) {
    return;
  }

  eof_ = false;

  if (eof_offset_ == 0) {
    return;
  }

  // if the eof was in the middle of a block (a partial block was read) we have
  // to read the rest of the block as readphysicalrecord can only read full
  // blocks and expects the file position indicator to be aligned to the start
  // of a block.
  //
  //      consumed_bytes + buffer_size() + remaining == kblocksize

  size_t consumed_bytes = eof_offset_ - buffer_.size();
  size_t remaining = kblocksize - eof_offset_;

  // backing_store_ is used to concatenate what is left in buffer_ and
  // the remainder of the block. if buffer_ already uses backing_store_,
  // we just append the new data.
  if (buffer_.data() != backing_store_ + consumed_bytes) {
    // buffer_ does not use backing_store_ for storage.
    // copy what is left in buffer_ to backing_store.
    memmove(backing_store_ + consumed_bytes, buffer_.data(), buffer_.size());
  }

  slice read_buffer;
  status status = file_->read(remaining, &read_buffer,
    backing_store_ + eof_offset_);

  size_t added = read_buffer.size();
  end_of_buffer_offset_ += added;

  if (!status.ok()) {
    if (added > 0) {
      reportdrop(added, status);
    }

    read_error_ = true;
    return;
  }

  if (read_buffer.data() != backing_store_ + eof_offset_) {
    // read did not write to backing_store_
    memmove(backing_store_ + eof_offset_, read_buffer.data(),
      read_buffer.size());
  }

  buffer_ = slice(backing_store_ + consumed_bytes,
    eof_offset_ + added - consumed_bytes);

  if (added < remaining) {
    eof_ = true;
    eof_offset_ += added;
  } else {
    eof_offset_ = 0;
  }
}

void reader::reportcorruption(size_t bytes, const char* reason) {
  reportdrop(bytes, status::corruption(reason));
}

void reader::reportdrop(size_t bytes, const status& reason) {
  if (reporter_ != nullptr &&
      end_of_buffer_offset_ - buffer_.size() - bytes >= initial_offset_) {
    reporter_->corruption(bytes, reason);
  }
}

unsigned int reader::readphysicalrecord(slice* result) {
  while (true) {
    if (buffer_.size() < (size_t)kheadersize) {
      if (!eof_ && !read_error_) {
        // last read was a full read, so this is a trailer to skip
        buffer_.clear();
        status status = file_->read(kblocksize, &buffer_, backing_store_);
        end_of_buffer_offset_ += buffer_.size();
        if (!status.ok()) {
          buffer_.clear();
          reportdrop(kblocksize, status);
          read_error_ = true;
          return keof;
        } else if (buffer_.size() < (size_t)kblocksize) {
          eof_ = true;
          eof_offset_ = buffer_.size();
        }
        continue;
      } else {
        // note that if buffer_ is non-empty, we have a truncated header at the
        //  end of the file, which can be caused by the writer crashing in the
        //  middle of writing the header. instead of considering this an error,
        //  just report eof.
        buffer_.clear();
        return keof;
      }
    }

    // parse the header
    const char* header = buffer_.data();
    const uint32_t a = static_cast<uint32_t>(header[4]) & 0xff;
    const uint32_t b = static_cast<uint32_t>(header[5]) & 0xff;
    const unsigned int type = header[6];
    const uint32_t length = a | (b << 8);
    if (kheadersize + length > buffer_.size()) {
      size_t drop_size = buffer_.size();
      buffer_.clear();
      if (!eof_) {
        reportcorruption(drop_size, "bad record length");
        return kbadrecord;
      }
      // if the end of the file has been reached without reading |length| bytes
      // of payload, assume the writer died in the middle of writing the record.
      // don't report a corruption.
      return keof;
    }

    if (type == kzerotype && length == 0) {
      // skip zero length record without reporting any drops since
      // such records are produced by the mmap based writing code in
      // env_posix.cc that preallocates file regions.
      // note: this should never happen in db written by new rocksdb versions,
      // since we turn off mmap writes to manifest and log files
      buffer_.clear();
      return kbadrecord;
    }

    // check crc
    if (checksum_) {
      uint32_t expected_crc = crc32c::unmask(decodefixed32(header));
      uint32_t actual_crc = crc32c::value(header + 6, 1 + length);
      if (actual_crc != expected_crc) {
        // drop the rest of the buffer since "length" itself may have
        // been corrupted and if we trust it, we could find some
        // fragment of a real log record that just happens to look
        // like a valid log record.
        size_t drop_size = buffer_.size();
        buffer_.clear();
        reportcorruption(drop_size, "checksum mismatch");
        return kbadrecord;
      }
    }

    buffer_.remove_prefix(kheadersize + length);

    // skip physical record that started before initial_offset_
    if (end_of_buffer_offset_ - buffer_.size() - kheadersize - length <
        initial_offset_) {
      result->clear();
      return kbadrecord;
    }

    *result = slice(header + kheadersize, length);
    return type;
  }
}

}  // namespace log
}  // namespace rocksdb
