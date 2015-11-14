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
#include <ripple/app/peers/uniquenodelist.h>

namespace ripple {

// {
//   node: <domain>|<node_public>,
//   comment: <comment>             // optional
// }
json::value dounladd (rpc::context& context)
{
    auto lock = getapp().masterlock();

    std::string strnode = context.params.ismember ("node")
            ? context.params["node"].asstring () : "";
    std::string strcomment = context.params.ismember ("comment")
            ? context.params["comment"].asstring () : "";

    rippleaddress ranodepublic;

    if (ranodepublic.setnodepublic (strnode))
    {
        getapp().getunl ().nodeaddpublic (
            ranodepublic, uniquenodelist::vsmanual, strcomment);
        return rpc::makeobjectvalue ("adding node by public key");
    }
    else
    {
        getapp().getunl ().nodeadddomain (
            strnode, uniquenodelist::vsmanual, strcomment);
        return rpc::makeobjectvalue ("adding node by domain");
    }
}

} // ripple
