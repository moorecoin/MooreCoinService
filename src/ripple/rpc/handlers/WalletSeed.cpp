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
json::value dowalletseed (rpc::context& context)
{
    rippleaddress   seed;
    bool bsecret = context.params.ismember ("secret");

    if (bsecret && !seed.setseedgeneric (context.params["secret"].asstring ()))
    {
        return rpcerror (rpcbad_seed);
    }
    else
    {
        rippleaddress   raaccount;

        if (!bsecret)
        {
            seed.setseedrandom ();
        }

        rippleaddress ragenerator = rippleaddress::creategeneratorpublic (seed);

        raaccount.setaccountpublic (ragenerator, 0);

        json::value obj (json::objectvalue);

        obj["seed"]     = seed.humanseed ();
        obj["key"]      = seed.humanseed1751 ();
        obj["deprecated"] = "use wallet_propose instead";

        return obj;
    }
}
} // ripple
