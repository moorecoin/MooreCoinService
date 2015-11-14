// copyright (c) 2006, google inc.
// all rights reserved.
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

// ----
// author: matt austern

#include <google/protobuf/stubs/type_traits.h>

#include <stdlib.h>   // for exit()
#include <stdio.h>
#include <string>
#include <vector>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

typedef int int32;
typedef long int64;

using std::string;
using std::vector;
using std::pair;


// this assertion produces errors like "error: invalid use of
// incomplete type 'struct <unnamed>::asserttypeseq<const int, int>'"
// when it fails.
template<typename t, typename u> struct asserttypeseq;
template<typename t> struct asserttypeseq<t, t> {};
#define compile_assert_types_eq(t, u) static_cast<void>(asserttypeseq<t, u>())

// a user-defined pod type.
struct a {
  int n_;
};

// a user-defined non-pod type with a trivial copy constructor.
class b {
 public:
  explicit b(int n) : n_(n) { }
 private:
  int n_;
};

// another user-defined non-pod type with a trivial copy constructor.
// we will explicitly declare c to have a trivial copy constructor
// by specializing has_trivial_copy.
class c {
 public:
  explicit c(int n) : n_(n) { }
 private:
  int n_;
};

namespace google {
namespace protobuf {
namespace internal {
template<> struct has_trivial_copy<c> : true_type { };
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// another user-defined non-pod type with a trivial assignment operator.
// we will explicitly declare c to have a trivial assignment operator
// by specializing has_trivial_assign.
class d {
 public:
  explicit d(int n) : n_(n) { }
 private:
  int n_;
};

namespace google {
namespace protobuf {
namespace internal {
template<> struct has_trivial_assign<d> : true_type { };
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// another user-defined non-pod type with a trivial constructor.
// we will explicitly declare e to have a trivial constructor
// by specializing has_trivial_constructor.
class e {
 public:
  int n_;
};

namespace google {
namespace protobuf {
namespace internal {
template<> struct has_trivial_constructor<e> : true_type { };
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// another user-defined non-pod type with a trivial destructor.
// we will explicitly declare e to have a trivial destructor
// by specializing has_trivial_destructor.
class f {
 public:
  explicit f(int n) : n_(n) { }
 private:
  int n_;
};

namespace google {
namespace protobuf {
namespace internal {
template<> struct has_trivial_destructor<f> : true_type { };
}  // namespace internal
}  // namespace protobuf
}  // namespace google

enum g {};

union h {};

class i {
 public:
  operator int() const;
};

class j {
 private:
  operator int() const;
};

namespace google {
namespace protobuf {
namespace internal {
namespace {

// a base class and a derived class that inherits from it, used for
// testing conversion type traits.
class base {
 public:
  virtual ~base() { }
};

class derived : public base {
};

test(typetraitstest, testisinteger) {
  // verify that is_integral is true for all integer types.
  expect_true(is_integral<bool>::value);
  expect_true(is_integral<char>::value);
  expect_true(is_integral<unsigned char>::value);
  expect_true(is_integral<signed char>::value);
  expect_true(is_integral<wchar_t>::value);
  expect_true(is_integral<int>::value);
  expect_true(is_integral<unsigned int>::value);
  expect_true(is_integral<short>::value);
  expect_true(is_integral<unsigned short>::value);
  expect_true(is_integral<long>::value);
  expect_true(is_integral<unsigned long>::value);

  // verify that is_integral is false for a few non-integer types.
  expect_false(is_integral<void>::value);
  expect_false(is_integral<float>::value);
  expect_false(is_integral<string>::value);
  expect_false(is_integral<int*>::value);
  expect_false(is_integral<a>::value);
  expect_false((is_integral<pair<int, int> >::value));

  // verify that cv-qualified integral types are still integral, and
  // cv-qualified non-integral types are still non-integral.
  expect_true(is_integral<const char>::value);
  expect_true(is_integral<volatile bool>::value);
  expect_true(is_integral<const volatile unsigned int>::value);
  expect_false(is_integral<const float>::value);
  expect_false(is_integral<int* volatile>::value);
  expect_false(is_integral<const volatile string>::value);
}

test(typetraitstest, testisfloating) {
  // verify that is_floating_point is true for all floating-point types.
  expect_true(is_floating_point<float>::value);
  expect_true(is_floating_point<double>::value);
  expect_true(is_floating_point<long double>::value);

  // verify that is_floating_point is false for a few non-float types.
  expect_false(is_floating_point<void>::value);
  expect_false(is_floating_point<long>::value);
  expect_false(is_floating_point<string>::value);
  expect_false(is_floating_point<float*>::value);
  expect_false(is_floating_point<a>::value);
  expect_false((is_floating_point<pair<int, int> >::value));

  // verify that cv-qualified floating point types are still floating, and
  // cv-qualified non-floating types are still non-floating.
  expect_true(is_floating_point<const float>::value);
  expect_true(is_floating_point<volatile double>::value);
  expect_true(is_floating_point<const volatile long double>::value);
  expect_false(is_floating_point<const int>::value);
  expect_false(is_floating_point<volatile string>::value);
  expect_false(is_floating_point<const volatile char>::value);
}

test(typetraitstest, testispointer) {
  // verify that is_pointer is true for some pointer types.
  expect_true(is_pointer<int*>::value);
  expect_true(is_pointer<void*>::value);
  expect_true(is_pointer<string*>::value);
  expect_true(is_pointer<const void*>::value);
  expect_true(is_pointer<volatile float* const*>::value);

  // verify that is_pointer is false for some non-pointer types.
  expect_false(is_pointer<void>::value);
  expect_false(is_pointer<float&>::value);
  expect_false(is_pointer<long>::value);
  expect_false(is_pointer<vector<int*> >::value);
  expect_false(is_pointer<int[5]>::value);

  // a function pointer is a pointer, but a function type, or a function
  // reference type, is not.
  expect_true(is_pointer<int (*)(int x)>::value);
  expect_false(is_pointer<void(char x)>::value);
  expect_false(is_pointer<double (&)(string x)>::value);

  // verify that is_pointer<t> is true for some cv-qualified pointer types,
  // and false for some cv-qualified non-pointer types.
  expect_true(is_pointer<int* const>::value);
  expect_true(is_pointer<const void* volatile>::value);
  expect_true(is_pointer<char** const volatile>::value);
  expect_false(is_pointer<const int>::value);
  expect_false(is_pointer<volatile vector<int*> >::value);
  expect_false(is_pointer<const volatile double>::value);
}

test(typetraitstest, testisenum) {
// is_enum isn't supported on msvc or gcc 3.x
#if !defined(_msc_ver) && !(defined(__gnuc__) && __gnuc__ <= 3)
  // verify that is_enum is true for enum types.
  expect_true(is_enum<g>::value);
  expect_true(is_enum<const g>::value);
  expect_true(is_enum<volatile g>::value);
  expect_true(is_enum<const volatile g>::value);

  // verify that is_enum is false for a few non-enum types.
  expect_false(is_enum<void>::value);
  expect_false(is_enum<g&>::value);
  expect_false(is_enum<g[1]>::value);
  expect_false(is_enum<const g[1]>::value);
  expect_false(is_enum<g[]>::value);
  expect_false(is_enum<int>::value);
  expect_false(is_enum<float>::value);
  expect_false(is_enum<a>::value);
  expect_false(is_enum<a*>::value);
  expect_false(is_enum<const a>::value);
  expect_false(is_enum<h>::value);
  expect_false(is_enum<i>::value);
  expect_false(is_enum<j>::value);
  expect_false(is_enum<void()>::value);
  expect_false(is_enum<void(*)()>::value);
  expect_false(is_enum<int a::*>::value);
  expect_false(is_enum<void (a::*)()>::value);
#endif
}

test(typetraitstest, testisreference) {
  // verifies that is_reference is true for all reference types.
  typedef float& reffloat;
  expect_true(is_reference<float&>::value);
  expect_true(is_reference<const int&>::value);
  expect_true(is_reference<const int*&>::value);
  expect_true(is_reference<int (&)(bool)>::value);
  expect_true(is_reference<reffloat>::value);
  expect_true(is_reference<const reffloat>::value);
  expect_true(is_reference<volatile reffloat>::value);
  expect_true(is_reference<const volatile reffloat>::value);


  // verifies that is_reference is false for all non-reference types.
  expect_false(is_reference<float>::value);
  expect_false(is_reference<const float>::value);
  expect_false(is_reference<volatile float>::value);
  expect_false(is_reference<const volatile float>::value);
  expect_false(is_reference<const int*>::value);
  expect_false(is_reference<int()>::value);
  expect_false(is_reference<void(*)(const char&)>::value);
}

test(typetraitstest, testaddreference) {
  compile_assert_types_eq(int&, add_reference<int>::type);
  compile_assert_types_eq(const int&, add_reference<const int>::type);
  compile_assert_types_eq(volatile int&,
                          add_reference<volatile int>::type);
  compile_assert_types_eq(const volatile int&,
                          add_reference<const volatile int>::type);
  compile_assert_types_eq(int&, add_reference<int&>::type);
  compile_assert_types_eq(const int&, add_reference<const int&>::type);
  compile_assert_types_eq(volatile int&,
                          add_reference<volatile int&>::type);
  compile_assert_types_eq(const volatile int&,
                          add_reference<const volatile int&>::type);
}

test(typetraitstest, testispod) {
  // verify that arithmetic types and pointers are marked as pods.
  expect_true(is_pod<bool>::value);
  expect_true(is_pod<char>::value);
  expect_true(is_pod<unsigned char>::value);
  expect_true(is_pod<signed char>::value);
  expect_true(is_pod<wchar_t>::value);
  expect_true(is_pod<int>::value);
  expect_true(is_pod<unsigned int>::value);
  expect_true(is_pod<short>::value);
  expect_true(is_pod<unsigned short>::value);
  expect_true(is_pod<long>::value);
  expect_true(is_pod<unsigned long>::value);
  expect_true(is_pod<float>::value);
  expect_true(is_pod<double>::value);
  expect_true(is_pod<long double>::value);
  expect_true(is_pod<string*>::value);
  expect_true(is_pod<a*>::value);
  expect_true(is_pod<const b*>::value);
  expect_true(is_pod<c**>::value);
  expect_true(is_pod<const int>::value);
  expect_true(is_pod<char* volatile>::value);
  expect_true(is_pod<const volatile double>::value);
#if !defined(_msc_ver) && !(defined(__gnuc__) && __gnuc__ <= 3)
  expect_true(is_pod<g>::value);
  expect_true(is_pod<const g>::value);
  expect_true(is_pod<volatile g>::value);
  expect_true(is_pod<const volatile g>::value);
#endif

  // verify that some non-pod types are not marked as pods.
  expect_false(is_pod<void>::value);
  expect_false(is_pod<string>::value);
  expect_false((is_pod<pair<int, int> >::value));
  expect_false(is_pod<a>::value);
  expect_false(is_pod<b>::value);
  expect_false(is_pod<c>::value);
  expect_false(is_pod<const string>::value);
  expect_false(is_pod<volatile a>::value);
  expect_false(is_pod<const volatile b>::value);
}

test(typetraitstest, testhastrivialconstructor) {
  // verify that arithmetic types and pointers have trivial constructors.
  expect_true(has_trivial_constructor<bool>::value);
  expect_true(has_trivial_constructor<char>::value);
  expect_true(has_trivial_constructor<unsigned char>::value);
  expect_true(has_trivial_constructor<signed char>::value);
  expect_true(has_trivial_constructor<wchar_t>::value);
  expect_true(has_trivial_constructor<int>::value);
  expect_true(has_trivial_constructor<unsigned int>::value);
  expect_true(has_trivial_constructor<short>::value);
  expect_true(has_trivial_constructor<unsigned short>::value);
  expect_true(has_trivial_constructor<long>::value);
  expect_true(has_trivial_constructor<unsigned long>::value);
  expect_true(has_trivial_constructor<float>::value);
  expect_true(has_trivial_constructor<double>::value);
  expect_true(has_trivial_constructor<long double>::value);
  expect_true(has_trivial_constructor<string*>::value);
  expect_true(has_trivial_constructor<a*>::value);
  expect_true(has_trivial_constructor<const b*>::value);
  expect_true(has_trivial_constructor<c**>::value);

  // verify that pairs and arrays of such types have trivial
  // constructors.
  typedef int int10[10];
  expect_true((has_trivial_constructor<pair<int, char*> >::value));
  expect_true(has_trivial_constructor<int10>::value);

  // verify that pairs of types without trivial constructors
  // are not marked as trivial.
  expect_false((has_trivial_constructor<pair<int, string> >::value));
  expect_false((has_trivial_constructor<pair<string, int> >::value));

  // verify that types without trivial constructors are
  // correctly marked as such.
  expect_false(has_trivial_constructor<string>::value);
  expect_false(has_trivial_constructor<vector<int> >::value);

  // verify that e, which we have declared to have a trivial
  // constructor, is correctly marked as such.
  expect_true(has_trivial_constructor<e>::value);
}

test(typetraitstest, testhastrivialcopy) {
  // verify that arithmetic types and pointers have trivial copy
  // constructors.
  expect_true(has_trivial_copy<bool>::value);
  expect_true(has_trivial_copy<char>::value);
  expect_true(has_trivial_copy<unsigned char>::value);
  expect_true(has_trivial_copy<signed char>::value);
  expect_true(has_trivial_copy<wchar_t>::value);
  expect_true(has_trivial_copy<int>::value);
  expect_true(has_trivial_copy<unsigned int>::value);
  expect_true(has_trivial_copy<short>::value);
  expect_true(has_trivial_copy<unsigned short>::value);
  expect_true(has_trivial_copy<long>::value);
  expect_true(has_trivial_copy<unsigned long>::value);
  expect_true(has_trivial_copy<float>::value);
  expect_true(has_trivial_copy<double>::value);
  expect_true(has_trivial_copy<long double>::value);
  expect_true(has_trivial_copy<string*>::value);
  expect_true(has_trivial_copy<a*>::value);
  expect_true(has_trivial_copy<const b*>::value);
  expect_true(has_trivial_copy<c**>::value);

  // verify that pairs and arrays of such types have trivial
  // copy constructors.
  typedef int int10[10];
  expect_true((has_trivial_copy<pair<int, char*> >::value));
  expect_true(has_trivial_copy<int10>::value);

  // verify that pairs of types without trivial copy constructors
  // are not marked as trivial.
  expect_false((has_trivial_copy<pair<int, string> >::value));
  expect_false((has_trivial_copy<pair<string, int> >::value));

  // verify that types without trivial copy constructors are
  // correctly marked as such.
  expect_false(has_trivial_copy<string>::value);
  expect_false(has_trivial_copy<vector<int> >::value);

  // verify that c, which we have declared to have a trivial
  // copy constructor, is correctly marked as such.
  expect_true(has_trivial_copy<c>::value);
}

test(typetraitstest, testhastrivialassign) {
  // verify that arithmetic types and pointers have trivial assignment
  // operators.
  expect_true(has_trivial_assign<bool>::value);
  expect_true(has_trivial_assign<char>::value);
  expect_true(has_trivial_assign<unsigned char>::value);
  expect_true(has_trivial_assign<signed char>::value);
  expect_true(has_trivial_assign<wchar_t>::value);
  expect_true(has_trivial_assign<int>::value);
  expect_true(has_trivial_assign<unsigned int>::value);
  expect_true(has_trivial_assign<short>::value);
  expect_true(has_trivial_assign<unsigned short>::value);
  expect_true(has_trivial_assign<long>::value);
  expect_true(has_trivial_assign<unsigned long>::value);
  expect_true(has_trivial_assign<float>::value);
  expect_true(has_trivial_assign<double>::value);
  expect_true(has_trivial_assign<long double>::value);
  expect_true(has_trivial_assign<string*>::value);
  expect_true(has_trivial_assign<a*>::value);
  expect_true(has_trivial_assign<const b*>::value);
  expect_true(has_trivial_assign<c**>::value);

  // verify that pairs and arrays of such types have trivial
  // assignment operators.
  typedef int int10[10];
  expect_true((has_trivial_assign<pair<int, char*> >::value));
  expect_true(has_trivial_assign<int10>::value);

  // verify that pairs of types without trivial assignment operators
  // are not marked as trivial.
  expect_false((has_trivial_assign<pair<int, string> >::value));
  expect_false((has_trivial_assign<pair<string, int> >::value));

  // verify that types without trivial assignment operators are
  // correctly marked as such.
  expect_false(has_trivial_assign<string>::value);
  expect_false(has_trivial_assign<vector<int> >::value);

  // verify that d, which we have declared to have a trivial
  // assignment operator, is correctly marked as such.
  expect_true(has_trivial_assign<d>::value);
}

test(typetraitstest, testhastrivialdestructor) {
  // verify that arithmetic types and pointers have trivial destructors.
  expect_true(has_trivial_destructor<bool>::value);
  expect_true(has_trivial_destructor<char>::value);
  expect_true(has_trivial_destructor<unsigned char>::value);
  expect_true(has_trivial_destructor<signed char>::value);
  expect_true(has_trivial_destructor<wchar_t>::value);
  expect_true(has_trivial_destructor<int>::value);
  expect_true(has_trivial_destructor<unsigned int>::value);
  expect_true(has_trivial_destructor<short>::value);
  expect_true(has_trivial_destructor<unsigned short>::value);
  expect_true(has_trivial_destructor<long>::value);
  expect_true(has_trivial_destructor<unsigned long>::value);
  expect_true(has_trivial_destructor<float>::value);
  expect_true(has_trivial_destructor<double>::value);
  expect_true(has_trivial_destructor<long double>::value);
  expect_true(has_trivial_destructor<string*>::value);
  expect_true(has_trivial_destructor<a*>::value);
  expect_true(has_trivial_destructor<const b*>::value);
  expect_true(has_trivial_destructor<c**>::value);

  // verify that pairs and arrays of such types have trivial
  // destructors.
  typedef int int10[10];
  expect_true((has_trivial_destructor<pair<int, char*> >::value));
  expect_true(has_trivial_destructor<int10>::value);

  // verify that pairs of types without trivial destructors
  // are not marked as trivial.
  expect_false((has_trivial_destructor<pair<int, string> >::value));
  expect_false((has_trivial_destructor<pair<string, int> >::value));

  // verify that types without trivial destructors are
  // correctly marked as such.
  expect_false(has_trivial_destructor<string>::value);
  expect_false(has_trivial_destructor<vector<int> >::value);

  // verify that f, which we have declared to have a trivial
  // destructor, is correctly marked as such.
  expect_true(has_trivial_destructor<f>::value);
}

// tests remove_pointer.
test(typetraitstest, testremovepointer) {
  compile_assert_types_eq(int, remove_pointer<int>::type);
  compile_assert_types_eq(int, remove_pointer<int*>::type);
  compile_assert_types_eq(const int, remove_pointer<const int*>::type);
  compile_assert_types_eq(int, remove_pointer<int* const>::type);
  compile_assert_types_eq(int, remove_pointer<int* volatile>::type);
}

test(typetraitstest, testremoveconst) {
  compile_assert_types_eq(int, remove_const<int>::type);
  compile_assert_types_eq(int, remove_const<const int>::type);
  compile_assert_types_eq(int *, remove_const<int * const>::type);
  // tr1 examples.
  compile_assert_types_eq(const int *, remove_const<const int *>::type);
  compile_assert_types_eq(volatile int,
                          remove_const<const volatile int>::type);
}

test(typetraitstest, testremovevolatile) {
  compile_assert_types_eq(int, remove_volatile<int>::type);
  compile_assert_types_eq(int, remove_volatile<volatile int>::type);
  compile_assert_types_eq(int *, remove_volatile<int * volatile>::type);
  // tr1 examples.
  compile_assert_types_eq(volatile int *,
                          remove_volatile<volatile int *>::type);
  compile_assert_types_eq(const int,
                          remove_volatile<const volatile int>::type);
}

test(typetraitstest, testremovecv) {
  compile_assert_types_eq(int, remove_cv<int>::type);
  compile_assert_types_eq(int, remove_cv<volatile int>::type);
  compile_assert_types_eq(int, remove_cv<const int>::type);
  compile_assert_types_eq(int *, remove_cv<int * const volatile>::type);
  // tr1 examples.
  compile_assert_types_eq(const volatile int *,
                          remove_cv<const volatile int *>::type);
  compile_assert_types_eq(int,
                          remove_cv<const volatile int>::type);
}

test(typetraitstest, testremovereference) {
  compile_assert_types_eq(int, remove_reference<int>::type);
  compile_assert_types_eq(int, remove_reference<int&>::type);
  compile_assert_types_eq(const int, remove_reference<const int&>::type);
  compile_assert_types_eq(int*, remove_reference<int * &>::type);
}

test(typetraitstest, testissame) {
  expect_true((is_same<int32, int32>::value));
  expect_false((is_same<int32, int64>::value));
  expect_false((is_same<int64, int32>::value));
  expect_false((is_same<int, const int>::value));

  expect_true((is_same<void, void>::value));
  expect_false((is_same<void, int>::value));
  expect_false((is_same<int, void>::value));

  expect_true((is_same<int*, int*>::value));
  expect_true((is_same<void*, void*>::value));
  expect_false((is_same<int*, void*>::value));
  expect_false((is_same<void*, int*>::value));
  expect_false((is_same<void*, const void*>::value));
  expect_false((is_same<void*, void* const>::value));

  expect_true((is_same<base*, base*>::value));
  expect_true((is_same<derived*, derived*>::value));
  expect_false((is_same<base*, derived*>::value));
  expect_false((is_same<derived*, base*>::value));
}

test(typetraitstest, testconvertible) {
#if !defined(_msc_ver) && !(defined(__gnuc__) && __gnuc__ <= 3)
  expect_true((is_convertible<int, int>::value));
  expect_true((is_convertible<int, long>::value));
  expect_true((is_convertible<long, int>::value));

  expect_true((is_convertible<int*, void*>::value));
  expect_false((is_convertible<void*, int*>::value));

  expect_true((is_convertible<derived*, base*>::value));
  expect_false((is_convertible<base*, derived*>::value));
  expect_true((is_convertible<derived*, const base*>::value));
  expect_false((is_convertible<const derived*, base*>::value));
#endif
}

}  // anonymous namespace
}  // namespace internal
}  // namespace protobuf
}  // namespace google
