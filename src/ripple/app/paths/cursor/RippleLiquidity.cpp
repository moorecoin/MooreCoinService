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
#include <ripple/app/paths/cursor/rippleliquidity.h>
#include <ripple/basics/log.h>

namespace ripple {
namespace path {

// compute how much might flow for the node for the pass. does not actually
// adjust balances.
//
// uqualityin -> uqualityout
//   saprvreq -> sacurreq
//   sqprvact -> sacuract
//
// this is a minimizing routine: moving in reverse it propagates the send limit
// to the sender, moving forward it propagates the actual send toward the
// receiver.
//
// when this routine works backwards, sacurreq is the driving variable: it
// calculates previous wants based on previous credit limits and current wants.
//
// when this routine works forwards, saprvreq is the driving variable: it
//   calculates current deliver based on previous delivery limits and current
//   wants.
//
// this routine is called one or two times for a node in a pass. if called once,
// it will work and set a rate.  if called again, the new work must not worsen
// the previous rate.

void rippleliquidity (
    ripplecalc& ripplecalc,
    std::uint32_t const uqualityin,
    std::uint32_t const uqualityout,
    stamount const& saprvreq,   // --> in limit including fees, <0 = unlimited
    stamount const& sacurreq,   // --> out limit
    stamount& saprvact,  // <-> in limit including achieved so far: <-- <= -->
    stamount& sacuract,  // <-> out limit including achieved so far: <-- <= -->
    std::uint64_t& uratemax)
{
    writelog (lstrace, ripplecalc)
        << "rippleliquidity>"
        << " uqualityin=" << uqualityin
        << " uqualityout=" << uqualityout
        << " saprvreq=" << saprvreq
        << " sacurreq=" << sacurreq
        << " saprvact=" << saprvact
        << " sacuract=" << sacuract;

    // sacuract was once zero in a production server.
    assert (sacurreq != zero);
    assert (sacurreq > zero);

    assert (saprvreq.getcurrency () == sacurreq.getcurrency ());
    assert (saprvreq.getcurrency () == saprvact.getcurrency ());
    assert (saprvreq.getissuer () == saprvact.getissuer ());

    const bool bprvunlimited = (saprvreq < zero);  // -1 means unlimited.

    // unlimited stays unlimited - don't do calculations.

    // how much could possibly flow through the previous node?
    const stamount saprv = bprvunlimited ? saprvreq : saprvreq - saprvact;

    // how much could possibly flow through the current node?
    const stamount  sacur = sacurreq - sacuract;

    writelog (lstrace, ripplecalc)
        << "rippleliquidity: "
        << " bprvunlimited=" << bprvunlimited
        << " saprv=" << saprv
        << " sacur=" << sacur;

    // if nothing can flow, we might as well not do any work.
    if (saprv == zero || sacur == zero)
        return;

    if (uqualityin >= uqualityout)
    {
        // you're getting better quality than you asked for, so no fee.
        writelog (lstrace, ripplecalc) << "rippleliquidity: no fees";

        // only process if the current rate, 1:1, is not worse than the previous
        // rate, uratemax - otherwise there is no flow.
        if (!uratemax || stamount::urateone <= uratemax)
        {
            // limit amount to transfer if need - the minimum of amount being
            // paid and the amount that's wanted.
            stamount satransfer = bprvunlimited ? sacur
                : std::min (saprv, sacur);

            // in reverse, we want to propagate the limited cur to prv and set
            // actual cur.
            //
            // in forward, we want to propagate the limited prv to cur and set
            // actual prv.
            //
            // this is the actual flow.
            saprvact += satransfer;
            sacuract += satransfer;

            // if no rate limit, set rate limit to avoid combining with
            // something with a worse rate.
            if (uratemax == 0)
                uratemax = stamount::urateone;
        }
    }
    else
    {
        // if the quality is worse than the previous
        writelog (lstrace, ripplecalc) << "rippleliquidity: fee";

        std::uint64_t urate = getrate (
            stamount (uqualityout), stamount (uqualityin));

        // if the next rate is at least as good as the current rate, process.
        if (!uratemax || urate <= uratemax)
        {
            auto currency = sacur.getcurrency ();
            auto ucurissuerid = sacur.getissuer ();

            // current actual = current request * (quality out / quality in).
            auto numerator = mulround (
                sacur, uqualityout, {currency, ucurissuerid}, true);
            // true means "round up" to get best flow.

            stamount sacurin = divround (
                numerator, uqualityin, {currency, ucurissuerid}, true);

            writelog (lstrace, ripplecalc)
                << "rippleliquidity:"
                << " bprvunlimited=" << bprvunlimited
                << " saprv=" << saprv
                << " sacurin=" << sacurin;

            if (bprvunlimited || sacurin <= saprv)
            {
                // all of current. some amount of previous.
                sacuract += sacur;
                saprvact += sacurin;
                writelog (lstrace, ripplecalc)
                    << "rippleliquidity:3c:"
                    << " sacurreq=" << sacurreq
                    << " saprvact=" << saprvact;
            }
            else
            {
                // there wasn't enough money to start with - so given the
                // limited input, how much could we deliver?
                // current actual = previous request
                //                  * (quality in / quality out).
                // this is inverted compared to the code above because we're
                // going the other way

                issue issue{currency, ucurissuerid};
                auto numerator = mulround (
                    saprv, uqualityin, issue, true);
                // a part of current. all of previous. (cur is the driver
                // variable.)
                stamount sacurout = divround (
                    numerator, uqualityout, issue, true);

                writelog (lstrace, ripplecalc)
                    << "rippleliquidity:4: sacurreq=" << sacurreq;

                sacuract += sacurout;
                saprvact = saprvreq;
            }
            if (!uratemax)
                uratemax = urate;
        }
    }

    writelog (lstrace, ripplecalc)
        << "rippleliquidity<"
        << " uqualityin=" << uqualityin
        << " uqualityout=" << uqualityout
        << " saprvreq=" << saprvreq
        << " sacurreq=" << sacurreq
        << " saprvact=" << saprvact
        << " sacuract=" << sacuract;
}

static
std::uint32_t
ripplequality (
    ledgerentryset& ledger,
    account const& destination,
    account const& source,
    currency const& currency,
    sfield::ref sflow,
    sfield::ref sfhigh)
{
    std::uint32_t uquality (quality_one);

    if (destination != source)
    {
        sle::pointer sleripplestate (ledger.entrycache (ltripple_state,
            getripplestateindex (destination, source, currency)));

        // we should be able to assert(sleripplestate) here

        if (sleripplestate)
        {
            sfield::ref sffield = destination < source ? sflow : sfhigh;

            uquality = sleripplestate->isfieldpresent (sffield)
                ? sleripplestate->getfieldu32 (sffield)
                : quality_one;

            if (!uquality)
                uquality = 1; // avoid divide by zero.
        }
    }

    return uquality;
}

std::uint32_t
quality_in (
    ledgerentryset& ledger,
    account const& utoaccountid,
    account const& ufromaccountid,
    currency const& currency)
{
    return ripplequality (ledger, utoaccountid, ufromaccountid, currency,
        sflowqualityin, sfhighqualityin);
}

std::uint32_t
quality_out (
    ledgerentryset& ledger,
    account const& utoaccountid,
    account const& ufromaccountid,
    currency const& currency)
{
    return ripplequality (ledger, utoaccountid, ufromaccountid, currency,
        sflowqualityout, sfhighqualityout);
}

} // path
} // ripple
