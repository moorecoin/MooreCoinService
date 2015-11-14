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
#include <ripple/rpc/internalhandler.h>

namespace ripple {

rpc::internalhandler* rpc::internalhandler::headhandler = nullptr;

json::value dointernal (rpc::context& context)
{
    // used for debug or special-purpose rpc commands
    if (!context.params.ismember ("internal_command"))
        return rpcerror (rpcinvalid_params);

    auto name = context.params["internal_command"].asstring ();
    auto params = context.params["params"];

    for (auto* h = rpc::internalhandler::headhandler; h; )
    {
        if (name == h->name_)
        {
            writelog (lswarning, rpchandler)
                << "internal command " << name << ": " << params;
            json::value ret = h->handler_ (params);
            writelog (lswarning, rpchandler)
                << "internal command returns: " << ret;
            return ret;
        }

        h = h->nexthandler_;
    }

    return rpcerror (rpcbad_syntax);
}

} // ripple
