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
//
// this is basically a portable version of pthread_once().
//
// this header declares:
// * a type called protobufoncetype.
// * a macro google_protobuf_declare_once() which declares a variable of type
//   protobufoncetype.  this is the only legal way to declare such a variable.
//   the macro may only be used at the global scope (you cannot create local or
//   class member variables of this type).
// * a function googleonceinit(protobufoncetype* once, void (*init_func)()).
//   this function, when invoked multiple times given the same protobufoncetype
//   object, will invoke init_func on the first call only, and will make sure
//   none of the calls return before that first call to init_func has finished.
// * the user can provide a parameter which googleonceinit() forwards to the
//   user-provided function when it is called. usage example:
//     int a = 10;
//     googleonceinit(&my_once, &myfunctionexpectingintargument, &a);
// * this implementation guarantees that protobufoncetype is a pod (i.e. no
//   static initializer generated).
//
// this implements a way to perform lazy initialization.  it's more efficient
// than using mutexes as no lock is needed if initialization has already
// happened.
//
// example usage:
//   void init();
//   google_protobuf_declare_once(once_init);
//
//   // calls init() exactly once.
//   void initonce() {
//     googleonceinit(&once_init, &init);
//   }
//
// note that if googleonceinit() is called before main() has begun, it must
// only be called by the thread that will eventually call main() -- that is,
// the thread that performs dynamic initialization.  in general this is a safe
// assumption since people don't usually construct threads before main() starts,
// but it is technically not guaranteed.  unfortunately, win32 provides no way
// whatsoever to statically-initialize its synchronization primitives, so our
// only choice is to assume that dynamic initialization is single-threaded.

#ifndef google_protobuf_stubs_once_h__
#define google_protobuf_stubs_once_h__

#include <google/protobuf/stubs/atomicops.h>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

#ifdef google_protobuf_no_thread_safety

typedef bool protobufoncetype;

#define google_protobuf_once_init false

inline void googleonceinit(protobufoncetype* once, void (*init_func)()) {
  if (!*once) {
    *once = true;
    init_func();
  }
}

template <typename arg>
inline void googleonceinit(protobufoncetype* once, void (*init_func)(arg),
    arg arg) {
  if (!*once) {
    *once = true;
    init_func(arg);
  }
}

#else

enum {
  once_state_uninitialized = 0,
  once_state_executing_closure = 1,
  once_state_done = 2
};

typedef internal::atomicword protobufoncetype;

#define google_protobuf_once_init ::google::protobuf::once_state_uninitialized

libprotobuf_export
void googleonceinitimpl(protobufoncetype* once, closure* closure);

inline void googleonceinit(protobufoncetype* once, void (*init_func)()) {
  if (internal::acquire_load(once) != once_state_done) {
    internal::functionclosure0 func(init_func, false);
    googleonceinitimpl(once, &func);
  }
}

template <typename arg>
inline void googleonceinit(protobufoncetype* once, void (*init_func)(arg*),
    arg* arg) {
  if (internal::acquire_load(once) != once_state_done) {
    internal::functionclosure1<arg*> func(init_func, false, arg);
    googleonceinitimpl(once, &func);
  }
}

#endif  // google_protobuf_no_thread_safety

#define google_protobuf_declare_once(name) \
  ::google::protobuf::protobufoncetype name = google_protobuf_once_init

}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_stubs_once_h__
