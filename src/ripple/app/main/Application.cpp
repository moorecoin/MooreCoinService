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
#include <ripple/app/main/application.h>
#include <ripple/app/data/databasecon.h>
#ifdef use_mysql
#include <ripple/app/data/mysqldatabase.h>
#endif
#include <ripple/app/data/nulldatabase.h>
#include <ripple/app/data/dbinit.h>
#include <ripple/app/impl/basicapp.h>
#include <ripple/app/main/tuning.h>
#include <ripple/app/ledger/acceptedledger.h>
#include <ripple/app/ledger/inboundledgers.h>
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/ledger/orderbookdb.h>
#include <ripple/app/main/collectormanager.h>
#include <ripple/app/main/loadmanager.h>
#include <ripple/app/main/localcredentials.h>
#include <ripple/app/main/nodestorescheduler.h>
#include <ripple/app/misc/amendmenttable.h>
#include <ripple/app/misc/ihashrouter.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/app/misc/shamapstore.h>
#include <ripple/app/misc/validations.h>
#include <ripple/app/paths/findpaths.h>
#include <ripple/app/paths/pathrequests.h>
#include <ripple/app/peers/uniquenodelist.h>
#include <ripple/app/tx/transactionmaster.h>
#include <ripple/app/websocket/wsdoor.h>
#include <ripple/basics/log.h>
#include <ripple/basics/loggedtimings.h>
#include <ripple/basics/resolverasio.h>
#include <ripple/basics/sustain.h>
#include <ripple/basics/seconds_clock.h>
#include <ripple/basics/make_sslcontext.h>
#include <ripple/json/json_reader.h>
#include <ripple/json/to_string.h>
#include <ripple/core/loadfeetrack.h>
#include <ripple/net/sntpclient.h>
#include <ripple/nodestore/database.h>
#include <ripple/nodestore/dummyscheduler.h>
#include <ripple/nodestore/manager.h>
#include <ripple/overlay/make_overlay.h>
#include <ripple/protocol/stparsedjson.h>
#include <ripple/rpc/manager.h>
#include <ripple/server/make_serverhandler.h>
#include <ripple/validators/make_manager.h>
#include <ripple/unity/git_id.h>
#include <beast/asio/io_latency_probe.h>
#include <beast/module/core/thread/deadlinetimer.h>
#include <boost/asio/signal_set.hpp>
#include <fstream>

namespace ripple {

// 204/256 about 80%
static int const majority_fraction (204);

// this hack lets the s_instance variable remain set during
// the call to ~application
class applicationimpbase : public application
{
public:
    applicationimpbase ()
    {
        assert (s_instance == nullptr);
        s_instance = this;
    }

    ~applicationimpbase ()
    {
        s_instance = nullptr;
    }

    static application* s_instance;

    static application& getinstance ()
    {
        bassert (s_instance != nullptr);
        return *s_instance;
    }
};

application* applicationimpbase::s_instance;

//------------------------------------------------------------------------------

// vfalco todo move the function definitions into the class declaration
class applicationimp
    : public applicationimpbase
    , public beast::rootstoppable
    , public beast::deadlinetimer::listener
    , public beast::leakchecked <applicationimp>
    , public basicapp
{
private:
    class io_latency_sampler
    {
    private:
        std::mutex mutable m_mutex;
        beast::insight::event m_event;
        beast::journal m_journal;
        beast::io_latency_probe <std::chrono::steady_clock> m_probe;
        std::chrono::milliseconds m_lastsample;

    public:
        io_latency_sampler (
            beast::insight::event ev,
            beast::journal journal,
            std::chrono::milliseconds interval,
            boost::asio::io_service& ios)
            : m_event (ev)
            , m_journal (journal)
            , m_probe (interval, ios)
        {
        }

        void
        start()
        {
            m_probe.sample (std::ref(*this));
        }

        template <class duration>
        void operator() (duration const& elapsed)
        {
            auto const ms (ceil <std::chrono::milliseconds> (elapsed));

            {
                std::unique_lock <std::mutex> lock (m_mutex);
                m_lastsample = ms;
            }

            if (ms.count() >= 10)
                m_event.notify (ms);
            if (ms.count() >= 500)
                m_journal.warning <<
                    "io_service latency = " << ms;
        }

        std::chrono::milliseconds
        get () const
        {
            std::unique_lock <std::mutex> lock (m_mutex);
            return m_lastsample;
        }

        void
        cancel ()
        {
            m_probe.cancel ();
        }

        void cancel_async ()
        {
            m_probe.cancel_async ();
        }
    };

public:
    logs& m_logs;
    beast::journal m_journal;
    application::locktype m_mastermutex;

    nodestorescheduler m_nodestorescheduler;
    std::unique_ptr <shamapstore> m_shamapstore;
    std::unique_ptr <nodestore::database> m_nodestore;

    // these are not stoppable-derived
    nodecache m_tempnodecache;
    treenodecache m_treenodecache;
    slecache m_slecache;
    localcredentials m_localcredentials;
    transactionmaster m_txmaster;

    std::unique_ptr <collectormanager> m_collectormanager;
    std::unique_ptr <resource::manager> m_resourcemanager;
    std::unique_ptr <fullbelowcache> m_fullbelowcache;

    // these are stoppable-related
    std::unique_ptr <jobqueue> m_jobqueue;
    std::unique_ptr <rpc::manager> m_rpcmanager;
    // vfalco todo make orderbookdb abstract
    orderbookdb m_orderbookdb;
    std::unique_ptr <pathrequests> m_pathrequests;
    std::unique_ptr <ledgermaster> m_ledgermaster;
    std::unique_ptr <inboundledgers> m_inboundledgers;
    std::unique_ptr <networkops> m_networkops;
    std::unique_ptr <uniquenodelist> m_deprecatedunl;
    std::unique_ptr <serverhandler> serverhandler_;
    std::unique_ptr <sntpclient> m_sntpclient;
    std::unique_ptr <validators::manager> m_validators;
    std::unique_ptr <amendmenttable> m_amendmenttable;
    std::unique_ptr <loadfeetrack> mfeetrack;
    std::unique_ptr <ihashrouter> mhashrouter;
    std::unique_ptr <validations> mvalidations;
    std::unique_ptr <loadmanager> m_loadmanager;
    beast::deadlinetimer m_sweeptimer;

    std::unique_ptr <databasecon> mrpcdb;
    std::unique_ptr <databasecon> mtxndb;
    std::unique_ptr <databasecon> mledgerdb;
    std::unique_ptr <databasecon> mwalletdb;
    std::unique_ptr <overlay> m_overlay;
    std::vector <std::unique_ptr<wsdoor>> wsdoors_;

    boost::asio::signal_set m_signals;
    beast::waitableevent m_stop;

    std::unique_ptr <resolverasio> m_resolver;

    io_latency_sampler m_io_latency_sampler;

    //--------------------------------------------------------------------------

    static
    std::size_t numberofthreads()
    {
    #if ripple_single_io_service_thread
        return 1;
    #else
        return (getconfig().node_size >= 2) ? 2 : 1;
    #endif
    }

    //--------------------------------------------------------------------------

    applicationimp (logs& logs)
        : rootstoppable ("application")
        , basicapp (numberofthreads())
        , m_logs (logs)

        , m_journal (m_logs.journal("application"))

        , m_nodestorescheduler (*this)

        , m_shamapstore (make_shamapstore (setup_shamapstore (
                getconfig()), *this, m_nodestorescheduler,
                m_logs.journal ("shamapstore"), m_logs.journal ("nodeobject"),
                    m_txmaster))

        , m_nodestore (m_shamapstore->makedatabase ("nodestore.main", 4))

        , m_tempnodecache ("nodecache", 16384, 90, get_seconds_clock (),
            m_logs.journal("taggedcache"))

        , m_treenodecache ("treenodecache", 65536, 60, get_seconds_clock (),
            deprecatedlogs().journal("taggedcache"))

        , m_slecache ("ledgerentrycache", 4096, 120, get_seconds_clock (),
            m_logs.journal("taggedcache"))

        , m_collectormanager (collectormanager::new (
            getconfig().insightsettings, m_logs.journal("collector")))

        , m_resourcemanager (resource::make_manager (
            m_collectormanager->collector(), m_logs.journal("resource")))

        , m_fullbelowcache (std::make_unique <fullbelowcache> (
            "full_below", get_seconds_clock (), m_collectormanager->collector (),
                fullbelowtargetsize, fullbelowexpirationseconds))

        // the jobqueue has to come pretty early since
        // almost everything is a stoppable child of the jobqueue.
        //
        , m_jobqueue (make_jobqueue (m_collectormanager->group ("jobq"),
            m_nodestorescheduler, m_logs.journal("jobqueue")))

        //
        // anything which calls addjob must be a descendant of the jobqueue
        //

        , m_rpcmanager (rpc::make_manager (m_logs.journal("rpcmanager")))

        , m_orderbookdb (*m_jobqueue)

        , m_pathrequests (new pathrequests (
            m_logs.journal("pathrequest"), m_collectormanager->collector ()))

        , m_ledgermaster (make_ledgermaster (getconfig (), *m_jobqueue,
            m_collectormanager->collector (), m_logs.journal("ledgermaster")))

        // vfalco note must come before networkops to prevent a crash due
        //             to dependencies in the destructor.
        //
        , m_inboundledgers (make_inboundledgers (get_seconds_clock (),
            *m_jobqueue, m_collectormanager->collector ()))

        , m_networkops (make_networkops (get_seconds_clock (),
            getconfig ().run_standalone, getconfig ().network_quorum,
            *m_jobqueue, *m_ledgermaster, *m_jobqueue,
            m_logs.journal("networkops")))

        // vfalco note localcredentials starts the deprecated unl service
        , m_deprecatedunl (make_uniquenodelist (*m_jobqueue))

        , serverhandler_ (make_serverhandler (*m_networkops,
            get_io_service(), *m_jobqueue, *m_networkops,
                *m_resourcemanager))

        , m_sntpclient (sntpclient::new (*this))

        , m_validators (validators::make_manager(*this, get_io_service(),
            getconfig ().getmoduledatabasepath (), m_logs.journal("uvl")))

        , m_amendmenttable (make_amendmenttable (weeks(2), majority_fraction,
            m_logs.journal("amendmenttable")))

        , mfeetrack (loadfeetrack::new (m_logs.journal("loadmanager")))

        , mhashrouter (ihashrouter::new (ihashrouter::getdefaultholdtime ()))

        , mvalidations (make_validations ())

        , m_loadmanager (make_loadmanager (*this, m_logs.journal("loadmanager")))

        , m_sweeptimer (this)

        , m_signals(get_io_service(), sigint)

        , m_resolver (resolverasio::new (get_io_service(), m_logs.journal("resolver")))

        , m_io_latency_sampler (m_collectormanager->collector()->make_event ("ios_latency"),
            m_logs.journal("application"), std::chrono::milliseconds (100), get_io_service())
    {
        add (m_resourcemanager.get ());

        //
        // vfalco - read this!
        //
        //  do not start threads, open sockets, or do any sort of "real work"
        //  inside the constructor. put it in onstart instead. or if you must,
        //  put it in setup (but everything in setup should be moved to onstart
        //  anyway.
        //
        //  the reason is that the unit tests require the application object to
        //  be created (since so much code calls getapp()). but we don't actually
        //  start all the threads, sockets, and services when running the unit
        //  tests. therefore anything which needs to be stopped will not get
        //  stopped correctly if it is started in this constructor.
        //

        // vfalco hack
        m_nodestorescheduler.setjobqueue (*m_jobqueue);

        add (*m_validators);
        add (m_ledgermaster->getpropertysource ());
        add (*serverhandler_);
    }

    //--------------------------------------------------------------------------

    collectormanager& getcollectormanager ()
    {
        return *m_collectormanager;
    }

    fullbelowcache& getfullbelowcache ()
    {
        return *m_fullbelowcache;
    }

    jobqueue& getjobqueue ()
    {
        return *m_jobqueue;
    }

    rpc::manager& getrpcmanager ()
    {
        return *m_rpcmanager;
    }

    localcredentials& getlocalcredentials ()
    {
        return m_localcredentials ;
    }

    networkops& getops ()
    {
        return *m_networkops;
    }

    boost::asio::io_service& getioservice ()
    {
        return get_io_service();
    }

    std::chrono::milliseconds getiolatency ()
    {
        std::unique_lock <std::mutex> m_iolatencylock;

        return m_io_latency_sampler.get ();
    }

    ledgermaster& getledgermaster ()
    {
        return *m_ledgermaster;
    }

    inboundledgers& getinboundledgers ()
    {
        return *m_inboundledgers;
    }

    transactionmaster& getmastertransaction ()
    {
        return m_txmaster;
    }

    nodecache& gettempnodecache ()
    {
        return m_tempnodecache;
    }

    treenodecache&  gettreenodecache ()
    {
        return m_treenodecache;
    }

    nodestore::database& getnodestore ()
    {
        return *m_nodestore;
    }

    application::locktype& getmasterlock ()
    {
        return m_mastermutex;
    }

    loadmanager& getloadmanager ()
    {
        return *m_loadmanager;
    }

    resource::manager& getresourcemanager ()
    {
        return *m_resourcemanager;
    }

    orderbookdb& getorderbookdb ()
    {
        return m_orderbookdb;
    }

    pathrequests& getpathrequests ()
    {
        return *m_pathrequests;
    }

    slecache& getslecache ()
    {
        return m_slecache;
    }

    validators::manager& getvalidators ()
    {
        return *m_validators;
    }

    amendmenttable& getamendmenttable()
    {
        return *m_amendmenttable;
    }

    loadfeetrack& getfeetrack ()
    {
        return *mfeetrack;
    }

    ihashrouter& gethashrouter ()
    {
        return *mhashrouter;
    }

    validations& getvalidations ()
    {
        return *mvalidations;
    }

    uniquenodelist& getunl ()
    {
        return *m_deprecatedunl;
    }

    shamapstore& getshamapstore () override
    {
        return *m_shamapstore;
    }

    overlay& overlay ()
    {
        return *m_overlay;
    }

    // vfalco todo move these to the .cpp
    bool running ()
    {
        return mtxndb != nullptr;
    }
    bool getsystemtimeoffset (int& offset)
    {
        return m_sntpclient->getoffset (offset);
    }

    databasecon& getrpcdb ()
    {
        assert (mrpcdb.get() != nullptr);
        return *mrpcdb;
    }
    databasecon& gettxndb ()
    {
        assert (mtxndb.get() != nullptr);
        return *mtxndb;
    }
    databasecon& getledgerdb ()
    {
        assert (mledgerdb.get() != nullptr);
        return *mledgerdb;
    }
    databasecon& getwalletdb ()
    {
        assert (mwalletdb.get() != nullptr);
        return *mwalletdb;
    }

    bool isshutdown ()
    {
        // from stoppable mixin
        return isstopped();
    }

    //--------------------------------------------------------------------------
    bool initsqlitedbs ()
    {
        assert (mrpcdb.get () == nullptr);
        assert (mtxndb.get () == nullptr);
        assert (mledgerdb.get () == nullptr);
        assert (mwalletdb.get () == nullptr);

        databasecon::setup setup = setup_databasecon (getconfig());
        mrpcdb = std::make_unique <databasecon> (setup, "rpc.db", rpcdbinit,
                rpcdbcount);
        if (getconfig().transactiondatabase[beast::string("type")] == beast::string::empty)
            mtxndb = std::make_unique <databasecon> (setup, "transaction.db",
                txndbinit, txndbcount);
        else if (getconfig().transactiondatabase[beast::string("type")] == beast::string("mysql"))
        {
#ifdef  use_mysql
            mtxndb = std::make_unique <mysqldatabasecon> (getconfig().transactiondatabase, txndbinitmysql, txndbcountmysql);
#else   // use_mysql
            m_journal.fatal << "mysql type used but not compiled in!";
            return false;
#endif  // use_mysql
        }
        else if (getconfig().transactiondatabase[beast::string("type")] == beast::string("none")) {
            mtxndb = std::make_unique <nulldatabasecon> ();
        }
        mledgerdb = std::make_unique <databasecon> (setup, "ledger.db",
                ledgerdbinit, ledgerdbcount);
        mwalletdb = std::make_unique <databasecon> (setup, "wallet.db",
                walletdbinit, walletdbcount);

        if (setup.onlinedelete && mtxndb && mledgerdb)
        {
            if (mtxndb->getdb()->getdbtype() == database::type::sqlite)
            {
                std::lock_guard <std::recursive_mutex> lock (
                        mtxndb->peekmutex());
                mtxndb->getdb()->executesql ("vacuum;");
            }
            {
                std::lock_guard <std::recursive_mutex> lock (
                        mledgerdb->peekmutex());
                mledgerdb->getdb()->executesql ("vacuum;");
            }
        }

        return
            mrpcdb.get() != nullptr &&
            mtxndb.get () != nullptr &&
            mledgerdb.get () != nullptr &&
            mwalletdb.get () != nullptr;
    }

    void signalled(const boost::system::error_code& ec, int signal_number)
    {
        if (ec == boost::asio::error::operation_aborted)
        {
            // indicates the signal handler has been aborted
            // do nothing
        }
        else if (ec)
        {
            m_journal.error << "received signal: " << signal_number
                            << " with error: " << ec.message();
        }
        else
        {
            m_journal.debug << "received signal: " << signal_number;
            signalstop();
        }
    }

    // vfalco todo break this function up into many small initialization segments.
    //             or better yet refactor these initializations into raii classes
    //             which are members of the application object.
    //
    void setup ()
    {
        // vfalco note: 0 means use heuristics to determine the thread count.
        m_jobqueue->setthreadcount (0, getconfig ().run_standalone);

        m_signals.async_wait(std::bind(&applicationimp::signalled, this,
                                      std::placeholders::_1,
                                      std::placeholders::_2));

        assert (mtxndb == nullptr);

        auto debug_log = getconfig ().getdebuglogfile ();

        if (!debug_log.empty ())
        {
            // let debug messages go to the file but only warning or higher to
            // regular output (unless verbose)

            if (!m_logs.open(debug_log))
                std::cerr << "can't open log file " << debug_log << '\n';

            if (m_logs.severity() > beast::journal::kdebug)
                m_logs.severity (beast::journal::kdebug);
        }

        if (!getconfig ().run_standalone)
            m_sntpclient->init (getconfig ().sntp_servers);

        if (!initsqlitedbs ())
        {
            m_journal.fatal << "can not create database connections!";
            exitwithcode(3);
        }

        getapp().getledgerdb ().getdb ()->executesql (boost::str (boost::format ("pragma cache_size=-%d;") %
                (getconfig ().getsize (silgrdbcache) * 1024)));
        if (getapp().gettxndb ().getdb ()->getdbtype() == database::type::sqlite)
        {
            getapp().gettxndb ().getdb ()->executesql (boost::str (boost::format ("pragma cache_size=-%d;") %
                                                (getconfig ().getsize (sitxndbcache) * 1024)));
        }

        mtxndb->getdb ()->setupcheckpointing (m_jobqueue.get());
        mledgerdb->getdb ()->setupcheckpointing (m_jobqueue.get());

        if (!getconfig ().run_standalone)
            updatetables ();

        m_amendmenttable->addinitial();
        initializepathfinding ();

        m_ledgermaster->setminvalidations (getconfig ().validation_quorum);

        auto const startup = getconfig ().start_up;
        if (startup == config::fresh)
        {
            m_journal.info << "starting new ledger";

            startnewledger ();
        }
        else if (startup == config::load ||
                 startup == config::load_file ||
                 startup == config::replay)
        {
            m_journal.info << "loading specified ledger";

            if (!loadoldledger (getconfig ().start_ledger,
                                startup == config::replay,
                                startup == config::load_file))
            {
                exitwithcode(-1);
            }
        }
        else if (startup == config::network)
        {
            // this should probably become the default once we have a stable network.
            if (!getconfig ().run_standalone)
                m_networkops->neednetworkledger ();

            startnewledger ();
        }
        else
            startnewledger ();

        m_orderbookdb.setup (getapp().getledgermaster ().getcurrentledger ());

        // begin validation and ip maintenance.
        //
        // - localcredentials maintains local information: including identity
        // - and network connection persistence information.
        //
        // vfalco note this starts the unl
        m_localcredentials.start ();

        //
        // set up unl.
        //
        if (!getconfig ().run_standalone)
            getunl ().nodebootstrap ();

        mvalidations->tune (getconfig ().getsize (sivalidationssize), getconfig ().getsize (sivalidationsage));
        m_nodestore->tune (getconfig ().getsize (sinodecachesize), getconfig ().getsize (sinodecacheage));
        m_ledgermaster->tune (getconfig ().getsize (siledgersize), getconfig ().getsize (siledgerage));
        m_slecache.settargetsize (getconfig ().getsize (sislecachesize));
        m_slecache.settargetage (getconfig ().getsize (sislecacheage));
        m_treenodecache.settargetsize (getconfig ().getsize (sitreecachesize));
        m_treenodecache.settargetage (getconfig ().getsize (sitreecacheage));

        //----------------------------------------------------------------------
        //
        // server
        //
        //----------------------------------------------------------------------

        // vfalco note unfortunately, in stand-alone mode some code still
        //             foolishly calls overlay(). when this is fixed we can
        //             move the instantiation inside a conditional:
        //
        //             if (!getconfig ().run_standalone)
        m_overlay = make_overlay (setup_overlay(getconfig()), *m_jobqueue,
            *serverhandler_, *m_resourcemanager,
                getconfig ().getmoduledatabasepath (), *m_resolver,
                    get_io_service());
        add (*m_overlay); // add to propertystream

        {
            auto setup = setup_serverhandler(getconfig(), std::cerr);
            setup.makecontexts();
            serverhandler_->setup (setup, m_journal);
        }

        // create websocket doors
        for (auto const& port : serverhandler_->setup().ports)
        {
            if (! port.websockets())
                continue;
            auto door = make_wsdoor(port, *m_resourcemanager, getops());
            if (door == nullptr)
            {
                m_journal.fatal << "could not create websocket for [" <<
                    port.name << "]";
                throw std::exception();
            }
            wsdoors_.emplace_back(std::move(door));
        }

        //----------------------------------------------------------------------

        // begin connecting to network.
        if (!getconfig ().run_standalone)
        {
            // should this message be here, conceptually? in theory this sort
            // of message, if displayed, should be displayed from peerfinder.
            if (getconfig ().peer_private && getconfig ().ips.empty ())
                m_journal.warning << "no outbound peer connections will be made";

            // vfalco note the state timer resets the deadlock detector.
            //
            m_networkops->setstatetimer ();
        }
        else
        {
            m_journal.warning << "running in standalone mode";

            m_networkops->setstandalone ();
        }
    }

    //--------------------------------------------------------------------------
    //
    // stoppable
    //

    void onprepare() override
    {
    }

    void onstart ()
    {
        m_journal.info << "application starting. build is " << gitcommitid();

        m_sweeptimer.setexpiration (10);

        m_io_latency_sampler.start();

        m_resolver->start ();
    }

    // called to indicate shutdown.
    void onstop ()
    {
        m_journal.debug << "application stopping";

        m_io_latency_sampler.cancel_async ();

        // vfalco enormous hack, we have to force the probe to cancel
        //        before we stop the io_service queue or else it never
        //        unblocks in its destructor. the fix is to make all
        //        io_objects gracefully handle exit so that we can
        //        naturally return from io_service::run() instead of
        //        forcing a call to io_service::stop()
        m_io_latency_sampler.cancel ();

        m_resolver->stop_async ();

        // nikb this is a hack - we need to wait for the resolver to
        //      stop. before we stop the io_server_queue or weird
        //      things will happen.
        m_resolver->stop ();

        m_sweeptimer.cancel ();

        mvalidations->flush ();

        rippleaddress::clearcache ();
        stopped ();
    }

    //------------------------------------------------------------------------------
    //
    // propertystream
    //

    void onwrite (beast::propertystream::map& stream)
    {
    }

    //------------------------------------------------------------------------------

    void run ()
    {
        // vfalco note i put this here in the hopes that when unit tests run (which
        //             tragically require an application object to exist or else they
        //             crash), the run() function will not get called and we will
        //             avoid doing silly things like contacting the sntp server, or
        //             running the various logic threads like validators, peerfinder, etc.
        prepare ();
        start ();


        {
            if (!getconfig ().run_standalone)
            {
                // vfalco note this seems unnecessary. if we properly refactor the load
                //             manager then the deadlock detector can just always be "armed"
                //
                getapp().getloadmanager ().activatedeadlockdetector ();
            }
        }

        m_stop.wait ();

        // stop the server. when this returns, all
        // stoppable objects should be stopped.
        m_journal.info << "received shutdown request";
        stop (m_journal);
        m_journal.info << "done.";
        stopsustain();
    }

    void exitwithcode(int code)
    {
        stopsustain();
        // vfalco this breaks invariants: automatic objects
        //        will not have destructors called.
        std::exit(code);
    }

    void signalstop ()
    {
        // unblock the main thread (which is sitting in run()).
        //
        m_stop.signal();
    }

    void ondeadlinetimer (beast::deadlinetimer& timer)
    {
        if (timer == m_sweeptimer)
        {
            // vfalco todo move all this into dosweep

            boost::filesystem::space_info space = boost::filesystem::space (getconfig ().data_dir);

            // vfalco todo give this magic constant a name and move it into a well documented header
            //
            if (space.available < (512 * 1024 * 1024))
            {
                m_journal.fatal << "remaining free disk space is less than 512mb";
                getapp().signalstop ();
            }

            m_jobqueue->addjob(jtsweep, "sweep",
                std::bind(&applicationimp::dosweep, this,
                          std::placeholders::_1));
        }
    }

    void dosweep (job& j)
    {
        // vfalco note does the order of calls matter?
        // vfalco todo fix the dependency inversion using an observer,
        //         have listeners register for "onsweep ()" notification.
        //

        m_fullbelowcache->sweep ();

        logtimedcall (m_journal.warning, "transactionmaster::sweep", __file__, __line__, std::bind (
            &transactionmaster::sweep, &m_txmaster));

        logtimedcall (m_journal.warning, "nodestore::sweep", __file__, __line__, std::bind (
            &nodestore::database::sweep, m_nodestore.get()));

        logtimedcall (m_journal.warning, "ledgermaster::sweep", __file__, __line__, std::bind (
            &ledgermaster::sweep, m_ledgermaster.get()));

        logtimedcall (m_journal.warning, "tempnodecache::sweep", __file__, __line__, std::bind (
            &nodecache::sweep, &m_tempnodecache));

        logtimedcall (m_journal.warning, "validations::sweep", __file__, __line__, std::bind (
            &validations::sweep, mvalidations.get ()));

        logtimedcall (m_journal.warning, "inboundledgers::sweep", __file__, __line__, std::bind (
            &inboundledgers::sweep, &getinboundledgers ()));

        logtimedcall (m_journal.warning, "slecache::sweep", __file__, __line__, std::bind (
            &slecache::sweep, &m_slecache));

        logtimedcall (m_journal.warning, "acceptedledger::sweep", __file__, __line__,
            &acceptedledger::sweep);

        logtimedcall (m_journal.warning, "shamap::sweep", __file__, __line__,std::bind (
            &treenodecache::sweep, &m_treenodecache));

        logtimedcall (m_journal.warning, "networkops::sweepfetchpack", __file__, __line__, std::bind (
            &networkops::sweepfetchpack, m_networkops.get ()));

        // vfalco note does the call to sweep() happen on another thread?
        m_sweeptimer.setexpiration (getconfig ().getsize (sisweepinterval));
    }


private:
    void updatetables ();
    void startnewledger ();
    bool loadoldledger (
        std::string const& ledgerid, bool replay, bool isfilename);

    void onannounceaddress ();
};

//------------------------------------------------------------------------------

void applicationimp::startnewledger ()
{
    // new stuff.
    rippleaddress   rootseedmaster      = rippleaddress::createseedgeneric ("masterpassphrase");
    rippleaddress   rootgeneratormaster = rippleaddress::creategeneratorpublic (rootseedmaster);
    rippleaddress   rootaddress         = rippleaddress::createaccountpublic (rootgeneratormaster, 0);

    // print enough information to be able to claim root account.
    m_journal.info << "root master seed: " << rootseedmaster.humanseed ();
    m_journal.info << "root account: " << rootaddress.humanaccountid ();

    {
		ledger::pointer firstledger = std::make_shared<ledger>(rootaddress, system_currency_start, system_currency_start_vbc);
		//ledger::pointer firstledger = std::make_shared<ledger>(rootaddress, system_currency_start);
        assert (firstledger->getaccountstate (rootaddress));
        // todo(david): add any default amendments
        // todo(david): set default fee/reserve
        firstledger->updatehash ();
        firstledger->setclosed ();
        firstledger->setaccepted ();
        m_ledgermaster->pushledger (firstledger);

        ledger::pointer secondledger = std::make_shared<ledger> (true, std::ref (*firstledger));
        secondledger->setclosed ();
        secondledger->setaccepted ();
        m_ledgermaster->pushledger (secondledger, std::make_shared<ledger> (true, std::ref (*secondledger)));
        assert (secondledger->getaccountstate (rootaddress));
        m_networkops->setlastclosetime (secondledger->getclosetimenc ());
    }
}

bool applicationimp::loadoldledger (
    std::string const& ledgerid, bool replay, bool isfilename)
{
    try
    {
        ledger::pointer loadledger, replayledger;

        if (isfilename)
        {
            std::ifstream ledgerfile (ledgerid.c_str (), std::ios::in);
            if (!ledgerfile)
            {
                m_journal.fatal << "unable to open file";
            }
            else
            {
                 json::reader reader;
                 json::value jledger;
                 if (!reader.parse (ledgerfile, jledger, false))
                     m_journal.fatal << "unable to parse ledger json";
                 else
                 {
                     std::reference_wrapper<json::value> ledger (jledger);

                     // accept a wrapped ledger
                     if (ledger.get().ismember  ("result"))
                         ledger = ledger.get()["result"];
                     if (ledger.get().ismember ("ledger"))
                         ledger = ledger.get()["ledger"];


                     std::uint32_t seq = 1;
                     std::uint32_t closetime = getapp().getops().getclosetimenc ();
                     std::uint32_t closetimeresolution = 30;
                     bool closetimeestimated = false;
                     std::uint64_t totalcoins = 0;
					 std::uint64_t totalcoinsvbc = 0;

                     if (ledger.get().ismember ("accountstate"))
                     {
                          if (ledger.get().ismember ("ledger_index"))
                          {
                              seq = ledger.get()["ledger_index"].asuint();
                          }
                          if (ledger.get().ismember ("close_time"))
                          {
                              closetime = ledger.get()["close_time"].asuint();
                          }
                          if (ledger.get().ismember ("close_time_resolution"))
                          {
                              closetimeresolution =
                                  ledger.get()["close_time_resolution"].asuint();
                          }
                          if (ledger.get().ismember ("close_time_estimated"))
                          {
                              closetimeestimated =
                                  ledger.get()["close_time_estimated"].asbool();
                          }
                          if (ledger.get().ismember ("total_coins"))
                          {
                              totalcoins =
                                beast::lexicalcastthrow<std::uint64_t>
                                    (ledger.get()["total_coins"].asstring());
                          }
						  if (ledger.get().ismember("total_coinsvbc"))
						  {
							  totalcoinsvbc =
								  beast::lexicalcastthrow<std::uint64_t>
								  (ledger.get()["total_coinsvbc"].asstring());
						  }
                         ledger = ledger.get()["accountstate"];
                     }
                     if (!ledger.get().isarray ())
                     {
                         m_journal.fatal << "state nodes must be an array";
                     }
                     else
                     {
                         loadledger = std::make_shared<ledger> (seq, closetime);
                         loadledger->settotalcoins(totalcoins);
						 loadledger->settotalcoinsvbc(totalcoinsvbc);

                         for (json::uint index = 0; index < ledger.get().size(); ++index)
                         {
                             json::value& entry = ledger.get()[index];

                             uint256 uindex;
                             uindex.sethex (entry["index"].asstring());
                             entry.removemember ("index");

                             stparsedjsonobject stp ("sle", ledger.get()[index]);
                             // m_journal.info << "json: " << stp.object->getjson(0);

                             if (stp.object && (uindex.isnonzero()))
                             {
                                 stledgerentry sle (*stp.object, uindex);
                                 bool ok = loadledger->addsle (sle);
                                 if (!ok)
                                     m_journal.warning << "couldn't add serialized ledger: " << uindex;
                             }
                             else
                             {
                                 m_journal.warning << "invalid entry in ledger";
                             }
                         }

                         loadledger->setclosed ();
                         loadledger->setaccepted (closetime,
                             closetimeresolution, ! closetimeestimated);
                     }
                 }
            }
        }
        else if (ledgerid.empty () || (ledgerid == "latest"))
            loadledger = ledger::getlastfullledger ();
        else if (ledgerid.length () == 64)
        {
            // by hash
            uint256 hash;
            hash.sethex (ledgerid);
            loadledger = ledger::loadbyhash (hash);

            if (!loadledger)
            {
                // try to build the ledger from the back end
                auto il = std::make_shared <inboundledger> (hash, 0, inboundledger::fcgeneric,
                    get_seconds_clock ());
                if (il->checklocal ())
                    loadledger = il->getledger ();
            }

        }
        else // assume by sequence
            loadledger = ledger::loadbyindex (
                beast::lexicalcastthrow <std::uint32_t> (ledgerid));

        if (!loadledger)
        {
            m_journal.fatal << "no ledger found from ledgerid="
                            << ledgerid << std::endl;
            return false;
        }

        if (replay)
        {
            // replay a ledger close with same prior ledger and transactions

            // this ledger holds the transactions we want to replay
            replayledger = loadledger;

            // this is the prior ledger
            loadledger = ledger::loadbyhash (replayledger->getparenthash ());
            if (!loadledger)
            {

                // try to build the ledger from the back end
                auto il = std::make_shared <inboundledger> (
                    replayledger->getparenthash(), 0, inboundledger::fcgeneric,
                    get_seconds_clock ());
                if (il->checklocal ())
                    loadledger = il->getledger ();

                if (!loadledger)
                {
                    m_journal.fatal << "replay ledger missing/damaged";
                    assert (false);
                    return false;
                }
            }
        }

        loadledger->setclosed ();

        m_journal.info << "loading ledger " << loadledger->gethash () << " seq:" << loadledger->getledgerseq ();

        if (loadledger->getaccounthash ().iszero ())
        {
            m_journal.fatal << "ledger is empty.";
            assert (false);
            return false;
        }

        if (!loadledger->walkledger ())
        {
            m_journal.fatal << "ledger is missing nodes.";
            assert(false);
            return false;
        }

        if (!loadledger->assertsane ())
        {
            m_journal.fatal << "ledger is not sane.";
            assert(false);
            return false;
        }

        m_ledgermaster->setledgerrangepresent (loadledger->getledgerseq (), loadledger->getledgerseq ());

        ledger::pointer openledger = std::make_shared<ledger> (false, std::ref (*loadledger));
        m_ledgermaster->switchledgers (loadledger, openledger);
        m_ledgermaster->forcevalid(loadledger);
        m_networkops->setlastclosetime (loadledger->getclosetimenc ());

        if (replay)
        {
            // inject transaction(s) from the replayledger into our open ledger
            shamap::ref txns = replayledger->peektransactionmap();

            // get a mutable snapshot of the open ledger
            ledger::pointer cur = getledgermaster().getcurrentledger();
            cur = std::make_shared <ledger> (*cur, true);
            assert (!cur->isimmutable());

            for (auto it = txns->peekfirstitem(); it != nullptr;
                 it = txns->peeknextitem(it->gettag()))
            {
                transaction::pointer txn = replayledger->gettransaction(it->gettag());
                m_journal.info << txn->getjson(0);
                serializer s;
                txn->getstransaction()->add(s);
                if (!cur->addtransaction(it->gettag(), s))
                    m_journal.warning << "unable to add transaction " << it->gettag();
            }

            // switch to the mutable snapshot
            m_ledgermaster->switchledgers (loadledger, cur);
        }
    }
    catch (shamapmissingnode&)
    {
        m_journal.fatal << "data is missing for selected ledger";
        return false;
    }
    catch (boost::bad_lexical_cast&)
    {
        m_journal.fatal << "ledger specified '" << ledgerid << "' is not valid";
        return false;
    }

    return true;
}

bool serverokay (std::string& reason)
{
    if (!getconfig ().elb_support)
        return true;

    if (getapp().isshutdown ())
    {
        reason = "server is shutting down";
        return false;
    }

    if (getapp().getops ().isneednetworkledger ())
    {
        reason = "not synchronized with network yet";
        return false;
    }

    if (getapp().getops ().getoperatingmode () < networkops::omsyncing)
    {
        reason = "not synchronized with network";
        return false;
    }

    if (!getapp().getledgermaster().iscaughtup(reason))
        return false;

    if (getapp().getfeetrack ().isloadedlocal ())
    {
        reason = "too much load";
        return false;
    }

    if (getapp().getops ().isamendmentblocked ())
    {
        reason = "server version too old";
        return false;
    }

    return true;
}

//vfalco todo clean this up since it is just a file holding a single member function definition

static std::vector<std::string> getschema (databasecon& dbc, std::string const& dbname)
{
    std::vector<std::string> schema;

    std::string sql = "select sql from sqlite_master where tbl_name='";
    sql += dbname;
    sql += "';";

    sql_foreach (dbc.getdb (), sql)
    {
        dbc.getdb ()->getstr ("sql", sql);
        schema.push_back (sql);
    }

    return schema;
}

static bool schemahas (databasecon& dbc, std::string const& dbname, int line, std::string const& content)
{
    std::vector<std::string> schema = getschema (dbc, dbname);

    if (static_cast<int> (schema.size ()) <= line)
    {
        writelog (lsfatal, application) << "schema for " << dbname << " has too few lines";
        throw std::runtime_error ("bad schema");
    }

    return schema[line].find (content) != std::string::npos;
}

static void addtxnseqfield ()
{
    //carl seems initial db already has txnseq now
    return;
    if (schemahas (getapp().gettxndb (), "accounttransactions", 0, "txnseq"))
        return;

    writelog (lswarning, application) << "transaction sequence field is missing";

    auto db = getapp().gettxndb ().getdb ();

    std::vector< std::pair<uint256, int> > txids;
    txids.reserve (300000);

    writelog (lsinfo, application) << "parsing transactions";
    int i = 0;
    uint256 transid;
    sql_foreach (db, "select transid,txnmeta from transactions;")
    {
        blob rawmeta;
        int metasize = 2048;
        rawmeta.resize (metasize);
        metasize = db->getbinary ("txnmeta", &*rawmeta.begin (), rawmeta.size ());

        if (metasize > static_cast<int> (rawmeta.size ()))
        {
            rawmeta.resize (metasize);
            db->getbinary ("txnmeta", &*rawmeta.begin (), rawmeta.size ());
        }
        else rawmeta.resize (metasize);

        std::string tid;
        db->getstr ("transid", tid);
        transid.sethex (tid, true);

        if (rawmeta.size () == 0)
        {
            txids.push_back (std::make_pair (transid, -1));
            writelog (lsinfo, application) << "no metadata for " << transid;
        }
        else
        {
            transactionmetaset m (transid, 0, rawmeta);
            txids.push_back (std::make_pair (transid, m.getindex ()));
        }

        if ((++i % 1000) == 0)
        {
            writelog (lsinfo, application) << i << " transactions read";
        }
    }

    writelog (lsinfo, application) << "all " << i << " transactions read";

//    db->executesql ("begin transaction;");
    db->begintransaction();

    writelog (lsinfo, application) << "dropping old index";
    db->executesql ("drop index accttxindex;");

    writelog (lsinfo, application) << "altering table";
    db->executesql ("alter table accounttransactions add column txnseq integer;");

    boost::format fmt ("update accounttransactions set txnseq = %d where transid = '%s';");
    i = 0;
    for (auto& t : txids)
    {
        db->executesql (boost::str (fmt % t.second % to_string (t.first)));

        if ((++i % 1000) == 0)
        {
            writelog (lsinfo, application) << i << " transactions updated";
        }
    }

    writelog (lsinfo, application) << "building new index";
    db->executesql ("create index accttxindex on accounttransactions(account, ledgerseq, txnseq, transid);");
//    db->executesql ("end transaction;");
    db->endtransaction();
}

static void addclosetimefield()
{
    auto db = getapp().gettxndb ().getdb ();
    if (!db->hasfield("transactions", "closetime"))
    {
        db->begintransaction();
        db->executesql("alter table transactions add column closetime integer not null default 0");
        db->endtransaction();
    }
}
    
void applicationimp::updatetables ()
{
    if (getconfig ().nodedatabase.size () <= 0)
    {
        writelog (lsfatal, application) << "the [node_db] configuration setting has been updated and must be set";
        exitwithcode(1);
    }

    addclosetimefield();
    // perform any needed table updates
    //assert (schemahas (getapp().gettxndb (), "accounttransactions", 0, "transid"));
    //assert (!schemahas (getapp().gettxndb (), "accounttransactions", 0, "foobar"));
    addtxnseqfield ();

    /*
    if (schemahas (getapp().gettxndb (), "accounttransactions", 0, "primary"))
    {
        writelog (lsfatal, application) << "accounttransactions database should not have a primary key";
        exitwithcode(1);
    }
     */

    if (getconfig ().doimport)
    {
        nodestore::dummyscheduler scheduler;
        std::unique_ptr <nodestore::database> source =
            nodestore::manager::instance().make_database ("nodestore.import", scheduler,
                deprecatedlogs().journal("nodeobject"), 0,
                    getconfig ().importnodedatabase);

        writelog (lswarning, nodeobject) <<
            "node import from '" << source->getname () << "' to '"
                                 << getapp().getnodestore().getname () << "'.";

        getapp().getnodestore().import (*source);
    }
}

void applicationimp::onannounceaddress ()
{
    // nikb codeme
}

//------------------------------------------------------------------------------

application::application ()
    : beast::propertystream::source ("app")
{
}

std::unique_ptr <application>
make_application (logs& logs)
{
    return std::make_unique <applicationimp> (logs);
}

application& getapp ()
{
    return applicationimpbase::getinstance ();
}

}
