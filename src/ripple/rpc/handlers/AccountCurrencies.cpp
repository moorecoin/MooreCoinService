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
#include <ripple/rpc/impl/accountfromstring.h>
#include <ripple/app/paths/ripplestate.h>

namespace ripple {

json::value doaccountcurrencies (rpc::context& context)
{
    auto& params = context.params;

    // get the current ledger
    ledger::pointer ledger;
    json::value result (rpc::lookupledger (params, ledger, context.netops));

    if (!ledger)
        return result;

    if (!(params.ismember ("account") || params.ismember ("ident")))
        return rpc::missing_field_error ("account");

    std::string const strident (params.ismember ("account")
        ? params["account"].asstring ()
        : params["ident"].asstring ());

    int const iindex (params.ismember ("account_index")
        ? params["account_index"].asuint ()
        : 0);
    bool const bstrict = params.ismember ("strict") &&
            params["strict"].asbool ();

    // get info on account.
    bool bindex; // out param
    rippleaddress naaccount; // out param
    json::value jvaccepted (rpc::accountfromstring (
        ledger, naaccount, bindex, strident, iindex, bstrict, context.netops));

    if (!jvaccepted.empty ())
        return jvaccepted;

    std::set<currency> send, receive;
    for (auto const& item : getripplestateitems (naaccount.getaccountid (), ledger))
    {
        ripplestate* rspentry = (ripplestate*) item.get ();
        stamount const& sabalance = rspentry->getbalance ();

        if (sabalance < rspentry->getlimit ())
            receive.insert (sabalance.getcurrency ());
        if ((-sabalance) < rspentry->getlimitpeer ())
            send.insert (sabalance.getcurrency ());
    }

    send.erase (badcurrency());
    receive.erase (badcurrency());

    json::value& sendcurrencies =
            (result["send_currencies"] = json::arrayvalue);
    for (auto const& c: send)
        sendcurrencies.append (to_string (c));

    json::value& recvcurrencies =
            (result["receive_currencies"] = json::arrayvalue);
    for (auto const& c: receive)
        recvcurrencies.append (to_string (c));

    return result;
}

} // ripple
