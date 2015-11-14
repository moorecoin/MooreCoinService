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

#ifndef ripple_jobtypes_h_included
#define ripple_jobtypes_h_included

#include <ripple/core/job.h>
#include <ripple/core/jobtypeinfo.h>
#include <map>

namespace ripple
{

class jobtypes
{
public:
    typedef std::map <jobtype, jobtypeinfo> map;
    typedef map::const_iterator const_iterator;


    jobtypes ()
        : m_unknown (jtinvalid, "invalid", 0, true, true, 0, 0)
    {
        int maxlimit = std::numeric_limits <int>::max ();

        // make a fetch pack for a peer
        add (jtpack,          "makefetchpack",
            1,        true,   false, 0,     0);

        // an old ledger has been accepted
        add (jtpuboldledger,  "publishacqledger",
            2,        true,   false, 10000, 15000);

        // a validation from an untrusted source
        add (jtvalidation_ut, "untrustedvalidation",
            maxlimit, true,   false, 2000,  5000);

        // a proof of work demand from another server
        add (jtproofwork,     "proofofwork",
            maxlimit, true,   false, 2000,  5000);

        // a local transaction
        add (jttransaction_l, "localtransaction",
            maxlimit, true,   false, 100,   500);

        // a proposal from an untrusted source
        add (jtproposal_ut,   "untrustedproposal",
            maxlimit, true,   false, 500,   1250);

        // received data for a ledger we're acquiring
        add (jtledger_data,   "ledgerdata",
            2,        true,   false, 0,     0);

        // update pathfinding requests
        add (jtupdate_pf,     "updatepaths",
            maxlimit, true,   false, 0,     0);

        // a websocket command from the client
        add (jtclient,        "clientcommand",
            maxlimit, true,   false, 2000,  5000);

        // a websocket command from the client
        add (jtrpc,           "rpc",
            maxlimit, false,  false, 0,     0);

        // a transaction received from the network
        add (jttransaction,   "transaction",
            maxlimit, true,   false, 250,   1000);

        // a score or fetch of the unl (deprecated)
        add (jtunl,           "unl",
            1,        true,   false, 0,     0);

        // advance validated/acquired ledgers
        add (jtadvance,       "advanceledger",
            maxlimit, true,   false, 0,     0);

        // publish a fully-accepted ledger
        add (jtpubledger,     "publishnewledger",
            maxlimit, true,   false, 3000,  4500);

        // fetch a proposed set
        add (jttxn_data,      "fetchtxndata",
            1,        true,   false, 0,     0);

        // write-ahead logging
        add (jtwal,           "writeahead",
            maxlimit, false,  false, 1000,  2500);

        // a validation from a trusted source
        add (jtvalidation_t,  "trustedvalidation",
            maxlimit, true,   false, 500,  1500);

        // process db batch commit
        add (jtdb_batch,      "dbbatch",
             maxlimit, false,  false, 0,     0);
        
        // write out hashed objects
        add (jtwrite,         "writeobjects",
            maxlimit, false,  false, 1750,  2500);

        // accept a consensus ledger
        add (jtaccept,        "acceptledger",
            maxlimit, false,  false, 0,     0);

        // a proposal from a trusted source
        add (jtproposal_t,    "trustedproposal",
            maxlimit, false,  false, 100,   500);
        
        // process dividend
        add (jtdividend,      "dividend",
            1,        false,  false, 0,     0);

        // sweep for stale structures
        add (jtsweep,         "sweep",
            maxlimit, true,   false, 0,     0);

        // networkops cluster peer report
        add (jtnetop_cluster, "clusterreport",
            1,        true,   false, 9999,  9999);

        // networkops net timer processing
        add (jtnetop_timer,   "heartbeat",
            1,        true,   false, 999,   999);

        // an administrative operation
        add (jtadmin,         "administration",
            maxlimit, true,   false, 0,     0);

        // the rest are special job types that are not dispatched
        // by the job pool. the "limit" and "skip" attributes are
        // not applicable to these types of jobs.

        add (jtpeer,          "peercommand",
            0,        false,  true,  200,   2500);

        add (jtdisk,          "diskaccess",
            0,        false,  true,  500,   1000);

        add (jttxn_proc,      "processtransaction",
            0,        false,  true,  0,     0);

        add (jtob_setup,      "orderbooksetup",
            0,        false,  true,  0,     0);

        add (jtpath_find,     "pathfind",
            0,        false,  true,  0,     0);

        add (jtho_read,       "noderead",
            0,        false,  true,  0,     0);

        add (jtho_write,      "nodewrite",
            0,        false,  true,  0,     0);

        add (jtgeneric,       "generic",
            0,        false,  true,  0,     0);

        add (jtns_sync_read,  "syncreadnode",
            0,        false,  true,  0,     0);
        add (jtns_async_read, "asyncreadnode",
            0,        false,  true,  0,     0);
        add (jtns_write,      "writenode",
            0,        false,  true,  0,     0);

    }

    jobtypeinfo const& get (jobtype jt) const
    {
        map::const_iterator const iter (m_map.find (jt));
        assert (iter != m_map.end ());

        if (iter != m_map.end())
            return iter->second;

        return m_unknown;
    }

    jobtypeinfo const& getinvalid () const
    {
        return m_unknown;
    }

    const_iterator begin () const
    {
        return m_map.cbegin ();
    }

    const_iterator cbegin () const
    {
        return m_map.cbegin ();
    }

    const_iterator end () const
    {
        return m_map.cend ();
    }

    const_iterator cend () const
    {
        return m_map.cend ();
    }

private:
    void add(jobtype jt, std::string name, int limit,
        bool skip, bool special, std::uint64_t avglatency, std::uint64_t peaklatency)
    {
        assert (m_map.find (jt) == m_map.end ());

        std::pair<map::iterator,bool> result (m_map.emplace (
            std::piecewise_construct,
            std::forward_as_tuple (jt),
            std::forward_as_tuple (jt, name, limit, skip, special,
                avglatency, peaklatency)));

        assert (result.second == true);
        (void) result.second;
    }

    jobtypeinfo m_unknown;
    map m_map;
};

}

#endif
