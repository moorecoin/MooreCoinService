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
#include <ripple/rpc/impl/getmastergenerator.h>

namespace ripple {

// {
//   seed: <string>
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
// }
json::value dowalletaccounts (rpc::context& context)
{
    ledger::pointer ledger;
    json::value jvresult
            = rpc::lookupledger (context.params, ledger, context.netops);

    if (!ledger)
        return jvresult;

    rippleaddress   naseed;

    if (!context.params.ismember ("seed")
        || !naseed.setseedgeneric (context.params["seed"].asstring ()))
    {
        return rpcerror (rpcbad_seed);
    }

    // try the seed as a master seed.
    rippleaddress namastergenerator
            = rippleaddress::creategeneratorpublic (naseed);

    json::value jsonaccounts
            = rpc::accounts (ledger, namastergenerator, context.netops);

    if (jsonaccounts.empty ())
    {
        // no account via seed as master, try seed a regular.
        json::value ret = rpc::getmastergenerator (
            ledger, naseed, namastergenerator, context.netops);

        if (!ret.empty ())
            return ret;

        ret[jss::accounts]
                = rpc::accounts (ledger, namastergenerator, context.netops);
        return ret;
    }
    else
    {
        // had accounts via seed as master, return them.
        return rpc::makeobjectvalue (jsonaccounts, jss::accounts);
    }
}

} // ripple
