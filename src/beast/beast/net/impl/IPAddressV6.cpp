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

#if beast_include_beastconfig
#include "../../beastconfig.h"
#endif

#include <beast/net/ipaddressv6.h>

namespace beast {
namespace ip {

//------------------------------------------------------------------------------

bool is_loopback (addressv6 const&)
{
    // vfalco todo
    assert(false);
    return false;
}

bool is_unspecified (addressv6 const&)
{
    // vfalco todo
    assert(false);
    return false;
}

bool is_multicast (addressv6 const&)
{
    // vfalco todo
    assert(false);
    return false;
}

bool is_private (addressv6 const&)
{
    // vfalco todo
    assert(false);
    return false;
}

bool is_public (addressv6 const&)
{
    // vfalco todo
    assert(false);
    return false;
}

//------------------------------------------------------------------------------

std::string to_string (addressv6 const&)
{
    // vfalco todo
    assert(false);
    return "";
}

std::istream& operator>> (std::istream& is, addressv6&)
{
    // vfalco todo
    assert(false);
    return is;
}

}
}
