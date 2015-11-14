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
#include <ripple/app/consensus/ledgerconsensus.h>
#include <ripple/app/misc/defaultmissingnodehandler.h> // vfalco bad dependency
#include <ripple/app/ledger/inboundledgers.h>
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/ledger/ledgertiming.h>
#include <ripple/app/ledger/ledgertojson.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/amendmenttable.h>
#include <ripple/app/misc/canonicaltxset.h>
#include <ripple/app/misc/ihashrouter.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/app/misc/validations.h>
#include <ripple/app/tx/transactionacquire.h>
#include <ripple/basics/countedobject.h>
#include <ripple/basics/log.h>
#include <ripple/core/config.h>
#include <ripple/core/jobqueue.h>
#include <ripple/core/loadfeetrack.h>
#include <ripple/json/to_string.h>
#include <ripple/overlay/overlay.h>
#include <ripple/overlay/predicates.h>
#include <ripple/protocol/stvalidation.h>
#include <ripple/protocol/uinttypes.h>
#include <ripple/app/misc/dividendvote.h>

namespace ripple {

/**
  provides the implementation for ledgerconsensus.

  achieves consensus on the next ledger.
  this object is created when the consensus process starts, and
  is destroyed when the process is complete.

  nearly everything herein is invoked with the master lock.

  two things need consensus:
    1.  the set of transactions.
    2.  the close time for the ledger.
*/
class ledgerconsensusimp
    : public ledgerconsensus
    , public std::enable_shared_from_this <ledgerconsensusimp>
    , public countedobject <ledgerconsensusimp>
{
public:
    /**
     * the result of applying a transaction to a ledger.
    */
    enum {resultsuccess, resultfail, resultretry};

    static char const* getcountedobjectname () { return "ledgerconsensus"; }

    ledgerconsensusimp(ledgerconsensusimp const&) = delete;
    ledgerconsensusimp& operator=(ledgerconsensusimp const&) = delete;

    /**
      the result of applying a transaction to a ledger.

      @param clock          the clock which will be used to measure time.
      @param localtx        a set of local transactions to apply.
      @param prevlclhash    the hash of the last closed ledger (lcl).
      @param previousledger best guess of what the last closed ledger (lcl)
                            was.
      @param closetime      closing time point of the lcl.
      @param feevote        our desired fee levels and voting logic.
    */
    ledgerconsensusimp (clock_type& clock, localtxs& localtx,
        ledgerhash const & prevlclhash, ledger::ref previousledger,
            std::uint32_t closetime, feevote& feevote, dividendvote& dividendvote)
        : m_clock (clock)
        , m_localtx (localtx)
        , m_feevote (feevote)
        , m_dividendvote (dividendvote)
        , mstate (lcspre_close)
        , mclosetime (closetime)
        , mprevledgerhash (prevlclhash)
        , mpreviousledger (previousledger)
        , mvalpublic (getconfig ().validation_pub)
        , mvalprivate (getconfig ().validation_priv)
        , mconsensusfail (false)
        , mcurrentmseconds (0)
        , mclosepercent (0)
        , mhaveclosetimeconsensus (false)
        , mconsensusstarttime
            (boost::posix_time::microsec_clock::universal_time ())
    {
        writelog (lsdebug, ledgerconsensus) << "creating consensus object";
        writelog (lstrace, ledgerconsensus)
            << "lcl:" << previousledger->gethash () << ", ct=" << closetime;
        mpreviousproposers = getapp().getops ().getpreviousproposers ();
        mpreviousmseconds = getapp().getops ().getpreviousconvergetime ();
        assert (mpreviousmseconds);

        // adapt close time resolution to recent network conditions
        mcloseresolution = continuousledgertiming::getnextledgertimeresolution (
            mpreviousledger->getcloseresolution (),
            mpreviousledger->getcloseagree (),
            previousledger->getledgerseq () + 1);

        if (mvalpublic.isset () && mvalprivate.isset ()
            && !getapp().getops ().isneednetworkledger ())
        {
            // if the validation keys were set, and if we need a ledger,
            // then we want to validate, and possibly propose a ledger.
            writelog (lsinfo, ledgerconsensus)
                << "entering consensus process, validating";
            mvalidating = true;
            // propose if we are in sync with the network
            mproposing =
                getapp().getops ().getoperatingmode () == networkops::omfull;
        }
        else
        {
            // otherwise we just want to monitor the validation process.
            writelog (lsinfo, ledgerconsensus)
                << "entering consensus process, watching";
            mproposing = mvalidating = false;
        }

        mhavecorrectlcl = (mpreviousledger->gethash () == mprevledgerhash);

        if (!mhavecorrectlcl)
        {
            // if we were not handed the correct lcl, then set our state
            // to not proposing.
            getapp().getops ().setproposing (false, false);
            handlelcl (mprevledgerhash);

            if (!mhavecorrectlcl)
            {
                //          mproposing = mvalidating = false;
                writelog (lsinfo, ledgerconsensus)
                    << "entering consensus with: "
                    << previousledger->gethash ();
                writelog (lsinfo, ledgerconsensus)
                    << "correct lcl is: " << prevlclhash;
            }
        }
        else  // update the network status table as to whether we're proposing/validating
            getapp().getops ().setproposing (mproposing, mvalidating);
    }

    /**
      this function is called, but its return value is always ignored.

      @return 1.
    */
    int startup ()
    {
        return 1;
    }

    /**
      get the json state of the consensus process.
      called by the consensus_info rpc.

      @param full true if verbose response desired.
      @return     the json state.
    */
    json::value getjson (bool full)
    {
        json::value ret (json::objectvalue);
        ret["proposing"] = mproposing;
        ret["validating"] = mvalidating;
        ret["proposers"] = static_cast<int> (mpeerpositions.size ());

        if (mhavecorrectlcl)
        {
            ret["synched"] = true;
            ret["ledger_seq"] = mpreviousledger->getledgerseq () + 1;
            ret["close_granularity"] = mcloseresolution;
        }
        else
            ret["synched"] = false;

        switch (mstate)
        {
        case lcspre_close:
            ret["state"] = "open";
            break;

        case lcsestablish:
            ret["state"] = "consensus";
            break;

        case lcsfinished:
            ret["state"] = "finished";
            break;

        case lcsaccepted:
            ret["state"] = "accepted";
            break;
        }

        int v = mdisputes.size ();

        if ((v != 0) && !full)
            ret["disputes"] = v;

        if (mourposition)
            ret["our_position"] = mourposition->getjson ();

        if (full)
        {

            ret["current_ms"] = mcurrentmseconds;
            ret["close_percent"] = mclosepercent;
            ret["close_resolution"] = mcloseresolution;
            ret["have_time_consensus"] = mhaveclosetimeconsensus;
            ret["previous_proposers"] = mpreviousproposers;
            ret["previous_mseconds"] = mpreviousmseconds;

            if (!mpeerpositions.empty ())
            {
                json::value ppj (json::objectvalue);

                for (auto& pp : mpeerpositions)
                {
                    ppj[to_string (pp.first)] = pp.second->getjson ();
                }
                ret["peer_positions"] = ppj;
            }

            if (!macquired.empty ())
            {
                // acquired
                json::value acq (json::objectvalue);
                for (auto& at : macquired)
                {
                    if (at.second)
                        acq[to_string (at.first)] = "acquired";
                    else
                        acq[to_string (at.first)] = "failed";
                }
                ret["acquired"] = acq;
            }

            if (!macquiring.empty ())
            {
                json::value acq (json::arrayvalue);
                for (auto& at : macquiring)
                {
                    acq.append (to_string (at.first));
                }
                ret["acquiring"] = acq;
            }

            if (!mdisputes.empty ())
            {
                json::value dsj (json::objectvalue);
                for (auto& dt : mdisputes)
                {
                    dsj[to_string (dt.first)] = dt.second->getjson ();
                }
                ret["disputes"] = dsj;
            }

            if (!mclosetimes.empty ())
            {
                json::value ctj (json::objectvalue);
                for (auto& ct : mclosetimes)
                {
                    ctj[beast::lexicalcastthrow <std::string> (ct.first)] = ct.second;
                }
                ret["close_times"] = ctj;
            }

            if (!mdeadnodes.empty ())
            {
                json::value dnj (json::arrayvalue);
                for (auto const& dn : mdeadnodes)
                {
                    dnj.append (to_string (dn));
                }
                ret["dead_nodes"] = dnj;
            }
        }

        return ret;
    }

    ledger::ref peekpreviousledger ()
    {
        return mpreviousledger;
    }

    uint256 getlcl ()
    {
        return mprevledgerhash;
    }

    /**
      get a transaction tree, fetching it from the network if required and
      requested.  when the transaction acquire engine successfully acquires
      a transaction set, it will call back.

      @param hash      hash of the requested transaction tree.
      @param doacquire true if we should get this from the network if we don't
                       already have it.
      @return          pointer to the transaction tree if we got it, else
                       nullptr.
    */
    shamap::pointer gettransactiontree (uint256 const& hash, bool doacquire)
    {
        auto it = macquired.find (hash);

        if (it != macquired.end ())
            return it->second;

        if (mstate == lcspre_close)
        {
            shamap::pointer currentmap
                = getapp().getledgermaster ().getcurrentledger ()
                    ->peektransactionmap ();

            if (currentmap->gethash () == hash)
            {
                writelog (lsdebug, ledgerconsensus)
                    << "map " << hash << " is our current";
                currentmap = currentmap->snapshot (false);
                mapcompleteinternal (hash, currentmap, false);
                return currentmap;
            }
        }

        if (doacquire)
        {
            transactionacquire::pointer& acquiring = macquiring[hash];

            if (!acquiring)
            {
                if (hash.iszero ())
                {
                    application& app = getapp();
                    shamap::pointer empty = std::make_shared<shamap> (
                        smttransaction, app.getfullbelowcache(),
                            app.gettreenodecache(), getapp().getnodestore(),
                                defaultmissingnodehandler(), deprecatedlogs().journal("shamap"));
                    mapcompleteinternal (hash, empty, false);
                    return empty;
                }

                acquiring = std::make_shared<transactionacquire> (hash, m_clock);
                startacquiring (acquiring);
            }
        }

        return shamap::pointer ();
    }

    /**
      we have a complete transaction set, typically acquired from the network

      @param hash     hash of the transaction set.
      @param map      the transaction set.
      @param acquired true if we have acquired the transaction set.
    */
    void mapcomplete (uint256 const& hash, shamap::ref map, bool acquired)
    {
        try
        {
            mapcompleteinternal (hash, map, acquired);
        }
        catch (shamapmissingnode const& mn)
        {
            leaveconsensus();
            writelog (lserror, ledgerconsensus) <<
                "missind node processing complete map " << mn;
            throw;
        }
    }

    void mapcompleteinternal (uint256 const& hash, shamap::ref map, bool acquired)
    {
        condlog (acquired, lsinfo, ledgerconsensus)
            << "we have acquired txs " << hash;

        if (!map)  // if the map was invalid
        {
            // this is an invalid/corrupt map
            macquired[hash] = map;
            macquiring.erase (hash);
            writelog (lswarning, ledgerconsensus)
                << "a trusted node directed us to acquire an invalid txn map";
            return;
        }

        assert (hash == map->gethash ());

        auto it = macquired.find (hash);

        // if we have already acquired this transaction set
        if (macquired.find (hash) != macquired.end ())
        {
            if (it->second)
            {
                macquiring.erase (hash);
                return; // we already have this map
            }

            // we previously failed to acquire this map, now we have it
            macquired.erase (hash);
        }

        // we now have a map that we did not have before

        if (mourposition && (!mourposition->isbowout ())
            && (hash != mourposition->getcurrenthash ()))
        {
            // this will create disputed transactions
            auto it2 = macquired.find (mourposition->getcurrenthash ());

            if (it2 != macquired.end ())
            {
                assert ((it2->first == mourposition->getcurrenthash ())
                    && it2->second);
                mcompares.insert(hash);
                // our position is not the same as the acquired position
                createdisputes (it2->second, map);
            }
            else
                assert (false); // we don't have our own position?!
        }
        else
            writelog (lsdebug, ledgerconsensus)
                << "not ready to create disputes";

        macquired[hash] = map;
        macquiring.erase (hash);

        // adjust tracking for each peer that takes this position
        std::vector<nodeid> peers;
        for (auto& it : mpeerpositions)
        {
            if (it.second->getcurrenthash () == map->gethash ())
                peers.push_back (it.second->getpeerid ());
        }

        if (!peers.empty ())
        {
            adjustcount (map, peers);
        }
        else
        {
            condlog (acquired, lswarning, ledgerconsensus)
                << "by the time we got the map "
                << hash << " no peers were proposing it";
        }

        // inform directly-connected peers that we have this transaction set
        sendhavetxset (hash, true);
    }

    /**
      determine if we still need to acquire a transaction set from the network.
      if a transaction set is popular, we probably have it. if it's unpopular,
      we probably don't need it (and the peer that initially made us
      retrieve it has probably already changed its position).

      @param hash hash of the transaction set.
      @return     true if we need to acquire it, else false.
    */
    bool stillneedtxset (uint256 const& hash)
    {
        if (macquired.find (hash) != macquired.end ())
            return false;

        for (auto const& it : mpeerpositions)
        {
            if (it.second->getcurrenthash () == hash)
                return true;
        }
        return false;
    }

    /**
      check if our last closed ledger matches the network's.
      this tells us if we are still in sync with the network.
      this also helps us if we enter the consensus round with
      the wrong ledger, to leave it with the correct ledger so
      that we can participate in the next round.
    */
    void checklcl ()
    {
        uint256 netlgr = mprevledgerhash;
        int netlgrcount = 0;

        uint256 favoredledger = mprevledgerhash; // don't jump forward
        uint256 priorledger;

        if (mhavecorrectlcl)
            priorledger = mpreviousledger->getparenthash (); // don't jump back

        // get validators that are on our ledger, or  "close" to being on
        // our ledger.
        hash_map<uint256, validationcounter> vals =
            getapp().getvalidations ().getcurrentvalidations
            (favoredledger, priorledger);

        for (auto& it : vals)
        {
            if ((it.second.first > netlgrcount) ||
                ((it.second.first == netlgrcount) && (it.first == mprevledgerhash)))
            {
               netlgr = it.first;
               netlgrcount = it.second.first;
            }
        }

        if (netlgr != mprevledgerhash)
        {
            // lcl change
            const char* status;

            switch (mstate)
            {
            case lcspre_close:
                status = "preclose";
                break;

            case lcsestablish:
                status = "establish";
                break;

            case lcsfinished:
                status = "finished";
                break;

            case lcsaccepted:
                status = "accepted";
                break;

            default:
                status = "unknown";
            }

            writelog (lswarning, ledgerconsensus)
                << "view of consensus changed during " << status
                << " (" << netlgrcount << ") status="
                << status << ", "
                << (mhavecorrectlcl ? "correctlcl" : "incorrectlcl");
            writelog (lswarning, ledgerconsensus) << mprevledgerhash
                << " to " << netlgr;
            writelog (lswarning, ledgerconsensus)
                << ripple::getjson (*mpreviousledger);

            if (shouldlog (lsdebug, ledgerconsensus))
            {
                for (auto& it : vals)
                {
                    writelog (lsdebug, ledgerconsensus)
                        << "v: " << it.first << ", " << it.second.first;
                }
            }

            if (mhavecorrectlcl)
                getapp().getops ().consensusviewchange ();

            handlelcl (netlgr);
        }
        else if (mpreviousledger->gethash () != mprevledgerhash)
            handlelcl (netlgr);
    }

    /**
      change our view of the last closed ledger

      @param lclhash hash of the last closed ledger.
    */
    void handlelcl (uint256 const& lclhash)
    {
        assert ((lclhash != mprevledgerhash) || (mpreviousledger->gethash () != lclhash));

        if (mprevledgerhash != lclhash)
        {
            // first time switching to this ledger
            mprevledgerhash = lclhash;

            if (mhavecorrectlcl && mproposing && mourposition)
            {
                writelog (lsinfo, ledgerconsensus) << "bowing out of consensus";
                mourposition->bowout ();
                propose ();
            }

            // stop proposing because we are out of sync
            mproposing = false;
            //      mvalidating = false;
            mpeerpositions.clear ();
            mdisputes.clear ();
            mclosetimes.clear ();
            mdeadnodes.clear ();
            // to get back in sync:
            playbackproposals ();
        }

        if (mpreviousledger->gethash () == mprevledgerhash)
            return;

        // we need to switch the ledger we're working from
        ledger::pointer newlcl = getapp().getledgermaster ().getledgerbyhash (mprevledgerhash);
        if (!newlcl)
        {
            if (macquiringledger != lclhash)
            {
                // need to start acquiring the correct consensus lcl
                writelog (lswarning, ledgerconsensus) << "need consensus ledger " << mprevledgerhash;

                // tell the ledger acquire system that we need the consensus ledger
                macquiringledger = mprevledgerhash;
                getapp().getjobqueue().addjob (jtadvance, "getconsensusledger",
                    std::bind (
                        &inboundledgers::findcreate,
                        &getapp().getinboundledgers(),
                        mprevledgerhash, 0, inboundledger::fcconsensus));
                mhavecorrectlcl = false;
            }
            return;
        }

        assert (newlcl->isclosed () && newlcl->isimmutable ());
        assert (newlcl->gethash () == lclhash);
        mpreviousledger = newlcl;
        mprevledgerhash = lclhash;

        writelog (lsinfo, ledgerconsensus) << "have the consensus ledger " << mprevledgerhash;
        mhavecorrectlcl = true;

        mcloseresolution = continuousledgertiming::getnextledgertimeresolution (
                               mpreviousledger->getcloseresolution (), mpreviousledger->getcloseagree (),
                               mpreviousledger->getledgerseq () + 1);
    }



    /**
      on timer call the correct handler for each state.
    */
    void timerentry ()
    {
        try
        {
           dotimer();
        }
        catch (shamapmissingnode const& mn)
        {
            leaveconsensus ();
            writelog (lserror, ledgerconsensus) <<
               "missing node during consensus process " << mn;
            throw;
        }
    }

    void dotimer ()
    {
        if ((mstate != lcsfinished) && (mstate != lcsaccepted))
            checklcl ();

        mcurrentmseconds =
            (boost::posix_time::microsec_clock::universal_time ()
            - mconsensusstarttime).total_milliseconds ();
        mclosepercent = mcurrentmseconds * 100 / mpreviousmseconds;

        switch (mstate)
        {
        case lcspre_close:
            statepreclose ();
            return;

        case lcsestablish:
            stateestablish ();

            if (mstate != lcsfinished) return;

            // fall through

        case lcsfinished:
            statefinished ();

            if (mstate != lcsaccepted) return;

            // fall through

        case lcsaccepted:
            stateaccepted ();
            return;
        }

        assert (false);
    }

    /**
      handle pre-close state.
    */
    void statepreclose ()
    {
        // it is shortly before ledger close time
        bool anytransactions
            = getapp().getledgermaster ().getcurrentledger ()
            ->peektransactionmap ()->gethash ().isnonzero ();
        int proposersclosed = mpeerpositions.size ();
        int proposersvalidated
            = getapp().getvalidations ().gettrustedvalidationcount
            (mprevledgerhash);

        // this ledger is open. this computes how long since last ledger closed
        int sinceclose;
        int idleinterval = 0;

        if (mhavecorrectlcl && mpreviousledger->getcloseagree ())
        {
            // we can use consensus timing
            sinceclose = 1000 * (getapp().getops ().getclosetimenc ()
                - mpreviousledger->getclosetimenc ());
            idleinterval = 2 * mpreviousledger->getcloseresolution ();

            if (idleinterval < ledger_idle_interval)
                idleinterval = ledger_idle_interval;
        }
        else
        {
            // use the time we saw the last ledger close
            sinceclose = 1000 * (getapp().getops ().getclosetimenc ()
                - getapp().getops ().getlastclosetime ());
            idleinterval = ledger_idle_interval;
        }

        idleinterval = std::max (idleinterval, ledger_idle_interval);
        idleinterval = std::max (idleinterval, 2 * mpreviousledger->getcloseresolution ());

        // decide if we should close the ledger
        if (continuousledgertiming::shouldclose (anytransactions
            , mpreviousproposers, proposersclosed, proposersvalidated
            , mpreviousmseconds, sinceclose, mcurrentmseconds
            , idleinterval))
        {
            closeledger ();
        }
    }

    /** we are establishing a consensus
       update our position only on the timer, and in this state.
       if we have consensus, move to the finish state
    */
    void stateestablish ()
    {

        // give everyone a chance to take an initial position
        if (mcurrentmseconds < ledger_min_consensus)
            return;

        updateourpositions ();

        if (!mhaveclosetimeconsensus)
        {
            condlog (haveconsensus (false), lsinfo, ledgerconsensus)
                << "we have tx consensus but not ct consensus";
        }
        else if (haveconsensus (true))
        {
            writelog (lsinfo, ledgerconsensus)
                << "converge cutoff (" << mpeerpositions.size ()
                << " participants)";
            mstate = lcsfinished;
            beginaccept (false);
        }
    }

    void statefinished ()
    {
        // we are processing the finished ledger
        // logic of calculating next ledger advances us out of this state
        // nothing to do
    }
    void stateaccepted ()
    {
        // we have accepted a new ledger
        endconsensus ();
    }

    /** check if we've reached consensus
    */
    bool haveconsensus (bool forreal)
    {
        // checkme: should possibly count unacquired tx sets as disagreeing
        int agree = 0, disagree = 0;
        uint256 ourposition = mourposition->getcurrenthash ();

        // count number of agreements/disagreements with our position
        for (auto& it : mpeerpositions)
        {
            if (!it.second->isbowout ())
            {
                if (it.second->getcurrenthash () == ourposition)
                {
                    ++agree;
                }
                else
                {
                    writelog (lsdebug, ledgerconsensus) << to_string (it.first)
                        << " has " << to_string (it.second->getcurrenthash ());
                    ++disagree;
                    if (mcompares.count(it.second->getcurrenthash()) == 0)
                    { // make sure we have generated disputes
                        uint256 hash = it.second->getcurrenthash();
                        writelog (lsdebug, ledgerconsensus)
                            << "we have not compared to " << hash;
                        auto it1 = macquired.find (hash);
                        auto it2 = macquired.find(mourposition->getcurrenthash ());
                        if ((it1 != macquired.end()) && (it2 != macquired.end())
                            && (it1->second) && (it2->second))
                        {
                            mcompares.insert(hash);
                            createdisputes(it2->second, it1->second);
                        }
                    }
                }
            }
        }
        int currentvalidations = getapp().getvalidations ()
            .getnodesafter (mprevledgerhash);

        writelog (lsdebug, ledgerconsensus)
            << "checking for tx consensus: agree=" << agree
            << ", disagree=" << disagree;

        // determine if we actually have consensus or not
        return continuousledgertiming::haveconsensus (mpreviousproposers,
            agree + disagree, agree, currentvalidations
            , mpreviousmseconds, mcurrentmseconds, forreal, mconsensusfail);
    }

    /**
      a server has taken a new position, adjust our tracking
      called when a peer takes a new postion.

      @param newposition the new position
      @return            true if we should do delayed relay of this position.
    */
    bool peerposition (ledgerproposal::ref newposition)
    {
        auto peerid = newposition->getpeerid ();

        if (mdeadnodes.find (peerid) != mdeadnodes.end ())
        {
            writelog (lsinfo, ledgerconsensus)
                << "position from dead node: " << to_string (peerid);
            return false;
        }

        ledgerproposal::pointer& currentposition = mpeerpositions[peerid];

        if (currentposition)
        {
            assert (peerid == currentposition->getpeerid ());

            if (newposition->getproposeseq ()
                <= currentposition->getproposeseq ())
            {
                return false;
            }
        }

        if (newposition->getproposeseq () == 0)
        {
            // new initial close time estimate
            writelog (lstrace, ledgerconsensus)
                << "peer reports close time as "
                << newposition->getclosetime ();
            ++mclosetimes[newposition->getclosetime ()];
        }
        else if (newposition->getproposeseq () == ledgerproposal::seqleave)
        {
            // peer bows out
            writelog (lsinfo, ledgerconsensus)
                << "peer bows out: " << to_string (peerid);
            for (auto& it : mdisputes)
                it.second->unvote (peerid);
            mpeerpositions.erase (peerid);
            mdeadnodes.insert (peerid);
            return true;
        }


        writelog (lstrace, ledgerconsensus) << "processing peer proposal "
            << newposition->getproposeseq () << "/"
            << newposition->getcurrenthash ();
        currentposition = newposition;

        shamap::pointer set
            = gettransactiontree (newposition->getcurrenthash (), true);

        if (set)
        {
            for (auto& it : mdisputes)
                it.second->setvote (peerid, set->hasitem (it.first));
        }
        else
        {
            writelog (lsdebug, ledgerconsensus)
                << "don't have tx set for peer";
        }

        return true;
    }

    /**
      a peer has informed us that it can give us a transaction set

      @param peer    the peer we can get it from.
      @param hashset the transaction set we can get.
      @param status  says whether or not the peer has the transaction set
                     locally.
      @return        true if we have or acquire the transaction set.
    */
    bool peerhasset (peer::ptr const& peer, uint256 const& hashset
        , protocol::txsetstatus status)
    {
        if (status != protocol::tshave) // indirect requests for future support
            return true;

        std::vector< std::weak_ptr<peer> >& set = mpeerdata[hashset];
        for (std::weak_ptr<peer>& iit : set)
            if (iit.lock () == peer)
                return false;
        set.push_back (peer);

        auto acq (macquiring.find (hashset));

        if (acq != macquiring.end ())
            getapp().getjobqueue().addjob(jttxn_data, "peerhastxndata",
                std::bind(&transactionacquire::peerhasvoid, acq->second, peer));

        return true;
    }

    /**
      a peer has sent us some nodes from a transaction set

      @param peer     the peer which has sent the nodes
      @param sethash  the transaction set
      @param nodeids  the nodes in the transaction set
      @param nodedata the data
      @return         the status results of adding the nodes.
    */
    shamapaddnode peergavenodes (peer::ptr const& peer
        , uint256 const& sethash, const std::list<shamapnodeid>& nodeids
        , const std::list< blob >& nodedata)
    {
        auto acq (macquiring.find (sethash));

        if (acq == macquiring.end ())
        {
            writelog (lsdebug, ledgerconsensus)
                << "got tx data for set no longer acquiring: " << sethash;
            return shamapaddnode ();
        }
        // we must keep the set around during the function
        transactionacquire::pointer set = acq->second;
        return set->takenodes (nodeids, nodedata, peer);
    }

    bool isourpubkey (const rippleaddress & k)
    {
        return k == mvalpublic;
    }

    /** simulate a consensus round without any network traffic
    */
    void simulate ()
    {
        writelog (lsinfo, ledgerconsensus) << "simulating consensus";
        closeledger ();
        mcurrentmseconds = 100;
        beginaccept (true);
        endconsensus ();
        writelog (lsinfo, ledgerconsensus) << "simulation complete";
    }
private:
    /** we have a new last closed ledger, process it. final accept logic

      @param set our consensus set
    */
    void accept (shamap::pointer set)
    {

        {
            application::scopedlocktype lock
                (getapp ().getmasterlock ());

            // put our set where others can get it later
            if (set->gethash ().isnonzero ())
               getapp().getops ().takeposition (
                   mpreviousledger->getledgerseq (), set);

            assert (set->gethash () == mourposition->getcurrenthash ());
            // these are now obsolete
            getapp().getops ().peekstoredproposals ().clear ();

            std::uint32_t closetime = roundclosetime (mourposition->getclosetime ());
            bool closetimecorrect = true;

            if (closetime == 0)
            {
                // we agreed to disagree
                closetimecorrect = false;
                closetime = mpreviousledger->getclosetimenc () + 1;
            }

            writelog (lsdebug, ledgerconsensus)
                << "report: prop=" << (mproposing ? "yes" : "no")
                << " val=" << (mvalidating ? "yes" : "no")
                << " corlcl=" << (mhavecorrectlcl ? "yes" : "no")
                << " fail=" << (mconsensusfail ? "yes" : "no");
            writelog (lsdebug, ledgerconsensus)
                << "report: prev = " << mprevledgerhash
                << ":" << mpreviousledger->getledgerseq ();
            writelog (lsdebug, ledgerconsensus)
                << "report: txst = " << set->gethash ()
                << ", close " << closetime << (closetimecorrect ? "" : "x");

            // put failed transactions into a deterministic order
            canonicaltxset retriabletransactions (set->gethash ());

            // build the new last closed ledger
            ledger::pointer newlcl
                = std::make_shared<ledger> (false
                , *mpreviousledger);

            // set up to write shamap changes to our database,
            //   perform updates, extract changes
            writelog (lsdebug, ledgerconsensus)
                << "applying consensus set transactions to the"
                << " last closed ledger";
            applytransactions (set, newlcl, newlcl, retriabletransactions, false);
            newlcl->updateskiplist ();
            newlcl->setclosed ();

            int asf = newlcl->peekaccountstatemap ()->flushdirty (
                hotaccount_node, newlcl->getledgerseq());
            int tmf = newlcl->peektransactionmap ()->flushdirty (
                hottransaction_node, newlcl->getledgerseq());
            writelog (lsdebug, ledgerconsensus) << "flushed " << asf << " account and " <<
                tmf << "transaction nodes";

            // accept ledger
            newlcl->setaccepted (closetime, mcloseresolution, closetimecorrect);

            // and stash the ledger in the ledger master
            if (getapp().getledgermaster().storeledger (newlcl))
                writelog (lsdebug, ledgerconsensus)
                    << "consensus built ledger we already had";
            else if (getapp().getinboundledgers().find (newlcl->gethash()))
                writelog (lsdebug, ledgerconsensus)
                    << "consensus built ledger we were acquiring";
            else
                writelog (lsdebug, ledgerconsensus)
                    << "consensus built new ledger";

            writelog (lsdebug, ledgerconsensus)
                << "report: newl  = " << newlcl->gethash ()
                << ":" << newlcl->getledgerseq ();
            uint256 newlclhash = newlcl->gethash ();
            // tell directly connected peers that we have a new lcl
            statuschange (protocol::neaccepted_ledger, *newlcl);

            if (mvalidating && !mconsensusfail)
            {
                // build validation
                uint256 signinghash;
                stvalidation::pointer v =
                    std::make_shared<stvalidation>
                    (newlclhash, getapp().getops ().getvalidationtimenc ()
                    , mvalpublic, mproposing);
                v->setfieldu32 (sfledgersequence, newlcl->getledgerseq ());
                addload(v);  // our network load

                if (((newlcl->getledgerseq () + 1) % 256) == 0)
                // next ledger is flag ledger
                {
                    // suggest fee changes and new features
                    m_feevote.dovalidation (newlcl, *v);
                    getapp().getamendmenttable ().dovalidation (newlcl, *v);
                }
#ifdef moorecoin_async_dividend
                else if (m_dividendvote.isapplyledger(newlcl))
                {
                    m_dividendvote.doapplyvalidation(newlcl, *v);
                }
#endif  //moorecoin_async_dividend
                else if (m_dividendvote.isstartledger(newlcl))
                {
                    m_dividendvote.dostartvalidation(newlcl, *v);
                }

                v->sign (signinghash, mvalprivate);
                v->settrusted ();
                // suppress it if we receive it - fixme: wrong suppression
                getapp().gethashrouter ().addsuppression (signinghash);
                getapp().getvalidations ().addvalidation (v, "local");
                getapp().getops ().setlastvalidation (v);
                blob validation = v->getsigned ();
                protocol::tmvalidation val;
                val.set_validation (&validation[0], validation.size ());
                // send signed validation to all of our directly connected peers
                getapp ().overlay ().foreach (send_always (
                    std::make_shared <message> (
                        val, protocol::mtvalidation)));
                writelog (lsinfo, ledgerconsensus)
                    << "cnf val " << newlclhash;
            }
            else
                writelog (lsinfo, ledgerconsensus)
                    << "cnf newlcl " << newlclhash;

            // see if we can accept a ledger as fully-validated
            getapp().getledgermaster().consensusbuilt (newlcl);

            // build new open ledger
            ledger::pointer newol = std::make_shared<ledger>
                (true, *newlcl);
            ledgermaster::scopedlocktype sl
                (getapp().getledgermaster ().peekmutex ());

            // apply disputed transactions that didn't get in
            transactionengine engine (newol);
            bool anydisputes = false;
            for (auto& it : mdisputes)
            {
                if (!it.second->getourvote ())
                {
                    // we voted no
                    try
                    {
                        writelog (lsdebug, ledgerconsensus)
                            << "test applying disputed transaction that did"
                            << " not get in";
                        serializeriterator sit (it.second->peektransaction ());
                        sttx::pointer txn
                            = std::make_shared<sttx>(sit);

                        retriabletransactions.push_back (txn);
                        anydisputes = true;
                    }
                    catch (...)
                    {
                        writelog (lsdebug, ledgerconsensus)
                            << "failed to apply transaction we voted no on";
                    }
                }
            }
            if (anydisputes)
            {
                applytransactions (std::shared_ptr<shamap>(),
                    newol, newlcl, retriabletransactions, true);
            }

            {
                // apply transactions from the old open ledger
                ledger::pointer oldol = getapp().getledgermaster().getcurrentledger();
                if (oldol->peektransactionmap()->gethash().isnonzero ())
                {
                    writelog (lsdebug, ledgerconsensus)
                        << "applying transactions from current open ledger";
                    applytransactions (oldol->peektransactionmap (),
                        newol, newlcl, retriabletransactions, true);
                }
            }

            {
                // apply local transactions
                transactionengine engine (newol);
                m_localtx.apply (engine);
            }

            // we have a new last closed ledger and new open ledger
            getapp().getledgermaster ().pushledger (newlcl, newol);
            mnewledgerhash = newlcl->gethash ();
            mstate = lcsaccepted;
            sl.unlock ();

            if (mvalidating)
            {
                // see how close our close time is to other node's
                //  close time reports, and update our clock.
                writelog (lsinfo, ledgerconsensus)
                    << "we closed at "
                    << beast::lexicalcastthrow <std::string> (mclosetime);
                std::uint64_t closetotal = mclosetime;
                int closecount = 1;

                for (auto it = mclosetimes.begin ()
                    , end = mclosetimes.end (); it != end; ++it)
                {
                    // fixme: use median, not average
                    writelog (lsinfo, ledgerconsensus)
                        << beast::lexicalcastthrow <std::string> (it->second)
                        << " time votes for "
                        << beast::lexicalcastthrow <std::string> (it->first);
                    closecount += it->second;
                    closetotal += static_cast<std::uint64_t>
                        (it->first) * static_cast<std::uint64_t> (it->second);
                }

                closetotal += (closecount / 2);
                closetotal /= closecount;
                int offset = static_cast<int> (closetotal)
                    - static_cast<int> (mclosetime);
                writelog (lsinfo, ledgerconsensus)
                    << "our close offset is estimated at "
                    << offset << " (" << closecount << ")";
                getapp().getops ().closetimeoffset (offset);
            }
        }
    }

    /**
      begin acquiring a transaction set

      @param acquire the transaction set to acquire.
    */
    void startacquiring (transactionacquire::pointer acquire)
    {
        auto it = mpeerdata.find (acquire->gethash ());

        if (it != mpeerdata.end ())
        {
            // add any peers we already know have his transaction set
            std::vector< std::weak_ptr<peer> >& peerlist = it->second;
            std::vector< std::weak_ptr<peer> >::iterator pit
                = peerlist.begin ();

            while (pit != peerlist.end ())
            {
                peer::ptr pr = pit->lock ();

                if (!pr)
                {
                    pit = peerlist.erase (pit);
                }
                else
                {
                    acquire->peerhas (pr);
                    ++pit;
                }
            }
        }

        struct build_acquire_list
        {
            typedef void return_type;

            transactionacquire::pointer const& acquire;

            build_acquire_list (transactionacquire::pointer const& acq)
                : acquire(acq)
            { }

            return_type operator() (peer::ptr const& peer) const
            {
                if (peer->hastxset (acquire->gethash ()))
                    acquire->peerhas (peer);
            }
        };

        getapp().overlay ().foreach (build_acquire_list (acquire));

        acquire->settimer ();
    }

    /**
      compare two proposed transaction sets and create disputed
        transctions structures for any mismatches

      @param m1 one transaction set
      @param m2 the other transaction set
    */
    void createdisputes (shamap::ref m1, shamap::ref m2)
    {
        if (m1->gethash() == m2->gethash())
            return;

        writelog (lsdebug, ledgerconsensus) << "createdisputes "
            << m1->gethash() << " to " << m2->gethash();
        shamap::delta differences;
        m1->compare (m2, differences, 16384);

        int dc = 0;
        // for each difference between the transactions
        for (auto& pos : differences)
        {
            ++dc;
            // create disputed transactions (from the ledger that has them)
            if (pos.second.first)
            {
                // transaction is only in first map
                assert (!pos.second.second);
                adddisputedtransaction (pos.first
                    , pos.second.first->peekdata ());
            }
            else if (pos.second.second)
            {
                // transaction is only in second map
                assert (!pos.second.first);
                adddisputedtransaction (pos.first
                    , pos.second.second->peekdata ());
            }
            else // no other disagreement over a transaction should be possible
                assert (false);
        }
        writelog (lsdebug, ledgerconsensus) << dc << " differences found";
    }

    /**
      add a disputed transaction (one that at least one node wants
      in the consensus set and at least one node does not) to our tracking

      @param txid the id of the disputed transaction
      @param tx   the data of the disputed transaction
    */
    void adddisputedtransaction (uint256 const& txid, blob const& tx)
    {
        if (mdisputes.find (txid) != mdisputes.end ())
            return;

        writelog (lsdebug, ledgerconsensus) << "transaction "
            << txid << " is disputed";

        bool ourvote = false;

        // update our vote on the disputed transaction
        if (mourposition)
        {
            auto mit (macquired.find (mourposition->getcurrenthash ()));

            if (mit != macquired.end ())
                ourvote = mit->second->hasitem (txid);
            else
                assert (false); // we don't have our own position?
        }

        disputedtx::pointer txn = std::make_shared<disputedtx>
            (txid, tx, ourvote);
        mdisputes[txid] = txn;

        // update all of the peer's votes on the disputed transaction
        for (auto& pit : mpeerpositions)
        {
            auto cit (macquired.find (pit.second->getcurrenthash ()));

            if ((cit != macquired.end ()) && cit->second)
            {
                txn->setvote (pit.first, cit->second->hasitem (txid));
            }
        }

        // if we didn't relay this transaction recently, relay it
        if (getapp().gethashrouter ().setflag (txid, sf_relayed))
        {
            protocol::tmtransaction msg;
            msg.set_rawtransaction (& (tx.front ()), tx.size ());
            msg.set_status (protocol::tsnew);
            msg.set_receivetimestamp (getapp().getops ().getnetworktimenc ());
            getapp ().overlay ().foreach (send_always (
                std::make_shared<message> (
                    msg, protocol::mttransaction)));
        }
    }

    /**
      adjust the votes on all disputed transactions based
        on the set of peers taking this position

      @param map   a disputed position
      @param peers peers which are taking the position map
    */
    void adjustcount (shamap::ref map, const std::vector<nodeid>& peers)
    {
        for (auto& it : mdisputes)
        {
            bool sethas = map->hasitem (it.second->gettransactionid ());
            for (auto const& pit : peers)
                it.second->setvote (pit, sethas);
        }
    }

    /**
      revoke our outstanding proposal, if any, and
      cease proposing at least until this round ends
    */
    void leaveconsensus ()
    {
        if (mproposing)
        {
            if (mourposition && ! mourposition->isbowout ())
            {
                mourposition->bowout();
                propose();
            }
            mproposing = false;
        }
    }

    /** make and send a proposal
    */
    void propose ()
    {
        writelog (lstrace, ledgerconsensus) << "we propose: " <<
            (mourposition->isbowout ()
                ? std::string ("bowout")
                : to_string (mourposition->getcurrenthash ()));
        protocol::tmproposeset prop;

        prop.set_currenttxhash (mourposition->getcurrenthash ().begin ()
            , 256 / 8);
        prop.set_previousledger (mourposition->getprevledger ().begin ()
            , 256 / 8);
        prop.set_proposeseq (mourposition->getproposeseq ());
        prop.set_closetime (mourposition->getclosetime ());

        blob pubkey = mourposition->getpubkey ();
        blob sig = mourposition->sign ();
        prop.set_nodepubkey (&pubkey[0], pubkey.size ());
        prop.set_signature (&sig[0], sig.size ());
        getapp ().overlay ().foreach (send_always (
            std::make_shared<message> (
                prop, protocol::mtpropose_ledger)));
    }

    /** let peers know that we a particular transactions set so they
       can fetch it from us.

      @param hash   the id of the transaction.
      @param direct true if we have this transaction set locally, else a
                    directly connected peer has it.
    */
    void sendhavetxset (uint256 const& hash, bool direct)
    {
        protocol::tmhavetransactionset msg;
        msg.set_hash (hash.begin (), 256 / 8);
        msg.set_status (direct ? protocol::tshave : protocol::tscan_get);
        getapp ().overlay ().foreach (send_always (
            std::make_shared <message> (
                msg, protocol::mthave_set)));
    }

    /**
      round the close time to the close time resolution.

      @param closetime the time to be rouned.
      @return          the rounded close time.
    */
    std::uint32_t roundclosetime (std::uint32_t closetime)
    {
        return ledger::roundclosetime (closetime, mcloseresolution);
    }

    /** send a node status change message to our directly connected peers

      @param event   the event which caused the status change.  this is
                     typically neaccepted_ledger or neclosing_ledger.
      @param ledger  the ledger associated with the event.
    */
    void statuschange (protocol::nodeevent event, ledger& ledger)
    {
        protocol::tmstatuschange s;

        if (!mhavecorrectlcl)
            s.set_newevent (protocol::nelost_sync);
        else
            s.set_newevent (event);

        s.set_ledgerseq (ledger.getledgerseq ());
        s.set_networktime (getapp().getops ().getnetworktimenc ());
        uint256 hash = ledger.getparenthash ();
        s.set_ledgerhashprevious (hash.begin (), hash.size ());
        hash = ledger.gethash ();
        s.set_ledgerhash (hash.begin (), hash.size ());

        std::uint32_t umin, umax;
        if (!getapp().getops ().getfullvalidatedrange (umin, umax))
        {
            umin = 0;
            umax = 0;
        }
        else
        {
            // don't advertise ledgers we're not willing to serve
            std::uint32_t early = getapp().getledgermaster().getearliestfetch ();
            if (umin < early)
               umin = early;
        }
        s.set_firstseq (umin);
        s.set_lastseq (umax);
        getapp ().overlay ().foreach (send_always (
            std::make_shared <message> (
                s, protocol::mtstatus_change)));
        writelog (lstrace, ledgerconsensus) << "send status change to peer";
    }

    /** take an initial position on what we think the consensus should be
        based on the transactions that made it into our open ledger

      @param initialledger the ledger that contains our initial position.
    */
    void takeinitialposition(ledger& initialledger)
    {
        shamap::pointer initialset;

        if (getconfig().run_standalone || (mproposing && mhavecorrectlcl))
        {
            if ((mpreviousledger->getledgerseq() % 256) == 0)
            {
                // previous ledger was flag ledger
                shamap::pointer preset
                    = initialledger.peektransactionmap()->snapshot(true);
                m_feevote.dovoting(mpreviousledger, preset);
                getapp().getamendmenttable().dovoting(mpreviousledger, preset);
                initialset = preset->snapshot(false);
            }
#ifdef moorecoin_async_dividend
            else if (mpreviousledger->isdividendstarted())
            {
                dividendmaster::pointer dividendmaster = getapp().getops().getdividendmaster();
                if (dividendmaster->trylock())
                {
                    if (!dividendmaster->isready() && !dividendmaster->isrunning())
                    {
                        getapp().getjobqueue().addjob(jtdividend,
                                                      "calcdividend",
                                                      std::bind(&dividendmaster::calcdividend, mpreviousledger));
                    }
                    dividendmaster->unlock();
                }
                if (m_dividendvote.isapplyledger(mpreviousledger))
                {
                    shamap::pointer preset = initialledger.peektransactionmap()->snapshot(true);
                    if (!m_dividendvote.doapplyvoting(mpreviousledger, preset))
                    {
                        writelog(lswarning, ledgerconsensus) << "we are missing a dividend apply";
                        throw;
                    }
                    initialset = preset->snapshot(false);
                }
            }
#else
            else if (mpreviousledger->isdividendstarted())
            {
                dividendmaster::pointer dividendmaster = getapp().getops().getdividendmaster();
                dividendmaster->setready(false);
                dividendmaster->calcdividend(mpreviousledger);
                if (dividendmaster->isready())
                {
                    shamap::pointer preset = initialledger.peektransactionmap()->snapshot(true);
                    dividendmaster->filldivresult(preset);
                    dividendmaster->filldivready(preset);
                    initialset = preset->snapshot(false);
                }
            }
#endif  //moorecoin_async_dividend
            else if (m_dividendvote.isstartledger(mpreviousledger))
            {
                writelog(lsinfo, ledgerconsensus) << "moorecoin: time for dividend";
                shamap::pointer preset = initialledger.peektransactionmap()->snapshot(true);
                m_dividendvote.dostartvoting(mpreviousledger, preset);
                initialset = preset->snapshot(false);
            }
        }

        if (initialset == nullptr)
        {
            initialset = initialledger.peektransactionmap()->snapshot(false);
        }

        // tell the ledger master not to acquire the ledger we're probably building
        getapp().getledgermaster().setbuildingledger (mpreviousledger->getledgerseq () + 1);

        uint256 txset = initialset->gethash ();
        writelog (lsinfo, ledgerconsensus) << "initial position " << txset;
        mapcompleteinternal (txset, initialset, false);

        if (mvalidating)
        {
            mourposition = std::make_shared<ledgerproposal>
                           (mvalpublic, mvalprivate
                            , initialledger.getparenthash ()
                            , txset, mclosetime);
        }
        else
        {
            mourposition
                = std::make_shared<ledgerproposal>
                (initialledger.getparenthash (), txset, mclosetime);
        }

        for (auto& it : mdisputes)
        {
            it.second->setourvote (initialledger.hastransaction (it.first));
        }

        // if any peers have taken a contrary position, process disputes
        hash_set<uint256> found;

        for (auto& it : mpeerpositions)
        {
            uint256 set = it.second->getcurrenthash ();

            if (found.insert (set).second)
            {
                auto iit (macquired.find (set));

                if (iit != macquired.end ())
                {
                    mcompares.insert(iit->second->gethash());
                    createdisputes (initialset, iit->second);
                }
            }
        }

        if (mproposing)
            propose ();
    }

    /**
      for a given number of participants and required percent
      for consensus, how many participants must agree?

      @param size    number of validators
      @param percent desired percent for consensus
      @return number of participates which must agree
    */
    static int computepercent (int size, int percent)
    {
        int result = ((size * percent) + (percent / 2)) / 100;
        return (result == 0) ? 1 : result;
    }

    /**
       called while trying to avalanche towards consensus.
       adjusts our positions to try to agree with other validators.
    */
    void updateourpositions ()
    {
        // compute a cutoff time
        boost::posix_time::ptime peercutoff
            = boost::posix_time::second_clock::universal_time ();
        boost::posix_time::ptime ourcutoff
            = peercutoff - boost::posix_time::seconds (propose_interval);
        peercutoff -= boost::posix_time::seconds (propose_freshness);

        bool changes = false;
        shamap::pointer ourposition;
        //  std::vector<uint256> addedtx, removedtx;

        // verify freshness of peer positions and compute close times
        std::map<std::uint32_t, int> closetimes;
        auto it = mpeerpositions.begin ();

        while (it != mpeerpositions.end ())
        {
            if (it->second->isstale (peercutoff))
            {
                // peer's proposal is stale, so remove it
                auto const& peerid = it->second->getpeerid ();
                writelog (lswarning, ledgerconsensus)
                    << "removing stale proposal from " << peerid;
                for (auto& dt : mdisputes)
                    dt.second->unvote (peerid);
                it = mpeerpositions.erase (it);
            }
            else
            {
                // proposal is still fresh
                ++closetimes[roundclosetime (it->second->getclosetime ())];
                ++it;
            }
        }

        // update votes on disputed transactions
        for (auto& it : mdisputes)
        {
            // because the threshold for inclusion increases,
            //  time can change our position on a dispute
            if (it.second->updatevote (mclosepercent, mproposing))
            {
                if (!changes)
                {
                    ourposition = macquired[mourposition->getcurrenthash ()]
                        ->snapshot (true);
                    assert (ourposition);
                    changes = true;
                }

                if (it.second->getourvote ()) // now a yes
                {
                    ourposition->additem (shamapitem (it.first
                        , it.second->peektransaction ()), true, false);
                    //              addedtx.push_back(it.first);
                }
                else // now a no
                {
                    ourposition->delitem (it.first);
                    //              removedtx.push_back(it.first);
                }
            }
        }

        int neededweight;

        if (mclosepercent < av_mid_consensus_time)
            neededweight = av_init_consensus_pct;
        else if (mclosepercent < av_late_consensus_time)
            neededweight = av_mid_consensus_pct;
        else if (mclosepercent < av_stuck_consensus_time)
            neededweight = av_late_consensus_pct;
        else
            neededweight = av_stuck_consensus_pct;

        std::uint32_t closetime = 0;
        mhaveclosetimeconsensus = false;

        if (mpeerpositions.empty ())
        {
            // no other times
            mhaveclosetimeconsensus = true;
            closetime = roundclosetime (mourposition->getclosetime ());
        }
        else
        {
            int participants = mpeerpositions.size ();
            if (mproposing)
            {
                ++closetimes[roundclosetime (mourposition->getclosetime ())];
                ++participants;
            }

            // threshold for non-zero vote
            int threshvote = computepercent (participants, neededweight);

            // threshold to declare consensus
            int threshconsensus = computepercent (participants, av_ct_consensus_pct);

            writelog (lsinfo, ledgerconsensus) << "proposers:"
                << mpeerpositions.size () << " nw:" << neededweight
                << " thrv:" << threshvote << " thrc:" << threshconsensus;

            for (auto it = closetimes.begin ()
                , end = closetimes.end (); it != end; ++it)
            {
                writelog (lsdebug, ledgerconsensus) << "cctime: seq"
                    << mpreviousledger->getledgerseq () + 1 << ": "
                    << it->first << " has " << it->second << ", "
                    << threshvote << " required";

                if (it->second >= threshvote)
                {
                    writelog (lsdebug, ledgerconsensus)
                        << "close time consensus reached: " << it->first;
                    closetime = it->first;
                    threshvote = it->second;

                    if (threshvote >= threshconsensus)
                        mhaveclosetimeconsensus = true;
                }
            }

            // if we agree to disagree on the close time, don't delay consensus
            if (!mhaveclosetimeconsensus && (closetimes[0] > threshconsensus))
            {
                closetime = 0;
                mhaveclosetimeconsensus = true;
            }

            condlog (!mhaveclosetimeconsensus, lsdebug, ledgerconsensus)
                << "no ct consensus: proposers:" << mpeerpositions.size ()
                << " proposing:" << (mproposing ? "yes" : "no") << " thresh:"
                << threshconsensus << " pos:" << closetime;
        }

        if (!changes &&
                ((closetime != roundclosetime (mourposition->getclosetime ()))
                || mourposition->isstale (ourcutoff)))
        {
            // close time changed or our position is stale
            ourposition = macquired[mourposition->getcurrenthash ()]
                ->snapshot (true);
            assert (ourposition);
            changes = true; // we pretend our position changed to force
        }                   //   a new proposal

        if (changes)
        {
            uint256 newhash = ourposition->gethash ();
            writelog (lsinfo, ledgerconsensus)
                << "position change: ctime " << closetime
                << ", tx " << newhash;

            if (mourposition->changeposition (newhash, closetime))
            {
                if (mproposing)
                    propose ();

                mapcompleteinternal (newhash, ourposition, false);
            }
        }
    }

    /** if we radically changed our consensus context for some reason,
        we need to replay recent proposals so that they're not lost.
    */
    void playbackproposals ()
    {
        for (auto it: getapp().getops ().peekstoredproposals ())
        {
            bool relay = false;
            for (auto proposal : it.second)
            {
                if (proposal->hassignature ())
                {
                    // we have the signature but don't know the
                    //  ledger so couldn't verify
                    proposal->setprevledger (mprevledgerhash);

                    if (proposal->checksign ())
                    {
                        writelog (lsinfo, ledgerconsensus)
                            << "applying stored proposal";
                        relay = peerposition (proposal);
                    }
                }
                else if (proposal->isprevledger (mprevledgerhash))
                    relay = peerposition (proposal);

                if (relay)
                {
                    writelog (lswarning, ledgerconsensus)
                        << "we should do delayed relay of this proposal,"
                        << " but we cannot";
                }

    #if 0
    // fixme: we can't do delayed relay because we don't have the signature
                std::set<peer::id_t> peers

                if (relay && getapp().gethashrouter ().swapset (proposal.getsuppress (), set, sf_relayed))
                {
                    writelog (lsdebug, ledgerconsensus) << "stored proposal delayed relay";
                    protocol::tmproposeset set;
                    set.set_proposeseq
                    set.set_currenttxhash (, 256 / 8);
                    previousledger
                    closetime
                    nodepubkey
                    signature
                    getapp ().overlay ().foreach (send_if_not (
                        std::make_shared<message> (
                            set, protocol::mtpropose_ledger),
                                peer_in_set(peers)));
                }

    #endif
            }
        }
    }

    /** we have just decided to close the ledger. start the consensus timer,
       stash the close time, inform peers, and take a position
    */
    void closeledger ()
    {
        checkourvalidation ();
        mstate = lcsestablish;
        mconsensusstarttime
            = boost::posix_time::microsec_clock::universal_time ();
        mclosetime = getapp().getops ().getclosetimenc ();
        getapp().getops ().setlastclosetime (mclosetime);
        statuschange (protocol::neclosing_ledger, *mpreviousledger);
        getapp().getledgermaster().applyheldtransactions ();
        takeinitialposition (*getapp().getledgermaster ().getcurrentledger ());
    }

    /**
      if we missed a consensus round, we may be missing a validation.
      this will send an older owed validation if we previously missed it.
    */
    void checkourvalidation ()
    {
        // this only covers some cases - fix for the case where we can't ever acquire the consensus ledger
        if (!mhavecorrectlcl || !mvalpublic.isset ()
            || !mvalprivate.isset ()
            || getapp().getops ().isneednetworkledger ())
        {
            return;
        }

        stvalidation::pointer lastval
            = getapp().getops ().getlastvalidation ();

        if (lastval)
        {
            if (lastval->getfieldu32 (sfledgersequence)
                == mpreviousledger->getledgerseq ())
            {
                return;
            }
            if (lastval->getledgerhash () == mprevledgerhash)
                return;
        }

        uint256 signinghash;
        stvalidation::pointer v
            = std::make_shared<stvalidation>
            (mpreviousledger->gethash ()
            , getapp().getops ().getvalidationtimenc (), mvalpublic, false);
        addload(v);
        v->settrusted ();
        v->sign (signinghash, mvalprivate);
            // fixme: wrong supression
        getapp().gethashrouter ().addsuppression (signinghash);
        getapp().getvalidations ().addvalidation (v, "localmissing");
        blob validation = v->getsigned ();
        protocol::tmvalidation val;
        val.set_validation (&validation[0], validation.size ());
    #if 0
        getapp ().overlay ().visit (relaymessage (
            std::make_shared <message> (
                val, protocol::mtvalidation)));
    #endif
        getapp().getops ().setlastvalidation (v);
        writelog (lswarning, ledgerconsensus) << "sending partial validation";
    }

    /** we have a new lcl and must accept it
    */
    void beginaccept (bool synchronous)
    {
        shamap::pointer consensusset
            = macquired[mourposition->getcurrenthash ()];

        if (!consensusset)
        {
            writelog (lsfatal, ledgerconsensus)
                << "we don't have a consensus set";
            abort ();
            return;
        }

        getapp().getops ().newlcl
            (mpeerpositions.size (), mcurrentmseconds, mnewledgerhash);

        if (synchronous)
            accept (consensusset);
        else
        {
            getapp().getjobqueue().addjob (jtaccept, "acceptledger",
                std::bind (&ledgerconsensusimp::accept, shared_from_this (), consensusset));
        }
    }

    void endconsensus ()
    {
        getapp().getops ().endconsensus (mhavecorrectlcl);
    }

    /** add our load fee to our validation
    */
    void addload(stvalidation::ref val)
    {
        std::uint32_t fee = std::max(
            getapp().getfeetrack().getlocalfee(),
            getapp().getfeetrack().getclusterfee());
        std::uint32_t ref = getapp().getfeetrack().getloadbase();
        if (fee > ref)
            val->setfieldu32(sfloadfee, fee);
    }
private:
    clock_type& m_clock;
    localtxs& m_localtx;
    feevote& m_feevote;
    dividendvote& m_dividendvote;

    // vfalco todo rename these to look pretty
    enum lcstate
    {
        lcspre_close,       // we haven't closed our ledger yet,
                            //  but others might have
        lcsestablish,       // establishing consensus
        lcsfinished,        // we have closed on a transaction set
        lcsaccepted,        // we have accepted/validated
                            //  a new last closed ledger
    };

    lcstate mstate;
    std::uint32_t mclosetime;      // the wall time this ledger closed
    uint256 mprevledgerhash, mnewledgerhash, macquiringledger;
    ledger::pointer mpreviousledger;
    ledgerproposal::pointer mourposition;
    rippleaddress mvalpublic, mvalprivate;
    bool mproposing, mvalidating, mhavecorrectlcl, mconsensusfail;

    int mcurrentmseconds, mclosepercent, mcloseresolution;
    bool mhaveclosetimeconsensus;

    boost::posix_time::ptime        mconsensusstarttime;
    int                             mpreviousproposers;
    int                             mpreviousmseconds;

    // convergence tracking, trusted peers indexed by hash of public key
    hash_map<nodeid, ledgerproposal::pointer>  mpeerpositions;

    // transaction sets, indexed by hash of transaction tree
    hash_map<uint256, shamap::pointer> macquired;
    hash_map<uint256, transactionacquire::pointer> macquiring;

    // peer sets
    hash_map<uint256, std::vector< std::weak_ptr<peer> > > mpeerdata;

    // disputed transactions
    hash_map<uint256, disputedtx::pointer> mdisputes;
    hash_set<uint256> mcompares;

    // close time estimates
    std::map<std::uint32_t, int> mclosetimes;

    // nodes that have bowed out of this consensus process
    nodeidset mdeadnodes;
};

//------------------------------------------------------------------------------

ledgerconsensus::~ledgerconsensus ()
{
}

std::shared_ptr <ledgerconsensus>
make_ledgerconsensus (ledgerconsensus::clock_type& clock, localtxs& localtx,
    ledgerhash const &prevlclhash, ledger::ref previousledger,
        std::uint32_t closetime, feevote& feevote, dividendvote& dividendvote)
{
    return std::make_shared <ledgerconsensusimp> (clock, localtx,
        prevlclhash, previousledger, closetime, feevote, dividendvote);
}

/** apply a transaction to a ledger

  @param engine       the transaction engine containing the ledger.
  @param txn          the transaction to be applied to ledger.
  @param openledger   true if ledger is open
  @param retryassured true if the transaction should be retried on failure.
  @return             one of resultsuccess, resultfail or resultretry.
*/
static
int applytransaction (transactionengine& engine
    , sttx::ref txn, bool openledger, bool retryassured)
{
    // returns false if the transaction has need not be retried.
    transactionengineparams parms = openledger ? tapopen_ledger : tapnone;

    if (retryassured)
    {
        parms = static_cast<transactionengineparams> (parms | tapretry);
    }

    if (getapp().gethashrouter ().setflag (txn->gettransactionid ()
        , sf_siggood))
    {
        parms = static_cast<transactionengineparams>
            (parms | tapno_check_sign);
    }
    writelog (lsdebug, ledgerconsensus) << "txn "
        << txn->gettransactionid ()
        << (openledger ? " open" : " closed")
        << (retryassured ? "/retry" : "/final");
    writelog (lstrace, ledgerconsensus) << txn->getjson (0);

    try
    {
        bool didapply;
        ter result = engine.applytransaction (*txn, parms, didapply);

        if (didapply)
        {
            writelog (lsdebug, ledgerconsensus)
            << "transaction success: " << transhuman (result);
            return ledgerconsensusimp::resultsuccess;
        }

        if (isteffailure (result) || istemmalformed (result) ||
            istellocal (result))
        {
            // failure
            writelog (lsdebug, ledgerconsensus)
                << "transaction failure: " << transhuman (result);
            return ledgerconsensusimp::resultfail;
        }

        writelog (lsdebug, ledgerconsensus)
            << "transaction retry: " << transhuman (result);
        return ledgerconsensusimp::resultretry;
    }
    catch (...)
    {
        writelog (lswarning, ledgerconsensus) << "throws";
        return ledgerconsensusimp::resultfail;
    }
}

/** apply a set of transactions to a ledger

  @param set                   the set of transactions to apply
  @param applyledger           the ledger to which the transactions should
                               be applied.
  @param checkledger           a reference ledger for determining error
                               messages (typically new last closed ledger).
  @param retriabletransactions collect failed transactions in this set
  @param openlgr               true if applyledger is open, else false.
*/
void applytransactions (shamap::ref set, ledger::ref applyledger,
    ledger::ref checkledger, canonicaltxset& retriabletransactions,
    bool openlgr)
{
    transactionengine engine (applyledger);

    if (set)
    {
        for (shamapitem::pointer item = set->peekfirstitem (); !!item;
            item = set->peeknextitem (item->gettag ()))
        {
            // if the checkledger doesn't have the transaction
            if (!checkledger->hastransaction (item->gettag ()))
            {
                // then try to apply the transaction to applyledger
                writelog (lsdebug, ledgerconsensus) <<
                    "processing candidate transaction: " << item->gettag ();
                try
                {
                    serializeriterator sit (item->peekserializer ());
                    sttx::pointer txn
                        = std::make_shared<sttx>(sit);
                    if (applytransaction (engine, txn,
                              openlgr, true) == ledgerconsensusimp::resultretry)
                    {
                        // on failure, stash the failed transaction for
                        // later retry.
                        retriabletransactions.push_back (txn);
                    }
                }
                catch (...)
                {
                    writelog (lswarning, ledgerconsensus) << "  throws";
                }
            }
        }
    }

    int changes;
    bool certainretry = true;
    // attempt to apply all of the retriable transactions
    for (int pass = 0; pass < ledger_total_passes; ++pass)
    {
        writelog (lsdebug, ledgerconsensus) << "pass: " << pass << " txns: "
            << retriabletransactions.size ()
            << (certainretry ? " retriable" : " final");
        changes = 0;

        auto it = retriabletransactions.begin ();

        while (it != retriabletransactions.end ())
        {
            try
            {
                switch (applytransaction (engine, it->second,
                        openlgr, certainretry))
                {
                case ledgerconsensusimp::resultsuccess:
                    it = retriabletransactions.erase (it);
                    ++changes;
                    break;

                case ledgerconsensusimp::resultfail:
                    it = retriabletransactions.erase (it);
                    break;

                case ledgerconsensusimp::resultretry:
                    ++it;
                }
            }
            catch (...)
            {
                writelog (lswarning, ledgerconsensus)
                    << "transaction throws";
                it = retriabletransactions.erase (it);
            }
        }

        writelog (lsdebug, ledgerconsensus) << "pass: "
            << pass << " finished " << changes << " changes";

        // a non-retry pass made no changes
        if (!changes && !certainretry)
            return;

        // stop retriable passes
        if ((!changes) || (pass >= ledger_retry_passes))
            certainretry = false;
    }

    // if there are any transactions left, we must have
    // tried them in at least one final pass
    assert (retriabletransactions.empty() || !certainretry);
}

} // ripple
