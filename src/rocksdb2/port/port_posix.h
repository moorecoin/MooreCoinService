//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// see port_example.h for documentation for the following types/functions.

#ifndef storage_leveldb_port_port_posix_h_
#define storage_leveldb_port_port_posix_h_

#undef platform_is_little_endian
#if defined(os_macosx)
  #include <machine/endian.h>
  #if defined(__darwin_little_endian) && defined(__darwin_byte_order)
    #define platform_is_little_endian \
        (__darwin_byte_order == __darwin_little_endian)
  #endif
#elif defined(os_solaris)
  #include <sys/isa_defs.h>
  #ifdef _little_endian
    #define platform_is_little_endian true
  #else
    #define platform_is_little_endian false
  #endif
#elif defined(os_freebsd) || defined(os_openbsd) || defined(os_netbsd) ||\
      defined(os_dragonflybsd) || defined(os_android)
  #include <sys/types.h>
  #include <sys/endian.h>
#else
  #include <endian.h>
#endif
#include <pthread.h>
#ifdef snappy
#include <snappy.h>
#endif

#ifdef zlib
#include <zlib.h>
#endif

#ifdef bzip2
#include <bzlib.h>
#endif

#if defined(lz4)
#include <lz4.h>
#include <lz4hc.h>
#endif

#include <stdint.h>
#include <string>
#include <string.h>
#include "rocksdb/options.h"
#include "port/atomic_pointer.h"

#ifndef platform_is_little_endian
#define platform_is_little_endian (__byte_order == __little_endian)
#endif

#if defined(os_macosx) || defined(os_solaris) || defined(os_freebsd) ||\
    defined(os_netbsd) || defined(os_openbsd) || defined(os_dragonflybsd) ||\
    defined(os_android)
// use fread/fwrite/fflush on platforms without _unlocked variants
#define fread_unlocked fread
#define fwrite_unlocked fwrite
#define fflush_unlocked fflush
#endif

#if defined(os_macosx) || defined(os_freebsd) ||\
    defined(os_openbsd) || defined(os_dragonflybsd)
// use fsync() on platforms without fdatasync()
#define fdatasync fsync
#endif

#if defined(os_android) && __android_api__ < 9
// fdatasync() was only introduced in api level 9 on android. use fsync()
// when targetting older platforms.
#define fdatasync fsync
#endif

namespace rocksdb {
namespace port {

static const bool klittleendian = platform_is_little_endian;
#undef platform_is_little_endian

class condvar;

class mutex {
 public:
  /* implicit */ mutex(bool adaptive = false);
  ~mutex();

  void lock();
  void unlock();
  // this will assert if the mutex is not locked
  // it does not verify that mutex is held by a calling thread
  void assertheld();

 private:
  friend class condvar;
  pthread_mutex_t mu_;
#ifndef ndebug
  bool locked_;
#endif

  // no copying
  mutex(const mutex&);
  void operator=(const mutex&);
};

class rwmutex {
 public:
  rwmutex();
  ~rwmutex();

  void readlock();
  void writelock();
  void readunlock();
  void writeunlock();
  void assertheld() { }

 private:
  pthread_rwlock_t mu_; // the underlying platform mutex

  // no copying allowed
  rwmutex(const rwmutex&);
  void operator=(const rwmutex&);
};

class condvar {
 public:
  explicit condvar(mutex* mu);
  ~condvar();
  void wait();
  // timed condition wait.  returns true if timeout occurred.
  bool timedwait(uint64_t abs_time_us);
  void signal();
  void signalall();
 private:
  pthread_cond_t cv_;
  mutex* mu_;
};

typedef pthread_once_t oncetype;
#define leveldb_once_init pthread_once_init
extern void initonce(oncetype* once, void (*initializer)());

inline bool snappy_compress(const compressionoptions& opts, const char* input,
                            size_t length, ::std::string* output) {
#ifdef snappy
  output->resize(snappy::maxcompressedlength(length));
  size_t outlen;
  snappy::rawcompress(input, length, &(*output)[0], &outlen);
  output->resize(outlen);
  return true;
#endif

  return false;
}

inline bool snappy_getuncompressedlength(const char* input, size_t length,
                                         size_t* result) {
#ifdef snappy
  return snappy::getuncompressedlength(input, length, result);
#else
  return false;
#endif
}

inline bool snappy_uncompress(const char* input, size_t length,
                              char* output) {
#ifdef snappy
  return snappy::rawuncompress(input, length, output);
#else
  return false;
#endif
}

inline bool zlib_compress(const compressionoptions& opts, const char* input,
                          size_t length, ::std::string* output) {
#ifdef zlib
  // the memlevel parameter specifies how much memory should be allocated for
  // the internal compression state.
  // memlevel=1 uses minimum memory but is slow and reduces compression ratio.
  // memlevel=9 uses maximum memory for optimal speed.
  // the default value is 8. see zconf.h for more details.
  static const int memlevel = 8;
  z_stream _stream;
  memset(&_stream, 0, sizeof(z_stream));
  int st = deflateinit2(&_stream, opts.level, z_deflated, opts.window_bits,
                        memlevel, opts.strategy);
  if (st != z_ok) {
    return false;
  }

  // resize output to be the plain data length.
  // this may not be big enough if the compression actually expands data.
  output->resize(length);

  // compress the input, and put compressed data in output.
  _stream.next_in = (bytef *)input;
  _stream.avail_in = length;

  // initialize the output size.
  _stream.avail_out = length;
  _stream.next_out = (bytef *)&(*output)[0];

  int old_sz =0, new_sz =0, new_sz_delta =0;
  bool done = false;
  while (!done) {
    int st = deflate(&_stream, z_finish);
    switch (st) {
      case z_stream_end:
        done = true;
        break;
      case z_ok:
        // no output space. increase the output space by 20%.
        // (should we fail the compression since it expands the size?)
        old_sz = output->size();
        new_sz_delta = (int)(output->size() * 0.2);
        new_sz = output->size() + (new_sz_delta < 10 ? 10 : new_sz_delta);
        output->resize(new_sz);
        // set more output.
        _stream.next_out = (bytef *)&(*output)[old_sz];
        _stream.avail_out = new_sz - old_sz;
        break;
      case z_buf_error:
      default:
        deflateend(&_stream);
        return false;
    }
  }

  output->resize(output->size() - _stream.avail_out);
  deflateend(&_stream);
  return true;
#endif
  return false;
}

inline char* zlib_uncompress(const char* input_data, size_t input_length,
    int* decompress_size, int windowbits = -14) {
#ifdef zlib
  z_stream _stream;
  memset(&_stream, 0, sizeof(z_stream));

  // for raw inflate, the windowbits should be -8..-15.
  // if windowbits is bigger than zero, it will use either zlib
  // header or gzip header. adding 32 to it will do automatic detection.
  int st = inflateinit2(&_stream,
      windowbits > 0 ? windowbits + 32 : windowbits);
  if (st != z_ok) {
    return nullptr;
  }

  _stream.next_in = (bytef *)input_data;
  _stream.avail_in = input_length;

  // assume the decompressed data size will 5x of compressed size.
  int output_len = input_length * 5;
  char* output = new char[output_len];
  int old_sz = output_len;

  _stream.next_out = (bytef *)output;
  _stream.avail_out = output_len;

  char* tmp = nullptr;
  int output_len_delta;
  bool done = false;

  //while(_stream.next_in != nullptr && _stream.avail_in != 0) {
  while (!done) {
    int st = inflate(&_stream, z_sync_flush);
    switch (st) {
      case z_stream_end:
        done = true;
        break;
      case z_ok:
        // no output space. increase the output space by 20%.
        old_sz = output_len;
        output_len_delta = (int)(output_len * 0.2);
        output_len += output_len_delta < 10 ? 10 : output_len_delta;
        tmp = new char[output_len];
        memcpy(tmp, output, old_sz);
        delete[] output;
        output = tmp;

        // set more output.
        _stream.next_out = (bytef *)(output + old_sz);
        _stream.avail_out = output_len - old_sz;
        break;
      case z_buf_error:
      default:
        delete[] output;
        inflateend(&_stream);
        return nullptr;
    }
  }

  *decompress_size = output_len - _stream.avail_out;
  inflateend(&_stream);
  return output;
#endif

  return nullptr;
}

inline bool bzip2_compress(const compressionoptions& opts, const char* input,
                           size_t length, ::std::string* output) {
#ifdef bzip2
  bz_stream _stream;
  memset(&_stream, 0, sizeof(bz_stream));

  // block size 1 is 100k.
  // 0 is for silent.
  // 30 is the default workfactor
  int st = bz2_bzcompressinit(&_stream, 1, 0, 30);
  if (st != bz_ok) {
    return false;
  }

  // resize output to be the plain data length.
  // this may not be big enough if the compression actually expands data.
  output->resize(length);

  // compress the input, and put compressed data in output.
  _stream.next_in = (char *)input;
  _stream.avail_in = length;

  // initialize the output size.
  _stream.next_out = (char *)&(*output)[0];
  _stream.avail_out = length;

  int old_sz =0, new_sz =0;
  while(_stream.next_in != nullptr && _stream.avail_in != 0) {
    int st = bz2_bzcompress(&_stream, bz_finish);
    switch (st) {
      case bz_stream_end:
        break;
      case bz_finish_ok:
        // no output space. increase the output space by 20%.
        // (should we fail the compression since it expands the size?)
        old_sz = output->size();
        new_sz = (int)(output->size() * 1.2);
        output->resize(new_sz);
        // set more output.
        _stream.next_out = (char *)&(*output)[old_sz];
        _stream.avail_out = new_sz - old_sz;
        break;
      case bz_sequence_error:
      default:
        bz2_bzcompressend(&_stream);
        return false;
    }
  }

  output->resize(output->size() - _stream.avail_out);
  bz2_bzcompressend(&_stream);
  return true;
#endif
  return false;
}

inline char* bzip2_uncompress(const char* input_data, size_t input_length,
                              int* decompress_size) {
#ifdef bzip2
  bz_stream _stream;
  memset(&_stream, 0, sizeof(bz_stream));

  int st = bz2_bzdecompressinit(&_stream, 0, 0);
  if (st != bz_ok) {
    return nullptr;
  }

  _stream.next_in = (char *)input_data;
  _stream.avail_in = input_length;

  // assume the decompressed data size will be 5x of compressed size.
  int output_len = input_length * 5;
  char* output = new char[output_len];
  int old_sz = output_len;

  _stream.next_out = (char *)output;
  _stream.avail_out = output_len;

  char* tmp = nullptr;

  while(_stream.next_in != nullptr && _stream.avail_in != 0) {
    int st = bz2_bzdecompress(&_stream);
    switch (st) {
      case bz_stream_end:
        break;
      case bz_ok:
        // no output space. increase the output space by 20%.
        old_sz = output_len;
        output_len = (int)(output_len * 1.2);
        tmp = new char[output_len];
        memcpy(tmp, output, old_sz);
        delete[] output;
        output = tmp;

        // set more output.
        _stream.next_out = (char *)(output + old_sz);
        _stream.avail_out = output_len - old_sz;
        break;
      default:
        delete[] output;
        bz2_bzdecompressend(&_stream);
        return nullptr;
    }
  }

  *decompress_size = output_len - _stream.avail_out;
  bz2_bzdecompressend(&_stream);
  return output;
#endif
  return nullptr;
}

inline bool lz4_compress(const compressionoptions &opts, const char *input,
                         size_t length, ::std::string* output) {
#ifdef lz4
  int compressbound = lz4_compressbound(length);
  output->resize(8 + compressbound);
  char *p = const_cast<char *>(output->c_str());
  memcpy(p, &length, sizeof(length));
  size_t outlen;
  outlen = lz4_compress_limitedoutput(input, p + 8, length, compressbound);
  if (outlen == 0) {
    return false;
  }
  output->resize(8 + outlen);
  return true;
#endif
  return false;
}

inline char* lz4_uncompress(const char* input_data, size_t input_length,
                            int* decompress_size) {
#ifdef lz4
  if (input_length < 8) {
    return nullptr;
  }
  int output_len;
  memcpy(&output_len, input_data, sizeof(output_len));
  char *output = new char[output_len];
  *decompress_size = lz4_decompress_safe_partial(
      input_data + 8, output, input_length - 8, output_len, output_len);
  if (*decompress_size < 0) {
    delete[] output;
    return nullptr;
  }
  return output;
#endif
  return nullptr;
}

inline bool lz4hc_compress(const compressionoptions &opts, const char* input,
                           size_t length, ::std::string* output) {
#ifdef lz4
  int compressbound = lz4_compressbound(length);
  output->resize(8 + compressbound);
  char *p = const_cast<char *>(output->c_str());
  memcpy(p, &length, sizeof(length));
  size_t outlen;
#ifdef lz4_version_major  // they only started defining this since r113
  outlen = lz4_compresshc2_limitedoutput(input, p + 8, length, compressbound,
                                         opts.level);
#else
  outlen = lz4_compresshc_limitedoutput(input, p + 8, length, compressbound);
#endif
  if (outlen == 0) {
    return false;
  }
  output->resize(8 + outlen);
  return true;
#endif
  return false;
}

#define cache_line_size 64u

#define prefetch(addr, rw, locality) __builtin_prefetch(addr, rw, locality)

} // namespace port
} // namespace rocksdb

#endif  // storage_leveldb_port_port_posix_h_
