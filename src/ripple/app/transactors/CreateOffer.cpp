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
#include <ripple/app/book/offerstream.h>
#include <ripple/app/book/taker.h>
#include <ripple/app/book/types.h>
#include <ripple/app/book/amounts.h>
#include <ripple/app/transactors/createoffer.h>
#include <ripple/basics/log.h>
#include <ripple/json/to_string.h>
#include <beast/cxx14/memory.h>

namespace ripple {

ter
createoffer::checkacceptasset(issueref issue) const
{
    /* only valid for custom currencies */
    assert (!isxrp (issue.currency));
    assert (!isvbc (issue.currency));

    sle::pointer const issueraccount = mengine->entrycache (
        ltaccount_root, getaccountrootindex (issue.account));

    if (!issueraccount)
    {
        if (m_journal.warning) m_journal.warning <<
            "delay: can't receive ious from non-existent issuer: " <<
            to_string (issue.account);

        return (mparams & tapretry)
            ? terno_account
            : tecno_issuer;
    }

    if (issueraccount->getfieldu32 (sfflags) & lsfrequireauth)
    {
        sle::pointer const trustline (mengine->entrycache (
            ltripple_state, getripplestateindex (
                mtxnaccountid, issue.account, issue.currency)));

        if (!trustline)
        {
            return (mparams & tapretry)
                ? terno_line
                : tecno_line;
        }

        // entries have a canonical representation, determined by a
        // lexicographical "greater than" comparison employing strict weak
        // ordering. determine which entry we need to access.
        bool const canonical_gt (mtxnaccountid > issue.account);

        bool const is_authorized (trustline->getfieldu32 (sfflags) &
            (canonical_gt ? lsflowauth : lsfhighauth));

        if (!is_authorized)
        {
            if (m_journal.debug) m_journal.debug <<
                "delay: can't receive ious from issuer without auth.";

            return (mparams & tapretry)
                ? terno_auth
                : tecno_auth;
        }
    }

    return tessuccess;
}

std::pair<ter, core::amounts>
createoffer::crossoffers (core::ledgerview& view,
    core::amounts const& taker_amount)
{
#if ripple_enable_autobridging
    if (autobridging_)
        return crossoffersbridged (view, taker_amount);
#endif

    return crossoffersdirect (view, taker_amount);
}

createoffer::createoffer (bool autobridging, sttx const& txn,
    transactionengineparams params, transactionengine* engine)
    : transactor (txn, params, engine, deprecatedlogs().journal("createoffer"))
#if ripple_enable_autobridging
        , autobridging_ (autobridging)
#endif
{
}

ter
createoffer::doapply()
{
    if (m_journal.debug) m_journal.debug <<
        "offercreate> " << mtxn.getjson (0);

    std::uint32_t const utxflags = mtxn.getflags ();

    bool const bpassive (utxflags & tfpassive);
    bool const bimmediateorcancel (utxflags & tfimmediateorcancel);
    bool const bfillorkill (utxflags & tffillorkill);
    bool const bsell  (utxflags & tfsell);

    stamount satakerpays = mtxn.getfieldamount (sftakerpays);
    stamount satakergets = mtxn.getfieldamount (sftakergets);

    if (!islegalnet (satakerpays) || !islegalnet (satakergets))
        return tembad_amount;

    auto const& upaysissuerid = satakerpays.getissuer ();
    auto const& upayscurrency = satakerpays.getcurrency ();

    auto const& ugetsissuerid = satakergets.getissuer ();
    auto const& ugetscurrency = satakergets.getcurrency ();

    bool const bhaveexpiration (mtxn.isfieldpresent (sfexpiration));
    bool const bhavecancel (mtxn.isfieldpresent (sfoffersequence));

    std::uint32_t const uexpiration = mtxn.getfieldu32 (sfexpiration);
    std::uint32_t const ucancelsequence = mtxn.getfieldu32 (sfoffersequence);

    // fixme understand why we use sequencenext instead of current transaction
    //       sequence to determine the transaction. why is the offer seuqnce
    //       number insufficient?

    std::uint32_t const uaccountsequencenext = mtxnaccount->getfieldu32 (sfsequence);
    std::uint32_t const usequence = mtxn.getsequence ();

    const uint256 uledgerindex = getofferindex (mtxnaccountid, usequence);

    if (m_journal.debug)
    {
        m_journal.debug <<
            "creating offer node: " << to_string (uledgerindex) <<
            " usequence=" << usequence;

        if (bimmediateorcancel)
            m_journal.debug << "transaction: ioc set.";

        if (bfillorkill)
            m_journal.debug << "transaction: fok set.";
    }

    // this is the original rate of this offer, and is the rate at which it will
    // be placed, even if crossing offers change the amounts.
    std::uint64_t const urate = getrate (satakergets, satakerpays);

    ter terresult (tessuccess);

    // this is the ledger view that we work against. transactions are applied
    // as we go on processing transactions.
    core::ledgerview& view (mengine->view ());

    // this is a checkpoint with just the fees paid. if something goes wrong
    // with this transaction, we roll back to this ledger.
    core::ledgerview view_checkpoint (view);

    view.bumpseq (); // begin ledger variance.

    sle::pointer slecreator = mengine->entrycache (
        ltaccount_root, getaccountrootindex (mtxnaccountid));

    // additional checking for currency asset.
    // buy asset
    if (assetcurrency() == upayscurrency) {
        if (assetcurrency() == ugetscurrency || // asset for asset
            bsell)                              // tfsell set while buying asset
            return temdisabled;

        if (satakerpays < stamount(satakerpays.issue(), getconfig().asset_tx_min) || !satakerpays.ismathematicalinteger())
            return tembad_offer;

        if (upaysissuerid == mtxnaccountid || ugetsissuerid == mtxnaccountid) {
            m_journal.trace << "creating asset offer is not allowed for issuer";
            return temdisabled;
        }
    }
    // sell asset
    if (assetcurrency() == ugetscurrency) {
        if (!bsell) // tfsell not set while selling asset
            return temdisabled;

        if (satakergets < stamount(satakergets.issue(), getconfig().asset_tx_min) || !satakergets.ismathematicalinteger())
            return tembad_offer;
    }

    if (utxflags & tfoffercreatemask)
    {
        if (m_journal.debug) m_journal.debug <<
            "malformed transaction: invalid flags set.";

        terresult = teminvalid_flag;
    }
    else if (bimmediateorcancel && bfillorkill)
    {
        if (m_journal.debug) m_journal.debug <<
            "malformed transaction: both ioc and fok set.";

        terresult = teminvalid_flag;
    }
    else if (bhaveexpiration && !uexpiration)
    {
        m_journal.warning <<
            "malformed offer: bad expiration";

        terresult = tembad_expiration;
    }
    else if (satakerpays.isnative () && satakergets.isnative ())
    {
        m_journal.warning <<
            "malformed offer: xrp for xrp";

        terresult = tembad_offer;
    }
    else if (satakerpays <= zero || satakergets <= zero)
    {
        m_journal.warning <<
            "malformed offer: bad amount";

        terresult = tembad_offer;
    }
    else if (upayscurrency == ugetscurrency && upaysissuerid == ugetsissuerid)
    {
        m_journal.warning <<
            "malformed offer: redundant offer";

        terresult = temredundant;
    }
    // we don't allow a non-native currency to use the currency code vrp.
    else if (badcurrency() == upayscurrency || badcurrency() == ugetscurrency)
    {
        m_journal.warning <<
            "malformed offer: bad currency.";

        terresult = tembad_currency;
    }
    else if (satakerpays.isnative () != isnative(upaysissuerid) ||
             satakergets.isnative () != isnative(ugetsissuerid))
    {
        m_journal.warning <<
            "malformed offer: bad issuer";

        terresult = tembad_issuer;
    }
    else if (view.isglobalfrozen (upaysissuerid) || view.isglobalfrozen (ugetsissuerid))
    {
        m_journal.warning <<
            "offer involves frozen asset";

        terresult = tecfrozen;
    }
    else if (view.accountfunds (
        mtxnaccountid, satakergets, fhzero_if_frozen) <= zero)
    {
        m_journal.warning <<
            "delay: offers must be at least partially funded.";

        terresult = tecunfunded_offer;
    }
    // this can probably be simplified to make sure that you cancel sequences
    // before the transaction sequence number.
    else if (bhavecancel && (!ucancelsequence || uaccountsequencenext - 1 <= ucancelsequence))
    {
        if (m_journal.debug) m_journal.debug <<
            "uaccountsequencenext=" << uaccountsequencenext <<
            " uoffersequence=" << ucancelsequence;

        terresult = tembad_sequence;
    }

    if (terresult != tessuccess)
    {
        if (m_journal.debug) m_journal.debug <<
            "final terresult=" << transtoken (terresult);

        return terresult;
    }

    // process a cancellation request that's passed along with an offer.
    if ((terresult == tessuccess) && bhavecancel)
    {
        uint256 const ucancelindex (
            getofferindex (mtxnaccountid, ucancelsequence));
        sle::pointer slecancel = mengine->entrycache (ltoffer, ucancelindex);

        // it's not an error to not find the offer to cancel: it might have
        // been consumed or removed as we are processing.
        if (slecancel)
        {
            m_journal.warning <<
                "cancelling order with sequence " << ucancelsequence;

            terresult = view.offerdelete (slecancel);
        }
    }

    // expiration is defined in terms of the close time of the parent ledger,
    // because we definitively know the time that it closed but we do not
    // know the closing time of the ledger that is under construction.
    if (bhaveexpiration &&
        (mengine->getledger ()->getparentclosetimenc () >= uexpiration))
    {
        return tessuccess;
    }

    // make sure that we are authorized to hold what the taker will pay us.
    if (terresult == tessuccess && !satakerpays.isnative ())
        terresult = checkacceptasset (issue (upayscurrency, upaysissuerid));

    bool crossed = false;
    bool const bopenledger (mparams & tapopen_ledger);

    if (terresult == tessuccess)
    {
        // we reverse gets and pays because during offer crossing we are taking.
        core::amounts const taker_amount (satakergets, satakerpays);

        // the amount of the offer that we will need to place, after we finish
        // offer crossing processing. it may be equal to the original amount,
        // empty (fully crossed), or something in-between.
        core::amounts place_offer;

        std::tie(terresult, place_offer) = crossoffers (view, taker_amount);

        if (terresult == tecfailed_processing && bopenledger)
            terresult = telfailed_processing;

        if (terresult == tessuccess)
        {
            // we now need to reduce the offer by the cross flow. we reverse
            // in and out here, since during crossing we were takers.
            assert (satakerpays.getcurrency () == place_offer.out.getcurrency ());
            assert (satakerpays.getissuer () == place_offer.out.getissuer ());
            assert (satakergets.getcurrency () == place_offer.in.getcurrency ());
            assert (satakergets.getissuer () == place_offer.in.getissuer ());

            if (taker_amount != place_offer)
                crossed = true;

            if (m_journal.debug)
            {
                m_journal.debug << "offer crossing: " << transtoken (terresult);

                if (terresult == tessuccess)
                {
                    m_journal.debug <<
                        "    takerpays: " << satakerpays.getfulltext () <<
                        " -> " << place_offer.out.getfulltext ();
                    m_journal.debug <<
                        "    takergets: " << satakergets.getfulltext () <<
                        " -> " << place_offer.in.getfulltext ();
                }
            }

            satakerpays = place_offer.out;
            satakergets = place_offer.in;
        }
    }

    if (terresult != tessuccess)
    {
        m_journal.debug <<
            "final terresult=" << transtoken (terresult);

        return terresult;
    }

    if (m_journal.debug)
    {
        m_journal.debug <<
            "takeoffers: satakerpays=" <<satakerpays.getfulltext ();
        m_journal.debug <<
            "takeoffers: satakergets=" << satakergets.getfulltext ();
        m_journal.debug <<
            "takeoffers: mtxnaccountid=" <<
            to_string (mtxnaccountid);
        m_journal.debug <<
            "takeoffers:         funds=" << view.accountfunds (
            mtxnaccountid, satakergets, fhzero_if_frozen).getfulltext ();
    }

    if (satakerpays < zero || satakergets < zero)
    {
        // earlier, we verified that the amounts, as specified in the offer,
        // were not negative. that they are now suggests that something went
        // very wrong with offer crossing.
        m_journal.fatal << (crossed ? "partially consumed" : "full") <<
            " offer has negative component:" <<
            " pays=" << satakerpays.getfulltext () <<
            " gets=" << satakergets.getfulltext ();

        assert (satakerpays >= zero);
        assert (satakergets >= zero);
        return tefinternal;
    }

    if (bfillorkill && (satakerpays != zero || satakergets != zero))
    {
        // fill or kill and have leftovers.
        view.swapwith (view_checkpoint); // restore with just fees paid.
        return tessuccess;
    }

    // what the reserve would be if this offer was placed.
    auto const accountreserve (mengine->getledger ()->getreserve (
        slecreator->getfieldu32 (sfownercount) + 1));

    if (satakerpays == zero ||                // wants nothing more.
        satakergets == zero ||                // offering nothing more.
        bimmediateorcancel)                   // do not persist.
    {
        // complete as is.
    }
    else if (mpriorbalance.getnvalue () < accountreserve)
    {
        // if we are here, the signing account had an insufficient reserve
        // *prior* to our processing. we use the prior balance to simplify
        // client writing and make the user experience better.

        if (bopenledger) // ledger is not final, can vote no.
        {
            // hope for more reserve to come in or more offers to consume. if we
            // specified a local error this transaction will not be retried, so
            // specify a tec to distribute the transaction and allow it to be
            // retried. in particular, it may have been successful to a
            // degree (partially filled) and if it hasn't, it might succeed.
            terresult = tecinsuf_reserve_offer;
        }
        else if (!crossed)
        {
            // ledger is final, insufficent reserve to create offer, processed
            // nothing.
            terresult = tecinsuf_reserve_offer;
        }
        else
        {
            // ledger is final, insufficent reserve to create offer, processed
            // something.
            // consider the offer unfunded. treat as tessuccess.
        }
    }
    else
    {
        assert (satakerpays > zero);
        assert (satakergets > zero);

        // we need to place the remainder of the offer into its order book.
        if (m_journal.debug) m_journal.debug <<
            "offer not fully consumed:" <<
            " satakerpays=" << satakerpays.getfulltext () <<
            " satakergets=" << satakergets.getfulltext ();

        std::uint64_t uownernode;
        std::uint64_t ubooknode;
        uint256 udirectory;

        // add offer to owner's directory.
        terresult = view.diradd (uownernode,
            getownerdirindex (mtxnaccountid), uledgerindex,
            std::bind (
                &ledger::ownerdirdescriber, std::placeholders::_1,
                std::placeholders::_2, mtxnaccountid));

        if (tessuccess == terresult)
        {
            // update owner count.
            view.incrementownercount (slecreator);

            uint256 const ubookbase (getbookbase (
                {{upayscurrency, upaysissuerid},
                    {ugetscurrency, ugetsissuerid}}));

            if (m_journal.debug) m_journal.debug <<
                "adding to book: " << to_string (ubookbase) <<
                " : " << satakerpays.gethumancurrency () <<
                "/" << to_string (satakerpays.getissuer ()) <<
                " -> " << satakergets.gethumancurrency () <<
                "/" << to_string (satakergets.getissuer ());

            // we use the original rate to place the offer.
            udirectory = getqualityindex (ubookbase, urate);

            // add offer to order book.
            terresult = view.diradd (ubooknode, udirectory, uledgerindex,
                std::bind (
                    &ledger::qualitydirdescriber, std::placeholders::_1,
                    std::placeholders::_2, satakerpays.getcurrency (),
                    upaysissuerid, satakergets.getcurrency (),
                    ugetsissuerid, urate));
        }

        if (tessuccess == terresult)
        {
            if (m_journal.debug)
            {
                m_journal.debug <<
                    "sfaccount=" <<
                    to_string (mtxnaccountid);
                m_journal.debug <<
                    "upaysissuerid=" <<
                    to_string (upaysissuerid);
                m_journal.debug <<
                    "ugetsissuerid=" <<
                    to_string (ugetsissuerid);
                m_journal.debug <<
                    "satakerpays.isnative()=" <<
                    satakerpays.isnative ();
                m_journal.debug <<
                    "satakergets.isnative()=" <<
                    satakergets.isnative ();
                m_journal.debug <<
                    "upayscurrency=" <<
                    satakerpays.gethumancurrency ();
                m_journal.debug <<
                    "ugetscurrency=" <<
                    satakergets.gethumancurrency ();
            }

            sle::pointer sleoffer (mengine->entrycreate (ltoffer, uledgerindex));

            sleoffer->setfieldaccount (sfaccount, mtxnaccountid);
            sleoffer->setfieldu32 (sfsequence, usequence);
            sleoffer->setfieldh256 (sfbookdirectory, udirectory);
            sleoffer->setfieldamount (sftakerpays, satakerpays);
            sleoffer->setfieldamount (sftakergets, satakergets);
            sleoffer->setfieldu64 (sfownernode, uownernode);
            sleoffer->setfieldu64 (sfbooknode, ubooknode);

            if (uexpiration)
                sleoffer->setfieldu32 (sfexpiration, uexpiration);

            if (bpassive)
                sleoffer->setflag (lsfpassive);

            if (bsell)
                sleoffer->setflag (lsfsell);

            if (m_journal.debug) m_journal.debug <<
                "final terresult=" << transtoken (terresult) <<
                " sleoffer=" << sleoffer->getjson (0);
        }
    }

    if (terresult != tessuccess)
    {
        m_journal.debug <<
            "final terresult=" << transtoken (terresult);
    }

    return terresult;
}

//------------------------------------------------------------------------------

ter
transact_createoffer (sttx const& txn,
    transactionengineparams params, transactionengine* engine)
{
    // autobridging is performed only when the offer does not involve xrp
    bool autobridging =
        ! txn.getfieldamount (sftakerpays).isnative() &&
        ! txn.getfieldamount (sftakergets).isnative ();

    return createoffer (autobridging, txn, params, engine).apply ();
}

}
