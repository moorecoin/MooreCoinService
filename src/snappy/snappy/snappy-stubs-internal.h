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
// various stubs for the open-source version of snappy.

#ifndef util_snappy_opensource_snappy_stubs_internal_h_
#define util_snappy_opensource_snappy_stubs_internal_h_

#ifdef have_config_h
#include "config.h"
#endif

#include <string>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef have_sys_mman_h
#include <sys/mman.h>
#endif

#include "snappy-stubs-public.h"

#if defined(__x86_64__)

// enable 64-bit optimized versions of some routines.
#define arch_k8 1

#endif

// needed by os x, among others.
#ifndef map_anonymous
#define map_anonymous map_anon
#endif

// pull in std::min, std::ostream, and the likes. this is safe because this
// header file is never used from any public header files.
using namespace std;

// the size of an array, if known at compile-time.
// will give unexpected results if used on a pointer.
// we undefine it first, since some compilers already have a definition.
#ifdef arraysize
#undef arraysize
#endif
#define arraysize(a) (sizeof(a) / sizeof(*(a)))

// static prediction hints.
#ifdef have_builtin_expect
#define predict_false(x) (__builtin_expect(x, 0))
#define predict_true(x) (__builtin_expect(!!(x), 1))
#else
#define predict_false(x) x
#define predict_true(x) x
#endif

// this is only used for recomputing the tag byte table used during
// decompression; for simplicity we just remove it from the open-source
// version (anyone who wants to regenerate it can just do the call
// themselves within main()).
#define define_bool(flag_name, default_value, description) \
  bool flags_ ## flag_name = default_value
#define declare_bool(flag_name) \
  extern bool flags_ ## flag_name

namespace snappy {

static const uint32 kuint32max = static_cast<uint32>(0xffffffff);
static const int64 kint64max = static_cast<int64>(0x7fffffffffffffffll);

// potentially unaligned loads and stores.

// x86 and powerpc can simply do these loads and stores native.

#if defined(__i386__) || defined(__x86_64__) || defined(__powerpc__)

#define unaligned_load16(_p) (*reinterpret_cast<const uint16 *>(_p))
#define unaligned_load32(_p) (*reinterpret_cast<const uint32 *>(_p))
#define unaligned_load64(_p) (*reinterpret_cast<const uint64 *>(_p))

#define unaligned_store16(_p, _val) (*reinterpret_cast<uint16 *>(_p) = (_val))
#define unaligned_store32(_p, _val) (*reinterpret_cast<uint32 *>(_p) = (_val))
#define unaligned_store64(_p, _val) (*reinterpret_cast<uint64 *>(_p) = (_val))

// armv7 and newer support native unaligned accesses, but only of 16-bit
// and 32-bit values (not 64-bit); older versions either raise a fatal signal,
// do an unaligned read and rotate the words around a bit, or do the reads very
// slowly (trip through kernel mode). there's no simple #define that says just
// 鈥淎rmv7 or higher鈥? so we have to filter away all armv5 and armv6
// sub-architectures.
//
// this is a mess, but there's not much we can do about it.

#elif defined(__arm__) && \
      !defined(__arm_arch_4__) && \
      !defined(__arm_arch_4t__) && \
      !defined(__arm_arch_5__) && \
      !defined(__arm_arch_5t__) && \
      !defined(__arm_arch_5te__) && \
      !defined(__arm_arch_5tej__) && \
      !defined(__arm_arch_6__) && \
      !defined(__arm_arch_6j__) && \
      !defined(__arm_arch_6k__) && \
      !defined(__arm_arch_6z__) && \
      !defined(__arm_arch_6zk__) && \
      !defined(__arm_arch_6t2__)

#define unaligned_load16(_p) (*reinterpret_cast<const uint16 *>(_p))
#define unaligned_load32(_p) (*reinterpret_cast<const uint32 *>(_p))

#define unaligned_store16(_p, _val) (*reinterpret_cast<uint16 *>(_p) = (_val))
#define unaligned_store32(_p, _val) (*reinterpret_cast<uint32 *>(_p) = (_val))

// todo(user): neon supports unaligned 64-bit loads and stores.
// see if that would be more efficient on platforms supporting it,
// at least for copies.

inline uint64 unaligned_load64(const void *p) {
  uint64 t;
  memcpy(&t, p, sizeof t);
  return t;
}

inline void unaligned_store64(void *p, uint64 v) {
  memcpy(p, &v, sizeof v);
}

#else

// these functions are provided for architectures that don't support
// unaligned loads and stores.

inline uint16 unaligned_load16(const void *p) {
  uint16 t;
  memcpy(&t, p, sizeof t);
  return t;
}

inline uint32 unaligned_load32(const void *p) {
  uint32 t;
  memcpy(&t, p, sizeof t);
  return t;
}

inline uint64 unaligned_load64(const void *p) {
  uint64 t;
  memcpy(&t, p, sizeof t);
  return t;
}

inline void unaligned_store16(void *p, uint16 v) {
  memcpy(p, &v, sizeof v);
}

inline void unaligned_store32(void *p, uint32 v) {
  memcpy(p, &v, sizeof v);
}

inline void unaligned_store64(void *p, uint64 v) {
  memcpy(p, &v, sizeof v);
}

#endif

// this can be more efficient than unaligned_load64 + unaligned_store64
// on some platforms, in particular arm.
inline void unalignedcopy64(const void *src, void *dst) {
  if (sizeof(void *) == 8) {
    unaligned_store64(dst, unaligned_load64(src));
  } else {
    const char *src_char = reinterpret_cast<const char *>(src);
    char *dst_char = reinterpret_cast<char *>(dst);

    unaligned_store32(dst_char, unaligned_load32(src_char));
    unaligned_store32(dst_char + 4, unaligned_load32(src_char + 4));
  }
}

// the following guarantees declaration of the byte swap functions.
#ifdef words_bigendian

#ifdef have_sys_byteorder_h
#include <sys/byteorder.h>
#endif

#ifdef have_sys_endian_h
#include <sys/endian.h>
#endif

#ifdef _msc_ver
#include <stdlib.h>
#define bswap_16(x) _byteswap_ushort(x)
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)

#elif defined(__apple__)
// mac os x / darwin features
#include <libkern/osbyteorder.h>
#define bswap_16(x) osswapint16(x)
#define bswap_32(x) osswapint32(x)
#define bswap_64(x) osswapint64(x)

#elif defined(have_byteswap_h)
#include <byteswap.h>

#elif defined(bswap32)
// freebsd defines bswap{16,32,64} in <sys/endian.h> (already #included).
#define bswap_16(x) bswap16(x)
#define bswap_32(x) bswap32(x)
#define bswap_64(x) bswap64(x)

#elif defined(bswap_64)
// solaris 10 defines bswap_{16,32,64} in <sys/byteorder.h> (already #included).
#define bswap_16(x) bswap_16(x)
#define bswap_32(x) bswap_32(x)
#define bswap_64(x) bswap_64(x)

#else

inline uint16 bswap_16(uint16 x) {
  return (x << 8) | (x >> 8);
}

inline uint32 bswap_32(uint32 x) {
  x = ((x & 0xff00ff00ul) >> 8) | ((x & 0x00ff00fful) << 8);
  return (x >> 16) | (x << 16);
}

inline uint64 bswap_64(uint64 x) {
  x = ((x & 0xff00ff00ff00ff00ull) >> 8) | ((x & 0x00ff00ff00ff00ffull) << 8);
  x = ((x & 0xffff0000ffff0000ull) >> 16) | ((x & 0x0000ffff0000ffffull) << 16);
  return (x >> 32) | (x << 32);
}

#endif

#endif  // words_bigendian

// convert to little-endian storage, opposite of network format.
// convert x from host to little endian: x = littleendian.fromhost(x);
// convert x from little endian to host: x = littleendian.tohost(x);
//
//  store values into unaligned memory converting to little endian order:
//    littleendian.store16(p, x);
//
//  load unaligned values stored in little endian converting to host order:
//    x = littleendian.load16(p);
class littleendian {
 public:
  // conversion functions.
#ifdef words_bigendian

  static uint16 fromhost16(uint16 x) { return bswap_16(x); }
  static uint16 tohost16(uint16 x) { return bswap_16(x); }

  static uint32 fromhost32(uint32 x) { return bswap_32(x); }
  static uint32 tohost32(uint32 x) { return bswap_32(x); }

  static bool islittleendian() { return false; }

#else  // !defined(words_bigendian)

  static uint16 fromhost16(uint16 x) { return x; }
  static uint16 tohost16(uint16 x) { return x; }

  static uint32 fromhost32(uint32 x) { return x; }
  static uint32 tohost32(uint32 x) { return x; }

  static bool islittleendian() { return true; }

#endif  // !defined(words_bigendian)

  // functions to do unaligned loads and stores in little-endian order.
  static uint16 load16(const void *p) {
    return tohost16(unaligned_load16(p));
  }

  static void store16(void *p, uint16 v) {
    unaligned_store16(p, fromhost16(v));
  }

  static uint32 load32(const void *p) {
    return tohost32(unaligned_load32(p));
  }

  static void store32(void *p, uint32 v) {
    unaligned_store32(p, fromhost32(v));
  }
};

// some bit-manipulation functions.
class bits {
 public:
  // return floor(log2(n)) for positive integer n.  returns -1 iff n == 0.
  static int log2floor(uint32 n);

  // return the first set least / most significant bit, 0-indexed.  returns an
  // undefined value if n == 0.  findlsbsetnonzero() is similar to ffs() except
  // that it's 0-indexed.
  static int findlsbsetnonzero(uint32 n);
  static int findlsbsetnonzero64(uint64 n);

 private:
  disallow_copy_and_assign(bits);
};

#ifdef have_builtin_ctz

inline int bits::log2floor(uint32 n) {
  return n == 0 ? -1 : 31 ^ __builtin_clz(n);
}

inline int bits::findlsbsetnonzero(uint32 n) {
  return __builtin_ctz(n);
}

inline int bits::findlsbsetnonzero64(uint64 n) {
  return __builtin_ctzll(n);
}

#else  // portable versions.

inline int bits::log2floor(uint32 n) {
  if (n == 0)
    return -1;
  int log = 0;
  uint32 value = n;
  for (int i = 4; i >= 0; --i) {
    int shift = (1 << i);
    uint32 x = value >> shift;
    if (x != 0) {
      value = x;
      log += shift;
    }
  }
  assert(value == 1);
  return log;
}

inline int bits::findlsbsetnonzero(uint32 n) {
  int rc = 31;
  for (int i = 4, shift = 1 << 4; i >= 0; --i) {
    const uint32 x = n << shift;
    if (x != 0) {
      n = x;
      rc -= shift;
    }
    shift >>= 1;
  }
  return rc;
}

// findlsbsetnonzero64() is defined in terms of findlsbsetnonzero().
inline int bits::findlsbsetnonzero64(uint64 n) {
  const uint32 bottombits = static_cast<uint32>(n);
  if (bottombits == 0) {
    // bottom bits are zero, so scan in top bits
    return 32 + findlsbsetnonzero(static_cast<uint32>(n >> 32));
  } else {
    return findlsbsetnonzero(bottombits);
  }
}

#endif  // end portable versions.

// variable-length integer encoding.
class varint {
 public:
  // maximum lengths of varint encoding of uint32.
  static const int kmax32 = 5;

  // attempts to parse a varint32 from a prefix of the bytes in [ptr,limit-1].
  // never reads a character at or beyond limit.  if a valid/terminated varint32
  // was found in the range, stores it in *output and returns a pointer just
  // past the last byte of the varint32. else returns null.  on success,
  // "result <= limit".
  static const char* parse32withlimit(const char* ptr, const char* limit,
                                      uint32* output);

  // requires   "ptr" points to a buffer of length sufficient to hold "v".
  // effects    encodes "v" into "ptr" and returns a pointer to the
  //            byte just past the last encoded byte.
  static char* encode32(char* ptr, uint32 v);

  // effects    appends the varint representation of "value" to "*s".
  static void append32(string* s, uint32 value);
};

inline const char* varint::parse32withlimit(const char* p,
                                            const char* l,
                                            uint32* output) {
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(p);
  const unsigned char* limit = reinterpret_cast<const unsigned char*>(l);
  uint32 b, result;
  if (ptr >= limit) return null;
  b = *(ptr++); result = b & 127;          if (b < 128) goto done;
  if (ptr >= limit) return null;
  b = *(ptr++); result |= (b & 127) <<  7; if (b < 128) goto done;
  if (ptr >= limit) return null;
  b = *(ptr++); result |= (b & 127) << 14; if (b < 128) goto done;
  if (ptr >= limit) return null;
  b = *(ptr++); result |= (b & 127) << 21; if (b < 128) goto done;
  if (ptr >= limit) return null;
  b = *(ptr++); result |= (b & 127) << 28; if (b < 16) goto done;
  return null;       // value is too long to be a varint32
 done:
  *output = result;
  return reinterpret_cast<const char*>(ptr);
}

inline char* varint::encode32(char* sptr, uint32 v) {
  // operate on characters as unsigneds
  unsigned char* ptr = reinterpret_cast<unsigned char*>(sptr);
  static const int b = 128;
  if (v < (1<<7)) {
    *(ptr++) = v;
  } else if (v < (1<<14)) {
    *(ptr++) = v | b;
    *(ptr++) = v>>7;
  } else if (v < (1<<21)) {
    *(ptr++) = v | b;
    *(ptr++) = (v>>7) | b;
    *(ptr++) = v>>14;
  } else if (v < (1<<28)) {
    *(ptr++) = v | b;
    *(ptr++) = (v>>7) | b;
    *(ptr++) = (v>>14) | b;
    *(ptr++) = v>>21;
  } else {
    *(ptr++) = v | b;
    *(ptr++) = (v>>7) | b;
    *(ptr++) = (v>>14) | b;
    *(ptr++) = (v>>21) | b;
    *(ptr++) = v>>28;
  }
  return reinterpret_cast<char*>(ptr);
}

// if you know the internal layout of the std::string in use, you can
// replace this function with one that resizes the string without
// filling the new space with zeros (if applicable) --
// it will be non-portable but faster.
inline void stlstringresizeuninitialized(string* s, size_t new_size) {
  s->resize(new_size);
}

// return a mutable char* pointing to a string's internal buffer,
// which may not be null-terminated. writing through this pointer will
// modify the string.
//
// string_as_array(&str)[i] is valid for 0 <= i < str.size() until the
// next call to a string method that invalidates iterators.
//
// as of 2006-04, there is no standard-blessed way of getting a
// mutable reference to a string's internal buffer. however, issue 530
// (http://www.open-std.org/jtc1/sc22/wg21/docs/lwg-defects.html#530)
// proposes this as the method. it will officially be part of the standard
// for c++0x. this should already work on all current implementations.
inline char* string_as_array(string* str) {
  return str->empty() ? null : &*str->begin();
}

}  // namespace snappy

#endif  // util_snappy_opensource_snappy_stubs_internal_h_
