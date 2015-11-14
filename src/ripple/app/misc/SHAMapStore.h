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

#ifndef ripple_app_shamapstore_h_included
#define ripple_app_shamapstore_h_included

#include <ripple/app/ledger/ledger.h>
#include <ripple/app/tx/transactionmaster.h>
#include <ripple/core/config.h>
#include <ripple/nodestore/manager.h>
#include <ripple/nodestore/scheduler.h>
#include <ripple/protocol/errorcodes.h>
#include <beast/threads/stoppable.h>

namespace ripple {

/**
 * class to create database, launch online delete thread, and
 * related sqlite databse
 */
class shamapstore
    : public beast::stoppable
{
public:
    struct setup
    {
        std::uint32_t deleteinterval = 0;
        bool advisorydelete = false;
        std::uint32_t ledgerhistory = 0;
        beast::stringpairarray nodedatabase;
        beast::stringpairarray ephemeralnodedatabase;
        std::string databasepath;
        std::uint32_t deletebatch = 100;
        std::uint32_t backoff = 100;
        std::int32_t agethreshold = 60;
    };

    shamapstore (stoppable& parent) : stoppable ("shamapstore", parent) {}

    /** called by ledgermaster every time a ledger validates. */
    virtual void onledgerclosed (ledger::pointer validatedledger) = 0;

    virtual std::uint32_t clampfetchdepth (std::uint32_t fetch_depth) const = 0;

    virtual std::unique_ptr <nodestore::database> makedatabase (
            std::string const& name, std::int32_t readthreads) = 0;

    /** highest ledger that may be deleted. */
    virtual ledgerindex setcandelete (ledgerindex candelete) = 0;

    /** whether advisory delete is enabled. */
    virtual bool advisorydelete() const = 0;

    /** last ledger which was copied during rotation of backends. */
    virtual ledgerindex getlastrotated() = 0;

    /** highest ledger that may be deleted. */
    virtual ledgerindex getcandelete() = 0;
};

//------------------------------------------------------------------------------

shamapstore::setup
setup_shamapstore(config const& c);

std::unique_ptr<shamapstore>
make_shamapstore(shamapstore::setup const& s,
        beast::stoppable& parent,
        nodestore::scheduler& scheduler,
        beast::journal journal,
        beast::journal nodestorejournal,
        transactionmaster& transactionmaster);
}

#endif
