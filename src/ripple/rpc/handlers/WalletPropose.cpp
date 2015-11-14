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
//  passphrase: <string>
// }
json::value dowalletpropose (rpc::context& context)
{
    rippleaddress   naseed;
    rippleaddress   naaccount;

    if (!context.params.ismember ("passphrase"))
        naseed.setseedrandom ();

    else if (!naseed.setseedgeneric (context.params["passphrase"].asstring ()))
        return rpcerror(rpcbad_seed);

    rippleaddress nagenerator = rippleaddress::creategeneratorpublic (naseed);
    naaccount.setaccountpublic (nagenerator, 0);

    json::value obj (json::objectvalue);

    obj["master_seed"]      = naseed.humanseed ();
    obj["master_seed_hex"]  = to_string (naseed.getseed ());
    obj["master_key"]     = naseed.humanseed1751();
    obj["account_id"]       = naaccount.humanaccountid ();
    obj["public_key"] = naaccount.humanaccountpublic();

    auto acct = naaccount.getaccountpublic();
    obj["public_key_hex"] = strhex(acct.begin(), acct.size());

    return obj;
}

} // ripple
