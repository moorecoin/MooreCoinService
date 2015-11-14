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
#include <ripple/app/ledger/ledgertiming.h>
#include <ripple/basics/log.h>

namespace ripple {

// vfalco should rename continuousledgertiming to ledgertiming

// note: first and last times must be repeated
int continuousledgertiming::ledgertimeresolution[] = { 10, 10, 20, 30, 60, 90, 120, 120 };

// called when a ledger is open and no close is in progress -- when a transaction is received and no close
// is in process, or when a close completes. returns the number of seconds the ledger should be be open.
bool continuousledgertiming::shouldclose (
    bool anytransactions,
    int previousproposers,      // proposers in the last closing
    int proposersclosed,        // proposers who have currently closed this ledgers
    int proposersvalidated,     // proposers who have validated the last closed ledger
    int previousmseconds,       // milliseconds the previous ledger took to reach consensus
    int currentmseconds,        // milliseconds since the previous ledger closed
    int openmseconds,           // milliseconds since the previous lcl was computed
    int idleinterval)           // network's desired idle interval
{
    if ((previousmseconds < -1000) || (previousmseconds > 600000) ||
            (currentmseconds < -1000) || (currentmseconds > 600000))
    {
        writelog (lswarning, ledgertiming) <<
            "clc::shouldclose range trans=" << (anytransactions ? "yes" : "no") <<
            " prop: " << previousproposers << "/" << proposersclosed <<
            " secs: " << currentmseconds << " (last: " << previousmseconds << ")";
        return true;
    }

    if (!anytransactions)
    {
        // no transactions so far this interval
        if (proposersclosed > (previousproposers / 4)) // did we miss a transaction?
        {
            writelog (lstrace, ledgertiming) <<
                "no transactions, many proposers: now (" << proposersclosed <<
                " closed, " << previousproposers << " before)";
            return true;
        }

#if 0 // this false triggers on the genesis ledger

        if (previousmseconds > (1000 * (ledger_idle_interval + 2))) // the last ledger was very slow to close
        {
            writelog (lstrace, ledgertiming) << "was slow to converge (p=" << (previousmseconds) << ")";

            if (previousmseconds < 2000)
                return previousmseconds;

            return previousmseconds - 1000;
        }

#endif
        return currentmseconds >= (idleinterval * 1000); // normal idle
    }

    if ((openmseconds < ledger_min_close) && ((proposersclosed + proposersvalidated) < (previousproposers / 2 )))
    {
        writelog (lsdebug, ledgertiming) <<
            "must wait minimum time before closing";
        return false;
    }

    if ((currentmseconds < previousmseconds) && ((proposersclosed + proposersvalidated) < previousproposers))
    {
        writelog (lsdebug, ledgertiming) <<
            "we are waiting for more closes/validations";
        return false;
    }

    return true; // this ledger should close now
}

// returns whether we have a consensus or not. if so, we expect all honest nodes
// to already have everything they need to accept a consensus. our vote is 'locked in'.
bool continuousledgertiming::haveconsensus (
    int previousproposers,      // proposers in the last closing (not including us)
    int currentproposers,       // proposers in this closing so far (not including us)
    int currentagree,           // proposers who agree with us
    int currentfinished,        // proposers who have validated a ledger after this one
    int previousagreetime,      // how long it took to agree on the last ledger
    int currentagreetime,       // how long we've been trying to agree
    bool forreal,               // deciding whether to stop consensus process
    bool& failed)               // we can't reach a consensus
{
    writelog (lstrace, ledgertiming) <<
        "clc::haveconsensus: prop=" << currentproposers <<
        "/" << previousproposers <<
        " agree=" << currentagree << " validated=" << currentfinished <<
        " time=" << currentagreetime <<  "/" << previousagreetime <<
        (forreal ? "" : "x");

    if (currentagreetime <= ledger_min_consensus)
        return false;

    if (currentproposers < (previousproposers * 3 / 4))
    {
        // less than 3/4 of the last ledger's proposers are present, we may need more time
        if (currentagreetime < (previousagreetime + ledger_min_consensus))
        {
            condlog (forreal, lstrace, ledgertiming) <<
                "too fast, not enough proposers";
            return false;
        }
    }

    // if 80% of current proposers (plus us) agree on a set, we have consensus
    if (((currentagree * 100 + 100) / (currentproposers + 1)) > 80)
    {
        condlog (forreal, lsinfo, ledgertiming) << "normal consensus";
        failed = false;
        return true;
    }

    // if 80% of the nodes on your unl have moved on, you should declare consensus
    if (((currentfinished * 100) / (currentproposers + 1)) > 80)
    {
        condlog (forreal, lswarning, ledgertiming) <<
            "we see no consensus, but 80% of nodes have moved on";
        failed = true;
        return true;
    }

    // no consensus yet
    condlog (forreal, lstrace, ledgertiming) << "no consensus";
    return false;
}

int continuousledgertiming::getnextledgertimeresolution (int previousresolution, bool previousagree, int ledgerseq)
{
    assert (ledgerseq);

    if ((!previousagree) && ((ledgerseq % ledger_res_decrease) == 0))
    {
        // reduce resolution
        int i = 1;

        while (ledgertimeresolution[i] != previousresolution)
            ++i;

        return ledgertimeresolution[i + 1];
    }

    if ((previousagree) && ((ledgerseq % ledger_res_increase) == 0))
    {
        // increase resolution
        int i = 1;

        while (ledgertimeresolution[i] != previousresolution)
            ++i;

        return ledgertimeresolution[i - 1];
    }

    return previousresolution;
}

} // ripple
