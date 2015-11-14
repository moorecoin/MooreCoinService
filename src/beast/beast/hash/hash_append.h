//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, howard hinnant <howard.hinnant@gmail.com>,
        vinnie falco <vinnie.falco@gmail.com

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

#ifndef beast_hash_hash_append_h_included
#define beast_hash_hash_append_h_included

#include <beast/utility/meta.h>
#include <beast/utility/noexcept.h>
#if beast_use_boost_features
#include <boost/shared_ptr.hpp>
#endif
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <beast/cxx14/type_traits.h> // <type_traits>
#include <beast/cxx14/utility.h> // <utility>
#include <vector>

// set to 1 to disable variadic hash_append for tuple. when set, overloads
// will be manually provided for tuples up to 10-arity. this also causes
// is_contiguously_hashable<> to always return false for tuples.
//
#ifndef beast_no_tuple_variadics
# ifdef _msc_ver
#  define beast_no_tuple_variadics 1
#  ifndef beast_variadic_max
#   ifdef _variadic_max
#    define beast_variadic_max _variadic_max
#   else
#    define beast_variadic_max 10
#   endif
#  endif
# else
#  define beast_no_tuple_variadics 0
# endif
#endif

// set to 1 if std::pair fails the trait test on a platform.
#ifndef beast_no_is_contiguous_hashable_pair
#define beast_no_is_contiguous_hashable_pair 0
#endif

// set to 1 if std::tuple fails the trait test on a platform.
#ifndef beast_no_is_contiguous_hashable_tuple
# ifdef _msc_ver
#  define beast_no_is_contiguous_hashable_tuple 1
# else
#  define beast_no_is_contiguous_hashable_tuple 0
# endif
#endif

namespace beast {

/** metafunction returning `true` if the type can be hashed in one call.

    for `is_contiguously_hashable<t>::value` to be true, then for every
    combination of possible values of `t` held in `x` and `y`,
    if `x == y`, then it must be true that `memcmp(&x, &y, sizeof(t))`
    return 0; i.e. that `x` and `y` are represented by the same bit pattern.
   
    for example:  a two's complement `int` should be contiguously hashable.
    every bit pattern produces a unique value that does not compare equal to
    any other bit pattern's value.  a ieee floating point should not be
    contiguously hashable because -0. and 0. have different bit patterns,
    though they compare equal.
*/
/** @{ */
// scalars
template <class t>
struct is_contiguously_hashable
    : public std::integral_constant <bool,
        std::is_integral<t>::value || 
        std::is_enum<t>::value     ||
        std::is_pointer<t>::value>
{
};

// if this fails, something is wrong with the trait
static_assert (is_contiguously_hashable<int>::value, "");

// pair
template <class t, class u>
struct is_contiguously_hashable <std::pair<t, u>>
    : public std::integral_constant <bool,
        is_contiguously_hashable<t>::value && 
        is_contiguously_hashable<u>::value &&
        sizeof(t) + sizeof(u) == sizeof(std::pair<t, u>)>
{
};

#if ! beast_no_is_contiguous_hashable_pair
static_assert (is_contiguously_hashable <std::pair <
    unsigned long long, long long>>::value, "");
#endif

#if ! beast_no_tuple_variadics
// std::tuple
template <class ...t>
struct is_contiguously_hashable <std::tuple<t...>>
    : public std::integral_constant <bool,
        static_and <is_contiguously_hashable <t>::value...>::value && 
        static_sum <sizeof(t)...>::value == sizeof(std::tuple<t...>)>
{
};
#endif

// std::array
template <class t, std::size_t n>
struct is_contiguously_hashable <std::array<t, n>>
    : public std::integral_constant <bool,
        is_contiguously_hashable<t>::value && 
        sizeof(t)*n == sizeof(std::array<t, n>)>
{
};

static_assert (is_contiguously_hashable <std::array<char, 3>>::value, "");

#if ! beast_no_is_contiguous_hashable_tuple
static_assert (is_contiguously_hashable <
    std::tuple <char, char, short>>::value, "");
#endif

/** @} */

//------------------------------------------------------------------------------

/** logically concatenate input data to a `hasher`.

    hasher requirements:

        `x` is the type `hasher`
        `h` is a value of type `x`
        `p` is a value convertible to `void const*`
        `n` is a value of type `std::size_t`, greater than zero

        expression:
            `h.append (p, n);`
        throws:
            never
        effect:
            adds the input data to the hasher state.

        expression:
            `static_cast<std::size_t>(j)`
        throws:
            never
        effect:
            returns the reslting hash of all the input data.
*/  
/** @{ */

// scalars

template <class hasher, class t>
inline
typename std::enable_if
<
    is_contiguously_hashable<t>::value
>::type
hash_append (hasher& h, t const& t) noexcept
{
    h.append (&t, sizeof(t));
}

template <class hasher, class t>
inline
typename std::enable_if
<
    std::is_floating_point<t>::value
>::type
hash_append (hasher& h, t t) noexcept
{
    // hash both signed zeroes identically
    if (t == 0)
        t = 0;
    h.append (&t, sizeof(t));
}

// arrays

template <class hasher, class t, std::size_t n>
inline
typename std::enable_if
<
    !is_contiguously_hashable<t>::value
>::type
hash_append (hasher& h, t (&a)[n]) noexcept
{
    for (auto const& t : a)
        hash_append (h, t);
}

template <class hasher, class t, std::size_t n>
inline
typename std::enable_if
<
    is_contiguously_hashable<t>::value
>::type
hash_append (hasher& h, t (&a)[n]) noexcept
{
    h.append (a, n*sizeof(t));
}

// nullptr_t

template <class hasher>
inline
void
hash_append (hasher& h, std::nullptr_t p) noexcept
{
    h.append (&p, sizeof(p));
}

// strings

template <class hasher, class chart, class traits, class alloc>
inline
void
hash_append (hasher& h,
    std::basic_string <chart, traits, alloc> const& s) noexcept
{
    h.append (s.data (), (s.size()+1)*sizeof(chart));
}

//------------------------------------------------------------------------------

// forward declare hash_append for all containers. this is required so that
// argument dependent lookup works recursively (i.e. containers of containers).

template <class hasher, class t, class u>
typename std::enable_if
<
    !is_contiguously_hashable<std::pair<t, u>>::value 
>::type
hash_append (hasher& h, std::pair<t, u> const& p) noexcept;

template <class hasher, class t, class alloc>
typename std::enable_if
<
    !is_contiguously_hashable<t>::value
>::type
hash_append (hasher& h, std::vector<t, alloc> const& v) noexcept;

template <class hasher, class t, class alloc>
typename std::enable_if
<
    is_contiguously_hashable<t>::value
>::type
hash_append (hasher& h, std::vector<t, alloc> const& v) noexcept;

template <class hasher, class t, std::size_t n>
typename std::enable_if
<
    !is_contiguously_hashable<std::array<t, n>>::value
>::type
hash_append (hasher& h, std::array<t, n> const& a) noexcept;

// std::tuple

template <class hasher>
inline
void
hash_append (hasher& h, std::tuple<> const& t) noexcept;

#if beast_no_tuple_variadics

#if beast_variadic_max >= 1
template <class hasher, class t1>
inline
void
hash_append (hasher& h, std::tuple <t1> const& t) noexcept;
#endif

#if beast_variadic_max >= 2
template <class hasher, class t1, class t2>
inline
void
hash_append (hasher& h, std::tuple <t1, t2> const& t) noexcept;
#endif

#if beast_variadic_max >= 3
template <class hasher, class t1, class t2, class t3>
inline
void
hash_append (hasher& h, std::tuple <t1, t2, t3> const& t) noexcept;
#endif

#if beast_variadic_max >= 4
template <class hasher, class t1, class t2, class t3, class t4>
inline
void
hash_append (hasher& h, std::tuple <t1, t2, t3, t4> const& t) noexcept;
#endif

#if beast_variadic_max >= 5
template <class hasher, class t1, class t2, class t3, class t4, class t5>
inline
void
hash_append (hasher& h, std::tuple <t1, t2, t3, t4, t5> const& t) noexcept;
#endif

#if beast_variadic_max >= 6
template <class hasher, class t1, class t2, class t3, class t4, class t5,
                        class t6>
inline
void
hash_append (hasher& h, std::tuple <
    t1, t2, t3, t4, t5, t6> const& t) noexcept;
#endif

#if beast_variadic_max >= 7
template <class hasher, class t1, class t2, class t3, class t4, class t5,
                        class t6, class t7>
inline
void
hash_append (hasher& h, std::tuple <
    t1, t2, t3, t4, t5, t6, t7> const& t) noexcept;
#endif

#if beast_variadic_max >= 8
template <class hasher, class t1, class t2, class t3, class t4, class t5,
                        class t6, class t7, class t8>
inline
void
hash_append (hasher& h, std::tuple <
    t1, t2, t3, t4, t5, t6, t7, t8> const& t) noexcept;
#endif

#if beast_variadic_max >= 9
template <class hasher, class t1, class t2, class t3, class t4, class t5,
                        class t6, class t7, class t8, class t9>
inline
void
hash_append (hasher& h, std::tuple <
    t1, t2, t3, t4, t5, t6, t7, t8, t9> const& t) noexcept;
#endif

#if beast_variadic_max >= 10
template <class hasher, class t1, class t2, class t3, class t4, class t5,
                        class t6, class t7, class t8, class t9, class t10>
inline
void
hash_append (hasher& h, std::tuple <
    t1, t2, t3, t4, t5, t6, t7, t8, t9, t10> const& t) noexcept;
#endif

#endif // beast_no_tuple_variadics

//------------------------------------------------------------------------------

namespace detail {

template <class hasher, class t>
inline
int
hash_one (hasher& h, t const& t) noexcept
{
    hash_append (h, t);
    return 0;
}

} // detail

//------------------------------------------------------------------------------

// std::tuple

template <class hasher>
inline
void
hash_append (hasher& h, std::tuple<> const& t) noexcept
{
    hash_append (h, nullptr);
}

#if beast_no_tuple_variadics

#if beast_variadic_max >= 1
template <class hasher, class t1>
inline
void
hash_append (hasher& h, std::tuple <t1> const& t) noexcept
{
    hash_append (h, std::get<0>(t));
}
#endif

#if beast_variadic_max >= 2
template <class hasher, class t1, class t2>
inline
void
hash_append (hasher& h, std::tuple <t1, t2> const& t) noexcept
{
    hash_append (h, std::get<0>(t));
    hash_append (h, std::get<1>(t));
}
#endif

#if beast_variadic_max >= 3
template <class hasher, class t1, class t2, class t3>
inline
void
hash_append (hasher& h, std::tuple <t1, t2, t3> const& t) noexcept
{
    hash_append (h, std::get<0>(t));
    hash_append (h, std::get<1>(t));
    hash_append (h, std::get<2>(t));
}
#endif

#if beast_variadic_max >= 4
template <class hasher, class t1, class t2, class t3, class t4>
inline
void
hash_append (hasher& h, std::tuple <t1, t2, t3, t4> const& t) noexcept
{
    hash_append (h, std::get<0>(t));
    hash_append (h, std::get<1>(t));
    hash_append (h, std::get<2>(t));
    hash_append (h, std::get<3>(t));
}
#endif

#if beast_variadic_max >= 5
template <class hasher, class t1, class t2, class t3, class t4, class t5>
inline
void
hash_append (hasher& h, std::tuple <
    t1, t2, t3, t4, t5> const& t) noexcept
{
    hash_append (h, std::get<0>(t));
    hash_append (h, std::get<1>(t));
    hash_append (h, std::get<2>(t));
    hash_append (h, std::get<3>(t));
    hash_append (h, std::get<4>(t));
}
#endif

#if beast_variadic_max >= 6
template <class hasher, class t1, class t2, class t3, class t4, class t5,
                        class t6>
inline
void
hash_append (hasher& h, std::tuple <
    t1, t2, t3, t4, t5, t6> const& t) noexcept
{
    hash_append (h, std::get<0>(t));
    hash_append (h, std::get<1>(t));
    hash_append (h, std::get<2>(t));
    hash_append (h, std::get<3>(t));
    hash_append (h, std::get<4>(t));
    hash_append (h, std::get<5>(t));
}
#endif

#if beast_variadic_max >= 7
template <class hasher, class t1, class t2, class t3, class t4, class t5,
                        class t6, class t7>
inline
void
hash_append (hasher& h, std::tuple <
    t1, t2, t3, t4, t5, t6, t7> const& t) noexcept
{
    hash_append (h, std::get<0>(t));
    hash_append (h, std::get<1>(t));
    hash_append (h, std::get<2>(t));
    hash_append (h, std::get<3>(t));
    hash_append (h, std::get<4>(t));
    hash_append (h, std::get<5>(t));
    hash_append (h, std::get<6>(t));
}
#endif

#if beast_variadic_max >= 8
template <class hasher, class t1, class t2, class t3, class t4, class t5,
                        class t6, class t7, class t8>
inline
void
hash_append (hasher& h, std::tuple <
    t1, t2, t3, t4, t5, t6, t7, t8> const& t) noexcept
{
    hash_append (h, std::get<0>(t));
    hash_append (h, std::get<1>(t));
    hash_append (h, std::get<2>(t));
    hash_append (h, std::get<3>(t));
    hash_append (h, std::get<4>(t));
    hash_append (h, std::get<5>(t));
    hash_append (h, std::get<6>(t));
    hash_append (h, std::get<7>(t));
}
#endif

#if beast_variadic_max >= 9
template <class hasher, class t1, class t2, class t3, class t4, class t5,
                        class t6, class t7, class t8, class t9>
inline
void
hash_append (hasher& h, std::tuple <
    t1, t2, t3, t4, t5, t6, t7, t8, t9> const& t) noexcept
{
    hash_append (h, std::get<0>(t));
    hash_append (h, std::get<1>(t));
    hash_append (h, std::get<2>(t));
    hash_append (h, std::get<3>(t));
    hash_append (h, std::get<4>(t));
    hash_append (h, std::get<5>(t));
    hash_append (h, std::get<6>(t));
    hash_append (h, std::get<7>(t));
    hash_append (h, std::get<8>(t));
}
#endif

#if beast_variadic_max >= 10
template <class hasher, class t1, class t2, class t3, class t4, class t5,
                        class t6, class t7, class t8, class t9, class t10>
inline
void
hash_append (hasher& h, std::tuple <
    t1, t2, t3, t4, t5, t6, t7, t8, t9, t10> const& t) noexcept
{
    hash_append (h, std::get<0>(t));
    hash_append (h, std::get<1>(t));
    hash_append (h, std::get<2>(t));
    hash_append (h, std::get<3>(t));
    hash_append (h, std::get<4>(t));
    hash_append (h, std::get<5>(t));
    hash_append (h, std::get<6>(t));
    hash_append (h, std::get<7>(t));
    hash_append (h, std::get<8>(t));
    hash_append (h, std::get<9>(t));
}
#endif

#else // beast_no_tuple_variadics

namespace detail {

template <class hasher, class ...t, std::size_t ...i>
inline
void
tuple_hash (hasher& h, std::tuple<t...> const& t,
    std::index_sequence<i...>) noexcept
{
    struct for_each_item {
        for_each_item (...) { }
    };
    for_each_item (hash_one(h, std::get<i>(t))...);
}

} // detail

template <class hasher, class ...t>
inline
typename std::enable_if
<
    !is_contiguously_hashable<std::tuple<t...>>::value
>::type
hash_append (hasher& h, std::tuple<t...> const& t) noexcept
{
    detail::tuple_hash(h, t, std::index_sequence_for<t...>{});
}

#endif // beast_no_tuple_variadics

// pair

template <class hasher, class t, class u>
inline
typename std::enable_if
<
    !is_contiguously_hashable<std::pair<t, u>>::value
>::type
hash_append (hasher& h, std::pair<t, u> const& p) noexcept
{
    hash_append (h, p.first);
    hash_append (h, p.second);
}

// vector

template <class hasher, class t, class alloc>
inline
typename std::enable_if
<
    !is_contiguously_hashable<t>::value
>::type
hash_append (hasher& h, std::vector<t, alloc> const& v) noexcept
{
    for (auto const& t : v)
        hash_append (h, t);
}

template <class hasher, class t, class alloc>
inline
typename std::enable_if
<
    is_contiguously_hashable<t>::value
>::type
hash_append (hasher& h, std::vector<t, alloc> const& v) noexcept
{
    h.append (v.data(), v.size()*sizeof(t));
}

// shared_ptr

template <class hasher, class t>
inline
void
hash_append (hasher& h, std::shared_ptr<t> const& p) noexcept
{
    hash_append(h, p.get());
}

#if beast_use_boost_features
template <class hasher, class t>
inline
void
hash_append (hasher& h, boost::shared_ptr<t> const& p) noexcept
{
    hash_append(h, p.get());
}
#endif

// variadic hash_append

template <class hasher, class t0, class t1, class ...t>
inline
void
hash_append (hasher& h, t0 const& t0, t1 const& t1, t const& ...t) noexcept
{
    hash_append (h, t0);
    hash_append (h, t1, t...);
}

} // beast

#endif
