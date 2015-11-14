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

namespace ripple {

// {
//   account: <indent>,
//   account_index : <index> // optional
//   strict: <bool>
//           if true, only allow public keys and addresses. false, default.
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
// }

// todo(tom): what is that "default"?
json::value doaccountinfo (rpc::context& context)
{
    auto& params = context.params;

    ledger::pointer ledger;
    json::value result = rpc::lookupledger (params, ledger, context.netops);

    if (!ledger)
        return result;

    if (!params.ismember ("account") && !params.ismember ("ident"))
        return rpc::missing_field_error ("account");

    std::string strident = params.ismember ("account")
            ? params["account"].asstring () : params["ident"].asstring ();
    bool bindex;
    int iindex = params.ismember ("account_index")
            ? params["account_index"].asuint () : 0;
    bool bstrict = params.ismember ("strict") && params["strict"].asbool ();
    rippleaddress naaccount;

    // get info on account.

    json::value jvaccepted = rpc::accountfromstring (
        ledger, naaccount, bindex, strident, iindex, bstrict, context.netops);

    if (!jvaccepted.empty ())
        return jvaccepted;

    auto asaccepted = context.netops.getaccountstate (ledger, naaccount);

    if (asaccepted)
    {
        asaccepted->addjson (jvaccepted);
        result["account_data"]    = jvaccepted;
    }
    else
    {
        result["account"] = naaccount.humanaccountid ();
        rpc::inject_error (rpcact_not_found, result);
    }

    return result;
}

} // ripple
