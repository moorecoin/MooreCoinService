//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    the  software is provided "as is" and the author disclaims all warranties
    with  regard  to  this  software  including  all  implied  warranties  of
    merchantability  and  fitness. in no event shall the author be liable for
    any  special ,  direct, indirect, or consequential damages or any damages
    whatsoever  resulting  from  loss  of use, data or profits, whether in an
    action  of  contract, negligence or other tortious action, arising out of
    or in connection with the use or performance of this software.
*/
//==============================================================================

#ifndef beast_cxx14_type_traits_h_included
#define beast_cxx14_type_traits_h_included

#include <beast/cxx14/config.h>

#include <tuple>
#include <type_traits>
#include <utility>

namespace std {

// ideas from howard hinnant
//
// specializations of is_constructible for pair and tuple which
// work around an apparent defect in the standard that causes well
// formed expressions involving pairs or tuples of non default-constructible
// types to generate compile errors.
//
template <class t, class u>
struct is_constructible <pair <t, u>>
    : integral_constant <bool,
        is_default_constructible <t>::value &&
        is_default_constructible <u>::value>
{
};

namespace detail {

template <bool...>
struct compile_time_all;

template <>
struct compile_time_all <>
{
    static const bool value = true;
};

template <bool arg0, bool ... argn>
struct compile_time_all <arg0, argn...>
{
    static const bool value =
        arg0 && compile_time_all <argn...>::value;
};

}

template <class ...t>
struct is_constructible <tuple <t...>>
    : integral_constant <bool,
        detail::compile_time_all <
            is_default_constructible <t>::value...>::value>
{
};

//------------------------------------------------------------------------------

#if ! beast_no_cxx14_compatibility

// from http://llvm.org/svn/llvm-project/libcxx/trunk/include/type_traits

// const-volatile modifications:
template <class t>
using remove_const_t    = typename remove_const<t>::type;  // c++14
template <class t>
using remove_volatile_t = typename remove_volatile<t>::type;  // c++14
template <class t>
using remove_cv_t       = typename remove_cv<t>::type;  // c++14
template <class t>
using add_const_t       = typename add_const<t>::type;  // c++14
template <class t>
using add_volatile_t    = typename add_volatile<t>::type;  // c++14
template <class t>
using add_cv_t          = typename add_cv<t>::type;  // c++14
  
// reference modifications:
template <class t>
using remove_reference_t     = typename remove_reference<t>::type;  // c++14
template <class t>
using add_lvalue_reference_t = typename add_lvalue_reference<t>::type;  // c++14
template <class t>
using add_rvalue_reference_t = typename add_rvalue_reference<t>::type;  // c++14
  
// sign modifications:
template <class t>
using make_signed_t   = typename make_signed<t>::type;  // c++14
template <class t>
using make_unsigned_t = typename make_unsigned<t>::type;  // c++14
  
// array modifications:
template <class t>
using remove_extent_t      = typename remove_extent<t>::type;  // c++14
template <class t>
using remove_all_extents_t = typename remove_all_extents<t>::type;  // c++14

// pointer modifications:
template <class t>
using remove_pointer_t = typename remove_pointer<t>::type;  // c++14
template <class t>
using add_pointer_t    = typename add_pointer<t>::type;  // c++14

// other transformations:

#if 0
// this is not easy to implement in c++11
template <size_t len, std::size_t align=std::alignment_of<max_align_t>::value>
using aligned_storage_t = typename aligned_storage<len,align>::type;  // c++14
template <std::size_t len, class... types>
using aligned_union_t   = typename aligned_union<len,types...>::type;  // c++14
#endif

template <class t>
using decay_t           = typename decay<t>::type;  // c++14
template <bool b, class t=void>
using enable_if_t       = typename enable_if<b,t>::type;  // c++14
template <bool b, class t, class f>
using conditional_t     = typename conditional<b,t,f>::type;  // c++14
template <class... t>
using common_type_t     = typename common_type<t...>::type;  // c++14
template <class t>
using underlying_type_t = typename underlying_type<t>::type;  // c++14
template <class f, class... argtypes>
using result_of_t       = typename result_of<f(argtypes...)>::type;  // c++14

#endif

}

#endif
