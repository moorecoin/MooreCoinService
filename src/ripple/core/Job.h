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

#ifndef ripple_job_h
#define ripple_job_h

#include <ripple/basics/basictypes.h>
#include <ripple/core/loadmonitor.h>

namespace ripple {

// note that this queue should only be used for cpu-bound jobs
// it is primarily intended for signature checking

enum jobtype
{
    // special type indicating an invalid job - will go away soon.
    jtinvalid = -1,

    // job types - the position in this enum indicates the job priority with
    // earlier jobs having lower priority than later jobs. if you wish to
    // insert a job at a specific priority, simply add it at the right location.

    jtpack,          // make a fetch pack for a peer
    jtpuboldledger,  // an old ledger has been accepted
    jtvalidation_ut, // a validation from an untrusted source
    jtproofwork,     // a proof of work demand from another server
    jttransaction_l, // a local transaction
    jtproposal_ut,   // a proposal from an untrusted source
    jtledger_data,   // received data for a ledger we're acquiring
    jtclient,        // a websocket command from the client
    jtrpc,           // a websocket command from the client
    jtupdate_pf,     // update pathfinding requests
    jttransaction,   // a transaction received from the network
    jtunl,           // a score or fetch of the unl (deprecated)
    jtadvance,       // advance validated/acquired ledgers
    jtpubledger,     // publish a fully-accepted ledger
    jttxn_data,      // fetch a proposed set
    jtwal,           // write-ahead logging
    jtvalidation_t,  // a validation from a trusted source
    jtdb_batch,      // batch db update
    jtwrite,         // write out hashed objects
    jtaccept,        // accept a consensus ledger
    jtproposal_t,    // a proposal from a trusted source
    jtdividend,      // process dividend
    jtsweep,         // sweep for stale structures
    jtnetop_cluster, // networkops cluster peer report
    jtnetop_timer,   // networkops net timer processing
    jtadmin,         // an administrative operation

    // special job types which are not dispatched by the job pool
    jtpeer          ,
    jtdisk          ,
    jttxn_proc      ,
    jtob_setup      ,
    jtpath_find     ,
    jtho_read       ,
    jtho_write      ,
    jtgeneric       ,   // used just to measure time

    // node store monitoring
    jtns_sync_read  ,
    jtns_async_read ,
    jtns_write      ,
};

class job
{
public:
    typedef std::chrono::steady_clock clock_type;

    /** default constructor.

        allows job to be used as a container type.

        this is used to allow things like jobmap [key] = value.
    */
    // vfalco note i'd prefer not to have a default constructed object.
    //             what is the semantic meaning of a job with no associated
    //             function? having the invariant "all job objects refer to
    //             a job" would reduce the number of states.
    //
    job ();

    //job (job const& other);

    job (jobtype type, std::uint64_t index);

    /** a callback used to check for canceling a job. */
    typedef std::function <bool(void)> cancelcallback;

    // vfalco todo try to remove the dependency on loadmonitor.
    job (jobtype type,
         std::string const& name,
         std::uint64_t index,
         loadmonitor& lm,
         std::function <void (job&)> const& job,
         cancelcallback cancelcallback);

    //job& operator= (job const& other);

    jobtype gettype () const;

    cancelcallback getcancelcallback () const;

    /** returns the time when the job was queued. */
    clock_type::time_point const& queue_time () const;

    /** returns `true` if the running job should make a best-effort cancel. */
    bool shouldcancel () const;

    void dojob ();

    void rename (std::string const& n);

    // these comparison operators make the jobs sort in priority order
    // in the job set
    bool operator< (const job& j) const;
    bool operator> (const job& j) const;
    bool operator<= (const job& j) const;
    bool operator>= (const job& j) const;

private:
    cancelcallback m_cancelcallback;
    jobtype                     mtype;
    std::uint64_t               mjobindex;
    std::function <void (job&)> mjob;
    loadevent::pointer          m_loadevent;
    std::string                 mname;
    clock_type::time_point m_queue_time;
};

}

#endif
