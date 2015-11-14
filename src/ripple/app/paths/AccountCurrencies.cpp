//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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
#include <ripple/app/paths/accountcurrencies.h>

namespace ripple {

currencyset accountsourcecurrencies (
    rippleaddress const& raaccountid,
    ripplelinecache::ref lrcache,
    bool includexrp)
{
    currencyset currencies;

    // yyy only bother if they are above reserve
    if (includexrp) {
        currencies.insert (xrpcurrency());
        currencies.insert (vbccurrency());
    }

    // list of ripple lines.
    auto& ripplelines (lrcache->getripplelines (raaccountid.getaccountid ()));

    for (auto const& item : ripplelines)
    {
        auto rspentry = (ripplestate*) item.get ();
        assert (rspentry);
        if (!rspentry)
            continue;

        auto& sabalance = rspentry->getbalance ();

        // filter out non
        if (sabalance > zero
            // have ious to send.
            || (rspentry->getlimitpeer ()
                // peer extends credit.
                && ((-sabalance) < rspentry->getlimitpeer ()))) // credit left.
        {
            currencies.insert (sabalance.getcurrency ());
        }
    }

    currencies.erase (badcurrency());
    return currencies;
}

currencyset accountdestcurrencies (
    rippleaddress const& raaccountid,
    ripplelinecache::ref lrcache,
    bool includexrp)
{
    currencyset currencies;

    if (includexrp) {
        currencies.insert (xrpcurrency());
        currencies.insert (vbccurrency());
    }
    // even if account doesn't exist

    // list of ripple lines.
    auto& ripplelines (lrcache->getripplelines (raaccountid.getaccountid ()));

    for (auto const& item : ripplelines)
    {
        auto rspentry = (ripplestate*) item.get ();
        assert (rspentry);
        if (!rspentry)
            continue;

        auto& sabalance  = rspentry->getbalance ();

        if (sabalance < rspentry->getlimit ())                  // can take more
            currencies.insert (sabalance.getcurrency ());
    }

    currencies.erase (badcurrency());
    return currencies;
}

} // ripple
