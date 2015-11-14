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
#include <ripple/app/ledger/inboundledgers.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/basics/log.h>
#include <ripple/core/jobqueue.h>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

class inboundledgersimp
    : public inboundledgers
    , public beast::stoppable
    , public beast::leakchecked <inboundledgers>
{
public:
    typedef std::pair<uint256, inboundledger::pointer> u256_acq_pair;
    // how long before we try again to acquire the same ledger
    static const int kreacquireintervalseconds = 300;

    inboundledgersimp (clock_type& clock, stoppable& parent,
                       beast::insight::collector::ptr const& collector)
        : stoppable ("inboundledgers", parent)
        , m_clock (clock)
        , mrecentfailures ("ledgeracquirerecentfailures",
            clock, 0, kreacquireintervalseconds)
        , mcounter(collector->make_counter("ledger_fetches"))
    {
    }

    // vfalco todo should this be called findoradd ?
    //
    inboundledger::pointer findcreate (uint256 const& hash, std::uint32_t seq, inboundledger::fcreason reason)
    {
        assert (hash.isnonzero ());
        inboundledger::pointer ret;

        // ensure that any previous il is destroyed outside the lock
        inboundledger::pointer oldledger;

        {
            scopedlocktype sl (mlock);

            if (! isstopping ())
            {

                if (reason == inboundledger::fcconsensus)
                {
                    if (mconsensusledger.isnonzero() && (mvalidationledger != mconsensusledger) && (hash != mconsensusledger))
                    {
                        hash_map<uint256, inboundledger::pointer>::iterator it = mledgers.find (mconsensusledger);
                        if (it != mledgers.end ())
                        {
                            oldledger = it->second;
                            mledgers.erase (it);
                        }
                    }
                    mconsensusledger = hash;
                }
                else if (reason == inboundledger::fcvalidation)
                {
                    if (mvalidationledger.isnonzero() && (mvalidationledger != mconsensusledger) && (hash != mvalidationledger))
                    {
                        hash_map<uint256, inboundledger::pointer>::iterator it = mledgers.find (mvalidationledger);
                        if (it != mledgers.end ())
                        {
                            oldledger = it->second;
                            mledgers.erase (it);
                       }
                    }
                    mvalidationledger = hash;
                }

                hash_map<uint256, inboundledger::pointer>::iterator it = mledgers.find (hash);
                if (it != mledgers.end ())
                {
                    ret = it->second;
                    // fixme: should set the sequence if it's not set
                }
                else
                {
                    ret = std::make_shared <inboundledger> (hash, seq, reason, std::ref (m_clock));
                    assert (ret);
                    mledgers.insert (std::make_pair (hash, ret));
                    ret->init (sl);
                    ++mcounter;
                }
            }
        }

        return ret;
    }

    inboundledger::pointer find (uint256 const& hash)
    {
        assert (hash.isnonzero ());

        inboundledger::pointer ret;

        {
            scopedlocktype sl (mlock);

            hash_map<uint256, inboundledger::pointer>::iterator it = mledgers.find (hash);
            if (it != mledgers.end ())
            {
                ret = it->second;
            }
        }

        return ret;
    }

    bool hasledger (ledgerhash const& hash)
    {
        assert (hash.isnonzero ());

        scopedlocktype sl (mlock);
        return mledgers.find (hash) != mledgers.end ();
    }

    void dropledger (ledgerhash const& hash)
    {
        assert (hash.isnonzero ());

        scopedlocktype sl (mlock);
        mledgers.erase (hash);

    }

    /*
    this gets called when
        "we got some data from an inbound ledger"

    inboundledgertrigger:
      "what do we do with this partial data?"
      figures out what to do with the responses to our requests for information.

    */
    // means "we got some data from an inbound ledger"

    // vfalco todo why is hash passed by value?
    // vfalco todo remove the dependency on the peer object.
    /** we received a tmledgerdata from a peer.
    */
    bool gotledgerdata (ledgerhash const& hash,
            std::shared_ptr<peer> peer,
            std::shared_ptr<protocol::tmledgerdata> packet_ptr)
    {
        protocol::tmledgerdata& packet = *packet_ptr;

        writelog (lstrace, inboundledger) << "got data (" << packet.nodes ().size () << ") for acquiring ledger: " << hash;

        inboundledger::pointer ledger = find (hash);

        if (!ledger)
        {
            writelog (lstrace, inboundledger) << "got data for ledger we're no longer acquiring";

            // if it's state node data, stash it because it still might be useful
            if (packet.type () == protocol::lias_node)
            {
                getapp().getjobqueue().addjob(jtledger_data, "gotstaledata",
                    std::bind(&inboundledgers::gotstaledata, this, packet_ptr));
            }

            return false;
        }

        // stash the data for later processing and see if we need to dispatch
        if (ledger->gotdata(std::weak_ptr<peer>(peer), packet_ptr))
            getapp().getjobqueue().addjob (jtledger_data, "processledgerdata",
                std::bind (&inboundledgers::doledgerdata, this,
                           std::placeholders::_1, hash));

        return true;
    }

    int getfetchcount (int& timeoutcount)
    {
        timeoutcount = 0;
        int ret = 0;

        std::vector<u256_acq_pair> inboundledgers;

        {
            scopedlocktype sl (mlock);

            inboundledgers.reserve(mledgers.size());
            for (auto const& it : mledgers)
            {
                inboundledgers.push_back(it);
            }
        }

        for (auto const& it : inboundledgers)
        {
            if (it.second->isactive ())
            {
                ++ret;
                timeoutcount += it.second->gettimeouts ();
            }
        }
        return ret;
    }

    void logfailure (uint256 const& h)
    {
        mrecentfailures.insert (h);
    }

    bool isfailure (uint256 const& h)
    {
        return mrecentfailures.exists (h);
    }

    void doledgerdata (job&, ledgerhash hash)
    {
        inboundledger::pointer ledger = find (hash);

        if (ledger)
            ledger->rundata ();
    }

    /** we got some data for a ledger we are no longer acquiring
        since we paid the price to receive it, we might as well stash it in case we need it.
        nodes are received in wire format and must be stashed/hashed in prefix format
    */
    void gotstaledata (std::shared_ptr<protocol::tmledgerdata> packet_ptr)
    {
        const uint256 uzero;
        serializer s;
        try
        {
            for (int i = 0; i < packet_ptr->nodes ().size (); ++i)
            {
                auto const& node = packet_ptr->nodes (i);

                if (!node.has_nodeid () || !node.has_nodedata ())
                    return;

                shamaptreenode newnode(
                    blob (node.nodedata().begin(), node.nodedata().end()),
                    0, snfwire, uzero, false);

                s.erase();
                newnode.addraw(s, snfprefix);

                auto blob = std::make_shared<blob> (s.begin(), s.end());

                getapp().getops().addfetchpack (newnode.getnodehash(), blob);
            }
        }
        catch (...)
        {
        }
    }

    void clearfailures ()
    {
        scopedlocktype sl (mlock);

        mrecentfailures.clear();
        mledgers.clear();
    }

    json::value getinfo()
    {
        json::value ret(json::objectvalue);

        std::vector<u256_acq_pair> acquires;
        {
            scopedlocktype sl (mlock);

            acquires.reserve (mledgers.size ());
            for (auto const& it : mledgers)
            {
                assert (it.second);
                acquires.push_back (it);
            }
        }

        for (auto const& it : acquires)
        {
            std::uint32_t seq = it.second->getseq();
            if (seq > 1)
                ret[beast::lexicalcastthrow <std::string>(seq)] = it.second->getjson(0);
            else
                ret[to_string (it.first)] = it.second->getjson(0);
        }

    return ret;
    }

    void gotfetchpack (job&)
    {
        std::vector<inboundledger::pointer> acquires;
        {
            scopedlocktype sl (mlock);

            acquires.reserve (mledgers.size ());
            for (auto const& it : mledgers)
            {
                assert (it.second);
                acquires.push_back (it.second);
            }
        }

        for (auto const& acquire : acquires)
        {
            acquire->checklocal ();
        }
    }

    void sweep ()
    {
        mrecentfailures.sweep ();

        clock_type::time_point const now (m_clock.now());

        // make a list of things to sweep, while holding the lock
        std::vector <maptype::mapped_type> stufftosweep;
        std::size_t total;
        {
            scopedlocktype sl (mlock);
            maptype::iterator it (mledgers.begin ());
            total = mledgers.size ();
            stufftosweep.reserve (total);

            while (it != mledgers.end ())
            {
                if (it->second->getlastaction () > now)
                {
                    it->second->touch ();
                    ++it;
                }
                else if ((it->second->getlastaction () + std::chrono::minutes (1)) < now)
                {
                    stufftosweep.push_back (it->second);
                    // shouldn't cause the actual final delete
                    // since we are holding a reference in the vector.
                    it = mledgers.erase (it);
                }
                else
                {
                    ++it;
                }
            }
        }

        writelog (lsdebug, inboundledger) <<
            "swept " << stufftosweep.size () <<
            " out of " << total << " inbound ledgers.";
    }

    void onstop ()
    {
        scopedlocktype lock (mlock);

        mledgers.clear();
        mrecentfailures.clear();

        stopped();
    }

private:
    clock_type& m_clock;

    typedef hash_map <uint256, inboundledger::pointer> maptype;

    typedef ripplerecursivemutex locktype;
    typedef std::unique_lock <locktype> scopedlocktype;
    locktype mlock;

    maptype mledgers;
    keycache <uint256> mrecentfailures;

    uint256 mconsensusledger;
    uint256 mvalidationledger;

    beast::insight::counter mcounter;
};

//------------------------------------------------------------------------------

inboundledgers::~inboundledgers()
{
}

std::unique_ptr<inboundledgers>
make_inboundledgers (inboundledgers::clock_type& clock, beast::stoppable& parent,
                     beast::insight::collector::ptr const& collector)
{
    return std::make_unique<inboundledgersimp> (clock, parent, collector);
}

} // ripple
