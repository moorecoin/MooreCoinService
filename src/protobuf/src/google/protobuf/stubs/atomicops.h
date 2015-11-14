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

// the routines exported by this module are subtle.  if you use them, even if
// you get the code right, it will depend on careful reasoning about atomicity
// and memory ordering; it will be less readable, and harder to maintain.  if
// you plan to use these routines, you should have a good reason, such as solid
// evidence that performance would otherwise suffer, or there being no
// alternative.  you should assume only properties explicitly guaranteed by the
// specifications in this file.  you are almost certainly _not_ writing code
// just for the x86; if you assume x86 semantics, x86 hardware bugs and
// implementations on other archtectures will cause your code to break.  if you
// do not know what you are doing, avoid these routines, and use a mutex.
//
// it is incorrect to make direct assignments to/from an atomic variable.
// you should use one of the load or store routines.  the nobarrier
// versions are provided when no barriers are needed:
//   nobarrier_store()
//   nobarrier_load()
// although there are currently no compiler enforcement, you are encouraged
// to use these.

// this header and the implementations for each platform (located in
// atomicops_internals_*) must be kept in sync with the upstream code (v8).

#ifndef google_protobuf_atomicops_h_
#define google_protobuf_atomicops_h_

// don't include this file for people not concerned about thread safety.
#ifndef google_protobuf_no_thread_safety

#include <google/protobuf/stubs/platform_macros.h>

namespace google {
namespace protobuf {
namespace internal {

typedef int32 atomic32;
#ifdef google_protobuf_arch_64_bit
// we need to be able to go between atomic64 and atomicword implicitly.  this
// means atomic64 and atomicword should be the same type on 64-bit.
#if defined(__ilp32__) || defined(google_protobuf_os_nacl)
// nacl's intptr_t is not actually 64-bits on 64-bit!
// http://code.google.com/p/nativeclient/issues/detail?id=1162
typedef int64 atomic64;
#else
typedef intptr_t atomic64;
#endif
#endif

// use atomicword for a machine-sized pointer.  it will use the atomic32 or
// atomic64 routines below, depending on your architecture.
typedef intptr_t atomicword;

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
atomic32 nobarrier_compareandswap(volatile atomic32* ptr,
                                  atomic32 old_value,
                                  atomic32 new_value);

// atomically store new_value into *ptr, returning the previous value held in
// *ptr.  this routine implies no memory barriers.
atomic32 nobarrier_atomicexchange(volatile atomic32* ptr, atomic32 new_value);

// atomically increment *ptr by "increment".  returns the new value of
// *ptr with the increment applied.  this routine implies no memory barriers.
atomic32 nobarrier_atomicincrement(volatile atomic32* ptr, atomic32 increment);

atomic32 barrier_atomicincrement(volatile atomic32* ptr,
                                 atomic32 increment);

// these following lower-level operations are typically useful only to people
// implementing higher-level synchronization operations like spinlocks,
// mutexes, and condition-variables.  they combine compareandswap(), a load, or
// a store with appropriate memory-ordering instructions.  "acquire" operations
// ensure that no later memory access can be reordered ahead of the operation.
// "release" operations ensure that no previous memory access can be reordered
// after the operation.  "barrier" operations have both "acquire" and "release"
// semantics.   a memorybarrier() has "barrier" semantics, but does no memory
// access.
atomic32 acquire_compareandswap(volatile atomic32* ptr,
                                atomic32 old_value,
                                atomic32 new_value);
atomic32 release_compareandswap(volatile atomic32* ptr,
                                atomic32 old_value,
                                atomic32 new_value);

void memorybarrier();
void nobarrier_store(volatile atomic32* ptr, atomic32 value);
void acquire_store(volatile atomic32* ptr, atomic32 value);
void release_store(volatile atomic32* ptr, atomic32 value);

atomic32 nobarrier_load(volatile const atomic32* ptr);
atomic32 acquire_load(volatile const atomic32* ptr);
atomic32 release_load(volatile const atomic32* ptr);

// 64-bit atomic operations (only available on 64-bit processors).
#ifdef google_protobuf_arch_64_bit
atomic64 nobarrier_compareandswap(volatile atomic64* ptr,
                                  atomic64 old_value,
                                  atomic64 new_value);
atomic64 nobarrier_atomicexchange(volatile atomic64* ptr, atomic64 new_value);
atomic64 nobarrier_atomicincrement(volatile atomic64* ptr, atomic64 increment);
atomic64 barrier_atomicincrement(volatile atomic64* ptr, atomic64 increment);

atomic64 acquire_compareandswap(volatile atomic64* ptr,
                                atomic64 old_value,
                                atomic64 new_value);
atomic64 release_compareandswap(volatile atomic64* ptr,
                                atomic64 old_value,
                                atomic64 new_value);
void nobarrier_store(volatile atomic64* ptr, atomic64 value);
void acquire_store(volatile atomic64* ptr, atomic64 value);
void release_store(volatile atomic64* ptr, atomic64 value);
atomic64 nobarrier_load(volatile const atomic64* ptr);
atomic64 acquire_load(volatile const atomic64* ptr);
atomic64 release_load(volatile const atomic64* ptr);
#endif  // google_protobuf_arch_64_bit

}  // namespace internal
}  // namespace protobuf
}  // namespace google

// include our platform specific implementation.
#define google_protobuf_atomicops_error \
#error "atomic operations are not supported on your platform"

// msvc.
#if defined(_msc_ver)
#if defined(google_protobuf_arch_ia32) || defined(google_protobuf_arch_x64)
#include <google/protobuf/stubs/atomicops_internals_x86_msvc.h>
#else
google_protobuf_atomicops_error
#endif

// apple.
#elif defined(google_protobuf_os_apple)
#include <google/protobuf/stubs/atomicops_internals_macosx.h>

// gcc.
#elif defined(__gnuc__)
#if defined(google_protobuf_arch_ia32) || defined(google_protobuf_arch_x64)
#include <google/protobuf/stubs/atomicops_internals_x86_gcc.h>
#elif defined(google_protobuf_arch_arm)
#include <google/protobuf/stubs/atomicops_internals_arm_gcc.h>
#elif defined(google_protobuf_arch_arm_qnx)
#include <google/protobuf/stubs/atomicops_internals_arm_qnx.h>
#elif defined(google_protobuf_arch_mips)
#include <google/protobuf/stubs/atomicops_internals_mips_gcc.h>
#elif defined(__pnacl__)
#include <google/protobuf/stubs/atomicops_internals_pnacl.h>
#else
google_protobuf_atomicops_error
#endif

// unknown.
#else
google_protobuf_atomicops_error
#endif

// on some platforms we need additional declarations to make atomicword
// compatible with our other atomic* types.
#if defined(google_protobuf_os_apple)
#include <google/protobuf/stubs/atomicops_internals_atomicword_compat.h>
#endif

#undef google_protobuf_atomicops_error

#endif  // google_protobuf_no_thread_safety

#endif  // google_protobuf_atomicops_h_
