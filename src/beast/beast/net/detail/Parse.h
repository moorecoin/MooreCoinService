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

#ifndef beast_net_detail_parse_h_included
#define beast_net_detail_parse_h_included

#include <ios>
#include <string>

namespace beast {
namespace ip {

namespace detail {

/** require and consume the specified character from the input.
    @return `true` if the character matched.
*/
template <typename inputstream>
bool expect (inputstream& is, char v)
{
    char c;
    if (is.get(c) && v == c)
        return true;
    is.unget();
    is.setstate (std::ios_base::failbit);
    return false;
}

/** require and consume whitespace from the input.
    @return `true` if the character matched.
*/
template <typename inputstream>
bool expect_whitespace (inputstream& is)
{
    char c;
    if (is.get(c) && isspace(c))
        return true;
    is.unget();
    is.setstate (std::ios_base::failbit);
    return false;
}

/** used to disambiguate 8-bit integers from characters. */
template <typename inttype>
struct integer_holder
{
    inttype* pi;
    explicit integer_holder (inttype& i)
        : pi (&i)
    {
    }
    template <typename otherinttype>
    inttype& operator= (otherinttype o) const
    {
        *pi = o;
        return *pi;
    }
};

/** parse 8-bit unsigned integer. */
template <typename inputstream>
inputstream& operator>> (inputstream& is, integer_holder <std::uint8_t> const& i)
{
    std::uint16_t v;
    is >> v;
    if (! (v>=0 && v<=255))
    {
        is.setstate (std::ios_base::failbit);
        return is;
    }
    i = std::uint8_t(v);
    return is;
}

/** free function for template argument deduction. */
template <typename inttype>
integer_holder <inttype> integer (inttype& i)
{
    return integer_holder <inttype> (i);
}

}

}
}

#endif
