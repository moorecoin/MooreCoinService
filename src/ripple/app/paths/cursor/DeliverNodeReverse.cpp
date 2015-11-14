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

// at the right most node of a list of consecutive offer nodes, given the amount
// requested to be delivered, push towards the left nodes the amount requested
// for the right nodes so we can compute how much to deliver from the source.
//
// between offer nodes, the fee charged may vary.  therefore, process one
// inbound offer at a time.  propagate the inbound offer's requirements to the
// previous node.  the previous node adjusts the amount output and the amount
// spent on fees.  continue processing until the request is satisified as long
// as the rate does not increase past the initial rate.

// to deliver from an order book, when computing
ter pathcursor::delivernodereverse (
    account const& uoutaccountid,  // --> output owner's account.
    stamount const& saoutreq,      // --> funds requested to be
                                   // delivered for an increment.
    stamount& saoutact) const      // <-- funds actually delivered for an
                                   // increment.
{
    ter resultcode   = tessuccess;

    node().directory.restart(multiquality_);

    // accumulation of what the previous node must deliver.
    // possible optimization: note this gets zeroed on each increment, ideally
    // only on first increment, then it could be a limit on the forward pass.
    saoutact.clear (saoutreq);

    writelog (lstrace, ripplecalc)
        << "delivernodereverse>"
        << " saoutact=" << saoutact
        << " saoutreq=" << saoutreq
        << " saprvdlvreq=" << previousnode().sarevdeliver;

    assert (saoutreq != zero);

    int loopcount = 0;

    // while we did not deliver as much as requested:
    while (saoutact < saoutreq)
    {
        if (++loopcount > calc_node_deliver_max_loops)
        {
            writelog (lsfatal, ripplecalc) << "loop count exceeded";
            return telfailed_processing;
        }

        resultcode = advancenode (saoutact, true);
        // if needed, advance to next funded offer.

        if (resultcode != tessuccess || !node().offerindex_)
            // error or out of offers.
            break;

        auto const hasfee = node().offerowneraccount_ == node().issue_.account
            || uoutaccountid == node().issue_.account;
        // issuer sending or receiving.

        const stamount saoutfeerate = hasfee
            ? saone             // no fee.
            : node().transferrate_;   // transfer rate of issuer.

        writelog (lstrace, ripplecalc)
            << "delivernodereverse:"
            << " offerowneraccount_="
            << node().offerowneraccount_
            << " uoutaccountid="
            << uoutaccountid
            << " node().issue_.account="
            << node().issue_.account
            << " node().transferrate_=" << node().transferrate_
            << " saoutfeerate=" << saoutfeerate;

        if (multiquality_)
        {
            // in multi-quality mode, ignore rate.
        }
        else if (!node().saratemax)
        {
            // set initial rate.
            node().saratemax = saoutfeerate;

            writelog (lstrace, ripplecalc)
                << "delivernodereverse: set initial rate:"
                << " node().saratemax=" << node().saratemax
                << " saoutfeerate=" << saoutfeerate;
        }
        else if (saoutfeerate > node().saratemax)
        {
            // offer exceeds initial rate.
            writelog (lstrace, ripplecalc)
                << "delivernodereverse: offer exceeds initial rate:"
                << " node().saratemax=" << node().saratemax
                << " saoutfeerate=" << saoutfeerate;

            break;  // done. don't bother looking for smaller transferrates.
        }
        else if (saoutfeerate < node().saratemax)
        {
            // reducing rate. additional offers will only considered for this
            // increment if they are at least this good.
            //
            // at this point, the overall rate is reducing, while the overall
            // rate is not saoutfeerate, it would be wrong to add anything with
            // a rate above saoutfeerate.
            //
            // the rate would be reduced if the current offer was from the
            // issuer and the previous offer wasn't.

            node().saratemax   = saoutfeerate;

            writelog (lstrace, ripplecalc)
                << "delivernodereverse: reducing rate:"
                << " node().saratemax=" << node().saratemax;
        }

        // amount that goes to the taker.
        stamount saoutpassreq = std::min (
            std::min (node().saofferfunds, node().satakergets),
            saoutreq - saoutact);

        // maximum out - assuming no out fees.
        stamount saoutpassact = saoutpassreq;

        // amount charged to the offer owner.
        //
        // the fee goes to issuer. the fee is paid by offer owner and not passed
        // as a cost to taker.
        //
        // round down: prefer liquidity rather than microscopic fees.
        stamount saoutplusfees   = mulround (
            saoutpassact, saoutfeerate, false);
        // offer out with fees.

        writelog (lstrace, ripplecalc)
            << "delivernodereverse:"
            << " saoutreq=" << saoutreq
            << " saoutact=" << saoutact
            << " node().satakergets=" << node().satakergets
            << " saoutpassact=" << saoutpassact
            << " saoutplusfees=" << saoutplusfees
            << " node().saofferfunds=" << node().saofferfunds;

        if (saoutplusfees > node().saofferfunds)
        {
            // offer owner can not cover all fees, compute saoutpassact based on
            // node().saofferfunds.
            saoutplusfees   = node().saofferfunds;

            // round up: prefer liquidity rather than microscopic fees. but,
            // limit by requested.
            auto fee = divround (saoutplusfees, saoutfeerate, true);
            saoutpassact = std::min (saoutpassreq, fee);

            writelog (lstrace, ripplecalc)
                << "delivernodereverse: total exceeds fees:"
                << " saoutpassact=" << saoutpassact
                << " saoutplusfees=" << saoutplusfees
                << " node().saofferfunds=" << node().saofferfunds;
        }

        // compute portion of input needed to cover actual output.
        auto outputfee = mulround (
            saoutpassact, node().saofrrate, node().satakerpays, true);
        stamount sainpassreq = std::min (node().satakerpays, outputfee);
        stamount sainpassact;

        writelog (lstrace, ripplecalc)
            << "delivernodereverse:"
            << " outputfee=" << outputfee
            << " sainpassreq=" << sainpassreq
            << " node().saofrrate=" << node().saofrrate
            << " saoutpassact=" << saoutpassact
            << " saoutplusfees=" << saoutplusfees;

        if (!sainpassreq) // fixme: this is bogus
        {
            // after rounding did not want anything.
            writelog (lsdebug, ripplecalc)
                << "delivernodereverse: micro offer is unfunded.";

            node().bentryadvance   = true;
            continue;
        }
        // find out input amount actually available at current rate.
        else if (!isnative(previousnode().account_))
        {
            // account --> offer --> ?
            // due to node expansion, previous is guaranteed to be the issuer.
            //
            // previous is the issuer and receiver is an offer, so no fee or
            // quality.
            //
            // previous is the issuer and has unlimited funds.
            //
            // offer owner is obtaining ious via an offer, so credit line limits
            // are ignored.  as limits are ignored, don't need to adjust
            // previous account's balance.

            sainpassact = sainpassreq;

            writelog (lstrace, ripplecalc)
                << "delivernodereverse: account --> offer --> ? :"
                << " sainpassact=" << sainpassact;
        }
        else
        {
            // offer --> offer --> ?
            // compute in previous offer node how much could come in.

            // todo(tom): fix nasty recursion here!
            resultcode = increment(-1).delivernodereverse(
                node().offerowneraccount_,
                sainpassreq,
                sainpassact);

            writelog (lstrace, ripplecalc)
                << "delivernodereverse: offer --> offer --> ? :"
                << " sainpassact=" << sainpassact;
        }

        if (resultcode != tessuccess)
            break;

        if (sainpassact < sainpassreq)
        {
            // adjust output to conform to limited input.
            auto outputrequirements = divround (
                sainpassact, node().saofrrate, node().satakergets, true);
            saoutpassact = std::min (saoutpassreq, outputrequirements);
            auto outputfees = mulround (
                saoutpassact, saoutfeerate, true);
            saoutplusfees   = std::min (node().saofferfunds, outputfees);

            writelog (lstrace, ripplecalc)
                << "delivernodereverse: adjusted:"
                << " saoutpassact=" << saoutpassact
                << " saoutplusfees=" << saoutplusfees;
        }
        else
        {
            // todo(tom): more logging here.
            assert (sainpassact == sainpassreq);
        }

        // funds were spent.
        node().bfundsdirty = true;

        // want to deduct output to limit calculations while computing reverse.
        // don't actually need to send.
        //
        // sending could be complicated: could fund a previous offer not yet
        // visited.  however, these deductions and adjustments are tenative.
        //
        // must reset balances when going forward to perform actual transfers.
        resultcode   = ledger().accountsend (
            node().offerowneraccount_, node().issue_.account, saoutpassact);

        if (resultcode != tessuccess)
            break;

        // adjust offer
        stamount satakergetsnew  = node().satakergets - saoutpassact;
        stamount satakerpaysnew  = node().satakerpays - sainpassact;

        if (satakerpaysnew < zero || satakergetsnew < zero)
        {
            writelog (lswarning, ripplecalc)
                << "delivernodereverse: negative:"
                << " node().satakerpaysnew=" << satakerpaysnew
                << " node().satakergetsnew=" << satakergetsnew;

            resultcode = telfailed_processing;
            break;
        }

        node().sleoffer->setfieldamount (sftakergets, satakergetsnew);
        node().sleoffer->setfieldamount (sftakerpays, satakerpaysnew);

        ledger().entrymodify (node().sleoffer);

        if (saoutpassact == node().satakergets)
        {
            // offer became unfunded.
            writelog (lsdebug, ripplecalc)
                << "delivernodereverse: offer became unfunded.";

            node().bentryadvance   = true;
            // xxx when don't we want to set advance?
        }
        else
        {
            assert (saoutpassact < node().satakergets);
        }

        saoutact += saoutpassact;
        // accumulate what is to be delivered from previous node.
        previousnode().sarevdeliver += sainpassact;
    }

    condlog (saoutact > saoutreq, lswarning, ripplecalc)
        << "delivernodereverse: too much:"
        << " saoutact=" << saoutact
        << " saoutreq=" << saoutreq;

    assert (saoutact <= saoutreq);

    if (resultcode == tessuccess && !saoutact)
        resultcode = tecpath_dry;
    // unable to meet request, consider path dry.
    // design invariant: if nothing was actually delivered, return tecpath_dry.

    writelog (lstrace, ripplecalc)
        << "delivernodereverse<"
        << " saoutact=" << saoutact
        << " saoutreq=" << saoutreq
        << " saprvdlvreq=" << previousnode().sarevdeliver;

    return resultcode;
}

}  // path
}  // ripple
