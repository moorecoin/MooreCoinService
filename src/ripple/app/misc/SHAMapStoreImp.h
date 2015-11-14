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

#ifndef ripple_app_shamapstoreimp_h_included
#define ripple_app_shamapstoreimp_h_included

#include <ripple/app/data/databasecon.h>
#include <ripple/app/misc/shamapstore.h>
#include <ripple/nodestore/impl/tuning.h>
#include <ripple/nodestore/databaserotating.h>
#include <beast/module/sqdb/sqdb.h>

#include <iostream>
#include <condition_variable>

#include "networkops.h"

namespace ripple {

class shamapstoreimp : public shamapstore
{
private:
    struct savedstate
    {
        std::string writabledb;
        std::string archivedb;
        ledgerindex lastrotated;
    };

    enum health : std::uint8_t
    {
        ok = 0,
        stopping,
        unhealthy
    };

    class savedstatedb
    {
    public:
        beast::sqdb::session session_;
        std::mutex mutex_;
        beast::journal journal_;

        // just instantiate without any logic in case online delete is not
        // configured
        savedstatedb() = default;

        // opens sqlite database and, if necessary, creates & initializes its tables.
        void init (std::string const& databasepath, std::string const& dbname);
        // get/set the ledger index that we can delete up to and including
        ledgerindex getcandelete();
        ledgerindex setcandelete (ledgerindex candelete);
        savedstate getstate();
        void setstate (savedstate const& state);
        void setlastrotated (ledgerindex seq);
        void checkerror (beast::error const& error);
    };

    // name of sqlite state database
    std::string const dbname_ = "state.db";
    // prefix of on-disk nodestore backend instances
    std::string const dbprefix_ = "rippledb";
    // check health/stop status as records are copied
    std::uint64_t const checkhealthinterval_ = 1000;
    // minimum # of ledgers to maintain for health of network
    std::uint32_t minimumdeletioninterval_ = 256;

    setup setup_;
    nodestore::scheduler& scheduler_;
    beast::journal journal_;
    beast::journal nodestorejournal_;
    nodestore::databaserotating* database_;
    savedstatedb state_db_;
    std::thread thread_;
    bool stop_ = false;
    bool healthy_ = true;
    mutable std::condition_variable cond_;
    mutable std::mutex mutex_;
    ledger::pointer newledger_;
    ledger::pointer validatedledger_;
    transactionmaster& transactionmaster_;
    // these do not exist upon shamapstore creation, but do exist
    // as of onprepare() or before
    networkops* netops_ = nullptr;
    ledgermaster* ledgermaster_ = nullptr;
    fullbelowcache* fullbelowcache_ = nullptr;
    treenodecache* treenodecache_ = nullptr;
    databasecon* transactiondb_ = nullptr;
    databasecon* ledgerdb_ = nullptr;

public:
    shamapstoreimp (setup const& setup,
            stoppable& parent,
            nodestore::scheduler& scheduler,
            beast::journal journal,
            beast::journal nodestorejournal,
            transactionmaster& transactionmaster);

    ~shamapstoreimp()
    {
        if (thread_.joinable())
            thread_.join();
    }

    std::uint32_t
    clampfetchdepth (std::uint32_t fetch_depth) const override
    {
        return setup_.deleteinterval ? std::min (fetch_depth,
                setup_.deleteinterval) : fetch_depth;
    }

    std::unique_ptr <nodestore::database> makedatabase (
            std::string const&name, std::int32_t readthreads) override;

    ledgerindex
    setcandelete (ledgerindex seq) override
    {
        return state_db_.setcandelete (seq);
    }

    bool
    advisorydelete() const override
    {
        return setup_.advisorydelete;
    }

    ledgerindex
    getlastrotated() override
    {
        return state_db_.getstate().lastrotated;
    }

    ledgerindex
    getcandelete() override
    {
        return state_db_.getcandelete();
    }

    void onledgerclosed (ledger::pointer validatedledger) override;

private:
    // callback for visitnodes
    bool copynode (std::uint64_t& nodecount, shamaptreenode const &node);
    void run();
    void dbpaths();
    std::shared_ptr <nodestore::backend> makebackendrotating (
            std::string path = std::string());
    /**
     * creates a nodestore with two
     * backends to allow online deletion of data.
     *
     * @param name a diagnostic label for the database.
     * @param readthreads the number of async read threads to create
     * @param writablebackend backend for writing
     * @param archivebackend backend for archiving
     *
     * @return the opened database.
     */
    std::unique_ptr <nodestore::databaserotating>
    makedatabaserotating (std::string const&name,
            std::int32_t readthreads,
            std::shared_ptr <nodestore::backend> writablebackend,
            std::shared_ptr <nodestore::backend> archivebackend) const;

    template <class cacheinstance>
    bool
    freshencache (cacheinstance& cache)
    {
        std::uint64_t check = 0;

        for (uint256 it: cache.getkeys())
        {
            database_->fetchnode (it);
            if (! (++check % checkhealthinterval_) && health())
                return true;
        }

        return false;
    }

    /** delete from sqlite table in batches to not lock the db excessively
     *  pause briefly to extend access time to other users
     *  call with mutex object unlocked
     */
    void clearsql (databasecon& database, ledgerindex lastrotated,
            std::string const& minquery, std::string const& deletequery);
    void clearcaches (ledgerindex validatedseq);
    void freshencaches();
    void clearprior (ledgerindex lastrotated);

    // if rippled is not healthy, defer rotate-delete.
    // if already unhealthy, do not change state on further check.
    // assume that, once unhealthy, a necessary step has been
    // aborted, so the online-delete process needs to restart
    // at next ledger.
    health health();
    //
    // stoppable
    //
    void
    onprepare() override
    {
    }

    void
    onstart() override
    {
        if (setup_.deleteinterval)
            thread_ = std::thread (&shamapstoreimp::run, this);
    }

    // called when the application begins shutdown
    void onstop() override;
    // called when all child stoppable objects have stoped
    void onchildrenstopped() override;

};

}

#endif
