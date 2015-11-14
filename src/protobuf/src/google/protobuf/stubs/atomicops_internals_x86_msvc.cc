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

// the compilation of extension_set.cc fails when windows.h is included.
// therefore we move the code depending on windows.h to this separate cc file.

// don't compile this file for people not concerned about thread safety.
#ifndef google_protobuf_no_thread_safety

#include <google/protobuf/stubs/atomicops.h>

#ifdef google_protobuf_atomicops_internals_x86_msvc_h_

#include <windows.h>

namespace google {
namespace protobuf {
namespace internal {

inline void memorybarrier() {
  // we use memorybarrier from winnt.h
  ::memorybarrier();
}

atomic32 nobarrier_compareandswap(volatile atomic32* ptr,
                                  atomic32 old_value,
                                  atomic32 new_value) {
  long result = interlockedcompareexchange(
      reinterpret_cast<volatile long*>(ptr),
      static_cast<long>(new_value),
      static_cast<long>(old_value));
  return static_cast<atomic32>(result);
}

atomic32 nobarrier_atomicexchange(volatile atomic32* ptr,
                                  atomic32 new_value) {
  long result = interlockedexchange(
      reinterpret_cast<volatile long*>(ptr),
      static_cast<long>(new_value));
  return static_cast<atomic32>(result);
}

atomic32 barrier_atomicincrement(volatile atomic32* ptr,
                                 atomic32 increment) {
  return interlockedexchangeadd(
      reinterpret_cast<volatile long*>(ptr),
      static_cast<long>(increment)) + increment;
}

#if defined(_win64)

// 64-bit low-level operations on 64-bit platform.

atomic64 nobarrier_compareandswap(volatile atomic64* ptr,
                                  atomic64 old_value,
                                  atomic64 new_value) {
  pvoid result = interlockedcompareexchangepointer(
    reinterpret_cast<volatile pvoid*>(ptr),
    reinterpret_cast<pvoid>(new_value), reinterpret_cast<pvoid>(old_value));
  return reinterpret_cast<atomic64>(result);
}

atomic64 nobarrier_atomicexchange(volatile atomic64* ptr,
                                  atomic64 new_value) {
  pvoid result = interlockedexchangepointer(
    reinterpret_cast<volatile pvoid*>(ptr),
    reinterpret_cast<pvoid>(new_value));
  return reinterpret_cast<atomic64>(result);
}

atomic64 barrier_atomicincrement(volatile atomic64* ptr,
                                 atomic64 increment) {
  return interlockedexchangeadd64(
      reinterpret_cast<volatile longlong*>(ptr),
      static_cast<longlong>(increment)) + increment;
}

#endif  // defined(_win64)

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_atomicops_internals_x86_msvc_h_
#endif  // google_protobuf_no_thread_safety
