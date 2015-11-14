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
#include <ripple/app/ledger/consensustranssetsf.h>
#include <ripple/app/ledger/inboundledgers.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/defaultmissingnodehandler.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/app/tx/transactionacquire.h>
#include <ripple/overlay/overlay.h>
#include <boost/foreach.hpp>
#include <memory>

namespace ripple {

enum
{
    // vfalco note this should be a std::chrono::duration constant.
    // todo document this. is it seconds? milliseconds? wtf?
    tx_acquire_timeout = 250
};

transactionacquire::transactionacquire (uint256 const& hash, clock_type& clock)
    : peerset (hash, tx_acquire_timeout, true, clock,
        deprecatedlogs().journal("transactionacquire"))
    , mhaveroot (false)
{
    application& app = getapp();
    mmap = std::make_shared<shamap> (smttransaction, hash,
        app.getfullbelowcache (), app.gettreenodecache(), app.getnodestore(),
            defaultmissingnodehandler(), deprecatedlogs().journal("shamap"));
    mmap->setunbacked ();
}

transactionacquire::~transactionacquire ()
{
}

static void tacompletionhandler (uint256 hash, shamap::pointer map)
{
    {
        application::scopedlocktype lock (getapp ().getmasterlock ());

        getapp().getops ().mapcomplete (hash, map);

        getapp().getinboundledgers ().dropledger (hash);
    }
}

void transactionacquire::done ()
{
    // we hold a peerset lock and so cannot acquire the master lock here
    shamap::pointer map;

    if (mfailed)
    {
        writelog (lswarning, transactionacquire) << "failed to acquire tx set " << mhash;
    }
    else
    {
        writelog (lsinfo, transactionacquire) << "acquired tx set " << mhash;
        mmap->setimmutable ();
        map = mmap;
    }

    getapp().getjobqueue().addjob (jttxn_data, "completeacquire", std::bind (&tacompletionhandler, mhash, map));
}

void transactionacquire::ontimer (bool progress, scopedlocktype& psl)
{
    bool aggressive = false;

    if (gettimeouts () > 10)
    {
        writelog (lswarning, transactionacquire) << "ten timeouts on tx set " << gethash ();
        psl.unlock();
        {
            auto lock = getapp().masterlock();

            if (getapp().getops ().stillneedtxset (mhash))
            {
                writelog (lswarning, transactionacquire) << "still need it";
                mtimeouts = 0;
                aggressive = true;
            }
        }
        psl.lock();

        if (!aggressive)
        {
            mfailed = true;
            done ();
            return;
        }
    }

    if (aggressive || !getpeercount ())
    {
        // out of peers
        writelog (lswarning, transactionacquire) << "out of peers for tx set " << gethash ();

        bool found = false;
        overlay::peersequence peerlist = getapp().overlay ().getactivepeers ();
        boost_foreach (peer::ptr const& peer, peerlist)
        {
            if (peer->hastxset (gethash ()))
            {
                found = true;
                peerhas (peer);
            }
        }

        if (!found)
        {
            boost_foreach (peer::ptr const& peer, peerlist)
            peerhas (peer);
        }
    }
    else if (!progress)
        trigger (peer::ptr ());
}

std::weak_ptr<peerset> transactionacquire::pmdowncast ()
{
    return std::dynamic_pointer_cast<peerset> (shared_from_this ());
}

void transactionacquire::trigger (peer::ptr const& peer)
{
    if (mcomplete)
    {
        writelog (lsinfo, transactionacquire) << "trigger after complete";
        return;
    }
    if (mfailed)
    {
        writelog (lsinfo, transactionacquire) << "trigger after fail";
        return;
    }

    if (!mhaveroot)
    {
        writelog (lstrace, transactionacquire) << "transactionacquire::trigger " << (peer ? "havepeer" : "nopeer") << " no root";
        protocol::tmgetledger tmgl;
        tmgl.set_ledgerhash (mhash.begin (), mhash.size ());
        tmgl.set_itype (protocol::lits_candidate);

        if (gettimeouts () != 0)
            tmgl.set_querytype (protocol::qtindirect);

        * (tmgl.add_nodeids ()) = shamapnodeid ().getrawstring ();
        sendrequest (tmgl, peer);
    }
    else if (!mmap->isvalid ())
    {
        mfailed = true;
        done ();
    }
    else
    {
        std::vector<shamapnodeid> nodeids;
        std::vector<uint256> nodehashes;
        // vfalco todo use a dependency injection on the temp node cache
        consensustranssetsf sf (getapp().gettempnodecache ());
        mmap->getmissingnodes (nodeids, nodehashes, 256, &sf);

        if (nodeids.empty ())
        {
            if (mmap->isvalid ())
                mcomplete = true;
            else
                mfailed = true;

            done ();
            return;
        }

        protocol::tmgetledger tmgl;
        tmgl.set_ledgerhash (mhash.begin (), mhash.size ());
        tmgl.set_itype (protocol::lits_candidate);

        if (gettimeouts () != 0)
            tmgl.set_querytype (protocol::qtindirect);

        for (shamapnodeid& it : nodeids)
        {
            *tmgl.add_nodeids () = it.getrawstring ();
        }
        sendrequest (tmgl, peer);
    }
}

shamapaddnode transactionacquire::takenodes (const std::list<shamapnodeid>& nodeids,
        const std::list< blob >& data, peer::ptr const& peer)
{
    if (mcomplete)
    {
        writelog (lstrace, transactionacquire) << "tx set complete";
        return shamapaddnode ();
    }

    if (mfailed)
    {
        writelog (lstrace, transactionacquire) << "tx set failed";
        return shamapaddnode ();
    }

    try
    {
        if (nodeids.empty ())
            return shamapaddnode::invalid ();

        std::list<shamapnodeid>::const_iterator nodeidit = nodeids.begin ();
        std::list< blob >::const_iterator nodedatait = data.begin ();
        consensustranssetsf sf (getapp().gettempnodecache ());

        while (nodeidit != nodeids.end ())
        {
            if (nodeidit->isroot ())
            {
                if (mhaveroot)
                    writelog (lsdebug, transactionacquire) << "got root txs node, already have it";
                else if (!mmap->addrootnode (gethash (), *nodedatait, snfwire, nullptr).isgood())
                {
                    writelog (lswarning, transactionacquire) << "tx acquire got bad root node";
                }
                else
                    mhaveroot = true;
            }
            else if (!mmap->addknownnode (*nodeidit, *nodedatait, &sf).isgood())
            {
                writelog (lswarning, transactionacquire) << "tx acquire got bad non-root node";
                return shamapaddnode::invalid ();
            }

            ++nodeidit;
            ++nodedatait;
        }

        trigger (peer);
        progress ();
        return shamapaddnode::useful ();
    }
    catch (...)
    {
        writelog (lserror, transactionacquire) << "peer sends us junky transaction node data";
        return shamapaddnode::invalid ();
    }
}

} // ripple
