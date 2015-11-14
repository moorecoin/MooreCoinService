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
#include <ripple/app/ledger/ledgerhistory.h>
#include <ripple/basics/log.h>
#include <ripple/basics/seconds_clock.h>

namespace ripple {

// vfalco todo replace macros

#ifndef cached_ledger_num
#define cached_ledger_num 96
#endif

#ifndef cached_ledger_age
#define cached_ledger_age 120
#endif

// fixme: need to clean up ledgers by index at some point

ledgerhistory::ledgerhistory (
    beast::insight::collector::ptr const& collector)
    : collector_ (collector)
    , mismatch_counter_ (collector->make_counter ("ledger.history", "mismatch"))
    , m_ledgers_by_hash ("ledgercache", cached_ledger_num, cached_ledger_age,
        get_seconds_clock (), deprecatedlogs().journal("taggedcache"))
    , m_consensus_validated ("consensusvalidated", 64, 300,
        get_seconds_clock (), deprecatedlogs().journal("taggedcache"))
{
}

bool ledgerhistory::addledger (ledger::pointer ledger, bool validated)
{
    assert (ledger && ledger->isimmutable ());
    assert (ledger->peekaccountstatemap ()->gethash ().isnonzero ());

    ledgersbyhash::scopedlocktype sl (m_ledgers_by_hash.peekmutex ());

    const bool alreadyhad = m_ledgers_by_hash.canonicalize (ledger->gethash(), ledger, true);
    if (validated)
        mledgersbyindex[ledger->getledgerseq()] = ledger->gethash();

    return alreadyhad;
}

uint256 ledgerhistory::getledgerhash (std::uint32_t index)
{
    ledgersbyhash::scopedlocktype sl (m_ledgers_by_hash.peekmutex ());
    std::map<std::uint32_t, uint256>::iterator it (mledgersbyindex.find (index));

    if (it != mledgersbyindex.end ())
        return it->second;

    return uint256 ();
}

ledger::pointer ledgerhistory::getledgerbyseq (std::uint32_t index)
{
    {
        ledgersbyhash::scopedlocktype sl (m_ledgers_by_hash.peekmutex ());
        std::map <std::uint32_t, uint256>::iterator it (mledgersbyindex.find (index));

        if (it != mledgersbyindex.end ())
        {
            uint256 hash = it->second;
            sl.unlock ();
            return getledgerbyhash (hash);
        }
    }

    ledger::pointer ret (ledger::loadbyindex (index));

    if (!ret)
        return ret;

    assert (ret->getledgerseq () == index);

    {
        // add this ledger to the local tracking by index
        ledgersbyhash::scopedlocktype sl (m_ledgers_by_hash.peekmutex ());

        assert (ret->isimmutable ());
        m_ledgers_by_hash.canonicalize (ret->gethash (), ret);
        mledgersbyindex[ret->getledgerseq ()] = ret->gethash ();
        return (ret->getledgerseq () == index) ? ret : ledger::pointer ();
    }
}

ledger::pointer ledgerhistory::getledgerbyhash (uint256 const& hash)
{
    ledger::pointer ret = m_ledgers_by_hash.fetch (hash);

    if (ret)
    {
        assert (ret->isimmutable ());
        assert (ret->gethash () == hash);
        return ret;
    }

    ret = ledger::loadbyhash (hash);

    if (!ret)
        return ret;

    assert (ret->isimmutable ());
    assert (ret->gethash () == hash);
    m_ledgers_by_hash.canonicalize (ret->gethash (), ret);
    assert (ret->gethash () == hash);

    return ret;
}

static void addleaf (std::vector <uint256> &vec, shamapitem::ref item)
{
    vec.push_back (item->gettag ());
}

void ledgerhistory::handlemismatch (ledgerhash const& built, ledgerhash  const& valid)
{
    assert (built != valid);
    ++mismatch_counter_;

    ledger::pointer builtledger = getledgerbyhash (built);
    ledger::pointer validledger = getledgerbyhash (valid);

    if (builtledger && validledger)
    {
        assert (builtledger->getledgerseq() == validledger->getledgerseq());

        // determine the mismatch reason
        // distinguish byzantine failure from transaction processing difference

        if (builtledger->getparenthash() != validledger->getparenthash())
        {
            // disagreement over prior ledger indicates sync issue
            writelog (lserror, ledgermaster) << "mismatch on prior ledger";
        }
        else if (builtledger->getclosetimenc() != validledger->getclosetimenc())
        {
            // disagreement over close time indicates byzantine failure
            writelog (lserror, ledgermaster) << "mismatch on close time";
        }
        else
        {
            std::vector <uint256> builttx, validtx;
            builtledger->peektransactionmap()->visitleaves(
                std::bind (&addleaf, std::ref (builttx), std::placeholders::_1));
            validledger->peektransactionmap()->visitleaves(
                std::bind (&addleaf, std::ref (validtx), std::placeholders::_1));
            std::sort (builttx.begin(), builttx.end());
            std::sort (validtx.begin(), validtx.end());

            if (builttx == validtx)
            {
                // disagreement with same prior ledger, close time, and transactions
                // indicates a transaction processing difference
                writelog (lserror, ledgermaster) <<
                    "mismatch with same " << builttx.size() << " tx";
            }
            else
            {
                std::vector <uint256> notbuilt, notvalid;
                std::set_difference (
                    validtx.begin(), validtx.end(),
                    builttx.begin(), builttx.end(),
                    std::inserter (notbuilt, notbuilt.begin()));
                std::set_difference (
                    builttx.begin(), builttx.end(),
                    validtx.begin(), validtx.end(),
                    std::inserter (notvalid, notvalid.begin()));

                // this can be either a disagreement over the consensus
                // set or difference in which transactions were rejected
                // as invalid

                writelog (lserror, ledgermaster) << "mismatch tx differ "
                    << builttx.size() << " built, " << validtx.size() << " valid";
                for (auto const& t : notbuilt)
                {
                    writelog (lserror, ledgermaster) << "mismatch built without " << t;
                }
                for (auto const& t : notvalid)
                {
                    writelog (lserror, ledgermaster) << "mismatch valid without " << t;
                }
            }
        }
    }
    else
        writelog (lserror, ledgermaster) << "mismatch cannot be analyzed";
}

void ledgerhistory::builtledger (ledger::ref ledger)
{
    ledgerindex index = ledger->getledgerseq();
    ledgerhash hash = ledger->gethash();
    assert (!hash.iszero());
    consensusvalidated::scopedlocktype sl (
        m_consensus_validated.peekmutex());

    auto entry = std::make_shared<std::pair< ledgerhash, ledgerhash >>();
    m_consensus_validated.canonicalize(index, entry, false);

    if (entry->first != hash)
    {
        bool mismatch (false);

        if (entry->first.isnonzero() && (entry->first != hash))
        {
            writelog (lserror, ledgermaster) << "mismatch: seq=" << index << " built:" << entry->first << " then:" << hash;
            mismatch = true;
        }
        if (entry->second.isnonzero() && (entry->second != hash))
        {
            writelog (lserror, ledgermaster) << "mismatch: seq=" << index << " validated:" << entry->second << " accepted:" << hash;
            mismatch = true;
        }

        if (mismatch)
            handlemismatch (hash, entry->first);

        entry->first = hash;
    }
}

void ledgerhistory::validatedledger (ledger::ref ledger)
{
    ledgerindex index = ledger->getledgerseq();
    ledgerhash hash = ledger->gethash();
    assert (!hash.iszero());
    consensusvalidated::scopedlocktype sl (
        m_consensus_validated.peekmutex());

    std::shared_ptr< std::pair< ledgerhash, ledgerhash > > entry = std::make_shared<std::pair< ledgerhash, ledgerhash >>();
    m_consensus_validated.canonicalize(index, entry, false);

    if (entry->second != hash)
    {
        bool mismatch (false);

        if (entry->second.isnonzero() && (entry->second != hash))
        {
            writelog (lserror, ledgermaster) << "mismatch: seq=" << index << " validated:" << entry->second << " then:" << hash;
            mismatch = true;
        }

        if (entry->first.isnonzero() && (entry->first != hash))
        {
            writelog (lserror, ledgermaster) << "mismatch: seq=" << index << " built:" << entry->first << " validated:" << hash;
            mismatch = true;
        }

        if (mismatch)
            handlemismatch (entry->second, hash);

        entry->second = hash;
    }
}

/** ensure m_ledgers_by_hash doesn't have the wrong hash for a particular index
*/
bool ledgerhistory::fixindex (ledgerindex ledgerindex, ledgerhash const& ledgerhash)
{
    ledgersbyhash::scopedlocktype sl (m_ledgers_by_hash.peekmutex ());
    std::map<std::uint32_t, uint256>::iterator it (mledgersbyindex.find (ledgerindex));

    if ((it != mledgersbyindex.end ()) && (it->second != ledgerhash) )
    {
        it->second = ledgerhash;
        return false;
    }
    return true;
}

void ledgerhistory::tune (int size, int age)
{
    m_ledgers_by_hash.settargetsize (size);
    m_ledgers_by_hash.settargetage (age);
}

void ledgerhistory::clearledgercacheprior (ledgerindex seq)
{
    for (ledgerhash it: m_ledgers_by_hash.getkeys())
    {
        if (getledgerbyhash (it)->getledgerseq() < seq)
            m_ledgers_by_hash.del (it, false);
    }
}

} // ripple
