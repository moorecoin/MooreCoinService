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
#include <ripple/app/book/quality.h>
#include <ripple/app/paths/credit.h>
#include <ripple/app/paths/cursor/rippleliquidity.h>
#include <ripple/basics/log.h>

namespace ripple {
namespace path {

// calculate saprvredeemreq, saprvissuereq, saprvdeliver from sacur, based on
// required deliverable, propagate redeem, issue (for accounts) and deliver
// requests (for order books) to the previous node.
//
// inflate amount requested by required fees.
// reedems are limited based on ious previous has on hand.
// issues are limited based on credit limits and amount owed.
//
// currency cannot be xrp because we are rippling.
//
// no permanent account balance adjustments as we don't know how much is going
// to actually be pushed through yet - changes are only in the scratch pad
// ledger.
//
// <-- tessuccess or tecpath_dry

ter pathcursor::reverseliquidityforaccount () const
{
    ter terresult = tessuccess;
    auto const lastnodeindex = nodesize () - 1;
    auto const isfinalnode = (nodeindex_ == lastnodeindex);

    // 0 quality means none has yet been determined.
    std::uint64_t uratemax = 0
;
    // current is allowed to redeem to next.
    const bool previousnodeisaccount = !nodeindex_ ||
            previousnode().isaccount();

    const bool nextnodeisaccount = isfinalnode || nextnode().isaccount();

    account const& previousaccountid = previousnodeisaccount
        ? previousnode().account_ : node().account_;
    account const& nextaccountid = nextnodeisaccount ? nextnode().account_
        : node().account_;   // offers are always issue.

    // this is the quality from from the previous node to this one.
    const std::uint32_t uqualityin
         = (nodeindex_ != 0)
            ? quality_in (ledger(),
                node().account_,
                previousaccountid,
                node().issue_.currency)
            : quality_one;

    // and this is the quality from the next one to this one.
    const std::uint32_t uqualityout
        = (nodeindex_ != lastnodeindex)
            ? quality_out (ledger(),
                node().account_,
                nextaccountid,
                node().issue_.currency)
            : quality_one;

    // for previousnodeisaccount:
    // previous account is already owed.
    const stamount saprvowed = (previousnodeisaccount && nodeindex_ != 0)
        ? creditbalance (ledger(),
            node().account_,
            previousaccountid,
            node().issue_.currency)
        : stamount (node().issue_);

    // the limit amount that the previous account may owe.
    const stamount saprvlimit = (previousnodeisaccount && nodeindex_ != 0)
        ? creditlimit (ledger(),
            node().account_,
            previousaccountid,
            node().issue_.currency)
        : stamount (node().issue_);

    // next account is owed.
    const stamount sanxtowed = (nextnodeisaccount && nodeindex_ != lastnodeindex)
        ? creditbalance (ledger(),
            node().account_,
            nextaccountid,
            node().issue_.currency)
        : stamount (node().issue_);

    writelog (lstrace, ripplecalc)
        << "reverseliquidityforaccount>"
        << " nodeindex_=" << nodeindex_ << "/" << lastnodeindex
        << " previousaccountid=" << previousaccountid
        << " node.account_=" << node().account_
        << " nextaccountid=" << nextaccountid
        << " currency=" << node().issue_.currency
        << " uqualityin=" << uqualityin
        << " uqualityout=" << uqualityout
        << " saprvowed=" << saprvowed
        << " saprvlimit=" << saprvlimit;

    // requests are computed to be the maximum flow possible.
    // previous can redeem the owed ious it holds.
    const stamount saprvredeemreq  = (saprvowed > zero)
        ? saprvowed
        : stamount (saprvowed.issue ());

    // previous can issue up to limit minus whatever portion of limit already
    // used (not including redeemable amount) - another "maximum flow".
    const stamount saprvissuereq = (saprvowed < zero)
        ? saprvlimit + saprvowed : saprvlimit;

    // precompute these values in case we have an order book.
    auto delivercurrency = previousnode().sarevdeliver.getcurrency ();
    const stamount saprvdeliverreq (
        {delivercurrency, previousnode().sarevdeliver.getissuer ()}, -1);
    // -1 means unlimited delivery.

    // set to zero, because we're trying to hit the previous node.
    auto sacurredeemact = node().sarevredeem.zeroed();

    // track the amount we actually redeem.
    auto sacurissueact = node().sarevissue.zeroed();

    // for !nextnodeisaccount
    auto sacurdeliveract  = node().sarevdeliver.zeroed();

    writelog (lstrace, ripplecalc)
        << "reverseliquidityforaccount:"
        << " saprvredeemreq:" << saprvredeemreq
        << " saprvissuereq:" << saprvissuereq
        << " previousnode.sarevdeliver:" << previousnode().sarevdeliver
        << " saprvdeliverreq:" << saprvdeliverreq
        << " node.sarevredeem:" << node().sarevredeem
        << " node.sarevissue:" << node().sarevissue
        << " sanxtowed:" << sanxtowed;

    // vfalco-fixme this generates errors
    //writelog (lstrace, ripplecalc) << pathstate_.getjson ();

    // current redeem req can't be more than ious on hand.
    assert (!node().sarevredeem || -sanxtowed >= node().sarevredeem);
    assert (!node().sarevissue  // if not issuing, fine.
            || sanxtowed >= zero
            // sanxtowed >= 0: sender not holding next ious, sanxtowed < 0:
            // sender holding next ious.
            || -sanxtowed == node().sarevredeem);
    // if issue req, then redeem req must consume all owed.

    if (nodeindex_ == 0)
    {
        // ^ --> account -->  account|offer
        // nothing to do, there is no previous to adjust.
        //
        // todo(tom): we could have skipped all that setup and just left
        // or even just never call this whole routine on nodeindex_ = 0!
    }

    // the next four cases correspond to the table at the bottom of this wiki
    // page section: https://ripple.com/wiki/transit_fees#implementation
    else if (previousnodeisaccount && nextnodeisaccount)
    {
        if (isfinalnode)
        {
            // account --> account --> $
            // overall deliverable.
            const stamount sacurwantedreq = std::min (
                pathstate_.outreq() - pathstate_.outact(),
                saprvlimit + saprvowed);
            auto sacurwantedact = sacurwantedreq.zeroed ();

            writelog (lstrace, ripplecalc)
                << "reverseliquidityforaccount: account --> "
                << "account --> $ :"
                << " sacurwantedreq=" << sacurwantedreq;

            // calculate redeem
            if (saprvredeemreq) // previous has ious to redeem.
            {
                // redeem your own ious at 1:1

                sacurwantedact = std::min (saprvredeemreq, sacurwantedreq);
                previousnode().sarevredeem = sacurwantedact;

                uratemax = stamount::urateone;

                writelog (lstrace, ripplecalc)
                    << "reverseliquidityforaccount: redeem at 1:1"
                    << " saprvredeemreq=" << saprvredeemreq
                    << " (available) previousnode.sarevredeem="
                    << previousnode().sarevredeem
                    << " uratemax="
                    << amountfromrate (uratemax).gettext ();
            }
            else
            {
                previousnode().sarevredeem.clear (saprvredeemreq);
            }

            // calculate issuing.
            previousnode().sarevissue.clear (saprvissuereq);

            if (sacurwantedreq != sacurwantedact // need more.
                && saprvissuereq)  // will accept ious from previous.
            {
                // rate: quality in : 1.0

                // if we previously redeemed and this has a poorer rate, this
                // won't be included the current increment.
                rippleliquidity (
                    ripplecalc_,
                    uqualityin,
                    quality_one,
                    saprvissuereq,
                    sacurwantedreq,
                    previousnode().sarevissue,
                    sacurwantedact,
                    uratemax);

                writelog (lstrace, ripplecalc)
                    << "reverseliquidityforaccount: issuing: rate: "
                    << "quality in : 1.0"
                    << " previousnode.sarevissue:" << previousnode().sarevissue
                    << " sacurwantedact:" << sacurwantedact;
            }

            if (!sacurwantedact)
            {
                // must have processed something.
                terresult   = tecpath_dry;
            }
        }
        else
        {
            // not final node.
            // account --> account --> account
            previousnode().sarevredeem.clear (saprvredeemreq);
            previousnode().sarevissue.clear (saprvissuereq);

            // redeem (part 1) -> redeem
            if (node().sarevredeem
                // next wants ious redeemed from current account.
                && saprvredeemreq)
                // previous has ious to redeem to the current account.
            {
                // todo(tom): add english.
                // rate : 1.0 : quality out - we must accept our own ious
                // as 1:1.
                rippleliquidity (
                    ripplecalc_,
                    quality_one,
                    uqualityout,
                    saprvredeemreq,
                    node().sarevredeem,
                    previousnode().sarevredeem,
                    sacurredeemact,
                    uratemax);

                writelog (lstrace, ripplecalc)
                    << "reverseliquidityforaccount: "
                    << "rate : 1.0 : quality out"
                    << " previousnode.sarevredeem:" << previousnode().sarevredeem
                    << " sacurredeemact:" << sacurredeemact;
            }

            // issue (part 1) -> redeem
            if (node().sarevredeem != sacurredeemact
                // the current node has more ious to redeem.
                && previousnode().sarevredeem == saprvredeemreq)
                // the previous node has no ious to redeem remaining, so issues.
            {
                // rate: quality in : quality out
                rippleliquidity (
                    ripplecalc_,
                    uqualityin,
                    uqualityout,
                    saprvissuereq,
                    node().sarevredeem,
                    previousnode().sarevissue,
                    sacurredeemact,
                    uratemax);

                writelog (lstrace, ripplecalc)
                    << "reverseliquidityforaccount: "
                    << "rate: quality in : quality out:"
                    << " previousnode.sarevissue:" << previousnode().sarevissue
                    << " sacurredeemact:" << sacurredeemact;
            }

            // redeem (part 2) -> issue.
            if (node().sarevissue   // next wants ious issued.
                // todo(tom): this condition seems redundant.
                && sacurredeemact == node().sarevredeem
                // can only issue if completed redeeming.
                && previousnode().sarevredeem != saprvredeemreq)
                // did not complete redeeming previous ious.
            {
                // rate : 1.0 : transfer_rate
                rippleliquidity (
                    ripplecalc_,
                    quality_one,
                    rippletransferrate (ledger(), node().account_),
                    saprvredeemreq,
                    node().sarevissue,
                    previousnode().sarevredeem,
                    sacurissueact,
                    uratemax);

                writelog (lsdebug, ripplecalc)
                    << "reverseliquidityforaccount: "
                    << "rate : 1.0 : transfer_rate:"
                    << " previousnode.sarevredeem:" << previousnode().sarevredeem
                    << " sacurissueact:" << sacurissueact;
            }

            // issue (part 2) -> issue
            if (node().sarevissue != sacurissueact
                // need wants more ious issued.
                && sacurredeemact == node().sarevredeem
                // can only issue if completed redeeming.
                && saprvredeemreq == previousnode().sarevredeem
                // previously redeemed all owed ious.
                && saprvissuereq)
                // previous can issue.
            {
                // rate: quality in : 1.0
                rippleliquidity (
                    ripplecalc_,
                    uqualityin,
                    quality_one,
                    saprvissuereq,
                    node().sarevissue,
                    previousnode().sarevissue,
                    sacurissueact,
                    uratemax);

                writelog (lstrace, ripplecalc)
                    << "reverseliquidityforaccount: "
                    << "rate: quality in : 1.0:"
                    << " previousnode.sarevissue:" << previousnode().sarevissue
                    << " sacurissueact:" << sacurissueact;
            }

            if (!sacurredeemact && !sacurissueact)
            {
                // did not make progress.
                terresult = tecpath_dry;
            }

            writelog (lstrace, ripplecalc)
                << "reverseliquidityforaccount: "
                << "^|account --> account --> account :"
                << " node.sarevredeem:" << node().sarevredeem
                << " node.sarevissue:" << node().sarevissue
                << " saprvowed:" << saprvowed
                << " sacurredeemact:" << sacurredeemact
                << " sacurissueact:" << sacurissueact;
        }
    }
    else if (previousnodeisaccount && !nextnodeisaccount)
    {
        // account --> account --> offer
        // note: deliver is always issue as account is the issuer for the offer
        // input.
        writelog (lstrace, ripplecalc)
            << "reverseliquidityforaccount: "
            << "account --> account --> offer";

        previousnode().sarevredeem.clear (saprvredeemreq);
        previousnode().sarevissue.clear (saprvissuereq);

        // we have three cases: the nxt offer can be owned by current account,
        // previous account or some third party account.
        //
        // also, the current account may or may not have a redeemable balance
        // with the account for the next offer, so we don't yet know if we're
        // redeeming or issuing.
        //
        // todo(tom): make sure deliver was cleared, or check actual is zero.
        // redeem -> deliver/issue.
        if (saprvowed > zero                    // previous has ious to redeem.
            && node().sarevdeliver)                 // need some issued.
        {
            // rate : 1.0 : transfer_rate
            rippleliquidity (
                ripplecalc_,
                quality_one,
                rippletransferrate (ledger(), node().account_),
                saprvredeemreq,
                node().sarevdeliver,
                previousnode().sarevredeem,
                sacurdeliveract,
                uratemax);
        }

        // issue -> deliver/issue
        if (saprvredeemreq == previousnode().sarevredeem
            // previously redeemed all owed.
            && node().sarevdeliver != sacurdeliveract)  // still need some issued.
        {
            // rate: quality in : 1.0
            rippleliquidity (
                ripplecalc_,
                uqualityin,
                quality_one,
                saprvissuereq,
                node().sarevdeliver,
                previousnode().sarevissue,
                sacurdeliveract,
                uratemax);
        }

        if (!sacurdeliveract)
        {
            // must want something.
            terresult   = tecpath_dry;
        }

        writelog (lstrace, ripplecalc)
            << "reverseliquidityforaccount: "
            << " node.sarevdeliver:" << node().sarevdeliver
            << " sacurdeliveract:" << sacurdeliveract
            << " saprvowed:" << saprvowed;
    }
    else if (!previousnodeisaccount && nextnodeisaccount)
    {
        if (isfinalnode)
        {
            // offer --> account --> $
            // previous is an offer, no limit: redeem own ious.
            //
            // this is the final node; we can't look to the right to get values;
            // we have to go up to get the out value for the entire path state.
            stamount const& sacurwantedreq  =
                    pathstate_.outreq() - pathstate_.outact();
            stamount sacurwantedact = sacurwantedreq.zeroed();

            writelog (lstrace, ripplecalc)
                << "reverseliquidityforaccount: "
                << "offer --> account --> $ :"
                << " sacurwantedreq:" << sacurwantedreq
                << " saoutact:" << pathstate_.outact()
                << " saoutreq:" << pathstate_.outreq();

            if (sacurwantedreq <= zero)
            {
                // temporary emergency fix
                //
                // todo(tom): why can't sacurwantedreq be -1 if you want to
                // compute maximum liquidity?  this might be unimplemented
                // functionality.  todo(tom): should the same check appear in
                // other paths or even be pulled up?
                writelog (lsfatal, ripplecalc) << "curwantreq was not positive";
                return tefexception;
            }

            assert (sacurwantedreq > zero); // fixme: we got one of these
            // the previous node is an offer; we are receiving our own currency.

            // the previous order book's entries might hold our issuances; might
            // not hold our issuances; might be our own offer.
            //
            // assume the worst case, the case which costs the most to go
            // through, which is that it is not our own offer or our own
            // issuances.  later on the forward pass we may be able to do
            // better.
            //
            // todo: this comment applies generally to this section - move it up
            // to a document.

            // rate: quality in : 1.0
            rippleliquidity (
                ripplecalc_,
                uqualityin,
                quality_one,
                saprvdeliverreq,
                sacurwantedreq,
                previousnode().sarevdeliver,
                sacurwantedact,
                uratemax);

            if (!sacurwantedact)
            {
                // must have processed something.
                terresult   = tecpath_dry;
            }

            writelog (lstrace, ripplecalc)
                << "reverseliquidityforaccount:"
                << " previousnode().sarevdeliver:" << previousnode().sarevdeliver
                << " saprvdeliverreq:" << saprvdeliverreq
                << " sacurwantedact:" << sacurwantedact
                << " sacurwantedreq:" << sacurwantedreq;
        }
        else
        {
            // offer --> account --> account
            // note: offer is always delivering(redeeming) as account is issuer.
            writelog (lstrace, ripplecalc)
                << "reverseliquidityforaccount: "
                << "offer --> account --> account :"
                << " node.sarevredeem:" << node().sarevredeem
                << " node.sarevissue:" << node().sarevissue;

            // deliver -> redeem
            // todo(tom): now we have more checking in noderipple, these checks
            // might be redundant.
            if (node().sarevredeem)  // next wants us to redeem.
            {
                // cur holds ious from the account to the right, the nxt
                // account.  if someone is making the current account get rid of
                // the nxt account's ious, then charge the input for quality
                // out.
                //
                // rate : 1.0 : quality out
                rippleliquidity (
                    ripplecalc_,
                    quality_one,
                    uqualityout,
                    saprvdeliverreq,
                    node().sarevredeem,
                    previousnode().sarevdeliver,
                    sacurredeemact,
                    uratemax);
            }

            // deliver -> issue.
            if (node().sarevredeem == sacurredeemact
                // can only issue if previously redeemed all.
                && node().sarevissue)
                // need some issued.
            {
                // rate : 1.0 : transfer_rate
                rippleliquidity (
                    ripplecalc_,
                    quality_one,
                    rippletransferrate (ledger(), node().account_),
                    saprvdeliverreq,
                    node().sarevissue,
                    previousnode().sarevdeliver,
                    sacurissueact,
                    uratemax);
            }

            writelog (lstrace, ripplecalc)
                << "reverseliquidityforaccount:"
                << " sacurredeemact:" << sacurredeemact
                << " node.sarevredeem:" << node().sarevredeem
                << " previousnode.sarevdeliver:" << previousnode().sarevdeliver
                << " node.sarevissue:" << node().sarevissue;

            if (!previousnode().sarevdeliver)
            {
                // must want something.
                terresult   = tecpath_dry;
            }
        }
    }
    else
    {
        // offer --> account --> offer
        // deliver/redeem -> deliver/issue.
        writelog (lstrace, ripplecalc)
            << "reverseliquidityforaccount: offer --> account --> offer";

        // rate : 1.0 : transfer_rate
        rippleliquidity (
            ripplecalc_,
            quality_one,
            rippletransferrate (ledger(), node().account_),
            saprvdeliverreq,
            node().sarevdeliver,
            previousnode().sarevdeliver,
            sacurdeliveract,
            uratemax);

        if (!sacurdeliveract)
        {
            // must want something.
            terresult   = tecpath_dry;
        }
    }

    return terresult;
}

} // path
} // ripple
