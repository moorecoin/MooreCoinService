// protocol buffers - google's data interchange format
// copyright 2012 google inc.  all rights reserved.
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

// this file is an internal atomic implementation, use atomicops.h instead.

#ifndef google_protobuf_atomicops_internals_macosx_h_
#define google_protobuf_atomicops_internals_macosx_h_

#include <libkern/osatomic.h>

namespace google {
namespace protobuf {
namespace internal {

inline atomic32 nobarrier_compareandswap(volatile atomic32* ptr,
                                         atomic32 old_value,
                                         atomic32 new_value) {
  atomic32 prev_value;
  do {
    if (osatomiccompareandswap32(old_value, new_value,
                                 const_cast<atomic32*>(ptr))) {
      return old_value;
    }
    prev_value = *ptr;
  } while (prev_value == old_value);
  return prev_value;
}

inline atomic32 nobarrier_atomicexchange(volatile atomic32* ptr,
                                         atomic32 new_value) {
  atomic32 old_value;
  do {
    old_value = *ptr;
  } while (!osatomiccompareandswap32(old_value, new_value,
                                     const_cast<atomic32*>(ptr)));
  return old_value;
}

inline atomic32 nobarrier_atomicincrement(volatile atomic32* ptr,
                                          atomic32 increment) {
  return osatomicadd32(increment, const_cast<atomic32*>(ptr));
}

inline atomic32 barrier_atomicincrement(volatile atomic32* ptr,
                                          atomic32 increment) {
  return osatomicadd32barrier(increment, const_cast<atomic32*>(ptr));
}

inline void memorybarrier() {
  osmemorybarrier();
}

inline atomic32 acquire_compareandswap(volatile atomic32* ptr,
                                       atomic32 old_value,
                                       atomic32 new_value) {
  atomic32 prev_value;
  do {
    if (osatomiccompareandswap32barrier(old_value, new_value,
                                        const_cast<atomic32*>(ptr))) {
      return old_value;
    }
    prev_value = *ptr;
  } while (prev_value == old_value);
  return prev_value;
}

inline atomic32 release_compareandswap(volatile atomic32* ptr,
                                       atomic32 old_value,
                                       atomic32 new_value) {
  return acquire_compareandswap(ptr, old_value, new_value);
}

inline void nobarrier_store(volatile atomic32* ptr, atomic32 value) {
  *ptr = value;
}

inline void acquire_store(volatile atomic32* ptr, atomic32 value) {
  *ptr = value;
  memorybarrier();
}

inline void release_store(volatile atomic32* ptr, atomic32 value) {
  memorybarrier();
  *ptr = value;
}

inline atomic32 nobarrier_load(volatile const atomic32* ptr) {
  return *ptr;
}

inline atomic32 acquire_load(volatile const atomic32* ptr) {
  atomic32 value = *ptr;
  memorybarrier();
  return value;
}

inline atomic32 release_load(volatile const atomic32* ptr) {
  memorybarrier();
  return *ptr;
}

#ifdef __lp64__

// 64-bit implementation on 64-bit platform

inline atomic64 nobarrier_compareandswap(volatile atomic64* ptr,
                                         atomic64 old_value,
                                         atomic64 new_value) {
  atomic64 prev_value;
  do {
    if (osatomiccompareandswap64(old_value, new_value,
                                 reinterpret_cast<volatile int64_t*>(ptr))) {
      return old_value;
    }
    prev_value = *ptr;
  } while (prev_value == old_value);
  return prev_value;
}

inline atomic64 nobarrier_atomicexchange(volatile atomic64* ptr,
                                         atomic64 new_value) {
  atomic64 old_value;
  do {
    old_value = *ptr;
  } while (!osatomiccompareandswap64(old_value, new_value,
                                     reinterpret_cast<volatile int64_t*>(ptr)));
  return old_value;
}

inline atomic64 nobarrier_atomicincrement(volatile atomic64* ptr,
                                          atomic64 increment) {
  return osatomicadd64(increment, reinterpret_cast<volatile int64_t*>(ptr));
}

inline atomic64 barrier_atomicincrement(volatile atomic64* ptr,
                                        atomic64 increment) {
  return osatomicadd64barrier(increment,
                              reinterpret_cast<volatile int64_t*>(ptr));
}

inline atomic64 acquire_compareandswap(volatile atomic64* ptr,
                                       atomic64 old_value,
                                       atomic64 new_value) {
  atomic64 prev_value;
  do {
    if (osatomiccompareandswap64barrier(
        old_value, new_value, reinterpret_cast<volatile int64_t*>(ptr))) {
      return old_value;
    }
    prev_value = *ptr;
  } while (prev_value == old_value);
  return prev_value;
}

inline atomic64 release_compareandswap(volatile atomic64* ptr,
                                       atomic64 old_value,
                                       atomic64 new_value) {
  // the lib kern interface does not distinguish between
  // acquire and release memory barriers; they are equivalent.
  return acquire_compareandswap(ptr, old_value, new_value);
}

inline void nobarrier_store(volatile atomic64* ptr, atomic64 value) {
  *ptr = value;
}

inline void acquire_store(volatile atomic64* ptr, atomic64 value) {
  *ptr = value;
  memorybarrier();
}

inline void release_store(volatile atomic64* ptr, atomic64 value) {
  memorybarrier();
  *ptr = value;
}

inline atomic64 nobarrier_load(volatile const atomic64* ptr) {
  return *ptr;
}

inline atomic64 acquire_load(volatile const atomic64* ptr) {
  atomic64 value = *ptr;
  memorybarrier();
  return value;
}

inline atomic64 release_load(volatile const atomic64* ptr) {
  memorybarrier();
  return *ptr;
}

#endif  // defined(__lp64__)

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_atomicops_internals_macosx_h_
