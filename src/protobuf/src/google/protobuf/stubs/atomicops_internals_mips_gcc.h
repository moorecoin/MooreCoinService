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

#ifndef google_protobuf_atomicops_internals_mips_gcc_h_
#define google_protobuf_atomicops_internals_mips_gcc_h_

#define atomicops_compiler_barrier() __asm__ __volatile__("" : : : "memory")

namespace google {
namespace protobuf {
namespace internal {

// atomically execute:
//      result = *ptr;
//      if (*ptr == old_value)
//        *ptr = new_value;
//      return result;
//
// i.e., replace "*ptr" with "new_value" if "*ptr" used to be "old_value".
// always return the old value of "*ptr"
//
// this routine implies no memory barriers.
inline atomic32 nobarrier_compareandswap(volatile atomic32* ptr,
                                         atomic32 old_value,
                                         atomic32 new_value) {
  atomic32 prev, tmp;
  __asm__ __volatile__(".set push\n"
                       ".set noreorder\n"
                       "1:\n"
                       "ll %0, %5\n"  // prev = *ptr
                       "bne %0, %3, 2f\n"  // if (prev != old_value) goto 2
                       "move %2, %4\n"  // tmp = new_value
                       "sc %2, %1\n"  // *ptr = tmp (with atomic check)
                       "beqz %2, 1b\n"  // start again on atomic error
                       "nop\n"  // delay slot nop
                       "2:\n"
                       ".set pop\n"
                       : "=&r" (prev), "=m" (*ptr), "=&r" (tmp)
                       : "ir" (old_value), "r" (new_value), "m" (*ptr)
                       : "memory");
  return prev;
}

// atomically store new_value into *ptr, returning the previous value held in
// *ptr.  this routine implies no memory barriers.
inline atomic32 nobarrier_atomicexchange(volatile atomic32* ptr,
                                         atomic32 new_value) {
  atomic32 temp, old;
  __asm__ __volatile__(".set push\n"
                       ".set noreorder\n"
                       "1:\n"
                       "ll %1, %2\n"  // old = *ptr
                       "move %0, %3\n"  // temp = new_value
                       "sc %0, %2\n"  // *ptr = temp (with atomic check)
                       "beqz %0, 1b\n"  // start again on atomic error
                       "nop\n"  // delay slot nop
                       ".set pop\n"
                       : "=&r" (temp), "=&r" (old), "=m" (*ptr)
                       : "r" (new_value), "m" (*ptr)
                       : "memory");

  return old;
}

// atomically increment *ptr by "increment".  returns the new value of
// *ptr with the increment applied.  this routine implies no memory barriers.
inline atomic32 nobarrier_atomicincrement(volatile atomic32* ptr,
                                          atomic32 increment) {
  atomic32 temp, temp2;

  __asm__ __volatile__(".set push\n"
                       ".set noreorder\n"
                       "1:\n"
                       "ll %0, %2\n"  // temp = *ptr
                       "addu %1, %0, %3\n"  // temp2 = temp + increment
                       "sc %1, %2\n"  // *ptr = temp2 (with atomic check)
                       "beqz %1, 1b\n"  // start again on atomic error
                       "addu %1, %0, %3\n"  // temp2 = temp + increment
                       ".set pop\n"
                       : "=&r" (temp), "=&r" (temp2), "=m" (*ptr)
                       : "ir" (increment), "m" (*ptr)
                       : "memory");
  // temp2 now holds the final value.
  return temp2;
}

inline atomic32 barrier_atomicincrement(volatile atomic32* ptr,
                                        atomic32 increment) {
  atomicops_compiler_barrier();
  atomic32 res = nobarrier_atomicincrement(ptr, increment);
  atomicops_compiler_barrier();
  return res;
}

// "acquire" operations
// ensure that no later memory access can be reordered ahead of the operation.
// "release" operations ensure that no previous memory access can be reordered
// after the operation.  "barrier" operations have both "acquire" and "release"
// semantics.   a memorybarrier() has "barrier" semantics, but does no memory
// access.
inline atomic32 acquire_compareandswap(volatile atomic32* ptr,
                                       atomic32 old_value,
                                       atomic32 new_value) {
  atomicops_compiler_barrier();
  atomic32 res = nobarrier_compareandswap(ptr, old_value, new_value);
  atomicops_compiler_barrier();
  return res;
}

inline atomic32 release_compareandswap(volatile atomic32* ptr,
                                       atomic32 old_value,
                                       atomic32 new_value) {
  atomicops_compiler_barrier();
  atomic32 res = nobarrier_compareandswap(ptr, old_value, new_value);
  atomicops_compiler_barrier();
  return res;
}

inline void nobarrier_store(volatile atomic32* ptr, atomic32 value) {
  *ptr = value;
}

inline void memorybarrier() {
  __asm__ __volatile__("sync" : : : "memory");
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

#undef atomicops_compiler_barrier

#endif  // google_protobuf_atomicops_internals_mips_gcc_h_
