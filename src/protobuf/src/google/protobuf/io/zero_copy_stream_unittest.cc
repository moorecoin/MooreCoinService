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
// testing strategy:  for each type of i/o (array, string, file, etc.) we
// create an output stream and write some data to it, then create a
// corresponding input stream to read the same data back and expect it to
// match.  when the data is written, it is written in several small chunks
// of varying sizes, with a backup() after each chunk.  it is read back
// similarly, but with chunks separated at different points.  the whole
// process is run with a variety of block sizes for both the input and
// the output.
//
// todo(kenton):  rewrite this test to bring it up to the standards of all
//   the other proto2 tests.  may want to wait for gtest to implement
//   "parametized tests" so that one set of tests can be used on all the
//   implementations.

#include "config.h"

#ifdef _msc_ver
#include <io.h>
#else
#include <unistd.h>
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sstream>

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>

#if have_zlib
#include <google/protobuf/io/gzip_stream.h>
#endif

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <google/protobuf/testing/file.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace io {
namespace {

#ifdef _win32
#define pipe(fds) _pipe(fds, 4096, o_binary)
#endif

#ifndef o_binary
#ifdef _o_binary
#define o_binary _o_binary
#else
#define o_binary 0     // if this isn't defined, the platform doesn't need it.
#endif
#endif

class iotest : public testing::test {
 protected:
  // test helpers.

  // helper to write an array of data to an output stream.
  bool writetooutput(zerocopyoutputstream* output, const void* data, int size);
  // helper to read a fixed-length array of data from an input stream.
  int readfrominput(zerocopyinputstream* input, void* data, int size);
  // write a string to the output stream.
  void writestring(zerocopyoutputstream* output, const string& str);
  // read a number of bytes equal to the size of the given string and checks
  // that it matches the string.
  void readstring(zerocopyinputstream* input, const string& str);
  // writes some text to the output stream in a particular order.  returns
  // the number of bytes written, incase the caller needs that to set up an
  // input stream.
  int writestuff(zerocopyoutputstream* output);
  // reads text from an input stream and expects it to match what
  // writestuff() writes.
  void readstuff(zerocopyinputstream* input);

  // similar to writestuff, but performs more sophisticated testing.
  int writestufflarge(zerocopyoutputstream* output);
  // reads and tests a stream that should have been written to
  // via writestufflarge().
  void readstufflarge(zerocopyinputstream* input);

#if have_zlib
  string compress(const string& data, const gzipoutputstream::options& options);
  string uncompress(const string& data);
#endif

  static const int kblocksizes[];
  static const int kblocksizecount;
};

const int iotest::kblocksizes[] = {-1, 1, 2, 5, 7, 10, 23, 64};
const int iotest::kblocksizecount = google_arraysize(iotest::kblocksizes);

bool iotest::writetooutput(zerocopyoutputstream* output,
                           const void* data, int size) {
  const uint8* in = reinterpret_cast<const uint8*>(data);
  int in_size = size;

  void* out;
  int out_size;

  while (true) {
    if (!output->next(&out, &out_size)) {
      return false;
    }
    expect_gt(out_size, 0);

    if (in_size <= out_size) {
      memcpy(out, in, in_size);
      output->backup(out_size - in_size);
      return true;
    }

    memcpy(out, in, out_size);
    in += out_size;
    in_size -= out_size;
  }
}

#define max_repeated_zeros 100

int iotest::readfrominput(zerocopyinputstream* input, void* data, int size) {
  uint8* out = reinterpret_cast<uint8*>(data);
  int out_size = size;

  const void* in;
  int in_size = 0;

  int repeated_zeros = 0;

  while (true) {
    if (!input->next(&in, &in_size)) {
      return size - out_size;
    }
    expect_gt(in_size, -1);
    if (in_size == 0) {
      repeated_zeros++;
    } else {
      repeated_zeros = 0;
    }
    expect_lt(repeated_zeros, max_repeated_zeros);

    if (out_size <= in_size) {
      memcpy(out, in, out_size);
      if (in_size > out_size) {
        input->backup(in_size - out_size);
      }
      return size;  // copied all of it.
    }

    memcpy(out, in, in_size);
    out += in_size;
    out_size -= in_size;
  }
}

void iotest::writestring(zerocopyoutputstream* output, const string& str) {
  expect_true(writetooutput(output, str.c_str(), str.size()));
}

void iotest::readstring(zerocopyinputstream* input, const string& str) {
  scoped_array<char> buffer(new char[str.size() + 1]);
  buffer[str.size()] = '\0';
  expect_eq(readfrominput(input, buffer.get(), str.size()), str.size());
  expect_streq(str.c_str(), buffer.get());
}

int iotest::writestuff(zerocopyoutputstream* output) {
  writestring(output, "hello world!\n");
  writestring(output, "some te");
  writestring(output, "xt.  blah blah.");
  writestring(output, "abcdefg");
  writestring(output, "01234567890123456789");
  writestring(output, "foobar");

  expect_eq(output->bytecount(), 68);

  int result = output->bytecount();
  return result;
}

// reads text from an input stream and expects it to match what writestuff()
// writes.
void iotest::readstuff(zerocopyinputstream* input) {
  readstring(input, "hello world!\n");
  readstring(input, "some text.  ");
  readstring(input, "blah ");
  readstring(input, "blah.");
  readstring(input, "abcdefg");
  expect_true(input->skip(20));
  readstring(input, "foo");
  readstring(input, "bar");

  expect_eq(input->bytecount(), 68);

  uint8 byte;
  expect_eq(readfrominput(input, &byte, 1), 0);
}

int iotest::writestufflarge(zerocopyoutputstream* output) {
  writestring(output, "hello world!\n");
  writestring(output, "some te");
  writestring(output, "xt.  blah blah.");
  writestring(output, string(100000, 'x'));  // a very long string
  writestring(output, string(100000, 'y'));  // a very long string
  writestring(output, "01234567890123456789");

  expect_eq(output->bytecount(), 200055);

  int result = output->bytecount();
  return result;
}

// reads text from an input stream and expects it to match what writestuff()
// writes.
void iotest::readstufflarge(zerocopyinputstream* input) {
  readstring(input, "hello world!\nsome text.  ");
  expect_true(input->skip(5));
  readstring(input, "blah.");
  expect_true(input->skip(100000 - 10));
  readstring(input, string(10, 'x') + string(100000 - 20000, 'y'));
  expect_true(input->skip(20000 - 10));
  readstring(input, "yyyyyyyyyy01234567890123456789");

  expect_eq(input->bytecount(), 200055);

  uint8 byte;
  expect_eq(readfrominput(input, &byte, 1), 0);
}

// ===================================================================

test_f(iotest, arrayio) {
  const int kbuffersize = 256;
  uint8 buffer[kbuffersize];

  for (int i = 0; i < kblocksizecount; i++) {
    for (int j = 0; j < kblocksizecount; j++) {
      int size;
      {
        arrayoutputstream output(buffer, kbuffersize, kblocksizes[i]);
        size = writestuff(&output);
      }
      {
        arrayinputstream input(buffer, size, kblocksizes[j]);
        readstuff(&input);
      }
    }
  }
}

test_f(iotest, twosessionwrite) {
  // test that two concatenated write sessions read correctly

  static const char* stra = "0123456789";
  static const char* strb = "whirledpeas";
  const int kbuffersize = 2*1024;
  uint8* buffer = new uint8[kbuffersize];
  char* temp_buffer = new char[40];

  for (int i = 0; i < kblocksizecount; i++) {
    for (int j = 0; j < kblocksizecount; j++) {
      arrayoutputstream* output =
          new arrayoutputstream(buffer, kbuffersize, kblocksizes[i]);
      codedoutputstream* coded_output = new codedoutputstream(output);
      coded_output->writevarint32(strlen(stra));
      coded_output->writeraw(stra, strlen(stra));
      delete coded_output;  // flush
      int64 pos = output->bytecount();
      delete output;
      output = new arrayoutputstream(
          buffer + pos, kbuffersize - pos, kblocksizes[i]);
      coded_output = new codedoutputstream(output);
      coded_output->writevarint32(strlen(strb));
      coded_output->writeraw(strb, strlen(strb));
      delete coded_output;  // flush
      int64 size = pos + output->bytecount();
      delete output;

      arrayinputstream* input =
          new arrayinputstream(buffer, size, kblocksizes[j]);
      codedinputstream* coded_input = new codedinputstream(input);
      uint32 insize;
      expect_true(coded_input->readvarint32(&insize));
      expect_eq(strlen(stra), insize);
      expect_true(coded_input->readraw(temp_buffer, insize));
      expect_eq(0, memcmp(temp_buffer, stra, insize));

      expect_true(coded_input->readvarint32(&insize));
      expect_eq(strlen(strb), insize);
      expect_true(coded_input->readraw(temp_buffer, insize));
      expect_eq(0, memcmp(temp_buffer, strb, insize));

      delete coded_input;
      delete input;
    }
  }

  delete [] temp_buffer;
  delete [] buffer;
}

#if have_zlib
test_f(iotest, gzipio) {
  const int kbuffersize = 2*1024;
  uint8* buffer = new uint8[kbuffersize];
  for (int i = 0; i < kblocksizecount; i++) {
    for (int j = 0; j < kblocksizecount; j++) {
      for (int z = 0; z < kblocksizecount; z++) {
        int gzip_buffer_size = kblocksizes[z];
        int size;
        {
          arrayoutputstream output(buffer, kbuffersize, kblocksizes[i]);
          gzipoutputstream::options options;
          options.format = gzipoutputstream::gzip;
          if (gzip_buffer_size != -1) {
            options.buffer_size = gzip_buffer_size;
          }
          gzipoutputstream gzout(&output, options);
          writestuff(&gzout);
          gzout.close();
          size = output.bytecount();
        }
        {
          arrayinputstream input(buffer, size, kblocksizes[j]);
          gzipinputstream gzin(
              &input, gzipinputstream::gzip, gzip_buffer_size);
          readstuff(&gzin);
        }
      }
    }
  }
  delete [] buffer;
}

test_f(iotest, gzipiowithflush) {
  const int kbuffersize = 2*1024;
  uint8* buffer = new uint8[kbuffersize];
  // we start with i = 4 as we want a block size > 6. with block size <= 6
  // flush() fills up the entire 2k buffer with flush markers and the test
  // fails. see documentation for flush() for more detail.
  for (int i = 4; i < kblocksizecount; i++) {
    for (int j = 0; j < kblocksizecount; j++) {
      for (int z = 0; z < kblocksizecount; z++) {
        int gzip_buffer_size = kblocksizes[z];
        int size;
        {
          arrayoutputstream output(buffer, kbuffersize, kblocksizes[i]);
          gzipoutputstream::options options;
          options.format = gzipoutputstream::gzip;
          if (gzip_buffer_size != -1) {
            options.buffer_size = gzip_buffer_size;
          }
          gzipoutputstream gzout(&output, options);
          writestuff(&gzout);
          expect_true(gzout.flush());
          gzout.close();
          size = output.bytecount();
        }
        {
          arrayinputstream input(buffer, size, kblocksizes[j]);
          gzipinputstream gzin(
              &input, gzipinputstream::gzip, gzip_buffer_size);
          readstuff(&gzin);
        }
      }
    }
  }
  delete [] buffer;
}

test_f(iotest, gzipiocontiguousflushes) {
  const int kbuffersize = 2*1024;
  uint8* buffer = new uint8[kbuffersize];

  int block_size = kblocksizes[4];
  int gzip_buffer_size = block_size;
  int size;

  arrayoutputstream output(buffer, kbuffersize, block_size);
  gzipoutputstream::options options;
  options.format = gzipoutputstream::gzip;
  if (gzip_buffer_size != -1) {
    options.buffer_size = gzip_buffer_size;
  }
  gzipoutputstream gzout(&output, options);
  writestuff(&gzout);
  expect_true(gzout.flush());
  expect_true(gzout.flush());
  gzout.close();
  size = output.bytecount();

  arrayinputstream input(buffer, size, block_size);
  gzipinputstream gzin(
      &input, gzipinputstream::gzip, gzip_buffer_size);
  readstuff(&gzin);

  delete [] buffer;
}

test_f(iotest, gzipioreadafterflush) {
  const int kbuffersize = 2*1024;
  uint8* buffer = new uint8[kbuffersize];

  int block_size = kblocksizes[4];
  int gzip_buffer_size = block_size;
  int size;
  arrayoutputstream output(buffer, kbuffersize, block_size);
  gzipoutputstream::options options;
  options.format = gzipoutputstream::gzip;
  if (gzip_buffer_size != -1) {
    options.buffer_size = gzip_buffer_size;
  }

  gzipoutputstream gzout(&output, options);
  writestuff(&gzout);
  expect_true(gzout.flush());
  size = output.bytecount();

  arrayinputstream input(buffer, size, block_size);
  gzipinputstream gzin(
      &input, gzipinputstream::gzip, gzip_buffer_size);
  readstuff(&gzin);

  gzout.close();

  delete [] buffer;
}

test_f(iotest, zlibio) {
  const int kbuffersize = 2*1024;
  uint8* buffer = new uint8[kbuffersize];
  for (int i = 0; i < kblocksizecount; i++) {
    for (int j = 0; j < kblocksizecount; j++) {
      for (int z = 0; z < kblocksizecount; z++) {
        int gzip_buffer_size = kblocksizes[z];
        int size;
        {
          arrayoutputstream output(buffer, kbuffersize, kblocksizes[i]);
          gzipoutputstream::options options;
          options.format = gzipoutputstream::zlib;
          if (gzip_buffer_size != -1) {
            options.buffer_size = gzip_buffer_size;
          }
          gzipoutputstream gzout(&output, options);
          writestuff(&gzout);
          gzout.close();
          size = output.bytecount();
        }
        {
          arrayinputstream input(buffer, size, kblocksizes[j]);
          gzipinputstream gzin(
              &input, gzipinputstream::zlib, gzip_buffer_size);
          readstuff(&gzin);
        }
      }
    }
  }
  delete [] buffer;
}

test_f(iotest, zlibioinputautodetect) {
  const int kbuffersize = 2*1024;
  uint8* buffer = new uint8[kbuffersize];
  int size;
  {
    arrayoutputstream output(buffer, kbuffersize);
    gzipoutputstream::options options;
    options.format = gzipoutputstream::zlib;
    gzipoutputstream gzout(&output, options);
    writestuff(&gzout);
    gzout.close();
    size = output.bytecount();
  }
  {
    arrayinputstream input(buffer, size);
    gzipinputstream gzin(&input, gzipinputstream::auto);
    readstuff(&gzin);
  }
  {
    arrayoutputstream output(buffer, kbuffersize);
    gzipoutputstream::options options;
    options.format = gzipoutputstream::gzip;
    gzipoutputstream gzout(&output, options);
    writestuff(&gzout);
    gzout.close();
    size = output.bytecount();
  }
  {
    arrayinputstream input(buffer, size);
    gzipinputstream gzin(&input, gzipinputstream::auto);
    readstuff(&gzin);
  }
  delete [] buffer;
}

string iotest::compress(const string& data,
                        const gzipoutputstream::options& options) {
  string result;
  {
    stringoutputstream output(&result);
    gzipoutputstream gzout(&output, options);
    writetooutput(&gzout, data.data(), data.size());
  }
  return result;
}

string iotest::uncompress(const string& data) {
  string result;
  {
    arrayinputstream input(data.data(), data.size());
    gzipinputstream gzin(&input);
    const void* buffer;
    int size;
    while (gzin.next(&buffer, &size)) {
      result.append(reinterpret_cast<const char*>(buffer), size);
    }
  }
  return result;
}

test_f(iotest, compressionoptions) {
  // some ad-hoc testing of compression options.

  string golden;
  file::readfiletostringordie(
    testsourcedir() + "/google/protobuf/testdata/golden_message",
    &golden);

  gzipoutputstream::options options;
  string gzip_compressed = compress(golden, options);

  options.compression_level = 0;
  string not_compressed = compress(golden, options);

  // try zlib compression for fun.
  options = gzipoutputstream::options();
  options.format = gzipoutputstream::zlib;
  string zlib_compressed = compress(golden, options);

  // uncompressed should be bigger than the original since it should have some
  // sort of header.
  expect_gt(not_compressed.size(), golden.size());

  // higher compression levels should result in smaller sizes.
  expect_lt(zlib_compressed.size(), not_compressed.size());

  // zlib format should differ from gzip format.
  expect_true(zlib_compressed != gzip_compressed);

  // everything should decompress correctly.
  expect_true(uncompress(not_compressed) == golden);
  expect_true(uncompress(gzip_compressed) == golden);
  expect_true(uncompress(zlib_compressed) == golden);
}

test_f(iotest, twosessionwritegzip) {
  // test that two concatenated gzip streams can be read correctly

  static const char* stra = "0123456789";
  static const char* strb = "quickbrownfox";
  const int kbuffersize = 2*1024;
  uint8* buffer = new uint8[kbuffersize];
  char* temp_buffer = new char[40];

  for (int i = 0; i < kblocksizecount; i++) {
    for (int j = 0; j < kblocksizecount; j++) {
      arrayoutputstream* output =
          new arrayoutputstream(buffer, kbuffersize, kblocksizes[i]);
      gzipoutputstream* gzout = new gzipoutputstream(output);
      codedoutputstream* coded_output = new codedoutputstream(gzout);
      int32 outlen = strlen(stra) + 1;
      coded_output->writevarint32(outlen);
      coded_output->writeraw(stra, outlen);
      delete coded_output;  // flush
      delete gzout;  // flush
      int64 pos = output->bytecount();
      delete output;
      output = new arrayoutputstream(
          buffer + pos, kbuffersize - pos, kblocksizes[i]);
      gzout = new gzipoutputstream(output);
      coded_output = new codedoutputstream(gzout);
      outlen = strlen(strb) + 1;
      coded_output->writevarint32(outlen);
      coded_output->writeraw(strb, outlen);
      delete coded_output;  // flush
      delete gzout;  // flush
      int64 size = pos + output->bytecount();
      delete output;

      arrayinputstream* input =
          new arrayinputstream(buffer, size, kblocksizes[j]);
      gzipinputstream* gzin = new gzipinputstream(input);
      codedinputstream* coded_input = new codedinputstream(gzin);
      uint32 insize;
      expect_true(coded_input->readvarint32(&insize));
      expect_eq(strlen(stra) + 1, insize);
      expect_true(coded_input->readraw(temp_buffer, insize));
      expect_eq(0, memcmp(temp_buffer, stra, insize))
          << "stra=" << stra << " in=" << temp_buffer;

      expect_true(coded_input->readvarint32(&insize));
      expect_eq(strlen(strb) + 1, insize);
      expect_true(coded_input->readraw(temp_buffer, insize));
      expect_eq(0, memcmp(temp_buffer, strb, insize))
          << " out_block_size=" << kblocksizes[i]
          << " in_block_size=" << kblocksizes[j]
          << " pos=" << pos
          << " size=" << size
          << " strb=" << strb << " in=" << temp_buffer;

      delete coded_input;
      delete gzin;
      delete input;
    }
  }

  delete [] temp_buffer;
  delete [] buffer;
}
#endif

// there is no string input, only string output.  also, it doesn't support
// explicit block sizes.  so, we'll only run one test and we'll use
// arrayinput to read back the results.
test_f(iotest, stringio) {
  string str;
  {
    stringoutputstream output(&str);
    writestuff(&output);
  }
  {
    arrayinputstream input(str.data(), str.size());
    readstuff(&input);
  }
}


// to test files, we create a temporary file, write, read, truncate, repeat.
test_f(iotest, fileio) {
  string filename = testtempdir() + "/zero_copy_stream_test_file";

  for (int i = 0; i < kblocksizecount; i++) {
    for (int j = 0; j < kblocksizecount; j++) {
      // make a temporary file.
      int file =
        open(filename.c_str(), o_rdwr | o_creat | o_trunc | o_binary, 0777);
      assert_ge(file, 0);

      {
        fileoutputstream output(file, kblocksizes[i]);
        writestuff(&output);
        expect_eq(0, output.geterrno());
      }

      // rewind.
      assert_ne(lseek(file, 0, seek_set), (off_t)-1);

      {
        fileinputstream input(file, kblocksizes[j]);
        readstuff(&input);
        expect_eq(0, input.geterrno());
      }

      close(file);
    }
  }
}

#if have_zlib
test_f(iotest, gzipfileio) {
  string filename = testtempdir() + "/zero_copy_stream_test_file";

  for (int i = 0; i < kblocksizecount; i++) {
    for (int j = 0; j < kblocksizecount; j++) {
      // make a temporary file.
      int file =
        open(filename.c_str(), o_rdwr | o_creat | o_trunc | o_binary, 0777);
      assert_ge(file, 0);
      {
        fileoutputstream output(file, kblocksizes[i]);
        gzipoutputstream gzout(&output);
        writestufflarge(&gzout);
        gzout.close();
        output.flush();
        expect_eq(0, output.geterrno());
      }

      // rewind.
      assert_ne(lseek(file, 0, seek_set), (off_t)-1);

      {
        fileinputstream input(file, kblocksizes[j]);
        gzipinputstream gzin(&input);
        readstufflarge(&gzin);
        expect_eq(0, input.geterrno());
      }

      close(file);
    }
  }
}
#endif

// msvc raises various debugging exceptions if we try to use a file
// descriptor of -1, defeating our tests below.  this class will disable
// these debug assertions while in scope.
class msvcdebugdisabler {
 public:
#if defined(_msc_ver) && _msc_ver >= 1400
  msvcdebugdisabler() {
    old_handler_ = _set_invalid_parameter_handler(myhandler);
    old_mode_ = _crtsetreportmode(_crt_assert, 0);
  }
  ~msvcdebugdisabler() {
    old_handler_ = _set_invalid_parameter_handler(old_handler_);
    old_mode_ = _crtsetreportmode(_crt_assert, old_mode_);
  }

  static void myhandler(const wchar_t *expr,
                        const wchar_t *func,
                        const wchar_t *file,
                        unsigned int line,
                        uintptr_t preserved) {
    // do nothing
  }

  _invalid_parameter_handler old_handler_;
  int old_mode_;
#else
  // dummy constructor and destructor to ensure that gcc doesn't complain
  // that debug_disabler is an unused variable.
  msvcdebugdisabler() {}
  ~msvcdebugdisabler() {}
#endif
};

// test that fileinputstreams report errors correctly.
test_f(iotest, filereaderror) {
  msvcdebugdisabler debug_disabler;

  // -1 = invalid file descriptor.
  fileinputstream input(-1);

  const void* buffer;
  int size;
  expect_false(input.next(&buffer, &size));
  expect_eq(ebadf, input.geterrno());
}

// test that fileoutputstreams report errors correctly.
test_f(iotest, filewriteerror) {
  msvcdebugdisabler debug_disabler;

  // -1 = invalid file descriptor.
  fileoutputstream input(-1);

  void* buffer;
  int size;

  // the first call to next() succeeds because it doesn't have anything to
  // write yet.
  expect_true(input.next(&buffer, &size));

  // second call fails.
  expect_false(input.next(&buffer, &size));

  expect_eq(ebadf, input.geterrno());
}

// pipes are not seekable, so file{input,output}stream ends up doing some
// different things to handle them.  we'll test by writing to a pipe and
// reading back from it.
test_f(iotest, pipeio) {
  int files[2];

  for (int i = 0; i < kblocksizecount; i++) {
    for (int j = 0; j < kblocksizecount; j++) {
      // need to create a new pipe each time because readstuff() expects
      // to see eof at the end.
      assert_eq(pipe(files), 0);

      {
        fileoutputstream output(files[1], kblocksizes[i]);
        writestuff(&output);
        expect_eq(0, output.geterrno());
      }
      close(files[1]);  // send eof.

      {
        fileinputstream input(files[0], kblocksizes[j]);
        readstuff(&input);
        expect_eq(0, input.geterrno());
      }
      close(files[0]);
    }
  }
}

// test using c++ iostreams.
test_f(iotest, iostreamio) {
  for (int i = 0; i < kblocksizecount; i++) {
    for (int j = 0; j < kblocksizecount; j++) {
      {
        stringstream stream;

        {
          ostreamoutputstream output(&stream, kblocksizes[i]);
          writestuff(&output);
          expect_false(stream.fail());
        }

        {
          istreaminputstream input(&stream, kblocksizes[j]);
          readstuff(&input);
          expect_true(stream.eof());
        }
      }

      {
        stringstream stream;

        {
          ostreamoutputstream output(&stream, kblocksizes[i]);
          writestufflarge(&output);
          expect_false(stream.fail());
        }

        {
          istreaminputstream input(&stream, kblocksizes[j]);
          readstufflarge(&input);
          expect_true(stream.eof());
        }
      }
    }
  }
}

// to test concatenatinginputstream, we create several arrayinputstreams
// covering a buffer and then concatenate them.
test_f(iotest, concatenatinginputstream) {
  const int kbuffersize = 256;
  uint8 buffer[kbuffersize];

  // fill the buffer.
  arrayoutputstream output(buffer, kbuffersize);
  writestuff(&output);

  // now split it up into multiple streams of varying sizes.
  assert_eq(68, output.bytecount());  // test depends on this.
  arrayinputstream input1(buffer     , 12);
  arrayinputstream input2(buffer + 12,  7);
  arrayinputstream input3(buffer + 19,  6);
  arrayinputstream input4(buffer + 25, 15);
  arrayinputstream input5(buffer + 40,  0);
  // note:  we want to make sure we have a stream boundary somewhere between
  // bytes 42 and 62, which is the range that it skip()ed by readstuff().  this
  // tests that a bug that existed in the original code for skip() is fixed.
  arrayinputstream input6(buffer + 40, 10);
  arrayinputstream input7(buffer + 50, 18);  // total = 68 bytes.

  zerocopyinputstream* streams[] =
    {&input1, &input2, &input3, &input4, &input5, &input6, &input7};

  // create the concatenating stream and read.
  concatenatinginputstream input(streams, google_arraysize(streams));
  readstuff(&input);
}

// to test limitinginputstream, we write our golden text to a buffer, then
// create an arrayinputstream that contains the whole buffer (not just the
// bytes written), then use a limitinginputstream to limit it just to the
// bytes written.
test_f(iotest, limitinginputstream) {
  const int kbuffersize = 256;
  uint8 buffer[kbuffersize];

  // fill the buffer.
  arrayoutputstream output(buffer, kbuffersize);
  writestuff(&output);

  // set up input.
  arrayinputstream array_input(buffer, kbuffersize);
  limitinginputstream input(&array_input, output.bytecount());

  readstuff(&input);
}

// check that a zero-size array doesn't confuse the code.
test(zerosizearray, input) {
  arrayinputstream input(null, 0);
  const void* data;
  int size;
  expect_false(input.next(&data, &size));
}

test(zerosizearray, output) {
  arrayoutputstream output(null, 0);
  void* data;
  int size;
  expect_false(output.next(&data, &size));
}

}  // namespace
}  // namespace io
}  // namespace protobuf
}  // namespace google
