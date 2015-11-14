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
#include <ripple/rpc/impl/accounts.h>

namespace ripple {
namespace rpc {

json::value accounts (
    ledger::ref lrledger,
    rippleaddress const& namastergenerator,
    networkops& netops)
{
    json::value jsonaccounts (json::arrayvalue);

    // yyy don't want to leak to thin server that these accounts are related.
    // yyy would be best to alternate requests to servers and to cache results.
    unsigned int    uindex  = 0;

    do
    {
        rippleaddress       naaccount;

        naaccount.setaccountpublic (namastergenerator, uindex++);

        accountstate::pointer as    = netops.getaccountstate (lrledger, naaccount);

        if (as)
        {
            json::value jsonaccount (json::objectvalue);

            as->addjson (jsonaccount);

            jsonaccounts.append (jsonaccount);
        }
        else
        {
            uindex  = 0;
        }
    }
    while (uindex);

    return jsonaccounts;
}

} // rpc
} // ripple
