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

#ifndef beast_cxx14_functional_h_included
#define beast_cxx14_functional_h_included

#include <beast/cxx14/config.h>

#include <functional>
#include <type_traits>
#include <utility>

#if ! beast_no_cxx14_compatibility

namespace std {

// c++14 implementation of std::equal_to <void> specialization.
// this supports heterogeneous comparisons.
template <>
struct equal_to <void>
{
    // vfalco note its not clear how to support is_transparent pre c++14
    typedef std::true_type is_transparent;

    template <class t, class u>
    auto operator() (t&& lhs, u&& rhs) const ->
        decltype (std::forward <t> (lhs) == std::forward <u> (rhs))
    {
        return std::forward <t> (lhs) == std::forward <u> (rhs);
    }
};

}

#endif

#endif
