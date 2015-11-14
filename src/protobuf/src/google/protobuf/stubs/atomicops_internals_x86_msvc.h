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

#ifndef google_protobuf_atomicops_internals_x86_msvc_h_
#define google_protobuf_atomicops_internals_x86_msvc_h_

namespace google {
namespace protobuf {
namespace internal {

inline atomic32 nobarrier_atomicincrement(volatile atomic32* ptr,
                                          atomic32 increment) {
  return barrier_atomicincrement(ptr, increment);
}

#if !(defined(_msc_ver) && _msc_ver >= 1400)
#error "we require at least vs2005 for memorybarrier"
#endif

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

inline void acquire_store(volatile atomic32* ptr, atomic32 value) {
  nobarrier_atomicexchange(ptr, value);
              // acts as a barrier in this implementation
}

inline void release_store(volatile atomic32* ptr, atomic32 value) {
  *ptr = value;  // works w/o barrier for current intel chips as of june 2005
  // see comments in atomic64 version of release_store() below.
}

inline atomic32 nobarrier_load(volatile const atomic32* ptr) {
  return *ptr;
}

inline atomic32 acquire_load(volatile const atomic32* ptr) {
  atomic32 value = *ptr;
  return value;
}

inline atomic32 release_load(volatile const atomic32* ptr) {
  memorybarrier();
  return *ptr;
}

#if defined(_win64)

// 64-bit low-level operations on 64-bit platform.

inline atomic64 nobarrier_atomicincrement(volatile atomic64* ptr,
                                          atomic64 increment) {
  return barrier_atomicincrement(ptr, increment);
}

inline void nobarrier_store(volatile atomic64* ptr, atomic64 value) {
  *ptr = value;
}

inline void acquire_store(volatile atomic64* ptr, atomic64 value) {
  nobarrier_atomicexchange(ptr, value);
              // acts as a barrier in this implementation
}

inline void release_store(volatile atomic64* ptr, atomic64 value) {
  *ptr = value;  // works w/o barrier for current intel chips as of june 2005

  // when new chips come out, check:
  //  ia-32 intel architecture software developer's manual, volume 3:
  //  system programming guide, chatper 7: multiple-processor management,
  //  section 7.2, memory ordering.
  // last seen at:
  //   http://developer.intel.com/design/pentium4/manuals/index_new.htm
}

inline atomic64 nobarrier_load(volatile const atomic64* ptr) {
  return *ptr;
}

inline atomic64 acquire_load(volatile const atomic64* ptr) {
  atomic64 value = *ptr;
  return value;
}

inline atomic64 release_load(volatile const atomic64* ptr) {
  memorybarrier();
  return *ptr;
}

inline atomic64 acquire_compareandswap(volatile atomic64* ptr,
                                       atomic64 old_value,
                                       atomic64 new_value) {
  return nobarrier_compareandswap(ptr, old_value, new_value);
}

inline atomic64 release_compareandswap(volatile atomic64* ptr,
                                       atomic64 old_value,
                                       atomic64 new_value) {
  return nobarrier_compareandswap(ptr, old_value, new_value);
}

#endif  // defined(_win64)

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_atomicops_internals_x86_msvc_h_
