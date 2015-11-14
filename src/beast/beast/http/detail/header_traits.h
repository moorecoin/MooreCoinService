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

#ifndef beast_http_header_traits_h_included
#define beast_http_header_traits_h_included

#include <beast/utility/ci_char_traits.h>

#include <boost/utility/string_ref.hpp>

#include <memory>
#include <string>

namespace beast {
namespace http {
namespace detail {

// utilities for dealing with http headers

template <class allocator = std::allocator <char>>
using basic_field_string =
    std::basic_string <char, ci_char_traits, allocator>;

typedef basic_field_string <> field_string;

typedef boost::basic_string_ref <char, ci_char_traits> field_string_ref;

/** returns `true` if two header fields are the same.
    the comparison is case-insensitive.
*/
template <class alloc1, class alloc2>
inline
bool field_eq (
    std::basic_string <char, std::char_traits <char>, alloc1> const& s1,
    std::basic_string <char, std::char_traits <char>, alloc2> const& s2)
{
    return field_string_ref (s1.c_str(), s1.size()) ==
           field_string_ref (s2.c_str(), s2.size());
}

/** returns the string with leading and trailing lws removed. */

}
}
}

#endif
