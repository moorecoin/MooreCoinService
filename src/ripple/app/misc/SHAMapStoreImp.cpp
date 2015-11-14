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
#include <ripple/app/misc/shamapstoreimp.h>
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/main/application.h>
#include <boost/format.hpp>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

void
shamapstoreimp::savedstatedb::init (std::string const& databasepath,
        std::string const& dbname)
{
    boost::filesystem::path pathname = databasepath;
    pathname /= dbname;

    std::lock_guard <std::mutex> lock (mutex_);

    auto error = session_.open (pathname.string());
    checkerror (error);

    session_.once (error) << "pragma synchronous=full;";
    checkerror (error);

    session_.once (error) <<
            "create table if not exists dbstate ("
            "  key                    integer primary key,"
            "  writabledb             text,"
            "  archivedb              text,"
            "  lastrotatedledger      integer"
            ");"
            ;
    checkerror (error);

    session_.once (error) <<
            "create table if not exists candelete ("
            "  key                    integer primary key,"
            "  candeleteseq           integer"
            ");"
            ;

    std::int64_t count = 0;
    beast::sqdb::statement st = (session_.prepare <<
            "select count(key) from dbstate where key = 1;"
            , beast::sqdb::into (count)
            );
    st.execute_and_fetch (error);
    checkerror (error);

    if (!count)
    {
        session_.once (error) <<
                "insert into dbstate values (1, '', '', 0);";
        checkerror (error);
    }

    st = (session_.prepare <<
            "select count(key) from candelete where key = 1;"
            , beast::sqdb::into (count)
            );
    st.execute_and_fetch (error);
    checkerror (error);

    if (!count)
    {
        session_.once (error) <<
                "insert into candelete values (1, 0);";
        checkerror (error);
    }
}

ledgerindex
shamapstoreimp::savedstatedb::getcandelete()
{
    beast::error error;
    ledgerindex seq;

    {
        std::lock_guard <std::mutex> lock (mutex_);

        session_.once (error) <<
                "select candeleteseq from candelete where key = 1;"
                , beast::sqdb::into (seq);
        ;
    }
    checkerror (error);

    return seq;
}

ledgerindex
shamapstoreimp::savedstatedb::setcandelete (ledgerindex candelete)
{
    beast::error error;
    {
        std::lock_guard <std::mutex> lock (mutex_);

        session_.once (error) <<
                "update candelete set candeleteseq = ? where key = 1;"
                , beast::sqdb::use (candelete)
                ;
    }
    checkerror (error);

    return candelete;
}

shamapstoreimp::savedstate
shamapstoreimp::savedstatedb::getstate()
{
    beast::error error;
    savedstate state;

    {
        std::lock_guard <std::mutex> lock (mutex_);

        session_.once (error) <<
                "select writabledb, archivedb, lastrotatedledger"
                " from dbstate where key = 1;"
                , beast::sqdb::into (state.writabledb)
                , beast::sqdb::into (state.archivedb)
                , beast::sqdb::into (state.lastrotated)
                ;
    }
    checkerror (error);

    return state;
}

void
shamapstoreimp::savedstatedb::setstate (savedstate const& state)
{
    beast::error error;

    {
        std::lock_guard <std::mutex> lock (mutex_);
        session_.once (error) <<
                "update dbstate"
                " set writabledb = ?,"
                " archivedb = ?,"
                " lastrotatedledger = ?"
                " where key = 1;"
                , beast::sqdb::use (state.writabledb)
                , beast::sqdb::use (state.archivedb)
                , beast::sqdb::use (state.lastrotated)
                ;
    }
    checkerror (error);
}

void
shamapstoreimp::savedstatedb::setlastrotated (ledgerindex seq)
{
    beast::error error;
    {
        std::lock_guard <std::mutex> lock (mutex_);
        session_.once (error) <<
                "update dbstate set lastrotatedledger = ?"
                " where key = 1;"
                , beast::sqdb::use (seq)
                ;
    }
    checkerror (error);
}

void
shamapstoreimp::savedstatedb::checkerror (beast::error const& error)
{
    if (error)
    {
        journal_.fatal << "state database error: " << error.code()
                << ": " << error.getreasontext();
        throw std::runtime_error ("state database error.");
    }
}

shamapstoreimp::shamapstoreimp (setup const& setup,
        stoppable& parent,
        nodestore::scheduler& scheduler,
        beast::journal journal,
        beast::journal nodestorejournal,
        transactionmaster& transactionmaster)
    : shamapstore (parent)
    , setup_ (setup)
    , scheduler_ (scheduler)
    , journal_ (journal)
    , nodestorejournal_ (nodestorejournal)
    , database_ (nullptr)
    , transactionmaster_ (transactionmaster)
{
    if (setup_.deleteinterval)
    {
        if (setup_.deleteinterval < minimumdeletioninterval_)
        {
            throw std::runtime_error ("online_delete must be at least " +
                std::to_string (minimumdeletioninterval_));
        }

        if (setup_.ledgerhistory > setup_.deleteinterval)
        {
            throw std::runtime_error (
                "online_delete must be less than ledger_history (currently " +
                std::to_string (setup_.ledgerhistory) + ")");
        }

        state_db_.init (setup_.databasepath, dbname_);

        dbpaths();
    }
}

std::unique_ptr <nodestore::database>
shamapstoreimp::makedatabase (std::string const& name,
        std::int32_t readthreads)
{
    std::unique_ptr <nodestore::database> db;

    if (setup_.deleteinterval)
    {
        savedstate state = state_db_.getstate();

        std::shared_ptr <nodestore::backend> writablebackend (
                makebackendrotating (state.writabledb));
        std::shared_ptr <nodestore::backend> archivebackend (
                makebackendrotating (state.archivedb));
        std::unique_ptr <nodestore::databaserotating> dbr =
                makedatabaserotating (name, readthreads, writablebackend,
                archivebackend);

        if (!state.writabledb.size())
        {
            state.writabledb = writablebackend->getname();
            state.archivedb = archivebackend->getname();
            state_db_.setstate (state);
        }

        database_ = dbr.get();
        db.reset (dynamic_cast <nodestore::database*>(dbr.release()));
    }
    else
    {
        db = nodestore::manager::instance().make_database (name, scheduler_, nodestorejournal_,
                readthreads, setup_.nodedatabase,
                setup_.ephemeralnodedatabase);
    }

    return db;
}

void
shamapstoreimp::onledgerclosed (ledger::pointer validatedledger)
{
    {
        std::lock_guard <std::mutex> lock (mutex_);
        newledger_ = validatedledger;
    }
    cond_.notify_one();
}

bool
shamapstoreimp::copynode (std::uint64_t& nodecount,
        shamaptreenode const& node)
{
    // copy a single record from node to database_
    database_->fetchnode (node.getnodehash());
    if (! (++nodecount % checkhealthinterval_))
    {
        if (health())
            return true;
    }

    return false;
}

void
shamapstoreimp::run()
{
    ledgerindex lastrotated = state_db_.getstate().lastrotated;
    netops_ = &getapp().getops();
    ledgermaster_ = &getapp().getledgermaster();
    fullbelowcache_ = &getapp().getfullbelowcache();
    treenodecache_ = &getapp().gettreenodecache();
    transactiondb_ = &getapp().gettxndb();
    ledgerdb_ = &getapp().getledgerdb();

    while (1)
    {
        healthy_ = true;
        validatedledger_.reset();

        std::unique_lock <std::mutex> lock (mutex_);
        if (stop_)
        {
            stopped();
            return;
        }
        cond_.wait (lock);
        if (newledger_)
            validatedledger_ = std::move (newledger_);
        else
            continue;
        lock.unlock();

        ledgerindex validatedseq = validatedledger_->getledgerseq();
        if (!lastrotated)
        {
            lastrotated = validatedseq;
            state_db_.setlastrotated (lastrotated);
        }
        ledgerindex candelete = std::numeric_limits <ledgerindex>::max();
        if (setup_.advisorydelete)
            candelete = state_db_.getcandelete();

        // will delete up to (not including) lastrotated)
        if (validatedseq >= lastrotated + setup_.deleteinterval
                && candelete >= lastrotated - 1)
        {
            journal_.debug << "rotating  validatedseq " << validatedseq
                    << " lastrotated " << lastrotated << " deleteinterval "
                    << setup_.deleteinterval << " candelete " << candelete;

            switch (health())
            {
                case health::stopping:
                    stopped();
                    return;
                case health::unhealthy:
                    continue;
                case health::ok:
                default:
                    ;
            }

            clearprior (lastrotated);
            switch (health())
            {
                case health::stopping:
                    stopped();
                    return;
                case health::unhealthy:
                    continue;
                case health::ok:
                default:
                    ;
            }

            std::uint64_t nodecount = 0;
            validatedledger_->peekaccountstatemap()->snapshot (
                    false)->visitnodes (
                    std::bind (&shamapstoreimp::copynode, this,
                    std::ref(nodecount), std::placeholders::_1));
            journal_.debug << "copied ledger " << validatedseq
                    << " nodecount " << nodecount;
            switch (health())
            {
                case health::stopping:
                    stopped();
                    return;
                case health::unhealthy:
                    continue;
                case health::ok:
                default:
                    ;
            }

            freshencaches();
            journal_.debug << validatedseq << " freshened caches";
            switch (health())
            {
                case health::stopping:
                    stopped();
                    return;
                case health::unhealthy:
                    continue;
                case health::ok:
                default:
                    ;
            }

            std::shared_ptr <nodestore::backend> newbackend =
                    makebackendrotating();
            journal_.debug << validatedseq << " new backend "
                    << newbackend->getname();
            std::shared_ptr <nodestore::backend> oldbackend;

            clearcaches (validatedseq);
            switch (health())
            {
                case health::stopping:
                    stopped();
                    return;
                case health::unhealthy:
                    continue;
                case health::ok:
                default:
                    ;
            }

            std::string nextarchivedir =
                    database_->getwritablebackend()->getname();
            lastrotated = validatedseq;
            {
                std::lock_guard <std::mutex> lock (database_->peekmutex());

                state_db_.setstate (savedstate {newbackend->getname(),
                        nextarchivedir, lastrotated});
                clearcaches (validatedseq);
                oldbackend = database_->rotatebackends (newbackend);
            }
            journal_.debug << "finished rotation " << validatedseq;

            oldbackend->setdeletepath();
        }
    }
}

void
shamapstoreimp::dbpaths()
{
    boost::filesystem::path dbpath =
            setup_.nodedatabase["path"].tostdstring();

    if (boost::filesystem::exists (dbpath))
    {
        if (! boost::filesystem::is_directory (dbpath))
        {
            std::cerr << "node db path must be a directory. "
                    << dbpath.string();
            throw std::runtime_error (
                    "node db path must be a directory.");
        }
    }
    else
    {
        boost::filesystem::create_directories (dbpath);
    }

    savedstate state = state_db_.getstate();
    bool writabledbexists = false;
    bool archivedbexists = false;

    for (boost::filesystem::directory_iterator it (dbpath);
            it != boost::filesystem::directory_iterator(); ++it)
    {
        if (! state.writabledb.compare (it->path().string()))
            writabledbexists = true;
        else if (! state.archivedb.compare (it->path().string()))
            archivedbexists = true;
        else if (! dbprefix_.compare (it->path().stem().string()))
            boost::filesystem::remove_all (it->path());
    }

    if ((!writabledbexists && state.writabledb.size()) ||
            (!archivedbexists && state.archivedb.size()) ||
            (writabledbexists != archivedbexists) ||
            state.writabledb.empty() != state.archivedb.empty())
    {
        boost::filesystem::path statedbpathname = setup_.databasepath;
        statedbpathname /= dbname_;
        statedbpathname += "*";

        std::cerr << "state db error: " << std::endl
                << "  writabledbexists " << writabledbexists
                << " archivedbexists " << archivedbexists << std::endl
                << "  writabledb '" << state.writabledb
                << "' archivedb '" << state.archivedb << "'"
                << std::endl << std::endl
                << "to resume operation, make backups of and "
                << "remove the files matching "
                << statedbpathname.string()
                << " and contents of the directory "
                << setup_.nodedatabase["path"].tostdstring()
                << std::endl;

        throw std::runtime_error ("state db error");
    }
}

std::shared_ptr <nodestore::backend>
shamapstoreimp::makebackendrotating (std::string path)
{
    boost::filesystem::path newpath;
    nodestore::parameters parameters = setup_.nodedatabase;

    if (path.size())
    {
        newpath = path;
    }
    else
    {
        boost::filesystem::path p = parameters["path"].tostdstring();
        p /= dbprefix_;
        p += ".%%%%";
        newpath = boost::filesystem::unique_path (p);
    }
    parameters.set("path", newpath.string());

    return nodestore::manager::instance().make_backend (parameters, scheduler_,
            nodestorejournal_);
}

std::unique_ptr <nodestore::databaserotating>
shamapstoreimp::makedatabaserotating (std::string const& name,
        std::int32_t readthreads,
        std::shared_ptr <nodestore::backend> writablebackend,
        std::shared_ptr <nodestore::backend> archivebackend) const
{
    std::unique_ptr <nodestore::backend> fastbackend (
        (setup_.ephemeralnodedatabase.size() > 0)
            ? nodestore::manager::instance().make_backend (setup_.ephemeralnodedatabase,
            scheduler_, journal_) : nullptr);

    return nodestore::manager::instance().make_databaserotating ("nodestore.main", scheduler_,
            readthreads, writablebackend, archivebackend,
            std::move (fastbackend), nodestorejournal_);
}

void
shamapstoreimp::clearsql (databasecon& database,
        ledgerindex lastrotated,
        std::string const& minquery,
        std::string const& deletequery)
{
    ledgerindex min = std::numeric_limits <ledgerindex>::max();
    database* db = database.getdb();

    std::unique_lock <std::recursive_mutex> lock (database.peekmutex());
    if (!db->executesql (minquery) || !db->startiterrows())
        return;
    min = db->getbigint (0);
    db->enditerrows ();
    lock.unlock();
    if (health() != health::ok)
        return;

    boost::format formatteddeletequery (deletequery);

    if (journal_.debug) journal_.debug <<
        "start: " << deletequery << " from " << min << " to " << lastrotated;
    while (min < lastrotated)
    {
        min = (min + setup_.deletebatch >= lastrotated) ? lastrotated :
            min + setup_.deletebatch;
        lock.lock();
        db->executesql (boost::str (formatteddeletequery % min));
        lock.unlock();
        if (health())
            return;
        if (min < lastrotated)
            std::this_thread::sleep_for (
                    std::chrono::milliseconds (setup_.backoff));
    }
    journal_.debug << "finished: " << deletequery;
}

void
shamapstoreimp::clearcaches (ledgerindex validatedseq)
{
    ledgermaster_->clearledgercacheprior (validatedseq);
    fullbelowcache_->clear();
}

void
shamapstoreimp::freshencaches()
{
    if (freshencache (database_->getpositivecache()))
        return;
    if (freshencache (*treenodecache_))
        return;
    if (freshencache (transactionmaster_.getcache()))
        return;
}

void
shamapstoreimp::clearprior (ledgerindex lastrotated)
{
    ledgermaster_->clearpriorledgers (lastrotated);
    if (health())
        return;

    // todo this won't remove validations for ledgers that do not get
    // validated. that will likely require inserting ledgerseq into
    // the validations table
    clearsql (*ledgerdb_, lastrotated,
        "select min(ledgerseq) from ledgers;",
        "delete from validations where ledgers.ledgerseq < %u"
        " and validations.ledgerhash = ledgers.ledgerhash;");
    if (health())
        return;

    clearsql (*ledgerdb_, lastrotated,
        "select min(ledgerseq) from ledgers;",
        "delete from ledgers where ledgerseq < %u;");
    if (health())
        return;

    clearsql (*transactiondb_, lastrotated,
        "select min(ledgerseq) from transactions;",
        "delete from transactions where ledgerseq < %u;");
    if (health())
        return;

    clearsql (*transactiondb_, lastrotated,
        "select min(ledgerseq) from accounttransactions;",
        "delete from accounttransactions where ledgerseq < %u;");
    if (health())
        return;
}

shamapstoreimp::health
shamapstoreimp::health()
{
    {
        std::lock_guard<std::mutex> lock (mutex_);
        if (stop_)
            return health::stopping;
    }
    if (!netops_)
        return health::ok;

    networkops::operatingmode mode = netops_->getoperatingmode();

    std::int32_t age = ledgermaster_->getvalidatedledgerage();
    if (mode != networkops::omfull || age >= setup_.agethreshold)
    {
        journal_.warning << "not deleting. state: " << mode << " age " << age
                << " age threshold " << setup_.agethreshold;
        healthy_ = false;
    }

    if (healthy_)
        return health::ok;
    else
        return health::unhealthy;
}

void
shamapstoreimp::onstop()
{
    if (setup_.deleteinterval)
    {
        {
            std::lock_guard <std::mutex> lock (mutex_);
            stop_ = true;
        }
        cond_.notify_one();
    }
    else
    {
        stopped();
    }
}

void
shamapstoreimp::onchildrenstopped()
{
    if (setup_.deleteinterval)
    {
        {
            std::lock_guard <std::mutex> lock (mutex_);
            stop_ = true;
        }
        cond_.notify_one();
    }
    else
    {
        stopped();
    }
}

//------------------------------------------------------------------------------
shamapstore::setup
setup_shamapstore (config const& c)
{
    shamapstore::setup setup;

    if (c.nodedatabase["online_delete"].isnotempty())
        setup.deleteinterval = c.nodedatabase["online_delete"].getintvalue();
    if (c.nodedatabase["advisory_delete"].isnotempty() && setup.deleteinterval)
        setup.advisorydelete = c.nodedatabase["advisory_delete"].getintvalue();
    setup.ledgerhistory = c.ledger_history;
    setup.nodedatabase = c.nodedatabase;
    setup.ephemeralnodedatabase = c.ephemeralnodedatabase;
    setup.databasepath = c.database_path;
    if (c.nodedatabase["delete_batch"].isnotempty())
        setup.deletebatch = c.nodedatabase["delete_batch"].getintvalue();
    if (c.nodedatabase["backoff"].isnotempty())
        setup.backoff = c.nodedatabase["backoff"].getintvalue();
    if (c.nodedatabase["age_threshold"].isnotempty())
        setup.agethreshold = c.nodedatabase["age_threshold"].getintvalue();

    return setup;
}

std::unique_ptr<shamapstore>
make_shamapstore (shamapstore::setup const& s,
        beast::stoppable& parent,
        nodestore::scheduler& scheduler,
        beast::journal journal,
        beast::journal nodestorejournal,
        transactionmaster& transactionmaster)
{
    return std::make_unique<shamapstoreimp> (s, parent, scheduler,
            journal, nodestorejournal, transactionmaster);
}

}
