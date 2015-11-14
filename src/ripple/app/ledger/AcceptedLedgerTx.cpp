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
#include <ripple/app/ledger/acceptedledgertx.h>
#include <ripple/app/ledger/ledgerentryset.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/protocol/jsonfields.h>
#include <boost/foreach.hpp>

namespace ripple {

acceptedledgertx::acceptedledgertx (ledger::ref ledger, serializeriterator& sit)
    : mledger (ledger)
{
    serializer          txnser (sit.getvl ());
    serializeriterator  txnit (txnser);

    mtxn =      std::make_shared<sttx> (std::ref (txnit));
    mrawmeta =  sit.getvl ();
    mmeta =     std::make_shared<transactionmetaset> (mtxn->gettransactionid (),
        ledger->getledgerseq (), mrawmeta);
    maffected = mmeta->getaffectedaccounts ();
    mresult =   mmeta->getresultter ();
}

acceptedledgertx::acceptedledgertx (ledger::ref ledger,
    sttx::ref txn, transactionmetaset::ref met)
    : mledger (ledger)
    , mtxn (txn)
    , mmeta (met)
    , maffected (met->getaffectedaccounts ())
{
    mresult = mmeta->getresultter ();
}

acceptedledgertx::acceptedledgertx (ledger::ref ledger,
    sttx::ref txn, ter result)
    : mledger (ledger)
    , mtxn (txn)
    , mresult (result)
    , maffected (txn->getmentionedaccounts ())
{
}

std::string acceptedledgertx::getescmeta () const
{
    assert (!mrawmeta.empty ());
    return sqlescape (mrawmeta);
}

void acceptedledgertx::buildjson ()
{
    mjson = json::objectvalue;
    mjson[jss::transaction] = mtxn->getjson (0);

    if (mmeta)
    {
        mjson[jss::meta] = mmeta->getjson (0);
        mjson[jss::raw_meta] = strhex (mrawmeta);
    }

    mjson[jss::result] = transhuman (mresult);

    if (!maffected.empty ())
    {
        json::value& affected = (mjson[jss::affected] = json::arrayvalue);
        boost_foreach (const rippleaddress & ra, maffected)
        {
            affected.append (ra.humanaccountid ());
        }
    }

    if (mtxn->gettxntype () == ttoffer_create)
    {
        auto const account (mtxn->getsourceaccount ().getaccountid ());
        auto const amount (mtxn->getfieldamount (sftakergets));

        // if the offer create is not self funded then add the owner balance
        if (account != amount.issue ().account)
        {
            ledgerentryset les (mledger, tapnone, true);
            auto const ownerfunds (les.accountfunds (account, amount, fhignore_freeze));

            mjson[jss::transaction][jss::owner_funds] = ownerfunds.gettext ();
        }
    }
}

} // ripple
