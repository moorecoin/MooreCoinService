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

// author: kenton@google.com (kenton varda) and others
//
// contains basic types and utilities used by the rest of the library.

#ifndef google_protobuf_common_h__
#define google_protobuf_common_h__

#include <assert.h>
#include <stdlib.h>
#include <cstddef>
#include <string>
#include <string.h>
#if defined(__osf__)
// tru64 lacks stdint.h, but has inttypes.h which defines a superset of
// what stdint.h would define.
#include <inttypes.h>
#elif !defined(_msc_ver)
#include <stdint.h>
#endif

#ifndef protobuf_use_exceptions
#if defined(_msc_ver) && defined(_cppunwind)
  #define protobuf_use_exceptions 1
#elif defined(__exceptions)
  #define protobuf_use_exceptions 1
#else
  #define protobuf_use_exceptions 0
#endif
#endif

#if protobuf_use_exceptions
#include <exception>
#endif

#if defined(_win32) && defined(getmessage)
// allow getmessage to be used as a valid method name in protobuf classes.
// windows.h defines getmessage() as a macro.  let's re-define it as an inline
// function.  the inline function should be equivalent for c++ users.
inline bool getmessage_win32(
    lpmsg lpmsg, hwnd hwnd,
    uint wmsgfiltermin, uint wmsgfiltermax) {
  return getmessage(lpmsg, hwnd, wmsgfiltermin, wmsgfiltermax);
}
#undef getmessage
inline bool getmessage(
    lpmsg lpmsg, hwnd hwnd,
    uint wmsgfiltermin, uint wmsgfiltermax) {
  return getmessage_win32(lpmsg, hwnd, wmsgfiltermin, wmsgfiltermax);
}
#endif


namespace std {}

namespace google {
namespace protobuf {

#undef google_disallow_evil_constructors
#define google_disallow_evil_constructors(typename)    \
  typename(const typename&);                           \
  void operator=(const typename&)

#if defined(_msc_ver) && defined(protobuf_use_dlls)
  #ifdef libprotobuf_exports
    #define libprotobuf_export __declspec(dllexport)
  #else
    #define libprotobuf_export __declspec(dllimport)
  #endif
  #ifdef libprotoc_exports
    #define libprotoc_export   __declspec(dllexport)
  #else
    #define libprotoc_export   __declspec(dllimport)
  #endif
#else
  #define libprotobuf_export
  #define libprotoc_export
#endif

namespace internal {

// some of these constants are macros rather than const ints so that they can
// be used in #if directives.

// the current version, represented as a single integer to make comparison
// easier:  major * 10^6 + minor * 10^3 + micro
#define google_protobuf_version 2005001

// the minimum library version which works with the current version of the
// headers.
#define google_protobuf_min_library_version 2005001

// the minimum header version which works with the current version of
// the library.  this constant should only be used by protoc's c++ code
// generator.
static const int kminheaderversionforlibrary = 2005001;

// the minimum protoc version which works with the current version of the
// headers.
#define google_protobuf_min_protoc_version 2005001

// the minimum header version which works with the current version of
// protoc.  this constant should only be used in verifyversion().
static const int kminheaderversionforprotoc = 2005001;

// verifies that the headers and libraries are compatible.  use the macro
// below to call this.
void libprotobuf_export verifyversion(int headerversion, int minlibraryversion,
                                      const char* filename);

// converts a numeric version number to a string.
std::string libprotobuf_export versionstring(int version);

}  // namespace internal

// place this macro in your main() function (or somewhere before you attempt
// to use the protobuf library) to verify that the version you link against
// matches the headers you compiled against.  if a version mismatch is
// detected, the process will abort.
#define google_protobuf_verify_version                                    \
  ::google::protobuf::internal::verifyversion(                            \
    google_protobuf_version, google_protobuf_min_library_version,         \
    __file__)

// ===================================================================
// from google3/base/port.h

typedef unsigned int uint;

#ifdef _msc_ver
typedef __int8  int8;
typedef __int16 int16;
typedef __int32 int32;
typedef __int64 int64;

typedef unsigned __int8  uint8;
typedef unsigned __int16 uint16;
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;
#else
typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
#endif

// long long macros to be used because gcc and vc++ use different suffixes,
// and different size specifiers in format strings
#undef google_longlong
#undef google_ulonglong
#undef google_ll_format

#ifdef _msc_ver
#define google_longlong(x) x##i64
#define google_ulonglong(x) x##ui64
#define google_ll_format "i64"  // as in printf("%i64d", ...)
#else
#define google_longlong(x) x##ll
#define google_ulonglong(x) x##ull
#define google_ll_format "ll"  // as in "%lld". note that "q" is poor form also.
#endif

static const int32 kint32max = 0x7fffffff;
static const int32 kint32min = -kint32max - 1;
static const int64 kint64max = google_longlong(0x7fffffffffffffff);
static const int64 kint64min = -kint64max - 1;
static const uint32 kuint32max = 0xffffffffu;
static const uint64 kuint64max = google_ulonglong(0xffffffffffffffff);

// -------------------------------------------------------------------
// annotations:  some parts of the code have been annotated in ways that might
//   be useful to some compilers or tools, but are not supported universally.
//   you can #define these annotations yourself if the default implementation
//   is not right for you.

#ifndef google_attribute_always_inline
#if defined(__gnuc__) && (__gnuc__ > 3 ||(__gnuc__ == 3 && __gnuc_minor__ >= 1))
// for functions we want to force inline.
// introduced in gcc 3.1.
#define google_attribute_always_inline __attribute__ ((always_inline))
#else
// other compilers will have to figure it out for themselves.
#define google_attribute_always_inline
#endif
#endif

#ifndef google_attribute_deprecated
#ifdef __gnuc__
// if the method/variable/type is used anywhere, produce a warning.
#define google_attribute_deprecated __attribute__((deprecated))
#else
#define google_attribute_deprecated
#endif
#endif

#ifndef google_predict_true
#ifdef __gnuc__
// provided at least since gcc 3.0.
#define google_predict_true(x) (__builtin_expect(!!(x), 1))
#else
#define google_predict_true
#endif
#endif

// delimits a block of code which may write to memory which is simultaneously
// written by other threads, but which has been determined to be thread-safe
// (e.g. because it is an idempotent write).
#ifndef google_safe_concurrent_writes_begin
#define google_safe_concurrent_writes_begin()
#endif
#ifndef google_safe_concurrent_writes_end
#define google_safe_concurrent_writes_end()
#endif

// ===================================================================
// from google3/base/basictypes.h

// the google_arraysize(arr) macro returns the # of elements in an array arr.
// the expression is a compile-time constant, and therefore can be
// used in defining new arrays, for example.
//
// google_arraysize catches a few type errors.  if you see a compiler error
//
//   "warning: division by zero in ..."
//
// when using google_arraysize, you are (wrongfully) giving it a pointer.
// you should only use google_arraysize on statically allocated arrays.
//
// the following comments are on the implementation details, and can
// be ignored by the users.
//
// arraysize(arr) works by inspecting sizeof(arr) (the # of bytes in
// the array) and sizeof(*(arr)) (the # of bytes in one array
// element).  if the former is divisible by the latter, perhaps arr is
// indeed an array, in which case the division result is the # of
// elements in the array.  otherwise, arr cannot possibly be an array,
// and we generate a compiler error to prevent the code from
// compiling.
//
// since the size of bool is implementation-defined, we need to cast
// !(sizeof(a) & sizeof(*(a))) to size_t in order to ensure the final
// result has type size_t.
//
// this macro is not perfect as it wrongfully accepts certain
// pointers, namely where the pointer size is divisible by the pointee
// size.  since all our code has to go through a 32-bit compiler,
// where a pointer is 4 bytes, this means all pointers to a type whose
// size is 3 or greater than 4 will be (righteously) rejected.
//
// kudos to jorg brown for this simple and elegant implementation.

#undef google_arraysize
#define google_arraysize(a) \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

namespace internal {

// use implicit_cast as a safe version of static_cast or const_cast
// for upcasting in the type hierarchy (i.e. casting a pointer to foo
// to a pointer to superclassoffoo or casting a pointer to foo to
// a const pointer to foo).
// when you use implicit_cast, the compiler checks that the cast is safe.
// such explicit implicit_casts are necessary in surprisingly many
// situations where c++ demands an exact type match instead of an
// argument type convertable to a target type.
//
// the from type can be inferred, so the preferred syntax for using
// implicit_cast is the same as for static_cast etc.:
//
//   implicit_cast<totype>(expr)
//
// implicit_cast would have been part of the c++ standard library,
// but the proposal was submitted too late.  it will probably make
// its way into the language in the future.
template<typename to, typename from>
inline to implicit_cast(from const &f) {
  return f;
}

// when you upcast (that is, cast a pointer from type foo to type
// superclassoffoo), it's fine to use implicit_cast<>, since upcasts
// always succeed.  when you downcast (that is, cast a pointer from
// type foo to type subclassoffoo), static_cast<> isn't safe, because
// how do you know the pointer is really of type subclassoffoo?  it
// could be a bare foo, or of type differentsubclassoffoo.  thus,
// when you downcast, you should use this macro.  in debug mode, we
// use dynamic_cast<> to double-check the downcast is legal (we die
// if it's not).  in normal mode, we do the efficient static_cast<>
// instead.  thus, it's important to test in debug mode to make sure
// the cast is legal!
//    this is the only place in the code we should use dynamic_cast<>.
// in particular, you shouldn't be using dynamic_cast<> in order to
// do rtti (eg code like this:
//    if (dynamic_cast<subclass1>(foo)) handleasubclass1object(foo);
//    if (dynamic_cast<subclass2>(foo)) handleasubclass2object(foo);
// you should design the code some other way not to need this.

template<typename to, typename from>     // use like this: down_cast<t*>(foo);
inline to down_cast(from* f) {                   // so we only accept pointers
  // ensures that to is a sub-type of from *.  this test is here only
  // for compile-time type checking, and has no overhead in an
  // optimized build at run-time, as it will be optimized away
  // completely.
  if (false) {
    implicit_cast<from*, to>(0);
  }

#if !defined(ndebug) && !defined(google_protobuf_no_rtti)
  assert(f == null || dynamic_cast<to>(f) != null);  // rtti: debug mode only!
#endif
  return static_cast<to>(f);
}

}  // namespace internal

// we made these internal so that they would show up as such in the docs,
// but we don't want to stick "internal::" in front of them everywhere.
using internal::implicit_cast;
using internal::down_cast;

// the compile_assert macro can be used to verify that a compile time
// expression is true. for example, you could use it to verify the
// size of a static array:
//
//   compile_assert(arraysize(content_type_names) == content_num_types,
//                  content_type_names_incorrect_size);
//
// or to make sure a struct is smaller than a certain size:
//
//   compile_assert(sizeof(foo) < 128, foo_too_large);
//
// the second argument to the macro is the name of the variable. if
// the expression is false, most compilers will issue a warning/error
// containing the name of the variable.

namespace internal {

template <bool>
struct compileassert {
};

}  // namespace internal

#undef google_compile_assert
#define google_compile_assert(expr, msg) \
  typedef ::google::protobuf::internal::compileassert<(bool(expr))> \
          msg[bool(expr) ? 1 : -1]


// implementation details of compile_assert:
//
// - compile_assert works by defining an array type that has -1
//   elements (and thus is invalid) when the expression is false.
//
// - the simpler definition
//
//     #define compile_assert(expr, msg) typedef char msg[(expr) ? 1 : -1]
//
//   does not work, as gcc supports variable-length arrays whose sizes
//   are determined at run-time (this is gcc's extension and not part
//   of the c++ standard).  as a result, gcc fails to reject the
//   following code with the simple definition:
//
//     int foo;
//     compile_assert(foo, msg); // not supposed to compile as foo is
//                               // not a compile-time constant.
//
// - by using the type compileassert<(bool(expr))>, we ensures that
//   expr is a compile-time constant.  (template arguments must be
//   determined at compile-time.)
//
// - the outter parentheses in compileassert<(bool(expr))> are necessary
//   to work around a bug in gcc 3.4.4 and 4.0.1.  if we had written
//
//     compileassert<bool(expr)>
//
//   instead, these compilers will refuse to compile
//
//     compile_assert(5 > 0, some_message);
//
//   (they seem to think the ">" in "5 > 0" marks the end of the
//   template argument list.)
//
// - the array size is (bool(expr) ? 1 : -1), instead of simply
//
//     ((expr) ? 1 : -1).
//
//   this is to avoid running into a bug in ms vc 7.1, which
//   causes ((0.0) ? 1 : -1) to incorrectly evaluate to 1.

// ===================================================================
// from google3/base/scoped_ptr.h

namespace internal {

//  this is an implementation designed to match the anticipated future tr2
//  implementation of the scoped_ptr class, and its closely-related brethren,
//  scoped_array, scoped_ptr_malloc, and make_scoped_ptr.

template <class c> class scoped_ptr;
template <class c> class scoped_array;

// a scoped_ptr<t> is like a t*, except that the destructor of scoped_ptr<t>
// automatically deletes the pointer it holds (if any).
// that is, scoped_ptr<t> owns the t object that it points to.
// like a t*, a scoped_ptr<t> may hold either null or a pointer to a t object.
//
// the size of a scoped_ptr is small:
// sizeof(scoped_ptr<c>) == sizeof(c*)
template <class c>
class scoped_ptr {
 public:

  // the element type
  typedef c element_type;

  // constructor.  defaults to intializing with null.
  // there is no way to create an uninitialized scoped_ptr.
  // the input parameter must be allocated with new.
  explicit scoped_ptr(c* p = null) : ptr_(p) { }

  // destructor.  if there is a c object, delete it.
  // we don't need to test ptr_ == null because c++ does that for us.
  ~scoped_ptr() {
    enum { type_must_be_complete = sizeof(c) };
    delete ptr_;
  }

  // reset.  deletes the current owned object, if any.
  // then takes ownership of a new object, if given.
  // this->reset(this->get()) works.
  void reset(c* p = null) {
    if (p != ptr_) {
      enum { type_must_be_complete = sizeof(c) };
      delete ptr_;
      ptr_ = p;
    }
  }

  // accessors to get the owned object.
  // operator* and operator-> will assert() if there is no current object.
  c& operator*() const {
    assert(ptr_ != null);
    return *ptr_;
  }
  c* operator->() const  {
    assert(ptr_ != null);
    return ptr_;
  }
  c* get() const { return ptr_; }

  // comparison operators.
  // these return whether two scoped_ptr refer to the same object, not just to
  // two different but equal objects.
  bool operator==(c* p) const { return ptr_ == p; }
  bool operator!=(c* p) const { return ptr_ != p; }

  // swap two scoped pointers.
  void swap(scoped_ptr& p2) {
    c* tmp = ptr_;
    ptr_ = p2.ptr_;
    p2.ptr_ = tmp;
  }

  // release a pointer.
  // the return value is the current pointer held by this object.
  // if this object holds a null pointer, the return value is null.
  // after this operation, this object will hold a null pointer,
  // and will not own the object any more.
  c* release() {
    c* retval = ptr_;
    ptr_ = null;
    return retval;
  }

 private:
  c* ptr_;

  // forbid comparison of scoped_ptr types.  if c2 != c, it totally doesn't
  // make sense, and if c2 == c, it still doesn't make sense because you should
  // never have the same object owned by two different scoped_ptrs.
  template <class c2> bool operator==(scoped_ptr<c2> const& p2) const;
  template <class c2> bool operator!=(scoped_ptr<c2> const& p2) const;

  // disallow evil constructors
  scoped_ptr(const scoped_ptr&);
  void operator=(const scoped_ptr&);
};

// scoped_array<c> is like scoped_ptr<c>, except that the caller must allocate
// with new [] and the destructor deletes objects with delete [].
//
// as with scoped_ptr<c>, a scoped_array<c> either points to an object
// or is null.  a scoped_array<c> owns the object that it points to.
//
// size: sizeof(scoped_array<c>) == sizeof(c*)
template <class c>
class scoped_array {
 public:

  // the element type
  typedef c element_type;

  // constructor.  defaults to intializing with null.
  // there is no way to create an uninitialized scoped_array.
  // the input parameter must be allocated with new [].
  explicit scoped_array(c* p = null) : array_(p) { }

  // destructor.  if there is a c object, delete it.
  // we don't need to test ptr_ == null because c++ does that for us.
  ~scoped_array() {
    enum { type_must_be_complete = sizeof(c) };
    delete[] array_;
  }

  // reset.  deletes the current owned object, if any.
  // then takes ownership of a new object, if given.
  // this->reset(this->get()) works.
  void reset(c* p = null) {
    if (p != array_) {
      enum { type_must_be_complete = sizeof(c) };
      delete[] array_;
      array_ = p;
    }
  }

  // get one element of the current object.
  // will assert() if there is no current object, or index i is negative.
  c& operator[](std::ptrdiff_t i) const {
    assert(i >= 0);
    assert(array_ != null);
    return array_[i];
  }

  // get a pointer to the zeroth element of the current object.
  // if there is no current object, return null.
  c* get() const {
    return array_;
  }

  // comparison operators.
  // these return whether two scoped_array refer to the same object, not just to
  // two different but equal objects.
  bool operator==(c* p) const { return array_ == p; }
  bool operator!=(c* p) const { return array_ != p; }

  // swap two scoped arrays.
  void swap(scoped_array& p2) {
    c* tmp = array_;
    array_ = p2.array_;
    p2.array_ = tmp;
  }

  // release an array.
  // the return value is the current pointer held by this object.
  // if this object holds a null pointer, the return value is null.
  // after this operation, this object will hold a null pointer,
  // and will not own the object any more.
  c* release() {
    c* retval = array_;
    array_ = null;
    return retval;
  }

 private:
  c* array_;

  // forbid comparison of different scoped_array types.
  template <class c2> bool operator==(scoped_array<c2> const& p2) const;
  template <class c2> bool operator!=(scoped_array<c2> const& p2) const;

  // disallow evil constructors
  scoped_array(const scoped_array&);
  void operator=(const scoped_array&);
};

}  // namespace internal

// we made these internal so that they would show up as such in the docs,
// but we don't want to stick "internal::" in front of them everywhere.
using internal::scoped_ptr;
using internal::scoped_array;

// ===================================================================
// emulates google3/base/logging.h

enum loglevel {
  loglevel_info,     // informational.  this is never actually used by
                     // libprotobuf.
  loglevel_warning,  // warns about issues that, although not technically a
                     // problem now, could cause problems in the future.  for
                     // example, a // warning will be printed when parsing a
                     // message that is near the message size limit.
  loglevel_error,    // an error occurred which should never happen during
                     // normal use.
  loglevel_fatal,    // an error occurred from which the library cannot
                     // recover.  this usually indicates a programming error
                     // in the code which calls the library, especially when
                     // compiled in debug mode.

#ifdef ndebug
  loglevel_dfatal = loglevel_error
#else
  loglevel_dfatal = loglevel_fatal
#endif
};

namespace internal {

class logfinisher;

class libprotobuf_export logmessage {
 public:
  logmessage(loglevel level, const char* filename, int line);
  ~logmessage();

  logmessage& operator<<(const std::string& value);
  logmessage& operator<<(const char* value);
  logmessage& operator<<(char value);
  logmessage& operator<<(int value);
  logmessage& operator<<(uint value);
  logmessage& operator<<(long value);
  logmessage& operator<<(unsigned long value);
  logmessage& operator<<(double value);

 private:
  friend class logfinisher;
  void finish();

  loglevel level_;
  const char* filename_;
  int line_;
  std::string message_;
};

// used to make the entire "log(blah) << etc." expression have a void return
// type and print a newline after each message.
class libprotobuf_export logfinisher {
 public:
  void operator=(logmessage& other);
};

}  // namespace internal

// undef everything in case we're being mixed with some other google library
// which already defined them itself.  presumably all google libraries will
// support the same syntax for these so it should not be a big deal if they
// end up using our definitions instead.
#undef google_log
#undef google_log_if

#undef google_check
#undef google_check_eq
#undef google_check_ne
#undef google_check_lt
#undef google_check_le
#undef google_check_gt
#undef google_check_ge
#undef google_check_notnull

#undef google_dlog
#undef google_dcheck
#undef google_dcheck_eq
#undef google_dcheck_ne
#undef google_dcheck_lt
#undef google_dcheck_le
#undef google_dcheck_gt
#undef google_dcheck_ge

#define google_log(level)                                                 \
  ::google::protobuf::internal::logfinisher() =                           \
    ::google::protobuf::internal::logmessage(                             \
      ::google::protobuf::loglevel_##level, __file__, __line__)
#define google_log_if(level, condition) \
  !(condition) ? (void)0 : google_log(level)

#define google_check(expression) \
  google_log_if(fatal, !(expression)) << "check failed: " #expression ": "
#define google_check_eq(a, b) google_check((a) == (b))
#define google_check_ne(a, b) google_check((a) != (b))
#define google_check_lt(a, b) google_check((a) <  (b))
#define google_check_le(a, b) google_check((a) <= (b))
#define google_check_gt(a, b) google_check((a) >  (b))
#define google_check_ge(a, b) google_check((a) >= (b))

namespace internal {
template<typename t>
t* checknotnull(const char *file, int line, const char *name, t* val) {
  if (val == null) {
    google_log(fatal) << name;
  }
  return val;
}
}  // namespace internal
#define google_check_notnull(a) \
  internal::checknotnull(__file__, __line__, "'" #a "' must not be null", (a))

#ifdef ndebug

#define google_dlog google_log_if(info, false)

#define google_dcheck(expression) while(false) google_check(expression)
#define google_dcheck_eq(a, b) google_dcheck((a) == (b))
#define google_dcheck_ne(a, b) google_dcheck((a) != (b))
#define google_dcheck_lt(a, b) google_dcheck((a) <  (b))
#define google_dcheck_le(a, b) google_dcheck((a) <= (b))
#define google_dcheck_gt(a, b) google_dcheck((a) >  (b))
#define google_dcheck_ge(a, b) google_dcheck((a) >= (b))

#else  // ndebug

#define google_dlog google_log

#define google_dcheck    google_check
#define google_dcheck_eq google_check_eq
#define google_dcheck_ne google_check_ne
#define google_dcheck_lt google_check_lt
#define google_dcheck_le google_check_le
#define google_dcheck_gt google_check_gt
#define google_dcheck_ge google_check_ge

#endif  // !ndebug

typedef void loghandler(loglevel level, const char* filename, int line,
                        const std::string& message);

// the protobuf library sometimes writes warning and error messages to
// stderr.  these messages are primarily useful for developers, but may
// also help end users figure out a problem.  if you would prefer that
// these messages be sent somewhere other than stderr, call setloghandler()
// to set your own handler.  this returns the old handler.  set the handler
// to null to ignore log messages (but see also logsilencer, below).
//
// obviously, setloghandler is not thread-safe.  you should only call it
// at initialization time, and probably not from library code.  if you
// simply want to suppress log messages temporarily (e.g. because you
// have some code that tends to trigger them frequently and you know
// the warnings are not important to you), use the logsilencer class
// below.
libprotobuf_export loghandler* setloghandler(loghandler* new_func);

// create a logsilencer if you want to temporarily suppress all log
// messages.  as long as any logsilencer objects exist, non-fatal
// log messages will be discarded (the current loghandler will *not*
// be called).  constructing a logsilencer is thread-safe.  you may
// accidentally suppress log messages occurring in another thread, but
// since messages are generally for debugging purposes only, this isn't
// a big deal.  if you want to intercept log messages, use setloghandler().
class libprotobuf_export logsilencer {
 public:
  logsilencer();
  ~logsilencer();
};

// ===================================================================
// emulates google3/base/callback.h

// abstract interface for a callback.  when calling an rpc, you must provide
// a closure to call when the procedure completes.  see the service interface
// in service.h.
//
// to automatically construct a closure which calls a particular function or
// method with a particular set of parameters, use the newcallback() function.
// example:
//   void foodone(const fooresponse* response) {
//     ...
//   }
//
//   void callfoo() {
//     ...
//     // when done, call foodone() and pass it a pointer to the response.
//     closure* callback = newcallback(&foodone, response);
//     // make the call.
//     service->foo(controller, request, response, callback);
//   }
//
// example that calls a method:
//   class handler {
//    public:
//     ...
//
//     void foodone(const fooresponse* response) {
//       ...
//     }
//
//     void callfoo() {
//       ...
//       // when done, call foodone() and pass it a pointer to the response.
//       closure* callback = newcallback(this, &handler::foodone, response);
//       // make the call.
//       service->foo(controller, request, response, callback);
//     }
//   };
//
// currently newcallback() supports binding zero, one, or two arguments.
//
// callbacks created with newcallback() automatically delete themselves when
// executed.  they should be used when a callback is to be called exactly
// once (usually the case with rpc callbacks).  if a callback may be called
// a different number of times (including zero), create it with
// newpermanentcallback() instead.  you are then responsible for deleting the
// callback (using the "delete" keyword as normal).
//
// note that newcallback() is a bit touchy regarding argument types.  generally,
// the values you provide for the parameter bindings must exactly match the
// types accepted by the callback function.  for example:
//   void foo(string s);
//   newcallback(&foo, "foo");          // won't work:  const char* != string
//   newcallback(&foo, string("foo"));  // works
// also note that the arguments cannot be references:
//   void foo(const string& s);
//   string my_str;
//   newcallback(&foo, my_str);  // won't work:  can't use referecnes.
// however, correctly-typed pointers will work just fine.
class libprotobuf_export closure {
 public:
  closure() {}
  virtual ~closure();

  virtual void run() = 0;

 private:
  google_disallow_evil_constructors(closure);
};

namespace internal {

class libprotobuf_export functionclosure0 : public closure {
 public:
  typedef void (*functiontype)();

  functionclosure0(functiontype function, bool self_deleting)
    : function_(function), self_deleting_(self_deleting) {}
  ~functionclosure0();

  void run() {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    function_();
    if (needs_delete) delete this;
  }

 private:
  functiontype function_;
  bool self_deleting_;
};

template <typename class>
class methodclosure0 : public closure {
 public:
  typedef void (class::*methodtype)();

  methodclosure0(class* object, methodtype method, bool self_deleting)
    : object_(object), method_(method), self_deleting_(self_deleting) {}
  ~methodclosure0() {}

  void run() {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    (object_->*method_)();
    if (needs_delete) delete this;
  }

 private:
  class* object_;
  methodtype method_;
  bool self_deleting_;
};

template <typename arg1>
class functionclosure1 : public closure {
 public:
  typedef void (*functiontype)(arg1 arg1);

  functionclosure1(functiontype function, bool self_deleting,
                   arg1 arg1)
    : function_(function), self_deleting_(self_deleting),
      arg1_(arg1) {}
  ~functionclosure1() {}

  void run() {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    function_(arg1_);
    if (needs_delete) delete this;
  }

 private:
  functiontype function_;
  bool self_deleting_;
  arg1 arg1_;
};

template <typename class, typename arg1>
class methodclosure1 : public closure {
 public:
  typedef void (class::*methodtype)(arg1 arg1);

  methodclosure1(class* object, methodtype method, bool self_deleting,
                 arg1 arg1)
    : object_(object), method_(method), self_deleting_(self_deleting),
      arg1_(arg1) {}
  ~methodclosure1() {}

  void run() {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    (object_->*method_)(arg1_);
    if (needs_delete) delete this;
  }

 private:
  class* object_;
  methodtype method_;
  bool self_deleting_;
  arg1 arg1_;
};

template <typename arg1, typename arg2>
class functionclosure2 : public closure {
 public:
  typedef void (*functiontype)(arg1 arg1, arg2 arg2);

  functionclosure2(functiontype function, bool self_deleting,
                   arg1 arg1, arg2 arg2)
    : function_(function), self_deleting_(self_deleting),
      arg1_(arg1), arg2_(arg2) {}
  ~functionclosure2() {}

  void run() {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    function_(arg1_, arg2_);
    if (needs_delete) delete this;
  }

 private:
  functiontype function_;
  bool self_deleting_;
  arg1 arg1_;
  arg2 arg2_;
};

template <typename class, typename arg1, typename arg2>
class methodclosure2 : public closure {
 public:
  typedef void (class::*methodtype)(arg1 arg1, arg2 arg2);

  methodclosure2(class* object, methodtype method, bool self_deleting,
                 arg1 arg1, arg2 arg2)
    : object_(object), method_(method), self_deleting_(self_deleting),
      arg1_(arg1), arg2_(arg2) {}
  ~methodclosure2() {}

  void run() {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    (object_->*method_)(arg1_, arg2_);
    if (needs_delete) delete this;
  }

 private:
  class* object_;
  methodtype method_;
  bool self_deleting_;
  arg1 arg1_;
  arg2 arg2_;
};

}  // namespace internal

// see closure.
inline closure* newcallback(void (*function)()) {
  return new internal::functionclosure0(function, true);
}

// see closure.
inline closure* newpermanentcallback(void (*function)()) {
  return new internal::functionclosure0(function, false);
}

// see closure.
template <typename class>
inline closure* newcallback(class* object, void (class::*method)()) {
  return new internal::methodclosure0<class>(object, method, true);
}

// see closure.
template <typename class>
inline closure* newpermanentcallback(class* object, void (class::*method)()) {
  return new internal::methodclosure0<class>(object, method, false);
}

// see closure.
template <typename arg1>
inline closure* newcallback(void (*function)(arg1),
                            arg1 arg1) {
  return new internal::functionclosure1<arg1>(function, true, arg1);
}

// see closure.
template <typename arg1>
inline closure* newpermanentcallback(void (*function)(arg1),
                                     arg1 arg1) {
  return new internal::functionclosure1<arg1>(function, false, arg1);
}

// see closure.
template <typename class, typename arg1>
inline closure* newcallback(class* object, void (class::*method)(arg1),
                            arg1 arg1) {
  return new internal::methodclosure1<class, arg1>(object, method, true, arg1);
}

// see closure.
template <typename class, typename arg1>
inline closure* newpermanentcallback(class* object, void (class::*method)(arg1),
                                     arg1 arg1) {
  return new internal::methodclosure1<class, arg1>(object, method, false, arg1);
}

// see closure.
template <typename arg1, typename arg2>
inline closure* newcallback(void (*function)(arg1, arg2),
                            arg1 arg1, arg2 arg2) {
  return new internal::functionclosure2<arg1, arg2>(
    function, true, arg1, arg2);
}

// see closure.
template <typename arg1, typename arg2>
inline closure* newpermanentcallback(void (*function)(arg1, arg2),
                                     arg1 arg1, arg2 arg2) {
  return new internal::functionclosure2<arg1, arg2>(
    function, false, arg1, arg2);
}

// see closure.
template <typename class, typename arg1, typename arg2>
inline closure* newcallback(class* object, void (class::*method)(arg1, arg2),
                            arg1 arg1, arg2 arg2) {
  return new internal::methodclosure2<class, arg1, arg2>(
    object, method, true, arg1, arg2);
}

// see closure.
template <typename class, typename arg1, typename arg2>
inline closure* newpermanentcallback(
    class* object, void (class::*method)(arg1, arg2),
    arg1 arg1, arg2 arg2) {
  return new internal::methodclosure2<class, arg1, arg2>(
    object, method, false, arg1, arg2);
}

// a function which does nothing.  useful for creating no-op callbacks, e.g.:
//   closure* nothing = newcallback(&donothing);
void libprotobuf_export donothing();

// ===================================================================
// emulates google3/base/mutex.h

namespace internal {

// a mutex is a non-reentrant (aka non-recursive) mutex.  at most one thread t
// may hold a mutex at a given time.  if t attempts to lock() the same mutex
// while holding it, t will deadlock.
class libprotobuf_export mutex {
 public:
  // create a mutex that is not held by anybody.
  mutex();

  // destructor
  ~mutex();

  // block if necessary until this mutex is free, then acquire it exclusively.
  void lock();

  // release this mutex.  caller must hold it exclusively.
  void unlock();

  // crash if this mutex is not held exclusively by this thread.
  // may fail to crash when it should; will never crash when it should not.
  void assertheld();

 private:
  struct internal;
  internal* minternal;

  google_disallow_evil_constructors(mutex);
};

// mutexlock(mu) acquires mu when constructed and releases it when destroyed.
class libprotobuf_export mutexlock {
 public:
  explicit mutexlock(mutex *mu) : mu_(mu) { this->mu_->lock(); }
  ~mutexlock() { this->mu_->unlock(); }
 private:
  mutex *const mu_;
  google_disallow_evil_constructors(mutexlock);
};

// todo(kenton):  implement these?  hard to implement portably.
typedef mutexlock readermutexlock;
typedef mutexlock writermutexlock;

// mutexlockmaybe is like mutexlock, but is a no-op when mu is null.
class libprotobuf_export mutexlockmaybe {
 public:
  explicit mutexlockmaybe(mutex *mu) :
    mu_(mu) { if (this->mu_ != null) { this->mu_->lock(); } }
  ~mutexlockmaybe() { if (this->mu_ != null) { this->mu_->unlock(); } }
 private:
  mutex *const mu_;
  google_disallow_evil_constructors(mutexlockmaybe);
};

}  // namespace internal

// we made these internal so that they would show up as such in the docs,
// but we don't want to stick "internal::" in front of them everywhere.
using internal::mutex;
using internal::mutexlock;
using internal::readermutexlock;
using internal::writermutexlock;
using internal::mutexlockmaybe;

// ===================================================================
// from google3/util/utf8/public/unilib.h

namespace internal {

// checks if the buffer contains structurally-valid utf-8.  implemented in
// structurally_valid.cc.
libprotobuf_export bool isstructurallyvalidutf8(const char* buf, int len);

}  // namespace internal

// ===================================================================
// from google3/util/endian/endian.h
libprotobuf_export uint32 ghtonl(uint32 x);

// ===================================================================
// shutdown support.

// shut down the entire protocol buffers library, deleting all static-duration
// objects allocated by the library or by generated .pb.cc files.
//
// there are two reasons you might want to call this:
// * you use a draconian definition of "memory leak" in which you expect
//   every single malloc() to have a corresponding free(), even for objects
//   which live until program exit.
// * you are writing a dynamically-loaded library which needs to clean up
//   after itself when the library is unloaded.
//
// it is safe to call this multiple times.  however, it is not safe to use
// any other part of the protocol buffers library after
// shutdownprotobuflibrary() has been called.
libprotobuf_export void shutdownprotobuflibrary();

namespace internal {

// register a function to be called when shutdownprotocolbuffers() is called.
libprotobuf_export void onshutdown(void (*func)());

}  // namespace internal

#if protobuf_use_exceptions
class fatalexception : public std::exception {
 public:
  fatalexception(const char* filename, int line, const std::string& message)
      : filename_(filename), line_(line), message_(message) {}
  virtual ~fatalexception() throw();

  virtual const char* what() const throw();

  const char* filename() const { return filename_; }
  int line() const { return line_; }
  const std::string& message() const { return message_; }

 private:
  const char* filename_;
  const int line_;
  const std::string message_;
};
#endif

// this is at the end of the file instead of the beginning to work around a bug
// in some versions of msvc.
using namespace std;  // don't do this at home, kids.

}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_common_h__
