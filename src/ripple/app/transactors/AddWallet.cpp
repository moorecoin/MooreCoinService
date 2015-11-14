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
#include <ripple/app/transactors/transactor.h>
#include <ripple/basics/log.h>
#include <ripple/protocol/indexes.h>
#include <ripple/protocol/txflags.h>

namespace ripple {

class addwallet
    : public transactor
{
public:
    addwallet (
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine)
        : transactor (
            txn,
            params,
            engine,
            deprecatedlogs().journal("addwallet"))
    {
    }

    ter doapply () override
    {
        std::uint32_t const utxflags = mtxn.getflags ();

        if (utxflags & tfuniversalmask)
        {
            m_journal.trace <<
                "malformed transaction: invalid flags set.";

            return teminvalid_flag;
        }

        blob const vucpubkey    = mtxn.getfieldvl (sfpublickey);
        blob const vucsignature = mtxn.getfieldvl (sfsignature);

        auto const uauthkeyid = mtxn.getfieldaccount160 (sfregularkey);
        auto const namasterpubkey =
            rippleaddress::createaccountpublic (vucpubkey);
        auto const udstaccountid = namasterpubkey.getaccountid ();

        // fixme: this should be moved to the transaction's signature check logic
        // and cached.
        if (!namasterpubkey.accountpublicverify (
            serializer::getsha512half (uauthkeyid.begin (), uauthkeyid.size ()),
            vucsignature, ecdsa::not_strict))
        {
            m_journal.trace <<
                "unauthorized: bad signature ";
            return tefbad_add_auth;
        }

        sle::pointer sledst (mengine->entrycache (
            ltaccount_root, getaccountrootindex (udstaccountid)));

        if (sledst)
        {
            m_journal.trace <<
                "account already created";
            return tefcreated;
        }

        // direct xrp payment.

        stamount sadstamount = mtxn.getfieldamount (sfamount);
        stamount sapaid = mtxn.gettransactionfee ();
        stamount const sasrcbalance = mtxnaccount->getfieldamount (sfbalance);
		stamount const sasrcbalancevbc = mtxnaccount->getfieldamount(sfbalancevbc);
        std::uint32_t const uownercount = mtxnaccount->getfieldu32 (sfownercount);
        std::uint64_t const ureserve = mengine->getledger ()->getreserve (uownercount);


        // make sure have enough reserve to send. allow final spend to use reserve
        // for fee.
        // note: reserve is not scaled by fee.
        if (sasrcbalance + sapaid < sadstamount + ureserve)
        {
            // vote no. however, transaction might succeed, if applied in a
            // different order.
            m_journal.trace <<
                "delay transaction: insufficient funds: %s / %s (%d)" <<
                sasrcbalance.gettext () << " / " <<
                (sadstamount + ureserve).gettext () << " with reserve = " <<
                ureserve;
            return tecunfunded_add;
        }

		if (sasrcbalancevbc < sadstamount + ureserve)
		{
			// vbc: vote no. however, transaction might succeed, if applied in a
			// different order.
			m_journal.trace <<
				"delay transaction: insufficient funds: %s / %s (%d)" <<
				sasrcbalancevbc.gettext() << " / " <<
				(sadstamount + ureserve).gettext() << " with reserve = " <<
				ureserve;
			return tecunfunded_add;
		}

        // deduct initial balance from source account.
        mtxnaccount->setfieldamount (sfbalance, sasrcbalance - sadstamount);
		mtxnaccount->setfieldamount (sfbalancevbc, sasrcbalancevbc - sadstamount);
        // create the account.
        sledst  = mengine->entrycreate (ltaccount_root,
            getaccountrootindex (udstaccountid));

        sledst->setfieldaccount (sfaccount, udstaccountid);
        sledst->setfieldu32 (sfsequence, 1);
        sledst->setfieldamount (sfbalance, sadstamount);
		sledst->setfieldamount (sfbalancevbc, sadstamount);
        sledst->setfieldaccount (sfregularkey, uauthkeyid);

        return tessuccess;
    }
};

ter
transact_addwallet (
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine)
{
    return addwallet (txn, params, engine).apply ();
}

}
