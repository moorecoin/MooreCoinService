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

#ifndef beast_container_aged_associative_container_h_included
#define beast_container_aged_associative_container_h_included

#include <type_traits>

namespace beast {
namespace detail {

// extracts the key portion of value
template <bool maybe_map>
struct aged_associative_container_extract_t
{
    template <class value>
    decltype (value::first) const&
    operator() (value const& value) const
    {
        return value.first;
    }
};

template <>
struct aged_associative_container_extract_t <false>
{
    template <class value>
    value const&
    operator() (value const& value) const
    {
        return value;
    }
};

}
}

#endif
