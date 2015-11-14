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
//   secret: <string>   // optional
// }
//
// this command requires role::admin access because it makes no sense to ask
// an untrusted server for this.
json::value dovalidationcreate (rpc::context& context)
{
    rippleaddress   raseed;
    json::value     obj (json::objectvalue);

    if (!context.params.ismember ("secret"))
    {
        writelog (lsdebug, rpchandler) << "creating random validation seed.";

        raseed.setseedrandom ();                // get a random seed.
    }
    else if (!raseed.setseedgeneric (context.params["secret"].asstring ()))
    {
        return rpcerror (rpcbad_seed);
    }

    obj["validation_public_key"]
            = rippleaddress::createnodepublic (raseed).humannodepublic ();
    obj["validation_seed"] = raseed.humanseed ();
    obj["validation_key"] = raseed.humanseed1751 ();

    return obj;
}

} // ripple
