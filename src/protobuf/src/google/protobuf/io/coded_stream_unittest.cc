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
// this file contains tests and benchmarks.

#include <vector>

#include <google/protobuf/io/coded_stream.h>

#include <limits.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>


// this declares an unsigned long long integer literal in a portable way.
// (the original macro is way too big and ruins my formatting.)
#undef ull
#define ull(x) google_ulonglong(x)

namespace google {
namespace protobuf {
namespace io {
namespace {

// ===================================================================
// data-driven test infrastructure

// test_1d and test_2d are macros i'd eventually like to see added to
// gtest.  these macros can be used to declare tests which should be
// run multiple times, once for each item in some input array.  test_1d
// tests all cases in a single input array.  test_2d tests all
// combinations of cases from two arrays.  the arrays must be statically
// defined such that the google_arraysize() macro works on them.  example:
//
// int kcases[] = {1, 2, 3, 4}
// test_1d(myfixture, mytest, kcases) {
//   expect_gt(kcases_case, 0);
// }
//
// this test iterates through the numbers 1, 2, 3, and 4 and tests that
// they are all grater than zero.  in case of failure, the exact case
// which failed will be printed.  the case type must be printable using
// ostream::operator<<.

// todo(kenton):  gtest now supports "parameterized tests" which would be
//   a better way to accomplish this.  rewrite when time permits.

#define test_1d(fixture, name, cases)                                      \
  class fixture##_##name##_dd : public fixture {                           \
   protected:                                                              \
    template <typename casetype>                                           \
    void dosinglecase(const casetype& cases##_case);                       \
  };                                                                       \
                                                                           \
  test_f(fixture##_##name##_dd, name) {                                    \
    for (int i = 0; i < google_arraysize(cases); i++) {                           \
      scoped_trace(testing::message()                                      \
        << #cases " case #" << i << ": " << cases[i]);                     \
      dosinglecase(cases[i]);                                              \
    }                                                                      \
  }                                                                        \
                                                                           \
  template <typename casetype>                                             \
  void fixture##_##name##_dd::dosinglecase(const casetype& cases##_case)

#define test_2d(fixture, name, cases1, cases2)                             \
  class fixture##_##name##_dd : public fixture {                           \
   protected:                                                              \
    template <typename casetype1, typename casetype2>                      \
    void dosinglecase(const casetype1& cases1##_case,                      \
                      const casetype2& cases2##_case);                     \
  };                                                                       \
                                                                           \
  test_f(fixture##_##name##_dd, name) {                                    \
    for (int i = 0; i < google_arraysize(cases1); i++) {                          \
      for (int j = 0; j < google_arraysize(cases2); j++) {                        \
        scoped_trace(testing::message()                                    \
          << #cases1 " case #" << i << ": " << cases1[i] << ", "           \
          << #cases2 " case #" << j << ": " << cases2[j]);                 \
        dosinglecase(cases1[i], cases2[j]);                                \
      }                                                                    \
    }                                                                      \
  }                                                                        \
                                                                           \
  template <typename casetype1, typename casetype2>                        \
  void fixture##_##name##_dd::dosinglecase(const casetype1& cases1##_case, \
                                           const casetype2& cases2##_case)

// ===================================================================

class codedstreamtest : public testing::test {
 protected:
  // helper method used by tests for bytes warning. see implementation comment
  // for further information.
  static void setuptotalbyteslimitwarningtest(
      int total_bytes_limit, int warning_threshold,
      vector<string>* out_errors, vector<string>* out_warnings);

  // buffer used during most of the tests. this assumes tests run sequentially.
  static const int kbuffersize = 1024 * 64;
  static uint8 buffer_[kbuffersize];
};

uint8 codedstreamtest::buffer_[codedstreamtest::kbuffersize];

// we test each operation over a variety of block sizes to insure that
// we test cases where reads or writes cross buffer boundaries, cases
// where they don't, and cases where there is so much buffer left that
// we can use special optimized paths that don't worry about bounds
// checks.
const int kblocksizes[] = {1, 2, 3, 5, 7, 13, 32, 1024};

// -------------------------------------------------------------------
// varint tests.

struct varintcase {
  uint8 bytes[10];          // encoded bytes.
  int size;                 // encoded size, in bytes.
  uint64 value;             // parsed value.
};

inline std::ostream& operator<<(std::ostream& os, const varintcase& c) {
  return os << c.value;
}

varintcase kvarintcases[] = {
  // 32-bit values
  {{0x00}      , 1, 0},
  {{0x01}      , 1, 1},
  {{0x7f}      , 1, 127},
  {{0xa2, 0x74}, 2, (0x22 << 0) | (0x74 << 7)},          // 14882
  {{0xbe, 0xf7, 0x92, 0x84, 0x0b}, 5,                    // 2961488830
    (0x3e << 0) | (0x77 << 7) | (0x12 << 14) | (0x04 << 21) |
    (ull(0x0b) << 28)},

  // 64-bit
  {{0xbe, 0xf7, 0x92, 0x84, 0x1b}, 5,                    // 7256456126
    (0x3e << 0) | (0x77 << 7) | (0x12 << 14) | (0x04 << 21) |
    (ull(0x1b) << 28)},
  {{0x80, 0xe6, 0xeb, 0x9c, 0xc3, 0xc9, 0xa4, 0x49}, 8,  // 41256202580718336
    (0x00 << 0) | (0x66 << 7) | (0x6b << 14) | (0x1c << 21) |
    (ull(0x43) << 28) | (ull(0x49) << 35) | (ull(0x24) << 42) |
    (ull(0x49) << 49)},
  // 11964378330978735131
  {{0x9b, 0xa8, 0xf9, 0xc2, 0xbb, 0xd6, 0x80, 0x85, 0xa6, 0x01}, 10,
    (0x1b << 0) | (0x28 << 7) | (0x79 << 14) | (0x42 << 21) |
    (ull(0x3b) << 28) | (ull(0x56) << 35) | (ull(0x00) << 42) |
    (ull(0x05) << 49) | (ull(0x26) << 56) | (ull(0x01) << 63)},
};

test_2d(codedstreamtest, readvarint32, kvarintcases, kblocksizes) {
  memcpy(buffer_, kvarintcases_case.bytes, kvarintcases_case.size);
  arrayinputstream input(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedinputstream coded_input(&input);

    uint32 value;
    expect_true(coded_input.readvarint32(&value));
    expect_eq(static_cast<uint32>(kvarintcases_case.value), value);
  }

  expect_eq(kvarintcases_case.size, input.bytecount());
}

test_2d(codedstreamtest, readtag, kvarintcases, kblocksizes) {
  memcpy(buffer_, kvarintcases_case.bytes, kvarintcases_case.size);
  arrayinputstream input(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedinputstream coded_input(&input);

    uint32 expected_value = static_cast<uint32>(kvarintcases_case.value);
    expect_eq(expected_value, coded_input.readtag());

    expect_true(coded_input.lasttagwas(expected_value));
    expect_false(coded_input.lasttagwas(expected_value + 1));
  }

  expect_eq(kvarintcases_case.size, input.bytecount());
}

// this is the regression test that verifies that there is no issues
// with the empty input buffers handling.
test_f(codedstreamtest, emptyinputbeforeeos) {
  class in : public zerocopyinputstream {
   public:
    in() : count_(0) {}
   private:
    virtual bool next(const void** data, int* size) {
      *data = null;
      *size = 0;
      return count_++ < 2;
    }
    virtual void backup(int count)  {
      google_log(fatal) << "tests never call this.";
    }
    virtual bool skip(int count) {
      google_log(fatal) << "tests never call this.";
      return false;
    }
    virtual int64 bytecount() const { return 0; }
    int count_;
  } in;
  codedinputstream input(&in);
  input.readtag();
  expect_true(input.consumedentiremessage());
}

test_1d(codedstreamtest, expecttag, kvarintcases) {
  // leave one byte at the beginning of the buffer so we can read it
  // to force the first buffer to be loaded.
  buffer_[0] = '\0';
  memcpy(buffer_ + 1, kvarintcases_case.bytes, kvarintcases_case.size);
  arrayinputstream input(buffer_, sizeof(buffer_));

  {
    codedinputstream coded_input(&input);

    // read one byte to force coded_input.refill() to be called.  otherwise,
    // expecttag() will return a false negative.
    uint8 dummy;
    coded_input.readraw(&dummy, 1);
    expect_eq((uint)'\0', (uint)dummy);

    uint32 expected_value = static_cast<uint32>(kvarintcases_case.value);

    // expecttag() produces false negatives for large values.
    if (kvarintcases_case.size <= 2) {
      expect_false(coded_input.expecttag(expected_value + 1));
      expect_true(coded_input.expecttag(expected_value));
    } else {
      expect_false(coded_input.expecttag(expected_value));
    }
  }

  if (kvarintcases_case.size <= 2) {
    expect_eq(kvarintcases_case.size + 1, input.bytecount());
  } else {
    expect_eq(1, input.bytecount());
  }
}

test_1d(codedstreamtest, expecttagfromarray, kvarintcases) {
  memcpy(buffer_, kvarintcases_case.bytes, kvarintcases_case.size);

  const uint32 expected_value = static_cast<uint32>(kvarintcases_case.value);

  // if the expectation succeeds, it should return a pointer past the tag.
  if (kvarintcases_case.size <= 2) {
    expect_true(null ==
                codedinputstream::expecttagfromarray(buffer_,
                                                     expected_value + 1));
    expect_true(buffer_ + kvarintcases_case.size ==
                codedinputstream::expecttagfromarray(buffer_, expected_value));
  } else {
    expect_true(null ==
                codedinputstream::expecttagfromarray(buffer_, expected_value));
  }
}

test_2d(codedstreamtest, readvarint64, kvarintcases, kblocksizes) {
  memcpy(buffer_, kvarintcases_case.bytes, kvarintcases_case.size);
  arrayinputstream input(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedinputstream coded_input(&input);

    uint64 value;
    expect_true(coded_input.readvarint64(&value));
    expect_eq(kvarintcases_case.value, value);
  }

  expect_eq(kvarintcases_case.size, input.bytecount());
}

test_2d(codedstreamtest, writevarint32, kvarintcases, kblocksizes) {
  if (kvarintcases_case.value > ull(0x00000000ffffffff)) {
    // skip this test for the 64-bit values.
    return;
  }

  arrayoutputstream output(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedoutputstream coded_output(&output);

    coded_output.writevarint32(static_cast<uint32>(kvarintcases_case.value));
    expect_false(coded_output.haderror());

    expect_eq(kvarintcases_case.size, coded_output.bytecount());
  }

  expect_eq(kvarintcases_case.size, output.bytecount());
  expect_eq(0,
    memcmp(buffer_, kvarintcases_case.bytes, kvarintcases_case.size));
}

test_2d(codedstreamtest, writevarint64, kvarintcases, kblocksizes) {
  arrayoutputstream output(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedoutputstream coded_output(&output);

    coded_output.writevarint64(kvarintcases_case.value);
    expect_false(coded_output.haderror());

    expect_eq(kvarintcases_case.size, coded_output.bytecount());
  }

  expect_eq(kvarintcases_case.size, output.bytecount());
  expect_eq(0,
    memcmp(buffer_, kvarintcases_case.bytes, kvarintcases_case.size));
}

// this test causes gcc 3.3.5 (and earlier?) to give the cryptic error:
//   "sorry, unimplemented: `method_call_expr' not supported by dump_expr"
#if !defined(__gnuc__) || __gnuc__ > 3 || (__gnuc__ == 3 && __gnuc_minor__ > 3)

int32 ksignextendedvarintcases[] = {
  0, 1, -1, 1237894, -37895138
};

test_2d(codedstreamtest, writevarint32signextended,
        ksignextendedvarintcases, kblocksizes) {
  arrayoutputstream output(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedoutputstream coded_output(&output);

    coded_output.writevarint32signextended(ksignextendedvarintcases_case);
    expect_false(coded_output.haderror());

    if (ksignextendedvarintcases_case < 0) {
      expect_eq(10, coded_output.bytecount());
    } else {
      expect_le(coded_output.bytecount(), 5);
    }
  }

  if (ksignextendedvarintcases_case < 0) {
    expect_eq(10, output.bytecount());
  } else {
    expect_le(output.bytecount(), 5);
  }

  // read value back in as a varint64 and insure it matches.
  arrayinputstream input(buffer_, sizeof(buffer_));

  {
    codedinputstream coded_input(&input);

    uint64 value;
    expect_true(coded_input.readvarint64(&value));

    expect_eq(ksignextendedvarintcases_case, static_cast<int64>(value));
  }

  expect_eq(output.bytecount(), input.bytecount());
}

#endif


// -------------------------------------------------------------------
// varint failure test.

struct varinterrorcase {
  uint8 bytes[12];
  int size;
  bool can_parse;
};

inline std::ostream& operator<<(std::ostream& os, const varinterrorcase& c) {
  return os << "size " << c.size;
}

const varinterrorcase kvarinterrorcases[] = {
  // control case.  (insures that there isn't something else wrong that
  // makes parsing always fail.)
  {{0x00}, 1, true},

  // no input data.
  {{}, 0, false},

  // input ends unexpectedly.
  {{0xf0, 0xab}, 2, false},

  // input ends unexpectedly after 32 bits.
  {{0xf0, 0xab, 0xc9, 0x9a, 0xf8, 0xb2}, 6, false},

  // longer than 10 bytes.
  {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01},
   11, false},
};

test_2d(codedstreamtest, readvarint32error, kvarinterrorcases, kblocksizes) {
  memcpy(buffer_, kvarinterrorcases_case.bytes, kvarinterrorcases_case.size);
  arrayinputstream input(buffer_, kvarinterrorcases_case.size,
                         kblocksizes_case);
  codedinputstream coded_input(&input);

  uint32 value;
  expect_eq(kvarinterrorcases_case.can_parse, coded_input.readvarint32(&value));
}

test_2d(codedstreamtest, readvarint64error, kvarinterrorcases, kblocksizes) {
  memcpy(buffer_, kvarinterrorcases_case.bytes, kvarinterrorcases_case.size);
  arrayinputstream input(buffer_, kvarinterrorcases_case.size,
                         kblocksizes_case);
  codedinputstream coded_input(&input);

  uint64 value;
  expect_eq(kvarinterrorcases_case.can_parse, coded_input.readvarint64(&value));
}

// -------------------------------------------------------------------
// varintsize

struct varintsizecase {
  uint64 value;
  int size;
};

inline std::ostream& operator<<(std::ostream& os, const varintsizecase& c) {
  return os << c.value;
}

varintsizecase kvarintsizecases[] = {
  {0u, 1},
  {1u, 1},
  {127u, 1},
  {128u, 2},
  {758923u, 3},
  {4000000000u, 5},
  {ull(41256202580718336), 8},
  {ull(11964378330978735131), 10},
};

test_1d(codedstreamtest, varintsize32, kvarintsizecases) {
  if (kvarintsizecases_case.value > 0xffffffffu) {
    // skip 64-bit values.
    return;
  }

  expect_eq(kvarintsizecases_case.size,
    codedoutputstream::varintsize32(
      static_cast<uint32>(kvarintsizecases_case.value)));
}

test_1d(codedstreamtest, varintsize64, kvarintsizecases) {
  expect_eq(kvarintsizecases_case.size,
    codedoutputstream::varintsize64(kvarintsizecases_case.value));
}

// -------------------------------------------------------------------
// fixed-size int tests

struct fixed32case {
  uint8 bytes[sizeof(uint32)];          // encoded bytes.
  uint32 value;                         // parsed value.
};

struct fixed64case {
  uint8 bytes[sizeof(uint64)];          // encoded bytes.
  uint64 value;                         // parsed value.
};

inline std::ostream& operator<<(std::ostream& os, const fixed32case& c) {
  return os << "0x" << hex << c.value << dec;
}

inline std::ostream& operator<<(std::ostream& os, const fixed64case& c) {
  return os << "0x" << hex << c.value << dec;
}

fixed32case kfixed32cases[] = {
  {{0xef, 0xcd, 0xab, 0x90}, 0x90abcdefu},
  {{0x12, 0x34, 0x56, 0x78}, 0x78563412u},
};

fixed64case kfixed64cases[] = {
  {{0xef, 0xcd, 0xab, 0x90, 0x12, 0x34, 0x56, 0x78}, ull(0x7856341290abcdef)},
  {{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}, ull(0x8877665544332211)},
};

test_2d(codedstreamtest, readlittleendian32, kfixed32cases, kblocksizes) {
  memcpy(buffer_, kfixed32cases_case.bytes, sizeof(kfixed32cases_case.bytes));
  arrayinputstream input(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedinputstream coded_input(&input);

    uint32 value;
    expect_true(coded_input.readlittleendian32(&value));
    expect_eq(kfixed32cases_case.value, value);
  }

  expect_eq(sizeof(uint32), input.bytecount());
}

test_2d(codedstreamtest, readlittleendian64, kfixed64cases, kblocksizes) {
  memcpy(buffer_, kfixed64cases_case.bytes, sizeof(kfixed64cases_case.bytes));
  arrayinputstream input(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedinputstream coded_input(&input);

    uint64 value;
    expect_true(coded_input.readlittleendian64(&value));
    expect_eq(kfixed64cases_case.value, value);
  }

  expect_eq(sizeof(uint64), input.bytecount());
}

test_2d(codedstreamtest, writelittleendian32, kfixed32cases, kblocksizes) {
  arrayoutputstream output(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedoutputstream coded_output(&output);

    coded_output.writelittleendian32(kfixed32cases_case.value);
    expect_false(coded_output.haderror());

    expect_eq(sizeof(uint32), coded_output.bytecount());
  }

  expect_eq(sizeof(uint32), output.bytecount());
  expect_eq(0, memcmp(buffer_, kfixed32cases_case.bytes, sizeof(uint32)));
}

test_2d(codedstreamtest, writelittleendian64, kfixed64cases, kblocksizes) {
  arrayoutputstream output(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedoutputstream coded_output(&output);

    coded_output.writelittleendian64(kfixed64cases_case.value);
    expect_false(coded_output.haderror());

    expect_eq(sizeof(uint64), coded_output.bytecount());
  }

  expect_eq(sizeof(uint64), output.bytecount());
  expect_eq(0, memcmp(buffer_, kfixed64cases_case.bytes, sizeof(uint64)));
}

// tests using the static methods to read fixed-size values from raw arrays.

test_1d(codedstreamtest, readlittleendian32fromarray, kfixed32cases) {
  memcpy(buffer_, kfixed32cases_case.bytes, sizeof(kfixed32cases_case.bytes));

  uint32 value;
  const uint8* end = codedinputstream::readlittleendian32fromarray(
      buffer_, &value);
  expect_eq(kfixed32cases_case.value, value);
  expect_true(end == buffer_ + sizeof(value));
}

test_1d(codedstreamtest, readlittleendian64fromarray, kfixed64cases) {
  memcpy(buffer_, kfixed64cases_case.bytes, sizeof(kfixed64cases_case.bytes));

  uint64 value;
  const uint8* end = codedinputstream::readlittleendian64fromarray(
      buffer_, &value);
  expect_eq(kfixed64cases_case.value, value);
  expect_true(end == buffer_ + sizeof(value));
}

// -------------------------------------------------------------------
// raw reads and writes

const char krawbytes[] = "some bytes which will be written and read raw.";

test_1d(codedstreamtest, readraw, kblocksizes) {
  memcpy(buffer_, krawbytes, sizeof(krawbytes));
  arrayinputstream input(buffer_, sizeof(buffer_), kblocksizes_case);
  char read_buffer[sizeof(krawbytes)];

  {
    codedinputstream coded_input(&input);

    expect_true(coded_input.readraw(read_buffer, sizeof(krawbytes)));
    expect_eq(0, memcmp(krawbytes, read_buffer, sizeof(krawbytes)));
  }

  expect_eq(sizeof(krawbytes), input.bytecount());
}

test_1d(codedstreamtest, writeraw, kblocksizes) {
  arrayoutputstream output(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedoutputstream coded_output(&output);

    coded_output.writeraw(krawbytes, sizeof(krawbytes));
    expect_false(coded_output.haderror());

    expect_eq(sizeof(krawbytes), coded_output.bytecount());
  }

  expect_eq(sizeof(krawbytes), output.bytecount());
  expect_eq(0, memcmp(buffer_, krawbytes, sizeof(krawbytes)));
}

test_1d(codedstreamtest, readstring, kblocksizes) {
  memcpy(buffer_, krawbytes, sizeof(krawbytes));
  arrayinputstream input(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedinputstream coded_input(&input);

    string str;
    expect_true(coded_input.readstring(&str, strlen(krawbytes)));
    expect_eq(krawbytes, str);
  }

  expect_eq(strlen(krawbytes), input.bytecount());
}

// check to make sure readstring doesn't crash on impossibly large strings.
test_1d(codedstreamtest, readstringimpossiblylarge, kblocksizes) {
  arrayinputstream input(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedinputstream coded_input(&input);

    string str;
    // try to read a gigabyte.
    expect_false(coded_input.readstring(&str, 1 << 30));
  }
}

test_f(codedstreamtest, readstringimpossiblylargefromstringonstack) {
  // same test as above, except directly use a buffer. this used to cause
  // crashes while the above did not.
  uint8 buffer[8];
  codedinputstream coded_input(buffer, 8);
  string str;
  expect_false(coded_input.readstring(&str, 1 << 30));
}

test_f(codedstreamtest, readstringimpossiblylargefromstringonheap) {
  scoped_array<uint8> buffer(new uint8[8]);
  codedinputstream coded_input(buffer.get(), 8);
  string str;
  expect_false(coded_input.readstring(&str, 1 << 30));
}


// -------------------------------------------------------------------
// skip

const char kskiptestbytes[] =
  "<before skipping><to be skipped><after skipping>";
const char kskipoutputtestbytes[] =
  "-----------------<to be skipped>----------------";

test_1d(codedstreamtest, skipinput, kblocksizes) {
  memcpy(buffer_, kskiptestbytes, sizeof(kskiptestbytes));
  arrayinputstream input(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedinputstream coded_input(&input);

    string str;
    expect_true(coded_input.readstring(&str, strlen("<before skipping>")));
    expect_eq("<before skipping>", str);
    expect_true(coded_input.skip(strlen("<to be skipped>")));
    expect_true(coded_input.readstring(&str, strlen("<after skipping>")));
    expect_eq("<after skipping>", str);
  }

  expect_eq(strlen(kskiptestbytes), input.bytecount());
}

// -------------------------------------------------------------------
// getdirectbufferpointer

test_f(codedstreamtest, getdirectbufferpointerinput) {
  arrayinputstream input(buffer_, sizeof(buffer_), 8);
  codedinputstream coded_input(&input);

  const void* ptr;
  int size;

  expect_true(coded_input.getdirectbufferpointer(&ptr, &size));
  expect_eq(buffer_, ptr);
  expect_eq(8, size);

  // peeking again should return the same pointer.
  expect_true(coded_input.getdirectbufferpointer(&ptr, &size));
  expect_eq(buffer_, ptr);
  expect_eq(8, size);

  // skip forward in the same buffer then peek again.
  expect_true(coded_input.skip(3));
  expect_true(coded_input.getdirectbufferpointer(&ptr, &size));
  expect_eq(buffer_ + 3, ptr);
  expect_eq(5, size);

  // skip to end of buffer and peek -- should get next buffer.
  expect_true(coded_input.skip(5));
  expect_true(coded_input.getdirectbufferpointer(&ptr, &size));
  expect_eq(buffer_ + 8, ptr);
  expect_eq(8, size);
}

test_f(codedstreamtest, getdirectbufferpointerinlineinput) {
  arrayinputstream input(buffer_, sizeof(buffer_), 8);
  codedinputstream coded_input(&input);

  const void* ptr;
  int size;

  coded_input.getdirectbufferpointerinline(&ptr, &size);
  expect_eq(buffer_, ptr);
  expect_eq(8, size);

  // peeking again should return the same pointer.
  coded_input.getdirectbufferpointerinline(&ptr, &size);
  expect_eq(buffer_, ptr);
  expect_eq(8, size);

  // skip forward in the same buffer then peek again.
  expect_true(coded_input.skip(3));
  coded_input.getdirectbufferpointerinline(&ptr, &size);
  expect_eq(buffer_ + 3, ptr);
  expect_eq(5, size);

  // skip to end of buffer and peek -- should return false and provide an empty
  // buffer. it does not try to refresh().
  expect_true(coded_input.skip(5));
  coded_input.getdirectbufferpointerinline(&ptr, &size);
  expect_eq(buffer_ + 8, ptr);
  expect_eq(0, size);
}

test_f(codedstreamtest, getdirectbufferpointeroutput) {
  arrayoutputstream output(buffer_, sizeof(buffer_), 8);
  codedoutputstream coded_output(&output);

  void* ptr;
  int size;

  expect_true(coded_output.getdirectbufferpointer(&ptr, &size));
  expect_eq(buffer_, ptr);
  expect_eq(8, size);

  // peeking again should return the same pointer.
  expect_true(coded_output.getdirectbufferpointer(&ptr, &size));
  expect_eq(buffer_, ptr);
  expect_eq(8, size);

  // skip forward in the same buffer then peek again.
  expect_true(coded_output.skip(3));
  expect_true(coded_output.getdirectbufferpointer(&ptr, &size));
  expect_eq(buffer_ + 3, ptr);
  expect_eq(5, size);

  // skip to end of buffer and peek -- should get next buffer.
  expect_true(coded_output.skip(5));
  expect_true(coded_output.getdirectbufferpointer(&ptr, &size));
  expect_eq(buffer_ + 8, ptr);
  expect_eq(8, size);

  // skip over multiple buffers.
  expect_true(coded_output.skip(22));
  expect_true(coded_output.getdirectbufferpointer(&ptr, &size));
  expect_eq(buffer_ + 30, ptr);
  expect_eq(2, size);
}

// -------------------------------------------------------------------
// limits

test_1d(codedstreamtest, basiclimit, kblocksizes) {
  arrayinputstream input(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedinputstream coded_input(&input);

    expect_eq(-1, coded_input.bytesuntillimit());
    codedinputstream::limit limit = coded_input.pushlimit(8);

    // read until we hit the limit.
    uint32 value;
    expect_eq(8, coded_input.bytesuntillimit());
    expect_true(coded_input.readlittleendian32(&value));
    expect_eq(4, coded_input.bytesuntillimit());
    expect_true(coded_input.readlittleendian32(&value));
    expect_eq(0, coded_input.bytesuntillimit());
    expect_false(coded_input.readlittleendian32(&value));
    expect_eq(0, coded_input.bytesuntillimit());

    coded_input.poplimit(limit);

    expect_eq(-1, coded_input.bytesuntillimit());
    expect_true(coded_input.readlittleendian32(&value));
  }

  expect_eq(12, input.bytecount());
}

// test what happens when we push two limits where the second (top) one is
// shorter.
test_1d(codedstreamtest, smalllimitontopofbiglimit, kblocksizes) {
  arrayinputstream input(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedinputstream coded_input(&input);

    expect_eq(-1, coded_input.bytesuntillimit());
    codedinputstream::limit limit1 = coded_input.pushlimit(8);
    expect_eq(8, coded_input.bytesuntillimit());
    codedinputstream::limit limit2 = coded_input.pushlimit(4);

    uint32 value;

    // read until we hit limit2, the top and shortest limit.
    expect_eq(4, coded_input.bytesuntillimit());
    expect_true(coded_input.readlittleendian32(&value));
    expect_eq(0, coded_input.bytesuntillimit());
    expect_false(coded_input.readlittleendian32(&value));
    expect_eq(0, coded_input.bytesuntillimit());

    coded_input.poplimit(limit2);

    // read until we hit limit1.
    expect_eq(4, coded_input.bytesuntillimit());
    expect_true(coded_input.readlittleendian32(&value));
    expect_eq(0, coded_input.bytesuntillimit());
    expect_false(coded_input.readlittleendian32(&value));
    expect_eq(0, coded_input.bytesuntillimit());

    coded_input.poplimit(limit1);

    // no more limits.
    expect_eq(-1, coded_input.bytesuntillimit());
    expect_true(coded_input.readlittleendian32(&value));
  }

  expect_eq(12, input.bytecount());
}

// test what happens when we push two limits where the second (top) one is
// longer.  in this case, the top limit is shortened to match the previous
// limit.
test_1d(codedstreamtest, biglimitontopofsmalllimit, kblocksizes) {
  arrayinputstream input(buffer_, sizeof(buffer_), kblocksizes_case);

  {
    codedinputstream coded_input(&input);

    expect_eq(-1, coded_input.bytesuntillimit());
    codedinputstream::limit limit1 = coded_input.pushlimit(4);
    expect_eq(4, coded_input.bytesuntillimit());
    codedinputstream::limit limit2 = coded_input.pushlimit(8);

    uint32 value;

    // read until we hit limit2.  except, wait!  limit1 is shorter, so
    // we end up hitting that first, despite having 4 bytes to go on
    // limit2.
    expect_eq(4, coded_input.bytesuntillimit());
    expect_true(coded_input.readlittleendian32(&value));
    expect_eq(0, coded_input.bytesuntillimit());
    expect_false(coded_input.readlittleendian32(&value));
    expect_eq(0, coded_input.bytesuntillimit());

    coded_input.poplimit(limit2);

    // ok, popped limit2, now limit1 is on top, which we've already hit.
    expect_eq(0, coded_input.bytesuntillimit());
    expect_false(coded_input.readlittleendian32(&value));
    expect_eq(0, coded_input.bytesuntillimit());

    coded_input.poplimit(limit1);

    // no more limits.
    expect_eq(-1, coded_input.bytesuntillimit());
    expect_true(coded_input.readlittleendian32(&value));
  }

  expect_eq(8, input.bytecount());
}

test_f(codedstreamtest, expectatend) {
  // test expectatend(), which is based on limits.
  arrayinputstream input(buffer_, sizeof(buffer_));
  codedinputstream coded_input(&input);

  expect_false(coded_input.expectatend());

  codedinputstream::limit limit = coded_input.pushlimit(4);

  uint32 value;
  expect_true(coded_input.readlittleendian32(&value));
  expect_true(coded_input.expectatend());

  coded_input.poplimit(limit);
  expect_false(coded_input.expectatend());
}

test_f(codedstreamtest, negativelimit) {
  // check what happens when we push a negative limit.
  arrayinputstream input(buffer_, sizeof(buffer_));
  codedinputstream coded_input(&input);

  codedinputstream::limit limit = coded_input.pushlimit(-1234);
  // bytesuntillimit() returns -1 to mean "no limit", which actually means
  // "the limit is int_max relative to the beginning of the stream".
  expect_eq(-1, coded_input.bytesuntillimit());
  coded_input.poplimit(limit);
}

test_f(codedstreamtest, negativelimitafterreading) {
  // check what happens when we push a negative limit.
  arrayinputstream input(buffer_, sizeof(buffer_));
  codedinputstream coded_input(&input);
  assert_true(coded_input.skip(128));

  codedinputstream::limit limit = coded_input.pushlimit(-64);
  // bytesuntillimit() returns -1 to mean "no limit", which actually means
  // "the limit is int_max relative to the beginning of the stream".
  expect_eq(-1, coded_input.bytesuntillimit());
  coded_input.poplimit(limit);
}

test_f(codedstreamtest, overflowlimit) {
  // check what happens when we push a limit large enough that its absolute
  // position is more than 2gb into the stream.
  arrayinputstream input(buffer_, sizeof(buffer_));
  codedinputstream coded_input(&input);
  assert_true(coded_input.skip(128));

  codedinputstream::limit limit = coded_input.pushlimit(int_max);
  // bytesuntillimit() returns -1 to mean "no limit", which actually means
  // "the limit is int_max relative to the beginning of the stream".
  expect_eq(-1, coded_input.bytesuntillimit());
  coded_input.poplimit(limit);
}

test_f(codedstreamtest, totalbyteslimit) {
  arrayinputstream input(buffer_, sizeof(buffer_));
  codedinputstream coded_input(&input);
  coded_input.settotalbyteslimit(16, -1);

  string str;
  expect_true(coded_input.readstring(&str, 16));

  vector<string> errors;

  {
    scopedmemorylog error_log;
    expect_false(coded_input.readstring(&str, 1));
    errors = error_log.getmessages(error);
  }

  assert_eq(1, errors.size());
  expect_pred_format2(testing::issubstring,
    "a protocol message was rejected because it was too big", errors[0]);

  coded_input.settotalbyteslimit(32, -1);
  expect_true(coded_input.readstring(&str, 16));
}

test_f(codedstreamtest, totalbyteslimitnotvalidmessageend) {
  // total_bytes_limit_ is not a valid place for a message to end.

  arrayinputstream input(buffer_, sizeof(buffer_));
  codedinputstream coded_input(&input);

  // set both total_bytes_limit and a regular limit at 16 bytes.
  coded_input.settotalbyteslimit(16, -1);
  codedinputstream::limit limit = coded_input.pushlimit(16);

  // read 16 bytes.
  string str;
  expect_true(coded_input.readstring(&str, 16));

  // read a tag.  should fail, but report being a valid endpoint since it's
  // a regular limit.
  expect_eq(0, coded_input.readtag());
  expect_true(coded_input.consumedentiremessage());

  // pop the limit.
  coded_input.poplimit(limit);

  // read a tag.  should fail, and report *not* being a valid endpoint, since
  // this time we're hitting the total bytes limit.
  expect_eq(0, coded_input.readtag());
  expect_false(coded_input.consumedentiremessage());
}

// this method is used by the tests below.
// it constructs a codedinputstream with the given limits and tries to read 2kib
// of data from it. then it returns the logged errors and warnings in the given
// vectors.
void codedstreamtest::setuptotalbyteslimitwarningtest(
    int total_bytes_limit, int warning_threshold,
    vector<string>* out_errors, vector<string>* out_warnings) {
  arrayinputstream raw_input(buffer_, sizeof(buffer_), 128);

  scopedmemorylog scoped_log;
  {
    codedinputstream input(&raw_input);
    input.settotalbyteslimit(total_bytes_limit, warning_threshold);
    string str;
    expect_true(input.readstring(&str, 2048));
  }

  *out_errors = scoped_log.getmessages(error);
  *out_warnings = scoped_log.getmessages(warning);
}

test_f(codedstreamtest, totalbyteslimitwarning) {
  vector<string> errors;
  vector<string> warnings;
  setuptotalbyteslimitwarningtest(10240, 1024, &errors, &warnings);

  expect_eq(0, errors.size());

  assert_eq(2, warnings.size());
  expect_pred_format2(testing::issubstring,
    "reading dangerously large protocol message.  if the message turns out to "
    "be larger than 10240 bytes, parsing will be halted for security reasons.",
    warnings[0]);
  expect_pred_format2(testing::issubstring,
    "the total number of bytes read was 2048",
    warnings[1]);
}

test_f(codedstreamtest, totalbyteslimitwarningdisabled) {
  vector<string> errors;
  vector<string> warnings;

  // test with -1
  setuptotalbyteslimitwarningtest(10240, -1, &errors, &warnings);
  expect_eq(0, errors.size());
  expect_eq(0, warnings.size());

  // test again with -2, expecting the same result
  setuptotalbyteslimitwarningtest(10240, -2, &errors, &warnings);
  expect_eq(0, errors.size());
  expect_eq(0, warnings.size());
}


test_f(codedstreamtest, recursionlimit) {
  arrayinputstream input(buffer_, sizeof(buffer_));
  codedinputstream coded_input(&input);
  coded_input.setrecursionlimit(4);

  // this is way too much testing for a counter.
  expect_true(coded_input.incrementrecursiondepth());      // 1
  expect_true(coded_input.incrementrecursiondepth());      // 2
  expect_true(coded_input.incrementrecursiondepth());      // 3
  expect_true(coded_input.incrementrecursiondepth());      // 4
  expect_false(coded_input.incrementrecursiondepth());     // 5
  expect_false(coded_input.incrementrecursiondepth());     // 6
  coded_input.decrementrecursiondepth();                   // 5
  expect_false(coded_input.incrementrecursiondepth());     // 6
  coded_input.decrementrecursiondepth();                   // 5
  coded_input.decrementrecursiondepth();                   // 4
  coded_input.decrementrecursiondepth();                   // 3
  expect_true(coded_input.incrementrecursiondepth());      // 4
  expect_false(coded_input.incrementrecursiondepth());     // 5
  coded_input.decrementrecursiondepth();                   // 4
  coded_input.decrementrecursiondepth();                   // 3
  coded_input.decrementrecursiondepth();                   // 2
  coded_input.decrementrecursiondepth();                   // 1
  coded_input.decrementrecursiondepth();                   // 0
  coded_input.decrementrecursiondepth();                   // 0
  coded_input.decrementrecursiondepth();                   // 0
  expect_true(coded_input.incrementrecursiondepth());      // 1
  expect_true(coded_input.incrementrecursiondepth());      // 2
  expect_true(coded_input.incrementrecursiondepth());      // 3
  expect_true(coded_input.incrementrecursiondepth());      // 4
  expect_false(coded_input.incrementrecursiondepth());     // 5

  coded_input.setrecursionlimit(6);
  expect_true(coded_input.incrementrecursiondepth());      // 6
  expect_false(coded_input.incrementrecursiondepth());     // 7
}


class reallybiginputstream : public zerocopyinputstream {
 public:
  reallybiginputstream() : backup_amount_(0), buffer_count_(0) {}
  ~reallybiginputstream() {}

  // implements zerocopyinputstream ----------------------------------
  bool next(const void** data, int* size) {
    // we only expect backup() to be called at the end.
    expect_eq(0, backup_amount_);

    switch (buffer_count_++) {
      case 0:
        *data = buffer_;
        *size = sizeof(buffer_);
        return true;
      case 1:
        // return an enormously large buffer that, when combined with the 1k
        // returned already, should overflow the total_bytes_read_ counter in
        // codedinputstream.  note that we'll only read the first 1024 bytes
        // of this buffer so it's ok that we have it point at buffer_.
        *data = buffer_;
        *size = int_max;
        return true;
      default:
        return false;
    }
  }

  void backup(int count) {
    backup_amount_ = count;
  }

  bool skip(int count)    { google_log(fatal) << "not implemented."; return false; }
  int64 bytecount() const { google_log(fatal) << "not implemented."; return 0; }

  int backup_amount_;

 private:
  char buffer_[1024];
  int64 buffer_count_;
};

test_f(codedstreamtest, inputover2g) {
  // codedinputstream should gracefully handle input over 2g and call
  // input.backup() with the correct number of bytes on destruction.
  reallybiginputstream input;

  vector<string> errors;

  {
    scopedmemorylog error_log;
    codedinputstream coded_input(&input);
    string str;
    expect_true(coded_input.readstring(&str, 512));
    expect_true(coded_input.readstring(&str, 1024));
    errors = error_log.getmessages(error);
  }

  expect_eq(int_max - 512, input.backup_amount_);
  expect_eq(0, errors.size());
}

// ===================================================================


}  // namespace
}  // namespace io
}  // namespace protobuf
}  // namespace google
