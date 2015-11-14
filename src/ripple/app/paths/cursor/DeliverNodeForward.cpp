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
#include <ripple/app/paths/cursor/rippleliquidity.h>
#include <ripple/basics/log.h>

namespace ripple {
namespace path {

// for current offer, get input from deliver/limbo and output to next account or
// deliver for next offers.
//
// <-- node.safwddeliver: for forwardliquidityforaccount to know
//                        how much went through
// --> node.sarevdeliver: do not exceed.

ter pathcursor::delivernodeforward (
    account const& uinaccountid,    // --> input owner's account.
    stamount const& sainreq,        // --> amount to deliver.
    stamount& sainact,              // <-- amount delivered, this invocation.
    stamount& sainfees) const       // <-- fees charged, this invocation.
{
    ter resultcode   = tessuccess;

    // don't deliver more than wanted.
    // zeroed in reverse pass.
    node().directory.restart(multiquality_);

    sainact.clear (sainreq);
    sainfees.clear (sainreq);

    int loopcount = 0;

    // xxx perhaps make sure do not exceed node().sarevdeliver as another way to
    // stop?
    while (resultcode == tessuccess && sainact + sainfees < sainreq)
    {
        // did not spend all inbound deliver funds.
        if (++loopcount > calc_node_deliver_max_loops)
        {
            writelog (lswarning, ripplecalc)
                << "delivernodeforward: max loops cndf";
            return telfailed_processing;
        }

        // determine values for pass to adjust sainact, sainfees, and
        // node().safwddeliver.
        advancenode (sainact, false);

        // if needed, advance to next funded offer.

        if (resultcode != tessuccess)
        {
        }
        else if (!node().offerindex_)
        {
            writelog (lswarning, ripplecalc)
                << "delivernodeforward: internal error: ran out of offers.";
            return telfailed_processing;
        }
        else if (resultcode == tessuccess)
        {
            // doesn't charge input. input funds are in limbo.
            // there's no fee if we're transferring xrp, if the sender is the
            // issuer, or if the receiver is the issuer.
            bool nofee = isnative (previousnode().issue_)
                || uinaccountid == previousnode().issue_.account
                || node().offerowneraccount_ == previousnode().issue_.account;
            const stamount sainfeerate = nofee ? saone
                : previousnode().transferrate_;  // transfer rate of issuer.

            // first calculate assuming no output fees: sainpassact,
            // sainpassfees, saoutpassact.

            // offer maximum out - limited by funds with out fees.
            auto saoutfunded = std::min (
                node().saofferfunds, node().satakergets);

            // offer maximum out - limit by most to deliver.
            auto saoutpassfunded = std::min (
                saoutfunded,
                node().sarevdeliver - node().safwddeliver);

            // offer maximum in - limited by by payout.
            auto sainfunded = mulround (
                saoutpassfunded,
                node().saofrrate,
                node().satakerpays,
                true);

            // offer maximum in with fees.
            auto saintotal = mulround (sainfunded, sainfeerate, true);
            auto sainremaining = sainreq - sainact - sainfees;

            if (sainremaining < zero)
                sainremaining.clear();

            // in limited by remaining.
            auto sainsum = std::min (saintotal, sainremaining);

            // in without fees.
            auto sainpassact = std::min (
                node().satakerpays, divround (
                    sainsum, sainfeerate, true));

            // out limited by in remaining.
            auto outpass = divround (
                sainpassact, node().saofrrate, node().satakergets, true);
            stamount saoutpassmax    = std::min (saoutpassfunded, outpass);

            stamount sainpassfeesmax = sainsum - sainpassact;

            // will be determined by next node().
            stamount saoutpassact;

            // will be determined by adjusted sainpassact.
            stamount sainpassfees;

            writelog (lstrace, ripplecalc)
                << "delivernodeforward:"
                << " nodeindex_=" << nodeindex_
                << " saoutfunded=" << saoutfunded
                << " saoutpassfunded=" << saoutpassfunded
                << " node().saofferfunds=" << node().saofferfunds
                << " node().satakergets=" << node().satakergets
                << " sainreq=" << sainreq
                << " sainact=" << sainact
                << " sainfees=" << sainfees
                << " sainfunded=" << sainfunded
                << " saintotal=" << saintotal
                << " sainsum=" << sainsum
                << " sainpassact=" << sainpassact
                << " saoutpassmax=" << saoutpassmax;

            // fixme: we remove an offer if we didn't want anything out of it?
            if (!node().satakerpays || sainsum <= zero)
            {
                writelog (lsdebug, ripplecalc)
                    << "delivernodeforward: microscopic offer unfunded.";

                // after math offer is effectively unfunded.
                pathstate_.unfundedoffers().push_back (node().offerindex_);
                node().bentryadvance = true;
                continue;
            }

            if (!sainfunded)
            {
                // previous check should catch this.
                writelog (lswarning, ripplecalc)
                    << "delivernodeforward: unreachable reached";

                // after math offer is effectively unfunded.
                pathstate_.unfundedoffers().push_back (node().offerindex_);
                node().bentryadvance = true;
                continue;
            }

            if (!isnative(nextnode().account_))
            {
                // ? --> offer --> account
                // input fees: vary based upon the consumed offer's owner.
                // output fees: none as xrp or the destination account is the
                // issuer.

                saoutpassact = saoutpassmax;
                sainpassfees = sainpassfeesmax;

                writelog (lstrace, ripplecalc)
                    << "delivernodeforward: ? --> offer --> account:"
                    << " offerowneraccount_="
                    << node().offerowneraccount_
                    << " nextnode().account_="
                    << nextnode().account_
                    << " saoutpassact=" << saoutpassact
                    << " saoutfunded=" << saoutfunded;

                // output: debit offer owner, send xrp or non-xpr to next
                // account.
                resultcode = ledger().accountsend (
                    node().offerowneraccount_,
                    nextnode().account_,
                    saoutpassact);

                if (resultcode != tessuccess)
                    break;
            }
            else
            {
                // ? --> offer --> offer
                //
                // offer to offer means current order book's output currency and
                // issuer match next order book's input current and issuer.
                //
                // output fees: possible if issuer has fees and is not on either
                // side.
                stamount saoutpassfees;

                // output fees vary as the next nodes offer owners may vary.
                // therefore, immediately push through output for current offer.
                resultcode = increment().delivernodeforward (
                    node().offerowneraccount_,  // --> current holder.
                    saoutpassmax,             // --> amount available.
                    saoutpassact,             // <-- amount delivered.
                    saoutpassfees);           // <-- fees charged.

                if (resultcode != tessuccess)
                    break;

                if (saoutpassact == saoutpassmax)
                {
                    // no fees and entire output amount.

                    sainpassfees = sainpassfeesmax;
                }
                else
                {
                    // fraction of output amount.
                    // output fees are paid by offer owner and not passed to
                    // previous.

                    assert (saoutpassact < saoutpassmax);
                    auto inpassact = mulround (
                        saoutpassact, node().saofrrate, sainreq, true);
                    sainpassact = std::min (node().satakerpays, inpassact);
                    auto inpassfees = mulround (
                        sainpassact, sainfeerate, true);
                    sainpassfees    = std::min (sainpassfeesmax, inpassfees);
                }

                // do outbound debiting.
                // send to issuer/limbo total amount including fees (issuer gets
                // fees).
                auto const& id = isxrp(node().issue_) ?
                    xrpaccount() : (isvbc(node().issue_) ? vbcaccount() : node().issue_.account);
                auto outpasstotal = saoutpassact + saoutpassfees;
                ledger().accountsend (
                    node().offerowneraccount_,
                    id,
                    outpasstotal);

                writelog (lstrace, ripplecalc)
                    << "delivernodeforward: ? --> offer --> offer:"
                    << " saoutpassact=" << saoutpassact
                    << " saoutpassfees=" << saoutpassfees;
            }

            writelog (lstrace, ripplecalc)
                << "delivernodeforward: "
                << " nodeindex_=" << nodeindex_
                << " node().satakergets=" << node().satakergets
                << " node().satakerpays=" << node().satakerpays
                << " sainpassact=" << sainpassact
                << " sainpassfees=" << sainpassfees
                << " saoutpassact=" << saoutpassact
                << " saoutfunded=" << saoutfunded;

            // funds were spent.
            node().bfundsdirty = true;

            // do inbound crediting.
            //
            // credit offer owner from in issuer/limbo (input transfer fees left
            // with owner).  don't attempt to have someone credit themselves, it
            // is redundant.
            if (isnative (previousnode().issue_.currency)
                || uinaccountid != node().offerowneraccount_)
            {
				auto id = !isxrp(previousnode().issue_.currency) ?
                        (isvbc(previousnode().issue_.currency) ? vbcaccount() : uinaccountid) : xrpaccount();

                resultcode = ledger().accountsend (
                    id,
                    node().offerowneraccount_,
                    sainpassact);

                if (resultcode != tessuccess)
                    break;
            }

            // adjust offer.
            //
            // fees are considered paid from a seperate budget and are not named
            // in the offer.
            stamount satakergetsnew  = node().satakergets - saoutpassact;
            stamount satakerpaysnew  = node().satakerpays - sainpassact;

            if (satakerpaysnew < zero || satakergetsnew < zero)
            {
                writelog (lswarning, ripplecalc)
                    << "delivernodeforward: negative:"
                    << " satakerpaysnew=" << satakerpaysnew
                    << " satakergetsnew=" << satakergetsnew;

                resultcode = telfailed_processing;
                break;
            }

            node().sleoffer->setfieldamount (sftakergets, satakergetsnew);
            node().sleoffer->setfieldamount (sftakerpays, satakerpaysnew);

            ledger().entrymodify (node().sleoffer);

            if (saoutpassact == saoutfunded || satakergetsnew == zero)
            {
                // offer became unfunded.

                writelog (lswarning, ripplecalc)
                    << "delivernodeforward: unfunded:"
                    << " saoutpassact=" << saoutpassact
                    << " saoutfunded=" << saoutfunded;

                pathstate_.unfundedoffers().push_back (node().offerindex_);
                node().bentryadvance   = true;
            }
            else
            {
                condlog (saoutpassact >= saoutfunded, lswarning, ripplecalc)
                    << "delivernodeforward: too much:"
                    << " saoutpassact=" << saoutpassact
                    << " saoutfunded=" << saoutfunded;

                assert (saoutpassact < saoutfunded);
            }

            sainact += sainpassact;
            sainfees += sainpassfees;

            // adjust amount available to next node().
            node().safwddeliver = std::min (node().sarevdeliver,
                                        node().safwddeliver + saoutpassact);
        }
    }

    writelog (lstrace, ripplecalc)
        << "delivernodeforward<"
        << " nodeindex_=" << nodeindex_
        << " sainact=" << sainact
        << " sainfees=" << sainfees;

    return resultcode;
}

} // path
} // ripple
