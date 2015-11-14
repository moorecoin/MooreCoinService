//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, tom ritchford <tom@swirly.com>

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

#ifndef beast_utility_zero_h_included
#define beast_utility_zero_h_included

#include <beast/config/compilerconfig.h>

// vs2013 sp1 fails with decltype return
#define beast_no_zero_auto_return 1

namespace beast {

/** zero allows classes to offer efficient comparisons to zero.

    zero is a struct to allow classes to efficiently compare with zero without
    requiring an rvalue construction.

    it's often the case that we have classes which combine a number and a unit.
    in such cases, comparisons like t > 0 or t != 0 make sense, but comparisons
    like t > 1 or t != 1 do not.

    the class zero allows such comparisons to be easily made.

    the comparing class t either needs to have a method called signum() which
    returns a positive number, 0, or a negative; or there needs to be a signum
    function which resolves in the namespace which takes an instance of t and
    returns a positive, zero or negative number.
*/

struct zero
{
};

namespace {
static beast_constexpr zero zero{};
}

/** default implementation of signum calls the method on the class. */
template <typename t>
#if beast_no_zero_auto_return
int signum(t const& t)
#else
auto signum(t const& t) -> decltype(t.signum())
#endif
{
    return t.signum();
}

namespace detail {
namespace zero_helper {

// for argument dependent lookup to function properly, calls to signum must
// be made from a namespace that does not include overloads of the function..
template <class t>
#if beast_no_zero_auto_return
int call_signum (t const& t)
#else
auto call_signum(t const& t) -> decltype(t.signum())
#endif
{
    return signum(t);
}

} // zero_helper
} // detail

// handle operators where t is on the left side using signum.

template <typename t>
bool operator==(t const& t, zero)
{
    return detail::zero_helper::call_signum(t) == 0;
}

template <typename t>
bool operator!=(t const& t, zero)
{
    return detail::zero_helper::call_signum(t) != 0;
}

template <typename t>
bool operator<(t const& t, zero)
{
    return detail::zero_helper::call_signum(t) < 0;
}

template <typename t>
bool operator>(t const& t, zero)
{
    return detail::zero_helper::call_signum(t) > 0;
}

template <typename t>
bool operator>=(t const& t, zero)
{
    return detail::zero_helper::call_signum(t) >= 0;
}

template <typename t>
bool operator<=(t const& t, zero)
{
    return detail::zero_helper::call_signum(t) <= 0;
}

// handle operators where t is on the right side by
// reversing the operation, so that t is on the left side.

template <typename t>
bool operator==(zero, t const& t)
{
    return t == zero;
}

template <typename t>
bool operator!=(zero, t const& t)
{
    return t != zero;
}

template <typename t>
bool operator<(zero, t const& t)
{
    return t > zero;
}

template <typename t>
bool operator>(zero, t const& t)
{
    return t < zero;
}

template <typename t>
bool operator>=(zero, t const& t)
{
    return t <= zero;
}

template <typename t>
bool operator<=(zero, t const& t)
{
    return t >= zero;
}

} // beast

#endif
