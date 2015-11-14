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
#include <ripple/rpc/impl/accountfromstring.h>

namespace ripple {
namespace rpc {

// --> strident: public key, account id, or regular seed.
// --> bstrict: only allow account id or public key.
// <-- bindex: true if iindex > 0 and used the index.
//
// returns a json::objectvalue, containing error information if there was one.
json::value accountfromstring (
    ledger::ref lrledger,
    rippleaddress& naaccount,
    bool& bindex,
    std::string const& strident,
    int const iindex,
    bool const bstrict,
    networkops& netops)
{
    rippleaddress   naseed;

    if (naaccount.setaccountpublic (strident) ||
        naaccount.setaccountid (strident))
    {
        // got the account.
        bindex = false;
        return json::value (json::objectvalue);
    }

    if (bstrict)
    {
        auto success = naaccount.setaccountid (
            strident, base58::getbitcoinalphabet ());
        return rpcerror (success ? rpcact_bitcoin : rpcact_malformed);
    }

    // otherwise, it must be a seed.
    if (!naseed.setseedgeneric (strident))
        return rpcerror (rpcbad_seed);

    // we allow the use of the seeds to access #0.
    // this is poor practice and merely for debugging convenience.
    rippleaddress naregular0public;
    rippleaddress naregular0private;

    auto nagenerator = rippleaddress::creategeneratorpublic (naseed);

    naregular0public.setaccountpublic (nagenerator, 0);
    naregular0private.setaccountprivate (nagenerator, naseed, 0);

    sle::pointer slegen = netops.getgenerator (
        lrledger, naregular0public.getaccountid ());

    if (slegen)
    {
        // found master public key.
        blob vuccipher = slegen->getfieldvl (sfgenerator);
        blob vucmastergenerator = naregular0private.accountprivatedecrypt (
            naregular0public, vuccipher);

        if (vucmastergenerator.empty ())
            rpcerror (rpcno_gen_decrypt);

        nagenerator.setgenerator (vucmastergenerator);
    }
    // otherwise, if we didn't find a generator map, assume it is a master
    // generator.

    bindex  = !iindex;
    naaccount.setaccountpublic (nagenerator, iindex);

    return json::value (json::objectvalue);
}

} // rpc
} // ripple
