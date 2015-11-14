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
#include <ripple/overlay/overlay.h>

namespace ripple {

// {
//   ip: <string>,
//   port: <number>
// }
// xxx might allow domain for manual connections.
json::value doconnect (rpc::context& context)
{
    auto lock = getapp().masterlock();
    if (getconfig ().run_standalone)
        return "cannot connect in standalone mode";

    if (!context.params.ismember ("ip"))
        return rpc::missing_field_error ("ip");

    if (context.params.ismember ("port") &&
        !context.params["port"].isconvertibleto (json::intvalue))
    {
        return rpcerror (rpcinvalid_params);
    }

    int iport;

    if(context.params.ismember ("port"))
        iport = context.params["port"].asint ();
    else
        iport = 6561;

    auto ip = beast::ip::endpoint::from_string(
        context.params["ip"].asstring ());

    if (! is_unspecified (ip))
        getapp().overlay ().connect (ip.at_port(iport));

    return rpc::makeobjectvalue ("connecting");
}

} // ripple
