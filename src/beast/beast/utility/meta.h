//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, howard hinnant <howard.hinnant@gmail.com>

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

#ifndef beast_utility_meta_h_included
#define beast_utility_meta_h_included

#include <beast/cxx14/type_traits.h> // <type_traits>

namespace beast {

template <bool ...> struct static_and;

template <bool b0, bool ... bn>
struct static_and <b0, bn...>
    : public std::integral_constant <
        bool, b0 && static_and<bn...>::value>
{
};

template <>
struct static_and<>
    : public std::true_type
{
};

static_assert( static_and<true, true, true>::value, "");
static_assert(!static_and<true, false, true>::value, "");

template <std::size_t ...>
struct static_sum;

template <std::size_t s0, std::size_t ...sn>
struct static_sum <s0, sn...>
    : public std::integral_constant <
        std::size_t, s0 + static_sum<sn...>::value>
{
};

template <>
struct static_sum<>
    : public std::integral_constant<std::size_t, 0>
{
};

static_assert(static_sum<5, 2, 17, 0>::value == 24, "");

template <class t, class u>
struct enable_if_lvalue
    : public std::enable_if
    <
    std::is_same<std::decay_t<t>, u>::value &&
    std::is_lvalue_reference<t>::value
    >
{
};

/** ensure const reference function parameters are valid lvalues.
 
    some functions, especially class constructors, accept const references and
    store them for later use. if any of those parameters are rvalue objects, 
    the object will be freed as soon as the function returns. this could 
    potentially lead to a variety of "use after free" errors.
 
    if the function is rewritten as a template using this type and the 
    parameters references as rvalue references (eg. tx&&), a compiler
    error will be generated if an rvalue is provided in the caller.
 
    @code
        // example:
        struct x
        {
        };
        struct y
        {
        };

        struct unsafe
        {
            unsafe (x const& x, y const& y)
                : x_ (x)
                , y_ (y)
            {
            }

            x const& x_;
            y const& y_;
        };

        struct safe
        {
            template <class tx, class ty,
            class = beast::enable_if_lvalue_t<tx, x>,
            class = beast::enable_if_lvalue_t < ty, y >>
                safe (tx&& x, ty&& y)
                : x_ (x)
                , y_ (y)
            {
            }

            x const& x_;
            y const& y_;
        };

        struct demo
        {
            void
                createobjects ()
            {
                x x {};
                y const y {};
                unsafe u1 (x, y);    // ok
                unsafe u2 (x (), y);  // compiles, but u2.x_ becomes invalid at the end of the line.
                safe s1 (x, y);      // ok
                //  safe s2 (x (), y);  // compile-time error
            }
        };
    @endcode
*/
template <class t, class u>
using enable_if_lvalue_t = typename enable_if_lvalue<t, u>::type;

} // beast

#endif // beast_utility_meta_h_included
