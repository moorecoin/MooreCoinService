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

#include "snappy-test.h"

#ifdef have_windows_h
#define win32_lean_and_mean
#include <windows.h>
#endif

#include <algorithm>

define_bool(run_microbenchmarks, true,
            "run microbenchmarks before doing anything else.");

namespace snappy {

string readtestdatafile(const string& base, size_t size_limit) {
  string contents;
  const char* srcdir = getenv("srcdir");  // this is set by automake.
  string prefix;
  if (srcdir) {
    prefix = string(srcdir) + "/";
  }
  file::getcontents(prefix + "testdata/" + base, &contents, file::defaults()
      ).checksuccess();
  if (size_limit > 0) {
    contents = contents.substr(0, size_limit);
  }
  return contents;
}

string readtestdatafile(const string& base) {
  return readtestdatafile(base, 0);
}

string stringprintf(const char* format, ...) {
  char buf[4096];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);
  return buf;
}

bool benchmark_running = false;
int64 benchmark_real_time_us = 0;
int64 benchmark_cpu_time_us = 0;
string *benchmark_label = null;
int64 benchmark_bytes_processed = 0;

void resetbenchmarktiming() {
  benchmark_real_time_us = 0;
  benchmark_cpu_time_us = 0;
}

#ifdef win32
large_integer benchmark_start_real;
filetime benchmark_start_cpu;
#else  // win32
struct timeval benchmark_start_real;
struct rusage benchmark_start_cpu;
#endif  // win32

void startbenchmarktiming() {
#ifdef win32
  queryperformancecounter(&benchmark_start_real);
  filetime dummy;
  check(getprocesstimes(
      getcurrentprocess(), &dummy, &dummy, &dummy, &benchmark_start_cpu));
#else
  gettimeofday(&benchmark_start_real, null);
  if (getrusage(rusage_self, &benchmark_start_cpu) == -1) {
    perror("getrusage(rusage_self)");
    exit(1);
  }
#endif
  benchmark_running = true;
}

void stopbenchmarktiming() {
  if (!benchmark_running) {
    return;
  }

#ifdef win32
  large_integer benchmark_stop_real;
  large_integer benchmark_frequency;
  queryperformancecounter(&benchmark_stop_real);
  queryperformancefrequency(&benchmark_frequency);

  double elapsed_real = static_cast<double>(
      benchmark_stop_real.quadpart - benchmark_start_real.quadpart) /
      benchmark_frequency.quadpart;
  benchmark_real_time_us += elapsed_real * 1e6 + 0.5;

  filetime benchmark_stop_cpu, dummy;
  check(getprocesstimes(
      getcurrentprocess(), &dummy, &dummy, &dummy, &benchmark_stop_cpu));

  ularge_integer start_ulargeint;
  start_ulargeint.lowpart = benchmark_start_cpu.dwlowdatetime;
  start_ulargeint.highpart = benchmark_start_cpu.dwhighdatetime;

  ularge_integer stop_ulargeint;
  stop_ulargeint.lowpart = benchmark_stop_cpu.dwlowdatetime;
  stop_ulargeint.highpart = benchmark_stop_cpu.dwhighdatetime;

  benchmark_cpu_time_us +=
      (stop_ulargeint.quadpart - start_ulargeint.quadpart + 5) / 10;
#else  // win32
  struct timeval benchmark_stop_real;
  gettimeofday(&benchmark_stop_real, null);
  benchmark_real_time_us +=
      1000000 * (benchmark_stop_real.tv_sec - benchmark_start_real.tv_sec);
  benchmark_real_time_us +=
      (benchmark_stop_real.tv_usec - benchmark_start_real.tv_usec);

  struct rusage benchmark_stop_cpu;
  if (getrusage(rusage_self, &benchmark_stop_cpu) == -1) {
    perror("getrusage(rusage_self)");
    exit(1);
  }
  benchmark_cpu_time_us += 1000000 * (benchmark_stop_cpu.ru_utime.tv_sec -
                                      benchmark_start_cpu.ru_utime.tv_sec);
  benchmark_cpu_time_us += (benchmark_stop_cpu.ru_utime.tv_usec -
                            benchmark_start_cpu.ru_utime.tv_usec);
#endif  // win32

  benchmark_running = false;
}

void setbenchmarklabel(const string& str) {
  if (benchmark_label) {
    delete benchmark_label;
  }
  benchmark_label = new string(str);
}

void setbenchmarkbytesprocessed(int64 bytes) {
  benchmark_bytes_processed = bytes;
}

struct benchmarkrun {
  int64 real_time_us;
  int64 cpu_time_us;
};

struct benchmarkcomparecputime {
  bool operator() (const benchmarkrun& a, const benchmarkrun& b) const {
    return a.cpu_time_us < b.cpu_time_us;
  }
};

void benchmark::run() {
  for (int test_case_num = start_; test_case_num <= stop_; ++test_case_num) {
    // run a few iterations first to find out approximately how fast
    // the benchmark is.
    const int kcalibrateiterations = 100;
    resetbenchmarktiming();
    startbenchmarktiming();
    (*function_)(kcalibrateiterations, test_case_num);
    stopbenchmarktiming();

    // let each test case run for about 200ms, but at least as many
    // as we used to calibrate.
    // run five times and pick the median.
    const int knumruns = 5;
    const int kmedianpos = knumruns / 2;
    int num_iterations = 0;
    if (benchmark_real_time_us > 0) {
      num_iterations = 200000 * kcalibrateiterations / benchmark_real_time_us;
    }
    num_iterations = max(num_iterations, kcalibrateiterations);
    benchmarkrun benchmark_runs[knumruns];

    for (int run = 0; run < knumruns; ++run) {
      resetbenchmarktiming();
      startbenchmarktiming();
      (*function_)(num_iterations, test_case_num);
      stopbenchmarktiming();

      benchmark_runs[run].real_time_us = benchmark_real_time_us;
      benchmark_runs[run].cpu_time_us = benchmark_cpu_time_us;
    }

    string heading = stringprintf("%s/%d", name_.c_str(), test_case_num);
    string human_readable_speed;

    nth_element(benchmark_runs,
                benchmark_runs + kmedianpos,
                benchmark_runs + knumruns,
                benchmarkcomparecputime());
    int64 real_time_us = benchmark_runs[kmedianpos].real_time_us;
    int64 cpu_time_us = benchmark_runs[kmedianpos].cpu_time_us;
    if (cpu_time_us <= 0) {
      human_readable_speed = "?";
    } else {
      int64 bytes_per_second =
          benchmark_bytes_processed * 1000000 / cpu_time_us;
      if (bytes_per_second < 1024) {
        human_readable_speed = stringprintf("%db/s", bytes_per_second);
      } else if (bytes_per_second < 1024 * 1024) {
        human_readable_speed = stringprintf(
            "%.1fkb/s", bytes_per_second / 1024.0f);
      } else if (bytes_per_second < 1024 * 1024 * 1024) {
        human_readable_speed = stringprintf(
            "%.1fmb/s", bytes_per_second / (1024.0f * 1024.0f));
      } else {
        human_readable_speed = stringprintf(
            "%.1fgb/s", bytes_per_second / (1024.0f * 1024.0f * 1024.0f));
      }
    }

    fprintf(stderr,
#ifdef win32
            "%-18s %10i64d %10i64d %10d %s  %s\n",
#else
            "%-18s %10lld %10lld %10d %s  %s\n",
#endif
            heading.c_str(),
            static_cast<long long>(real_time_us * 1000 / num_iterations),
            static_cast<long long>(cpu_time_us * 1000 / num_iterations),
            num_iterations,
            human_readable_speed.c_str(),
            benchmark_label->c_str());
  }
}

#ifdef have_libz

zlib::zlib()
    : comp_init_(false),
      uncomp_init_(false) {
  reinit();
}

zlib::~zlib() {
  if (comp_init_)   { deflateend(&comp_stream_); }
  if (uncomp_init_) { inflateend(&uncomp_stream_); }
}

void zlib::reinit() {
  compression_level_ = z_default_compression;
  window_bits_ = max_wbits;
  mem_level_ =  8;  // def_mem_level
  if (comp_init_) {
    deflateend(&comp_stream_);
    comp_init_ = false;
  }
  if (uncomp_init_) {
    inflateend(&uncomp_stream_);
    uncomp_init_ = false;
  }
  first_chunk_ = true;
}

void zlib::reset() {
  first_chunk_ = true;
}

// --------- compress mode

// initialization method to be called if we hit an error while
// compressing. on hitting an error, call this method before returning
// the error.
void zlib::compresserrorinit() {
  deflateend(&comp_stream_);
  comp_init_ = false;
  reset();
}

int zlib::deflateinit() {
  return deflateinit2(&comp_stream_,
                      compression_level_,
                      z_deflated,
                      window_bits_,
                      mem_level_,
                      z_default_strategy);
}

int zlib::compressinit(bytef *dest, ulongf *destlen,
                       const bytef *source, ulong *sourcelen) {
  int err;

  comp_stream_.next_in = (bytef*)source;
  comp_stream_.avail_in = (uint)*sourcelen;
  if ((ulong)comp_stream_.avail_in != *sourcelen) return z_buf_error;
  comp_stream_.next_out = dest;
  comp_stream_.avail_out = (uint)*destlen;
  if ((ulong)comp_stream_.avail_out != *destlen) return z_buf_error;

  if ( !first_chunk_ )   // only need to set up stream the first time through
    return z_ok;

  if (comp_init_) {      // we've already initted it
    err = deflatereset(&comp_stream_);
    if (err != z_ok) {
      log(warning) << "error: can't reset compress object; creating a new one";
      deflateend(&comp_stream_);
      comp_init_ = false;
    }
  }
  if (!comp_init_) {     // first use
    comp_stream_.zalloc = (alloc_func)0;
    comp_stream_.zfree = (free_func)0;
    comp_stream_.opaque = (voidpf)0;
    err = deflateinit();
    if (err != z_ok) return err;
    comp_init_ = true;
  }
  return z_ok;
}

// in a perfect world we'd always have the full buffer to compress
// when the time came, and we could just call compress().  alas, we
// want to do chunked compression on our webserver.  in this
// application, we compress the header, send it off, then compress the
// results, send them off, then compress the footer.  thus we need to
// use the chunked compression features of zlib.
int zlib::compressatmostorall(bytef *dest, ulongf *destlen,
                              const bytef *source, ulong *sourcelen,
                              int flush_mode) {   // z_full_flush or z_finish
  int err;

  if ( (err=compressinit(dest, destlen, source, sourcelen)) != z_ok )
    return err;

  // this is used to figure out how many bytes we wrote *this chunk*
  int compressed_size = comp_stream_.total_out;

  // some setup happens only for the first chunk we compress in a run
  if ( first_chunk_ ) {
    first_chunk_ = false;
  }

  // flush_mode is z_finish for all mode, z_sync_flush for incremental
  // compression.
  err = deflate(&comp_stream_, flush_mode);

  *sourcelen = comp_stream_.avail_in;

  if ((err == z_stream_end || err == z_ok)
      && comp_stream_.avail_in == 0
      && comp_stream_.avail_out != 0 ) {
    // we processed everything ok and the output buffer was large enough.
    ;
  } else if (err == z_stream_end && comp_stream_.avail_in > 0) {
    return z_buf_error;                            // should never happen
  } else if (err != z_ok && err != z_stream_end && err != z_buf_error) {
    // an error happened
    compresserrorinit();
    return err;
  } else if (comp_stream_.avail_out == 0) {     // not enough space
    err = z_buf_error;
  }

  assert(err == z_ok || err == z_stream_end || err == z_buf_error);
  if (err == z_stream_end)
    err = z_ok;

  // update the crc and other metadata
  compressed_size = comp_stream_.total_out - compressed_size;  // delta
  *destlen = compressed_size;

  return err;
}

int zlib::compresschunkorall(bytef *dest, ulongf *destlen,
                             const bytef *source, ulong sourcelen,
                             int flush_mode) {   // z_full_flush or z_finish
  const int ret =
    compressatmostorall(dest, destlen, source, &sourcelen, flush_mode);
  if (ret == z_buf_error)
    compresserrorinit();
  return ret;
}

// this routine only initializes the compression stream once.  thereafter, it
// just does a deflatereset on the stream, which should be faster.
int zlib::compress(bytef *dest, ulongf *destlen,
                   const bytef *source, ulong sourcelen) {
  int err;
  if ( (err=compresschunkorall(dest, destlen, source, sourcelen,
                               z_finish)) != z_ok )
    return err;
  reset();         // reset for next call to compress

  return z_ok;
}


// --------- uncompress mode

int zlib::inflateinit() {
  return inflateinit2(&uncomp_stream_, max_wbits);
}

// initialization method to be called if we hit an error while
// uncompressing. on hitting an error, call this method before
// returning the error.
void zlib::uncompresserrorinit() {
  inflateend(&uncomp_stream_);
  uncomp_init_ = false;
  reset();
}

int zlib::uncompressinit(bytef *dest, ulongf *destlen,
                         const bytef *source, ulong *sourcelen) {
  int err;

  uncomp_stream_.next_in = (bytef*)source;
  uncomp_stream_.avail_in = (uint)*sourcelen;
  // check for source > 64k on 16-bit machine:
  if ((ulong)uncomp_stream_.avail_in != *sourcelen) return z_buf_error;

  uncomp_stream_.next_out = dest;
  uncomp_stream_.avail_out = (uint)*destlen;
  if ((ulong)uncomp_stream_.avail_out != *destlen) return z_buf_error;

  if ( !first_chunk_ )   // only need to set up stream the first time through
    return z_ok;

  if (uncomp_init_) {    // we've already initted it
    err = inflatereset(&uncomp_stream_);
    if (err != z_ok) {
      log(warning)
        << "error: can't reset uncompress object; creating a new one";
      uncompresserrorinit();
    }
  }
  if (!uncomp_init_) {
    uncomp_stream_.zalloc = (alloc_func)0;
    uncomp_stream_.zfree = (free_func)0;
    uncomp_stream_.opaque = (voidpf)0;
    err = inflateinit();
    if (err != z_ok) return err;
    uncomp_init_ = true;
  }
  return z_ok;
}

// if you compressed your data a chunk at a time, with compresschunk,
// you can uncompress it a chunk at a time with uncompresschunk.
// only difference bewteen chunked and unchunked uncompression
// is the flush mode we use: z_sync_flush (chunked) or z_finish (unchunked).
int zlib::uncompressatmostorall(bytef *dest, ulongf *destlen,
                                const bytef *source, ulong *sourcelen,
                                int flush_mode) {  // z_sync_flush or z_finish
  int err = z_ok;

  if ( (err=uncompressinit(dest, destlen, source, sourcelen)) != z_ok ) {
    log(warning) << "uncompressinit: error: " << err << " sourcelen: "
                 << *sourcelen;
    return err;
  }

  // this is used to figure out how many output bytes we wrote *this chunk*:
  const ulong old_total_out = uncomp_stream_.total_out;

  // this is used to figure out how many input bytes we read *this chunk*:
  const ulong old_total_in = uncomp_stream_.total_in;

  // some setup happens only for the first chunk we compress in a run
  if ( first_chunk_ ) {
    first_chunk_ = false;                          // so we don't do this again

    // for the first chunk *only* (to avoid infinite troubles), we let
    // there be no actual data to uncompress.  this sometimes triggers
    // when the input is only the gzip header, say.
    if ( *sourcelen == 0 ) {
      *destlen = 0;
      return z_ok;
    }
  }

  // we'll uncompress as much as we can.  if we end ok great, otherwise
  // if we get an error that seems to be the gzip footer, we store the
  // gzip footer and return ok, otherwise we return the error.

  // flush_mode is z_sync_flush for chunked mode, z_finish for all mode.
  err = inflate(&uncomp_stream_, flush_mode);

  // figure out how many bytes of the input zlib slurped up:
  const ulong bytes_read = uncomp_stream_.total_in - old_total_in;
  check_le(source + bytes_read, source + *sourcelen);
  *sourcelen = uncomp_stream_.avail_in;

  if ((err == z_stream_end || err == z_ok)  // everything went ok
             && uncomp_stream_.avail_in == 0) {    // and we read it all
    ;
  } else if (err == z_stream_end && uncomp_stream_.avail_in > 0) {
    log(warning)
      << "uncompresschunkorall: received some extra data, bytes total: "
      << uncomp_stream_.avail_in << " bytes: "
      << string(reinterpret_cast<const char *>(uncomp_stream_.next_in),
                min(int(uncomp_stream_.avail_in), 20));
    uncompresserrorinit();
    return z_data_error;       // what's the extra data for?
  } else if (err != z_ok && err != z_stream_end && err != z_buf_error) {
    // an error happened
    log(warning) << "uncompresschunkorall: error: " << err
                 << " avail_out: " << uncomp_stream_.avail_out;
    uncompresserrorinit();
    return err;
  } else if (uncomp_stream_.avail_out == 0) {
    err = z_buf_error;
  }

  assert(err == z_ok || err == z_buf_error || err == z_stream_end);
  if (err == z_stream_end)
    err = z_ok;

  *destlen = uncomp_stream_.total_out - old_total_out;  // size for this call

  return err;
}

int zlib::uncompresschunkorall(bytef *dest, ulongf *destlen,
                               const bytef *source, ulong sourcelen,
                               int flush_mode) {  // z_sync_flush or z_finish
  const int ret =
    uncompressatmostorall(dest, destlen, source, &sourcelen, flush_mode);
  if (ret == z_buf_error)
    uncompresserrorinit();
  return ret;
}

int zlib::uncompressatmost(bytef *dest, ulongf *destlen,
                          const bytef *source, ulong *sourcelen) {
  return uncompressatmostorall(dest, destlen, source, sourcelen, z_sync_flush);
}

// we make sure we've uncompressed everything, that is, the current
// uncompress stream is at a compressed-buffer-eof boundary.  in gzip
// mode, we also check the gzip footer to make sure we pass the gzip
// consistency checks.  we return true iff both types of checks pass.
bool zlib::uncompresschunkdone() {
  assert(!first_chunk_ && uncomp_init_);
  // make sure we're at the end-of-compressed-data point.  this means
  // if we call inflate with z_finish we won't consume any input or
  // write any output
  bytef dummyin, dummyout;
  ulongf dummylen = 0;
  if ( uncompresschunkorall(&dummyout, &dummylen, &dummyin, 0, z_finish)
       != z_ok ) {
    return false;
  }

  // make sure that when we exit, we can start a new round of chunks later
  reset();

  return true;
}

// uncompresses the source buffer into the destination buffer.
// the destination buffer must be long enough to hold the entire
// decompressed contents.
//
// we only initialize the uncomp_stream once.  thereafter, we use
// inflatereset, which should be faster.
//
// returns z_ok on success, otherwise, it returns a zlib error code.
int zlib::uncompress(bytef *dest, ulongf *destlen,
                     const bytef *source, ulong sourcelen) {
  int err;
  if ( (err=uncompresschunkorall(dest, destlen, source, sourcelen,
                                 z_finish)) != z_ok ) {
    reset();                           // let us try to compress again
    return err;
  }
  if ( !uncompresschunkdone() )        // calls reset()
    return z_data_error;
  return z_ok;  // stream_end is ok
}

#endif  // have_libz

}  // namespace snappy
