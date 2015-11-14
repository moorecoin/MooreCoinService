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
#include <ripple/app/misc/amendmenttable.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/validations.h>
#include <ripple/app/data/databasecon.h>
#include <boost/format.hpp>

namespace ripple {

/** track the list of "amendments"

    an "amendment" is an option that can affect transaction processing
    rules that is identified by a 256-bit amendment identifier
    and adopted, or rejected, by the network.
*/
class amendmenttableimpl : public amendmenttable
{
protected:
    typedef hash_map<uint256, amendmentstate> amendmentmap_t;
    typedef std::pair<const uint256, amendmentstate> amendmentit_t;
    typedef hash_set<uint256> amendmentlist_t;

    typedef ripplemutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;
    locktype mlock;

    amendmentmap_t m_amendmentmap;
    std::chrono::seconds m_majoritytime; // seconds an amendment must hold a majority
    int mmajorityfraction;  // 256 = 100%
    core::clock::time_point m_firstreport; // close time of first majority report
    core::clock::time_point m_lastreport;  // close time of most recent majority report
    beast::journal m_journal;

    amendmentstate* getcreate (uint256 const& amendment, bool create);
    bool shouldenable (std::uint32_t closetime, const amendmentstate& fs);
    void setjson (json::value& v, const amendmentstate&);

public:
    amendmenttableimpl (std::chrono::seconds majoritytime, int majorityfraction,
            beast::journal journal)
        : m_majoritytime (majoritytime)
        , mmajorityfraction (majorityfraction)
        , m_firstreport (0)
        , m_lastreport (0)
        , m_journal (journal)
    {
    }

    void addinitial () override;

    amendmentstate* addknown (const char* amendmentid, const char* friendlyname,
        bool veto) override;
    uint256 get (std::string const& name) override;

    bool veto (uint256 const& amendment) override;
    bool unveto (uint256 const& amendment) override;

    bool enable (uint256 const& amendment) override;
    bool disable (uint256 const& amendment) override;

    bool isenabled (uint256 const& amendment) override;
    bool issupported (uint256 const& amendment) override;

    void setenabled (const std::vector<uint256>& amendments) override;
    void setsupported (const std::vector<uint256>& amendments) override;

    void reportvalidations (const amendmentset&) override;

    json::value getjson (int) override;
    json::value getjson (uint256 const&) override;

    void dovalidation (ledger::ref lastclosedledger, stobject& basevalidation) override;
    void dovoting (ledger::ref lastclosedledger, shamap::ref initialposition) override;

    amendmentlist_t getvetoed();
    amendmentlist_t getenabled();
    amendmentlist_t gettoenable(core::clock::time_point closetime);   // gets amendments we would vote to enable
    amendmentlist_t getdesired();    // amendments we support, do not veto, are not enabled
};

void
amendmenttableimpl::addinitial ()
{
    // for each amendment this version supports, construct the amendmentstate object by calling
    // addknown. set any vetoes or defaults. a pointer to the amendmentstate can be stashed
}

amendmentstate*
amendmenttableimpl::getcreate (uint256 const& amendmenthash, bool create)
{
    // call with the mutex held
    auto iter (m_amendmentmap.find (amendmenthash));

    if (iter == m_amendmentmap.end())
    {
        if (!create)
            return nullptr;

        amendmentstate* amendment = & (m_amendmentmap[amendmenthash]);

        {
            std::string query = "select firstmajority,lastmajority from features where hash='";
            query.append (to_string (amendmenthash));
            query.append ("';");

            auto sl (getapp().getwalletdb ().lock ());
            auto db = getapp().getwalletdb ().getdb ();

            if (db->executesql (query) && db->startiterrows ())
            {
                amendment->m_firstmajority = db->getbigint("firstmajority");
                amendment->m_lastmajority = db->getbigint("lastmajority");
                db->enditerrows ();
            }
        }

        return amendment;
    }

    return & (iter->second);
}

uint256
amendmenttableimpl::get (std::string const& name)
{
    for (auto const& e : m_amendmentmap)
    {
        if (name == e.second.mfriendlyname)
            return e.first;
    }

    return uint256 ();
}

amendmentstate*
amendmenttableimpl::addknown (const char* amendmentid, const char* friendlyname,
    bool veto)
{
    uint256 hash;
    hash.sethex (amendmentid);

    if (hash.iszero ())
    {
        assert (false);
        return nullptr;
    }

    amendmentstate* f = getcreate (hash, true);

    if (friendlyname != nullptr)
        f->setfriendlyname (friendlyname);

    f->mvetoed = veto;
    f->msupported = true;

    return f;
}

bool
amendmenttableimpl::veto (uint256 const& amendment)
{
    scopedlocktype sl (mlock);
    amendmentstate* s = getcreate (amendment, true);

    if (s->mvetoed)
        return false;

    s->mvetoed = true;
    return true;
}

bool
amendmenttableimpl::unveto (uint256 const& amendment)
{
    scopedlocktype sl (mlock);
    amendmentstate* s = getcreate (amendment, false);

    if (!s || !s->mvetoed)
        return false;

    s->mvetoed = false;
    return true;
}

bool
amendmenttableimpl::enable (uint256 const& amendment)
{
    scopedlocktype sl (mlock);
    amendmentstate* s = getcreate (amendment, true);

    if (s->menabled)
        return false;

    s->menabled = true;
    return true;
}

bool
amendmenttableimpl::disable (uint256 const& amendment)
{
    scopedlocktype sl (mlock);
    amendmentstate* s = getcreate (amendment, false);

    if (!s || !s->menabled)
        return false;

    s->menabled = false;
    return true;
}

bool
amendmenttableimpl::isenabled (uint256 const& amendment)
{
    scopedlocktype sl (mlock);
    amendmentstate* s = getcreate (amendment, false);
    return s && s->menabled;
}

bool
amendmenttableimpl::issupported (uint256 const& amendment)
{
    scopedlocktype sl (mlock);
    amendmentstate* s = getcreate (amendment, false);
    return s && s->msupported;
}

amendmenttableimpl::amendmentlist_t
amendmenttableimpl::getvetoed ()
{
    amendmentlist_t ret;
    scopedlocktype sl (mlock);
    for (auto const& e : m_amendmentmap)
    {
        if (e.second.mvetoed)
            ret.insert (e.first);
    }
    return ret;
}

amendmenttableimpl::amendmentlist_t
amendmenttableimpl::getenabled ()
{
    amendmentlist_t ret;
    scopedlocktype sl (mlock);
    for (auto const& e : m_amendmentmap)
    {
        if (e.second.menabled)
            ret.insert (e.first);
    }
    return ret;
}

bool
amendmenttableimpl::shouldenable (std::uint32_t closetime,
    const amendmentstate& fs)
{
    if (fs.mvetoed || fs.menabled || !fs.msupported || (fs.m_lastmajority != m_lastreport))
        return false;

    if (fs.m_firstmajority == m_firstreport)
    {
        // had a majority when we first started the server, relaxed check
        // writeme
    }

    // didn't have a majority when we first started the server, normal check
    return (fs.m_lastmajority - fs.m_firstmajority) > m_majoritytime.count();
}

amendmenttableimpl::amendmentlist_t
amendmenttableimpl::gettoenable (core::clock::time_point closetime)
{
    amendmentlist_t ret;
    scopedlocktype sl (mlock);

    if (m_lastreport != 0)
    {
        for (auto const& e : m_amendmentmap)
        {
            if (shouldenable (closetime, e.second))
                ret.insert (e.first);
        }
    }

    return ret;
}

amendmenttableimpl::amendmentlist_t
amendmenttableimpl::getdesired ()
{
    amendmentlist_t ret;
    scopedlocktype sl (mlock);

    for (auto const& e : m_amendmentmap)
    {
        if (e.second.msupported && !e.second.menabled && !e.second.mvetoed)
            ret.insert (e.first);
    }

    return ret;
}

void
amendmenttableimpl::reportvalidations (const amendmentset& set)
{
    if (set.mtrustedvalidations == 0)
        return;

    int threshold = (set.mtrustedvalidations * mmajorityfraction) / 256;

    typedef std::map<uint256, int>::value_type u256_int_pair;

    scopedlocktype sl (mlock);

    if (m_firstreport == 0)
        m_firstreport = set.mclosetime;

    std::vector<uint256> changedamendments;
    changedamendments.resize(set.mvotes.size());

    for (auto const& e : set.mvotes)
    {
        amendmentstate& state = m_amendmentmap[e.first];
        if (m_journal.debug) m_journal.debug <<
            "amendment " << to_string (e.first) <<
            " has " << e.second <<
            " votes, needs " << threshold;

        if (e.second >= threshold)
        {
            // we have a majority
            state.m_lastmajority = set.mclosetime;

            if (state.m_firstmajority == 0)
            {
                if (m_journal.warning) m_journal.warning <<
                    "amendment " << to_string (e.first) <<
                    " attains a majority vote";

                state.m_firstmajority = set.mclosetime;
                changedamendments.push_back(e.first);
            }
        }
        else // we have no majority
        {
            if (state.m_firstmajority != 0)
            {
                if (m_journal.warning) m_journal.warning <<
                    "amendment " << to_string (e.first) <<
                    " loses majority vote";

                state.m_firstmajority = 0;
                state.m_lastmajority = 0;
                changedamendments.push_back(e.first);
            }
        }
    }
    m_lastreport = set.mclosetime;

    if (!changedamendments.empty())
    {
        auto sl (getapp().getwalletdb ().lock ());
        auto db = getapp().getwalletdb ().getdb ();

//        db->executesql ("begin transaction;");
        db->begintransaction();
        for (auto const& hash : changedamendments)
        {
            amendmentstate& fstate = m_amendmentmap[hash];
            db->executesql (boost::str (boost::format (
                "update features set firstmajority = %d where hash = '%s';") %
                fstate.m_firstmajority % to_string (hash)));
            db->executesql (boost::str (boost::format (
                "update features set lastmajority = %d where hash = '%s';") %
                fstate.m_lastmajority % to_string(hash)));
        }
//        db->executesql ("end transaction;");
        db->endtransaction();
        changedamendments.clear();
    }
}

void
amendmenttableimpl::setenabled (const std::vector<uint256>& amendments)
{
    scopedlocktype sl (mlock);
    for (auto& e : m_amendmentmap)
    {
        e.second.menabled = false;
    }
    for (auto const& e : amendments)
    {
        m_amendmentmap[e].menabled = true;
    }
}

void
amendmenttableimpl::setsupported (const std::vector<uint256>& amendments)
{
    scopedlocktype sl (mlock);
    for (auto &e : m_amendmentmap)
    {
        e.second.msupported = false;
    }
    for (auto const& e : amendments)
    {
        m_amendmentmap[e].msupported = true;
    }
}

void
amendmenttableimpl::dovalidation (ledger::ref lastclosedledger,
    stobject& basevalidation)
{
    amendmentlist_t lamendments = getdesired();

    if (lamendments.empty())
        return;

    stvector256 vamendments (sfamendments);
    for (auto const& uamendment : lamendments)
        vamendments.push_back (uamendment);
    vamendments.sort ();
    basevalidation.setfieldv256 (sfamendments, vamendments);
}

void
amendmenttableimpl::dovoting (ledger::ref lastclosedledger,
    shamap::ref initialposition)
{

    // lcl must be flag ledger
    assert((lastclosedledger->getledgerseq () % 256) == 0);

    amendmentset amendmentset (lastclosedledger->getparentclosetimenc ());

    // get validations for ledger before flag ledger
    validationset valset = getapp().getvalidations ().getvalidations (lastclosedledger->getparenthash ());
    for (auto const& entry : valset)
    {
        auto const& val = *entry.second;

        if (val.istrusted ())
        {
            amendmentset.addvoter ();
            if (val.isfieldpresent (sfamendments))
            {
                for (auto const& amendment : val.getfieldv256 (sfamendments))
                {
                    amendmentset.addvote (amendment);
                }
            }
        }
    }
    reportvalidations (amendmentset);

    amendmentlist_t lamendments = gettoenable (lastclosedledger->getclosetimenc ());
    for (auto const& uamendment : lamendments)
    {
        if (m_journal.warning) m_journal.warning <<
            "voting for amendment: " << uamendment;

        // create the transaction to enable the amendment
        sttx trans (ttamendment);
        trans.setfieldaccount (sfaccount, account ());
        trans.setfieldh256 (sfamendment, uamendment);
        uint256 txid = trans.gettransactionid ();

        if (m_journal.warning) m_journal.warning <<
            "vote id: " << txid;

        // inject the transaction into our initial proposal
        serializer s;
        trans.add (s, true);
#if ripple_propose_amendments
        shamapitem::pointer titem = std::make_shared<shamapitem> (txid, s.peekdata ());
        if (!initialposition->addgiveitem (titem, true, false))
        {
            if (m_journal.warning) m_journal.warning <<
                "ledger already had amendment transaction";
        }
#endif
    }
}

json::value
amendmenttableimpl::getjson (int)
{
    json::value ret(json::objectvalue);
    {
        scopedlocktype sl(mlock);
        for (auto const& e : m_amendmentmap)
        {
            setjson (ret[to_string (e.first)] = json::objectvalue, e.second);
        }
    }
    return ret;
}

void
amendmenttableimpl::setjson (json::value& v, const amendmentstate& fs)
{
    if (!fs.mfriendlyname.empty())
        v["name"] = fs.mfriendlyname;

    v["supported"] = fs.msupported;
    v["vetoed"] = fs.mvetoed;

    if (fs.menabled)
        v["enabled"] = true;
    else
    {
        v["enabled"] = false;

        if (m_lastreport != 0)
        {
            if (fs.m_lastmajority == 0)
            {
                v["majority"] = false;
            }
            else
            {
                if (fs.m_firstmajority != 0)
                {
                    if (fs.m_firstmajority == m_firstreport)
                        v["majority_start"] = "start";
                    else
                        v["majority_start"] = fs.m_firstmajority;
                }

                if (fs.m_lastmajority != 0)
                {
                    if (fs.m_lastmajority == m_lastreport)
                        v["majority_until"] = "now";
                    else
                        v["majority_until"] = fs.m_lastmajority;
                }
            }
        }
    }

    if (fs.mvetoed)
        v["veto"] = true;
}

json::value
amendmenttableimpl::getjson (uint256 const& amendmentid)
{
    json::value ret = json::objectvalue;
    json::value& jamendment = (ret[to_string (amendmentid)] = json::objectvalue);

    {
        scopedlocktype sl(mlock);

        amendmentstate *amendmentstate = getcreate (amendmentid, true);
        setjson (jamendment, *amendmentstate);
    }

    return ret;
}

std::unique_ptr<amendmenttable>
make_amendmenttable (std::chrono::seconds majoritytime, int majorityfraction,
    beast::journal journal)
{
    return std::make_unique<amendmenttableimpl> (majoritytime, majorityfraction,
        journal);
}

} // ripple
