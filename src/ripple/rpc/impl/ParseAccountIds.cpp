//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012-2014 ripple labs inc.

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

#include <beastconfig.h>
#include <ripple/rpc/impl/parseaccountids.h>

namespace ripple {
namespace rpc {

hash_set<rippleaddress> parseaccountids (json::value const& jvarray)
{
    hash_set<rippleaddress> result;

    for (auto const& jv: jvarray)
    {
        rippleaddress address;

        if (!(jv.isstring () && address.setaccountid ((jv.asstring ()))))
        {
            result.clear ();
            break;
        }
        result.insert (address);
    }

    return result;
}

} // rpc
} // ripple
