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
//   secret: <string>
// }
json::value dovalidationseed (rpc::context& context)
{
    auto lock = getapp().masterlock();
    json::value obj (json::objectvalue);

    if (!context.params.ismember ("secret"))
    {
        std::cerr << "unset validation seed." << std::endl;

        getconfig ().validation_seed.clear ();
        getconfig ().validation_pub.clear ();
        getconfig ().validation_priv.clear ();
    }
    else if (!getconfig ().validation_seed.setseedgeneric (
        context.params["secret"].asstring ()))
    {
        getconfig ().validation_pub.clear ();
        getconfig ().validation_priv.clear ();

        return rpcerror (rpcbad_seed);
    }
    else
    {
        auto& seed = getconfig ().validation_seed;
        auto& pub = getconfig ().validation_pub;

        pub = rippleaddress::createnodepublic (seed);
        getconfig ().validation_priv = rippleaddress::createnodeprivate (seed);

        obj["validation_public_key"] = pub.humannodepublic ();
        obj["validation_seed"] = seed.humanseed ();
        obj["validation_key"] = seed.humanseed1751 ();
    }

    return obj;
}

} // ripple
