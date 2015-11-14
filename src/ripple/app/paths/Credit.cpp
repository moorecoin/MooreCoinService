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
#include <ripple/app/book/types.h>
#include <ripple/protocol/stamount.h>
#include <ripple/protocol/indexes.h>

namespace ripple {

stamount creditlimit (
    ledgerentryset& ledger,
    account const& account,
    account const& issuer,
    currency const& currency)
{
    stamount result ({currency, account});

    auto sleripplestate = ledger.entrycache (ltripple_state,
        getripplestateindex (account, issuer, currency));

    if (sleripplestate)
    {
        result = sleripplestate->getfieldamount (
            account < issuer ? sflowlimit : sfhighlimit);
        result.setissuer (account);
    }

    assert (result.getissuer () == account);
    assert (result.getcurrency () == currency);
    return result;
}

stamount creditbalance (
    ledgerentryset& ledger,
    account const& account,
    account const& issuer,
    currency const& currency)
{
    stamount result ({currency, account});

    auto sleripplestate = ledger.entrycache (ltripple_state,
        getripplestateindex (account, issuer, currency));

    if (sleripplestate)
    {
        if (assetcurrency() == currency)
            ledger.assetrelease(account, issuer, currency, sleripplestate);

        result = sleripplestate->getfieldamount (sfbalance);
        if (account < issuer)
            result.negate ();
        result.setissuer (account);
    }

    assert (result.getissuer () == account);
    assert (result.getcurrency () == currency);
    return result;
}

} // ripple
