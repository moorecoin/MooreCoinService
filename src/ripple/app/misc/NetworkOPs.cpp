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
#include <ripple/app/consensus/ledgerconsensus.h>
#include <ripple/app/data/databasecon.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/feevote.h>
#include <ripple/app/ledger/acceptedledger.h>
#include <ripple/app/ledger/inboundledger.h>
#include <ripple/app/ledger/inboundledgers.h>
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/ledger/ledgertiming.h>
#include <ripple/app/ledger/ledgertojson.h>
#include <ripple/app/ledger/orderbookdb.h>
#include <ripple/app/main/loadmanager.h>
#include <ripple/app/main/localcredentials.h>
#include <ripple/app/misc/ihashrouter.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/app/misc/validations.h>
#include <ripple/app/peers/clusternodestatus.h>
#include <ripple/app/peers/uniquenodelist.h>
#include <ripple/app/tx/transactionmaster.h>
#include <ripple/basics/log.h>
#include <ripple/basics/time.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/basics/uptimetimer.h>
#include <ripple/protocol/jsonfields.h>
#include <ripple/core/config.h>
#include <ripple/core/loadfeetrack.h>
#include <ripple/crypto/randomnumbers.h>
#include <ripple/crypto/rfc1751.h>
#include <ripple/json/to_string.h>
#include <ripple/overlay/overlay.h>
#include <ripple/overlay/predicates.h>
#include <ripple/protocol/buildinfo.h>
#include <ripple/protocol/hashprefix.h>
#include <ripple/protocol/indexes.h>
#include <ripple/resource/fees.h>
#include <ripple/resource/gossip.h>
#include <ripple/resource/manager.h>
#include <beast/module/core/thread/deadlinetimer.h>
#include <beast/module/core/system/systemstats.h>
#include <beast/cxx14/memory.h> // <memory>
#include <boost/foreach.hpp>
#include <tuple>

namespace ripple {

class networkopsimp
    : public networkops
    , public beast::deadlinetimer::listener
    , public beast::leakchecked <networkopsimp>
{
public:
    enum fault
    {
        // exceptions these functions can throw
        io_error    = 1,
        no_network  = 2,
    };

public:
    // vfalco todo make ledgermaster a sharedptr or a reference.
    //
    networkopsimp (
            clock_type& clock, bool standalone, std::size_t network_quorum,
            jobqueue& job_queue, ledgermaster& ledgermaster, stoppable& parent,
            beast::journal journal)
        : networkops (parent)
        , m_clock (clock)
        , m_journal (journal)
        , m_localtx (localtxs::new ())
        , m_feevote (make_feevote (setup_feevote (getconfig().section ("voting")),
            deprecatedlogs().journal("feevote")))
        , mmode (omdisconnected)
        , mneednetworkledger (false)
        , mproposing (false)
        , mvalidating (false)
        , m_amendmentblocked (false)
        , m_heartbeattimer (this)
        , m_clustertimer (this)
        , m_ledgermaster (ledgermaster)
        , mclosetimeoffset (0)
        , mlastcloseproposers (0)
        , mlastcloseconvergetime (1000 * ledger_idle_interval)
        , mlastclosetime (0)
        , mlastvalidationtime (0)
        , mfetchpack ("fetchpack", 65536, 45, clock,
            deprecatedlogs().journal("taggedcache"))
        , mfetchseq (0)
        , mlastloadbase (256)
        , mlastloadfactor (256)
        , m_job_queue (job_queue)
        , m_standalone (standalone)
        , m_network_quorum (network_quorum)
    {
        m_dividendvote = make_dividendvote(deprecatedlogs().journal("dividendvote"));
        m_dividendmaster = make_dividendmaster(deprecatedlogs().journal("dividendmaster"));
    }

    ~networkopsimp()
    {
    }

    // network information
    // our best estimate of wall time in seconds from 1/1/2000
    std::uint32_t getnetworktimenc () const;

    // our best estimate of current ledger close time
    std::uint32_t getclosetimenc () const;
    std::uint32_t getclosetimenc (int& offset) const;

    // use *only* to timestamp our own validation
    std::uint32_t getvalidationtimenc ();
    void closetimeoffset (int);

    /** on return the offset param holds the system time offset in seconds.
    */
    boost::posix_time::ptime getnetworktimept(int& offset) const;
    std::uint32_t getledgerid (uint256 const& hash);
    std::uint32_t getcurrentledgerid ();
    operatingmode getoperatingmode () const
    {
        return mmode;
    }
    std::string stroperatingmode () const;

    ledger::pointer     getclosedledger ()
    {
        return m_ledgermaster.getclosedledger ();
    }
    ledger::pointer     getvalidatedledger ()
    {
        return m_ledgermaster.getvalidatedledger ();
    }
    ledger::pointer     getpublishedledger ()
    {
        return m_ledgermaster.getpublishedledger ();
    }
    ledger::pointer     getcurrentledger ()
    {
        return m_ledgermaster.getcurrentledger ();
    }
    ledger::pointer getledgerbyhash (uint256 const& hash)
    {
        return m_ledgermaster.getledgerbyhash (hash);
    }
    ledger::pointer getledgerbyseq (const std::uint32_t seq);
    void            missingnodeinledger (const std::uint32_t seq);

    uint256         getclosedledgerhash ()
    {
        return m_ledgermaster.getclosedledger ()->gethash ();
    }

    // do we have this inclusive range of ledgers in our database
    bool haveledgerrange (std::uint32_t from, std::uint32_t to);
    bool haveledger (std::uint32_t seq);
    std::uint32_t getvalidatedseq ();
    bool isvalidated (std::uint32_t seq);
    bool isvalidated (std::uint32_t seq, uint256 const& hash);
    bool isvalidated (ledger::ref l)
    {
        return isvalidated (l->getledgerseq (), l->gethash ());
    }
    bool getvalidatedrange (std::uint32_t& minval, std::uint32_t& maxval)
    {
        return m_ledgermaster.getvalidatedrange (minval, maxval);
    }
    bool getfullvalidatedrange (std::uint32_t& minval, std::uint32_t& maxval)
    {
        return m_ledgermaster.getfullvalidatedrange (minval, maxval);
    }

    stvalidation::ref getlastvalidation ()
    {
        return mlastvalidation;
    }
    void setlastvalidation (stvalidation::ref v)
    {
        mlastvalidation = v;
    }

    sle::pointer getsle (ledger::pointer lpledger, uint256 const& uhash)
    {
        return lpledger->getsle (uhash);
    }
    sle::pointer getslei (ledger::pointer lpledger, uint256 const& uhash)
    {
        return lpledger->getslei (uhash);
    }

    //
    // transaction operations
    //

    // must complete immediately
    typedef std::function<void (transaction::pointer, ter)> stcallback;
    void submittransaction (
        job&, sttx::pointer,
        stcallback callback = stcallback ());

    transaction::pointer submittransactionsync (
        transaction::ref tptrans,
        bool badmin, bool blocal, bool bfailhard, bool bsubmit);

    transaction::pointer processtransactioncb (
        transaction::pointer,
        bool badmin, bool blocal, bool bfailhard, stcallback);
    transaction::pointer processtransaction (
        transaction::pointer transaction,
        bool badmin, bool blocal, bool bfailhard)
    {
        return processtransactioncb (
            transaction, badmin, blocal, bfailhard, stcallback ());
    }

    // vfalco workaround for msvc std::function which doesn't swallow return
    // types.
    //
    void processtransactioncbvoid (
        transaction::pointer p,
        bool badmin, bool blocal, bool bfailhard, stcallback cb)
    {
        processtransactioncb (p, badmin, blocal, bfailhard, cb);
    }

    transaction::pointer findtransactionbyid (uint256 const& transactionid);

    int findtransactionsbydestination (
        std::list<transaction::pointer>&,
        rippleaddress const& destinationaccount,
        std::uint32_t startledgerseq, std::uint32_t endledgerseq,
        int maxtransactions);

    //
    // account functions
    //

    accountstate::pointer getaccountstate (
        ledger::ref lrledger, rippleaddress const& accountid);
    sle::pointer getgenerator (
        ledger::ref lrledger, account const& ugeneratorid);

    //
    // directory functions
    //

    stvector256 getdirnodeinfo (
        ledger::ref lrledger, uint256 const& urootindex,
        std::uint64_t& unodeprevious, std::uint64_t& unodenext);

    //
    // owner functions
    //

    json::value getownerinfo (
        ledger::pointer lpledger, rippleaddress const& naaccount);

    //
    // book functions
    //

    void getbookpage (bool badmin, ledger::pointer lpledger, book const&,
        account const& utakerid, const bool bproof, const unsigned int ilimit,
            json::value const& jvmarker, json::value& jvresult);

    // ledger proposal/close functions
    void processtrustedproposal (
        ledgerproposal::pointer proposal,
        std::shared_ptr<protocol::tmproposeset> set,
        rippleaddress nodepublic, uint256 checkledger, bool siggood);

    shamapaddnode gottxdata (
        const std::shared_ptr<peer>& peer, uint256 const& hash,
        const std::list<shamapnodeid>& nodeids,
        const std::list< blob >& nodedata);

    bool recvvalidation (
        stvalidation::ref val, std::string const& source);
    void takeposition (int seq, shamap::ref position);
    shamap::pointer gettxmap (uint256 const& hash);
    bool hastxset (
        const std::shared_ptr<peer>& peer, uint256 const& set,
        protocol::txsetstatus status);

    void mapcomplete (uint256 const& hash, shamap::ref map);
    bool stillneedtxset (uint256 const& hash);
    void makefetchpack (
        job&, std::weak_ptr<peer> peer,
        std::shared_ptr<protocol::tmgetobjectbyhash> request,
        uint256 haveledger, std::uint32_t uuptime);

    bool shouldfetchpack (std::uint32_t seq);
    void gotfetchpack (bool progress, std::uint32_t seq);
    void addfetchpack (uint256 const& hash, std::shared_ptr< blob >& data);
    bool getfetchpack (uint256 const& hash, blob& data);
    int getfetchsize ();
    void sweepfetchpack ();

    // network state machine

    // vfalco todo try to make all these private since they seem to be...private
    //

    // used for the "jump" case
    void switchlastclosedledger (
        ledger::pointer newledger, bool duringconsensus);
    bool checklastclosedledger (
        const overlay::peersequence&, uint256& networkclosed);
    int beginconsensus (
        uint256 const& networkclosed, ledger::pointer closingledger);
    void trystartconsensus ();
    void endconsensus (bool correctlcl);
    void setstandalone ()
    {
        setmode (omfull);
    }

    /** called to initially start our timers.
        not called for stand-alone mode.
    */
    void setstatetimer ();

    void newlcl (int proposers, int convergetime, uint256 const& ledgerhash);
    void neednetworkledger ()
    {
        mneednetworkledger = true;
    }
    void clearneednetworkledger ()
    {
        mneednetworkledger = false;
    }
    bool isneednetworkledger ()
    {
        return mneednetworkledger;
    }
    bool isfull ()
    {
        return !mneednetworkledger && (mmode == omfull);
    }
    void setproposing (bool p, bool v)
    {
        mproposing = p;
        mvalidating = v;
    }
    bool isproposing ()
    {
        return mproposing;
    }
    bool isvalidating ()
    {
        return mvalidating;
    }
    bool isamendmentblocked ()
    {
        return m_amendmentblocked;
    }
    void setamendmentblocked ();
    void consensusviewchange ();
    int getpreviousproposers ()
    {
        return mlastcloseproposers;
    }
    int getpreviousconvergetime ()
    {
        return mlastcloseconvergetime;
    }
    std::uint32_t getlastclosetime ()
    {
        return mlastclosetime;
    }
    void setlastclosetime (std::uint32_t t)
    {
        mlastclosetime = t;
    }
    json::value getconsensusinfo ();
    json::value getserverinfo (bool human, bool admin);
    void clearledgerfetch ();
    json::value getledgerfetchinfo ();
    std::uint32_t acceptledger ();
    proposals & peekstoredproposals ()
    {
        return mstoredproposals;
    }
    void storeproposal (
        ledgerproposal::ref proposal, rippleaddress const& peerpublic);
    uint256 getconsensuslcl ();
    void reportfeechange ();

    void updatelocaltx (ledger::ref newvalidledger) override
    {
        m_localtx->sweep (newvalidledger);
    }
    void addlocaltx (
        ledger::ref openledger, sttx::ref txn) override
    {
        m_localtx->push_back (openledger->getledgerseq(), txn);
    }
    std::size_t getlocaltxcount () override
    {
        return m_localtx->size ();
    }

    //helper function to generate sql query to get transactions
    std::string transactionssql (
        std::string selection, rippleaddress const& account,
        std::int32_t minledger, std::int32_t maxledger,
        bool descending, std::uint32_t offset, int limit,
        bool binary, bool count, bool badmin);

    // client information retrieval functions
    using networkops::accounttxs;
    accounttxs getaccounttxs (
        rippleaddress const& account,
        std::int32_t minledger, std::int32_t maxledger, bool descending,
        std::uint32_t offset, int limit, bool badmin);

    accounttxs gettxsaccount (
        rippleaddress const& account, std::int32_t minledger,
        std::int32_t maxledger, bool forward, json::value& token, int limit,
        bool badmin, const std::string& txtype);

    using networkops::txnmetaledgertype;
    using networkops::metatxslist;

    metatxslist
    getaccounttxsb (
        rippleaddress const& account, std::int32_t minledger,
        std::int32_t maxledger,  bool descending, std::uint32_t offset,
        int limit, bool badmin);

    metatxslist
    gettxsaccountb (
        rippleaddress const& account, std::int32_t minledger,
        std::int32_t maxledger,  bool forward, json::value& token,
        int limit, bool badmin, const std::string& txtype);

    std::vector<rippleaddress> getledgeraffectedaccounts (
        std::uint32_t ledgerseq);

    dividendmaster::pointer getdividendmaster()
    {
        return m_dividendmaster;
    }
    //
    // monitoring: publisher side
    //
    void publedger (ledger::ref lpaccepted);
    void pubproposedtransaction (
        ledger::ref lpcurrent, sttx::ref sttxn, ter terresult);

    //--------------------------------------------------------------------------
    //
    // infosub::source
    //
    void subaccount (
        infosub::ref isplistener,
        const hash_set<rippleaddress>& vnaaccountids,
        std::uint32_t uledgerindex, bool rt);
    void unsubaccount (
        std::uint64_t ulistener, const hash_set<rippleaddress>& vnaaccountids,
        bool rt);

    bool subledger (infosub::ref isplistener, json::value& jvresult);
    bool unsubledger (std::uint64_t ulistener);

    bool subserver (infosub::ref isplistener, json::value& jvresult, bool admin);
    bool unsubserver (std::uint64_t ulistener);

    bool subbook (infosub::ref isplistener, book const&) override;
    bool unsubbook (std::uint64_t ulistener, book const&) override;

    bool subtransactions (infosub::ref isplistener);
    bool unsubtransactions (std::uint64_t ulistener);

    bool subrttransactions (infosub::ref isplistener);
    bool unsubrttransactions (std::uint64_t ulistener);

    infosub::pointer findrpcsub (std::string const& strurl);
    infosub::pointer addrpcsub (std::string const& strurl, infosub::ref);

    //--------------------------------------------------------------------------
    //
    // stoppable

    void onstop ()
    {
        macquiringledger.reset();
        m_heartbeattimer.cancel();
        m_clustertimer.cancel();

        stopped ();
    }

private:
    void setheartbeattimer ();
    void setclustertimer ();
    void ondeadlinetimer (beast::deadlinetimer& timer);
    void processheartbeattimer ();
    void processclustertimer ();

    void setmode (operatingmode);

    json::value transjson (
        const sttx& sttxn, ter terresult, bool bvalidated,
        ledger::ref lpcurrent){
        return networkops_transjson(sttxn, terresult, bvalidated, lpcurrent);
    }
    bool haveconsensusobject ();

    json::value pubbootstrapaccountinfo (
        ledger::ref lpaccepted, rippleaddress const& naaccountid);

    void pubvalidatedtransaction (
        ledger::ref alaccepted, const acceptedledgertx& altransaction);
    void pubaccounttransaction (
        ledger::ref lpcurrent, const acceptedledgertx& altransaction,
        bool isaccepted);

    void pubserver ();

    std::string gethostid (bool foradmin);

private:
    clock_type& m_clock;

    typedef hash_map <account, submaptype> subinfomaptype;
    typedef hash_map<std::string, infosub::pointer> subrpcmaptype;

    // xxx split into more locks.
    typedef ripplerecursivemutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;

    beast::journal m_journal;

    std::unique_ptr <localtxs> m_localtx;
    std::unique_ptr <feevote> m_feevote;
    std::unique_ptr <dividendvote> m_dividendvote;
    dividendmaster::pointer m_dividendmaster;

    locktype mlock;

    std::atomic<operatingmode> mmode;

    std::atomic <bool> mneednetworkledger;
    bool mproposing;
    bool mvalidating;
    bool m_amendmentblocked;

    boost::posix_time::ptime mconnecttime;

    beast::deadlinetimer m_heartbeattimer;
    beast::deadlinetimer m_clustertimer;

    std::shared_ptr<ledgerconsensus> mconsensus;
    networkops::proposals mstoredproposals;

    ledgermaster& m_ledgermaster;
    inboundledger::pointer macquiringledger;

    int mclosetimeoffset;

    // last ledger close
    int mlastcloseproposers;
    int mlastcloseconvergetime;
    uint256 mlastclosehash;

    std::uint32_t mlastclosetime;
    std::uint32_t mlastvalidationtime;
    stvalidation::pointer       mlastvalidation;

    // recent positions taken
    std::map<uint256, std::pair<int, shamap::pointer> > mrecentpositions;

    subinfomaptype msubaccount;
    subinfomaptype msubrtaccount;

    subrpcmaptype mrpcsubmap;

    submaptype msubledger;             // accepted ledgers
    submaptype msubserver;             // when server changes connectivity state
    submaptype msubtransactions;       // all accepted transactions
    submaptype msubrttransactions;     // all proposed and accepted transactions

    taggedcache<uint256, blob>  mfetchpack;
    std::uint32_t mfetchseq;

    std::uint32_t mlastloadbase;
    std::uint32_t mlastloadfactor;

    jobqueue& m_job_queue;

    // whether we are in standalone mode
    bool const m_standalone;

    // the number of nodes that we need to consider ourselves connected
    std::size_t const m_network_quorum;
};

//------------------------------------------------------------------------------
std::string
networkopsimp::gethostid (bool foradmin)
{
    if (foradmin)
        return beast::systemstats::getcomputername ();

    // for non-admin uses we hash the node id into a single rfc1751 word:
    // (this could be cached instead of recalculated every time)
    blob const& addr (getapp ().getlocalcredentials ().getnodepublic ().
            getnodepublic ());

    return rfc1751::getwordfromblob (addr.data (), addr.size ());
}

void networkopsimp::setstatetimer ()
{
    setheartbeattimer ();
    setclustertimer ();
}

void networkopsimp::setheartbeattimer ()
{
    m_heartbeattimer.setexpiration (ledger_granularity / 1000.0);
}

void networkopsimp::setclustertimer ()
{
    m_clustertimer.setexpiration (10.0);
}

void networkopsimp::ondeadlinetimer (beast::deadlinetimer& timer)
{
    if (timer == m_heartbeattimer)
    {
        m_job_queue.addjob (jtnetop_timer, "netops.heartbeat",
            std::bind (&networkopsimp::processheartbeattimer, this));
    }
    else if (timer == m_clustertimer)
    {
        m_job_queue.addjob (jtnetop_cluster, "netops.cluster",
            std::bind (&networkopsimp::processclustertimer, this));
    }
}

void networkopsimp::processheartbeattimer ()
{
    {
        auto lock = getapp().masterlock();

        // vfalco note this is for diagnosing a crash on exit
        application& app (getapp ());
        loadmanager& mgr (app.getloadmanager ());
        mgr.resetdeadlockdetector ();

        std::size_t const numpeers = getapp().overlay ().size ();

        // do we have sufficient peers? if not, we are disconnected.
        if (numpeers < m_network_quorum)
        {
            if (mmode != omdisconnected)
            {
                setmode (omdisconnected);
                m_journal.warning
                    << "node count (" << numpeers << ") "
                    << "has fallen below quorum (" << m_network_quorum << ").";
            }

            setheartbeattimer ();

            return;
        }

        if (mmode == omdisconnected)
        {
            setmode (omconnected);
            m_journal.info << "node count (" << numpeers << ") is sufficient.";
        }

        // check if the last validated ledger forces a change between these
        // states.
        if (mmode == omsyncing)
            setmode (omsyncing);
        else if (mmode == omconnected)
            setmode (omconnected);

        if (!mconsensus)
            trystartconsensus ();

        if (mconsensus)
            mconsensus->timerentry ();
    }

    setheartbeattimer ();
}

void networkopsimp::processclustertimer ()
{
    bool synced = (m_ledgermaster.getvalidatedledgerage() <= 240);
    clusternodestatus us("", synced ? getapp().getfeetrack().getlocalfee() : 0,
                         getnetworktimenc());
    auto& unl = getapp().getunl();
    if (!unl.nodeupdate(getapp().getlocalcredentials().getnodepublic(), us))
    {
        m_journal.debug << "to soon to send cluster update";
        return;
    }

    auto nodes = unl.getclusterstatus();

    protocol::tmcluster cluster;
    for (auto& it: nodes)
    {
        protocol::tmclusternode& node = *cluster.add_clusternodes();
        node.set_publickey(it.first.humannodepublic());
        node.set_reporttime(it.second.getreporttime());
        node.set_nodeload(it.second.getloadfee());
        if (!it.second.getname().empty())
            node.set_nodename(it.second.getname());
    }

    resource::gossip gossip = getapp().getresourcemanager().exportconsumers();
    for (auto& item: gossip.items)
    {
        protocol::tmloadsource& node = *cluster.add_loadsources();
        node.set_name (to_string (item.address));
        node.set_cost (item.balance);
    }
    getapp ().overlay ().foreach (send_if (
        std::make_shared<message>(cluster, protocol::mtcluster),
        peer_in_cluster ()));
    setclustertimer ();
}

//------------------------------------------------------------------------------


std::string networkopsimp::stroperatingmode () const
{
    static char const* pastatustoken [] =
    {
        "disconnected",
        "connected",
        "syncing",
        "tracking",
        "full"
    };

    if (mmode == omfull)
    {
        if (mproposing)
            return "proposing";

        if (mvalidating)
            return "validating";
    }

    return pastatustoken[mmode];
}

boost::posix_time::ptime networkopsimp::getnetworktimept (int& offset) const
{
    offset = 0;
    getapp().getsystemtimeoffset (offset);

    if (std::abs (offset) >= 60)
        m_journal.warning << "large system time offset (" << offset << ").";

    // vfalco todo replace this with a beast call
    return boost::posix_time::microsec_clock::universal_time () +
            boost::posix_time::seconds (offset);
}

std::uint32_t networkopsimp::getnetworktimenc () const
{
    int offset;
    return itoseconds (getnetworktimept (offset));
}

std::uint32_t networkopsimp::getclosetimenc () const
{
    int offset;
    return itoseconds (getnetworktimept (offset) +
                       boost::posix_time::seconds (mclosetimeoffset));
}

std::uint32_t networkopsimp::getclosetimenc (int& offset) const
{
    return itoseconds (getnetworktimept (offset) +
                       boost::posix_time::seconds (mclosetimeoffset));
}

std::uint32_t networkopsimp::getvalidationtimenc ()
{
    std::uint32_t vt = getnetworktimenc ();

    if (vt <= mlastvalidationtime)
        vt = mlastvalidationtime + 1;

    mlastvalidationtime = vt;
    return vt;
}

void networkopsimp::closetimeoffset (int offset)
{
    // take large offsets, ignore small offsets, push towards our wall time
    if (offset > 1)
        mclosetimeoffset += (offset + 3) / 4;
    else if (offset < -1)
        mclosetimeoffset += (offset - 3) / 4;
    else
        mclosetimeoffset = (mclosetimeoffset * 3) / 4;

    if (mclosetimeoffset != 0)
    {
        m_journal.info << "close time offset now " << mclosetimeoffset;

        if (std::abs (mclosetimeoffset) >= 60)
            m_journal.warning << "large close time offset (" << mclosetimeoffset << ").";
    }
}

std::uint32_t networkopsimp::getledgerid (uint256 const& hash)
{
    ledger::pointer  lrledger   = m_ledgermaster.getledgerbyhash (hash);

    return lrledger ? lrledger->getledgerseq () : 0;
}

ledger::pointer networkopsimp::getledgerbyseq (const std::uint32_t seq)
{
    return m_ledgermaster.getledgerbyseq (seq);
}

std::uint32_t networkopsimp::getcurrentledgerid ()
{
    return m_ledgermaster.getcurrentledger ()->getledgerseq ();
}

bool networkopsimp::haveledgerrange (std::uint32_t from, std::uint32_t to)
{
    return m_ledgermaster.haveledgerrange (from, to);
}

bool networkopsimp::haveledger (std::uint32_t seq)
{
    return m_ledgermaster.haveledger (seq);
}

std::uint32_t networkopsimp::getvalidatedseq ()
{
    return m_ledgermaster.getvalidatedledger ()->getledgerseq ();
}

bool networkopsimp::isvalidated (std::uint32_t seq, uint256 const& hash)
{
    if (!isvalidated (seq))
        return false;

    return m_ledgermaster.gethashbyseq (seq) == hash;
}

bool networkopsimp::isvalidated (std::uint32_t seq)
{
    // use when ledger was retrieved by seq
    return haveledger (seq) &&
            seq <= m_ledgermaster.getvalidatedledger ()->getledgerseq ();
}

void networkopsimp::submittransaction (
    job&, sttx::pointer itrans, stcallback callback)
{
    if (isneednetworkledger ())
    {
        // nothing we can do if we've never been in sync
        return;
    }

    // this is an asynchronous interface
    serializer s;
    itrans->add (s);

    serializeriterator sit (s);
    auto trans = std::make_shared<sttx> (std::ref (sit));

    uint256 suppress = trans->gettransactionid ();
    int flags;

    if (getapp().gethashrouter ().addsuppressionpeer (suppress, 0, flags) &&
        ((flags & sf_retry) != 0))
    {
        m_journal.warning << "redundant transactions submitted";
        return;
    }

    if ((flags & sf_bad) != 0)
    {
        m_journal.warning << "submitted transaction cached bad";
        return;
    }

    if ((flags & sf_siggood) == 0)
    {
        try
        {
            if (!passeslocalchecks (*trans) || !trans->checksign ())
            {
                m_journal.warning << "submitted transaction has bad signature";
                getapp().gethashrouter ().setflag (suppress, sf_bad);
                return;
            }

            getapp().gethashrouter ().setflag (suppress, sf_siggood);
        }
        catch (...)
        {
            m_journal.warning << "exception checking transaction " << suppress;
            return;
        }
    }

    m_job_queue.addjob (jttransaction, "submittxn",
        std::bind (&networkopsimp::processtransactioncbvoid,
                   this,
                   std::make_shared<transaction> (trans, validate::no),
                   false,
                   false,
                   false,
                   callback));
}

// sterilize transaction through serialization.
// this is fully synchronous and deprecated
transaction::pointer networkopsimp::submittransactionsync (
    transaction::ref tptrans,
    bool badmin, bool blocal, bool bfailhard, bool bsubmit)
{
    serializer s;
    tptrans->getstransaction ()->add (s);

    auto tptransnew = transaction::sharedtransaction (
        s.getdata (), validate::yes);

    if (!tptransnew)
    {
        // could not construct transaction.
        return tptransnew;
    }

    if (tptransnew->getstransaction ()->isequivalent (
            *tptrans->getstransaction ()))
    {
        if (bsubmit)
            processtransaction (tptransnew, badmin, blocal, bfailhard);
    }
    else
    {
        m_journal.fatal << "transaction reconstruction failure";
        m_journal.fatal << tptransnew->getstransaction ()->getjson (0);
        m_journal.fatal << tptrans->getstransaction ()->getjson (0);

        // assert (false); "1e-95" as amount can trigger this

        tptransnew.reset ();
    }

    return tptransnew;
}

transaction::pointer networkopsimp::processtransactioncb (
    transaction::pointer trans,
    bool badmin, bool blocal, bool bfailhard, stcallback callback)
{
    auto ev = m_job_queue.getloadeventap (jttxn_proc, "processtxn");
    int newflags = getapp().gethashrouter ().getflags (trans->getid ());

    if ((newflags & sf_bad) != 0)
    {
        // cached bad
        trans->setstatus (invalid);
        trans->setresult (tembad_signature);
        return trans;
    }

    if ((newflags & sf_siggood) == 0)
    {
        // signature not checked
        if (!trans->checksign ())
        {
            m_journal.info << "transaction has bad signature";
            trans->setstatus (invalid);
            trans->setresult (tembad_signature);
            getapp().gethashrouter ().setflag (trans->getid (), sf_bad);
            return trans;
        }

        getapp().gethashrouter ().setflag (trans->getid (), sf_siggood);
    }

    {
        auto lock = getapp().masterlock();

        bool didapply;
        ter r = m_ledgermaster.dotransaction (
            trans->getstransaction (),
            badmin ? (tapopen_ledger | tapno_check_sign | tapadmin)
            : (tapopen_ledger | tapno_check_sign), didapply);
        trans->setresult (r);

        if (istemmalformed (r)) // malformed, cache bad
            getapp().gethashrouter ().setflag (trans->getid (), sf_bad);

#ifdef beast_debug
        if (r != tessuccess)
        {
            std::string token, human;
            if (transresultinfo (r, token, human))
                m_journal.info << "transactionresult: "
                               << token << ": " << human;
        }

#endif

        if (callback)
            callback (trans, r);


        if (r == teffailure)
            throw fault (io_error);

        bool addlocal = blocal;

        if (r == tessuccess)
        {
            m_journal.info << "transaction is now included in open ledger";
            trans->setstatus (included);

            // vfalco note the value of trans can be changed here!
            getapp().getmastertransaction ().canonicalize (&trans);
        }
        else if (r == tefpast_seq)
        {
            // duplicate or conflict
            m_journal.info << "transaction is obsolete";
            trans->setstatus (obsolete);
        }
        else if (isterretry (r))
        {
            if (bfailhard)
                addlocal = false;
            else
            {
                // transaction should be held
                m_journal.debug << "transaction should be held: " << r;
                trans->setstatus (held);
                getapp().getmastertransaction ().canonicalize (&trans);
                m_ledgermaster.addheldtransaction (trans);
            }
        }
        else
        {
            m_journal.debug << "status other than success " << r;
            trans->setstatus (invalid);
        }

        if (addlocal)
        {
            addlocaltx (m_ledgermaster.getcurrentledger (),
                        trans->getstransaction ());
        }

        if (didapply || ((mmode != omfull) && !bfailhard && blocal))
        {
            std::set<peer::id_t> peers;

            if (getapp().gethashrouter ().swapset (
                    trans->getid (), peers, sf_relayed))
            {
                protocol::tmtransaction tx;
                serializer s;
                trans->getstransaction ()->add (s);
                tx.set_rawtransaction (&s.getdata ().front (), s.getlength ());
                tx.set_status (protocol::tscurrent);
                tx.set_receivetimestamp (getnetworktimenc ());
                // fixme: this should be when we received it
                getapp ().overlay ().foreach (send_if_not (
                    std::make_shared<message> (tx, protocol::mttransaction),
                    peer_in_set(peers)));
            }
        }
    }

    return trans;
}

transaction::pointer networkopsimp::findtransactionbyid (
    uint256 const& transactionid)
{
    return transaction::load (transactionid);
}

int networkopsimp::findtransactionsbydestination (
    std::list<transaction::pointer>& txns,
    rippleaddress const& destinationaccount, std::uint32_t startledgerseq,
    std::uint32_t endledgerseq, int maxtransactions)
{
    // writeme
    return 0;
}

//
// account functions
//

accountstate::pointer networkopsimp::getaccountstate (
    ledger::ref lrledger, rippleaddress const& accountid)
{
    return lrledger->getaccountstate (accountid);
}

sle::pointer networkopsimp::getgenerator (
    ledger::ref lrledger, account const& ugeneratorid)
{
    if (!lrledger)
        return sle::pointer ();

    return lrledger->getgenerator (ugeneratorid);
}

//
// directory functions
//

// <-- false : no entries
stvector256 networkopsimp::getdirnodeinfo (
    ledger::ref         lrledger,
    uint256 const&      unodeindex,
    std::uint64_t&      unodeprevious,
    std::uint64_t&      unodenext)
{
    stvector256         svindexes;
    sle::pointer        slenode     = lrledger->getdirnode (unodeindex);

    if (slenode)
    {
        m_journal.debug
            << "getdirnodeinfo: node index: " << to_string (unodeindex);

        m_journal.trace
            << "getdirnodeinfo: first: "
            << strhex (slenode->getfieldu64 (sfindexprevious));
        m_journal.trace
            << "getdirnodeinfo:  last: "
            << strhex (slenode->getfieldu64 (sfindexnext));

        unodeprevious = slenode->getfieldu64 (sfindexprevious);
        unodenext = slenode->getfieldu64 (sfindexnext);
        svindexes = slenode->getfieldv256 (sfindexes);

        m_journal.trace
            << "getdirnodeinfo: first: " << strhex (unodeprevious);
        m_journal.trace
            << "getdirnodeinfo:  last: " << strhex (unodenext);
    }
    else
    {
        m_journal.info
            << "getdirnodeinfo: node index: not found: "
            << to_string (unodeindex);

        unodeprevious   = 0;
        unodenext       = 0;
    }

    return svindexes;
}

//
// owner functions
//

json::value networkopsimp::getownerinfo (
    ledger::pointer lpledger, rippleaddress const& naaccount)
{
    json::value jvobjects (json::objectvalue);
    auto urootindex = getownerdirindex (naaccount.getaccountid ());
    auto slenode = lpledger->getdirnode (urootindex);

    if (slenode)
    {
        std::uint64_t  unodedir;

        do
        {
            for (auto const& udirentry : slenode->getfieldv256 (sfindexes))
            {
                auto slecur = lpledger->getslei (udirentry);

                switch (slecur->gettype ())
                {
                case ltoffer:
                    if (!jvobjects.ismember (jss::offers))
                        jvobjects[jss::offers] = json::value (json::arrayvalue);

                    jvobjects[jss::offers].append (slecur->getjson (0));
                    break;

                case ltripple_state:
                    if (!jvobjects.ismember (jss::ripple_lines))
                    {
                        jvobjects[jss::ripple_lines] =
                                json::value (json::arrayvalue);
                    }

                    jvobjects[jss::ripple_lines].append (slecur->getjson (0));
                    break;

                case ltaccount_root:
                case ltdir_node:
                case ltgenerator_map:
                default:
                    assert (false);
                    break;
                }
            }

            unodedir = slenode->getfieldu64 (sfindexnext);

            if (unodedir)
            {
                slenode = lpledger->getdirnode (
                    getdirnodeindex (urootindex, unodedir));
                assert (slenode);
            }
        }
        while (unodedir);
    }

    return jvobjects;
}

//
// other
//

void networkopsimp::setamendmentblocked ()
{
    m_amendmentblocked = true;
    setmode (omtracking);
}

class validationcount
{
public:
    int trustedvalidations, nodesusing;
    nodeid highnodeusing, highvalidation;

    validationcount () : trustedvalidations (0), nodesusing (0)
    {
    }

    bool operator> (const validationcount& v) const
    {
        if (trustedvalidations > v.trustedvalidations)
            return true;

        if (trustedvalidations < v.trustedvalidations)
            return false;

        if (trustedvalidations == 0)
        {
            if (nodesusing > v.nodesusing)
                return true;

            if (nodesusing < v.nodesusing) return
                false;

            return highnodeusing > v.highnodeusing;
        }

        return highvalidation > v.highvalidation;
    }
};

void networkopsimp::trystartconsensus ()
{
    uint256 networkclosed;
    bool ledgerchange = checklastclosedledger (
        getapp().overlay ().getactivepeers (), networkclosed);

    if (networkclosed.iszero ())
        return;

    // writeme: unless we are in omfull and in the process of doing a consensus,
    // we must count how many nodes share our lcl, how many nodes disagree with
    // our lcl, and how many validations our lcl has. we also want to check
    // timing to make sure there shouldn't be a newer lcl. we need this
    // information to do the next three tests.

    if (((mmode == omconnected) || (mmode == omsyncing)) && !ledgerchange)
    {
        // count number of peers that agree with us and unl nodes whose
        // validations we have for lcl.  if the ledger is good enough, go to
        // omtracking - todo
        if (!mneednetworkledger)
            setmode (omtracking);
    }

    if (((mmode == omconnected) || (mmode == omtracking)) && !ledgerchange)
    {
        // check if the ledger is good enough to go to omfull
        // note: do not go to omfull if we don't have the previous ledger
        // check if the ledger is bad enough to go to omconnected -- todo
        if (getapp().getops ().getnetworktimenc () <
            m_ledgermaster.getcurrentledger ()->getclosetimenc ())
        {
            setmode (omfull);
        }
    }

    if ((!mconsensus) && (mmode != omdisconnected))
        beginconsensus (networkclosed, m_ledgermaster.getcurrentledger ());
}

bool networkopsimp::checklastclosedledger (
    const overlay::peersequence& peerlist, uint256& networkclosed)
{
    // returns true if there's an *abnormal* ledger issue, normal changing in
    // tracking mode should return false.  do we have sufficient validations for
    // our last closed ledger? or do sufficient nodes agree? and do we have no
    // better ledger available?  if so, we are either tracking or full.

    m_journal.trace << "networkopsimp::checklastclosedledger";

    ledger::pointer ourclosed = m_ledgermaster.getclosedledger ();

    if (!ourclosed)
        return false;

    uint256 closedledger = ourclosed->gethash ();
    uint256 prevclosedledger = ourclosed->getparenthash ();
    m_journal.trace << "ourclosed:  " << closedledger;
    m_journal.trace << "prevclosed: " << prevclosedledger;

    hash_map<uint256, validationcount> ledgers;
    {
        auto current = getapp().getvalidations ().getcurrentvalidations (
            closedledger, prevclosedledger);

        typedef std::map<uint256, validationcounter>::value_type u256_cvc_pair;
        for (auto & it: current)
        {
            validationcount& vc = ledgers[it.first];
            vc.trustedvalidations += it.second.first;

            if (it.second.second > vc.highvalidation)
                vc.highvalidation = it.second.second;
        }
    }

    validationcount& ourvc = ledgers[closedledger];

    if (mmode >= omtracking)
    {
        ++ourvc.nodesusing;
        auto ouraddress =
                getapp().getlocalcredentials ().getnodepublic ().getnodeid ();

        if (ouraddress > ourvc.highnodeusing)
            ourvc.highnodeusing = ouraddress;
    }

    for (auto& peer: peerlist)
    {
        uint256 peerledger = peer->getclosedledgerhash ();

        if (peerledger.isnonzero ())
        {
            try
            {
                validationcount& vc = ledgers[peerledger];

                if (vc.nodesusing == 0 ||
                    peer->getnodepublic ().getnodeid () > vc.highnodeusing)
                {
                    vc.highnodeusing = peer->getnodepublic ().getnodeid ();
                }

                ++vc.nodesusing;
            }
            catch (...)
            {
                // peer is likely not connected anymore
            }
        }
    }

    validationcount bestvc = ledgers[closedledger];

    // 3) is there a network ledger we'd like to switch to? if so, do we have
    // it?
    bool switchledgers = false;

    for (auto const& it: ledgers)
    {
        m_journal.debug << "l: " << it.first
                        << " t=" << it.second.trustedvalidations
                        << ", n=" << it.second.nodesusing;

        // temporary logging to make sure tiebreaking isn't broken
        if (it.second.trustedvalidations > 0)
            m_journal.trace << "  tiebreaktv: " << it.second.highvalidation;
        else
        {
            if (it.second.nodesusing > 0)
                m_journal.trace << "  tiebreaknu: " << it.second.highnodeusing;
        }

        if (it.second > bestvc)
        {
            bestvc = it.second;
            closedledger = it.first;
            switchledgers = true;
        }
    }

    if (switchledgers && (closedledger == prevclosedledger))
    {
        // don't switch to our own previous ledger
        m_journal.info << "we won't switch to our own previous ledger";
        networkclosed = ourclosed->gethash ();
        switchledgers = false;
    }
    else
        networkclosed = closedledger;

    if (!switchledgers)
    {
        if (macquiringledger)
        {
            macquiringledger->abort ();
            getapp().getinboundledgers ().dropledger (
                macquiringledger->gethash ());
            macquiringledger.reset ();
        }

        return false;
    }

    m_journal.warning << "we are not running on the consensus ledger";
    m_journal.info << "our lcl: " << getjson (*ourclosed);
    m_journal.info << "net lcl " << closedledger;

    if ((mmode == omtracking) || (mmode == omfull))
        setmode (omconnected);

    ledger::pointer consensus = m_ledgermaster.getledgerbyhash (closedledger);

    if (!consensus)
    {
        m_journal.info << "acquiring consensus ledger " << closedledger;

        if (!macquiringledger || (macquiringledger->gethash () != closedledger))
            macquiringledger = getapp().getinboundledgers ().findcreate (
                closedledger, 0, inboundledger::fcconsensus);

        if (!macquiringledger || macquiringledger->isfailed ())
        {
            getapp().getinboundledgers ().dropledger (closedledger);
            m_journal.error << "network ledger cannot be acquired";
            return true;
        }

        if (!macquiringledger->iscomplete ())
            return true;

        clearneednetworkledger ();
        consensus = macquiringledger->getledger ();
    }

    // fixme: if this rewinds the ledger sequence, or has the same sequence, we
    // should update the status on any stored transactions in the invalidated
    // ledgers.
    switchlastclosedledger (consensus, false);

    return true;
}

void networkopsimp::switchlastclosedledger (
    ledger::pointer newledger, bool duringconsensus)
{
    // set the newledger as our last closed ledger -- this is abnormal code

    auto msg = duringconsensus ? "jumpdc" : "jump";
    m_journal.error << msg << " last closed ledger to " << newledger->gethash ();

    clearneednetworkledger ();
    newledger->setclosed ();
    auto openledger = std::make_shared<ledger> (false, std::ref (*newledger));
    m_ledgermaster.switchledgers (newledger, openledger);

    protocol::tmstatuschange s;
    s.set_newevent (protocol::neswitched_ledger);
    s.set_ledgerseq (newledger->getledgerseq ());
    s.set_networktime (getapp().getops ().getnetworktimenc ());
    uint256 hash = newledger->getparenthash ();
    s.set_ledgerhashprevious (hash.begin (), hash.size ());
    hash = newledger->gethash ();
    s.set_ledgerhash (hash.begin (), hash.size ());

    getapp ().overlay ().foreach (send_always (
        std::make_shared<message> (s, protocol::mtstatus_change)));
}

int networkopsimp::beginconsensus (
    uint256 const& networkclosed, ledger::pointer closingledger)
{
    m_journal.info
        << "consensus time for ledger " << closingledger->getledgerseq ();
    m_journal.info
        << " lcl is " << closingledger->getparenthash ();

    auto prevledger
            = m_ledgermaster.getledgerbyhash (closingledger->getparenthash ());

    if (!prevledger)
    {
        // this shouldn't happen unless we jump ledgers
        if (mmode == omfull)
        {
            m_journal.warning << "don't have lcl, going to tracking";
            setmode (omtracking);
        }

        return 3;
    }

    assert (prevledger->gethash () == closingledger->getparenthash ());
    assert (closingledger->getparenthash () ==
            m_ledgermaster.getclosedledger ()->gethash ());

    // create a consensus object to get consensus on this ledger
    assert (!mconsensus);
    prevledger->setimmutable ();

    mconsensus = make_ledgerconsensus (m_clock, *m_localtx, networkclosed,
        prevledger, m_ledgermaster.getcurrentledger ()->getclosetimenc (),
            *m_feevote, *m_dividendvote);

    m_journal.debug << "initiating consensus engine";
    return mconsensus->startup ();
}

bool networkopsimp::haveconsensusobject ()
{
    if (mconsensus != nullptr)
        return true;

    if ((mmode == omfull) || (mmode == omtracking))
    {
        trystartconsensus ();
    }
    else
    {
        // we need to get into the consensus process
        uint256 networkclosed;
        overlay::peersequence peerlist = getapp().overlay ().getactivepeers ();
        bool ledgerchange = checklastclosedledger (peerlist, networkclosed);

        if (!ledgerchange)
        {
            m_journal.info << "beginning consensus due to peer action";
            if ( ((mmode == omtracking) || (mmode == omsyncing)) &&
                 (getpreviousproposers() >= m_ledgermaster.getminvalidations()) )
                setmode (omfull);
            beginconsensus (networkclosed, m_ledgermaster.getcurrentledger ());
        }
    }

    return mconsensus != nullptr;
}

uint256 networkopsimp::getconsensuslcl ()
{
    if (!haveconsensusobject ())
        return uint256 ();

    return mconsensus->getlcl ();
}

void networkopsimp::processtrustedproposal (
    ledgerproposal::pointer proposal,
    std::shared_ptr<protocol::tmproposeset> set, rippleaddress nodepublic,
    uint256 checkledger, bool siggood)
{
    {
        auto lock = getapp().masterlock();

        bool relay = true;

        if (!haveconsensusobject ())
        {
            m_journal.info << "received proposal outside consensus window";

            if (mmode == omfull)
                relay = false;
        }
        else
        {
            storeproposal (proposal, nodepublic);

            uint256 consensuslcl = mconsensus->getlcl ();

            if (!set->has_previousledger () && (checkledger != consensuslcl))
            {
                m_journal.warning
                    << "have to re-check proposal signature due to "
                    << "consensus view change";
                assert (proposal->hassignature ());
                proposal->setprevledger (consensuslcl);

                if (proposal->checksign ())
                    siggood = true;
            }

            if (siggood && (consensuslcl == proposal->getprevledger ()))
            {
                relay = mconsensus->peerposition (proposal);
                m_journal.trace
                    << "proposal processing finished, relay=" << relay;
            }
        }

        if (relay)
        {
            std::set<peer::id_t> peers;
            if (getapp().gethashrouter ().swapset (
                proposal->getsuppressionid (), peers, sf_relayed))
            {
                getapp ().overlay ().foreach (send_if_not (
                    std::make_shared<message> (
                        *set, protocol::mtpropose_ledger),
                    peer_in_set(peers)));
            }
        }
        else
        {
            m_journal.info << "not relaying trusted proposal";
        }
    }
}

// must be called while holding the master lock
shamap::pointer networkopsimp::gettxmap (uint256 const& hash)
{
    auto it = mrecentpositions.find (hash);

    if (it != mrecentpositions.end ())
        return it->second.second;

    if (!haveconsensusobject ())
        return shamap::pointer ();

    return mconsensus->gettransactiontree (hash, false);
}

// must be called while holding the master lock
void networkopsimp::takeposition (int seq, shamap::ref position)
{
    mrecentpositions[position->gethash ()] = std::make_pair (seq, position);

    if (mrecentpositions.size () > 4)
    {
        for (auto i = mrecentpositions.begin (); i != mrecentpositions.end ();)
        {
            if (i->second.first < (seq - 2))
            {
                mrecentpositions.erase (i);
                return;
            }

            ++i;
        }
    }
}

// call with the master lock for now
shamapaddnode networkopsimp::gottxdata (
    const std::shared_ptr<peer>& peer, uint256 const& hash,
    const std::list<shamapnodeid>& nodeids, const std::list< blob >& nodedata)
{

    if (!mconsensus)
    {
        m_journal.warning << "got tx data with no consensus object";
        return shamapaddnode ();
    }

    return mconsensus->peergavenodes (peer, hash, nodeids, nodedata);
}

bool networkopsimp::hastxset (
    const std::shared_ptr<peer>& peer, uint256 const& set,
    protocol::txsetstatus status)
{
    if (mconsensus == nullptr)
    {
        m_journal.info << "peer has tx set, not during consensus";
        return false;
    }

    return mconsensus->peerhasset (peer, set, status);
}

bool networkopsimp::stillneedtxset (uint256 const& hash)
{
    if (!mconsensus)
        return false;

    return mconsensus->stillneedtxset (hash);
}

void networkopsimp::mapcomplete (uint256 const& hash, shamap::ref map)
{
    if (haveconsensusobject ())
        mconsensus->mapcomplete (hash, map, true);
}

void networkopsimp::endconsensus (bool correctlcl)
{
    uint256 deadledger = m_ledgermaster.getclosedledger ()->getparenthash ();

    std::vector <peer::ptr> peerlist = getapp().overlay ().getactivepeers ();

    boost_foreach (peer::ptr const& it, peerlist)
    {
        if (it && (it->getclosedledgerhash () == deadledger))
        {
            m_journal.trace << "killing obsolete peer status";
            it->cyclestatus ();
        }
    }

    mconsensus = std::shared_ptr<ledgerconsensus> ();
}

void networkopsimp::consensusviewchange ()
{
    if ((mmode == omfull) || (mmode == omtracking))
        setmode (omconnected);
}

void networkopsimp::pubserver ()
{
    // vfalco todo don't hold the lock across calls to send...make a copy of the
    //             list into a local array while holding the lock then release the
    //             lock and call send on everyone.
    //
    scopedlocktype sl (mlock);

    if (!msubserver.empty ())
    {
        json::value jvobj (json::objectvalue);

        jvobj [jss::type]          = "serverstatus";
        jvobj [jss::server_status] = stroperatingmode ();
        jvobj [jss::load_base]     =
                (mlastloadbase = getapp().getfeetrack ().getloadbase ());
        jvobj [jss::load_factor]   =
                (mlastloadfactor = getapp().getfeetrack ().getloadfactor ());

        std::string sobj = to_string (jvobj);


        for (auto i = msubserver.begin (); i != msubserver.end (); )
        {
            infosub::pointer p = i->second.lock ();

            // vfalco todo research the possibility of using thread queues and
            //             linearizing the deletion of subscribers with the
            //             sending of json data.
            if (p)
            {
                p->send (jvobj, sobj, true);
                ++i;
            }
            else
            {
                i = msubserver.erase (i);
            }
        }
    }
}

void networkopsimp::setmode (operatingmode om)
{

    if (om == omconnected)
    {
        if (getapp().getledgermaster ().getvalidatedledgerage () < 60)
            om = omsyncing;
    }
    else if (om == omsyncing)
    {
        if (getapp().getledgermaster ().getvalidatedledgerage () >= 60)
            om = omconnected;
    }

    if ((om > omtracking) && m_amendmentblocked)
        om = omtracking;

    if (mmode == om)
        return;

    if ((om >= omconnected) && (mmode == omdisconnected))
        mconnecttime = boost::posix_time::second_clock::universal_time ();

    mmode = om;

    m_journal.stream((om < mmode)
                     ? beast::journal::kwarning : beast::journal::kinfo)
        << "state->" << stroperatingmode ();
    pubserver ();
}


std::string
networkopsimp::transactionssql (
    std::string selection, rippleaddress const& account,
    std::int32_t minledger, std::int32_t maxledger, bool descending,
    std::uint32_t offset, int limit,
    bool binary, bool count, bool badmin)
{
    std::uint32_t nonbinary_page_length = 200;
    std::uint32_t binary_page_length = 500;

    std::uint32_t numberofresults;

    if (count)
    {
        numberofresults = 1000000000;
    }
    else if (limit < 0)
    {
        numberofresults = binary ? binary_page_length : nonbinary_page_length;
    }
    else if (!badmin)
    {
        numberofresults = std::min (
            binary ? binary_page_length : nonbinary_page_length,
            static_cast<std::uint32_t> (limit));
    }
    else
    {
        numberofresults = limit;
    }

    std::string maxclause = "";
    std::string minclause = "";

    if (maxledger != -1)
    {
        maxclause = boost::str (boost::format (
            "and accounttransactions.ledgerseq <= '%u'") % maxledger);
    }

    if (minledger != -1)
    {
        minclause = boost::str (boost::format (
            "and accounttransactions.ledgerseq >= '%u'") % minledger);
    }

    std::string sql;

    if (count)
        sql =
            boost::str (boost::format (
                "select %s from accounttransactions "
                "where account = '%s' %s %s limit %u, %u;")
            % selection
            % account.humanaccountid ()
            % maxclause
            % minclause
            % beast::lexicalcastthrow <std::string> (offset)
            % beast::lexicalcastthrow <std::string> (numberofresults)
        );
    else
        sql =
            boost::str (boost::format (
                "select %s from "
                "accounttransactions inner join transactions "
                "on transactions.transid = accounttransactions.transid "
                "where account = '%s' %s %s "
                "order by accounttransactions.ledgerseq %s, "
                "accounttransactions.txnseq %s, accounttransactions.transid %s "
                "limit %u, %u;")
                    % selection
                    % account.humanaccountid ()
                    % maxclause
                    % minclause
                    % (descending ? "desc" : "asc")
                    % (descending ? "desc" : "asc")
                    % (descending ? "desc" : "asc")
                    % beast::lexicalcastthrow <std::string> (offset)
                    % beast::lexicalcastthrow <std::string> (numberofresults)
                   );
    m_journal.trace << "txsql query: " << sql;
    return sql;
}

networkops::accounttxs networkopsimp::getaccounttxs (
    rippleaddress const& account,
    std::int32_t minledger, std::int32_t maxledger, bool descending,
    std::uint32_t offset, int limit, bool badmin)
{
    // can be called with no locks
    accounttxs ret;

    std::string sql = networkopsimp::transactionssql (
        "accounttransactions.ledgerseq,status,rawtxn,txnmeta", account,
        minledger, maxledger, descending, offset, limit, false, false, badmin);

    {
        auto db = getapp().gettxndb ().getdb ();
        auto sl (getapp().gettxndb ().lock ());

        sql_foreach (db, sql)
        {
            auto txn = transaction::transactionfromsql (db, validate::no);

            serializer rawmeta;
            int metasize = 2048;
            rawmeta.resize (metasize);
            metasize = db->getbinary (
                "txnmeta", &*rawmeta.begin (), rawmeta.getlength ());

            if (metasize > rawmeta.getlength ())
            {
                rawmeta.resize (metasize);
                db->getbinary (
                    "txnmeta", &*rawmeta.begin (), rawmeta.getlength ());
            }
            else
                rawmeta.resize (metasize);

            if (rawmeta.getlength() == 0)
            { // work around a bug that could leave the metadata missing
                auto seq = static_cast<std::uint32_t>(
                    db->getbigint("ledgerseq"));
                m_journal.warning << "recovering ledger " << seq
                                  << ", txn " << txn->getid();
                ledger::pointer ledger = getledgerbyseq(seq);
                if (ledger)
                    ledger->pendsavevalidated(false, false);
            }

            // drop useless dividend before 3501
            if (txn->getledger() > 3501 || txn->getstransaction()->gettxntype() != ttdividend)
            ret.emplace_back (txn, std::make_shared<transactionmetaset> (
                txn->getid (), txn->getledger (), rawmeta.getdata ()));
        }
    }

    return ret;
}

std::vector<networkopsimp::txnmetaledgertype> networkopsimp::getaccounttxsb (
    rippleaddress const& account,
    std::int32_t minledger, std::int32_t maxledger, bool descending,
    std::uint32_t offset, int limit, bool badmin)
{
    // can be called with no locks
    std::vector< txnmetaledgertype> ret;

    std::string sql = networkopsimp::transactionssql (
        "accounttransactions.ledgerseq,status,rawtxn,txnmeta", account,
        minledger, maxledger, descending, offset, limit, true/*binary*/, false,
        badmin);

    {
        auto db = getapp().gettxndb ().getdb ();
        auto sl (getapp().gettxndb ().lock ());

        sql_foreach (db, sql)
        {
            int txnsize = 2048;
            blob rawtxn (txnsize);
            txnsize = db->getbinary ("rawtxn", &rawtxn[0], rawtxn.size ());

            if (txnsize > rawtxn.size ())
            {
                rawtxn.resize (txnsize);
                db->getbinary ("rawtxn", &*rawtxn.begin (), rawtxn.size ());
            }
            else
                rawtxn.resize (txnsize);

            int metasize = 2048;
            blob rawmeta (metasize);
            metasize = db->getbinary ("txnmeta", &rawmeta[0], rawmeta.size ());

            if (metasize > rawmeta.size ())
            {
                rawmeta.resize (metasize);
                db->getbinary ("txnmeta", &*rawmeta.begin (), rawmeta.size ());
            }
            else
                rawmeta.resize (metasize);

            ret.emplace_back (
                strhex (rawtxn), strhex (rawmeta), db->getint ("ledgerseq"));
        }
    }

    return ret;
}


networkopsimp::accounttxs networkopsimp::gettxsaccount (
    rippleaddress const& account, std::int32_t minledger,
    std::int32_t maxledger, bool forward, json::value& token,
    int limit, bool badmin, const std::string& txtype)
{
    accounttxs ret;

    std::uint32_t nonbinary_page_length = 200;
    std::uint32_t extra_length = 100;

    bool foundresume = token.isnull() || !token.isobject();

    std::uint32_t numberofresults, querylimit;
    if (limit <= 0)
        numberofresults = nonbinary_page_length;
    else if (!badmin && (limit > nonbinary_page_length))
        numberofresults = nonbinary_page_length;
    else
        numberofresults = limit;
    querylimit = numberofresults + 1 + (foundresume ? 0 : extra_length);

    std::uint32_t findledger = 0, findseq = 0;
    if (!foundresume)
    {
        try
        {
            if (!token.ismember(jss::ledger) || !token.ismember(jss::seq))
                return ret;
            findledger = token[jss::ledger].asint();
            findseq = token[jss::seq].asint();
        }
        catch (...)
        {
            return ret;
        }
    }
    
    //add trans type support
    std::string txtypesql = "";
    if (txtype != "")
    {
        txtypesql = "and transtype = '" + txtype + "' ";
    }

    // st note we're using the token reference both for passing inputs and
    //         outputs, so we need to clear it in between.
    token = json::nullvalue;

    std::string sql = boost::str (boost::format
        ("select accounttransactions.ledgerseq,accounttransactions.txnseq,"
         "status,rawtxn,txnmeta "
         "from accounttransactions inner join transactions "
         "on transactions.transid = accounttransactions.transid "
         "where accounttransactions.account = '%s' "
         "%s"
         "and accounttransactions.ledgerseq between '%u' and '%u' "
         "order by accounttransactions.ledgerseq %s, "
         "accounttransactions.txnseq %s, accounttransactions.transid %s "
         "limit %u;")
             % account.humanaccountid()
             % txtypesql
             % ((forward && (findledger != 0)) ? findledger : minledger)
             % ((!forward && (findledger != 0)) ? findledger: maxledger)
             % (forward ? "asc" : "desc")
             % (forward ? "asc" : "desc")
             % (forward ? "asc" : "desc")
             % querylimit);
    {
        auto db = getapp().gettxndb ().getdb ();
        auto sl (getapp().gettxndb ().lock ());

        sql_foreach (db, sql)
        {
            if (!foundresume)
            {
                foundresume = (findledger == db->getint("ledgerseq") &&
                               findseq == db->getint("txnseq"));
            }
            else if (numberofresults == 0)
            {
                token = json::objectvalue;
                token[jss::ledger] = db->getint("ledgerseq");
                token[jss::seq] = db->getint("txnseq");
                break;
            }

            if (foundresume)
            {
                auto txn = transaction::transactionfromsql (db, validate::no);

                serializer rawmeta;
                int metasize = 2048;
                rawmeta.resize (metasize);
                metasize = db->getbinary (
                    "txnmeta", &*rawmeta.begin (), rawmeta.getlength ());

                if (metasize > rawmeta.getlength ())
                {
                    rawmeta.resize (metasize);
                    db->getbinary (
                        "txnmeta", &*rawmeta.begin (), rawmeta.getlength ());
                }
                else
                    rawmeta.resize (metasize);

                if (rawmeta.getlength() == 0)
                {
                    // work around a bug that could leave the metadata missing
                    auto seq = static_cast<std::uint32_t>(
                        db->getbigint("ledgerseq"));
                    m_journal.warning << "recovering ledger " << seq
                                      << ", txn " << txn->getid();
                    ledger::pointer ledger = getledgerbyseq(seq);
                    if (ledger)
                        ledger->pendsavevalidated(false, false);
                }

                --numberofresults;

                // drop useless dividend before 3501
                if (txn->getledger() > 3501 || txn->getstransaction()->gettxntype() != ttdividend)
                ret.emplace_back (std::move (txn),
                    std::make_shared<transactionmetaset> (
                        txn->getid (), txn->getledger (), rawmeta.getdata ()));
            }
        }
    }

    return ret;
}

networkopsimp::metatxslist networkopsimp::gettxsaccountb (
    rippleaddress const& account, std::int32_t minledger,
    std::int32_t maxledger,  bool forward, json::value& token,
    int limit, bool badmin, const std::string& txtype)
{
    metatxslist ret;

    std::uint32_t binary_page_length = 500;
    std::uint32_t extra_length = 100;

    bool foundresume = token.isnull() || !token.isobject();

    std::uint32_t numberofresults, querylimit;
    if (limit <= 0)
        numberofresults = binary_page_length;
    else if (!badmin && (limit > binary_page_length))
        numberofresults = binary_page_length;
    else
        numberofresults = limit;
    querylimit = numberofresults + 1 + (foundresume ? 0 : extra_length);

    std::uint32_t findledger = 0, findseq = 0;
    if (!foundresume)
    {
        try
        {
            if (!token.ismember(jss::ledger) || !token.ismember(jss::seq))
                return ret;
            findledger = token[jss::ledger].asint();
            findseq = token[jss::seq].asint();
        }
        catch (...)
        {
            return ret;
        }
    }

    token = json::nullvalue;

    //add trans type support
    std::string txtypesql = "";
    if (txtype != "")
    {
        txtypesql = "and transtype = '" + txtype + "' ";
    }
    
    std::string sql = boost::str (boost::format
        ("select accounttransactions.ledgerseq,accounttransactions.txnseq,"
         "status,rawtxn,txnmeta "
         "from accounttransactions inner join transactions "
         "on transactions.transid = accounttransactions.transid "
         "where accounttransactions.account = '%s' "
         "%s"
         "and accounttransactions.ledgerseq between '%u' and '%u' "
         "order by accounttransactions.ledgerseq %s, "
         "accounttransactions.txnseq %s, accounttransactions.transid %s "
         "limit %u;")
             % account.humanaccountid()
             % txtypesql
             % ((forward && (findledger != 0)) ? findledger : minledger)
             % ((!forward && (findledger != 0)) ? findledger: maxledger)
             % (forward ? "asc" : "desc")
             % (forward ? "asc" : "desc")
             % (forward ? "asc" : "desc")
             % querylimit);
    {
        auto db = getapp().gettxndb ().getdb ();
        auto sl (getapp().gettxndb ().lock ());

        sql_foreach (db, sql)
        {
            if (!foundresume)
            {
                if (findledger == db->getint("ledgerseq") &&
                    findseq == db->getint("txnseq"))
                {
                    foundresume = true;
                }
            }
            else if (numberofresults == 0)
            {
                token = json::objectvalue;
                token[jss::ledger] = db->getint("ledgerseq");
                token[jss::seq] = db->getint("txnseq");
                break;
            }

            if (foundresume)
            {
                int txnsize = 2048;
                blob rawtxn (txnsize);
                txnsize = db->getbinary ("rawtxn", &rawtxn[0], rawtxn.size ());

                if (txnsize > rawtxn.size ())
                {
                    rawtxn.resize (txnsize);
                    db->getbinary ("rawtxn", &*rawtxn.begin (), rawtxn.size ());
                }
                else
                    rawtxn.resize (txnsize);

                int metasize = 2048;
                blob rawmeta (metasize);
                metasize = db->getbinary (
                    "txnmeta", &rawmeta[0], rawmeta.size ());

                if (metasize > rawmeta.size ())
                {
                    rawmeta.resize (metasize);
                    db->getbinary (
                        "txnmeta", &*rawmeta.begin (), rawmeta.size ());
                }
                else
                {
                    rawmeta.resize (metasize);
                }

                ret.emplace_back (strhex (rawtxn), strhex (rawmeta),
                                  db->getint ("ledgerseq"));
                --numberofresults;
            }
        }
    }

    return ret;
}


std::vector<rippleaddress>
networkopsimp::getledgeraffectedaccounts (std::uint32_t ledgerseq)
{
    std::vector<rippleaddress> accounts;
    std::string sql = str (boost::format (
        "select distinct account from accounttransactions "
        "indexed by acctlgrindex where ledgerseq = '%u';")
                           % ledgerseq);
    rippleaddress acct;
    {
        auto db = getapp().gettxndb ().getdb ();
        auto sl (getapp().gettxndb ().lock ());
        sql_foreach (db, sql)
        {
            if (acct.setaccountid (db->getstrbinary ("account")))
                accounts.push_back (acct);
        }
    }
    return accounts;
}

bool networkopsimp::recvvalidation (
    stvalidation::ref val, std::string const& source)
{
    m_journal.debug << "recvvalidation " << val->getledgerhash ()
                    << " from " << source;
    return getapp().getvalidations ().addvalidation (val, source);
}

json::value networkopsimp::getconsensusinfo ()
{
    if (mconsensus)
        return mconsensus->getjson (true);

    json::value info = json::objectvalue;
    info[jss::consensus] = "none";
    return info;
}


json::value networkopsimp::getserverinfo (bool human, bool admin)
{
    json::value info = json::objectvalue;

    // hostid: unique string describing the machine
    if (human)
        info [jss::hostid] = gethostid (admin);

    info [jss::build_version] = buildinfo::getversionstring ();

    info [jss::server_state] = stroperatingmode ();

    if (mneednetworkledger)
        info[jss::network_ledger] = jss::waiting;

    info[jss::validation_quorum] = m_ledgermaster.getminvalidations ();

    info["io_latency_ms"] = static_cast<json::uint> (
        getapp().getiolatency().count());

    if (admin)
    {
        if (getconfig ().validation_pub.isvalid ())
        {
            info[jss::pubkey_validator] =
                    getconfig ().validation_pub.humannodepublic ();
        }
        else
        {
            info[jss::pubkey_validator] = jss::none;
        }
    }

    info[jss::pubkey_node] =
            getapp().getlocalcredentials ().getnodepublic ().humannodepublic ();


    info[jss::complete_ledgers] =
            getapp().getledgermaster ().getcompleteledgers ();

    if (m_amendmentblocked)
        info[jss::amendment_blocked] = true;

    size_t fp = mfetchpack.getcachesize ();

    if (fp != 0)
        info[jss::fetch_pack] = json::uint (fp);

    info[jss::peers] = json::uint (getapp ().overlay ().size ());

    json::value lastclose = json::objectvalue;
    lastclose[jss::proposers] = getapp().getops ().getpreviousproposers ();

    if (human)
    {
        lastclose[jss::converge_time_s] = static_cast<double> (
            getapp().getops ().getpreviousconvergetime ()) / 1000.0;
    }
    else
    {
        lastclose[jss::converge_time] =
                json::int (getapp().getops ().getpreviousconvergetime ());
    }

    info[jss::last_close] = lastclose;

    //  if (mconsensus)
    //      info[jss::consensus] = mconsensus->getjson();

    if (admin)
        info[jss::load] = m_job_queue.getjson ();

    if (!human)
    {
        info[jss::load_base] = getapp().getfeetrack ().getloadbase ();
        info[jss::load_factor] = getapp().getfeetrack ().getloadfactor ();
    }
    else
    {
        info[jss::load_factor] =
            static_cast<double> (getapp().getfeetrack ().getloadfactor ()) /
                getapp().getfeetrack ().getloadbase ();
        if (admin)
        {
            std::uint32_t base = getapp().getfeetrack().getloadbase();
            std::uint32_t fee = getapp().getfeetrack().getlocalfee();
            if (fee != base)
                info[jss::load_factor_local] =
                    static_cast<double> (fee) / base;
            fee = getapp().getfeetrack ().getremotefee();
            if (fee != base)
                info[jss::load_factor_net] =
                    static_cast<double> (fee) / base;
            fee = getapp().getfeetrack().getclusterfee();
            if (fee != base)
                info[jss::load_factor_cluster] =
                    static_cast<double> (fee) / base;
        }
    }

    bool valid = false;
    ledger::pointer lpclosed    = getvalidatedledger ();

    if (lpclosed)
        valid = true;
    else
        lpclosed                = getclosedledger ();

    if (lpclosed)
    {
        std::uint64_t basefee = lpclosed->getbasefee ();
        std::uint64_t baseref = lpclosed->getreferencefeeunits ();
        json::value l (json::objectvalue);
        l[jss::seq] = json::uint (lpclosed->getledgerseq ());
        l[jss::hash] = to_string (lpclosed->gethash ());

        if (!human)
        {
            l[jss::base_fee] = json::value::uint (basefee);
            l[jss::reserve_base] = json::value::uint (lpclosed->getreserve (0));
            l[jss::reserve_inc] =
                    json::value::uint (lpclosed->getreserveinc ());
            l[jss::close_time] =
                    json::value::uint (lpclosed->getclosetimenc ());
        }
        else
        {
            l[jss::base_fee_xrp] = static_cast<double> (basefee) /
                    system_currency_parts;
            l[jss::reserve_base_xrp]   =
                static_cast<double> (json::uint (
                    lpclosed->getreserve (0) * basefee / baseref))
                    / system_currency_parts;
            l[jss::reserve_inc_xrp]    =
                static_cast<double> (json::uint (
                    lpclosed->getreserveinc () * basefee / baseref))
                    / system_currency_parts;

            int offset;
            std::uint32_t closetime (getclosetimenc (offset));
            if (std::abs (offset) >= 60)
                l[jss::system_time_offset] = offset;

            std::uint32_t lclosetime (lpclosed->getclosetimenc ());
            if (std::abs (mclosetimeoffset) >= 60)
                l[jss::close_time_offset] = mclosetimeoffset;

            if (lclosetime <= closetime)
            {
                std::uint32_t age = closetime - lclosetime;

                if (age < 1000000)
                    l[jss::age] = json::uint (age);
            }
        }

        if (valid)
            info[jss::validated_ledger] = l;
        else
            info[jss::closed_ledger] = l;

        ledger::pointer lppublished = getpublishedledger ();
        if (!lppublished)
            info[jss::published_ledger] = jss::none;
        else if (lppublished->getledgerseq() != lpclosed->getledgerseq())
            info[jss::published_ledger] = lppublished->getledgerseq();
    }

    return info;
}

void networkopsimp::clearledgerfetch ()
{
    getapp().getinboundledgers().clearfailures();
}

json::value networkopsimp::getledgerfetchinfo ()
{
    return getapp().getinboundledgers().getinfo();
}

//
// monitoring: publisher side
//

json::value networkopsimp::pubbootstrapaccountinfo (
    ledger::ref lpaccepted, rippleaddress const& naaccountid)
{
    json::value         jvobj (json::objectvalue);

    jvobj["type"]           = "accountinfobootstrap";
    jvobj["account"]        = naaccountid.humanaccountid ();
    jvobj["owner"]          = getownerinfo (lpaccepted, naaccountid);
    jvobj["ledger_index"]   = lpaccepted->getledgerseq ();
    jvobj["ledger_hash"]    = to_string (lpaccepted->gethash ());
    jvobj["ledger_time"]
            = json::value::uint (utfromseconds (lpaccepted->getclosetimenc ()));

    return jvobj;
}

void networkopsimp::pubproposedtransaction (
    ledger::ref lpcurrent, sttx::ref sttxn, ter terresult)
{
    json::value jvobj   = transjson (*sttxn, terresult, false, lpcurrent);

    {
        scopedlocktype sl (mlock);

        auto it = msubrttransactions.begin ();
        while (it != msubrttransactions.end ())
        {
            infosub::pointer p = it->second.lock ();

            if (p)
            {
                p->send (jvobj, true);
                ++it;
            }
            else
            {
                it = msubrttransactions.erase (it);
            }
        }
    }
    acceptedledgertx alt (lpcurrent, sttxn, terresult);
    if (m_journal.trace.active())
        m_journal.trace << "pubproposed: " << alt.getjson ();
    pubaccounttransaction (lpcurrent, alt, false);
}

void networkopsimp::publedger (ledger::ref accepted)
{
    // ledgers are published only when they acquire sufficient validations
    // holes are filled across connection loss or other catastrophe

    auto alpaccepted = acceptedledger::makeacceptedledger (accepted);
    ledger::ref lpaccepted = alpaccepted->getledger ();

    {
        scopedlocktype sl (mlock);

        if (!msubledger.empty ())
        {
            json::value jvobj (json::objectvalue);

            jvobj[jss::type] = jss::ledgerclosed;
            jvobj[jss::ledger_index] = lpaccepted->getledgerseq ();
            jvobj[jss::ledger_hash] = to_string (lpaccepted->gethash ());
            jvobj[jss::ledger_time]
                    = json::value::uint (lpaccepted->getclosetimenc ());

            jvobj[jss::fee_ref]
                    = json::uint (lpaccepted->getreferencefeeunits ());
            jvobj[jss::fee_base] = json::uint (lpaccepted->getbasefee ());
            jvobj[jss::reserve_base] = json::uint (lpaccepted->getreserve (0));
            jvobj[jss::reserve_inc] = json::uint (lpaccepted->getreserveinc ());

            jvobj[jss::txn_count] = json::uint (alpaccepted->gettxncount ());

            if (mmode >= omsyncing)
            {
                jvobj[jss::validated_ledgers]
                        = getapp().getledgermaster ().getcompleteledgers ();
            }

            auto it = msubledger.begin ();
            while (it != msubledger.end ())
            {
                infosub::pointer p = it->second.lock ();
                if (p)
                {
                    p->send (jvobj, true);
                    ++it;
                }
                else
                    it = msubledger.erase (it);
            }
        }
    }
    
    m_journal.info << "start pubaccepted: " << alpaccepted->getmap ().size ();
    // don't lock since pubacceptedtransaction is locking.
    boost_foreach (const acceptedledger::value_type & vt, alpaccepted->getmap ())
    {
        if (m_journal.trace.active())
            m_journal.trace << "pubaccepted: " << vt.second->getjson ();
        pubvalidatedtransaction (lpaccepted, *vt.second);
    }
    m_journal.info << "finish pubaccepted: " << alpaccepted->getmap ().size ();
}

void networkopsimp::reportfeechange ()
{
    if ((getapp().getfeetrack ().getloadbase () == mlastloadbase) &&
            (getapp().getfeetrack ().getloadfactor () == mlastloadfactor))
        return;

    m_job_queue.addjob (
        jtclient, "reportfeechange->pubserver",
        std::bind (&networkopsimp::pubserver, this));
}

// this routine should only be used to publish accepted or validated
// transactions.
json::value networkops_transjson(
    const sttx& sttxn, ter terresult, bool bvalidated,
    ledger::ref lpcurrent)
{
    json::value jvobj (json::objectvalue);
    std::string stoken;
    std::string shuman;

    transresultinfo (terresult, stoken, shuman);

    jvobj[jss::type]           = jss::transaction;
    jvobj[jss::transaction]    = sttxn.getjson (0);

    if (bvalidated)
    {
        jvobj[jss::ledger_index]           = lpcurrent->getledgerseq ();
        jvobj[jss::ledger_hash]            = to_string (lpcurrent->gethash ());
        jvobj[jss::transaction][jss::date]  = lpcurrent->getclosetimenc ();
        jvobj[jss::validated]              = true;

        // writeme: put the account next seq here

    }
    else
    {
        jvobj[jss::validated]              = false;
        jvobj[jss::ledger_current_index]   = lpcurrent->getledgerseq ();
    }

    jvobj[jss::status]                 = bvalidated ? jss::closed : jss::proposed;
    jvobj[jss::engine_result]          = stoken;
    jvobj[jss::engine_result_code]     = terresult;
    jvobj[jss::engine_result_message]  = shuman;

    if (sttxn.gettxntype() == ttoffer_create)
    {
        auto const account (sttxn.getsourceaccount ().getaccountid ());
        auto const amount (sttxn.getfieldamount (sftakergets));

        // if the offer create is not self funded then add the owner balance
        if (account != amount.issue ().account)
        {
            ledgerentryset les (lpcurrent, tapnone, true);
            auto const ownerfunds (les.accountfunds (account, amount, fhignore_freeze));

            jvobj[jss::transaction][jss::owner_funds] = ownerfunds.gettext ();
        }
    }

    return jvobj;
}

void networkopsimp::pubvalidatedtransaction (
    ledger::ref alaccepted, const acceptedledgertx& altx)
{
    json::value jvobj;

    std::string sobj;
    bool bsobjinitialized = false;

    {
        scopedlocktype sl (mlock);

        auto it = msubtransactions.begin ();
        while (it != msubtransactions.end ())
        {
            infosub::pointer p = it->second.lock ();

            if (p)
            {
                if (!bsobjinitialized) {
                    jvobj = transjson (*altx.gettxn (), altx.getresult (), true, alaccepted);
                    jvobj[jss::meta] = altx.getmeta ()->getjson (0);
                    sobj = to_string (jvobj);
                    bsobjinitialized = true;
                }
                p->send (jvobj, sobj, true);
                ++it;
            }
            else
                it = msubtransactions.erase (it);
        }

        it = msubrttransactions.begin ();

        while (it != msubrttransactions.end ())
        {
            infosub::pointer p = it->second.lock ();

            if (p)
            {
                if (!bsobjinitialized) {
                    jvobj = transjson (*altx.gettxn (), altx.getresult (), true, alaccepted);
                    jvobj[jss::meta] = altx.getmeta ()->getjson (0);
                    sobj = to_string (jvobj);
                    bsobjinitialized = true;
                }
                p->send (jvobj, sobj, true);
                ++it;
            }
            else
                it = msubrttransactions.erase (it);
        }
    }
    getapp().getorderbookdb ().processtxn (alaccepted, altx);
    pubaccounttransaction (alaccepted, altx, true);
}

void networkopsimp::pubaccounttransaction (
    ledger::ref lpcurrent, const acceptedledgertx& altx, bool baccepted)
{
    hash_set<infosub::pointer>  notify;
    int                             iproposed   = 0;
    int                             iaccepted   = 0;

    {
        scopedlocktype sl (mlock);

        if (!baccepted && msubrtaccount.empty ()) return;

        if (!msubaccount.empty () || (!msubrtaccount.empty ()) )
        {
            for (auto const& affectedaccount: altx.getaffected ())
            {
                auto simiit
                        = msubrtaccount.find (affectedaccount.getaccountid ());
                if (simiit != msubrtaccount.end ())
                {
                    auto it = simiit->second.begin ();

                    while (it != simiit->second.end ())
                    {
                        infosub::pointer p = it->second.lock ();

                        if (p)
                        {
                            notify.insert (p);
                            ++it;
                            ++iproposed;
                        }
                        else
                            it = simiit->second.erase (it);
                    }
                }

                if (baccepted)
                {
                    simiit  = msubaccount.find (affectedaccount.getaccountid ());

                    if (simiit != msubaccount.end ())
                    {
                        auto it = simiit->second.begin ();
                        while (it != simiit->second.end ())
                        {
                            infosub::pointer p = it->second.lock ();

                            if (p)
                            {
                                notify.insert (p);
                                ++it;
                                ++iaccepted;
                            }
                            else
                                it = simiit->second.erase (it);
                        }
                    }
                }
            }
        }
    }
    m_journal.debug << "pubaccounttransaction:" <<
        " iproposed=" << iproposed <<
        " iaccepted=" << iaccepted;

    if (!notify.empty ())
    {
        json::value jvobj = transjson (
            *altx.gettxn (), altx.getresult (), baccepted, lpcurrent);

        if (altx.isapplied ())
            jvobj[jss::meta] = altx.getmeta ()->getjson (0);

        std::string sobj = to_string (jvobj);

        boost_foreach (infosub::ref isrlistener, notify)
        {
            isrlistener->send (jvobj, sobj, true);
        }
    }
}

//
// monitoring
//

void networkopsimp::subaccount (infosub::ref isrlistener,
    const hash_set<rippleaddress>& vnaaccountids,
    std::uint32_t uledgerindex, bool rt)
{
    subinfomaptype& submap = rt ? msubrtaccount : msubaccount;

    // for the connection, monitor each account.
    boost_foreach (const rippleaddress & naaccountid, vnaaccountids)
    {
        m_journal.trace << "subaccount:"
            " account: " << naaccountid.humanaccountid ();

        isrlistener->insertsubaccountinfo (naaccountid, uledgerindex);
    }

    scopedlocktype sl (mlock);

    boost_foreach (const rippleaddress & naaccountid, vnaaccountids)
    {
        auto simiterator = submap.find (naaccountid.getaccountid ());
        if (simiterator == submap.end ())
        {
            // not found, note that account has a new single listner.
            submaptype  usiselement;
            usiselement[isrlistener->getseq ()] = isrlistener;
            submap.insert (simiterator, make_pair (naaccountid.getaccountid (),
                                                   usiselement));
        }
        else
        {
            // found, note that the account has another listener.
            simiterator->second[isrlistener->getseq ()] = isrlistener;
        }
    }
}

void networkopsimp::unsubaccount (
    std::uint64_t useq,
    hash_set<rippleaddress> const& vnaaccountids,
    bool rt)
{
    subinfomaptype& submap = rt ? msubrtaccount : msubaccount;

    // for the connection, unmonitor each account.
    // fixme: don't we need to unsub?
    // boost_foreach(rippleaddress const& naaccountid, vnaaccountids)
    // {
    //  isrlistener->deletesubaccountinfo(naaccountid);
    // }

    scopedlocktype sl (mlock);

    for (auto const& naaccountid : vnaaccountids)
    {
        auto simiterator = submap.find (naaccountid.getaccountid ());

        if (simiterator != submap.end ())
        {
            // found
            simiterator->second.erase (useq);

            if (simiterator->second.empty ())
            {
                // don't need hash entry.
                submap.erase (simiterator);
            }
        }
    }
}

bool networkopsimp::subbook (infosub::ref isrlistener, book const& book)
{
    if (auto listeners = getapp().getorderbookdb ().makebooklisteners (book))
        listeners->addsubscriber (isrlistener);
    else
        assert (false);
    return true;
}

bool networkopsimp::unsubbook (std::uint64_t useq, book const& book)
{
    if (auto listeners = getapp().getorderbookdb ().getbooklisteners (book))
        listeners->removesubscriber (useq);

    return true;
}

void networkopsimp::newlcl (
    int proposers, int convergetime, uint256 const& ledgerhash)
{
    assert (convergetime);
    mlastcloseproposers = proposers;
    mlastcloseconvergetime = convergetime;
    mlastclosehash = ledgerhash;
}

std::uint32_t networkopsimp::acceptledger ()
{
    // accept the current transaction tree, return the new ledger's sequence
    beginconsensus (
        m_ledgermaster.getclosedledger ()->gethash (),
        m_ledgermaster.getcurrentledger ());
    mconsensus->simulate ();
    return m_ledgermaster.getcurrentledger ()->getledgerseq ();
}

void networkopsimp::storeproposal (
    ledgerproposal::ref proposal, rippleaddress const& peerpublic)
{
    auto& props = mstoredproposals[peerpublic.getnodeid ()];

    if (props.size () >= (unsigned) (mlastcloseproposers + 10))
        props.pop_front ();

    props.push_back (proposal);
}

// <-- bool: true=added, false=already there
bool networkopsimp::subledger (infosub::ref isrlistener, json::value& jvresult)
{
    ledger::pointer lpclosed    = getvalidatedledger ();

    if (lpclosed)
    {
        jvresult[jss::ledger_index]    = lpclosed->getledgerseq ();
        jvresult[jss::ledger_hash]     = to_string (lpclosed->gethash ());
        jvresult[jss::ledger_time]
                = json::value::uint (lpclosed->getclosetimenc ());
        jvresult[jss::fee_ref]
                = json::uint (lpclosed->getreferencefeeunits ());
        jvresult[jss::fee_base]        = json::uint (lpclosed->getbasefee ());
        jvresult[jss::reserve_base]    = json::uint (lpclosed->getreserve (0));
        jvresult[jss::reserve_inc]     = json::uint (lpclosed->getreserveinc ());
    }

    if ((mmode >= omsyncing) && !isneednetworkledger ())
    {
        jvresult[jss::validated_ledgers]
                = getapp().getledgermaster ().getcompleteledgers ();
    }

    scopedlocktype sl (mlock);
    return msubledger.emplace (isrlistener->getseq (), isrlistener).second;
}

// <-- bool: true=erased, false=was not there
bool networkopsimp::unsubledger (std::uint64_t useq)
{
    scopedlocktype sl (mlock);
    return msubledger.erase (useq);
}

// <-- bool: true=added, false=already there
bool networkopsimp::subserver (infosub::ref isrlistener, json::value& jvresult,
    bool admin)
{
    uint256 urandom;

    if (m_standalone)
        jvresult[jss::stand_alone] = m_standalone;

    // checkme: is it necessary to provide a random number here?
    random_fill (urandom.begin (), urandom.size ());

    jvresult[jss::random]          = to_string (urandom);
    jvresult[jss::server_status]   = stroperatingmode ();
    jvresult[jss::load_base]       = getapp().getfeetrack ().getloadbase ();
    jvresult[jss::load_factor]     = getapp().getfeetrack ().getloadfactor ();
    jvresult [jss::hostid]         = gethostid (admin);
    jvresult[jss::pubkey_node]     = getapp ().getlocalcredentials ().
        getnodepublic ().humannodepublic ();

    scopedlocktype sl (mlock);
    return msubserver.emplace (isrlistener->getseq (), isrlistener).second;
}

// <-- bool: true=erased, false=was not there
bool networkopsimp::unsubserver (std::uint64_t useq)
{
    scopedlocktype sl (mlock);
    return msubserver.erase (useq);
}

// <-- bool: true=added, false=already there
bool networkopsimp::subtransactions (infosub::ref isrlistener)
{
    scopedlocktype sl (mlock);
    return msubtransactions.emplace (
        isrlistener->getseq (), isrlistener).second;
}

// <-- bool: true=erased, false=was not there
bool networkopsimp::unsubtransactions (std::uint64_t useq)
{
    scopedlocktype sl (mlock);
    return msubtransactions.erase (useq);
}

// <-- bool: true=added, false=already there
bool networkopsimp::subrttransactions (infosub::ref isrlistener)
{
    scopedlocktype sl (mlock);
    return msubrttransactions.emplace (
        isrlistener->getseq (), isrlistener).second;
}

// <-- bool: true=erased, false=was not there
bool networkopsimp::unsubrttransactions (std::uint64_t useq)
{
    scopedlocktype sl (mlock);
    return msubrttransactions.erase (useq);
}

infosub::pointer networkopsimp::findrpcsub (std::string const& strurl)
{
    scopedlocktype sl (mlock);

    subrpcmaptype::iterator it = mrpcsubmap.find (strurl);

    if (it != mrpcsubmap.end ())
        return it->second;

    return infosub::pointer ();
}

infosub::pointer networkopsimp::addrpcsub (
    std::string const& strurl, infosub::ref rspentry)
{
    scopedlocktype sl (mlock);

    mrpcsubmap.emplace (strurl, rspentry);

    return rspentry;
}

#ifndef use_new_book_page

// nikb fixme this should be looked at. there's no reason why this shouldn't
//            work, but it demonstrated poor performance.
//
// fixme : support ilimit.
void networkopsimp::getbookpage (
    bool badmin,
    ledger::pointer lpledger,
    book const& book,
    account const& utakerid,
    bool const bproof,
    const unsigned int ilimit,
    json::value const& jvmarker,
    json::value& jvresult)
{ // caution: this is the old get book page logic
    json::value& jvoffers =
            (jvresult[jss::offers] = json::value (json::arrayvalue));

    std::map<account, stamount> umbalance;
    const uint256   ubookbase   = getbookbase (book);
    const uint256   ubookend    = getqualitynext (ubookbase);
    uint256         utipindex   = ubookbase;

    if (m_journal.trace)
    {
        m_journal.trace << "getbookpage:" << book;
        m_journal.trace << "getbookpage: ubookbase=" << ubookbase;
        m_journal.trace << "getbookpage: ubookend=" << ubookend;
        m_journal.trace << "getbookpage: utipindex=" << utipindex;
    }

    ledgerentryset  lesactive (lpledger, tapnone, true);

    const bool      bglobalfreeze =  lesactive.isglobalfrozen (book.out.account) ||
                                     lesactive.isglobalfrozen (book.in.account);

    bool            bdone           = false;
    bool            bdirectadvance  = true;

    sle::pointer    sleofferdir;
    uint256         offerindex;
    unsigned int    ubookentry;
    stamount        sadirrate;

    auto utransferrate = rippletransferrate (lesactive, book.out.account);

    unsigned int left (ilimit == 0 ? 300 : ilimit);
    if (! badmin && left > 300)
        left = 300;

    while (!bdone && left-- > 0)
    {
        if (bdirectadvance)
        {
            bdirectadvance  = false;

            m_journal.trace << "getbookpage: bdirectadvance";

            sleofferdir = lesactive.entrycache (
                ltdir_node, lpledger->getnextledgerindex (utipindex, ubookend));

            if (!sleofferdir)
            {
                m_journal.trace << "getbookpage: bdone";
                bdone           = true;
            }
            else
            {
                utipindex = sleofferdir->getindex ();
                sadirrate = amountfromquality (getquality (utipindex));

                lesactive.dirfirst (
                    utipindex, sleofferdir, ubookentry, offerindex);

                m_journal.trace << "getbookpage:   utipindex=" << utipindex;
                m_journal.trace << "getbookpage: offerindex=" << offerindex;
            }
        }

        if (!bdone)
        {
            auto sleoffer = lesactive.entrycache (ltoffer, offerindex);

            if (sleoffer)
            {
                auto const uofferownerid =
                        sleoffer->getfieldaccount160 (sfaccount);
                auto const& satakergets =
                        sleoffer->getfieldamount (sftakergets);
                auto const& satakerpays =
                        sleoffer->getfieldamount (sftakerpays);
                stamount saownerfunds;
                bool firstowneroffer (true);

                if (book.out.account == uofferownerid)
                {
                    // if an offer is selling issuer's own ious, it is fully
                    // funded.
                    saownerfunds    = satakergets;
                }
                else if (bglobalfreeze)
                {
                    // if either asset is globally frozen, consider all offers
                    // that aren't ours to be totally unfunded
                    saownerfunds.clear (issueref (book.out.currency, book.out.account));
                }
                else
                {
                    auto umbalanceentry  = umbalance.find (uofferownerid);
                    if (umbalanceentry != umbalance.end ())
                    {
                        // found in running balance table.

                        saownerfunds    = umbalanceentry->second;
                        firstowneroffer = false;
                    }
                    else
                    {
                        // did not find balance in table.

                        saownerfunds = lesactive.accountholds (
                            uofferownerid, book.out.currency,
                            book.out.account, fhzero_if_frozen);

                        if (saownerfunds < zero)
                        {
                            // treat negative funds as zero.

                            saownerfunds.clear ();
                        }
                    }
                }

                json::value jvoffer = sleoffer->getjson (0);

                stamount    satakergetsfunded;
                stamount    saownerfundslimit;
                std::uint32_t uofferrate;


                if (utransferrate != quality_one
                    // have a tranfer fee.
                    && utakerid != book.out.account
                    // not taking offers of own ious.
                    && book.out.account != uofferownerid)
                    // offer owner not issuing ownfunds
                {
                    // need to charge a transfer fee to offer owner.
                    uofferrate          = utransferrate;
                    saownerfundslimit   = divide (
                        saownerfunds, stamount (noissue(), uofferrate, -9));
                    // todo(tom): why -9?
                }
                else
                {
                    uofferrate          = quality_one;
                    saownerfundslimit   = saownerfunds;
                }

                if (saownerfundslimit >= satakergets)
                {
                    // sufficient funds no shenanigans.
                    satakergetsfunded   = satakergets;
                }
                else
                {
                    // only provide, if not fully funded.

                    satakergetsfunded   = saownerfundslimit;

                    satakergetsfunded.setjson (jvoffer[jss::taker_gets_funded]);
                    std::min (
                        satakerpays, multiply (
                            satakergetsfunded, sadirrate, satakerpays)).setjson
                            (jvoffer[jss::taker_pays_funded]);
                }

                stamount saownerpays = (quality_one == uofferrate)
                    ? satakergetsfunded
                    : std::min (
                        saownerfunds,
                        multiply (
                            satakergetsfunded,
                            stamount (noissue(),
                                      uofferrate, -9)));

                umbalance[uofferownerid]    = saownerfunds - saownerpays;

                // include all offers funded and unfunded
                json::value& jvof = jvoffers.append (jvoffer);
                jvof[jss::quality] = sadirrate.gettext ();

                if (firstowneroffer)
                    jvof[jss::owner_funds] = saownerfunds.gettext ();
            }
            else
            {
                m_journal.warning << "missing offer";
            }

            if (!lesactive.dirnext (
                    utipindex, sleofferdir, ubookentry, offerindex))
            {
                bdirectadvance  = true;
            }
            else
            {
                m_journal.trace << "getbookpage: offerindex=" << offerindex;
            }
        }
    }

    //  jvresult["marker"]  = json::value(json::arrayvalue);
    //  jvresult["nodes"]   = json::value(json::arrayvalue);
}


#else

// this is the new code that uses the book iterators
// it has temporarily been disabled

// fixme : support ilimit.
void networkopsimp::getbookpage (
    bool badmin,
    ledger::pointer lpledger,
    book const& book,
    account const& utakerid,
    bool const bproof,
    const unsigned int ilimit,
    json::value const& jvmarker,
    json::value& jvresult)
{
    auto& jvoffers = (jvresult[jss::offers] = json::value (json::arrayvalue));

    std::map<account, stamount> umbalance;

    ledgerentryset  lesactive (lpledger, tapnone, true);
    orderbookiterator obiterator (lesactive, book);

    auto utransferrate = rippletransferrate (lesactive, book.out.account);

    const bool bglobalfreeze = lesactive.isglobalfrozen (book.out.account) ||
                               lesactive.isglobalfrozen (book.in.account);

    unsigned int left (ilimit == 0 ? 300 : ilimit);
    if (! badmin && left > 300)
        left = 300;

    while (left-- > 0 && obiterator.nextoffer ())
    {

        sle::pointer    sleoffer        = obiterator.getcurrentoffer();
        if (sleoffer)
        {
            auto const uofferownerid = sleoffer->getfieldaccount160 (sfaccount);
            auto const& satakergets = sleoffer->getfieldamount (sftakergets);
            auto const& satakerpays = sleoffer->getfieldamount (sftakerpays);
            stamount sadirrate = obiterator.getcurrentrate ();
            stamount saownerfunds;

            if (book.out.account == uofferownerid)
            {
                // if offer is selling issuer's own ious, it is fully funded.
                saownerfunds    = satakergets;
            }
            else if (bglobalfreeze)
            {
                // if either asset is globally frozen, consider all offers
                // that aren't ours to be totally unfunded
                saownerfunds.clear (issueref (book.out.currency, book.out.account));
            }
            else
            {
                auto umbalanceentry = umbalance.find (uofferownerid);

                if (umbalanceentry != umbalance.end ())
                {
                    // found in running balance table.

                    saownerfunds    = umbalanceentry->second;
                }
                else
                {
                    // did not find balance in table.

                    saownerfunds = lesactive.accountholds (
                        uofferownerid, book.out.currency, book.out.account, fhzero_if_frozen);

                    if (saownerfunds.isnegative ())
                    {
                        // treat negative funds as zero.

                        saownerfunds.zero ();
                    }
                }
            }

            json::value jvoffer = sleoffer->getjson (0);

            stamount    satakergetsfunded;
            stamount    saownerfundslimit;
            std::uint32_t uofferrate;


            if (utransferrate != quality_one
                // have a tranfer fee.
                && utakerid != book.out.account
                // not taking offers of own ious.
                && book.out.account != uofferownerid)
                // offer owner not issuing ownfunds
            {
                // need to charge a transfer fee to offer owner.
                uofferrate = utransferrate;
                // todo(tom): where does -9 come from?!
                stamount amount (noissue(), uofferrate, -9);
                saownerfundslimit = divide (saownerfunds, amount);
            }
            else
            {
                uofferrate          = quality_one;
                saownerfundslimit   = saownerfunds;
            }

            if (saownerfundslimit >= satakergets)
            {
                // sufficient funds no shenanigans.
                satakergetsfunded   = satakergets;
            }
            else
            {
                // only provide, if not fully funded.
                satakergetsfunded   = saownerfundslimit;

                satakergetsfunded.setjson (jvoffer[jss::taker_gets_funded]);

                // tood(tom): the result of this expression is not used - what's
                // going on here?
                std::min (satakerpays, multiply (
                    satakergetsfunded, sadirrate, satakerpays)).setjson (
                        jvoffer[jss::taker_pays_funded]);
            }

            stamount saownerpays = (uofferrate == quality_one)
                    ? satakergetsfunded
                    : std::min (saownerfunds,
                                multiply (
                                    satakergetsfunded, stamount (
                                        noissue(), uofferrate,
                                        -9)));

            umbalance[uofferownerid]    = saownerfunds - saownerpays;

            if (!saownerfunds.iszero () || uofferownerid == utakerid)
            {
                // only provide funded offers and offers of the taker.
                json::value& jvof   = jvoffers.append (jvoffer);
                jvof[jss::quality]     = sadirrate.gettext ();
            }

        }
    }

    //  jvresult["marker"]  = json::value(json::arrayvalue);
    //  jvresult["nodes"]   = json::value(json::arrayvalue);
}

#endif

static void fpappender (
    protocol::tmgetobjectbyhash* reply, std::uint32_t ledgerseq,
    uint256 const& hash, const blob& blob)
{
    protocol::tmindexedobject& newobj = * (reply->add_objects ());
    newobj.set_ledgerseq (ledgerseq);
    newobj.set_hash (hash.begin (), 256 / 8);
    newobj.set_data (&blob[0], blob.size ());
}

void networkopsimp::makefetchpack (
    job&, std::weak_ptr<peer> wpeer,
    std::shared_ptr<protocol::tmgetobjectbyhash> request,
    uint256 haveledgerhash, std::uint32_t uuptime)
{
    if (uptimetimer::getinstance ().getelapsedseconds () > (uuptime + 1))
    {
        m_journal.info << "fetch pack request got stale";
        return;
    }

    if (getapp().getfeetrack ().isloadedlocal () ||
        (m_ledgermaster.getvalidatedledgerage() > 40))
    {
        m_journal.info << "too busy to make fetch pack";
        return;
    }

    peer::ptr peer = wpeer.lock ();

    if (!peer)
        return;

    ledger::pointer haveledger = getledgerbyhash (haveledgerhash);

    if (!haveledger)
    {
        m_journal.info
            << "peer requests fetch pack for ledger we don't have: "
            << haveledger;
        peer->charge (resource::feerequestnoreply);
        return;
    }

    if (!haveledger->isclosed ())
    {
        m_journal.warning
            << "peer requests fetch pack from open ledger: "
            << haveledger;
        peer->charge (resource::feeinvalidrequest);
        return;
    }

    if (haveledger->getledgerseq() < m_ledgermaster.getearliestfetch())
    {
        m_journal.debug << "peer requests fetch pack that is too early";
        peer->charge (resource::feeinvalidrequest);
        return;
    }

    ledger::pointer wantledger = getledgerbyhash (haveledger->getparenthash ());

    if (!wantledger)
    {
        m_journal.info
            << "peer requests fetch pack for ledger whose predecessor we "
            << "don't have: " << haveledger;
        peer->charge (resource::feerequestnoreply);
        return;
    }

    try
    {
        protocol::tmgetobjectbyhash reply;
        reply.set_query (false);

        if (request->has_seq ())
            reply.set_seq (request->seq ());

        reply.set_ledgerhash (request->ledgerhash ());
        reply.set_type (protocol::tmgetobjectbyhash::otfetch_pack);

        // building a fetch pack:
        //  1. add the header for the requested ledger.
        //  2. add the nodes for the accountstatemap of that ledger.
        //  3. if there are transactions, add the nodes for the
        //     transactions of the ledger.
        //  4. if the fetchpack now contains greater than or equal to
        //     256 entries then stop.
        //  5. if not very much time has elapsed, then loop back and repeat
        //     the same process adding the previous ledger to the fetchpack.
        do
        {
            std::uint32_t lseq = wantledger->getledgerseq ();

            protocol::tmindexedobject& newobj = *reply.add_objects ();
            newobj.set_hash (wantledger->gethash ().begin (), 256 / 8);
            serializer s (256);
            s.add32 (hashprefix::ledgermaster);
            wantledger->addraw (s);
            newobj.set_data (s.getdataptr (), s.getlength ());
            newobj.set_ledgerseq (lseq);

            wantledger->peekaccountstatemap ()->getfetchpack
                (haveledger->peekaccountstatemap ().get (), true, 1024,
                    std::bind (fpappender, &reply, lseq, std::placeholders::_1,
                               std::placeholders::_2));

            if (wantledger->gettranshash ().isnonzero ())
                wantledger->peektransactionmap ()->getfetchpack (
                    nullptr, true, 256,
                    std::bind (fpappender, &reply, lseq, std::placeholders::_1,
                               std::placeholders::_2));

            if (reply.objects ().size () >= 256)
                break;

            // move may save a ref/unref
            haveledger = std::move (wantledger);
            wantledger = getledgerbyhash (haveledger->getparenthash ());
        }
        while (wantledger &&
               uptimetimer::getinstance ().getelapsedseconds () <= uuptime + 1);

        m_journal.info
            << "built fetch pack with " << reply.objects ().size () << " nodes";
        auto msg = std::make_shared<message> (reply, protocol::mtget_objects);
        peer->send (msg);
    }
    catch (...)
    {
        m_journal.warning << "exception building fetch pach";
    }
}

void networkopsimp::sweepfetchpack ()
{
    mfetchpack.sweep ();
}

void networkopsimp::addfetchpack (
    uint256 const& hash, std::shared_ptr< blob >& data)
{
    mfetchpack.canonicalize (hash, data);
}

bool networkopsimp::getfetchpack (uint256 const& hash, blob& data)
{
    bool ret = mfetchpack.retrieve (hash, data);

    if (!ret)
        return false;

    mfetchpack.del (hash, false);

    if (hash != serializer::getsha512half (data))
    {
        m_journal.warning << "bad entry in fetch pack";
        return false;
    }

    return true;
}

bool networkopsimp::shouldfetchpack (std::uint32_t seq)
{
    if (mfetchseq == seq)
        return false;
    mfetchseq = seq;
    return true;
}

int networkopsimp::getfetchsize ()
{
    return mfetchpack.getcachesize ();
}

void networkopsimp::gotfetchpack (bool progress, std::uint32_t seq)
{

    // fixme: calling this function more than once will result in
    // inboundledgers::gotfetchpack being called more than once
    // which is expensive. a flag should track whether we've already dispatched

    m_job_queue.addjob (
        jtledger_data, "gotfetchpack",
        std::bind (&inboundledgers::gotfetchpack,
                   &getapp().getinboundledgers (), std::placeholders::_1));
}

void networkopsimp::missingnodeinledger (std::uint32_t seq)
{
    uint256 hash = getapp().getledgermaster ().gethashbyseq (seq);
    if (hash.iszero())
    {
        m_journal.warning
            << "missing a node in ledger " << seq << " cannot fetch";
    }
    else
    {
        m_journal.warning << "missing a node in ledger " << seq << " fetching";
        getapp().getinboundledgers ().findcreate (
            hash, seq, inboundledger::fcgeneric);
    }
}

//------------------------------------------------------------------------------

networkops::networkops (stoppable& parent)
    : infosub::source ("networkops", parent)
{
}

networkops::~networkops ()
{
}

//------------------------------------------------------------------------------

std::unique_ptr<networkops>
make_networkops (networkops::clock_type& clock, bool standalone,
    std::size_t network_quorum, jobqueue& job_queue, ledgermaster& ledgermaster,
    beast::stoppable& parent, beast::journal journal)
{
    return std::make_unique<networkopsimp> (clock, standalone, network_quorum,
        job_queue, ledgermaster, parent, journal);
}

} // ripple
