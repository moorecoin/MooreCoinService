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
#include <ripple/app/ledger/ledgercleaner.h>
#include <ripple/app/ledger/inboundledgers.h>
#include <ripple/app/ledger/ledger.h>
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/main/application.h>
#include <ripple/core/loadfeetrack.h>
#include <ripple/protocol/protocol.h>
#include <ripple/protocol/rippleledgerhash.h>
#include <beast/threads/thread.h>
#include <beast/cxx14/memory.h> // <memory>
#include <thread>

namespace ripple {

/*

ledgercleaner

cleans up the ledger. specifically, resolves these issues:

1. older versions could leave the sqlite account and transaction databases in
   an inconsistent state. the cleaner identifies these inconsistencies and
   resolves them.

2. upon request, checks for missing nodes in a ledger and triggers a fetch.

*/

class ledgercleanerimp
    : public ledgercleaner
    , public beast::thread
    , public beast::leakchecked <ledgercleanerimp>
{
public:
    struct state
    {
        state()
            : minrange (0)
            , maxrange (0)
            , checknodes (false)
            , fixtxns (false)
            , failures (0)
        {
        }

        ledgerindex  minrange;    // the lowest ledger in the range we're checking
        ledgerindex  maxrange;    // the highest ledger in the range we're checking
        bool         checknodes;  // check all state/transaction nodes
        bool         fixtxns;     // rewrite sql databases
        int          failures;    // number of errors encountered since last success
    };

    typedef beast::shareddata <state> sharedstate;

    sharedstate m_state;
    beast::journal m_journal;

    //--------------------------------------------------------------------------

    ledgercleanerimp (
        stoppable& stoppable,
        beast::journal journal)
        : ledgercleaner (stoppable)
        , thread ("ledgercleaner")
        , m_journal (journal)
    {
    }

    ~ledgercleanerimp ()
    {
        stopthread ();
    }

    //--------------------------------------------------------------------------
    //
    // stoppable
    //
    //--------------------------------------------------------------------------

    void onprepare ()
    {
    }

    void onstart ()
    {
        startthread();
    }

    void onstop ()
    {
        m_journal.info << "stopping";
        signalthreadshouldexit();
        notify();
    }

    //--------------------------------------------------------------------------
    //
    // propertystream
    //
    //--------------------------------------------------------------------------

    void onwrite (beast::propertystream::map& map)
    {
        sharedstate::access state (m_state);

        if (state->maxrange == 0)
            map["status"] = "idle";
        else
        {
            map["status"] = "running";
            map["min_ledger"] = state->minrange;
            map["max_ledger"] = state->maxrange;
            map["check_nodes"] = state->checknodes ? "true" : "false";
            map["fix_txns"] = state->fixtxns ? "true" : "false";
            if (state->failures > 0)
                map["fail_counts"] = state->failures;
        }
    }

    //--------------------------------------------------------------------------
    //
    // ledgercleaner
    //
    //--------------------------------------------------------------------------

    void doclean (json::value const& params)
    {
        ledgerindex minrange;
        ledgerindex maxrange;
        getapp().getledgermaster().getfullvalidatedrange (minrange, maxrange);

        {
            sharedstate::access state (m_state);

            state->maxrange = maxrange;
            state->minrange = minrange;
            state->checknodes = false;
            state->fixtxns = false;
            state->failures = 0;

            /*
            json parameters:

                all parameters are optional. by default the cleaner cleans
                things it thinks are necessary. this behavior can be modified
                using the following options supplied via json rpc:

                "ledger"
                    a single unsigned integer representing an individual
                    ledger to clean.

                "min_ledger", "max_ledger"
                    unsigned integers representing the starting and ending
                    ledger numbers to clean. if unspecified, clean all ledgers.

                "full"
                    a boolean. when set to true, means clean everything possible.

                "fix_txns"
                    a boolean value indicating whether or not to fix the
                    transactions in the database as well.

                "check_nodes"
                    a boolean, when set to true means check the nodes.

                "stop"
                    a boolean, when set to true informs the cleaner to gracefully
                    stop its current activities if any cleaning is taking place.
            */

            // quick way to fix a single ledger
            if (params.ismember("ledger"))
            {
                state->maxrange = params["ledger"].asuint();
                state->minrange = params["ledger"].asuint();
                state->fixtxns = true;
                state->checknodes = true;
            }

            if (params.ismember("max_ledger"))
                 state->maxrange = params["max_ledger"].asuint();

            if (params.ismember("min_ledger"))
                state->minrange = params["min_ledger"].asuint();

            if (params.ismember("full"))
                state->fixtxns = state->checknodes = params["full"].asbool();

            if (params.ismember("fix_txns"))
                state->fixtxns = params["fix_txns"].asbool();

            if (params.ismember("check_nodes"))
                state->checknodes = params["check_nodes"].asbool();

            if (params.ismember("stop") && params["stop"].asbool())
                state->minrange = state->maxrange = 0;
        }

        notify();
    }

    //--------------------------------------------------------------------------
    //
    // ledgercleanerimp
    //
    //--------------------------------------------------------------------------

    void init ()
    {
        m_journal.debug << "initializing";
    }

    void run ()
    {
        m_journal.debug << "started";

        init ();

        while (! this->threadshouldexit())
        {
            this->wait ();
            if (! this->threadshouldexit())
            {
                doledgercleaner();
            }
        }

        stopped();
    }

    ledgerhash getledgerhash(ledger::pointer ledger, ledgerindex index)
    {
        ledgerhash hash;
        try
        {
            hash = ledger->getledgerhash(index);
        }
        catch (shamapmissingnode &)
        {
            m_journal.warning <<
                "node missing from ledger " << ledger->getledgerseq();
            getapp().getinboundledgers().findcreate (
                ledger->gethash(), ledger->getledgerseq(), inboundledger::fcgeneric);
        }
        return hash;
    }

    /** process a single ledger
        @param ledgerindex the index of the ledger to process.
        @param ledgerhash  the known correct hash of the ledger.
        @param donodes ensure all ledger nodes are in the node db.
        @param dotxns reprocess (account) transactions to sql databases.
        @return `true` if the ledger was cleaned.
    */
    bool doledger(
        ledgerindex const& ledgerindex,
        ledgerhash const& ledgerhash,
        bool donodes,
        bool dotxns)
    {
        ledger::pointer nodeledger = getapp().getledgermaster().findacquireledger(ledgerindex, ledgerhash);
        if (!nodeledger)
        {
            m_journal.debug << "ledger " << ledgerindex << " not available";
            return false;
        }

        ledger::pointer dbledger = ledger::loadbyindex(ledgerindex);
        if (! dbledger ||
            (dbledger->gethash() != ledgerhash) ||
            (dbledger->getparenthash() != nodeledger->getparenthash()))
        {
            // ideally we'd also check for more than one ledger with that index
            m_journal.debug <<
                "ledger " << ledgerindex << " mismatches sql db";
            dotxns = true;
        }

        if(! getapp().getledgermaster().fixindex(ledgerindex, ledgerhash))
        {
            m_journal.debug << "ledger " << ledgerindex << " had wrong entry in history";
            dotxns = true;
        }

        if (donodes && !nodeledger->walkledger())
        {
            m_journal.debug << "ledger " << ledgerindex << " is missing nodes";
            getapp().getinboundledgers().findcreate(ledgerhash, ledgerindex, inboundledger::fcgeneric);
            return false;
        }

        if (dotxns && !nodeledger->pendsavevalidated(true, false))
        {
            m_journal.debug << "failed to save ledger " << ledgerindex;
            return false;
        }

        return true;
    }

    /** returns the hash of the specified ledger.
        @param ledgerindex the index of the desired ledger.
        @param referenceledger [out] an optional known good subsequent ledger.
        @return the hash of the ledger. this will be all-bits-zero if not found.
    */
    ledgerhash gethash(
        ledgerindex const& ledgerindex,
        ledger::pointer& referenceledger)
    {
        ledgerhash ledgerhash;

        if (!referenceledger || (referenceledger->getledgerseq() < ledgerindex))
        {
            referenceledger = getapp().getledgermaster().getvalidatedledger();
            if (!referenceledger)
            {
                m_journal.warning << "no validated ledger";
                return ledgerhash; // nothing we can do. no validated ledger.
            }
        }

        if (referenceledger->getledgerseq() >= ledgerindex)
        {
            // see if the hash for the ledger we need is in the reference ledger
            ledgerhash = getledgerhash(referenceledger, ledgerindex);
            if (ledgerhash.iszero())
            {
                // no, try to get another ledger that might have the hash we need
                // compute the index and hash of a ledger that will have the hash we need
                ledgerindex refindex = (ledgerindex + 255) & (~255);
                ledgerhash refhash = getledgerhash (referenceledger, refindex);

                bool const nonzero (refhash.isnonzero ());
                assert (nonzero);
                if (nonzero)
                {
                    // we found the hash and sequence of a better reference ledger
                    referenceledger = getapp().getledgermaster().findacquireledger (refindex, refhash);
                    if (referenceledger)
                        ledgerhash = getledgerhash(referenceledger, ledgerindex);
                }
            }
        }
        else
            m_journal.warning << "validated ledger is prior to target ledger";

        return ledgerhash;
    }

    /** run the ledger cleaner. */
    void doledgercleaner()
    {
        ledger::pointer goodledger;

        while (! this->threadshouldexit())
        {
            ledgerindex ledgerindex;
            ledgerhash ledgerhash;
            bool donodes;
            bool dotxns;

            while (getapp().getfeetrack().isloadedlocal())
            {
                m_journal.debug << "waiting for load to subside";
                std::this_thread::sleep_for(std::chrono::seconds(5));
                if (this->threadshouldexit ())
                    return;
            }

            {
                sharedstate::access state (m_state);
                if ((state->minrange > state->maxrange) ||
                    (state->maxrange == 0) || (state->minrange == 0))
                {
                    state->minrange = state->maxrange = 0;
                    return;
                }
                ledgerindex = state->maxrange;
                donodes = state->checknodes;
                dotxns = state->fixtxns;
            }

            ledgerhash = gethash(ledgerindex, goodledger);

            bool fail = false;
            if (ledgerhash.iszero())
            {
                m_journal.info << "unable to get hash for ledger " << ledgerindex;
                fail = true;
            }
            else if (!doledger(ledgerindex, ledgerhash, donodes, dotxns))
            {
                m_journal.info << "failed to process ledger " << ledgerindex;
                fail = true;
            }

            if (fail)
            {
                {
                    sharedstate::access state (m_state);
                    ++state->failures;
                }
                // wait for acquiring to catch up to us
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            else
            {
                {
                    sharedstate::access state (m_state);
                    if (ledgerindex == state->minrange)
                        ++state->minrange;
                    if (ledgerindex == state->maxrange)
                        --state->maxrange;
                    state->failures = 0;
                }
                // reduce i/o pressure and wait for acquiring to catch up to us
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

        }
    }
};

//------------------------------------------------------------------------------

ledgercleaner::ledgercleaner (stoppable& parent)
    : stoppable ("ledgercleaner", parent)
    , beast::propertystream::source ("ledgercleaner")
{
}

ledgercleaner::~ledgercleaner ()
{
}

std::unique_ptr<ledgercleaner>
make_ledgercleaner (beast::stoppable& parent, beast::journal journal)
{
    return std::make_unique <ledgercleanerimp> (parent, journal);
}

} // ripple
