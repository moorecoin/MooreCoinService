// protocol buffers - google's data interchange format
// copyright 2008 google inc.  all rights reserved.
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

// author: kenton@google.com (kenton varda)
//
// emulates google3/base/once.h
//
// this header is intended to be included only by internal .cc files and
// generated .pb.cc files.  users should not use this directly.

#include <google/protobuf/stubs/once.h>

#ifndef google_protobuf_no_thread_safety

#ifdef _win32
#include <windows.h>
#else
#include <sched.h>
#endif

#include <google/protobuf/stubs/atomicops.h>

namespace google {
namespace protobuf {

namespace {

void schedyield() {
#ifdef _win32
  sleep(0);
#else  // posix
  sched_yield();
#endif
}

}  // namespace

void googleonceinitimpl(protobufoncetype* once, closure* closure) {
  internal::atomicword state = internal::acquire_load(once);
  // fast path. the provided closure was already executed.
  if (state == once_state_done) {
    return;
  }
  // the closure execution did not complete yet. the once object can be in one
  // of the two following states:
  //   - uninitialized: we are the first thread calling this function.
  //   - executing_closure: another thread is already executing the closure.
  //
  // first, try to change the state from uninitialized to executing_closure
  // atomically.
  state = internal::acquire_compareandswap(
      once, once_state_uninitialized, once_state_executing_closure);
  if (state == once_state_uninitialized) {
    // we are the first thread to call this function, so we have to call the
    // closure.
    closure->run();
    internal::release_store(once, once_state_done);
  } else {
    // another thread has already started executing the closure. we need to
    // wait until it completes the initialization.
    while (state == once_state_executing_closure) {
      // note that futex() could be used here on linux as an improvement.
      schedyield();
      state = internal::acquire_load(once);
    }
  }
}

}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_no_thread_safety
