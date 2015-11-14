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
//   'ident' : <indent>,
//   'account_index' : <index> // optional
// }
json::value doownerinfo (rpc::context& context)
{
    auto lock = getapp().masterlock();
    if (!context.params.ismember ("account") &&
        !context.params.ismember ("ident"))
    {
        return rpc::missing_field_error ("account");
    }

    std::string strident = context.params.ismember ("account")
            ? context.params["account"].asstring ()
            : context.params["ident"].asstring ();
    bool bindex;
    int iindex = context.params.ismember ("account_index")
            ? context.params["account_index"].asuint () : 0;
    rippleaddress raaccount;
    json::value ret;

    // get info on account.

    auto const& closedledger = context.netops.getclosedledger ();
    json::value jaccepted = rpc::accountfromstring (
        closedledger,
        raaccount,
        bindex,
        strident,
        iindex,
        false,
        context.netops);

    ret["accepted"] = jaccepted.empty () ? context.netops.getownerinfo (
        closedledger, raaccount) : jaccepted;

    auto const& currentledger = context.netops.getcurrentledger ();
    json::value jcurrent = rpc::accountfromstring (
        currentledger,
        raaccount,
        bindex,
        strident,
        iindex,
        false,
        context.netops);

    ret["current"] = jcurrent.empty () ? context.netops.getownerinfo (
        currentledger, raaccount) : jcurrent;

    return ret;
}

} // ripple
