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

#ifndef google_protobuf_atomicops_internals_x86_gcc_h_
#define google_protobuf_atomicops_internals_x86_gcc_h_

namespace google {
namespace protobuf {
namespace internal {

// this struct is not part of the public api of this module; clients may not
// use it.
// features of this x86.  values may not be correct before main() is run,
// but are set conservatively.
struct atomicops_x86cpufeaturestruct {
  bool has_amd_lock_mb_bug;  // processor has amd memory-barrier bug; do lfence
                             // after acquire compare-and-swap.
  bool has_sse2;             // processor has sse2.
};
extern struct atomicops_x86cpufeaturestruct atomicops_internalx86cpufeatures;

#define atomicops_compiler_barrier() __asm__ __volatile__("" : : : "memory")

// 32-bit low-level operations on any platform.

inline atomic32 nobarrier_compareandswap(volatile atomic32* ptr,
                                         atomic32 old_value,
                                         atomic32 new_value) {
  atomic32 prev;
  __asm__ __volatile__("lock; cmpxchgl %1,%2"
                       : "=a" (prev)
                       : "q" (new_value), "m" (*ptr), "0" (old_value)
                       : "memory");
  return prev;
}

inline atomic32 nobarrier_atomicexchange(volatile atomic32* ptr,
                                         atomic32 new_value) {
  __asm__ __volatile__("xchgl %1,%0"  // the lock prefix is implicit for xchg.
                       : "=r" (new_value)
                       : "m" (*ptr), "0" (new_value)
                       : "memory");
  return new_value;  // now it's the previous value.
}

inline atomic32 nobarrier_atomicincrement(volatile atomic32* ptr,
                                          atomic32 increment) {
  atomic32 temp = increment;
  __asm__ __volatile__("lock; xaddl %0,%1"
                       : "+r" (temp), "+m" (*ptr)
                       : : "memory");
  // temp now holds the old value of *ptr
  return temp + increment;
}

inline atomic32 barrier_atomicincrement(volatile atomic32* ptr,
                                        atomic32 increment) {
  atomic32 temp = increment;
  __asm__ __volatile__("lock; xaddl %0,%1"
                       : "+r" (temp), "+m" (*ptr)
                       : : "memory");
  // temp now holds the old value of *ptr
  if (atomicops_internalx86cpufeatures.has_amd_lock_mb_bug) {
    __asm__ __volatile__("lfence" : : : "memory");
  }
  return temp + increment;
}

inline atomic32 acquire_compareandswap(volatile atomic32* ptr,
                                       atomic32 old_value,
                                       atomic32 new_value) {
  atomic32 x = nobarrier_compareandswap(ptr, old_value, new_value);
  if (atomicops_internalx86cpufeatures.has_amd_lock_mb_bug) {
    __asm__ __volatile__("lfence" : : : "memory");
  }
  return x;
}

inline atomic32 release_compareandswap(volatile atomic32* ptr,
                                       atomic32 old_value,
                                       atomic32 new_value) {
  return nobarrier_compareandswap(ptr, old_value, new_value);
}

inline void nobarrier_store(volatile atomic32* ptr, atomic32 value) {
  *ptr = value;
}

#if defined(__x86_64__)

// 64-bit implementations of memory barrier can be simpler, because it
// "mfence" is guaranteed to exist.
inline void memorybarrier() {
  __asm__ __volatile__("mfence" : : : "memory");
}

inline void acquire_store(volatile atomic32* ptr, atomic32 value) {
  *ptr = value;
  memorybarrier();
}

#else

inline void memorybarrier() {
  if (atomicops_internalx86cpufeatures.has_sse2) {
    __asm__ __volatile__("mfence" : : : "memory");
  } else {  // mfence is faster but not present on piii
    atomic32 x = 0;
    nobarrier_atomicexchange(&x, 0);  // acts as a barrier on piii
  }
}

inline void acquire_store(volatile atomic32* ptr, atomic32 value) {
  if (atomicops_internalx86cpufeatures.has_sse2) {
    *ptr = value;
    __asm__ __volatile__("mfence" : : : "memory");
  } else {
    nobarrier_atomicexchange(ptr, value);
                          // acts as a barrier on piii
  }
}
#endif

inline void release_store(volatile atomic32* ptr, atomic32 value) {
  atomicops_compiler_barrier();
  *ptr = value;  // an x86 store acts as a release barrier.
  // see comments in atomic64 version of release_store(), below.
}

inline atomic32 nobarrier_load(volatile const atomic32* ptr) {
  return *ptr;
}

inline atomic32 acquire_load(volatile const atomic32* ptr) {
  atomic32 value = *ptr;  // an x86 load acts as a acquire barrier.
  // see comments in atomic64 version of release_store(), below.
  atomicops_compiler_barrier();
  return value;
}

inline atomic32 release_load(volatile const atomic32* ptr) {
  memorybarrier();
  return *ptr;
}

#if defined(__x86_64__)

// 64-bit low-level operations on 64-bit platform.

inline atomic64 nobarrier_compareandswap(volatile atomic64* ptr,
                                         atomic64 old_value,
                                         atomic64 new_value) {
  atomic64 prev;
  __asm__ __volatile__("lock; cmpxchgq %1,%2"
                       : "=a" (prev)
                       : "q" (new_value), "m" (*ptr), "0" (old_value)
                       : "memory");
  return prev;
}

inline atomic64 nobarrier_atomicexchange(volatile atomic64* ptr,
                                         atomic64 new_value) {
  __asm__ __volatile__("xchgq %1,%0"  // the lock prefix is implicit for xchg.
                       : "=r" (new_value)
                       : "m" (*ptr), "0" (new_value)
                       : "memory");
  return new_value;  // now it's the previous value.
}

inline atomic64 nobarrier_atomicincrement(volatile atomic64* ptr,
                                          atomic64 increment) {
  atomic64 temp = increment;
  __asm__ __volatile__("lock; xaddq %0,%1"
                       : "+r" (temp), "+m" (*ptr)
                       : : "memory");
  // temp now contains the previous value of *ptr
  return temp + increment;
}

inline atomic64 barrier_atomicincrement(volatile atomic64* ptr,
                                        atomic64 increment) {
  atomic64 temp = increment;
  __asm__ __volatile__("lock; xaddq %0,%1"
                       : "+r" (temp), "+m" (*ptr)
                       : : "memory");
  // temp now contains the previous value of *ptr
  if (atomicops_internalx86cpufeatures.has_amd_lock_mb_bug) {
    __asm__ __volatile__("lfence" : : : "memory");
  }
  return temp + increment;
}

inline void nobarrier_store(volatile atomic64* ptr, atomic64 value) {
  *ptr = value;
}

inline void acquire_store(volatile atomic64* ptr, atomic64 value) {
  *ptr = value;
  memorybarrier();
}

inline void release_store(volatile atomic64* ptr, atomic64 value) {
  atomicops_compiler_barrier();

  *ptr = value;  // an x86 store acts as a release barrier
                 // for current amd/intel chips as of jan 2008.
                 // see also acquire_load(), below.

  // when new chips come out, check:
  //  ia-32 intel architecture software developer's manual, volume 3:
  //  system programming guide, chatper 7: multiple-processor management,
  //  section 7.2, memory ordering.
  // last seen at:
  //   http://developer.intel.com/design/pentium4/manuals/index_new.htm
  //
  // x86 stores/loads fail to act as barriers for a few instructions (clflush
  // maskmovdqu maskmovq movntdq movnti movntpd movntps movntq) but these are
  // not generated by the compiler, and are rare.  users of these instructions
  // need to know about cache behaviour in any case since all of these involve
  // either flushing cache lines or non-temporal cache hints.
}

inline atomic64 nobarrier_load(volatile const atomic64* ptr) {
  return *ptr;
}

inline atomic64 acquire_load(volatile const atomic64* ptr) {
  atomic64 value = *ptr;  // an x86 load acts as a acquire barrier,
                          // for current amd/intel chips as of jan 2008.
                          // see also release_store(), above.
  atomicops_compiler_barrier();
  return value;
}

inline atomic64 release_load(volatile const atomic64* ptr) {
  memorybarrier();
  return *ptr;
}

inline atomic64 acquire_compareandswap(volatile atomic64* ptr,
                                       atomic64 old_value,
                                       atomic64 new_value) {
  atomic64 x = nobarrier_compareandswap(ptr, old_value, new_value);
  if (atomicops_internalx86cpufeatures.has_amd_lock_mb_bug) {
    __asm__ __volatile__("lfence" : : : "memory");
  }
  return x;
}

inline atomic64 release_compareandswap(volatile atomic64* ptr,
                                       atomic64 old_value,
                                       atomic64 new_value) {
  return nobarrier_compareandswap(ptr, old_value, new_value);
}

#endif  // defined(__x86_64__)

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#undef atomicops_compiler_barrier

#endif  // google_protobuf_atomicops_internals_x86_gcc_h_
