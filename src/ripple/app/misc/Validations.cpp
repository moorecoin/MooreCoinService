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
#include <ripple/app/misc/validations.h>
#include <ripple/app/data/databasecon.h>
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/ledger/ledgertiming.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/app/peers/uniquenodelist.h>
#include <ripple/basics/log.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/basics/seconds_clock.h>
#include <ripple/core/jobqueue.h>
#include <beast/cxx14/memory.h> // <memory>
#include <mutex>
#include <thread>

namespace ripple {

class validationsimp : public validations
{
private:
    using locktype = std::mutex;
    typedef std::lock_guard <locktype> scopedlocktype;
    typedef beast::genericscopedunlock <locktype> scopedunlocktype;
    std::mutex mutable mlock;

    taggedcache<uint256, validationset> mvalidations;
    validationset mcurrentvalidations;
    validationvector mstalevalidations;

    bool mwriting;

private:
    std::shared_ptr<validationset> findcreateset (uint256 const& ledgerhash)
    {
        auto j = mvalidations.fetch (ledgerhash);

        if (!j)
        {
            j = std::make_shared<validationset> ();
            mvalidations.canonicalize (ledgerhash, j);
        }

        return j;
    }

    std::shared_ptr<validationset> findset (uint256 const& ledgerhash)
    {
        return mvalidations.fetch (ledgerhash);
    }

public:
    validationsimp ()
        : mvalidations ("validations", 128, 600, get_seconds_clock (),
            deprecatedlogs().journal("taggedcache"))
        , mwriting (false)
    {
        mstalevalidations.reserve (512);
    }

private:
    bool addvalidation (stvalidation::ref val, std::string const& source)
    {
        rippleaddress signer = val->getsignerpublic ();
        bool iscurrent = false;

        if (!val->istrusted() && getapp().getunl().nodeinunl (signer))
            val->settrusted();

        std::uint32_t now = getapp().getops().getclosetimenc();
        std::uint32_t valclose = val->getsigntime();

        if ((now > (valclose - ledger_early_interval)) && (now < (valclose + ledger_val_interval)))
            iscurrent = true;
        else
            writelog (lswarning, validations) << "received stale validation now=" << now << ", close=" << valclose;

        if (!val->istrusted ())
        {
            writelog (lsdebug, validations) << "node " << signer.humannodepublic () << " not in unl st=" << val->getsigntime () <<
                                            ", hash=" << val->getledgerhash () << ", shash=" << val->getsigninghash () << " src=" << source;
        }

        auto hash = val->getledgerhash ();
        auto node = signer.getnodeid ();

        if (val->istrusted () && iscurrent)
        {
            scopedlocktype sl (mlock);

            if (!findcreateset (hash)->insert (std::make_pair (node, val)).second)
                return false;

            auto it = mcurrentvalidations.find (node);

            if (it == mcurrentvalidations.end ())
            {
                // no previous validation from this validator
                mcurrentvalidations.emplace (node, val);
            }
            else if (!it->second)
            {
                // previous validation has expired
                it->second = val;
            }
            else if (val->getsigntime () > it->second->getsigntime ())
            {
                // this is a newer validation
                val->setprevioushash (it->second->getledgerhash ());
                mstalevalidations.push_back (it->second);
                it->second = val;
                condwrite ();
            }
            else
            {
                // we already have a newer validation from this source
                iscurrent = false;
            }
        }

        writelog (lsdebug, validations) << "val for " << hash << " from " << signer.humannodepublic ()
                                        << " added " << (val->istrusted () ? "trusted/" : "untrusted/") << (iscurrent ? "current" : "stale");

        if (val->istrusted () && iscurrent)
        {
            getapp().getledgermaster ().checkaccept (hash, val->getfieldu32 (sfledgersequence));
            return true;
        }

        // fixme: this never forwards untrusted validations
        return false;
    }

    void tune (int size, int age)
    {
        mvalidations.settargetsize (size);
        mvalidations.settargetage (age);
    }

    validationset getvalidations (uint256 const& ledger)
    {
        {
            scopedlocktype sl (mlock);
            auto set = findset (ledger);

            if (set)
                return *set;
        }
        return validationset ();
    }

    void getvalidationcount (uint256 const& ledger, bool currentonly, int& trusted, int& untrusted)
    {
        trusted = untrusted = 0;
        scopedlocktype sl (mlock);
        auto set = findset (ledger);

        if (set)
        {
            std::uint32_t now = getapp().getops ().getnetworktimenc ();
            for (auto& it: *set)
            {
                bool istrusted = it.second->istrusted ();

                if (istrusted && currentonly)
                {
                    std::uint32_t closetime = it.second->getsigntime ();

                    if ((now < (closetime - ledger_early_interval)) || (now > (closetime + ledger_val_interval)))
                        istrusted = false;
                    else
                    {
                        writelog (lstrace, validations) << "vc: untrusted due to time " << ledger;
                    }
                }

                if (istrusted)
                    ++trusted;
                else
                    ++untrusted;
            }
        }

        writelog (lstrace, validations) << "vc: " << ledger << "t:" << trusted << " u:" << untrusted;
    }

    void getvalidationtypes (uint256 const& ledger, int& full, int& partial)
    {
        full = partial = 0;
        scopedlocktype sl (mlock);
        auto set = findset (ledger);

        if (set)
        {
            for (auto& it:*set)
            {
                if (it.second->istrusted ())
                {
                    if (it.second->isfull ())
                        ++full;
                    else
                        ++partial;
                }
            }
        }

        writelog (lstrace, validations) << "vc: " << ledger << "f:" << full << " p:" << partial;
    }


    int gettrustedvalidationcount (uint256 const& ledger)
    {
        int trusted = 0;
        scopedlocktype sl (mlock);
        auto set = findset (ledger);

        if (set)
        {
            for (auto& it: *set)
            {
                if (it.second->istrusted ())
                    ++trusted;
            }
        }

        return trusted;
    }

    std::vector <std::uint64_t>
    fees (uint256 const& ledger, std::uint64_t base) override
    {
        std::vector <std::uint64_t> result;
        std::lock_guard <std::mutex> lock (mlock);
        auto const set = findset (ledger);
        if (set)
        {
            for (auto const& v : *set)
            {
                if (v.second->istrusted())
                {
                    if (v.second->isfieldpresent(sfloadfee))
                        result.push_back(v.second->getfieldu32(sfloadfee));
                    else
                        result.push_back(base);
                }
            }
        }

        return result;
    }

    int getnodesafter (uint256 const& ledger)
    {
        // number of trusted nodes that have moved past this ledger
        int count = 0;
        scopedlocktype sl (mlock);
        for (auto& it: mcurrentvalidations)
        {
            if (it.second->istrusted () && it.second->isprevioushash (ledger))
                ++count;
        }
        return count;
    }

    int getloadratio (bool overloaded)
    {
        // how many trusted nodes are able to keep up, higher is better
        int goodnodes = overloaded ? 1 : 0;
        int badnodes = overloaded ? 0 : 1;
        {
            scopedlocktype sl (mlock);
            for (auto& it: mcurrentvalidations)
            {
                if (it.second->istrusted ())
                {
                    if (it.second->isfull ())
                        ++goodnodes;
                    else
                        ++badnodes;
                }
            }
        }
        return (goodnodes * 100) / (goodnodes + badnodes);
    }

    std::list<stvalidation::pointer> getcurrenttrustedvalidations ()
    {
        std::uint32_t cutoff = getapp().getops ().getnetworktimenc () - ledger_val_interval;

        std::list<stvalidation::pointer> ret;

        scopedlocktype sl (mlock);
        auto it = mcurrentvalidations.begin ();

        while (it != mcurrentvalidations.end ())
        {
            if (!it->second) // contains no record
                it = mcurrentvalidations.erase (it);
            else if (it->second->getsigntime () < cutoff)
            {
                // contains a stale record
                mstalevalidations.push_back (it->second);
                it->second.reset ();
                condwrite ();
                it = mcurrentvalidations.erase (it);
            }
            else
            {
                // contains a live record
                if (it->second->istrusted ())
                    ret.push_back (it->second);

                ++it;
            }
        }

        return ret;
    }

    ledgertovalidationcounter getcurrentvalidations (
        uint256 currentledger, uint256 priorledger)
    {
        std::uint32_t cutoff = getapp().getops ().getnetworktimenc () - ledger_val_interval;
        bool valcurrentledger = currentledger.isnonzero ();
        bool valpriorledger = priorledger.isnonzero ();

        ledgertovalidationcounter ret;

        scopedlocktype sl (mlock);
        auto it = mcurrentvalidations.begin ();

        while (it != mcurrentvalidations.end ())
        {
            if (!it->second) // contains no record
                it = mcurrentvalidations.erase (it);
            else if (it->second->getsigntime () < cutoff)
            {
                // contains a stale record
                mstalevalidations.push_back (it->second);
                it->second.reset ();
                condwrite ();
                it = mcurrentvalidations.erase (it);
            }
            else
            {
                // contains a live record
                bool countpreferred = valcurrentledger && (it->second->getledgerhash () == currentledger);

                if (!countpreferred && // allow up to one ledger slip in either direction
                        ((valcurrentledger && it->second->isprevioushash (currentledger)) ||
                         (valpriorledger && (it->second->getledgerhash () == priorledger))))
                {
                    countpreferred = true;
                    writelog (lstrace, validations) << "counting for " << currentledger << " not " << it->second->getledgerhash ();
                }

                validationcounter& p = countpreferred ? ret[currentledger] : ret[it->second->getledgerhash ()];
                ++ (p.first);
                auto ni = it->second->getnodeid ();

                if (ni > p.second)
                    p.second = ni;

                ++it;
            }
        }

        return ret;
    }

    void flush ()
    {
        bool anynew = false;

        writelog (lsinfo, validations) << "flushing validations";
        scopedlocktype sl (mlock);
        for (auto& it: mcurrentvalidations)
        {
            if (it.second)
                mstalevalidations.push_back (it.second);

            anynew = true;
        }
        mcurrentvalidations.clear ();

        if (anynew)
            condwrite ();

        while (mwriting)
        {
            scopedunlocktype sul (mlock);
            std::this_thread::sleep_for (std::chrono::milliseconds (100));
        }

        writelog (lsdebug, validations) << "validations flushed";
    }

    void condwrite ()
    {
        if (mwriting)
            return;

        mwriting = true;
        getapp().getjobqueue ().addjob (jtwrite, "validations::dowrite",
                                       std::bind (&validationsimp::dowrite,
                                                  this, std::placeholders::_1));
    }

    void dowrite (job&)
    {
        loadevent::autoptr event (getapp().getjobqueue ().getloadeventap (jtdisk, "validationwrite"));
        boost::format insval ("insert into validations "
                              "(ledgerhash,nodepubkey,signtime,rawdata) values ('%s','%s','%u',%s);");

        scopedlocktype sl (mlock);
        assert (mwriting);

        while (!mstalevalidations.empty ())
        {
            validationvector vector;
            vector.reserve (512);
            mstalevalidations.swap (vector);

            {
                scopedunlocktype sul (mlock);
                {
                    auto db = getapp().getledgerdb ().getdb ();
                    auto dbl (getapp().getledgerdb ().lock ());

                    serializer s (1024);
//                    db->executesql ("begin transaction;");
                    db->begintransaction();
                    for (auto it: vector)
                    {
                        s.erase ();
                        it->add (s);
                        db->executesql (boost::str (
                            insval % to_string (it->getledgerhash ()) %
                            it->getsignerpublic ().humannodepublic () %
                            it->getsigntime () % sqlescape (s.peekdata ())));
                    }
//                    db->executesql ("end transaction;");
                    db->endtransaction();
                }
            }
        }

        mwriting = false;
    }

    void sweep ()
    {
        scopedlocktype sl (mlock);
        mvalidations.sweep ();
    }
};

std::unique_ptr <validations> make_validations ()
{
    return std::make_unique <validationsimp> ();
}

} // ripple
