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

#ifndef google_protobuf_atomicops_internals_arm_qnx_h_
#define google_protobuf_atomicops_internals_arm_qnx_h_

// for _smp_cmpxchg()
#include <pthread.h>

namespace google {
namespace protobuf {
namespace internal {

inline atomic32 qnxcmpxchg(atomic32 old_value,
                           atomic32 new_value,
                           volatile atomic32* ptr) {
  return static_cast<atomic32>(
      _smp_cmpxchg((volatile unsigned *)ptr,
                   (unsigned)old_value,
                   (unsigned)new_value));
}


inline atomic32 nobarrier_compareandswap(volatile atomic32* ptr,
                                         atomic32 old_value,
                                         atomic32 new_value) {
  atomic32 prev_value = *ptr;
  do {
    if (!qnxcmpxchg(old_value, new_value,
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
  } while (qnxcmpxchg(old_value, new_value,
                      const_cast<atomic32*>(ptr)));
  return old_value;
}

inline atomic32 nobarrier_atomicincrement(volatile atomic32* ptr,
                                          atomic32 increment) {
  return barrier_atomicincrement(ptr, increment);
}

inline atomic32 barrier_atomicincrement(volatile atomic32* ptr,
                                        atomic32 increment) {
  for (;;) {
    // atomic exchange the old value with an incremented one.
    atomic32 old_value = *ptr;
    atomic32 new_value = old_value + increment;
    if (qnxcmpxchg(old_value, new_value,
                   const_cast<atomic32*>(ptr)) == 0) {
      // the exchange took place as expected.
      return new_value;
    }
    // otherwise, *ptr changed mid-loop and we need to retry.
  }
}

inline atomic32 acquire_compareandswap(volatile atomic32* ptr,
                                       atomic32 old_value,
                                       atomic32 new_value) {
  return nobarrier_compareandswap(ptr, old_value, new_value);
}

inline atomic32 release_compareandswap(volatile atomic32* ptr,
                                       atomic32 old_value,
                                       atomic32 new_value) {
  return nobarrier_compareandswap(ptr, old_value, new_value);
}

inline void nobarrier_store(volatile atomic32* ptr, atomic32 value) {
  *ptr = value;
}

inline void memorybarrier() {
  __sync_synchronize();
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

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_atomicops_internals_arm_qnx_h_
