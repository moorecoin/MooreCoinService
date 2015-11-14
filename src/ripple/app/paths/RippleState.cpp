//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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
#include <ripple/app/book/types.h>
#include <ripple/app/paths/ripplestate.h>
#include <ripple/protocol/stamount.h>
#include <cstdint>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

ripplestate::pointer ripplestate::makeitem (
    account const& accountid, stledgerentry::ref ledgerentry)
{
    // vfalco does this ever happen in practice?
    if (!ledgerentry || ledgerentry->gettype () != ltripple_state)
        return pointer ();

    return pointer (new ripplestate (ledgerentry, accountid));
}

ripplestate::ripplestate (
        stledgerentry::ref ledgerentry,
        account const& viewaccount)
    : mledgerentry (ledgerentry)
    , mlowlimit (ledgerentry->getfieldamount (sflowlimit))
    , mhighlimit (ledgerentry->getfieldamount (sfhighlimit))
    , mlowid (mlowlimit.getissuer ())
    , mhighid (mhighlimit.getissuer ())
    , mbalance (ledgerentry->getfieldamount (sfbalance))
{
    mflags          = mledgerentry->getfieldu32 (sfflags);

    mlowqualityin   = mledgerentry->getfieldu32 (sflowqualityin);
    mlowqualityout  = mledgerentry->getfieldu32 (sflowqualityout);

    mhighqualityin  = mledgerentry->getfieldu32 (sfhighqualityin);
    mhighqualityout = mledgerentry->getfieldu32 (sfhighqualityout);

    mviewlowest = (mlowid == viewaccount);

    if (!mviewlowest)
        mbalance.negate ();
}

json::value ripplestate::getjson (int)
{
    json::value ret (json::objectvalue);
    ret["low_id"] = to_string (mlowid);
    ret["high_id"] = to_string (mhighid);
    return ret;
}

std::vector <ripplestate::pointer>
getripplestateitems (
    account const& accountid,
    ledger::ref ledger)
{
    std::vector <ripplestate::pointer> items;

    ledger->visitaccountitems (accountid,
        [&items,&accountid](sle::ref slecur)
        {
             auto ret = ripplestate::makeitem (accountid, slecur);

             if (ret)
                items.push_back (ret);
        });

    return items;
}

} // ripple
