// copyright 2011 google inc. all rights reserved.
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
//
// various stubs for the unit tests for the open-source version of snappy.

#ifndef util_snappy_opensource_snappy_test_h_
#define util_snappy_opensource_snappy_test_h_

#include <iostream>
#include <string>

#include "snappy-stubs-internal.h"

#include <stdio.h>
#include <stdarg.h>

#ifdef have_sys_mman_h
#include <sys/mman.h>
#endif

#ifdef have_sys_resource_h
#include <sys/resource.h>
#endif

#ifdef have_sys_time_h
#include <sys/time.h>
#endif

#ifdef have_windows_h
#define win32_lean_and_mean
#include <windows.h>
#endif

#include <string>

#ifdef have_gtest

#include <gtest/gtest.h>
#undef typed_test
#define typed_test test
#define init_gtest(argc, argv) ::testing::initgoogletest(argc, *argv)

#else

// stubs for if the user doesn't have google test installed.

#define test(test_case, test_subcase) \
  void test_ ## test_case ## _ ## test_subcase()
#define init_gtest(argc, argv)

#define typed_test test
#define expect_eq check_eq
#define expect_ne check_ne
#define expect_false(cond) check(!(cond))

#endif

#ifdef have_gflags

#include <gflags/gflags.h>

// this is tricky; both gflags and google test want to look at the command line
// arguments. google test seems to be the most happy with unknown arguments,
// though, so we call it first and hope for the best.
#define initgoogle(argv0, argc, argv, remove_flags) \
  init_gtest(argc, argv); \
  google::parsecommandlineflags(argc, argv, remove_flags);

#else

// if we don't have the gflags package installed, these can only be
// changed at compile time.
#define define_int32(flag_name, default_value, description) \
  static int flags_ ## flag_name = default_value;

#define initgoogle(argv0, argc, argv, remove_flags) \
  init_gtest(argc, argv)

#endif

#ifdef have_libz
#include "zlib.h"
#endif

#ifdef have_liblzo2
#include "lzo/lzo1x.h"
#endif

#ifdef have_liblzf
extern "c" {
#include "lzf.h"
}
#endif

#ifdef have_libfastlz
#include "fastlz.h"
#endif

#ifdef have_libquicklz
#include "quicklz.h"
#endif

namespace {

namespace file {
  void init() { }
}  // namespace file

namespace file {
  int defaults() { }

  class dummystatus {
   public:
    void checksuccess() { }
  };

  dummystatus getcontents(const string& filename, string* data, int unused) {
    file* fp = fopen(filename.c_str(), "rb");
    if (fp == null) {
      perror(filename.c_str());
      exit(1);
    }

    data->clear();
    while (!feof(fp)) {
      char buf[4096];
      size_t ret = fread(buf, 1, 4096, fp);
      if (ret == 0 && ferror(fp)) {
        perror("fread");
        exit(1);
      }
      data->append(string(buf, ret));
    }

    fclose(fp);
  }

  dummystatus setcontents(const string& filename,
                          const string& str,
                          int unused) {
    file* fp = fopen(filename.c_str(), "wb");
    if (fp == null) {
      perror(filename.c_str());
      exit(1);
    }

    int ret = fwrite(str.data(), str.size(), 1, fp);
    if (ret != 1) {
      perror("fwrite");
      exit(1);
    }

    fclose(fp);
  }
}  // namespace file

}  // namespace

namespace snappy {

#define flags_test_random_seed 301
typedef string typeparam;

void test_corruptedtest_verifycorrupted();
void test_snappy_simpletests();
void test_snappy_maxblowup();
void test_snappy_randomdata();
void test_snappy_fourbyteoffset();
void test_snappycorruption_truncatedvarint();
void test_snappycorruption_unterminatedvarint();
void test_snappy_readpastendofbuffer();
void test_snappy_findmatchlength();
void test_snappy_findmatchlengthrandom();

string readtestdatafile(const string& base, size_t size_limit);

string readtestdatafile(const string& base);

// a sprintf() variant that returns a std::string.
// not safe for general use due to truncation issues.
string stringprintf(const char* format, ...);

// a simple, non-cryptographically-secure random generator.
class acmrandom {
 public:
  explicit acmrandom(uint32 seed) : seed_(seed) {}

  int32 next();

  int32 uniform(int32 n) {
    return next() % n;
  }
  uint8 rand8() {
    return static_cast<uint8>((next() >> 1) & 0x000000ff);
  }
  bool onein(int x) { return uniform(x) == 0; }

  // skewed: pick "base" uniformly from range [0,max_log] and then
  // return "base" random bits.  the effect is to pick a number in the
  // range [0,2^max_log-1] with bias towards smaller numbers.
  int32 skewed(int max_log);

 private:
  static const uint32 m = 2147483647l;   // 2^31-1
  uint32 seed_;
};

inline int32 acmrandom::next() {
  static const uint64 a = 16807;  // bits 14, 8, 7, 5, 2, 1, 0
  // we are computing
  //       seed_ = (seed_ * a) % m,    where m = 2^31-1
  //
  // seed_ must not be zero or m, or else all subsequent computed values
  // will be zero or m respectively.  for all other values, seed_ will end
  // up cycling through every number in [1,m-1]
  uint64 product = seed_ * a;

  // compute (product % m) using the fact that ((x << 31) % m) == x.
  seed_ = (product >> 31) + (product & m);
  // the first reduction may overflow by 1 bit, so we may need to repeat.
  // mod == m is not possible; using > allows the faster sign-bit-based test.
  if (seed_ > m) {
    seed_ -= m;
  }
  return seed_;
}

inline int32 acmrandom::skewed(int max_log) {
  const int32 base = (next() - 1) % (max_log+1);
  return (next() - 1) & ((1u << base)-1);
}

// a wall-time clock. this stub is not super-accurate, nor resistant to the
// system time changing.
class cycletimer {
 public:
  cycletimer() : real_time_us_(0) {}

  void start() {
#ifdef win32
    queryperformancecounter(&start_);
#else
    gettimeofday(&start_, null);
#endif
  }

  void stop() {
#ifdef win32
    large_integer stop;
    large_integer frequency;
    queryperformancecounter(&stop);
    queryperformancefrequency(&frequency);

    double elapsed = static_cast<double>(stop.quadpart - start_.quadpart) /
        frequency.quadpart;
    real_time_us_ += elapsed * 1e6 + 0.5;
#else
    struct timeval stop;
    gettimeofday(&stop, null);

    real_time_us_ += 1000000 * (stop.tv_sec - start_.tv_sec);
    real_time_us_ += (stop.tv_usec - start_.tv_usec);
#endif
  }

  double get() {
    return real_time_us_ * 1e-6;
  }

 private:
  int64 real_time_us_;
#ifdef win32
  large_integer start_;
#else
  struct timeval start_;
#endif
};

// minimalistic microbenchmark framework.

typedef void (*benchmarkfunction)(int, int);

class benchmark {
 public:
  benchmark(const string& name, benchmarkfunction function) :
      name_(name), function_(function) {}

  benchmark* denserange(int start, int stop) {
    start_ = start;
    stop_ = stop;
    return this;
  }

  void run();

 private:
  const string name_;
  const benchmarkfunction function_;
  int start_, stop_;
};
#define benchmark(benchmark_name) \
  benchmark* benchmark_ ## benchmark_name = \
          (new benchmark(#benchmark_name, benchmark_name))

extern benchmark* benchmark_bm_uflat;
extern benchmark* benchmark_bm_uiovec;
extern benchmark* benchmark_bm_uvalidate;
extern benchmark* benchmark_bm_zflat;

void resetbenchmarktiming();
void startbenchmarktiming();
void stopbenchmarktiming();
void setbenchmarklabel(const string& str);
void setbenchmarkbytesprocessed(int64 bytes);

#ifdef have_libz

// object-oriented wrapper around zlib.
class zlib {
 public:
  zlib();
  ~zlib();

  // wipe a zlib object to a virgin state.  this differs from reset()
  // in that it also breaks any state.
  void reinit();

  // call this to make a zlib buffer as good as new.  here's the only
  // case where they differ:
  //    compresschunk(a); compresschunk(b); compresschunkdone();   vs
  //    compresschunk(a); reset(); compresschunk(b); compresschunkdone();
  // you'll want to use reset(), then, when you interrupt a compress
  // (or uncompress) in the middle of a chunk and want to start over.
  void reset();

  // according to the zlib manual, when you compress, the destination
  // buffer must have size at least src + .1%*src + 12.  this function
  // helps you calculate that.  augment this to account for a potential
  // gzip header and footer, plus a few bytes of slack.
  static int mincompressbufsize(int uncompress_size) {
    return uncompress_size + uncompress_size/1000 + 40;
  }

  // compresses the source buffer into the destination buffer.
  // sourcelen is the byte length of the source buffer.
  // upon entry, destlen is the total size of the destination buffer,
  // which must be of size at least mincompressbufsize(sourcelen).
  // upon exit, destlen is the actual size of the compressed buffer.
  //
  // this function can be used to compress a whole file at once if the
  // input file is mmap'ed.
  //
  // returns z_ok if success, z_mem_error if there was not
  // enough memory, z_buf_error if there was not enough room in the
  // output buffer. note that if the output buffer is exactly the same
  // size as the compressed result, we still return z_buf_error.
  // (check cl#1936076)
  int compress(bytef *dest, ulongf *destlen,
               const bytef *source, ulong sourcelen);

  // uncompresses the source buffer into the destination buffer.
  // the destination buffer must be long enough to hold the entire
  // decompressed contents.
  //
  // returns z_ok on success, otherwise, it returns a zlib error code.
  int uncompress(bytef *dest, ulongf *destlen,
                 const bytef *source, ulong sourcelen);

  // uncompress data one chunk at a time -- ie you can call this
  // more than once.  to get this to work you need to call per-chunk
  // and "done" routines.
  //
  // returns z_ok if success, z_mem_error if there was not
  // enough memory, z_buf_error if there was not enough room in the
  // output buffer.

  int uncompressatmost(bytef *dest, ulongf *destlen,
                       const bytef *source, ulong *sourcelen);

  // checks gzip footer information, as needed.  mostly this just
  // makes sure the checksums match.  whenever you call this, it
  // will assume the last 8 bytes from the previous uncompresschunk
  // call are the footer.  returns true iff everything looks ok.
  bool uncompresschunkdone();

 private:
  int inflateinit();       // sets up the zlib inflate structure
  int deflateinit();       // sets up the zlib deflate structure

  // these init the zlib data structures for compressing/uncompressing
  int compressinit(bytef *dest, ulongf *destlen,
                   const bytef *source, ulong *sourcelen);
  int uncompressinit(bytef *dest, ulongf *destlen,
                     const bytef *source, ulong *sourcelen);
  // initialization method to be called if we hit an error while
  // uncompressing. on hitting an error, call this method before
  // returning the error.
  void uncompresserrorinit();

  // helper function for compress
  int compresschunkorall(bytef *dest, ulongf *destlen,
                         const bytef *source, ulong sourcelen,
                         int flush_mode);
  int compressatmostorall(bytef *dest, ulongf *destlen,
                          const bytef *source, ulong *sourcelen,
                          int flush_mode);

  // likewise for uncompressanduncompresschunk
  int uncompresschunkorall(bytef *dest, ulongf *destlen,
                           const bytef *source, ulong sourcelen,
                           int flush_mode);

  int uncompressatmostorall(bytef *dest, ulongf *destlen,
                            const bytef *source, ulong *sourcelen,
                            int flush_mode);

  // initialization method to be called if we hit an error while
  // compressing. on hitting an error, call this method before
  // returning the error.
  void compresserrorinit();

  int compression_level_;   // compression level
  int window_bits_;         // log base 2 of the window size used in compression
  int mem_level_;           // specifies the amount of memory to be used by
                            // compressor (1-9)
  z_stream comp_stream_;    // zlib stream data structure
  bool comp_init_;          // true if we have initialized comp_stream_
  z_stream uncomp_stream_;  // zlib stream data structure
  bool uncomp_init_;        // true if we have initialized uncomp_stream_

  // these are used only with chunked compression.
  bool first_chunk_;       // true if we need to emit headers with this chunk
};

#endif  // have_libz

}  // namespace snappy

declare_bool(run_microbenchmarks);

static void runspecifiedbenchmarks() {
  if (!flags_run_microbenchmarks) {
    return;
  }

  fprintf(stderr, "running microbenchmarks.\n");
#ifndef ndebug
  fprintf(stderr, "warning: compiled with assertions enabled, will be slow.\n");
#endif
#ifndef __optimize__
  fprintf(stderr, "warning: compiled without optimization, will be slow.\n");
#endif
  fprintf(stderr, "benchmark            time(ns)    cpu(ns) iterations\n");
  fprintf(stderr, "---------------------------------------------------\n");

  snappy::benchmark_bm_uflat->run();
  snappy::benchmark_bm_uiovec->run();
  snappy::benchmark_bm_uvalidate->run();
  snappy::benchmark_bm_zflat->run();

  fprintf(stderr, "\n");
}

#ifndef have_gtest

static inline int run_all_tests() {
  fprintf(stderr, "running correctness tests.\n");
  snappy::test_corruptedtest_verifycorrupted();
  snappy::test_snappy_simpletests();
  snappy::test_snappy_maxblowup();
  snappy::test_snappy_randomdata();
  snappy::test_snappy_fourbyteoffset();
  snappy::test_snappycorruption_truncatedvarint();
  snappy::test_snappycorruption_unterminatedvarint();
  snappy::test_snappy_readpastendofbuffer();
  snappy::test_snappy_findmatchlength();
  snappy::test_snappy_findmatchlengthrandom();
  fprintf(stderr, "all tests passed.\n");

  return 0;
}

#endif  // have_gtest

// for main().
namespace snappy {

static void compressfile(const char* fname);
static void uncompressfile(const char* fname);
static void measurefile(const char* fname);

// logging.

#define log(level) logmessage()
#define vlog(level) true ? (void)0 : \
    snappy::logmessagevoidify() & snappy::logmessage()

class logmessage {
 public:
  logmessage() { }
  ~logmessage() {
    cerr << endl;
  }

  logmessage& operator<<(const std::string& msg) {
    cerr << msg;
    return *this;
  }
  logmessage& operator<<(int x) {
    cerr << x;
    return *this;
  }
};

// asserts, both versions activated in debug mode only,
// and ones that are always active.

#define crash_unless(condition) \
    predict_true(condition) ? (void)0 : \
    snappy::logmessagevoidify() & snappy::logmessagecrash()

class logmessagecrash : public logmessage {
 public:
  logmessagecrash() { }
  ~logmessagecrash() {
    cerr << endl;
    abort();
  }
};

// this class is used to explicitly ignore values in the conditional
// logging macros.  this avoids compiler warnings like "value computed
// is not used" and "statement has no effect".

class logmessagevoidify {
 public:
  logmessagevoidify() { }
  // this has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(const logmessage&) { }
};

#define check(cond) crash_unless(cond)
#define check_le(a, b) crash_unless((a) <= (b))
#define check_ge(a, b) crash_unless((a) >= (b))
#define check_eq(a, b) crash_unless((a) == (b))
#define check_ne(a, b) crash_unless((a) != (b))
#define check_lt(a, b) crash_unless((a) < (b))
#define check_gt(a, b) crash_unless((a) > (b))

}  // namespace

using snappy::compressfile;
using snappy::uncompressfile;
using snappy::measurefile;

#endif  // util_snappy_opensource_snappy_test_h_
