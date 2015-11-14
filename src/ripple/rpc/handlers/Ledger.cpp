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
#include <ripple/app/ledger/ledgertojson.h>
#include <ripple/core/loadfeetrack.h>
#include <ripple/protocol/errorcodes.h>
#include <ripple/rpc/handlers/ledger.h>
#include <ripple/rpc/impl/jsonobject.h>
#include <ripple/server/role.h>

namespace ripple {
namespace rpc {

ledgerhandler::ledgerhandler (context& context) : context_ (context)
{
}

status ledgerhandler::check ()
{
    auto const& params = context_.params;
    bool needsledger = params.ismember (jss::ledger) ||
            params.ismember (jss::ledger_hash) ||
            params.ismember (jss::ledger_index);
    if (!needsledger)
        return status::ok;

    if (auto s = rpc::lookupledger (params, ledger_, context_.netops, result_))
        return s;

    bool bfull = params[jss::full].asbool();
    bool bwithdividend = params["dividend"].asbool ();
    bool btransactions = params[jss::transactions].asbool();
    bool baccounts = params[jss::accounts].asbool();
    bool bexpand = params[jss::expand].asbool();

    options_ = (bfull ? ledger_json_full : 0)
            | (bexpand ? ledger_json_expand : 0)
            | (bwithdividend ? ledger_json_dump_txdiv : 0)
            | (btransactions ? ledger_json_dump_txrp : 0)
            | (baccounts ? ledger_json_dump_state : 0);

    if (bfull || baccounts)
    {
        // until some sane way to get full ledgers has been implemented,
        // disallow retrieving all state nodes.
        if (context_.role != role::admin)
            return rpcno_permission;

        if (getapp().getfeetrack().isloadedlocal() &&
            context_.role != role::admin)
        {
            return rpctoo_busy;
        }
        context_.loadtype = resource::feehighburdenrpc;
    }

    return status::ok;
}

} // rpc
} // ripple
