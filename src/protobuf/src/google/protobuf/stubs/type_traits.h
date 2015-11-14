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
//
// this code is compiled directly on many platforms, including client
// platforms like windows, mac, and embedded systems.  before making
// any changes here, make sure that you're not breaking any platforms.
//
// define a small subset of tr1 type traits. the traits we define are:
//   is_integral
//   is_floating_point
//   is_pointer
//   is_enum
//   is_reference
//   is_pod
//   has_trivial_constructor
//   has_trivial_copy
//   has_trivial_assign
//   has_trivial_destructor
//   remove_const
//   remove_volatile
//   remove_cv
//   remove_reference
//   add_reference
//   remove_pointer
//   is_same
//   is_convertible
// we can add more type traits as required.

#ifndef google_protobuf_type_traits_h_
#define google_protobuf_type_traits_h_

#include <utility>                  // for pair

#include <google/protobuf/stubs/template_util.h>  // for true_type and false_type

namespace google {
namespace protobuf {
namespace internal {

template <class t> struct is_integral;
template <class t> struct is_floating_point;
template <class t> struct is_pointer;
// msvc can't compile this correctly, and neither can gcc 3.3.5 (at least)
#if !defined(_msc_ver) && !(defined(__gnuc__) && __gnuc__ <= 3)
// is_enum uses is_convertible, which is not available on msvc.
template <class t> struct is_enum;
#endif
template <class t> struct is_reference;
template <class t> struct is_pod;
template <class t> struct has_trivial_constructor;
template <class t> struct has_trivial_copy;
template <class t> struct has_trivial_assign;
template <class t> struct has_trivial_destructor;
template <class t> struct remove_const;
template <class t> struct remove_volatile;
template <class t> struct remove_cv;
template <class t> struct remove_reference;
template <class t> struct add_reference;
template <class t> struct remove_pointer;
template <class t, class u> struct is_same;
#if !defined(_msc_ver) && !(defined(__gnuc__) && __gnuc__ <= 3)
template <class from, class to> struct is_convertible;
#endif

// is_integral is false except for the built-in integer types. a
// cv-qualified type is integral if and only if the underlying type is.
template <class t> struct is_integral : false_type { };
template<> struct is_integral<bool> : true_type { };
template<> struct is_integral<char> : true_type { };
template<> struct is_integral<unsigned char> : true_type { };
template<> struct is_integral<signed char> : true_type { };
#if defined(_msc_ver)
// wchar_t is not by default a distinct type from unsigned short in
// microsoft c.
// see http://msdn2.microsoft.com/en-us/library/dh8che7s(vs.80).aspx
template<> struct is_integral<__wchar_t> : true_type { };
#else
template<> struct is_integral<wchar_t> : true_type { };
#endif
template<> struct is_integral<short> : true_type { };
template<> struct is_integral<unsigned short> : true_type { };
template<> struct is_integral<int> : true_type { };
template<> struct is_integral<unsigned int> : true_type { };
template<> struct is_integral<long> : true_type { };
template<> struct is_integral<unsigned long> : true_type { };
#ifdef have_long_long
template<> struct is_integral<long long> : true_type { };
template<> struct is_integral<unsigned long long> : true_type { };
#endif
template <class t> struct is_integral<const t> : is_integral<t> { };
template <class t> struct is_integral<volatile t> : is_integral<t> { };
template <class t> struct is_integral<const volatile t> : is_integral<t> { };

// is_floating_point is false except for the built-in floating-point types.
// a cv-qualified type is integral if and only if the underlying type is.
template <class t> struct is_floating_point : false_type { };
template<> struct is_floating_point<float> : true_type { };
template<> struct is_floating_point<double> : true_type { };
template<> struct is_floating_point<long double> : true_type { };
template <class t> struct is_floating_point<const t>
    : is_floating_point<t> { };
template <class t> struct is_floating_point<volatile t>
    : is_floating_point<t> { };
template <class t> struct is_floating_point<const volatile t>
    : is_floating_point<t> { };

// is_pointer is false except for pointer types. a cv-qualified type (e.g.
// "int* const", as opposed to "int const*") is cv-qualified if and only if
// the underlying type is.
template <class t> struct is_pointer : false_type { };
template <class t> struct is_pointer<t*> : true_type { };
template <class t> struct is_pointer<const t> : is_pointer<t> { };
template <class t> struct is_pointer<volatile t> : is_pointer<t> { };
template <class t> struct is_pointer<const volatile t> : is_pointer<t> { };

#if !defined(_msc_ver) && !(defined(__gnuc__) && __gnuc__ <= 3)

namespace internal {

template <class t> struct is_class_or_union {
  template <class u> static small_ tester(void (u::*)());
  template <class u> static big_ tester(...);
  static const bool value = sizeof(tester<t>(0)) == sizeof(small_);
};

// is_convertible chokes if the first argument is an array. that's why
// we use add_reference here.
template <bool notunum, class t> struct is_enum_impl
    : is_convertible<typename add_reference<t>::type, int> { };

template <class t> struct is_enum_impl<true, t> : false_type { };

}  // namespace internal

// specified by tr1 [4.5.1] primary type categories.

// implementation note:
//
// each type is either void, integral, floating point, array, pointer,
// reference, member object pointer, member function pointer, enum,
// union or class. out of these, only integral, floating point, reference,
// class and enum types are potentially convertible to int. therefore,
// if a type is not a reference, integral, floating point or class and
// is convertible to int, it's a enum. adding cv-qualification to a type
// does not change whether it's an enum.
//
// is-convertible-to-int check is done only if all other checks pass,
// because it can't be used with some types (e.g. void or classes with
// inaccessible conversion operators).
template <class t> struct is_enum
    : internal::is_enum_impl<
          is_same<t, void>::value ||
              is_integral<t>::value ||
              is_floating_point<t>::value ||
              is_reference<t>::value ||
              internal::is_class_or_union<t>::value,
          t> { };

template <class t> struct is_enum<const t> : is_enum<t> { };
template <class t> struct is_enum<volatile t> : is_enum<t> { };
template <class t> struct is_enum<const volatile t> : is_enum<t> { };

#endif

// is_reference is false except for reference types.
template<typename t> struct is_reference : false_type {};
template<typename t> struct is_reference<t&> : true_type {};


// we can't get is_pod right without compiler help, so fail conservatively.
// we will assume it's false except for arithmetic types, enumerations,
// pointers and cv-qualified versions thereof. note that std::pair<t,u>
// is not a pod even if t and u are pods.
template <class t> struct is_pod
 : integral_constant<bool, (is_integral<t>::value ||
                            is_floating_point<t>::value ||
#if !defined(_msc_ver) && !(defined(__gnuc__) && __gnuc__ <= 3)
                            // is_enum is not available on msvc.
                            is_enum<t>::value ||
#endif
                            is_pointer<t>::value)> { };
template <class t> struct is_pod<const t> : is_pod<t> { };
template <class t> struct is_pod<volatile t> : is_pod<t> { };
template <class t> struct is_pod<const volatile t> : is_pod<t> { };


// we can't get has_trivial_constructor right without compiler help, so
// fail conservatively. we will assume it's false except for: (1) types
// for which is_pod is true. (2) std::pair of types with trivial
// constructors. (3) array of a type with a trivial constructor.
// (4) const versions thereof.
template <class t> struct has_trivial_constructor : is_pod<t> { };
template <class t, class u> struct has_trivial_constructor<std::pair<t, u> >
  : integral_constant<bool,
                      (has_trivial_constructor<t>::value &&
                       has_trivial_constructor<u>::value)> { };
template <class a, int n> struct has_trivial_constructor<a[n]>
  : has_trivial_constructor<a> { };
template <class t> struct has_trivial_constructor<const t>
  : has_trivial_constructor<t> { };

// we can't get has_trivial_copy right without compiler help, so fail
// conservatively. we will assume it's false except for: (1) types
// for which is_pod is true. (2) std::pair of types with trivial copy
// constructors. (3) array of a type with a trivial copy constructor.
// (4) const versions thereof.
template <class t> struct has_trivial_copy : is_pod<t> { };
template <class t, class u> struct has_trivial_copy<std::pair<t, u> >
  : integral_constant<bool,
                      (has_trivial_copy<t>::value &&
                       has_trivial_copy<u>::value)> { };
template <class a, int n> struct has_trivial_copy<a[n]>
  : has_trivial_copy<a> { };
template <class t> struct has_trivial_copy<const t> : has_trivial_copy<t> { };

// we can't get has_trivial_assign right without compiler help, so fail
// conservatively. we will assume it's false except for: (1) types
// for which is_pod is true. (2) std::pair of types with trivial copy
// constructors. (3) array of a type with a trivial assign constructor.
template <class t> struct has_trivial_assign : is_pod<t> { };
template <class t, class u> struct has_trivial_assign<std::pair<t, u> >
  : integral_constant<bool,
                      (has_trivial_assign<t>::value &&
                       has_trivial_assign<u>::value)> { };
template <class a, int n> struct has_trivial_assign<a[n]>
  : has_trivial_assign<a> { };

// we can't get has_trivial_destructor right without compiler help, so
// fail conservatively. we will assume it's false except for: (1) types
// for which is_pod is true. (2) std::pair of types with trivial
// destructors. (3) array of a type with a trivial destructor.
// (4) const versions thereof.
template <class t> struct has_trivial_destructor : is_pod<t> { };
template <class t, class u> struct has_trivial_destructor<std::pair<t, u> >
  : integral_constant<bool,
                      (has_trivial_destructor<t>::value &&
                       has_trivial_destructor<u>::value)> { };
template <class a, int n> struct has_trivial_destructor<a[n]>
  : has_trivial_destructor<a> { };
template <class t> struct has_trivial_destructor<const t>
  : has_trivial_destructor<t> { };

// specified by tr1 [4.7.1]
template<typename t> struct remove_const { typedef t type; };
template<typename t> struct remove_const<t const> { typedef t type; };
template<typename t> struct remove_volatile { typedef t type; };
template<typename t> struct remove_volatile<t volatile> { typedef t type; };
template<typename t> struct remove_cv {
  typedef typename remove_const<typename remove_volatile<t>::type>::type type;
};


// specified by tr1 [4.7.2] reference modifications.
template<typename t> struct remove_reference { typedef t type; };
template<typename t> struct remove_reference<t&> { typedef t type; };

template <typename t> struct add_reference { typedef t& type; };
template <typename t> struct add_reference<t&> { typedef t& type; };

// specified by tr1 [4.7.4] pointer modifications.
template<typename t> struct remove_pointer { typedef t type; };
template<typename t> struct remove_pointer<t*> { typedef t type; };
template<typename t> struct remove_pointer<t* const> { typedef t type; };
template<typename t> struct remove_pointer<t* volatile> { typedef t type; };
template<typename t> struct remove_pointer<t* const volatile> {
  typedef t type; };

// specified by tr1 [4.6] relationships between types
template<typename t, typename u> struct is_same : public false_type { };
template<typename t> struct is_same<t, t> : public true_type { };

// specified by tr1 [4.6] relationships between types
#if !defined(_msc_ver) && !(defined(__gnuc__) && __gnuc__ <= 3)
namespace internal {

// this class is an implementation detail for is_convertible, and you
// don't need to know how it works to use is_convertible. for those
// who care: we declare two different functions, one whose argument is
// of type to and one with a variadic argument list. we give them
// return types of different size, so we can use sizeof to trick the
// compiler into telling us which function it would have chosen if we
// had called it with an argument of type from.  see alexandrescu's
// _modern c++ design_ for more details on this sort of trick.

template <typename from, typename to>
struct converthelper {
  static small_ test(to);
  static big_ test(...);
  static from create();
};
}  // namespace internal

// inherits from true_type if from is convertible to to, false_type otherwise.
template <typename from, typename to>
struct is_convertible
    : integral_constant<bool,
                        sizeof(internal::converthelper<from, to>::test(
                                  internal::converthelper<from, to>::create()))
                        == sizeof(small_)> {
};
#endif

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_type_traits_h_
