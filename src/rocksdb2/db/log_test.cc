//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/log_reader.h"
#include "db/log_writer.h"
#include "rocksdb/env.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include "util/random.h"
#include "util/testharness.h"

namespace rocksdb {
namespace log {

// construct a string of the specified length made out of the supplied
// partial string.
static std::string bigstring(const std::string& partial_string, size_t n) {
  std::string result;
  while (result.size() < n) {
    result.append(partial_string);
  }
  result.resize(n);
  return result;
}

// construct a string from a number
static std::string numberstring(int n) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%d.", n);
  return std::string(buf);
}

// return a skewed potentially long string
static std::string randomskewedstring(int i, random* rnd) {
  return bigstring(numberstring(i), rnd->skewed(17));
}

class logtest {
 private:
  class stringdest : public writablefile {
   public:
    std::string contents_;

    explicit stringdest(slice& reader_contents) :
      writablefile(),
      contents_(""),
      reader_contents_(reader_contents),
      last_flush_(0) {
      reader_contents_ = slice(contents_.data(), 0);
    };

    virtual status close() { return status::ok(); }
    virtual status flush() {
      assert_true(reader_contents_.size() <= last_flush_);
      size_t offset = last_flush_ - reader_contents_.size();
      reader_contents_ = slice(
          contents_.data() + offset,
          contents_.size() - offset);
      last_flush_ = contents_.size();

      return status::ok();
    }
    virtual status sync() { return status::ok(); }
    virtual status append(const slice& slice) {
      contents_.append(slice.data(), slice.size());
      return status::ok();
    }
    void drop(size_t bytes) {
      contents_.resize(contents_.size() - bytes);
      reader_contents_ = slice(
          reader_contents_.data(), reader_contents_.size() - bytes);
      last_flush_ = contents_.size();
    }

   private:
    slice& reader_contents_;
    size_t last_flush_;
  };

  class stringsource : public sequentialfile {
   public:
    slice& contents_;
    bool force_error_;
    size_t force_error_position_;
    bool force_eof_;
    size_t force_eof_position_;
    bool returned_partial_;
    explicit stringsource(slice& contents) :
      contents_(contents),
      force_error_(false),
      force_error_position_(0),
      force_eof_(false),
      force_eof_position_(0),
      returned_partial_(false) { }

    virtual status read(size_t n, slice* result, char* scratch) {
      assert_true(!returned_partial_) << "must not read() after eof/error";

      if (force_error_) {
        if (force_error_position_ >= n) {
          force_error_position_ -= n;
        } else {
          *result = slice(contents_.data(), force_error_position_);
          contents_.remove_prefix(force_error_position_);
          force_error_ = false;
          returned_partial_ = true;
          return status::corruption("read error");
        }
      }

      if (contents_.size() < n) {
        n = contents_.size();
        returned_partial_ = true;
      }

      if (force_eof_) {
        if (force_eof_position_ >= n) {
          force_eof_position_ -= n;
        } else {
          force_eof_ = false;
          n = force_eof_position_;
          returned_partial_ = true;
        }
      }

      // by using scratch we ensure that caller has control over the
      // lifetime of result.data()
      memcpy(scratch, contents_.data(), n);
      *result = slice(scratch, n);

      contents_.remove_prefix(n);
      return status::ok();
    }

    virtual status skip(uint64_t n) {
      if (n > contents_.size()) {
        contents_.clear();
        return status::notfound("in-memory file skipepd past end");
      }

      contents_.remove_prefix(n);

      return status::ok();
    }
  };

  class reportcollector : public reader::reporter {
   public:
    size_t dropped_bytes_;
    std::string message_;

    reportcollector() : dropped_bytes_(0) { }
    virtual void corruption(size_t bytes, const status& status) {
      dropped_bytes_ += bytes;
      message_.append(status.tostring());
    }
  };

  std::string& dest_contents() {
    auto dest = dynamic_cast<stringdest*>(writer_.file());
    assert(dest);
    return dest->contents_;
  }

  const std::string& dest_contents() const {
    auto dest = dynamic_cast<const stringdest*>(writer_.file());
    assert(dest);
    return dest->contents_;
  }

  void reset_source_contents() {
    auto src = dynamic_cast<stringsource*>(reader_.file());
    assert(src);
    src->contents_ = dest_contents();
  }

  slice reader_contents_;
  unique_ptr<stringdest> dest_holder_;
  unique_ptr<stringsource> source_holder_;
  reportcollector report_;
  writer writer_;
  reader reader_;

  // record metadata for testing initial offset functionality
  static size_t initial_offset_record_sizes_[];
  static uint64_t initial_offset_last_record_offsets_[];

 public:
  logtest() : reader_contents_(),
              dest_holder_(new stringdest(reader_contents_)),
              source_holder_(new stringsource(reader_contents_)),
              writer_(std::move(dest_holder_)),
              reader_(std::move(source_holder_), &report_, true/*checksum*/,
                      0/*initial_offset*/) {
  }

  void write(const std::string& msg) {
    writer_.addrecord(slice(msg));
  }

  size_t writtenbytes() const {
    return dest_contents().size();
  }

  std::string read() {
    std::string scratch;
    slice record;
    if (reader_.readrecord(&record, &scratch)) {
      return record.tostring();
    } else {
      return "eof";
    }
  }

  void incrementbyte(int offset, int delta) {
    dest_contents()[offset] += delta;
  }

  void setbyte(int offset, char new_byte) {
    dest_contents()[offset] = new_byte;
  }

  void shrinksize(int bytes) {
    auto dest = dynamic_cast<stringdest*>(writer_.file());
    assert(dest);
    dest->drop(bytes);
  }

  void fixchecksum(int header_offset, int len) {
    // compute crc of type/len/data
    uint32_t crc = crc32c::value(&dest_contents()[header_offset+6], 1 + len);
    crc = crc32c::mask(crc);
    encodefixed32(&dest_contents()[header_offset], crc);
  }

  void forceerror(size_t position = 0) {
    auto src = dynamic_cast<stringsource*>(reader_.file());
    src->force_error_ = true;
    src->force_error_position_ = position;
  }

  size_t droppedbytes() const {
    return report_.dropped_bytes_;
  }

  std::string reportmessage() const {
    return report_.message_;
  }

  void forceeof(size_t position = 0) {
    auto src = dynamic_cast<stringsource*>(reader_.file());
    src->force_eof_ = true;
    src->force_eof_position_ = position;
  }

  void unmarkeof() {
    auto src = dynamic_cast<stringsource*>(reader_.file());
    src->returned_partial_ = false;
    reader_.unmarkeof();
  }

  bool iseof() {
    return reader_.iseof();
  }

  // returns ok iff recorded error message contains "msg"
  std::string matcherror(const std::string& msg) const {
    if (report_.message_.find(msg) == std::string::npos) {
      return report_.message_;
    } else {
      return "ok";
    }
  }

  void writeinitialoffsetlog() {
    for (int i = 0; i < 4; i++) {
      std::string record(initial_offset_record_sizes_[i],
                         static_cast<char>('a' + i));
      write(record);
    }
  }

  void checkoffsetpastendreturnsnorecords(uint64_t offset_past_end) {
    writeinitialoffsetlog();
    unique_ptr<stringsource> source(new stringsource(reader_contents_));
    unique_ptr<reader> offset_reader(
      new reader(std::move(source), &report_, true/*checksum*/,
                 writtenbytes() + offset_past_end));
    slice record;
    std::string scratch;
    assert_true(!offset_reader->readrecord(&record, &scratch));
  }

  void checkinitialoffsetrecord(uint64_t initial_offset,
                                int expected_record_offset) {
    writeinitialoffsetlog();
    unique_ptr<stringsource> source(new stringsource(reader_contents_));
    unique_ptr<reader> offset_reader(
      new reader(std::move(source), &report_, true/*checksum*/,
                 initial_offset));
    slice record;
    std::string scratch;
    assert_true(offset_reader->readrecord(&record, &scratch));
    assert_eq(initial_offset_record_sizes_[expected_record_offset],
              record.size());
    assert_eq(initial_offset_last_record_offsets_[expected_record_offset],
              offset_reader->lastrecordoffset());
    assert_eq((char)('a' + expected_record_offset), record.data()[0]);
  }

};

size_t logtest::initial_offset_record_sizes_[] =
    {10000,  // two sizable records in first block
     10000,
     2 * log::kblocksize - 1000,  // span three blocks
     1};

uint64_t logtest::initial_offset_last_record_offsets_[] =
    {0,
     kheadersize + 10000,
     2 * (kheadersize + 10000),
     2 * (kheadersize + 10000) +
         (2 * log::kblocksize - 1000) + 3 * kheadersize};


test(logtest, empty) {
  assert_eq("eof", read());
}

test(logtest, readwrite) {
  write("foo");
  write("bar");
  write("");
  write("xxxx");
  assert_eq("foo", read());
  assert_eq("bar", read());
  assert_eq("", read());
  assert_eq("xxxx", read());
  assert_eq("eof", read());
  assert_eq("eof", read());  // make sure reads at eof work
}

test(logtest, manyblocks) {
  for (int i = 0; i < 100000; i++) {
    write(numberstring(i));
  }
  for (int i = 0; i < 100000; i++) {
    assert_eq(numberstring(i), read());
  }
  assert_eq("eof", read());
}

test(logtest, fragmentation) {
  write("small");
  write(bigstring("medium", 50000));
  write(bigstring("large", 100000));
  assert_eq("small", read());
  assert_eq(bigstring("medium", 50000), read());
  assert_eq(bigstring("large", 100000), read());
  assert_eq("eof", read());
}

test(logtest, marginaltrailer) {
  // make a trailer that is exactly the same length as an empty record.
  const int n = kblocksize - 2*kheadersize;
  write(bigstring("foo", n));
  assert_eq((unsigned int)(kblocksize - kheadersize), writtenbytes());
  write("");
  write("bar");
  assert_eq(bigstring("foo", n), read());
  assert_eq("", read());
  assert_eq("bar", read());
  assert_eq("eof", read());
}

test(logtest, marginaltrailer2) {
  // make a trailer that is exactly the same length as an empty record.
  const int n = kblocksize - 2*kheadersize;
  write(bigstring("foo", n));
  assert_eq((unsigned int)(kblocksize - kheadersize), writtenbytes());
  write("bar");
  assert_eq(bigstring("foo", n), read());
  assert_eq("bar", read());
  assert_eq("eof", read());
  assert_eq(0u, droppedbytes());
  assert_eq("", reportmessage());
}

test(logtest, shorttrailer) {
  const int n = kblocksize - 2*kheadersize + 4;
  write(bigstring("foo", n));
  assert_eq((unsigned int)(kblocksize - kheadersize + 4), writtenbytes());
  write("");
  write("bar");
  assert_eq(bigstring("foo", n), read());
  assert_eq("", read());
  assert_eq("bar", read());
  assert_eq("eof", read());
}

test(logtest, alignedeof) {
  const int n = kblocksize - 2*kheadersize + 4;
  write(bigstring("foo", n));
  assert_eq((unsigned int)(kblocksize - kheadersize + 4), writtenbytes());
  assert_eq(bigstring("foo", n), read());
  assert_eq("eof", read());
}

test(logtest, randomread) {
  const int n = 500;
  random write_rnd(301);
  for (int i = 0; i < n; i++) {
    write(randomskewedstring(i, &write_rnd));
  }
  random read_rnd(301);
  for (int i = 0; i < n; i++) {
    assert_eq(randomskewedstring(i, &read_rnd), read());
  }
  assert_eq("eof", read());
}

// tests of all the error paths in log_reader.cc follow:

test(logtest, readerror) {
  write("foo");
  forceerror();
  assert_eq("eof", read());
  assert_eq((unsigned int)kblocksize, droppedbytes());
  assert_eq("ok", matcherror("read error"));
}

test(logtest, badrecordtype) {
  write("foo");
  // type is stored in header[6]
  incrementbyte(6, 100);
  fixchecksum(0, 3);
  assert_eq("eof", read());
  assert_eq(3u, droppedbytes());
  assert_eq("ok", matcherror("unknown record type"));
}

test(logtest, truncatedtrailingrecordisignored) {
  write("foo");
  shrinksize(4);   // drop all payload as well as a header byte
  assert_eq("eof", read());
  // truncated last record is ignored, not treated as an error
  assert_eq(0u, droppedbytes());
  assert_eq("", reportmessage());
}

test(logtest, badlength) {
  const int kpayloadsize = kblocksize - kheadersize;
  write(bigstring("bar", kpayloadsize));
  write("foo");
  // least significant size byte is stored in header[4].
  incrementbyte(4, 1);
  assert_eq("foo", read());
  assert_eq(kblocksize, droppedbytes());
  assert_eq("ok", matcherror("bad record length"));
}

test(logtest, badlengthatendisignored) {
  write("foo");
  shrinksize(1);
  assert_eq("eof", read());
  assert_eq(0u, droppedbytes());
  assert_eq("", reportmessage());
}

test(logtest, checksummismatch) {
  write("foo");
  incrementbyte(0, 10);
  assert_eq("eof", read());
  assert_eq(10u, droppedbytes());
  assert_eq("ok", matcherror("checksum mismatch"));
}

test(logtest, unexpectedmiddletype) {
  write("foo");
  setbyte(6, kmiddletype);
  fixchecksum(0, 3);
  assert_eq("eof", read());
  assert_eq(3u, droppedbytes());
  assert_eq("ok", matcherror("missing start"));
}

test(logtest, unexpectedlasttype) {
  write("foo");
  setbyte(6, klasttype);
  fixchecksum(0, 3);
  assert_eq("eof", read());
  assert_eq(3u, droppedbytes());
  assert_eq("ok", matcherror("missing start"));
}

test(logtest, unexpectedfulltype) {
  write("foo");
  write("bar");
  setbyte(6, kfirsttype);
  fixchecksum(0, 3);
  assert_eq("bar", read());
  assert_eq("eof", read());
  assert_eq(3u, droppedbytes());
  assert_eq("ok", matcherror("partial record without end"));
}

test(logtest, unexpectedfirsttype) {
  write("foo");
  write(bigstring("bar", 100000));
  setbyte(6, kfirsttype);
  fixchecksum(0, 3);
  assert_eq(bigstring("bar", 100000), read());
  assert_eq("eof", read());
  assert_eq(3u, droppedbytes());
  assert_eq("ok", matcherror("partial record without end"));
}

test(logtest, missinglastisignored) {
  write(bigstring("bar", kblocksize));
  // remove the last block, including header.
  shrinksize(14);
  assert_eq("eof", read());
  assert_eq("", reportmessage());
  assert_eq(0u, droppedbytes());
}

test(logtest, partiallastisignored) {
  write(bigstring("bar", kblocksize));
  // cause a bad record length in the last block.
  shrinksize(1);
  assert_eq("eof", read());
  assert_eq("", reportmessage());
  assert_eq(0u, droppedbytes());
}

test(logtest, errorjoinsrecords) {
  // consider two fragmented records:
  //    first(r1) last(r1) first(r2) last(r2)
  // where the middle two fragments disappear.  we do not want
  // first(r1),last(r2) to get joined and returned as a valid record.

  // write records that span two blocks
  write(bigstring("foo", kblocksize));
  write(bigstring("bar", kblocksize));
  write("correct");

  // wipe the middle block
  for (unsigned int offset = kblocksize; offset < 2*kblocksize; offset++) {
    setbyte(offset, 'x');
  }

  assert_eq("correct", read());
  assert_eq("eof", read());
  const unsigned int dropped = droppedbytes();
  assert_le(dropped, 2*kblocksize + 100);
  assert_ge(dropped, 2*kblocksize);
}

test(logtest, readstart) {
  checkinitialoffsetrecord(0, 0);
}

test(logtest, readsecondoneoff) {
  checkinitialoffsetrecord(1, 1);
}

test(logtest, readsecondtenthousand) {
  checkinitialoffsetrecord(10000, 1);
}

test(logtest, readsecondstart) {
  checkinitialoffsetrecord(10007, 1);
}

test(logtest, readthirdoneoff) {
  checkinitialoffsetrecord(10008, 2);
}

test(logtest, readthirdstart) {
  checkinitialoffsetrecord(20014, 2);
}

test(logtest, readfourthoneoff) {
  checkinitialoffsetrecord(20015, 3);
}

test(logtest, readfourthfirstblocktrailer) {
  checkinitialoffsetrecord(log::kblocksize - 4, 3);
}

test(logtest, readfourthmiddleblock) {
  checkinitialoffsetrecord(log::kblocksize + 1, 3);
}

test(logtest, readfourthlastblock) {
  checkinitialoffsetrecord(2 * log::kblocksize + 1, 3);
}

test(logtest, readfourthstart) {
  checkinitialoffsetrecord(
      2 * (kheadersize + 1000) + (2 * log::kblocksize - 1000) + 3 * kheadersize,
      3);
}

test(logtest, readend) {
  checkoffsetpastendreturnsnorecords(0);
}

test(logtest, readpastend) {
  checkoffsetpastendreturnsnorecords(5);
}

test(logtest, cleareofsingleblock) {
  write("foo");
  write("bar");
  forceeof(3 + kheadersize + 2);
  assert_eq("foo", read());
  unmarkeof();
  assert_eq("bar", read());
  assert_true(iseof());
  assert_eq("eof", read());
  write("xxx");
  unmarkeof();
  assert_eq("xxx", read());
  assert_true(iseof());
}

test(logtest, cleareofmultiblock) {
  size_t num_full_blocks = 5;
  size_t n = (kblocksize - kheadersize) * num_full_blocks + 25;
  write(bigstring("foo", n));
  write(bigstring("bar", n));
  forceeof(n + num_full_blocks * kheadersize + 10);
  assert_eq(bigstring("foo", n), read());
  assert_true(iseof());
  unmarkeof();
  assert_eq(bigstring("bar", n), read());
  assert_true(iseof());
  write(bigstring("xxx", n));
  unmarkeof();
  assert_eq(bigstring("xxx", n), read());
  assert_true(iseof());
}

test(logtest, cleareoferror) {
  // if an error occurs during read() in unmarkeof(), the records contained
  // in the buffer should be returned on subsequent calls of readrecord()
  // until no more full records are left, whereafter readrecord() should return
  // false to indicate that it cannot read any further.

  write("foo");
  write("bar");
  unmarkeof();
  assert_eq("foo", read());
  assert_true(iseof());
  write("xxx");
  forceerror(0);
  unmarkeof();
  assert_eq("bar", read());
  assert_eq("eof", read());
}

test(logtest, cleareoferror2) {
  write("foo");
  write("bar");
  unmarkeof();
  assert_eq("foo", read());
  write("xxx");
  forceerror(3);
  unmarkeof();
  assert_eq("bar", read());
  assert_eq("eof", read());
  assert_eq(3u, droppedbytes());
  assert_eq("ok", matcherror("read error"));
}

}  // namespace log
}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
