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

ter pathcursor::advancenode (stamount const& amount, bool reverse) const
{
    bool multi = multiquality_ || amount == zero;

    // if the multiquality_ is unchanged, use the pathcursor we're using now.
    if (multi == multiquality_)
        return advancenode (reverse);

    // otherwise, use a new pathcursor with the new multiquality_.
    pathcursor withmultiquality {ripplecalc_, pathstate_, multi, nodeindex_};
    return withmultiquality.advancenode (reverse);
}

// optimize: when calculating path increment, note if increment consumes all
// liquidity. no need to revisit path in the future if all liquidity is used.
//
ter pathcursor::advancenode (bool const breverse) const
{
    ter resultcode = tessuccess;

    // taker is the active party against an offer in the ledger - the entity
    // that is taking advantage of an offer in the order book.
    writelog (lstrace, ripplecalc)
            << "advancenode: takerpays:"
            << node().satakerpays << " takergets:" << node().satakergets;

    int loopcount = 0;

    do
    {
        // vfalco note why not use a for() loop?
        // vfalco todo the limit on loop iterations puts an
        //             upper limit on the number of different quality
        // levels (ratio of pay:get) that will be considered for one path.
        // changing this value has repercusssions on validation and consensus.
        //
        if (++loopcount > node_advance_max_loops)
        {
            writelog (lswarning, ripplecalc) << "loop count exceeded";
            return tefexception;
        }

        bool bdirectdirdirty = node().directory.initialize (
            {previousnode().issue_, node().issue_},
            ledger());

        if (auto advance = node().directory.advance (ledger()))
        {
            bdirectdirdirty = true;
            if (advance == nodedirectory::new_quality)
            {
                // we didn't run off the end of this order book and found
                // another quality directory.
                writelog (lstrace, ripplecalc)
                    << "advancenode: quality advance: node.directory.current="
                    << node().directory.current;
            }
            else if (breverse)
            {
                writelog (lstrace, ripplecalc)
                    << "advancenode: no more offers.";

                node().offerindex_ = 0;
                break;
            }
            else
            {
                // no more offers. should be done rather than fall off end of
                // book.
                writelog (lswarning, ripplecalc)
                    << "advancenode: unreachable: "
                    << "fell off end of order book.";
                // fixme: why?
                return telfailed_processing;
            }
        }

        if (bdirectdirdirty)
        {
            // our quality changed since last iteration.
            // use the rate from the directory.
            node().saofrrate = amountfromquality (
                getquality (node().directory.current));
            // for correct ratio
            node().uentry = 0;
            node().bentryadvance   = true;

            writelog (lstrace, ripplecalc)
                << "advancenode: directory dirty: node.saofrrate="
                << node().saofrrate;
        }

        if (!node().bentryadvance)
        {
            if (node().bfundsdirty)
            {
                // we were called again probably merely to update structure
                // variables.
                node().satakerpays
                        = node().sleoffer->getfieldamount (sftakerpays);
                node().satakergets
                        = node().sleoffer->getfieldamount (sftakergets);

                // funds left.
                node().saofferfunds = ledger().accountfunds (
                    node().offerowneraccount_,
                    node().satakergets,
                    fhzero_if_frozen);
                node().bfundsdirty = false;

                writelog (lstrace, ripplecalc)
                    << "advancenode: funds dirty: node().saofrrate="
                    << node().saofrrate;
            }
            else
            {
                writelog (lstrace, ripplecalc) << "advancenode: as is";
            }
        }
        else if (!ledger().dirnext (
            node().directory.current,
            node().directory.ledgerentry,
            node().uentry,
            node().offerindex_))
            // this is the only place that offerindex_ changes.
        {
            // failed to find an entry in directory.
            // do another cur directory iff multiquality_
            if (multiquality_)
            {
                // we are allowed to process multiple qualities if this is the
                // only path.
                writelog (lstrace, ripplecalc)
                    << "advancenode: next quality";
                node().directory.advanceneeded  = true;  // process next quality.
            }
            else if (!breverse)
            {
                // we didn't run dry going backwards - why are we running dry
                // going forwards - this should be impossible!
                // todo(tom): these warnings occur in production!  they
                // shouldn't.
                writelog (lswarning, ripplecalc)
                    << "advancenode: unreachable: ran out of offers";
                return telfailed_processing;
            }
            else
            {
                // ran off end of offers.
                node().bentryadvance   = false;        // done.
                node().offerindex_ = 0;            // report no more entries.
            }
        }
        else
        {
            // got a new offer.
            node().sleoffer = ledger().entrycache (
                ltoffer, node().offerindex_);

            if (!node().sleoffer)
            {
                // corrupt directory that points to an entry that doesn't exist.
                // this has happened in production.
                writelog (lswarning, ripplecalc) <<
                    "missing offer in directory";
                node().bentryadvance = true;
            }
            else
            {
                node().offerowneraccount_
                        = node().sleoffer->getfieldaccount160 (sfaccount);
                node().satakerpays
                        = node().sleoffer->getfieldamount (sftakerpays);
                node().satakergets
                        = node().sleoffer->getfieldamount (sftakergets);

                accountissue const accountissue (
                    node().offerowneraccount_, node().issue_);

                writelog (lstrace, ripplecalc)
                    << "advancenode: offerowneraccount_="
                    << to_string (node().offerowneraccount_)
                    << " node.satakerpays=" << node().satakerpays
                    << " node.satakergets=" << node().satakergets
                    << " node.offerindex_=" << node().offerindex_;

                if (node().sleoffer->isfieldpresent (sfexpiration) &&
                    (node().sleoffer->getfieldu32 (sfexpiration) <=
                     ledger().getledger ()->
                     getparentclosetimenc ()))
                {
                    // offer is expired.
                    writelog (lstrace, ripplecalc)
                        << "advancenode: expired offer";
                    ripplecalc_.permanentlyunfundedoffers_.insert(
                        node().offerindex_);
                    continue;
                }

                if (node().satakerpays <= zero || node().satakergets <= zero)
                {
                    // offer has bad amounts. offers should never have a bad
                    // amounts.
                    auto const index = node().offerindex_;
                    if (breverse)
                    {
                        // past internal error, offer had bad amounts.
                        // this has occurred in production.
                        writelog (lswarning, ripplecalc)
                            << "advancenode: past internal error"
                            << " reverse: offer non-positive:"
                            << " node.satakerpays=" << node().satakerpays
                            << " node.satakergets=" << node().satakergets;

                        // mark offer for always deletion.
                        ripplecalc_.permanentlyunfundedoffers_.insert (
                            node().offerindex_);
                    }
                    else if (ripplecalc_.permanentlyunfundedoffers_.find (index)
                             != ripplecalc_.permanentlyunfundedoffers_.end ())
                    {
                        // past internal error, offer was found failed to place
                        // this in permanentlyunfundedoffers_.
                        // just skip it. it will be deleted.
                        writelog (lsdebug, ripplecalc)
                            << "advancenode: past internal error "
                            << " forward confirm: offer non-positive:"
                            << " node.satakerpays=" << node().satakerpays
                            << " node.satakergets=" << node().satakergets;

                    }
                    else
                    {
                        // reverse should have previously put bad offer in list.
                        // an internal error previously left a bad offer.
                        writelog (lswarning, ripplecalc)
                            << "advancenode: internal error"

                            <<" forward newly found: offer non-positive:"
                            << " node.satakerpays=" << node().satakerpays
                            << " node.satakergets=" << node().satakergets;

                        // don't process at all, things are in an unexpected
                        // state for this transactions.
                        resultcode = tefexception;
                    }

                    continue;
                }

                // allowed to access source from this node?
                //
                // xxx this can get called multiple times for same source in a
                // row, caching result would be nice.
                //
                // xxx going forward could we fund something with a worse
                // quality which was previously skipped? might need to check
                // quality.
                auto itforward = pathstate_.forward().find (accountissue);
                const bool bfoundforward =
                        itforward != pathstate_.forward().end ();

                // only allow a source to be used once, in the first node
                // encountered from initial path scan.  this prevents
                // conflicting uses of the same balance when going reverse vs
                // forward.
                if (bfoundforward &&
                    itforward->second != nodeindex_ &&
                    node().offerowneraccount_ != node().issue_.account)
                {
                    // temporarily unfunded. another node uses this source,
                    // ignore in this offer.
                    writelog (lstrace, ripplecalc)
                        << "advancenode: temporarily unfunded offer"
                        << " (forward)";
                    continue;
                }

                // this is overly strict. for contributions to past. we should
                // only count source if actually used.
                auto itreverse = pathstate_.reverse().find (accountissue);
                bool bfoundreverse = itreverse != pathstate_.reverse().end ();

                // for this quality increment, only allow a source to be used
                // from a single node, in the first node encountered from
                // applying offers in reverse.
                if (bfoundreverse &&
                    itreverse->second != nodeindex_ &&
                    node().offerowneraccount_ != node().issue_.account)
                {
                    // temporarily unfunded. another node uses this source,
                    // ignore in this offer.
                    writelog (lstrace, ripplecalc)
                        << "advancenode: temporarily unfunded offer"
                        <<" (reverse)";
                    continue;
                }

                // determine if used in past.
                // we only need to know if it might need to be marked unfunded.
                auto itpast = ripplecalc_.mumsource_.find (accountissue);
                bool bfoundpast = (itpast != ripplecalc_.mumsource_.end ());

                // only the current node is allowed to use the source.

                node().saofferfunds = ledger().accountfunds (
                    node().offerowneraccount_,
                    node().satakergets,
                    fhzero_if_frozen);
                // funds held.

                if (node().saofferfunds <= zero)
                {
                    // offer is unfunded.
                    writelog (lstrace, ripplecalc)
                        << "advancenode: unfunded offer";

                    if (breverse && !bfoundreverse && !bfoundpast)
                    {
                        // never mentioned before, clearly just: found unfunded.
                        // that is, even if this offer fails due to fill or kill
                        // still do deletions.
                        // mark offer for always deletion.
                        ripplecalc_.permanentlyunfundedoffers_.insert (node().offerindex_);
                    }
                    else
                    {
                        // moving forward, don't need to insert again
                        // or, already found it.
                    }

                    // yyy could verify offer is correct place for unfundeds.
                    continue;
                }

                if (breverse            // need to remember reverse mention.
                    && !bfoundpast      // not mentioned in previous passes.
                    && !bfoundreverse)  // new to pass.
                {
                    // consider source mentioned by current path state.
                    writelog (lstrace, ripplecalc)
                        << "advancenode: remember="
                        <<  node().offerowneraccount_
                        << "/"
                        << node().issue_;

                    pathstate_.insertreverse (accountissue, nodeindex_);
                }

                node().bfundsdirty = false;
                node().bentryadvance = false;
            }
        }
    }
    while (resultcode == tessuccess &&
           (node().bentryadvance || node().directory.advanceneeded));

    if (resultcode == tessuccess)
    {
        writelog (lstrace, ripplecalc)
            << "advancenode: node.offerindex_=" << node().offerindex_;
    }
    else
    {
        writelog (lsdebug, ripplecalc)
            << "advancenode: resultcode=" << transtoken (resultcode);
    }

    return resultcode;
}

}  // path
}  // ripple
