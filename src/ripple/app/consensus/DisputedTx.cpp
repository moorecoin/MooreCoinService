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
#include <ripple/app/consensus/disputedtx.h>
#include <ripple/app/ledger/ledgertiming.h>
#include <ripple/basics/log.h>
#include <ripple/json/to_string.h>

namespace ripple {

// track a peer's yes/no vote on a particular disputed transaction
void disputedtx::setvote (nodeid const& peer, bool votesyes)
{
    auto res = mvotes.insert (std::make_pair (peer, votesyes));

    // new vote
    if (res.second)
    {
        if (votesyes)
        {
            writelog (lsdebug, ledgerconsensus)
                    << "peer " << peer << " votes yes on " << mtransactionid;
            ++myays;
        }
        else
        {
            writelog (lsdebug, ledgerconsensus)
                    << "peer " << peer << " votes no on " << mtransactionid;
            ++mnays;
        }
    }
    // changes vote to yes
    else if (votesyes && !res.first->second)
    {
        writelog (lsdebug, ledgerconsensus)
                << "peer " << peer << " now votes yes on " << mtransactionid;
        --mnays;
        ++myays;
        res.first->second = true;
    }
    // changes vote to no
    else if (!votesyes && res.first->second)
    {
        writelog (lsdebug, ledgerconsensus) << "peer " << peer
                                            << " now votes no on " << mtransactionid;
        ++mnays;
        --myays;
        res.first->second = false;
    }
}

// remove a peer's vote on this disputed transasction
void disputedtx::unvote (nodeid const& peer)
{
    auto it = mvotes.find (peer);

    if (it != mvotes.end ())
    {
        if (it->second)
            --myays;
        else
            --mnays;

        mvotes.erase (it);
    }
}

bool disputedtx::updatevote (int percenttime, bool proposing)
{
    // vfalco todo give the return value a descriptive local variable name
    //             and don't return from the middle.

    if (mourvote && (mnays == 0))
        return false;

    if (!mourvote && (myays == 0))
        return false;

    bool newposition;
    int weight;

    if (proposing) // give ourselves full weight
    {
        // this is basically the percentage of nodes voting 'yes' (including us)
        weight = (myays * 100 + (mourvote ? 100 : 0)) / (mnays + myays + 1);

        // vfalco todo rename these macros and turn them into language
        //             constructs.  consolidate them into a class that collects
        //             all these related values.
        //
        // to prevent avalanche stalls, we increase the needed weight slightly
        // over time.
        if (percenttime < av_mid_consensus_time)
            newposition = weight >  av_init_consensus_pct;
        else if (percenttime < av_late_consensus_time)
            newposition = weight > av_mid_consensus_pct;
        else if (percenttime < av_stuck_consensus_time)
            newposition = weight > av_late_consensus_pct;
        else
            newposition = weight > av_stuck_consensus_pct;
    }
    else // don't let us outweigh a proposing node, just recognize consensus
    {
        weight = -1;
        newposition = myays > mnays;
    }

    if (newposition == mourvote)
    {
        writelog (lsinfo, ledgerconsensus)
                << "no change (" << (mourvote ? "yes" : "no") << ") : weight "
                << weight << ", percent " << percenttime;
        writelog (lsdebug, ledgerconsensus) << getjson ();
        return false;
    }

    mourvote = newposition;
    writelog (lsdebug, ledgerconsensus)
            << "we now vote " << (mourvote ? "yes" : "no")
            << " on " << mtransactionid;
    writelog (lsdebug, ledgerconsensus) << getjson ();
    return true;
}

json::value disputedtx::getjson ()
{
    json::value ret (json::objectvalue);

    ret["yays"] = myays;
    ret["nays"] = mnays;
    ret["our_vote"] = mourvote;

    if (!mvotes.empty ())
    {
        json::value votesj (json::objectvalue);
        for (auto& vote : mvotes)
            votesj[to_string (vote.first)] = vote.second;
        ret["votes"] = votesj;
    }

    return ret;
}

} // ripple
