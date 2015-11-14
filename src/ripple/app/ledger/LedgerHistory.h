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

#ifndef ripple_ledgerhistory_h
#define ripple_ledgerhistory_h

#include <ripple/app/ledger/ledger.h>
#include <ripple/protocol/rippleledgerhash.h>
#include <beast/insight/collector.h>
#include <beast/insight/event.h>

namespace ripple {

// vfalco todo rename to oldledgers ?

/** retains historical ledgers. */
class ledgerhistory
{
public:
    explicit
    ledgerhistory (beast::insight::collector::ptr const& collector);

    /** track a ledger
        @return `true` if the ledger was already tracked
    */
    bool addledger (ledger::pointer ledger, bool validated);

    /** get the ledgers_by_hash cache hit rate
        @return the hit rate
    */
    float getcachehitrate ()
    {
        return m_ledgers_by_hash.gethitrate ();
    }

    /** get a ledger given its squence number
        @param ledgerindex the sequence number of the desired ledger
    */
    ledger::pointer getledgerbyseq (ledgerindex ledgerindex);

    /** get a ledger's hash given its sequence number
        @param ledgerindex the sequence number of the desired ledger
        @return the hash of the specified ledger
    */
    ledgerhash getledgerhash (ledgerindex ledgerindex);

    /** retrieve a ledger given its hash
        @param ledgerhash the hash of the requested ledger
        @return the ledger requested
    */
    ledger::pointer getledgerbyhash (ledgerhash const& ledgerhash);

    /** set the history cache's paramters
        @param size the target size of the cache
        @param age the target age of the cache, in seconds
    */
    void tune (int size, int age);

    /** remove stale cache entries
    */
    void sweep ()
    {
        m_ledgers_by_hash.sweep ();
        m_consensus_validated.sweep ();
    }

    /** report that we have locally built a particular ledger
    */
    void builtledger (ledger::ref);

    /** report that we have validated a particular ledger
    */
    void validatedledger (ledger::ref);

    /** repair a hash to index mapping
        @param ledgerindex the index whose mapping is to be repaired
        @param ledgerhash the hash it is to be mapped to
        @return `true` if the mapping was repaired
    */
    bool fixindex(ledgerindex ledgerindex, ledgerhash const& ledgerhash);

    void clearledgercacheprior (ledgerindex seq);

private:

    /** log details in the case where we build one ledger but
        validate a different one.
        @param built the hash of the ledger we built
        @param valid the hash of the ledger we deemed fully valid
    */
    void handlemismatch (ledgerhash const& built, ledgerhash const& valid);

    beast::insight::collector::ptr collector_;
    beast::insight::counter mismatch_counter_;

    typedef taggedcache <ledgerhash, ledger> ledgersbyhash;

    ledgersbyhash m_ledgers_by_hash;

    // maps ledger indexes to the corresponding hashes
    // for debug and logging purposes
    // 1) the hash of a ledger with that index we build
    // 2) the hash of a ledger with that index we validated
    typedef taggedcache <ledgerindex,
        std::pair< ledgerhash, ledgerhash >> consensusvalidated;
    consensusvalidated m_consensus_validated;


    // maps ledger indexes to the corresponding hash.
    std::map <ledgerindex, ledgerhash> mledgersbyindex; // validated ledgers
};

} // ripple

#endif
