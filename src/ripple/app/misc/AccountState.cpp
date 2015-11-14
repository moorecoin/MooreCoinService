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
#include <ripple/app/misc/accountstate.h>
#include <ripple/basics/log.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/json/to_string.h>
#include <ripple/protocol/indexes.h>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

namespace ripple {

accountstate::accountstate (rippleaddress const& naaccountid)
    : maccountid (naaccountid)
    , mvalid (false)
{
    if (naaccountid.isvalid ())
    {
        mvalid = true;

        mledgerentry = std::make_shared <stledgerentry> (
                           ltaccount_root, getaccountrootindex (naaccountid));

        mledgerentry->setfieldaccount (sfaccount, naaccountid.getaccountid ());
    }
}

accountstate::accountstate (sle::ref ledgerentry, rippleaddress const& naaccountid, sle::pointer slerefer) :
    maccountid (naaccountid), mledgerentry (ledgerentry), mvalid (false), mslerefer(slerefer)
{
    if (!mledgerentry)
        return;

    if (mledgerentry->gettype () != ltaccount_root)
        return;
    
    if (slerefer && slerefer->gettype() != ltrefer) {
        return;
    }

    mvalid = true;
}

// vfalco todo make this a generic utility function of some container class
//
std::string accountstate::creategravatarurl (uint128 uemailhash)
{
    blob    vucmd5 (uemailhash.begin (), uemailhash.end ());
    std::string                 strmd5lower = strhex (vucmd5);
    boost::to_lower (strmd5lower);

    // vfalco todo give a name and move this constant to a more visible location.
    //             also shouldn't this be https?
    return str (boost::format ("http://www.gravatar.com/avatar/%s") % strmd5lower);
}

void accountstate::addjson (json::value& val)
{
    val = mledgerentry->getjson (0);

    if (mvalid)
    {
        if (mledgerentry->isfieldpresent (sfemailhash))
            val["urlgravatar"]  = creategravatarurl (mledgerentry->getfieldh128 (sfemailhash));
        if (mslerefer) {
            val["references"] = mslerefer->getjson(0)["references"];
        }
    }
    else
    {
        val["invalid"] = true;
    }
}

void accountstate::dump ()
{
    json::value j (json::objectvalue);
    addjson (j);
    writelog (lsinfo, ledger) << j;
}

} // ripple
