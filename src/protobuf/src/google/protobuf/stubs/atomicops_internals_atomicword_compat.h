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

#ifndef google_protobuf_atomicops_internals_atomicword_compat_h_
#define google_protobuf_atomicops_internals_atomicword_compat_h_

// atomicword is a synonym for intptr_t, and atomic32 is a synonym for int32,
// which in turn means int. on some lp32 platforms, intptr_t is an int, but
// on others, it's a long. when atomicword and atomic32 are based on different
// fundamental types, their pointers are incompatible.
//
// this file defines function overloads to allow both atomicword and atomic32
// data to be used with this interface.
//
// on lp64 platforms, atomicword and atomic64 are both always long,
// so this problem doesn't occur.

#if !defined(google_protobuf_arch_64_bit)

namespace google {
namespace protobuf {
namespace internal {

inline atomicword nobarrier_compareandswap(volatile atomicword* ptr,
                                           atomicword old_value,
                                           atomicword new_value) {
  return nobarrier_compareandswap(
      reinterpret_cast<volatile atomic32*>(ptr), old_value, new_value);
}

inline atomicword nobarrier_atomicexchange(volatile atomicword* ptr,
                                           atomicword new_value) {
  return nobarrier_atomicexchange(
      reinterpret_cast<volatile atomic32*>(ptr), new_value);
}

inline atomicword nobarrier_atomicincrement(volatile atomicword* ptr,
                                            atomicword increment) {
  return nobarrier_atomicincrement(
      reinterpret_cast<volatile atomic32*>(ptr), increment);
}

inline atomicword barrier_atomicincrement(volatile atomicword* ptr,
                                          atomicword increment) {
  return barrier_atomicincrement(
      reinterpret_cast<volatile atomic32*>(ptr), increment);
}

inline atomicword acquire_compareandswap(volatile atomicword* ptr,
                                         atomicword old_value,
                                         atomicword new_value) {
  return acquire_compareandswap(
      reinterpret_cast<volatile atomic32*>(ptr), old_value, new_value);
}

inline atomicword release_compareandswap(volatile atomicword* ptr,
                                         atomicword old_value,
                                         atomicword new_value) {
  return release_compareandswap(
      reinterpret_cast<volatile atomic32*>(ptr), old_value, new_value);
}

inline void nobarrier_store(volatile atomicword *ptr, atomicword value) {
  nobarrier_store(reinterpret_cast<volatile atomic32*>(ptr), value);
}

inline void acquire_store(volatile atomicword* ptr, atomicword value) {
  return acquire_store(reinterpret_cast<volatile atomic32*>(ptr), value);
}

inline void release_store(volatile atomicword* ptr, atomicword value) {
  return release_store(reinterpret_cast<volatile atomic32*>(ptr), value);
}

inline atomicword nobarrier_load(volatile const atomicword *ptr) {
  return nobarrier_load(reinterpret_cast<volatile const atomic32*>(ptr));
}

inline atomicword acquire_load(volatile const atomicword* ptr) {
  return acquire_load(reinterpret_cast<volatile const atomic32*>(ptr));
}

inline atomicword release_load(volatile const atomicword* ptr) {
  return release_load(reinterpret_cast<volatile const atomic32*>(ptr));
}

}   // namespace internal
}   // namespace protobuf
}   // namespace google

#endif  // !defined(google_protobuf_arch_64_bit)

#endif  // google_protobuf_atomicops_internals_atomicword_compat_h_
