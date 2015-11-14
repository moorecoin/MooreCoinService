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

#ifndef beast_cxx14_utility_h_included
#define beast_cxx14_utility_h_included

#include <beast/cxx14/config.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace std {

template <class t, t... ints>
struct integer_sequence
{
    typedef t value_type;
    static_assert (is_integral<t>::value,
        "std::integer_sequence can only be instantiated with an integral type" );

    static const size_t static_size = sizeof...(ints);

    static /* constexpr */ size_t size() /* noexcept */
    {
        return sizeof...(ints);
    }
};

template <size_t... ints>
using index_sequence = integer_sequence <size_t, ints...>;

namespace detail {

// this workaround is needed for msvc broken sizeof...
template <class... args>
struct sizeof_workaround
{
    static size_t const size = sizeof... (args);
};

} // detail

#ifdef _msc_ver

// this implementation compiles on msvc and clang but not gcc

namespace detail {

template <class t, unsigned long long n, class seq>
struct make_integer_sequence_unchecked;

template <class t, unsigned long long n, unsigned long long ...indices>
struct make_integer_sequence_unchecked <
    t, n, integer_sequence <t, indices...>>
{
    typedef typename make_integer_sequence_unchecked<
        t, n-1, integer_sequence<t, n-1, indices...>>::type type;
};

template <class t, unsigned long long ...indices>
struct make_integer_sequence_unchecked <
    t, 0, integer_sequence<t, indices...>>
{
    typedef integer_sequence <t, indices...> type;
};

template <class t, t n>
struct make_integer_sequence_checked
{
    static_assert (is_integral <t>::value,
        "t must be an integral type");

    static_assert (n >= 0,
        "n must be non-negative");

    typedef typename make_integer_sequence_unchecked <
        t, n, integer_sequence<t>>::type type;
};

} // detail

template <class t, t n>
using make_integer_sequence =
    typename detail::make_integer_sequence_checked <t, n>::type;

template <size_t n>
using make_index_sequence = make_integer_sequence <size_t, n>;

template <class... args>
using index_sequence_for =
    make_index_sequence <detail::sizeof_workaround <args...>::size>;

#else

// this implementation compiles on gcc but not msvc

namespace detail {

template <size_t... ints>
struct index_tuple
{
    typedef index_tuple <ints..., sizeof... (ints)> next;

};

template <size_t n>
struct build_index_tuple
{
    typedef typename build_index_tuple <n-1>::type::next type;
};

template <>
struct build_index_tuple <0>
{
    typedef index_tuple<> type;
};

template <class t, t n,
    class seq = typename build_index_tuple <n>::type
>
struct make_integer_sequence;

template <class t, t n, size_t... ints>
struct make_integer_sequence <t, n, index_tuple <ints...>>
{
    static_assert (is_integral <t>::value,
        "t must be an integral type");

    static_assert (n >= 0,
        "n must be non-negative");

    typedef integer_sequence <t, static_cast <t> (ints)...> type;
};

} // detail

template <class t, t n>
using make_integer_sequence =
    typename detail::make_integer_sequence <t, n>::type;

template <size_t n>
using make_index_sequence = make_integer_sequence <size_t, n>;

template <class... args>
using index_sequence_for =
    make_index_sequence <detail::sizeof_workaround <args...>::size>;

#endif

}

#endif
