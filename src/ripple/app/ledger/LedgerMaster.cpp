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
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/ledger/inboundledgers.h>
#include <ripple/app/ledger/ledgercleaner.h>
#include <ripple/app/ledger/ledgerhistory.h>
#include <ripple/app/ledger/ledgerholder.h>
#include <ripple/app/ledger/orderbookdb.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/ihashrouter.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/app/misc/canonicaltxset.h>
#include <ripple/app/misc/shamapstore.h>
#include <ripple/app/misc/validations.h>
#include <ripple/app/paths/pathrequests.h>
#include <ripple/app/tx/transactionengine.h>
#include <ripple/basics/log.h>
#include <ripple/basics/rangeset.h>
#include <ripple/core/loadfeetrack.h>
#include <ripple/overlay/overlay.h>
#include <ripple/overlay/peer.h>
#include <ripple/validators/manager.h>
#include <algorithm>
#include <cassert>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

#define min_validation_ratio    150     // 150/256ths of validations of previous ledger
#define max_ledger_gap          100     // don't catch up more than 100 ledgers  (cannot exceed 256)

class ledgermasterimp
    : public ledgermaster
    , public beast::leakchecked <ledgermasterimp>
{
public:
    typedef std::function <void (ledger::ref)> callback;

    typedef ripplerecursivemutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;
    typedef beast::genericscopedunlock <locktype> scopedunlocktype;

    beast::journal m_journal;

    locktype m_mutex;

    ledgerholder mcurrentledger;        // the ledger we are currently processiong
    ledgerholder mclosedledger;         // the ledger that most recently closed
    ledgerholder mvalidledger;          // the highest-sequence ledger we have fully accepted
    ledger::pointer mpubledger;         // the last ledger we have published
    ledger::pointer mpathledger;        // the last ledger we did pathfinding against

    ledgerhistory mledgerhistory;

    canonicaltxset mheldtransactions;

    locktype mcompletelock;
    rangeset mcompleteledgers;

    std::unique_ptr <ledgercleaner> mledgercleaner;

    int                         mminvalidations;    // the minimum validations to publish a ledger
    uint256                     mlastvalidatehash;
    std::uint32_t               mlastvalidateseq;
    std::list<callback>         monvalidate;        // called when a ledger has enough validations

    bool                        madvancethread;     // publish thread is running
    bool                        madvancework;       // publish thread has work to do
    int                         mfillinprogress;

    int                         mpathfindthread;    // pathfinder jobs dispatched
    bool                        mpathfindnewrequest;

    std::atomic <std::uint32_t> mpubledgerclose;
    std::atomic <std::uint32_t> mpubledgerseq;
    std::atomic <std::uint32_t> mvalidledgerclose;
    std::atomic <std::uint32_t> mvalidledgerseq;
    std::atomic <std::uint32_t> mbuildingledgerseq;

    // the server is in standalone mode
    bool const standalone_;

    // how many ledgers before the current ledger do we allow peers to request?
    std::uint32_t const fetch_depth_;

    // how much history do we want to keep
    std::uint32_t const ledger_history_;
    // acquire past ledgers down to this ledger index
    std::uint32_t const ledger_history_index_;

    int const ledger_fetch_size_;

    //--------------------------------------------------------------------------

    ledgermasterimp (config const& config, stoppable& parent,
        beast::insight::collector::ptr const& collector, beast::journal journal)
        : ledgermaster (parent)
        , m_journal (journal)
        , mledgerhistory (collector)
        , mheldtransactions (uint256 ())
        , mledgercleaner (make_ledgercleaner (
            *this, deprecatedlogs().journal("ledgercleaner")))
        , mminvalidations (0)
        , mlastvalidateseq (0)
        , madvancethread (false)
        , madvancework (false)
        , mfillinprogress (0)
        , mpathfindthread (0)
        , mpathfindnewrequest (false)
        , mpubledgerclose (0)
        , mpubledgerseq (0)
        , mvalidledgerclose (0)
        , mvalidledgerseq (0)
        , mbuildingledgerseq (0)
        , standalone_ (config.run_standalone)
        , fetch_depth_ (getapp ().getshamapstore ().clampfetchdepth (config.fetch_depth))
        , ledger_history_ (config.ledger_history)
        , ledger_history_index_ (config.ledger_history_index)
        , ledger_fetch_size_ (config.getsize (siledgerfetch))
    {
        if (ledger_history_index_ != 0 &&
            config.nodedatabase["online_delete"].isnotempty () &&
            config.nodedatabase["online_delete"].getintvalue () > 0)
        {
            std::stringstream ss;
            ss << "[node_db] online_delete option and [ledger_history_index]"
                " cannot be configured at the same time.";
            throw std::runtime_error (ss.str ());
        }
    }

    ~ledgermasterimp ()
    {
    }

    ledgerindex getcurrentledgerindex ()
    {
        return mcurrentledger.get ()->getledgerseq ();
    }

    ledgerindex getvalidledgerindex ()
    {
        return mvalidledgerseq;
    }

    int getpublishedledgerage ()
    {
        std::uint32_t pubclose = mpubledgerclose.load();
        if (!pubclose)
        {
            writelog (lsdebug, ledgermaster) << "no published ledger";
            return 999999;
        }

        std::int64_t ret = getapp().getops ().getclosetimenc ();
        ret -= static_cast<std::int64_t> (pubclose);
        ret = (ret > 0) ? ret : 0;

        writelog (lstrace, ledgermaster) << "published ledger age is " << ret;
        return static_cast<int> (ret);
    }

    int getvalidatedledgerage ()
    {
        std::uint32_t valclose = mvalidledgerclose.load();
        if (!valclose)
        {
            writelog (lsdebug, ledgermaster) << "no validated ledger";
            return 999999;
        }

        std::int64_t ret = getapp().getops ().getclosetimenc ();
        ret -= static_cast<std::int64_t> (valclose);
        ret = (ret > 0) ? ret : 0;

        writelog (lstrace, ledgermaster) << "validated ledger age is " << ret;
        return static_cast<int> (ret);
    }

    bool iscaughtup(std::string& reason)
    {
        if (getpublishedledgerage() > 180)
        {
            reason = "no recently-published ledger";
            return false;
        }
        std::uint32_t validclose = mvalidledgerclose.load();
        std::uint32_t pubclose = mpubledgerclose.load();
        if (!validclose || !pubclose)
        {
            reason = "no published ledger";
            return false;
        }
        if (validclose  > (pubclose + 90))
        {
            reason = "published ledger lags validated ledger";
            return false;
        }
        return true;
    }

    void setvalidledger(ledger::ref l)
    {
        mvalidledger.set (l);
        mvalidledgerclose = l->getclosetimenc();
        mvalidledgerseq = l->getledgerseq();
        getapp().getops().updatelocaltx (l);
        getapp().getshamapstore().onledgerclosed (getvalidatedledger());

    #if ripple_hook_validators
        getapp().getvalidators().onledgerclosed (l->getledgerseq(),
            l->gethash(), l->getparenthash());
    #endif
    }

    void setpubledger(ledger::ref l)
    {
        mpubledger = l;
        mpubledgerclose = l->getclosetimenc();
        mpubledgerseq = l->getledgerseq();
    }

    void addheldtransaction (transaction::ref transaction)
    {
        // returns true if transaction was added
        scopedlocktype ml (m_mutex);
        mheldtransactions.push_back (transaction->getstransaction ());
    }

    void pushledger (ledger::pointer newledger)
    {
        // caller should already have properly assembled this ledger into "ready-to-close" form --
        // all candidate transactions must already be applied
        writelog (lsinfo, ledgermaster) << "pushledger: " << newledger->gethash ();

        {
            scopedlocktype ml (m_mutex);

            ledger::pointer closedledger = mcurrentledger.getmutable ();
            if (closedledger)
            {
                closedledger->setclosed ();
                closedledger->setimmutable ();
                mclosedledger.set (closedledger);
            }

            mcurrentledger.set (newledger);
        }

        if (standalone_)
        {
            setfullledger(newledger, true, false);
            tryadvance();
        }
        else
            checkaccept(newledger);
    }

    void pushledger (ledger::pointer newlcl, ledger::pointer newol)
    {
        assert (newlcl->isclosed () && newlcl->isaccepted ());
        assert (!newol->isclosed () && !newol->isaccepted ());


        {
            scopedlocktype ml (m_mutex);
            mclosedledger.set (newlcl);
            mcurrentledger.set (newol);
        }

        if (standalone_)
        {
            setfullledger(newlcl, true, false);
            tryadvance();
        }
        else
        {
            mledgerhistory.builtledger (newlcl);
        }
    }

    void switchledgers (ledger::pointer lastclosed, ledger::pointer current)
    {
        assert (lastclosed && current);

        {
            scopedlocktype ml (m_mutex);

            lastclosed->setclosed ();
            lastclosed->setaccepted ();

            mcurrentledger.set (current);
            mclosedledger.set (lastclosed);

            assert (!current->isclosed ());
        }
        checkaccept (lastclosed);
    }

    bool fixindex (ledgerindex ledgerindex, ledgerhash const& ledgerhash)
    {
        return mledgerhistory.fixindex (ledgerindex, ledgerhash);
    }

    bool storeledger (ledger::pointer ledger)
    {
        // returns true if we already had the ledger
        return mledgerhistory.addledger (ledger, false);
    }

    void forcevalid (ledger::pointer ledger)
    {
        ledger->setvalidated();
        setfullledger(ledger, true, false);
    }

    /** apply held transactions to the open ledger
        this is normally called as we close the ledger.
        the open ledger remains open to handle new transactions
        until a new open ledger is built.
    */
    void applyheldtransactions ()
    {
        scopedlocktype sl (m_mutex);

        // start with a mutable snapshot of the open ledger
        transactionengine engine (mcurrentledger.getmutable ());

        int recovers = 0;

        for (auto const& it : mheldtransactions)
        {
            try
            {
                transactionengineparams tepflags = tapopen_ledger;

                if (getapp().gethashrouter ().addsuppressionflags (it.first.gettxid (), sf_siggood))
                    tepflags = static_cast<transactionengineparams> (tepflags | tapno_check_sign);

                bool didapply;
                engine.applytransaction (*it.second, tepflags, didapply);

                if (didapply)
                    ++recovers;

                // if a transaction is recovered but hasn't been relayed,
                // it will become disputed in the consensus process, which
                // will cause it to be relayed.

            }
            catch (...)
            {
                writelog (lswarning, ledgermaster) << "held transaction throws";
            }
        }

        condlog (recovers != 0, lsinfo, ledgermaster) << "recovered " << recovers << " held transactions";

        // vfalco todo recreate the canonicaltxset object instead of resetting it
        mheldtransactions.reset (engine.getledger()->gethash ());
        mcurrentledger.set (engine.getledger ());
    }

    ledgerindex getbuildingledger ()
    {
        // the ledger we are currently building, 0 of none
        return mbuildingledgerseq.load ();
    }

    void setbuildingledger (ledgerindex i)
    {
        mbuildingledgerseq.store (i);
    }

    ter dotransaction (sttx::ref txn, transactionengineparams params, bool& didapply)
    {
        ledger::pointer ledger;
        transactionengine engine;
        ter result;
        didapply = false;

        {
            scopedlocktype sl (m_mutex);
            ledger = mcurrentledger.getmutable ();
            engine.setledger (ledger);
            result = engine.applytransaction (*txn, params, didapply);
        }
        if (didapply)
        {
           mcurrentledger.set (ledger);
           getapp().getops ().pubproposedtransaction (ledger, txn, result);
        }
        return result;
    }

    bool haveledgerrange (std::uint32_t from, std::uint32_t to)
    {
        scopedlocktype sl (mcompletelock);
        std::uint32_t prevmissing = mcompleteledgers.prevmissing (to + 1);
        return (prevmissing == rangeset::absent) || (prevmissing < from);
    }

    bool haveledger (std::uint32_t seq)
    {
        scopedlocktype sl (mcompletelock);
        return mcompleteledgers.hasvalue (seq);
    }

    void clearledger (std::uint32_t seq)
    {
        scopedlocktype sl (mcompletelock);
        return mcompleteledgers.clearvalue (seq);
    }

    // returns ledgers we have all the nodes for
    bool getfullvalidatedrange (std::uint32_t& minval, std::uint32_t& maxval)
    {
        maxval = mpubledgerseq.load();

        if (!maxval)
            return false;

        {
            scopedlocktype sl (mcompletelock);
            minval = mcompleteledgers.prevmissing (maxval);
        }

        if (minval == rangeset::absent)
            minval = maxval;
        else
            ++minval;

        return true;
    }

    // returns ledgers we have all the nodes for and are indexed
    bool getvalidatedrange (std::uint32_t& minval, std::uint32_t& maxval)
    {
        maxval = mpubledgerseq.load();

        if (!maxval)
            return false;

        {
            scopedlocktype sl (mcompletelock);
            minval = mcompleteledgers.prevmissing (maxval);
        }

        if (minval == rangeset::absent)
            minval = maxval;
        else
            ++minval;

        // remove from the validated range any ledger sequences that may not be
        // fully updated in the database yet

        std::set<std::uint32_t> spendingsaves = ledger::getpendingsaves();

        if (!spendingsaves.empty() && ((minval != 0) || (maxval != 0)))
        {
            // ensure we shrink the tips as much as possible
            // if we have 7-9 and 8,9 are invalid, we don't want to see the 8 and shrink to just 9
            // because then we'll have nothing when we could have 7.
            while (spendingsaves.count(maxval) > 0)
                --maxval;
            while (spendingsaves.count(minval) > 0)
                ++minval;

            // best effort for remaining exclusions
            for(auto v : spendingsaves)
            {
                if ((v >= minval) && (v <= maxval))
                {
                    if (v > ((minval + maxval) / 2))
                        maxval = v - 1;
                    else
                        minval = v + 1;
                }
            }

            if (minval > maxval)
                minval = maxval = 0;
        }

        return true;
    }

    // get the earliest ledger we will let peers fetch
    std::uint32_t getearliestfetch ()
    {
        // the earliest ledger we will let people fetch is ledger zero,
        // unless that creates a larger range than allowed
        std::uint32_t e = getclosedledger()->getledgerseq();

        if (e > fetch_depth_)
            e -= fetch_depth_;
        else
            e = 0;
        return e;
    }

    void tryfill (job& job, ledger::pointer ledger)
    {
        std::uint32_t seq = ledger->getledgerseq ();
        uint256 prevhash = ledger->getparenthash ();

        std::map< std::uint32_t, std::pair<uint256, uint256> > ledgerhashes;

        std::uint32_t minhas = ledger->getledgerseq ();
        std::uint32_t maxhas = ledger->getledgerseq ();

        while (! job.shouldcancel() && seq > 0)
        {
            {
                scopedlocktype ml (m_mutex);
                minhas = seq;
                --seq;

                if (haveledger (seq))
                    break;
            }

            auto it (ledgerhashes.find (seq));

            if (it == ledgerhashes.end ())
            {
                if (getapp().isshutdown ())
                    return;

                {
                    scopedlocktype ml (mcompletelock);
                    mcompleteledgers.setrange (minhas, maxhas);
                }
                maxhas = minhas;
                ledgerhashes = ledger::gethashesbyindex ((seq < 500)
                    ? 0
                    : (seq - 499), seq);
                it = ledgerhashes.find (seq);

                if (it == ledgerhashes.end ())
                    break;
            }

            if (it->second.first != prevhash)
                break;

            prevhash = it->second.second;
        }

        {
            scopedlocktype ml (mcompletelock);
            mcompleteledgers.setrange (minhas, maxhas);
        }
        {
            scopedlocktype ml (m_mutex);
            mfillinprogress = 0;
            tryadvance();
        }
    }

    /** request a fetch pack to get the ledger prior to 'nextledger'
    */
    void getfetchpack (ledger::ref nextledger)
    {
        peer::ptr target;
        int count = 0;

        overlay::peersequence peerlist = getapp().overlay ().getactivepeers ();
        for (auto const& peer : peerlist)
        {
            if (peer->hasrange (nextledger->getledgerseq() - 1, nextledger->getledgerseq()))
            {
                if (count++ == 0)
                    target = peer;
                else if ((rand () % ++count) == 0)
                    target = peer;
            }
        }

        if (target)
        {
            protocol::tmgetobjectbyhash tmbh;
            tmbh.set_query (true);
            tmbh.set_type (protocol::tmgetobjectbyhash::otfetch_pack);
            tmbh.set_ledgerhash (nextledger->gethash().begin (), 32);
            message::pointer packet = std::make_shared<message> (tmbh, protocol::mtget_objects);

            target->send (packet);
            writelog (lstrace, ledgermaster) << "requested fetch pack for " << nextledger->getledgerseq() - 1;
        }
        else
            writelog (lsdebug, ledgermaster) << "no peer for fetch pack";
    }

    void fixmismatch (ledger::ref ledger)
    {
        int invalidate = 0;
        uint256 hash;

        for (std::uint32_t lseq = ledger->getledgerseq () - 1; lseq > 0; --lseq)
            if (haveledger (lseq))
            {
                try
                {
                    hash = ledger->getledgerhash (lseq);
                }
                catch (...)
                {
                    writelog (lswarning, ledgermaster) <<
                        "fixmismatch encounters partial ledger";
                    clearledger(lseq);
                    return;
                }

                if (hash.isnonzero ())
                {
                    // try to close the seam
                    ledger::pointer otherledger = getledgerbyseq (lseq);

                    if (otherledger && (otherledger->gethash () == hash))
                    {
                        // we closed the seam
                        condlog (invalidate != 0, lswarning, ledgermaster) <<
                            "match at " << lseq << ", " << invalidate <<
                            " prior ledgers invalidated";
                        return;
                    }
                }

                clearledger (lseq);
                ++invalidate;
            }

        // all prior ledgers invalidated
        condlog (invalidate != 0, lswarning, ledgermaster) << "all " <<
            invalidate << " prior ledgers invalidated";
    }

    void setfullledger (ledger::pointer ledger, bool issynchronous, bool iscurrent)
    {
        // a new ledger has been accepted as part of the trusted chain
        writelog (lsdebug, ledgermaster) << "ledger " << ledger->getledgerseq () << " accepted :" << ledger->gethash ();
        assert (ledger->peekaccountstatemap ()->gethash ().isnonzero ());

        ledger->setvalidated();
        mledgerhistory.addledger(ledger, true);
        ledger->setfull();
        ledger->pendsavevalidated (issynchronous, iscurrent);

        {

            {
                scopedlocktype ml (mcompletelock);
                mcompleteledgers.setvalue (ledger->getledgerseq ());
            }

            scopedlocktype ml (m_mutex);

            if (ledger->getledgerseq() > mvalidledgerseq)
                setvalidledger(ledger);
            if (!mpubledger)
            {
                setpubledger(ledger);
                getapp().getorderbookdb().setup(ledger);
            }

            if ((ledger->getledgerseq () != 0) && haveledger (ledger->getledgerseq () - 1))
            {
                // we think we have the previous ledger, double check
                ledger::pointer prevledger = getledgerbyseq (ledger->getledgerseq () - 1);

                if (!prevledger || (prevledger->gethash () != ledger->getparenthash ()))
                {
                    writelog (lswarning, ledgermaster) << "acquired ledger invalidates previous ledger: " <<
                                                       (prevledger ? "hashmismatch" : "missingledger");
                    fixmismatch (ledger);
                }
            }
        }
    }

    void failedsave(std::uint32_t seq, uint256 const& hash)
    {
        clearledger(seq);
        getapp().getinboundledgers().findcreate(hash, seq, inboundledger::fcgeneric);
    }

    // check if the specified ledger can become the new last fully-validated ledger
    void checkaccept (uint256 const& hash, std::uint32_t seq)
    {

        if (seq != 0)
        {
            // ledger is too old
            if (seq <= mvalidledgerseq)
                return;

            // ledger could match the ledger we're already building
            if (seq == mbuildingledgerseq)
                return;
        }

        ledger::pointer ledger = mledgerhistory.getledgerbyhash (hash);

        if (!ledger)
        {
            // fixme: we should really only fetch if the ledger
            //has sufficient validations to accept it

            inboundledger::pointer l =
                getapp().getinboundledgers().findcreate(hash, 0, inboundledger::fcgeneric);
            if (l && l->iscomplete() && !l->isfailed())
                ledger = l->getledger();
            else
            {
                writelog (lsdebug, ledgermaster) <<
                    "checkaccept triggers acquire " << to_string (hash);
            }
        }

        if (ledger)
            checkaccept (ledger);
    }

    /**
     * determines how many validations are needed to fully-validated a ledger
     *
     * @return number of validations needed
     */
    int getneededvalidations ()
    {
        if (standalone_)
            return 0;

        int minval = mminvalidations;

        if (mlastvalidatehash.isnonzero ())
        {
            int val = getapp().getvalidations ().gettrustedvalidationcount (mlastvalidatehash);
            val *= min_validation_ratio;
            val /= 256;

            if (val > minval)
                minval = val;
        }

        return minval;
    }

    void checkaccept (ledger::ref ledger)
    {
        if (ledger->getledgerseq() <= mvalidledgerseq)
            return;

        // can we advance the last fully-validated ledger? if so, can we publish?
        scopedlocktype ml (m_mutex);

        if (ledger->getledgerseq() <= mvalidledgerseq)
            return;

        int minval = getneededvalidations();
        int tvc = getapp().getvalidations().gettrustedvalidationcount(ledger->gethash());
        if (tvc < minval) // nothing we can do
        {
            writelog (lstrace, ledgermaster) << "only " << tvc << " validations for " << ledger->gethash();
            return;
        }

        writelog (lsinfo, ledgermaster) << "advancing accepted ledger to " << ledger->getledgerseq() << " with >= " << minval << " validations";

        mlastvalidatehash = ledger->gethash();
        mlastvalidateseq = ledger->getledgerseq();

        ledger->setvalidated();
        ledger->setfull();
        setvalidledger(ledger);
        if (!mpubledger)
        {
            ledger->pendsavevalidated(true, true);
            setpubledger(ledger);
            getapp().getorderbookdb().setup(ledger);
        }

        std::uint64_t const base = getapp().getfeetrack().getloadbase();
        auto fees = getapp().getvalidations().fees (ledger->gethash(), base);
        {
            auto fees2 = getapp().getvalidations().fees (ledger->getparenthash(), base);
            fees.reserve (fees.size() + fees2.size());
            std::copy (fees2.begin(), fees2.end(), std::back_inserter(fees));
        }
        std::uint64_t fee;
        if (! fees.empty())
        {
            std::sort (fees.begin(), fees.end());
            fee = fees[fees.size() / 2]; // median
        }
        else
        {
            fee = base;
        }

        getapp().getfeetrack().setremotefee(fee);

        tryadvance ();
    }

    /** report that the consensus process built a particular ledger */
    void consensusbuilt (ledger::ref ledger) override
    {

        // because we just built a ledger, we are no longer building one
        setbuildingledger (0);

        // no need to process validations in standalone mode
        if (standalone_)
            return;

        if (ledger->getledgerseq() <= mvalidledgerseq)
        {
            writelog (lsinfo,  ledgerconsensus)
               << "consensus built old ledger: "
               << ledger->getledgerseq() << " <= " << mvalidledgerseq;
            return;
        }

        // see if this ledger can be the new fully-validated ledger
        checkaccept (ledger);

        if (ledger->getledgerseq() <= mvalidledgerseq)
        {
            writelog (lsdebug, ledgerconsensus)
                << "consensus ledger fully validated";
            return;
        }

        // this ledger cannot be the new fully-validated ledger, but
        // maybe we saved up validations for some other ledger that can be

        auto const val = getapp().getvalidations().getcurrenttrustedvalidations();

        // track validation counts with sequence numbers
        class valseq
        {
            public:

            valseq () : valcount_ (0), ledgerseq_ (0) { ; }

            void mergevalidation (ledgerseq seq)
            {
                valcount_++;

                // if we didn't already know the sequence, now we do
                if (ledgerseq_ == 0)
                    ledgerseq_ = seq;
            }

            int valcount_;
            ledgerseq ledgerseq_;
        };

        // count the number of current, trusted validations
        hash_map <uint256, valseq> count;
        for (auto const& v : val)
        {
            valseq& vs = count[v->getledgerhash()];
            vs.mergevalidation (v->getfieldu32 (sfledgersequence));
        }

        int neededvalidations = getneededvalidations ();
        ledgerseq maxseq = mvalidledgerseq;
        uint256 maxledger = ledger->gethash();

        // of the ledgers with sufficient validations,
        // find the one with the highest sequence
        for (auto& v : count)
            if (v.second.valcount_ > neededvalidations)
            {
                // if we still don't know the sequence, get it
                if (v.second.ledgerseq_ == 0)
                {
                    ledger::pointer ledger = getledgerbyhash (v.first);
                    if (ledger)
                        v.second.ledgerseq_ = ledger->getledgerseq();
                }

                if (v.second.ledgerseq_ > maxseq)
                {
                    maxseq = v.second.ledgerseq_;
                    maxledger = v.first;
                }
            }

        if (maxseq > mvalidledgerseq)
        {
            writelog (lsdebug, ledgerconsensus)
                << "consensus triggered check of ledger";
            checkaccept (maxledger, maxseq);
        }
    }

    void advancethread()
    {
        scopedlocktype sl (m_mutex);
        assert (!mvalidledger.empty () && madvancethread);

        writelog (lstrace, ledgermaster) << "advancethread<";

        try
        {
            doadvance();
        }
        catch (...)
        {
            writelog (lsfatal, ledgermaster) << "doadvance throws an exception";
        }

        madvancethread = false;
        writelog (lstrace, ledgermaster) << "advancethread>";
    }

    // try to publish ledgers, acquire missing ledgers
    void doadvance ()
    {
        do
        {
            madvancework = false; // if there's work to do, we'll make progress
            bool progress = false;

            std::list<ledger::pointer> publedgers = findnewledgerstopublish ();
            if (publedgers.empty())
            {
                if (!standalone_ && !getapp().getfeetrack().isloadedlocal() &&
                    (getapp().getjobqueue().getjobcount(jtpuboldledger) < 10) &&
                    (mvalidledgerseq == mpubledgerseq))
                { // we are in sync, so can acquire
                    std::uint32_t missing;
                    {
                        scopedlocktype sl (mcompletelock);
                        missing = mcompleteledgers.prevmissing(mpubledger->getledgerseq());
                    }
                    writelog (lstrace, ledgermaster) << "tryadvance discovered missing " << missing;
                    if ((missing != rangeset::absent) && (missing > 0) &&
                        shouldacquire (mvalidledgerseq, ledger_history_,
                            ledger_history_index_, missing) &&
                        ((mfillinprogress == 0) || (missing > mfillinprogress)))
                    {
                        writelog (lstrace, ledgermaster) << "advancethread should acquire";
                        {
                            scopedunlocktype sl(m_mutex);
                            ledger::pointer nextledger = mledgerhistory.getledgerbyseq(missing + 1);
                            if (nextledger)
                            {
                                assert (nextledger->getledgerseq() == (missing + 1));
                                ledger::pointer ledger = getledgerbyhash(nextledger->getparenthash());
                                if (!ledger)
                                {
                                    if (!getapp().getinboundledgers().isfailure(nextledger->getparenthash()))
                                    {
                                        inboundledger::pointer acq =
                                            getapp().getinboundledgers().findcreate(nextledger->getparenthash(),
                                                                                    nextledger->getledgerseq() - 1,
                                                                                    inboundledger::fchistory);
                                        if (!acq)
                                        {
                                            // on system shutdown, findcreate may return a nullptr
                                            writelog (lstrace, ledgermaster)
                                                    << "findcreate failed to return an inbound ledger";
                                            return;
                                        }

                                        if (acq->iscomplete() && !acq->isfailed())
                                            ledger = acq->getledger();
                                        else if ((missing > 40000) && getapp().getops().shouldfetchpack(missing))
                                        {
                                            writelog (lstrace, ledgermaster) << "tryadvance want fetch pack " << missing;
                                            getfetchpack(nextledger);
                                        }
                                        else
                                            writelog (lstrace, ledgermaster) << "tryadvance no fetch pack for " << missing;
                                    }
                                    else
                                        writelog (lsdebug, ledgermaster) << "tryadvance found failed acquire";
                                }
                                if (ledger)
                                {
                                    assert(ledger->getledgerseq() == missing);
                                    writelog (lstrace, ledgermaster) << "tryadvance acquired " << ledger->getledgerseq();
                                    setfullledger(ledger, false, false);
                                    if ((mfillinprogress == 0) && (ledger::gethashbyindex(ledger->getledgerseq() - 1) == ledger->getparenthash()))
                                    { // previous ledger is in db
                                        scopedlocktype sl(m_mutex);
                                        mfillinprogress = ledger->getledgerseq();
                                        getapp().getjobqueue().addjob(jtadvance, "tryfill", std::bind (
                                            &ledgermasterimp::tryfill, this,
                                            std::placeholders::_1, ledger));
                                    }
                                    progress = true;
                                }
                                else
                                {
                                    try
                                    {
                                        for (int i = 0; i < ledger_fetch_size_; ++i)
                                        {
                                            std::uint32_t seq = missing - i;
                                            uint256 hash = nextledger->getledgerhash(seq);
                                            if (hash.isnonzero())
                                                getapp().getinboundledgers().findcreate(hash,
                                                     seq, inboundledger::fchistory);
                                        }
                                    }
                                    catch (...)
                                    {
                                        writelog (lswarning, ledgermaster) << "threw while prefecthing";
                                    }
                                }
                            }
                            else
                            {
                                writelog (lsfatal, ledgermaster) << "unable to find ledger following prevmissing " << missing;
                                writelog (lsfatal, ledgermaster) << "pub:" << mpubledgerseq << " val:" << mvalidledgerseq;
                                writelog (lsfatal, ledgermaster) << "ledgers: " << getapp().getledgermaster().getcompleteledgers();
                                clearledger (missing + 1);
                                progress = true;
                            }
                        }
                        if (mvalidledgerseq != mpubledgerseq)
                        {
                            writelog (lsdebug, ledgermaster) << "tryadvance found last valid changed";
                            progress = true;
                        }
                    }
                }
                else
                    writelog (lstrace, ledgermaster) << "tryadvance not fetching history";
            }
            else
            {
                writelog (lstrace, ledgermaster) <<
                    "tryadvance found " << publedgers.size() <<
                    " ledgers to publish";
                for(auto ledger : publedgers)
                {
                    {
                        scopedunlocktype sul (m_mutex);
                        writelog(lsdebug, ledgermaster) <<
                            "tryadvance publishing seq " << ledger->getledgerseq();

                        setfullledger(ledger, true, true);
                        getapp().getops().publedger(ledger);
                    }

                    setpubledger(ledger);
                    progress = true;
                }

                getapp().getops().clearneednetworkledger();
                newpfwork ("pf:newledger");
            }
            if (progress)
                madvancework = true;
        } while (madvancework);
    }

    std::list<ledger::pointer> findnewledgerstopublish ()
    {
        std::list<ledger::pointer> ret;

        writelog (lstrace, ledgermaster) << "findnewledgerstopublish<";
        if (!mpubledger)
        {
            writelog (lsinfo, ledgermaster) << "first published ledger will be " << mvalidledgerseq;
            ret.push_back (mvalidledger.get ());
        }
        else if (mvalidledgerseq > (mpubledgerseq + max_ledger_gap))
        {
            writelog (lswarning, ledgermaster) << "gap in validated ledger stream " << mpubledgerseq << " - " <<
                                               mvalidledgerseq - 1;
            ledger::pointer valledger = mvalidledger.get ();
            ret.push_back (valledger);
            setpubledger (valledger);
            getapp().getorderbookdb().setup(valledger);
        }
        else if (mvalidledgerseq > mpubledgerseq)
        {
            int acqcount = 0;

            std::uint32_t pubseq = mpubledgerseq + 1; // next sequence to publish
            ledger::pointer valledger = mvalidledger.get ();
            std::uint32_t valseq = valledger->getledgerseq ();

            scopedunlocktype sul(m_mutex);
            try
            {
                for (std::uint32_t seq = pubseq; seq <= valseq; ++seq)
                {
                    writelog (lstrace, ledgermaster) << "trying to fetch/publish valid ledger " << seq;

                    ledger::pointer ledger;
                    uint256 hash = valledger->getledgerhash (seq); // this can throw

                    if (seq == valseq)
                    { // we need to publish the ledger we just fully validated
                        ledger = valledger;
                    }
                    else
                    {
                        if (hash.iszero ())
                        {
                            writelog (lsfatal, ledgermaster) << "ledger: " << valseq << " does not have hash for " << seq;
                            assert (false);
                        }

                        ledger = mledgerhistory.getledgerbyhash (hash);
                    }

                    if (!ledger && (++acqcount < 4))
                    { // we can try to acquire the ledger we need
                        inboundledger::pointer acq =
                            getapp().getinboundledgers ().findcreate (hash, seq, inboundledger::fcgeneric);

                        if (!acq)
                        {
                            // on system shutdown, findcreate may return a nullptr
                            writelog (lstrace, ledgermaster)
                                << "findcreate failed to return an inbound ledger";
                            return {};
                        }

                        if (!acq->isdone()) {
                        }
                        else if (acq->iscomplete () && !acq->isfailed ())
                        {
                            ledger = acq->getledger();
                        }
                        else
                        {
                            writelog (lswarning, ledgermaster) << "failed to acquire a published ledger";
                            getapp().getinboundledgers().dropledger(hash);
                            acq = getapp().getinboundledgers().findcreate(hash, seq, inboundledger::fcgeneric);

                            if (!acq)
                            {
                                // on system shutdown, findcreate may return a nullptr
                                writelog (lstrace, ledgermaster)
                                    << "findcreate failed to return an inbound ledger";
                                return {};
                            }

                            if (acq->iscomplete())
                            {
                                if (acq->isfailed())
                                    getapp().getinboundledgers().dropledger(hash);
                                else
                                    ledger = acq->getledger();
                            }
                        }
                    }

                    if (ledger && (ledger->getledgerseq() == pubseq))
                    { // we acquired the next ledger we need to publish
                        ledger->setvalidated();
                        ret.push_back (ledger);
                        ++pubseq;
                    }

                }
            }
            catch (...)
            {
                writelog (lserror, ledgermaster) << "findnewledgerstopublish catches an exception";
            }
        }

        writelog (lstrace, ledgermaster) << "findnewledgerstopublish> " << ret.size();
        return ret;
    }

    void tryadvance()
    {
        scopedlocktype ml (m_mutex);

        // can't advance without at least one fully-valid ledger
        madvancework = true;
        if (!madvancethread && !mvalidledger.empty ())
        {
            madvancethread = true;
            getapp().getjobqueue ().addjob (jtadvance, "advanceledger",
                                            std::bind (&ledgermasterimp::advancethread, this));
        }
    }

    // return the hash of the valid ledger with a particular sequence, given a subsequent ledger known valid
    uint256 getledgerhash(std::uint32_t desiredseq, ledger::ref knowngoodledger)
    {
        assert(desiredseq < knowngoodledger->getledgerseq());

        uint256 hash = knowngoodledger->getledgerhash(desiredseq);

        // not directly in the given ledger
        if (hash.iszero ())
        {
            std::uint32_t seq = (desiredseq + 255) % 256;
            assert(seq < desiredseq);

            uint256 i = knowngoodledger->getledgerhash(seq);
            if (i.isnonzero())
            {
                ledger::pointer l = getledgerbyhash(i);
                if (l)
                {
                    hash = l->getledgerhash(desiredseq);
                    assert (hash.isnonzero());
                }
            }
            else
                assert(false);
        }

        return hash;
    }

    void updatepaths (job& job)
    {
        {
            scopedlocktype ml (m_mutex);
            if (getapp().getops().isneednetworkledger () || mcurrentledger.empty ())
            {
                --mpathfindthread;
                return;
            }
        }


        while (! job.shouldcancel())
        {
            ledger::pointer lastledger;
            {
                scopedlocktype ml (m_mutex);

                if (!mvalidledger.empty() &&
                    (!mpathledger || (mpathledger->getledgerseq() != mvalidledgerseq)))
                { // we have a new valid ledger since the last full pathfinding
                    mpathledger = mvalidledger.get ();
                    lastledger = mpathledger;
                }
                else if (mpathfindnewrequest)
                { // we have a new request but no new ledger
                    lastledger = mcurrentledger.get ();
                }
                else
                { // nothing to do
                    --mpathfindthread;
                    return;
                }
            }

            if (!standalone_)
            { // don't pathfind with a ledger that's more than 60 seconds old
                std::int64_t age = getapp().getops().getclosetimenc();
                age -= static_cast<std::int64_t> (lastledger->getclosetimenc());
                if (age > 60)
                {
                    writelog (lsdebug, ledgermaster) << "published ledger too old for updating paths";
                    --mpathfindthread;
                    return;
                }
            }

            try
            {
                getapp().getpathrequests().updateall (lastledger, job.getcancelcallback ());
            }
            catch (shamapmissingnode&)
            {
                writelog (lsinfo, ledgermaster) << "missing node detected during pathfinding";
                getapp().getinboundledgers().findcreate(lastledger->gethash (), lastledger->getledgerseq (),
                    inboundledger::fcgeneric);
            }
        }
    }

    void newpathrequest ()
    {
        scopedlocktype ml (m_mutex);
        mpathfindnewrequest = true;

        newpfwork("pf:newrequest");
    }

    bool isnewpathrequest ()
    {
        scopedlocktype ml (m_mutex);
        if (!mpathfindnewrequest)
            return false;
        mpathfindnewrequest = false;
        return true;
    }

    // if the order book is radically updated, we need to reprocess all pathfinding requests
    void neworderbookdb ()
    {
        scopedlocktype ml (m_mutex);
        mpathledger.reset();

        newpfwork("pf:newobdb");
    }

    /** a thread needs to be dispatched to handle pathfinding work of some kind
    */
    void newpfwork (const char *name)
    {
        if (mpathfindthread < 2)
        {
            ++mpathfindthread;
            getapp().getjobqueue().addjob (jtupdate_pf, name,
                std::bind (&ledgermasterimp::updatepaths, this,
                           std::placeholders::_1));
        }
    }

    locktype& peekmutex ()
    {
        return m_mutex;
    }

    // the current ledger is the ledger we believe new transactions should go in
    ledger::pointer getcurrentledger ()
    {
        return mcurrentledger.get ();
    }

    // the finalized ledger is the last closed/accepted ledger
    ledger::pointer getclosedledger ()
    {
        return mclosedledger.get ();
    }

    // the validated ledger is the last fully validated ledger
    ledger::pointer getvalidatedledger ()
    {
        return mvalidledger.get ();
    }

    // this is the last ledger we published to clients and can lag the validated ledger
    ledger::ref getpublishedledger ()
    {
        return mpubledger;
    }

    int getminvalidations ()
    {
        return mminvalidations;
    }

    void setminvalidations (int v)
    {
        writelog (lsinfo, ledgermaster) << "validation quorum: " << v;
        mminvalidations = v;
    }

    std::string getcompleteledgers ()
    {
        scopedlocktype sl (mcompletelock);
        return mcompleteledgers.tostring ();
    }

    /** find or acquire the ledger with the specified index and the specified hash
        return a pointer to that ledger if it is immediately available
    */
    ledger::pointer findacquireledger (std::uint32_t index, uint256 const& hash)
    {
        ledger::pointer ledger (getledgerbyhash (hash));
        if (!ledger)
        {
            inboundledger::pointer inboundledger =
                getapp().getinboundledgers().findcreate (hash, index, inboundledger::fcgeneric);
            if (inboundledger && inboundledger->iscomplete() && !inboundledger->isfailed())
                ledger = inboundledger->getledger();
        }
        return ledger;
    }

    uint256 gethashbyseq (std::uint32_t index)
    {
        uint256 hash = mledgerhistory.getledgerhash (index);

        if (hash.isnonzero ())
            return hash;

        return ledger::gethashbyindex (index);
    }

    uint256 walkhashbyseq (std::uint32_t index)
    {
        uint256 ledgerhash;
        ledger::pointer referenceledger;

        referenceledger = mvalidledger.get ();
        if (referenceledger)
            ledgerhash = walkhashbyseq (index, referenceledger);
        return ledgerhash;
    }

    /** walk the chain of ledger hashed to determine the hash of the
        ledger with the specified index. the referenceledger is used as
        the base of the chain and should be fully validated and must not
        precede the target index. this function may throw if nodes
        from the reference ledger or any prior ledger are not present
        in the node store.
    */
    uint256 walkhashbyseq (std::uint32_t index, ledger::ref referenceledger)
    {
        uint256 ledgerhash;
        if (!referenceledger || (referenceledger->getledgerseq() < index))
            return ledgerhash; // nothing we can do. no validated ledger.

        // see if the hash for the ledger we need is in the reference ledger
        ledgerhash = referenceledger->getledgerhash (index);
        if (ledgerhash.iszero())
        {
            // no, try to get another ledger that might have the hash we need
            // compute the index and hash of a ledger that will have the hash we need
            ledgerindex refindex = (index + 255) & (~255);
            ledgerhash refhash = referenceledger->getledgerhash (refindex);

            bool const nonzero (refhash.isnonzero ());
            assert (nonzero);
            if (nonzero)
            {
                // we found the hash and sequence of a better reference ledger
                ledger::pointer ledger = findacquireledger (refindex, refhash);
                if (ledger)
                {
                    ledgerhash = ledger->getledgerhash (index);
                    assert (ledgerhash.isnonzero());
                }
            }
        }
        return ledgerhash;
    }

    ledger::pointer getledgerbyseq (std::uint32_t index)
    {
        ledger::pointer ret = mledgerhistory.getledgerbyseq (index);
        if (ret)
            return ret;

        ret = mcurrentledger.get ();
        if (ret && (ret->getledgerseq () == index))
            return ret;

        ret = mclosedledger.get ();
        if (ret && (ret->getledgerseq () == index))
            return ret;

        clearledger (index);
        return ledger::pointer();
    }

    ledger::pointer getledgerbyhash (uint256 const& hash)
    {
        if (hash.iszero ())
            return mcurrentledger.get ();

        ledger::pointer ret = mledgerhistory.getledgerbyhash (hash);
        if (ret)
            return ret;

        ret = mcurrentledger.get ();
        if (ret && (ret->gethash () == hash))
            return ret;

        ret = mclosedledger.get ();
        if (ret && (ret->gethash () == hash))
            return ret;

        return ledger::pointer ();
    }

    void doledgercleaner(json::value const& parameters)
    {
        mledgercleaner->doclean (parameters);
    }

    void setledgerrangepresent (std::uint32_t minv, std::uint32_t maxv)
    {
        scopedlocktype sl (mcompletelock);
        mcompleteledgers.setrange (minv, maxv);
    }
    void tune (int size, int age)
    {
        mledgerhistory.tune (size, age);
    }

    void sweep ()
    {
        mledgerhistory.sweep ();
    }

    float getcachehitrate ()
    {
        return mledgerhistory.getcachehitrate ();
    }

    void addvalidatecallback (callback& c)
    {
        monvalidate.push_back (c);
    }

    beast::propertystream::source& getpropertysource ()
    {
        return *mledgercleaner;
    }

    void clearpriorledgers (ledgerindex seq) override
    {
        scopedlocktype sl (mcompletelock);
        for (ledgerindex i = mcompleteledgers.getfirst(); i < seq; ++i)
        {
            if (haveledger (i))
                clearledger (i);
        }
    }

    void clearledgercacheprior (ledgerindex seq) override
    {
        mledgerhistory.clearledgercacheprior (seq);
    }
};

//------------------------------------------------------------------------------

ledgermaster::ledgermaster (stoppable& parent)
    : stoppable ("ledgermaster", parent)
{
}

ledgermaster::~ledgermaster ()
{
}

bool ledgermaster::shouldacquire (std::uint32_t currentledger,
    std::uint32_t ledgerhistory, std::uint32_t ledgerhistoryindex,
    std::uint32_t candidateledger)
{
    bool ret (candidateledger >= currentledger ||
        (ledgerhistoryindex != 0 && candidateledger >= ledgerhistoryindex) ||
        (currentledger - candidateledger) <= ledgerhistory);

    writelog (lstrace, ledgermaster)
        << "missing ledger "
        << candidateledger
        << (ret ? " should" : " should not")
        << " be acquired";
    return ret;
}

std::unique_ptr <ledgermaster>
make_ledgermaster (config const& config, beast::stoppable& parent,
    beast::insight::collector::ptr const& collector, beast::journal journal)
{
    return std::make_unique <ledgermasterimp> (config, parent, collector, journal);
}

} // ripple
