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
#include <ripple/shamap/shamapnodeid.h>
#include <ripple/app/ledger/accountstatesf.h>
#include <ripple/app/ledger/inboundledger.h>
#include <ripple/app/ledger/inboundledgers.h>
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/ledger/transactionstatesf.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/basics/log.h>
#include <ripple/core/jobqueue.h>
#include <ripple/overlay/overlay.h>
#include <ripple/resource/fees.h>
#include <ripple/protocol/hashprefix.h>
#include <ripple/nodestore/database.h>
#include <boost/foreach.hpp>

namespace ripple {

enum
{
    // millisecond for each ledger timeout
    ledgeracquiretimeoutmillis = 2500

    // how many timeouts before we giveup
    ,ledgertimeoutretriesmax = 10

    // how many timeouts before we get aggressive
    ,ledgerbecomeaggressivethreshold = 6
};

inboundledger::inboundledger (uint256 const& hash, std::uint32_t seq, fcreason reason,
    clock_type& clock)
    : peerset (hash, ledgeracquiretimeoutmillis, false, clock,
        deprecatedlogs().journal("inboundledger"))
    , mhaveheader (false)
    , mhavestate (false)
    , mhavetransactions (false)
    , maborted (false)
    , msignaled (false)
    , mbyhash (true)
    , mseq (seq)
    , mreason (reason)
    , mreceivedispatched (false)
{

    if (m_journal.trace) m_journal.trace <<
        "acquiring ledger " << mhash;
}

bool inboundledger::checklocal ()
{
    scopedlocktype sl (mlock);

    if (!isdone () && trylocal())
    {
        done();
        return true;
    }
    return false;
}

inboundledger::~inboundledger ()
{
    // save any received as data not processed. it could be useful
    // for populating a different ledger
    for (auto& entry : mreceiveddata)
    {
        if (entry.second->type () == protocol::lias_node)
            getapp().getinboundledgers().gotstaledata(entry.second);
    }

}

void inboundledger::init (scopedlocktype& collectionlock)
{
    scopedlocktype sl (mlock);
    collectionlock.unlock ();

    if (!trylocal ())
    {
        addpeers ();
        settimer ();

        // for historical nodes, wait a bit since a
        // fetch pack is probably coming
        if (mreason != fchistory)
            trigger (peer::ptr ());
    }
    else if (!isfailed ())
    {
        if (m_journal.debug) m_journal.debug <<
            "acquiring ledger we already have locally: " << gethash ();
        mledger->setclosed ();
        mledger->setimmutable ();
        getapp ().getledgermaster ().storeledger (mledger);

        // check if this could be a newer fully-validated ledger
        if ((mreason == fcvalidation) || (mreason == fccurrent) || (mreason ==  fcconsensus))
            getapp ().getledgermaster ().checkaccept (mledger);
    }
}

/** see how much of the ledger data, if any, is
    in our node store
*/
bool inboundledger::trylocal ()
{
    // return value: true = no more work to do

    if (!mhaveheader)
    {
        // nothing we can do without the ledger header
        nodeobject::pointer node = getapp().getnodestore ().fetch (mhash);

        if (!node)
        {
            blob data;

            if (!getapp().getops ().getfetchpack (mhash, data))
                return false;

            if (m_journal.trace) m_journal.trace <<
                "ledger header found in fetch pack";
            mledger = std::make_shared<ledger> (data, true);
            getapp().getnodestore ().store (
                hotledger, std::move (data), mhash);
        }
        else
        {
            mledger = std::make_shared<ledger> (
                strcopy (node->getdata ()), true);
        }

        if (mledger->gethash () != mhash)
        {
            // we know for a fact the ledger can never be acquired
            if (m_journal.warning) m_journal.warning <<
                mhash << " cannot be a ledger";
            mfailed = true;
            return true;
        }

        mhaveheader = true;
    }

    if (!mhavetransactions)
    {
        if (mledger->gettranshash ().iszero ())
        {
            if (m_journal.trace) m_journal.trace <<
                "no txns to fetch";
            mhavetransactions = true;
        }
        else
        {
            transactionstatesf filter;

            if (mledger->peektransactionmap ()->fetchroot (
                mledger->gettranshash (), &filter))
            {
                auto h (mledger->getneededtransactionhashes (1, &filter));

                if (h.empty ())
                {
                    if (m_journal.trace) m_journal.trace <<
                        "had full txn map locally";
                    mhavetransactions = true;
                }
            }
        }
    }

    if (!mhavestate)
    {
        if (mledger->getaccounthash ().iszero ())
        {
            if (m_journal.fatal) m_journal.fatal <<
                "we are acquiring a ledger with a zero account hash";
            mfailed = true;
            return true;
        }
        else
        {
            accountstatesf filter;

            if (mledger->peekaccountstatemap ()->fetchroot (
                mledger->getaccounthash (), &filter))
            {
                auto h (mledger->getneededaccountstatehashes (1, &filter));

                if (h.empty ())
                {
                    if (m_journal.trace) m_journal.trace <<
                        "had full as map locally";
                    mhavestate = true;
                }
            }
        }
    }

    if (mhavetransactions && mhavestate)
    {
        if (m_journal.debug) m_journal.debug <<
            "had everything locally";
        mcomplete = true;
        mledger->setclosed ();
        mledger->setimmutable ();
    }

    return mcomplete;
}

/** called with a lock by the peerset when the timer expires
*/
void inboundledger::ontimer (bool wasprogress, scopedlocktype&)
{
    mrecenttxnodes.clear ();
    mrecentasnodes.clear ();

    if (isdone())
    {
        if (m_journal.info) m_journal.info <<
            "already done " << mhash;
        return;
    }

    if (gettimeouts () > ledgertimeoutretriesmax)
    {
        if (mseq != 0)
        {
            if (m_journal.warning) m_journal.warning <<
                gettimeouts() << " timeouts for ledger " << mseq;
        }
        else
        {
            if (m_journal.warning) m_journal.warning <<
                gettimeouts() << " timeouts for ledger " << mhash;
        }
        setfailed ();
        done ();
        return;
    }

    if (!wasprogress)
    {
        checklocal();

        maggressive = true;
        mbyhash = true;

        std::size_t pc = getpeercount ();
        writelog (lsdebug, inboundledger) <<
            "no progress(" << pc <<
            ") for ledger " << mhash;

        trigger (peer::ptr ());
        if (pc < 4)
            addpeers ();
    }
}

/** add more peers to the set, if possible */
void inboundledger::addpeers ()
{
    overlay::peersequence peerlist = getapp().overlay ().getactivepeers ();

    int vsize = peerlist.size ();

    if (vsize == 0)
    {
        writelog (lserror, inboundledger) <<
            "no peers to add for ledger acquisition";
        return;
    }

    // fixme-nikb why are we doing this convoluted thing here instead of simply
    // shuffling this vector and then pulling however many entries we need?

    // we traverse the peer list in random order so as not to favor
    // any particular peer
    //
    // vfalco use random_shuffle
    //        http://en.cppreference.com/w/cpp/algorithm/random_shuffle
    //
    int firstpeer = rand () % vsize;

    int found = 0;

    // first look for peers that are likely to have this ledger
    for (int i = 0; i < vsize; ++i)
    {
        peer::ptr const& peer = peerlist[ (i + firstpeer) % vsize];

        if (peer->hasledger (gethash (), mseq))
        {
           if (peerhas (peer) && (++found > 6))
               break;
        }
    }

    if (!found)
    { // oh well, try some random peers
        for (int i = 0; (i < 6) && (i < vsize); ++i)
        {
            if (peerhas (peerlist[ (i + firstpeer) % vsize]))
                ++found;
        }
        if (mseq != 0)
        {
            if (m_journal.debug) m_journal.debug <<
                "chose " << found << " peer(s) for ledger " << mseq;
        }
        else
        {
            if (m_journal.debug) m_journal.debug <<
                "chose " << found << " peer(s) for ledger " <<
                    to_string (gethash ());
        }
    }
    else if (mseq != 0)
    {
        if (m_journal.debug) m_journal.debug <<
            "found " << found << " peer(s) with ledger " << mseq;
    }
    else
    {
        if (m_journal.debug) m_journal.debug <<
            "found " << found << " peer(s) with ledger " <<
                to_string (gethash ());
    }
}

std::weak_ptr<peerset> inboundledger::pmdowncast ()
{
    return std::dynamic_pointer_cast<peerset> (shared_from_this ());
}

/** dispatch acquire completion
*/
static void ladispatch (
    job& job,
    inboundledger::pointer la,
    std::vector< std::function<void (inboundledger::pointer)> > trig)
{
    if (la->iscomplete() && !la->isfailed())
    {
        getapp().getledgermaster().checkaccept(la->getledger());
        getapp().getledgermaster().tryadvance();
    }
    for (unsigned int i = 0; i < trig.size (); ++i)
        trig[i] (la);
}

void inboundledger::done ()
{
    if (msignaled)
        return;

    msignaled = true;
    touch ();

    if (m_journal.trace) m_journal.trace <<
        "done acquiring ledger " << mhash;

    assert (iscomplete () || isfailed ());

    std::vector< std::function<void (inboundledger::pointer)> > triggers;
    {
        scopedlocktype sl (mlock);
        triggers.swap (moncomplete);
    }

    if (iscomplete () && !isfailed () && mledger)
    {
        mledger->setclosed ();
        mledger->setimmutable ();
        getapp().getledgermaster ().storeledger (mledger);
    }
    else
        getapp().getinboundledgers ().logfailure (mhash);

    // we hold the peerset lock, so must dispatch
    getapp().getjobqueue ().addjob (jtledger_data, "triggers",
        std::bind (ladispatch, std::placeholders::_1, shared_from_this (),
                   triggers));
}

bool inboundledger::addoncomplete (
    std::function <void (inboundledger::pointer)> triggerfunc)
{
    scopedlocktype sl (mlock);

    if (isdone ())
        return false;

    moncomplete.push_back (triggerfunc);
    return true;
}

/** request more nodes, perhaps from a specific peer
*/
void inboundledger::trigger (peer::ptr const& peer)
{
    scopedlocktype sl (mlock);

    if (isdone ())
    {
        if (m_journal.debug) m_journal.debug <<
            "trigger on ledger: " << mhash << (maborted ? " aborted" : "") <<
                (mcomplete ? " completed" : "") << (mfailed ? " failed" : "");
        return;
    }

    if (m_journal.trace)
    {
        if (peer)
            m_journal.trace <<
                "trigger acquiring ledger " << mhash << " from " << peer;
        else
            m_journal.trace <<
                "trigger acquiring ledger " << mhash;

        if (mcomplete || mfailed)
            m_journal.trace <<
                "complete=" << mcomplete << " failed=" << mfailed;
        else
            m_journal.trace <<
                "header=" << mhaveheader << " tx=" << mhavetransactions <<
                    " as=" << mhavestate;
    }

    if (!mhaveheader)
    {
        trylocal ();

        if (mfailed)
        {
            if (m_journal.warning) m_journal.warning <<
                " failed local for " << mhash;
            return;
        }
    }

    protocol::tmgetledger tmgl;
    tmgl.set_ledgerhash (mhash.begin (), mhash.size ());

    if (gettimeouts () != 0)
    { // be more aggressive if we've timed out at least once
        tmgl.set_querytype (protocol::qtindirect);

        if (!isprogress () && !mfailed && mbyhash && (
            gettimeouts () > ledgerbecomeaggressivethreshold))
        {
            auto need = getneededhashes ();

            if (!need.empty ())
            {
                protocol::tmgetobjectbyhash tmbh;
                tmbh.set_query (true);
                tmbh.set_ledgerhash (mhash.begin (), mhash.size ());
                bool typeset = false;
                for (auto& p : need)
                {
                    if (m_journal.warning) m_journal.warning
                        << "want: " << p.second;

                    if (!typeset)
                    {
                        tmbh.set_type (p.first);
                        typeset = true;
                    }

                    if (p.first == tmbh.type ())
                    {
                        protocol::tmindexedobject* io = tmbh.add_objects ();
                        io->set_hash (p.second.begin (), p.second.size ());
                    }
                }

                message::pointer packet (std::make_shared <message> (
                    tmbh, protocol::mtget_objects));
                {
                    scopedlocktype sl (mlock);

                    for (peersetmap::iterator it = mpeers.begin (), end = mpeers.end ();
                            it != end; ++it)
                    {
                        peer::ptr ipeer (
                            getapp().overlay ().findpeerbyshortid (it->first));

                        if (ipeer)
                        {
                            mbyhash = false;
                            ipeer->send (packet);
                        }
                    }
                }
                if (m_journal.info) m_journal.info <<
                    "attempting by hash fetch for ledger " << mhash;
            }
            else
            {
                if (m_journal.info) m_journal.info <<
                    "getneededhashes says acquire is complete";
                mhaveheader = true;
                mhavetransactions = true;
                mhavestate = true;
                mcomplete = true;
            }
        }
    }

    // we can't do much without the header data because we don't know the
    // state or transaction root hashes.
    if (!mhaveheader && !mfailed)
    {
        tmgl.set_itype (protocol::libase);
        if (m_journal.trace) m_journal.trace <<
            "sending header request to " << (peer ? "selected peer" : "all peers");
        sendrequest (tmgl, peer);
        return;
    }

    if (mledger)
        tmgl.set_ledgerseq (mledger->getledgerseq ());

    // get the state data first because it's the most likely to be useful
    // if we wind up abandoning this fetch.
    if (mhaveheader && !mhavestate && !mfailed)
    {
        assert (mledger);

        if (!mledger->peekaccountstatemap ()->isvalid ())
        {
            mfailed = true;
        }
        else if (mledger->peekaccountstatemap ()->gethash ().iszero ())
        {
            // we need the root node
            tmgl.set_itype (protocol::lias_node);
            *tmgl.add_nodeids () = shamapnodeid ().getrawstring ();
            if (m_journal.trace) m_journal.trace <<
                "sending as root request to " << (peer ? "selected peer" : "all peers");
            sendrequest (tmgl, peer);
            return;
        }
        else
        {
            std::vector<shamapnodeid> nodeids;
            std::vector<uint256> nodehashes;
            // vfalco why 256? make this a constant
            nodeids.reserve (256);
            nodehashes.reserve (256);
            accountstatesf filter;

            // release the lock while we process the large state map
            sl.unlock();
            mledger->peekaccountstatemap ()->getmissingnodes (
                nodeids, nodehashes, 256, &filter);
            sl.lock();

            // make sure nothing happened while we released the lock
            if (!mfailed && !mcomplete && !mhavestate)
            {
                if (nodeids.empty ())
                {
                    if (!mledger->peekaccountstatemap ()->isvalid ())
                        mfailed = true;
                    else
                    {
                        mhavestate = true;

                        if (mhavetransactions)
                            mcomplete = true;
                    }
                }
                else
                {
                    // vfalco why 128? make this a constant
                    if (!maggressive)
                        filternodes (nodeids, nodehashes, mrecentasnodes,
                            128, !isprogress ());

                    if (!nodeids.empty ())
                    {
                        tmgl.set_itype (protocol::lias_node);
                        boost_foreach (shamapnodeid const& it, nodeids)
                        {
                            * (tmgl.add_nodeids ()) = it.getrawstring ();
                        }
                        if (m_journal.trace) m_journal.trace <<
                            "sending as node " << nodeids.size () <<
                                " request to " << (
                                    peer ? "selected peer" : "all peers");
                        if (nodeids.size () == 1 && m_journal.trace) m_journal.trace <<
                            "as node: " << nodeids[0];
                        sendrequest (tmgl, peer);
                        return;
                    }
                    else
                        if (m_journal.trace) m_journal.trace <<
                            "all as nodes filtered";
                }
            }
        }
    }

    if (mhaveheader && !mhavetransactions && !mfailed)
    {
        assert (mledger);

        if (!mledger->peektransactionmap ()->isvalid ())
        {
            mfailed = true;
        }
        else if (mledger->peektransactionmap ()->gethash ().iszero ())
        {
            // we need the root node
            tmgl.set_itype (protocol::litx_node);
            * (tmgl.add_nodeids ()) = shamapnodeid ().getrawstring ();
            if (m_journal.trace) m_journal.trace <<
                "sending tx root request to " << (
                    peer ? "selected peer" : "all peers");
            sendrequest (tmgl, peer);
            return;
        }
        else
        {
            std::vector<shamapnodeid> nodeids;
            std::vector<uint256> nodehashes;
            nodeids.reserve (256);
            nodehashes.reserve (256);
            transactionstatesf filter;
            mledger->peektransactionmap ()->getmissingnodes (
                nodeids, nodehashes, 256, &filter);

            if (nodeids.empty ())
            {
                if (!mledger->peektransactionmap ()->isvalid ())
                    mfailed = true;
                else
                {
                    mhavetransactions = true;

                    if (mhavestate)
                        mcomplete = true;
                }
            }
            else
            {
                if (!maggressive)
                    filternodes (nodeids, nodehashes, mrecenttxnodes,
                        128, !isprogress ());

                if (!nodeids.empty ())
                {
                    tmgl.set_itype (protocol::litx_node);
                    boost_foreach (shamapnodeid const& it, nodeids)
                    {
                        * (tmgl.add_nodeids ()) = it.getrawstring ();
                    }
                    if (m_journal.trace) m_journal.trace <<
                        "sending tx node " << nodeids.size () <<
                        " request to " << (
                            peer ? "selected peer" : "all peers");
                    sendrequest (tmgl, peer);
                    return;
                }
                else
                    if (m_journal.trace) m_journal.trace <<
                        "all tx nodes filtered";
            }
        }
    }

    if (mcomplete || mfailed)
    {
        if (m_journal.debug) m_journal.debug <<
            "done:" << (mcomplete ? " complete" : "") <<
                (mfailed ? " failed " : " ") <<
            mledger->getledgerseq ();
        sl.unlock ();
        done ();
    }
}

void inboundledger::filternodes (std::vector<shamapnodeid>& nodeids,
    std::vector<uint256>& nodehashes, std::set<shamapnodeid>& recentnodes,
    int max, bool aggressive)
{
    // ask for new nodes in preference to ones we've already asked for
    assert (nodeids.size () == nodehashes.size ());

    std::vector<bool> duplicates;
    duplicates.reserve (nodeids.size ());

    int dupcount = 0;

    boost_foreach(shamapnodeid const& nodeid, nodeids)
    {
        if (recentnodes.count (nodeid) != 0)
        {
            duplicates.push_back (true);
            ++dupcount;
        }
        else
            duplicates.push_back (false);
    }

    if (dupcount == nodeids.size ())
    {
        // all duplicates
        if (!aggressive)
        {
            nodeids.clear ();
            nodehashes.clear ();
            if (m_journal.trace) m_journal.trace <<
                "filternodes: all are duplicates";
            return;
        }
    }
    else if (dupcount > 0)
    {
        // some, but not all, duplicates
        int insertpoint = 0;

        for (unsigned int i = 0; i < nodeids.size (); ++i)
            if (!duplicates[i])
            {
                // keep this node
                if (insertpoint != i)
                {
                    nodeids[insertpoint] = nodeids[i];
                    nodehashes[insertpoint] = nodehashes[i];
                }

                ++insertpoint;
            }

        if (m_journal.trace) m_journal.trace <<
            "filternodes " << nodeids.size () << " to " << insertpoint;
        nodeids.resize (insertpoint);
        nodehashes.resize (insertpoint);
    }

    if (nodeids.size () > max)
    {
        nodeids.resize (max);
        nodehashes.resize (max);
    }

    boost_foreach (const shamapnodeid & n, nodeids)
    {
        recentnodes.insert (n);
    }
}

/** take ledger header data
    call with a lock
*/
// data must not have hash prefix
bool inboundledger::takeheader (std::string const& data)
{
    // return value: true=normal, false=bad data
    if (m_journal.trace) m_journal.trace <<
        "got header acquiring ledger " << mhash;

    if (mcomplete || mfailed || mhaveheader)
        return true;

    mledger = std::make_shared<ledger> (data, false);

    if (mledger->gethash () != mhash)
    {
        if (m_journal.warning) m_journal.warning <<
            "acquire hash mismatch";
        if (m_journal.warning) m_journal.warning <<
            mledger->gethash () << "!=" << mhash;
        mledger.reset ();
        return false;
    }

    mhaveheader = true;

    serializer s (data.size () + 4);
    s.add32 (hashprefix::ledgermaster);
    s.addraw (data);
    getapp().getnodestore ().store (
        hotledger, std::move (s.moddata ()), mhash);

    progress ();

    if (mledger->gettranshash ().iszero ())
        mhavetransactions = true;

    if (mledger->getaccounthash ().iszero ())
        mhavestate = true;

    mledger->setacquiring ();
    return true;
}

/** process tx data received from a peer
    call with a lock
*/
bool inboundledger::taketxnode (const std::list<shamapnodeid>& nodeids,
    const std::list< blob >& data, shamapaddnode& san)
{
    if (!mhaveheader)
    {
        if (m_journal.warning) m_journal.warning <<
            "tx node without header";
        san.incinvalid();
        return false;
    }

    if (mhavetransactions || mfailed)
    {
        san.incduplicate();
        return true;
    }

    std::list<shamapnodeid>::const_iterator nodeidit = nodeids.begin ();
    std::list< blob >::const_iterator nodedatait = data.begin ();
    transactionstatesf tfilter;

    while (nodeidit != nodeids.end ())
    {
        if (nodeidit->isroot ())
        {
            san += mledger->peektransactionmap ()->addrootnode (
                mledger->gettranshash (), *nodedatait, snfwire, &tfilter);
            if (!san.isgood())
                return false;
        }
        else
        {
            san +=  mledger->peektransactionmap ()->addknownnode (
                *nodeidit, *nodedatait, &tfilter);
            if (!san.isgood())
                return false;
        }

        ++nodeidit;
        ++nodedatait;
    }

    if (!mledger->peektransactionmap ()->issynching ())
    {
        mhavetransactions = true;

        if (mhavestate)
        {
            mcomplete = true;
            done ();
        }
    }

    progress ();
    return true;
}

/** process as data received from a peer
    call with a lock
*/
bool inboundledger::takeasnode (const std::list<shamapnodeid>& nodeids,
    const std::list< blob >& data, shamapaddnode& san)
{
    if (m_journal.trace) m_journal.trace <<
        "got asdata (" << nodeids.size () << ") acquiring ledger " << mhash;
    if (nodeids.size () == 1 && m_journal.trace) m_journal.trace <<
        "got as node: " << nodeids.front ();

    scopedlocktype sl (mlock);

    if (!mhaveheader)
    {
        if (m_journal.warning) m_journal.warning <<
            "don't have ledger header";
        san.incinvalid();
        return false;
    }

    if (mhavestate || mfailed)
    {
        san.incduplicate();
        return true;
    }

    std::list<shamapnodeid>::const_iterator nodeidit = nodeids.begin ();
    std::list< blob >::const_iterator nodedatait = data.begin ();
    accountstatesf tfilter;

    while (nodeidit != nodeids.end ())
    {
        if (nodeidit->isroot ())
        {
            san += mledger->peekaccountstatemap ()->addrootnode (
                mledger->getaccounthash (), *nodedatait, snfwire, &tfilter);
            if (!san.isgood ())
            {
                if (m_journal.warning) m_journal.warning <<
                    "bad ledger header";
                return false;
            }
        }
        else
        {
            san += mledger->peekaccountstatemap ()->addknownnode (
                *nodeidit, *nodedatait, &tfilter);
            if (!san.isgood ())
            {
                if (m_journal.warning) m_journal.warning <<
                    "unable to add as node";
                return false;
            }
        }

        ++nodeidit;
        ++nodedatait;
    }

    if (!mledger->peekaccountstatemap ()->issynching ())
    {
        mhavestate = true;

        if (mhavetransactions)
        {
            mcomplete = true;
            done ();
        }
    }

    progress ();
    return true;
}

/** process as root node received from a peer
    call with a lock
*/
bool inboundledger::takeasrootnode (blob const& data, shamapaddnode& san)
{
    if (mfailed || mhavestate)
    {
        san.incduplicate();
        return true;
    }

    if (!mhaveheader)
    {
        assert(false);
        san.incinvalid();
        return false;
    }

    accountstatesf tfilter;
    san += mledger->peekaccountstatemap ()->addrootnode (
        mledger->getaccounthash (), data, snfwire, &tfilter);
    return san.isgood();
}

/** process as root node received from a peer
    call with a lock
*/
bool inboundledger::taketxrootnode (blob const& data, shamapaddnode& san)
{
    if (mfailed || mhavestate)
    {
        san.incduplicate();
        return true;
    }

    if (!mhaveheader)
    {
        assert(false);
        san.incinvalid();
        return false;
    }

    transactionstatesf tfilter;
    san += mledger->peektransactionmap ()->addrootnode (
        mledger->gettranshash (), data, snfwire, &tfilter);
    return san.isgood();
}

std::vector<inboundledger::neededhash_t> inboundledger::getneededhashes ()
{
    std::vector<neededhash_t> ret;

    if (!mhaveheader)
    {
        ret.push_back (std::make_pair (
            protocol::tmgetobjectbyhash::otledger, mhash));
        return ret;
    }

    if (!mhavestate)
    {
        accountstatesf filter;
        // vfalco note what's the number 4?
        for (auto const& h : mledger->getneededaccountstatehashes (4, &filter))
        {
            ret.push_back (std::make_pair (
                protocol::tmgetobjectbyhash::otstate_node, h));
        }
    }

    if (!mhavetransactions)
    {
        transactionstatesf filter;
        // vfalco note what's the number 4?
        for (auto const& h : mledger->getneededtransactionhashes (4, &filter))
        {
            ret.push_back (std::make_pair (
                protocol::tmgetobjectbyhash::ottransaction_node, h));
        }
    }

    return ret;
}

/** stash a tmledgerdata received from a peer for later processing
    returns 'true' if we need to dispatch
*/
// vfalco todo why isn't the shared_ptr passed by const& ?
bool inboundledger::gotdata (std::weak_ptr<peer> peer,
    std::shared_ptr<protocol::tmledgerdata> data)
{
    scopedlocktype sl (mreceiveddatalock);

    mreceiveddata.push_back (peerdatapairtype (peer, data));

    if (mreceivedispatched)
        return false;

    mreceivedispatched = true;
    return true;
}

/** process one tmledgerdata
    returns the number of useful nodes
*/
// vfalco note, it is not necessary to pass the entire peer,
//              we can get away with just a resource::consumer endpoint.
//
//        todo change peer to consumer
//
int inboundledger::processdata (std::shared_ptr<peer> peer,
    protocol::tmledgerdata& packet)
{
    scopedlocktype sl (mlock);

    if (packet.type () == protocol::libase)
    {
        if (packet.nodes_size () < 1)
        {
            if (m_journal.warning) m_journal.warning <<
                "got empty header data";
            peer->charge (resource::feeinvalidrequest);
            return -1;
        }

        shamapaddnode san;

        if (!mhaveheader)
        {
            if (takeheader (packet.nodes (0).nodedata ()))
                san.incuseful ();
            else
            {
                if (m_journal.warning) m_journal.warning <<
                    "got invalid header data";
                peer->charge (resource::feeinvalidrequest);
                return -1;
            }
        }


        if (!mhavestate && (packet.nodes ().size () > 1) &&
            !takeasrootnode (strcopy (packet.nodes (1).nodedata ()), san))
        {
            if (m_journal.warning) m_journal.warning <<
                "included as root invalid";
        }

        if (!mhavetransactions && (packet.nodes ().size () > 2) &&
            !taketxrootnode (strcopy (packet.nodes (2).nodedata ()), san))
        {
            if (m_journal.warning) m_journal.warning <<
                "included tx root invalid";
        }

        if (!san.isinvalid ())
            progress ();
        else
            if (m_journal.debug) m_journal.debug <<
                "peer sends invalid base data";

        return san.getgood ();
    }

    if ((packet.type () == protocol::litx_node) || (
        packet.type () == protocol::lias_node))
    {
        std::list<shamapnodeid> nodeids;
        std::list< blob > nodedata;

        if (packet.nodes ().size () == 0)
        {
            if (m_journal.info) m_journal.info <<
                "got response with no nodes";
            peer->charge (resource::feeinvalidrequest);
            return -1;
        }

        for (int i = 0; i < packet.nodes ().size (); ++i)
        {
            const protocol::tmledgernode& node = packet.nodes (i);

            if (!node.has_nodeid () || !node.has_nodedata ())
            {
                if (m_journal.warning) m_journal.warning <<
                    "got bad node";
                peer->charge (resource::feeinvalidrequest);
                return -1;
            }

            nodeids.push_back (shamapnodeid (node.nodeid ().data (),
                node.nodeid ().size ()));
            nodedata.push_back (blob (node.nodedata ().begin (),
                node.nodedata ().end ()));
        }

        shamapaddnode ret;

        if (packet.type () == protocol::litx_node)
        {
            taketxnode (nodeids, nodedata, ret);
            if (m_journal.debug) m_journal.debug <<
                "ledger tx node stats: " << ret.get();
        }
        else
        {
            takeasnode (nodeids, nodedata, ret);
            if (m_journal.debug) m_journal.debug <<
                "ledger as node stats: " << ret.get();
        }

        if (!ret.isinvalid ())
            progress ();
        else
            if (m_journal.debug) m_journal.debug <<
                "peer sends invalid node data";

        return ret.getgood ();
    }

    return -1;
}

/** process pending tmledgerdata
    query the 'best' peer
*/
void inboundledger::rundata ()
{
    std::shared_ptr<peer> chosenpeer;
    int chosenpeercount = -1;

    std::vector <peerdatapairtype> data;
    do
    {
        data.clear();
        {
            scopedlocktype sl (mreceiveddatalock);

            if (mreceiveddata.empty ())
            {
                mreceivedispatched = false;
                break;
            }
            data.swap(mreceiveddata);
        }

        // select the peer that gives us the most nodes that are useful,
        // breaking ties in favor of the peer that responded first.
        for (auto& entry : data)
        {
            peer::ptr peer = entry.first.lock();
            if (peer)
            {
                int count = processdata (peer, *(entry.second));
                if (count > chosenpeercount)
                {
                    chosenpeer = peer;
                    chosenpeercount = count;
                }
            }
        }

    } while (1);

    if (chosenpeer)
        trigger (chosenpeer);
}

json::value inboundledger::getjson (int)
{
    json::value ret (json::objectvalue);

    scopedlocktype sl (mlock);

    ret["hash"] = to_string (mhash);

    if (mcomplete)
        ret["complete"] = true;

    if (mfailed)
        ret["failed"] = true;

    if (!mcomplete && !mfailed)
        ret["peers"] = static_cast<int>(mpeers.size());

    ret["have_header"] = mhaveheader;

    if (mhaveheader)
    {
        ret["have_state"] = mhavestate;
        ret["have_transactions"] = mhavetransactions;
    }

    if (maborted)
        ret["aborted"] = true;

    ret["timeouts"] = gettimeouts ();

    if (mhaveheader && !mhavestate)
    {
        json::value hv (json::arrayvalue);

        // vfalco why 16?
        auto v = mledger->getneededaccountstatehashes (16, nullptr);
        for (auto const& h : v)
        {
            hv.append (to_string (h));
        }
        ret["needed_state_hashes"] = hv;
    }

    if (mhaveheader && !mhavetransactions)
    {
        json::value hv (json::arrayvalue);
        // vfalco why 16?
        auto v = mledger->getneededtransactionhashes (16, nullptr);
        for (auto const& h : v)
        {
            hv.append (to_string (h));
        }
        ret["needed_transaction_hashes"] = hv;
    }

    return ret;
}

} // ripple
