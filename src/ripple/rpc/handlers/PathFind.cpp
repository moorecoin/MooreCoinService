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
#include <ripple/app/paths/pathrequests.h>

namespace ripple {

json::value dopathfind (rpc::context& context)
{
    ledger::pointer lpledger = context.netops.getclosedledger();

    if (!context.params.ismember ("subcommand") ||
        !context.params["subcommand"].isstring ())
    {
        return rpcerror (rpcinvalid_params);
    }

    if (!context.infosub)
        return rpcerror (rpcno_events);

    std::string ssubcommand = context.params["subcommand"].asstring ();

    if (ssubcommand == "create")
    {
        context.loadtype = resource::feehighburdenrpc;
        context.infosub->clearpathrequest ();
        return getapp().getpathrequests().makepathrequest (
            context.infosub, lpledger, context.params);
    }

    if (ssubcommand == "close")
    {
        pathrequest::pointer request = context.infosub->getpathrequest ();

        if (!request)
            return rpcerror (rpcno_pf_request);

        context.infosub->clearpathrequest ();
        return request->doclose (context.params);
    }

    if (ssubcommand == "status")
    {
        pathrequest::pointer request = context.infosub->getpathrequest ();

        if (!request)
            return rpcerror (rpcno_pf_request);

        return request->dostatus (context.params);
    }

    return rpcerror (rpcinvalid_params);
}

} // ripple
