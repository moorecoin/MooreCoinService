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
#include <ripple/rpc/impl/getmastergenerator.h>

namespace ripple {
namespace rpc {

// look up the master public generator for a regular seed so we may index source accounts ids.
// --> naregularseed
// <-- namastergenerator
json::value getmastergenerator (
    ledger::ref lrledger, rippleaddress const& naregularseed,
    rippleaddress& namastergenerator, networkops& netops)
{
    rippleaddress       na0public;      // to find the generator's index.
    rippleaddress       na0private;     // to decrypt the master generator's cipher.
    rippleaddress       nagenerator = rippleaddress::creategeneratorpublic (naregularseed);

    na0public.setaccountpublic (nagenerator, 0);
    na0private.setaccountprivate (nagenerator, naregularseed, 0);

    sle::pointer        slegen          = netops.getgenerator (lrledger, na0public.getaccountid ());

    if (!slegen)
    {
        // no account has been claimed or has had it password set for seed.
        return rpcerror (rpcno_account);
    }

    blob    vuccipher           = slegen->getfieldvl (sfgenerator);
    blob    vucmastergenerator  = na0private.accountprivatedecrypt (na0public, vuccipher);

    if (vucmastergenerator.empty ())
    {
        return rpcerror (rpcfail_gen_decrypt);
    }

    namastergenerator.setgenerator (vucmastergenerator);

    return json::value (json::objectvalue);
}

} // rpc
} // ripple
