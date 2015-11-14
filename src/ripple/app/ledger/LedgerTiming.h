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

#ifndef __ledgertiming__
#define __ledgertiming__

namespace ripple {

// the number of seconds a ledger may remain idle before closing
const int ledger_idle_interval = 15;

// the number of seconds a validation remains current after its ledger's close time
// this is a safety to protect against very old validations and the time it takes to adjust
// the close time accuracy window
const int ledger_val_interval = 300;

// the number of seconds before a close time that we consider a validation acceptable
// this protects against extreme clock errors
const int ledger_early_interval = 180;

// the number of milliseconds we wait minimum to ensure participation
const int ledger_min_consensus = 2000;

// the number of milliseconds we wait minimum to ensure others have computed the lcl
const int ledger_min_close = 2000;

// initial resolution of ledger close time
const int ledger_time_accuracy = 30;

// how often to increase resolution
const int ledger_res_increase = 8;

// how often to decrease resolution
const int ledger_res_decrease = 1;

// how often we check state or change positions (in milliseconds)
const int ledger_granularity = 1000;

// the percentage of active trusted validators that must be able to
// keep up with the network or we consider the network overloaded
const int ledger_net_ratio = 70;

// how long we consider a proposal fresh
const int propose_freshness = 20;

// how often we force generating a new proposal to keep ours fresh
const int propose_interval = 12;

// avalanche tuning
// percentage of nodes on our unl that must vote yes
const int av_init_consensus_pct = 50;

// percentage of previous close time before we advance
const int av_mid_consensus_time = 50;

// percentage of nodes that most vote yes after advancing
const int av_mid_consensus_pct = 65;

// percentage of previous close time before we advance
const int av_late_consensus_time = 85;

// percentage of nodes that most vote yes after advancing
const int av_late_consensus_pct = 70;

const int av_stuck_consensus_time = 200;
const int av_stuck_consensus_pct = 95;

const int av_ct_consensus_pct = 75;

class continuousledgertiming
{
public:

    static int ledgertimeresolution[];

    // returns the number of seconds the ledger was or should be open
    // call when a consensus is reached and when any transaction is relayed to be added
    static bool shouldclose (
        bool anytransactions,
        int previousproposers,      int proposersclosed,    int proposerersvalidated,
        int previousmseconds,       int currentmseconds,    int openmseconds,
        int idleinterval);

    static bool haveconsensus (
        int previousproposers,      int currentproposers,
        int currentagree,           int currentclosed,
        int previousagreetime,      int currentagreetime,
        bool forreal,               bool& failed);

    static int getnextledgertimeresolution (int previousresolution, bool previousagree, int ledgerseq);
};

} // ripple

#endif
