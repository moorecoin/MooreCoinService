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
#include <ripple/app/book/quality.h>
#include <ripple/app/ledger/ledgerentryset.h>
#include <ripple/basics/log.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/core/config.h>
#include <ripple/json/to_string.h>
#include <ripple/protocol/indexes.h>
#include <ripple/app/misc/dividendmaster.h>
#include <ripple/protocol/systemparameters.h>

namespace ripple {

// #define meta_debug

// vfalco todo replace this macro with a documented language constant
//        note is this part of the protocol?
//
#define dir_node_max        32

void ledgerentryset::init (ledger::ref ledger, uint256 const& transactionid,
                           std::uint32_t ledgerid, transactionengineparams params)
{
    mentries.clear ();
    mledger = ledger;
    mset.init (transactionid, ledgerid);
    mparams = params;
    mseq    = 0;
}

void ledgerentryset::clear ()
{
    mentries.clear ();
    mset.clear ();
}

ledgerentryset ledgerentryset::duplicate () const
{
    return ledgerentryset (mledger, mentries, mset, mseq + 1);
}

void ledgerentryset::swapwith (ledgerentryset& e)
{
    std::swap (mledger, e.mledger);
    mentries.swap (e.mentries);
    mset.swap (e.mset);
    std::swap (mparams, e.mparams);
    std::swap (mseq, e.mseq);
}

// find an entry in the set.  if it has the wrong sequence number, copy it and update the sequence number.
// this is basically: copy-on-read.
sle::pointer ledgerentryset::getentry (uint256 const& index, ledgerentryaction& action)
{
    auto it = mentries.find (index);

    if (it == mentries.end ())
    {
        action = taanone;
        return sle::pointer ();
    }

    if (it->second.mseq != mseq)
    {
        assert (it->second.mseq < mseq);
        it->second.mentry = std::make_shared<stledgerentry> (*it->second.mentry);
        it->second.mseq = mseq;
    }

    action = it->second.maction;
    return it->second.mentry;
}

sle::pointer ledgerentryset::entrycreate (ledgerentrytype lettype, uint256 const& index)
{
    assert (index.isnonzero ());
    sle::pointer slenew = std::make_shared<sle> (lettype, index);
    entrycreate (slenew);
    return slenew;
}

sle::pointer ledgerentryset::entrycache (ledgerentrytype lettype, uint256 const& index)
{
    assert (mledger);
    sle::pointer sleentry;

    if (index.isnonzero ())
    {
        ledgerentryaction action;
        sleentry = getentry (index, action);

        if (!sleentry)
        {
            assert (action != taadelete);
            sleentry = mimmutable ? mledger->getslei (index) : mledger->getsle (index);

            if (sleentry)
                entrycache (sleentry);
        }
        else if (action == taadelete)
            sleentry.reset ();
    }

    return sleentry;
}

ledgerentryaction ledgerentryset::hasentry (uint256 const& index) const
{
    std::map<uint256, ledgerentrysetentry>::const_iterator it = mentries.find (index);

    if (it == mentries.end ())
        return taanone;

    return it->second.maction;
}

void ledgerentryset::entrycache (sle::ref sle)
{
    assert (mledger);
    assert (sle->ismutable () || mimmutable); // don't put an immutable sle in a mutable les
    auto it = mentries.find (sle->getindex ());

    if (it == mentries.end ())
    {
        mentries.insert (std::make_pair (sle->getindex (), ledgerentrysetentry (sle, taacached, mseq)));
        return;
    }

    switch (it->second.maction)
    {
    case taacached:
        assert (sle == it->second.mentry);
        it->second.mseq     = mseq;
        it->second.mentry   = sle;
        return;

    default:
        throw std::runtime_error ("cache after modify/delete/create");
    }
}

void ledgerentryset::entrycreate (sle::ref sle)
{
    assert (mledger && !mimmutable);
    assert (sle->ismutable ());
    auto it = mentries.find (sle->getindex ());

    if (it == mentries.end ())
    {
        mentries.insert (std::make_pair (sle->getindex (), ledgerentrysetentry (sle, taacreate, mseq)));
        return;
    }

    switch (it->second.maction)
    {

    case taadelete:
        writelog (lsdebug, ledgerentryset) << "create after delete = modify";
        it->second.mentry = sle;
        it->second.maction = taamodify;
        it->second.mseq = mseq;
        break;

    case taamodify:
        throw std::runtime_error ("create after modify");

    case taacreate:
        throw std::runtime_error ("create after create"); // this could be made to work

    case taacached:
        throw std::runtime_error ("create after cache");

    default:
        throw std::runtime_error ("unknown taa");
    }

    assert (it->second.mseq == mseq);
}

void ledgerentryset::entrymodify (sle::ref sle)
{
    assert (sle->ismutable () && !mimmutable);
    assert (mledger);
    auto it = mentries.find (sle->getindex ());

    if (it == mentries.end ())
    {
        mentries.insert (std::make_pair (sle->getindex (), ledgerentrysetentry (sle, taamodify, mseq)));
        return;
    }

    assert (it->second.mseq == mseq);
    assert (it->second.mentry == sle);

    switch (it->second.maction)
    {
    case taacached:
        it->second.maction  = taamodify;

        // fall through

    case taacreate:
    case taamodify:
        it->second.mseq     = mseq;
        it->second.mentry   = sle;
        break;

    case taadelete:
        throw std::runtime_error ("modify after delete");

    default:
        throw std::runtime_error ("unknown taa");
    }
}

void ledgerentryset::entrydelete (sle::ref sle)
{
    assert (sle->ismutable () && !mimmutable);
    assert (mledger);
    auto it = mentries.find (sle->getindex ());

    if (it == mentries.end ())
    {
        assert (false); // deleting an entry not cached?
        mentries.insert (std::make_pair (sle->getindex (), ledgerentrysetentry (sle, taadelete, mseq)));
        return;
    }

    assert (it->second.mseq == mseq);
    assert (it->second.mentry == sle);

    switch (it->second.maction)
    {
    case taacached:
    case taamodify:
        it->second.mseq     = mseq;
        it->second.mentry   = sle;
        it->second.maction  = taadelete;
        break;

    case taacreate:
        mentries.erase (it);
        break;

    case taadelete:
        break;

    default:
        throw std::runtime_error ("unknown taa");
    }
}

json::value ledgerentryset::getjson (int) const
{
    json::value ret (json::objectvalue);

    json::value nodes (json::arrayvalue);

    for (auto it = mentries.begin (), end = mentries.end (); it != end; ++it)
    {
        json::value entry (json::objectvalue);
        entry["node"] = to_string (it->first);

        switch (it->second.mentry->gettype ())
        {
        case ltinvalid:
            entry["type"] = "invalid";
            break;

        case ltaccount_root:
            entry["type"] = "acccount_root";
            break;

        case ltdir_node:
            entry["type"] = "dir_node";
            break;

        case ltgenerator_map:
            entry["type"] = "generator_map";
            break;

        case ltripple_state:
            entry["type"] = "ripple_state";
            break;

        case ltnickname:
            entry["type"] = "nickname";
            break;

        case ltoffer:
            entry["type"] = "offer";
            break;

        default:
            assert (false);
        }

        switch (it->second.maction)
        {
        case taacached:
            entry["action"] = "cache";
            break;

        case taamodify:
            entry["action"] = "modify";
            break;

        case taadelete:
            entry["action"] = "delete";
            break;

        case taacreate:
            entry["action"] = "create";
            break;

        default:
            assert (false);
        }

        nodes.append (entry);
    }

    ret["nodes" ] = nodes;

    ret["metadata"] = mset.getjson (0);

    return ret;
}

sle::pointer ledgerentryset::getformod (uint256 const& node, ledger::ref ledger,
                                        nodetoledgerentry& newmods)
{
    auto it = mentries.find (node);

    if (it != mentries.end ())
    {
        if (it->second.maction == taadelete)
        {
            writelog (lsfatal, ledgerentryset) << "trying to thread to deleted node";
            return sle::pointer ();
        }

        if (it->second.maction == taacached)
            it->second.maction = taamodify;

        if (it->second.mseq != mseq)
        {
            it->second.mentry = std::make_shared<stledgerentry> (*it->second.mentry);
            it->second.mseq = mseq;
        }

        return it->second.mentry;
    }

    auto me = newmods.find (node);

    if (me != newmods.end ())
    {
        assert (me->second);
        return me->second;
    }

    sle::pointer ret = ledger->getsle (node);

    if (ret)
        newmods.insert (std::make_pair (node, ret));

    return ret;
}

bool ledgerentryset::threadtx (rippleaddress const& threadto, ledger::ref ledger,
                               nodetoledgerentry& newmods)
{
    sle::pointer sle = getformod (
        getaccountrootindex (threadto.getaccountid ()), ledger, newmods);

#ifdef meta_debug
    writelog (lstrace, ledgerentryset) << "thread to " << threadto.getaccountid ();
#endif

    if (!sle)
    {
        writelog (lsfatal, ledgerentryset) <<
            "threading to non-existent account: " << threadto.humanaccountid ();
        assert (false);
        return false;
    }

    return threadtx (sle, ledger, newmods);
}

bool ledgerentryset::threadtx (sle::ref threadto, ledger::ref ledger,
                               nodetoledgerentry& newmods)
{
    // node = the node that was modified/deleted/created
    // threadto = the node that needs to know
    uint256 prevtxid;
    std::uint32_t prevlgrid;

    if (!threadto->thread (mset.gettxid (), mset.getlgrseq (), prevtxid, prevlgrid))
        return false;

    if (prevtxid.iszero () ||
            transactionmetaset::thread (mset.getaffectednode (threadto, sfmodifiednode), prevtxid, prevlgrid))
        return true;

    assert (false);
    return false;
}

bool ledgerentryset::threadowners (sle::ref node, ledger::ref ledger,
                                   nodetoledgerentry& newmods)
{
    // thread new or modified node to owner or owners
    if (node->hasoneowner ()) // thread to owner's account
    {
#ifdef meta_debug
        writelog (lstrace, ledgerentryset) << "thread to single owner";
#endif
        return threadtx (node->getowner (), ledger, newmods);
    }
    else if (node->hastwoowners ()) // thread to owner's accounts
    {
#ifdef meta_debug
        writelog (lstrace, ledgerentryset) << "thread to two owners";
#endif
        return
            threadtx (node->getfirstowner (), ledger, newmods) &&
            threadtx (node->getsecondowner (), ledger, newmods);
    }
    else
        return false;
}

void ledgerentryset::calcrawmeta (serializer& s, ter result, std::uint32_t index)
{
    // calculate the raw meta data and return it. this must be called before the set is committed

    // entries modified only as a result of building the transaction metadata
    nodetoledgerentry newmod;

    for (auto& it : mentries)
    {
        sfield::ptr type = &sfgeneric;

        switch (it.second.maction)
        {
        case taamodify:
#ifdef meta_debug
            writelog (lstrace, ledgerentryset) << "modified node " << it.first;
#endif
            type = &sfmodifiednode;
            break;

        case taadelete:
#ifdef meta_debug
            writelog (lstrace, ledgerentryset) << "deleted node " << it.first;
#endif
            type = &sfdeletednode;
            break;

        case taacreate:
#ifdef meta_debug
            writelog (lstrace, ledgerentryset) << "created node " << it.first;
#endif
            type = &sfcreatednode;
            break;

        default: // ignore these
            break;
        }

        if (type == &sfgeneric)
            continue;

        sle::pointer orignode = mledger->getslei (it.first);
        sle::pointer curnode = it.second.mentry;

        if ((type == &sfmodifiednode) && (*curnode == *orignode))
            continue;

        std::uint16_t nodetype = curnode
            ? curnode->getfieldu16 (sfledgerentrytype)
            : orignode->getfieldu16 (sfledgerentrytype);

        mset.setaffectednode (it.first, *type, nodetype);

        if (type == &sfdeletednode)
        {
            assert (orignode && curnode);
            threadowners (orignode, mledger, newmod); // thread transaction to owners

            stobject prevs (sfpreviousfields);
            for (auto const& obj : *orignode)
            {
                // go through the original node for modified fields saved on modification
                if (obj.getfname ().shouldmeta (sfield::smd_changeorig) && !curnode->hasmatchingentry (obj))
                    prevs.addobject (obj);
            }

            if (!prevs.empty ())
                mset.getaffectednode (it.first).addobject (prevs);

            stobject finals (sffinalfields);
            for (auto const& obj : *curnode)
            {
                // go through the final node for final fields
                if (obj.getfname ().shouldmeta (sfield::smd_always | sfield::smd_deletefinal))
                    finals.addobject (obj);
            }

            if (!finals.empty ())
                mset.getaffectednode (it.first).addobject (finals);
        }
        else if (type == &sfmodifiednode)
        {
            assert (curnode && orignode);

            if (curnode->isthreadedtype ()) // thread transaction to node it modified
                threadtx (curnode, mledger, newmod);

            stobject prevs (sfpreviousfields);
            for (auto const& obj : *orignode)
            {
                // search the original node for values saved on modify
                if (obj.getfname ().shouldmeta (sfield::smd_changeorig) && !curnode->hasmatchingentry (obj))
                    prevs.addobject (obj);
            }

            if (!prevs.empty ())
                mset.getaffectednode (it.first).addobject (prevs);

            stobject finals (sffinalfields);
            for (auto const& obj : *curnode)
            {
                // search the final node for values saved always
                if (obj.getfname ().shouldmeta (sfield::smd_always | sfield::smd_changenew))
                    finals.addobject (obj);
            }

            if (!finals.empty ())
                mset.getaffectednode (it.first).addobject (finals);
        }
        else if (type == &sfcreatednode) // if created, thread to owner(s)
        {
            assert (curnode && !orignode);
            threadowners (curnode, mledger, newmod);

            if (curnode->isthreadedtype ()) // always thread to self
                threadtx (curnode, mledger, newmod);

            stobject news (sfnewfields);
            for (auto const& obj : *curnode)
            {
                // save non-default values
                if (!obj.isdefault () && obj.getfname ().shouldmeta (sfield::smd_create | sfield::smd_always))
                    news.addobject (obj);
            }

            if (!news.empty ())
                mset.getaffectednode (it.first).addobject (news);
        }
        else assert (false);
    }

    // add any new modified nodes to the modification set
    for (auto& it : newmod)
        entrymodify (it.second);

    mset.addraw (s, result, index);
    writelog (lstrace, ledgerentryset) << "metadata:" << mset.getjson (0);
}

ter ledgerentryset::dircount (uint256 const& urootindex, std::uint32_t& ucount)
{
    std::uint64_t  unodedir    = 0;

    ucount  = 0;

    do
    {
        sle::pointer    slenode = entrycache (ltdir_node, getdirnodeindex (urootindex, unodedir));

        if (slenode)
        {
            ucount      += slenode->getfieldv256 (sfindexes).peekvalue ().size ();

            unodedir    = slenode->getfieldu64 (sfindexnext);       // get next node.
        }
        else if (unodedir)
        {
            writelog (lswarning, ledgerentryset) << "dircount: no such node";

            assert (false);

            return tefbad_ledger;
        }
    }
    while (unodedir);

    return tessuccess;
}

bool ledgerentryset::dirisempty (uint256 const& urootindex)
{
    std::uint64_t  unodedir = 0;

    sle::pointer slenode = entrycache (ltdir_node, getdirnodeindex (urootindex, unodedir));

    if (!slenode)
        return true;

    if (!slenode->getfieldv256 (sfindexes).peekvalue ().empty ())
        return false;

    // if there's another page, it must be non-empty
    return slenode->getfieldu64 (sfindexnext) == 0;
}

// <--     unodedir: for deletion, present to make dirdelete efficient.
// -->   urootindex: the index of the base of the directory.  nodes are based off of this.
// --> uledgerindex: value to add to directory.
// only append. this allow for things that watch append only structure to just monitor from the last node on ward.
// within a node with no deletions order of elements is sequential.  otherwise, order of elements is random.
ter ledgerentryset::diradd (
    std::uint64_t&                          unodedir,
    uint256 const&                          urootindex,
    uint256 const&                          uledgerindex,
    std::function<void (sle::ref, bool)>    fdescriber)
{
    writelog (lstrace, ledgerentryset) << "diradd:" <<
        " urootindex=" << to_string (urootindex) <<
        " uledgerindex=" << to_string (uledgerindex);

    sle::pointer        slenode;
    stvector256         svindexes;
    sle::pointer        sleroot     = entrycache (ltdir_node, urootindex);

    if (!sleroot)
    {
        // no root, make it.
        sleroot     = entrycreate (ltdir_node, urootindex);
        sleroot->setfieldh256 (sfrootindex, urootindex);
        fdescriber (sleroot, true);

        slenode     = sleroot;
        unodedir    = 0;
    }
    else
    {
        unodedir    = sleroot->getfieldu64 (sfindexprevious);       // get index to last directory node.

        if (unodedir)
        {
            // try adding to last node.
            slenode     = entrycache (ltdir_node, getdirnodeindex (urootindex, unodedir));

            assert (slenode);
        }
        else
        {
            // try adding to root.  didn't have a previous set to the last node.
            slenode     = sleroot;
        }

        svindexes   = slenode->getfieldv256 (sfindexes);

        if (dir_node_max != svindexes.peekvalue ().size ())
        {
            // add to current node.
            entrymodify (slenode);
        }
        // add to new node.
        else if (!++unodedir)
        {
            return tecdir_full;
        }
        else
        {
            // have old last point to new node
            slenode->setfieldu64 (sfindexnext, unodedir);
            entrymodify (slenode);

            // have root point to new node.
            sleroot->setfieldu64 (sfindexprevious, unodedir);
            entrymodify (sleroot);

            // create the new node.
            slenode     = entrycreate (ltdir_node, getdirnodeindex (urootindex, unodedir));
            slenode->setfieldh256 (sfrootindex, urootindex);

            if (unodedir != 1)
                slenode->setfieldu64 (sfindexprevious, unodedir - 1);

            fdescriber (slenode, false);

            svindexes   = stvector256 ();
        }
    }

    svindexes.peekvalue ().push_back (uledgerindex); // append entry.
    slenode->setfieldv256 (sfindexes, svindexes);   // save entry.

    writelog (lstrace, ledgerentryset) <<
        "diradd:   creating: root: " << to_string (urootindex);
    writelog (lstrace, ledgerentryset) <<
        "diradd:  appending: entry: " << to_string (uledgerindex);
    writelog (lstrace, ledgerentryset) <<
        "diradd:  appending: node: " << strhex (unodedir);
    // writelog (lsinfo, ledgerentryset) << "diradd:  appending: prev: " << svindexes.peekvalue()[0].tostring();

    return tessuccess;
}

// ledger must be in a state for this to work.
ter ledgerentryset::dirdelete (
    const bool                      bkeeproot,      // --> true, if we never completely clean up, after we overflow the root node.
    const std::uint64_t&            unodedir,       // --> node containing entry.
    uint256 const&                  urootindex,     // --> the index of the base of the directory.  nodes are based off of this.
    uint256 const&                  uledgerindex,   // --> value to remove from directory.
    const bool                      bstable,        // --> true, not to change relative order of entries.
    const bool                      bsoft)          // --> true, unodedir is not hard and fast (pass unodedir=0).
{
    std::uint64_t       unodecur    = unodedir;
    sle::pointer        slenode     = entrycache (ltdir_node, getdirnodeindex (urootindex, unodecur));

    if (!slenode)
    {
        writelog (lswarning, ledgerentryset) << "dirdelete: no such node:" <<
            " urootindex=" << to_string (urootindex) <<
            " unodedir=" << strhex (unodedir) <<
            " uledgerindex=" << to_string (uledgerindex);

        if (!bsoft)
        {
            assert (false);
            return tefbad_ledger;
        }
        else if (unodedir < 20)
        {
            // go the extra mile. even if node doesn't exist, try the next node.

            return dirdelete (bkeeproot, unodedir + 1, urootindex, uledgerindex, bstable, true);
        }
        else
        {
            return tefbad_ledger;
        }
    }

    stvector256 svindexes   = slenode->getfieldv256 (sfindexes);
    std::vector<uint256>& vuiindexes  = svindexes.peekvalue ();

    auto it = std::find (vuiindexes.begin (), vuiindexes.end (), uledgerindex);

    if (vuiindexes.end () == it)
    {
        if (!bsoft)
        {
            assert (false);

            writelog (lswarning, ledgerentryset) << "dirdelete: no such entry";

            return tefbad_ledger;
        }
        else if (unodedir < 20)
        {
            // go the extra mile. even if entry not in node, try the next node.

            return dirdelete (bkeeproot, unodedir + 1, urootindex, uledgerindex,
                bstable, true);
        }
        else
        {
            return tefbad_ledger;
        }
    }

    // remove the element.
    if (vuiindexes.size () > 1)
    {
        if (bstable)
        {
            vuiindexes.erase (it);
        }
        else
        {
            *it = vuiindexes[vuiindexes.size () - 1];
            vuiindexes.resize (vuiindexes.size () - 1);
        }
    }
    else
    {
        vuiindexes.clear ();
    }

    slenode->setfieldv256 (sfindexes, svindexes);
    entrymodify (slenode);

    if (vuiindexes.empty ())
    {
        // may be able to delete nodes.
        std::uint64_t       unodeprevious   = slenode->getfieldu64 (sfindexprevious);
        std::uint64_t       unodenext       = slenode->getfieldu64 (sfindexnext);

        if (!unodecur)
        {
            // just emptied root node.

            if (!unodeprevious)
            {
                // never overflowed the root node.  delete it.
                entrydelete (slenode);
            }
            // root overflowed.
            else if (bkeeproot)
            {
                // if root overflowed and not allowed to delete overflowed root node.
            }
            else if (unodeprevious != unodenext)
            {
                // have more than 2 nodes.  can't delete root node.
            }
            else
            {
                // have only a root node and a last node.
                sle::pointer        slelast = entrycache (ltdir_node, getdirnodeindex (urootindex, unodenext));

                assert (slelast);

                if (slelast->getfieldv256 (sfindexes).peekvalue ().empty ())
                {
                    // both nodes are empty.

                    entrydelete (slenode);  // delete root.
                    entrydelete (slelast);  // delete last.
                }
                else
                {
                    // have an entry, can't delete root node.
                }
            }
        }
        // just emptied a non-root node.
        else if (unodenext)
        {
            // not root and not last node. can delete node.

            sle::pointer        sleprevious = entrycache (ltdir_node, getdirnodeindex (urootindex, unodeprevious));

            assert (sleprevious);

            sle::pointer        slenext     = entrycache (ltdir_node, getdirnodeindex (urootindex, unodenext));

            assert (sleprevious);
            assert (slenext);

            if (!sleprevious)
            {
                writelog (lswarning, ledgerentryset) << "dirdelete: previous node is missing";

                return tefbad_ledger;
            }

            if (!slenext)
            {
                writelog (lswarning, ledgerentryset) << "dirdelete: next node is missing";

                return tefbad_ledger;
            }

            // fix previous to point to its new next.
            sleprevious->setfieldu64 (sfindexnext, unodenext);
            entrymodify (sleprevious);

            // fix next to point to its new previous.
            slenext->setfieldu64 (sfindexprevious, unodeprevious);
            entrymodify (slenext);

            entrydelete(slenode);
        }
        // last node.
        else if (bkeeproot || unodeprevious)
        {
            // not allowed to delete last node as root was overflowed.
            // or, have pervious entries preventing complete delete.
        }
        else
        {
            // last and only node besides the root.
            sle::pointer            sleroot = entrycache (ltdir_node, urootindex);

            assert (sleroot);

            if (sleroot->getfieldv256 (sfindexes).peekvalue ().empty ())
            {
                // both nodes are empty.

                entrydelete (sleroot);  // delete root.
                entrydelete (slenode);  // delete last.
            }
            else
            {
                // root has an entry, can't delete.
            }
        }
    }

    return tessuccess;
}

// return the first entry and advance udirentry.
// <-- true, if had a next entry.
bool ledgerentryset::dirfirst (
    uint256 const& urootindex,  // --> root of directory.
    sle::pointer& slenode,      // <-- current node
    unsigned int& udirentry,    // <-- next entry
    uint256& uentryindex)       // <-- the entry, if available. otherwise, zero.
{
    slenode     = entrycache (ltdir_node, urootindex);
    udirentry   = 0;

    assert (slenode);           // never probe for directories.

    return ledgerentryset::dirnext (urootindex, slenode, udirentry, uentryindex);
}

// return the current entry and advance udirentry.
// <-- true, if had a next entry.
bool ledgerentryset::dirnext (
    uint256 const& urootindex,  // --> root of directory
    sle::pointer& slenode,      // <-> current node
    unsigned int& udirentry,    // <-> next entry
    uint256& uentryindex)       // <-- the entry, if available. otherwise, zero.
{
    stvector256             svindexes   = slenode->getfieldv256 (sfindexes);
    std::vector<uint256>&   vuiindexes  = svindexes.peekvalue ();

    assert (udirentry <= vuiindexes.size ());

    if (udirentry >= vuiindexes.size ())
    {
        std::uint64_t         unodenext   = slenode->getfieldu64 (sfindexnext);

        if (!unodenext)
        {
            uentryindex.zero ();

            return false;
        }
        else
        {
            sle::pointer slenext = entrycache (ltdir_node, getdirnodeindex (urootindex, unodenext));
            udirentry   = 0;

            if (!slenext)
            { // this should never happen
                writelog (lsfatal, ledgerentryset)
                        << "corrupt directory: index:"
                        << urootindex << " next:" << unodenext;
                return false;
            }

            slenode = slenext;
            // todo(tom): make this iterative.
            return dirnext (urootindex, slenode, udirentry, uentryindex);
        }
    }

    uentryindex = vuiindexes[udirentry++];

    writelog (lstrace, ledgerentryset) << "dirnext:" <<
        " udirentry=" << udirentry <<
        " uentryindex=" << uentryindex;

    return true;
}

uint256 ledgerentryset::getnextledgerindex (uint256 const& uhash)
{
    // find next node in ledger that isn't deleted by les
    uint256 ledgernext = uhash;
    std::map<uint256, ledgerentrysetentry>::const_iterator it;

    do
    {
        ledgernext = mledger->getnextledgerindex (ledgernext);
        it  = mentries.find (ledgernext);
    }
    while ((it != mentries.end ()) && (it->second.maction == taadelete));

    // find next node in les that isn't deleted
    for (it = mentries.upper_bound (uhash); it != mentries.end (); ++it)
    {
        // node found in les, node found in ledger, return earliest
        if (it->second.maction != taadelete)
            return (ledgernext.isnonzero () && (ledgernext < it->first)) ?
                    ledgernext : it->first;
    }

    // nothing next in les, return next ledger node
    return ledgernext;
}

uint256 ledgerentryset::getnextledgerindex (
    uint256 const& uhash, uint256 const& uend)
{
    uint256 next = getnextledgerindex (uhash);

    if (next > uend)
        return uint256 ();

    return next;
}

void ledgerentryset::incrementownercount (sle::ref sleaccount)
{
    assert (sleaccount);

    std::uint32_t const current_count = sleaccount->getfieldu32 (sfownercount);

    if (current_count == std::numeric_limits<std::uint32_t>::max ())
    {
        writelog (lsfatal, ledgerentryset) <<
            "account " << sleaccount->getfieldaccount160 (sfaccount) <<
            " owner count exceeds max!";
        return;
    }

    sleaccount->setfieldu32 (sfownercount, current_count + 1);
    entrymodify (sleaccount);
}

void ledgerentryset::incrementownercount (account const& owner)
{
    incrementownercount(entrycache (ltaccount_root,
        getaccountrootindex (owner)));
}

void ledgerentryset::decrementownercount (sle::ref sleaccount)
{
    assert (sleaccount);

    std::uint32_t const current_count = sleaccount->getfieldu32 (sfownercount);

    if (current_count == 0)
    {
        writelog (lsfatal, ledgerentryset) <<
            "account " << sleaccount->getfieldaccount160 (sfaccount) <<
            " owner count is already 0!";
        return;
    }

    sleaccount->setfieldu32 (sfownercount, current_count - 1);
    entrymodify (sleaccount);
}

void ledgerentryset::decrementownercount (account const& owner)
{
    decrementownercount(entrycache (ltaccount_root,
        getaccountrootindex (owner)));
}

ter ledgerentryset::offerdelete (sle::pointer sleoffer)
{
    if (!sleoffer)
        return tessuccess;

    auto offerindex = sleoffer->getindex ();
    auto owner = sleoffer->getfieldaccount160  (sfaccount);

    // detect legacy directories.
    bool bownernode = sleoffer->isfieldpresent (sfownernode);
    std::uint64_t uownernode = sleoffer->getfieldu64 (sfownernode);
    uint256 udirectory = sleoffer->getfieldh256 (sfbookdirectory);
    std::uint64_t ubooknode  = sleoffer->getfieldu64 (sfbooknode);

    ter terresult  = dirdelete (
        false, uownernode,
        getownerdirindex (owner), offerindex, false, !bownernode);
    ter terresult2 = dirdelete (
        false, ubooknode, udirectory, offerindex, true, false);

    if (tessuccess == terresult)
        decrementownercount (owner);

    entrydelete (sleoffer);

    return (terresult == tessuccess) ? terresult2 : terresult;
}

std::tuple<stamount, bool>
ledgerentryset::assetreleased (
    stamount const& amount,
    uint256 assetstateindex,
    sle::ref sleassetstate)
{
    stamount released(amount.issue());
    bool bisreleasefinished = false;
    auto const& sleasset = entrycache(ltasset, getassetindex(amount.issue()));

    if (sleasset) {
        uint64 boughttime = getquality(assetstateindex);
        starray const& releaseschedule = sleasset->getfieldarray(sfreleaseschedule);
        uint32 releaserate = 0;
        uint32 nextinterval = 0;

        if (releaseschedule.empty ())
            bisreleasefinished = true;
        else
        {
            auto itereleasepoint = releaseschedule.begin ();
            for (;itereleasepoint != releaseschedule.end (); ++itereleasepoint)
            {
                if (boughttime + itereleasepoint->getfieldu32 (sfexpiration) > getledger ()->getparentclosetimenc ())
                {
                    nextinterval = itereleasepoint->getfieldu32 (sfexpiration);
                    break;
                }
                
                releaserate = itereleasepoint->getfieldu32 (sfreleaserate);
            }
            if (itereleasepoint == releaseschedule.end ())
            {
                bisreleasefinished = true;
                releaserate = releaseschedule.back ().getfieldu32 (sfreleaserate);
            }
            else if (nextinterval > 0)
            {
                sleassetstate->setfieldu32 (sfnextreleasetime,
                                            (uint32)boughttime + nextinterval);
                entrymodify (sleassetstate);
            }
        }
        if (releaserate > 0) {
            released = mulround(amount, amountfromrate(releaserate), amount.issue(), true);
            released.floor();
        }
    }
    return std::make_tuple(released, bisreleasefinished);
}

ter
ledgerentryset::assetrelease (
    account const&  usrcaccountid,
    account const&  udstaccountid,
    currency const& currency,
    sle::ref sleripplestate)
{
    ter terresult = tessuccess;
    stamount sabalance = sleripplestate->getfieldamount(sfbalance);
    stamount sareserve ({assetcurrency (), noaccount ()});
    uint256 baseindex = getassetstateindex(usrcaccountid, udstaccountid, currency);
    uint256 assetstateindex = getqualityindex(baseindex);
    uint256 assetstateend = getqualitynext(assetstateindex);
    uint256 assetstateindexzero = assetstateindex;
    
    auto const& sleassetstate = entrycache(ltasset_state, assetstateindexzero);
    if (sleassetstate) {
        stamount amount = sleassetstate->getfieldamount(sfamount);
        account const& owner = sleassetstate->getfieldaccount160(sfaccount);
        if ((owner == usrcaccountid && amount.getissuer() == udstaccountid) ||
            (owner == udstaccountid && amount.getissuer() == usrcaccountid)) {
            stamount delivered = sleassetstate->getfieldamount(sfdeliveredamount);
            sareserve = (amount.getissuer() > owner) ? amount - delivered : delivered - amount;
        }
    }
    
    for (;;)
    {
        assetstateindex = getnextledgerindex(assetstateindex, assetstateend);

        if (assetstateindex.iszero())
            break;

        auto const& sleassetstate = entrycache(ltasset_state, assetstateindex);
        if (!sleassetstate)
            continue;

        stamount amount = sleassetstate->getfieldamount(sfamount);
        account const& owner = sleassetstate->getfieldaccount160(sfaccount);
        if (!(owner == usrcaccountid && amount.getissuer() == udstaccountid) &&
            !(owner == udstaccountid && amount.getissuer() == usrcaccountid))
            continue;

        stamount delivered = sleassetstate->getfieldamount(sfdeliveredamount);
        if (!delivered)
            delivered.setissue(amount.issue());

        stamount released;
        bool bisreleasefinished = false;
        // make sure next release time is up.
        uint32 nextreleasetime = sleassetstate->getfieldu32(sfnextreleasetime);
        if (nextreleasetime > getledger ()->getparentclosetimenc ())
            released = delivered;
        else
            std::tie (released, bisreleasefinished) = assetreleased (amount, assetstateindex, sleassetstate);

        bool bissuerhigh = amount.getissuer() > owner;

        // update reserve
        if (!sareserve)
            sareserve.setissue(amount.issue());
        auto reserve = amount - released;
        if (!bissuerhigh)
            reserve.negate ();
        sareserve += reserve;

        // no newly release.
        if (released <= delivered)
            continue;

        if (!bisreleasefinished) {
            // just update delivered amount if there are further releases.
            sleassetstate->setfieldamount(sfdeliveredamount, released);
            entrymodify(sleassetstate);
        } else {
            // compact asset state if no more further release.
            bool bdsthigh = usrcaccountid < udstaccountid;
            if (amount != released) {
                // move forever locked asset to assetstatezero
                sle::pointer sleassetstatezero = entrycache(ltasset_state, assetstateindexzero);
                if (sleassetstatezero) {
                    sleassetstatezero->setfieldamount(sfamount,
                                                      sleassetstatezero->getfieldamount(sfamount) + amount);
                    sleassetstatezero->setfieldamount(sfdeliveredamount,
                                                      sleassetstatezero->getfieldamount(sfdeliveredamount) + released);
                    entrymodify(sleassetstatezero);
                } else {
                    sleassetstatezero = entrycreate(ltasset_state, assetstateindexzero);
                    uint64 ulownode, uhighnode;
                    // add to receiver
                    terresult = diradd(bdsthigh ? uhighnode : ulownode,
                                       getownerdirindex(udstaccountid),
                                       sleassetstatezero->getindex(),
                                       std::bind(
                                           &ledger::ownerdirdescriber, std::placeholders::_1,
                                           std::placeholders::_2, udstaccountid));
                    if (tessuccess != terresult)
                        break;
                    // add to sender
                    terresult = diradd(bdsthigh ? ulownode : uhighnode,
                                       getownerdirindex(usrcaccountid),
                                       sleassetstatezero->getindex(),
                                       std::bind(
                                           &ledger::ownerdirdescriber, std::placeholders::_1,
                                           std::placeholders::_2, usrcaccountid));
                    if (tessuccess != terresult)
                        break;
                    sleassetstatezero->setfieldu64(sflownode, ulownode);
                    sleassetstatezero->setfieldu64(sfhighnode, uhighnode);
                    sleassetstatezero->setfieldaccount(sfaccount, owner);
                    sleassetstatezero->setfieldamount(sfamount, amount);
                    sleassetstatezero->setfieldamount(sfdeliveredamount, released);

                    incrementownercount(owner);
                }
            }
            if (tessuccess != terresult)
                break;
            uint64 ulownode = sleassetstate->getfieldu64(sflownode);
            uint64 uhighnode = sleassetstate->getfieldu64(sfhighnode);
            terresult = dirdelete(
                false,
                bdsthigh ? uhighnode : ulownode,
                getownerdirindex(udstaccountid),
                sleassetstate->getindex(),
                true,
                false);
            if (tessuccess != terresult)
                break;
            terresult = dirdelete(
                false,
                bdsthigh ? ulownode : uhighnode,
                getownerdirindex(usrcaccountid),
                sleassetstate->getindex(),
                true,
                false);
            if (tessuccess != terresult)
                break;
            entrydelete (sleassetstate);
            decrementownercount (owner);
        }

        // update balance in ripplestate
        released.setissue(sabalance.issue());
        delivered.setissue(sabalance.issue());

        if (bissuerhigh)
            sabalance += released - delivered;
        else
            sabalance -= released - delivered;
        sleripplestate->setfieldamount(sfbalance, sabalance);
        entrymodify(sleripplestate);
    }

    sareserve.setissue (sabalance.issue ());

    if (!sleripplestate->isfieldpresent(sfreserve) ||
        sleripplestate->getfieldamount(sfreserve) != sareserve) {
        sleripplestate->setfieldamount(sfreserve, sareserve);
        entrymodify(sleripplestate);
    }

    return terresult;
}

// return how much of issuer's currency ious that account holds.  may be
// negative.
// <-- iou's account has of issuer.
stamount ledgerentryset::rippleholds (
    account const& account,
    currency const& currency,
    account const& issuer,
    freezehandling zeroiffrozen)
{
    stamount sabalance;
    sle::pointer sleripplestate = entrycache (ltripple_state,
        getripplestateindex (account, issuer, currency));

    if (!sleripplestate)
    {
        sabalance.clear ({currency, issuer});
    }
    else if ((zeroiffrozen == fhzero_if_frozen) && isfrozen (account, currency, issuer))
    {
        sabalance.clear (issueref (currency, issuer));
    }
    else
    {
        if (assetcurrency() == currency)
            assetrelease(account, issuer, currency, sleripplestate);

        sabalance = sleripplestate->getfieldamount(sfbalance);

        if (account > issuer)
            sabalance.negate(); // put balance in account terms.

        sabalance.setissuer(issuer);
    }

    return sabalance;
}

// returns the amount an account can spend without going into debt.
//
// <-- saamount: amount of currency held by account. may be negative.
stamount ledgerentryset::accountholds (
    account const& account,
    currency const& currency,
    account const& issuer,
    freezehandling zeroiffrozen)
{
    stamount    saamount;

    bool const bvbc (isvbc (currency));

    if (isxrp(currency) || bvbc)
    {
        sle::pointer sleaccount = entrycache (ltaccount_root,
            getaccountrootindex (account));
        std::uint64_t ureserve = mledger->getreserve (
            sleaccount->getfieldu32 (sfownercount));

        stamount sabalance   = sleaccount->getfieldamount(bvbc?sfbalancevbc:sfbalance);
        if (bvbc)
            sabalance.setissue(vbcissue());

        if (sabalance < ureserve)
        {
            saamount.clear ();
        }
        else
        {
            saamount = sabalance - ureserve;
        }

        writelog (lstrace, ledgerentryset) << "accountholds:" <<
            " account=" << to_string (account) <<
            " saamount=" << saamount.getfulltext () <<
            " sabalance=" << sabalance.getfulltext () <<
            " ureserve=" << ureserve;
    }
    else
    {
        saamount = rippleholds (account, currency, issuer, zeroiffrozen);

        writelog (lstrace, ledgerentryset) << "accountholds:" <<
            " account=" << to_string (account) <<
            " saamount=" << saamount.getfulltext ();
    }

    return saamount;
}

bool ledgerentryset::isglobalfrozen (account const& issuer)
{
    if (!enforcefreeze() || isnative(issuer))
        return false;

    sle::pointer sle = entrycache (ltaccount_root, getaccountrootindex (issuer));
    if (sle && sle->isflag (lsfglobalfreeze))
        return true;

    return false;
}

// can the specified account spend the specified currency issued by
// the specified issuer or does the freeze flag prohibit it?
bool ledgerentryset::isfrozen(
    account const& account,
    currency const& currency,
    account const& issuer)
{
    if (!enforcefreeze() || isnative(currency))
        return false;

    sle::pointer sle = entrycache (ltaccount_root, getaccountrootindex (issuer));
    if (sle && sle->isflag (lsfglobalfreeze))
        return true;

    if (issuer != account)
    {
        // check if the issuer froze the line
        sle = entrycache (ltripple_state,
            getripplestateindex (account, issuer, currency));
        if (sle && sle->isflag ((issuer > account) ? lsfhighfreeze : lsflowfreeze))
        {
            return true;
        }
    }

    return false;
}

// returns the funds available for account for a currency/issuer.
// use when you need a default for rippling account's currency.
// xxx should take into account quality?
// --> sadefault/currency/issuer
// <-- safunds: funds available. may be negative.
//
// if the issuer is the same as account, funds are unlimited, use result is
// sadefault.
stamount ledgerentryset::accountfunds (
    account const& account, stamount const& sadefault, freezehandling zeroiffrozen)
{
    stamount    safunds;

    if (!sadefault.isnative () && sadefault.getissuer () == account)
    {
        safunds = sadefault;

        writelog (lstrace, ledgerentryset) << "accountfunds:" <<
            " account=" << to_string (account) <<
            " sadefault=" << sadefault.getfulltext () <<
            " self-funded";
    }
    else
    {
        safunds = accountholds (
            account, sadefault.getcurrency (), sadefault.getissuer (),
            zeroiffrozen);

        writelog (lstrace, ledgerentryset) << "accountfunds:" <<
            " account=" << to_string (account) <<
            " sadefault=" << sadefault.getfulltext () <<
            " safunds=" << safunds.getfulltext ();
    }

    return safunds;
}

// calculate transit fee.
stamount ledgerentryset::rippletransferfee (
    account const& usenderid,
    account const& ureceiverid,
    account const& issuer,
    stamount const& saamount)
{
    if (saamount.getcurrency() != assetcurrency() && usenderid != issuer && ureceiverid != issuer)
    {
        std::uint32_t utransitrate = rippletransferrate (*this, issuer);

        if (quality_one != utransitrate)
        {
            stamount satransfertotal = multiply (
                saamount, amountfromrate (utransitrate), saamount.issue ());
            stamount satransferfee = satransfertotal - saamount;

            writelog (lsdebug, ledgerentryset) << "rippletransferfee:" <<
                " satransferfee=" << satransferfee.getfulltext ();

            return satransferfee;
        }
    }

    return saamount.zeroed();
}

ter ledgerentryset::trustcreate (
    const bool      bsrchigh,
    account const&  usrcaccountid,
    account const&  udstaccountid,
    uint256 const&  uindex,             // --> ripple state entry
    sle::ref        sleaccount,         // --> the account being set.
    const bool      bauth,              // --> authorize account.
    const bool      bnoripple,          // --> others cannot ripple through
    const bool      bfreeze,            // --> funds cannot leave
    stamount const& sabalance,          // --> balance of account being set.
                                        // issuer should be noaccount()
    stamount const& salimit,            // --> limit for account being set.
                                        // issuer should be the account being set.
    const std::uint32_t uqualityin,
    const std::uint32_t uqualityout)
{
    auto const& ulowaccountid   = !bsrchigh ? usrcaccountid : udstaccountid;
    auto const& uhighaccountid  =  bsrchigh ? usrcaccountid : udstaccountid;

    sle::pointer sleripplestate  = entrycreate (ltripple_state, uindex);

    std::uint64_t   ulownode;
    std::uint64_t   uhighnode;

    ter terresult = diradd (
        ulownode,
        getownerdirindex (ulowaccountid),
        sleripplestate->getindex (),
        std::bind (&ledger::ownerdirdescriber,
                   std::placeholders::_1, std::placeholders::_2,
                   ulowaccountid));

    if (tessuccess == terresult)
    {
        terresult = diradd (
            uhighnode,
            getownerdirindex (uhighaccountid),
            sleripplestate->getindex (),
            std::bind (&ledger::ownerdirdescriber,
                       std::placeholders::_1, std::placeholders::_2,
                       uhighaccountid));
    }

    if (tessuccess == terresult)
    {
        const bool bsetdst = salimit.getissuer () == udstaccountid;
        const bool bsethigh = bsrchigh ^ bsetdst;

        // remember deletion hints.
        sleripplestate->setfieldu64 (sflownode, ulownode);
        sleripplestate->setfieldu64 (sfhighnode, uhighnode);

        sleripplestate->setfieldamount (
            bsethigh ? sfhighlimit : sflowlimit, salimit);
        sleripplestate->setfieldamount (
            bsethigh ? sflowlimit : sfhighlimit,
            stamount ({sabalance.getcurrency (),
                       bsetdst ? usrcaccountid : udstaccountid}));

        if (uqualityin)
            sleripplestate->setfieldu32 (
                bsethigh ? sfhighqualityin : sflowqualityin, uqualityin);

        if (uqualityout)
            sleripplestate->setfieldu32 (
                bsethigh ? sfhighqualityout : sflowqualityout, uqualityout);

        std::uint32_t uflags = bsethigh ? lsfhighreserve : lsflowreserve;

        if (bauth)
        {
            uflags |= (bsethigh ? lsfhighauth : lsflowauth);
        }
        if (bnoripple || assetcurrency() == salimit.getcurrency())
        {
            uflags |= (bsethigh ? lsfhighnoripple : lsflownoripple);
        }
        if (bfreeze)
        {
            uflags |= (!bsethigh ? lsflowfreeze : lsfhighfreeze);
        }

        sleripplestate->setfieldu32 (sfflags, uflags);
        incrementownercount (sleaccount);

        // only: create ripple balance.
        sleripplestate->setfieldamount (sfbalance, bsethigh ? -sabalance : sabalance);
        if (assetcurrency () == salimit.getcurrency ())
            sleripplestate->setfieldamount (sfreserve, stamount ({assetcurrency (), noaccount ()}));
    }

    return terresult;
}

ter ledgerentryset::trustdelete (
    sle::ref sleripplestate, account const& ulowaccountid,
    account const& uhighaccountid)
{
    // detect legacy dirs.
    bool        blownode    = sleripplestate->isfieldpresent (sflownode);
    bool        bhighnode   = sleripplestate->isfieldpresent (sfhighnode);
    std::uint64_t ulownode    = sleripplestate->getfieldu64 (sflownode);
    std::uint64_t uhighnode   = sleripplestate->getfieldu64 (sfhighnode);
    ter         terresult;

    writelog (lstrace, ledgerentryset)
        << "trustdelete: deleting ripple line: low";
    terresult   = dirdelete (
        false,
        ulownode,
        getownerdirindex (ulowaccountid),
        sleripplestate->getindex (),
        false,
        !blownode);

    if (tessuccess == terresult)
    {
        writelog (lstrace, ledgerentryset)
                << "trustdelete: deleting ripple line: high";
        terresult   = dirdelete (
            false,
            uhighnode,
            getownerdirindex (uhighaccountid),
            sleripplestate->getindex (),
            false,
            !bhighnode);
    }

    writelog (lstrace, ledgerentryset) << "trustdelete: deleting ripple line: state";
    entrydelete (sleripplestate);

    return terresult;
}
    
ter ledgerentryset::sharefeewithreferee(account const& usenderid, account const& uissuerid, const stamount& saamount)
{
    writelog (lsinfo, ledgerentryset)
        << "feeshare:\n"
        << "\tsender:" << usenderid << "\n"
        << "\tissuer:" << uissuerid << "\n"
        << "\tamount:" << saamount;
    
    ter terresult = tessuccess;
    // evenly divide saamount to 5 shares
    stamount satransfeeshareeach = multiply(saamount, stamount(saamount.issue(), 2, -1));
    // first get dividend object
    sle::pointer sledivobj = mledger->getdividendobject();
    // we have a dividend object, and its state is done
    if (sledivobj && sledivobj->getfieldu8(sfdividendstate) == dividendmaster::divstate_done)
    {
        std::map<account, stamount> takersmap;
        // extract ledgerseq and total vspd
        std::uint32_t divledgerseq = sledivobj->getfieldu32(sfdividendledger);
        // try find parent referee start from the sender itself
        sle::pointer slesender = mledger->getaccountroot(usenderid);
        sle::pointer slecurrent = slesender;
        int sendcnt = 0;
        account lastaccount;
        while (tessuccess == terresult && slecurrent && sendcnt < 5)
        {
            //no referee anymore
            if (!slecurrent->isfieldpresent(sfreferee))
            {
                break;
            }
            rippleaddress refereeaccountid = slecurrent->getfieldaccount(sfreferee);

            sle::pointer slereferee = mledger->getaccountroot(refereeaccountid);
            if (slereferee)
            {
                // there is a referee and it has field sfdividendledger, which is exact the same as divobjledgerseq
                if (slereferee->isfieldpresent(sfdividendledger) && slereferee->getfieldu32(sfdividendledger) == divledgerseq)
                {
                    if (slereferee->isfieldpresent(sfdividendvsprd))
                    {
                        std::uint64_t divvspd = slereferee->getfieldu64(sfdividendvsprd);
                        // only vspd greater than 10000(000000) get the fee share
                        if (divvspd > min_vspd_to_get_fee_share)
                        {
                            terresult = ripplecredit (uissuerid, refereeaccountid.getaccountid(), satransfeeshareeach);
                            if (tessuccess == terresult)
                            {
                                sendcnt += 1;
                                lastaccount = refereeaccountid.getaccountid();
                                takersmap.insert(std::pair<account, stamount>(lastaccount, satransfeeshareeach));
                                writelog (lsinfo, ledgerentryset) << "feeshare: " << refereeaccountid.getaccountid() << " get " << satransfeeshareeach;
                            }
                        }
                    }
                }
            }
            slecurrent = slereferee;
        }
        // can't find 5 ancestors, give all share to last ancestor
        if (terresult == tessuccess)
        {
            if (sendcnt == 0)
            {
                writelog (lsinfo, ledgerentryset) << "feeshare: no ancestor find gateway keep all fee share.";
            }
            else if (sendcnt < 5)
            {
                stamount saleft = multiply(satransfeeshareeach, stamount(satransfeeshareeach.issue(), 5 - sendcnt));
                terresult = ripplecredit (uissuerid, lastaccount, saleft);
                if (terresult == tessuccess)
                {
                    auto ittaker = takersmap.find(lastaccount);
                    if (ittaker == takersmap.end())
                    {
                        writelog (lswarning, ledgerentryset) << "last share account not found, this should not happpen.";
                    }
                    ittaker->second += saleft;
                }
                writelog (lsinfo, ledgerentryset) << "feeshare: left " << saleft << " goes to "<< lastaccount;
            }
            
            if (terresult == tessuccess && takersmap.size())
            {
                // if there are feesharetakers, record it
                starray feesharetakers = starray(sffeesharetakers);
                if (mset.hasfeesharetakers())
                {
                    feesharetakers = mset.getfeesharetakers();
                }
                // update takers' record in former rounds
                for (auto ittakerobj = feesharetakers.begin(); ittakerobj != feesharetakers.end(); ++ittakerobj)
                {
                    auto itfind = takersmap.find(ittakerobj->getfieldaccount(sfaccount).getaccountid());
                    if (itfind != takersmap.end())
                    {
                        stamount amountbefore = ittakerobj->getfieldamount(sfamount);
                        if (amountbefore.getcurrency() == itfind->second.getcurrency()
                                && amountbefore.getissuer() == itfind->second.getissuer())
                        {
                            ittakerobj->setfieldamount(sfamount, amountbefore + itfind->second);
                            takersmap.erase(itfind);
                        }
                    }
                }
                // append new takers' record
                for (auto ittakerrecord : takersmap)
                {
                    stobject feesharetaker(sffeesharetaker);
                    feesharetaker.setfieldaccount(sfaccount, ittakerrecord.first);
                    feesharetaker.setfieldamount(sfamount, ittakerrecord.second);
                    feesharetakers.push_back(feesharetaker);
                }
                mset.setfeesharetakers(feesharetakers);
            }
        }
    }
    return terresult;
}
    
ter ledgerentryset::addrefer(account const& refereeid, account const& referenceid)
{
    // open a ledger for editing.
    sle::pointer slereferee(entrycache(ltaccount_root, getaccountrootindex(refereeid)));
    sle::pointer slereference(entrycache(ltaccount_root, getaccountrootindex(referenceid)));
    
    auto const refereereferindex = getaccountreferindex (refereeid);
    sle::pointer slerefereerefer(entrycache (ltrefer, refereereferindex));
    sle::pointer slereferencerefer(entrycache (ltrefer, getaccountreferindex(referenceid)));
    
    if (!slereferee) {
        // referee account does not exist.
        writelog (lstrace, ledgerentryset) << "referee account does not exist.";
        return terno_account;
    } else if (!slereference) {
        // reference account does not exist.
        writelog (lstrace, ledgerentryset) << "reference account does not exist.";
        return terno_account;
    } else if (slereference->isfieldpresent(sfreferee)
               && slereference->getfieldaccount(sfreferee).getaccountid().isnonzero()) {
        // reference account already has referee
        writelog (lstrace, ledgerentryset) << "referee has been set.";
        return tefreferee_exist;
    } else if ((slereferencerefer && !slereferencerefer->getfieldarray(sfreferences).empty())) {
        // reference account already has references
        writelog (lstrace, ledgerentryset) << "reference has been set.";
        return tefreference_exist;
    } else {
        // modify references for referee account
        starray references(sfreferences);
        if (slerefereerefer) {
            if (slerefereerefer->isfieldpresent(sfreferences)) {
                references = slerefereerefer->getfieldarray(sfreferences);
                for (const auto & it: references) {
                    if (it.getfieldaccount(sfreference).getaccountid() == referenceid) {
                        writelog (lstrace, ledgerentryset) << "reference already exists in referee.";
                        return tefreference_exist;
                    }
                }
            }
            entrymodify(slerefereerefer);
        }
        else
            slerefereerefer = entrycreate(ltrefer, refereereferindex);
        
        references.push_back(stobject(sfreferenceholder));
        references.back().setfieldaccount(sfreference, referenceid);
        slerefereerefer->setfieldarray(sfreferences, references);
        // fix bug, add account for referee
        slerefereerefer->setfieldaccount(sfaccount, refereeid);
        
        // modify referee&referenceheight for reference account
        entrymodify(slereference);
        slereference->setfieldaccount(sfreferee, refereeid);
        
        int referenceheight=0;
        if (slereferee->isfieldpresent(sfreferenceheight))
            referenceheight=slereferee->getfieldu32(sfreferenceheight);
        slereference->setfieldu32(sfreferenceheight, referenceheight+1);
    }
    
    return tessuccess;
}

// direct send w/o fees:
// - redeeming ious and/or sending sender's own ious.
// - create trust line of needed.
// --> bcheckissuer : normally require issuer to be involved.
ter ledgerentryset::ripplecredit (
    account const& usenderid, account const& ureceiverid,
    stamount const& saamount, bool bcheckissuer)
{
    auto issuer = saamount.getissuer ();
    auto currency = saamount.getcurrency ();

    // make sure issuer is involved.
    assert (
        !bcheckissuer || usenderid == issuer || ureceiverid == issuer);
    (void) issuer;

    // disallow sending to self.
    assert (usenderid != ureceiverid);

    bool bsenderhigh = usenderid > ureceiverid;
    uint256 uindex = getripplestateindex (
        usenderid, ureceiverid, saamount.getcurrency ());
    auto sleripplestate  = entrycache (ltripple_state, uindex);

    ter terresult;

    assert (!isxrp (usenderid) && usenderid != noaccount());
    assert (!isxrp (ureceiverid) && ureceiverid != noaccount());
    assert (!isvbc (usenderid) && usenderid != noaccount());
    assert (!isvbc (ureceiverid) && ureceiverid != noaccount());

    // asset process
    if (currency == assetcurrency() && entrycache(ltasset, getassetindex(ureceiverid, currency))==nullptr)
    {
        sle::pointer sleasset = entrycache (ltasset, getassetindex (usenderid, currency));
        if (!sleasset)
        {
            return tembad_issuer;
        }
        if (sleasset->getfieldaccount160(sfregularkey) != ureceiverid)
        {
            uint32 parentclosetime = getledger()->getparentclosetimenc ();
            uint256 baseassetstateindex = getassetstateindex (usenderid, ureceiverid, currency);
            uint256 assetstateindex = getqualityindex (baseassetstateindex,
                parentclosetime - parentclosetime%getconfig().asset_interval_min);

            stamount amount(saamount);
            amount.setissuer(usenderid);
            sle::pointer sleassetstate = entrycache (ltasset_state, assetstateindex);
            if (!sleassetstate)
            {
                uint64 ulownode, uhighnode;
                sle::pointer sleassetstate  = entrycreate (ltasset_state, assetstateindex);
                // add to receiver
                terresult = diradd (bsenderhigh ? ulownode : uhighnode,
                    getownerdirindex(ureceiverid),
                    sleassetstate->getindex(),
                    std::bind(
                        &ledger::ownerdirdescriber, std::placeholders::_1,
                        std::placeholders::_2, ureceiverid));
                if (tessuccess == terresult)
                {
                    // add to issue
                    terresult = diradd (bsenderhigh ? uhighnode : ulownode,
                        getownerdirindex(usenderid),
                        sleassetstate->getindex(),
                        std::bind(
                            &ledger::ownerdirdescriber, std::placeholders::_1,
                            std::placeholders::_2, usenderid));
                }
                if (tessuccess == terresult) {
                    sleassetstate->setfieldu64 (sflownode, ulownode);
                    sleassetstate->setfieldu64 (sfhighnode, uhighnode);
                    sleassetstate->setfieldaccount(sfaccount, ureceiverid);
                    sleassetstate->setfieldamount(sfamount, amount);
                    
                    incrementownercount(ureceiverid);
                }
            }
            else
            {
                stamount before = sleassetstate->getfieldamount(sfamount);
                sleassetstate->setfieldamount(sfamount, before + amount);
                sleassetstate->setfieldu32(sfnextreleasetime, 0);
                entrymodify (sleassetstate);
                terresult = tessuccess;
            }
            if (tessuccess == terresult && !sleripplestate) {
                stamount sareceiverlimit({currency, ureceiverid}, getconfig().asset_limit_default);
                stamount sabalance({currency, noaccount()});

                writelog(lsdebug, ledgerentryset) << "ripplecredit: create line: " << to_string(usenderid) << " -> " << to_string(ureceiverid) << " : " << saamount.getfulltext();

                terresult = trustcreate(
                    bsenderhigh,
                    usenderid,
                    ureceiverid,
                    uindex,
                    entrycache(ltaccount_root, getaccountrootindex(ureceiverid)),
                    false,
                    true,
                    false,
                    sabalance,
                    sareceiverlimit);
                if (tessuccess == terresult)
                    sleripplestate = entrycache(ltripple_state, uindex);
            }
            // move released amount to trustline
            if (tessuccess == terresult)
                assetrelease(usenderid, ureceiverid, currency, sleripplestate);
            return terresult;
        }
    }
    
    if (!sleripplestate)
    {
        stamount sareceiverlimit({currency, ureceiverid});
        stamount sabalance = saamount;

        sabalance.setissuer (noaccount());

        writelog (lsdebug, ledgerentryset) << "ripplecredit: "
            "create line: " << to_string (usenderid) <<
            " -> " << to_string (ureceiverid) <<
            " : " << saamount.getfulltext ();

        terresult = trustcreate (
            bsenderhigh,
            usenderid,
            ureceiverid,
            uindex,
            entrycache (ltaccount_root, getaccountrootindex (ureceiverid)),
            false,
            false,
            false,
            sabalance,
            sareceiverlimit);
    }
    else
    {
        stamount    sabalance   = sleripplestate->getfieldamount (sfbalance);

        if (bsenderhigh)
            sabalance.negate ();    // put balance in sender terms.

        stamount    sabefore    = sabalance;

        sabalance   -= saamount;

        writelog (lstrace, ledgerentryset) << "ripplecredit: " <<
            to_string (usenderid) <<
            " -> " << to_string (ureceiverid) <<
            " : before=" << sabefore.getfulltext () <<
            " amount=" << saamount.getfulltext () <<
            " after=" << sabalance.getfulltext ();

        std::uint32_t const uflags (sleripplestate->getfieldu32 (sfflags));
        bool bdelete = false;

        // yyy could skip this if rippling in reverse.
        if (sabefore > zero
            // sender balance was positive.
            && sabalance <= zero
            // sender is zero or negative.
            && (uflags & (!bsenderhigh ? lsflowreserve : lsfhighreserve))
            // sender reserve is set.
            && !(uflags & (!bsenderhigh ? lsflownoripple : lsfhighnoripple))
            && !(uflags & (!bsenderhigh ? lsflowfreeze : lsfhighfreeze))
            && !sleripplestate->getfieldamount (
                !bsenderhigh ? sflowlimit : sfhighlimit)
            // sender trust limit is 0.
            && !sleripplestate->getfieldu32 (
                !bsenderhigh ? sflowqualityin : sfhighqualityin)
            // sender quality in is 0.
            && !sleripplestate->getfieldu32 (
                !bsenderhigh ? sflowqualityout : sfhighqualityout))
            // sender quality out is 0.
        {
            // clear the reserve of the sender, possibly delete the line!
            decrementownercount (usenderid);

            // clear reserve flag.
            sleripplestate->setfieldu32 (
                sfflags,
                uflags & (!bsenderhigh ? ~lsflowreserve : ~lsfhighreserve));

            // balance is zero, receiver reserve is clear.
            bdelete = !sabalance        // balance is zero.
                && !(uflags & (bsenderhigh ? lsflowreserve : lsfhighreserve));
            // receiver reserve is clear.
        }

        if (bsenderhigh)
            sabalance.negate ();

        // want to reflect balance to zero even if we are deleting line.
        sleripplestate->setfieldamount (sfbalance, sabalance);
        // only: adjust ripple balance.

        if (bdelete)
        {
            terresult   = trustdelete (
                sleripplestate,
                bsenderhigh ? ureceiverid : usenderid,
                !bsenderhigh ? ureceiverid : usenderid);
        }
        else
        {
            entrymodify (sleripplestate);
            terresult   = tessuccess;
        }
    }

    return terresult;
}

// send regardless of limits.
// --> saamount: amount/currency/issuer to deliver to reciever.
// <-- saactual: amount actually cost.  sender pay's fees.
ter ledgerentryset::ripplesend (
    account const& usenderid, account const& ureceiverid,
    stamount const& saamount, stamount& saactual)
{
    auto const issuer   = saamount.getissuer ();
    ter             terresult = tessuccess;

    assert (!isxrp (usenderid) && !isxrp (ureceiverid));
    assert (!isvbc (usenderid) && !isvbc (ureceiverid));
    assert (usenderid != ureceiverid);

    if (usenderid == issuer || ureceiverid == issuer || issuer == noaccount())
    {
        // direct send: redeeming ious and/or sending own ious.
        terresult   = ripplecredit (usenderid, ureceiverid, saamount, false);
        saactual    = saamount;
        terresult   = tessuccess;
    }
    else
    {
        // sending 3rd party ious: transit.

        stamount satransitfee = rippletransferfee (
            usenderid, ureceiverid, issuer, saamount);

        // share upto 25% of transfee with sender's ancestors (25% * 20% ecah).
        if (satransitfee)
        {
            stamount satransfeeshare = multiply(satransitfee, stamount(satransitfee.issue(), 25, -2));
            terresult = sharefeewithreferee(usenderid, issuer, satransfeeshare);
        }
        
        // actualfee = totalfee - ancestersharefee
        saactual = !satransitfee ? saamount : saamount + satransitfee;

        saactual.setissuer (issuer); // xxx make sure this done in + above.

        writelog (lsdebug, ledgerentryset) << "ripplesend> " <<
            to_string (usenderid) <<
            " - > " << to_string (ureceiverid) <<
            " : deliver=" << saamount.getfulltext () <<
            " fee=" << satransitfee.getfulltext () <<
            " cost=" << saactual.getfulltext ();

        if (tessuccess == terresult)
            terresult   = ripplecredit (issuer, ureceiverid, saamount);

        if (tessuccess == terresult)
            terresult   = ripplecredit (usenderid, issuer, saactual);
    }
    
    return terresult;
}

ter ledgerentryset::accountsend (
    account const& usenderid, account const& ureceiverid,
    stamount const& saamount)
{
    assert (saamount >= zero);

    /* if we aren't sending anything or if the sender is the same as the
     * receiver then we don't need to do anything.
     */
    if (!saamount || (usenderid == ureceiverid))
        return tessuccess;

    if (!saamount.isnative ())
    {
        stamount saactual;

        writelog (lstrace, ledgerentryset) << "accountsend: " <<
            to_string (usenderid) << " -> " << to_string (ureceiverid) <<
            " : " << saamount.getfulltext ();

        return ripplesend (usenderid, ureceiverid, saamount, saactual);
    }

    /* xrp or vbc send which does not check reserve and can do pure adjustment.
     * note that sender or receiver may be null and this not a mistake; this
     * setup is used during pathfinding and it is carefully controlled to
     * ensure that transfers are balanced.
     */

    ter terresult (tessuccess);

    sle::pointer sender = usenderid != beast::zero
        ? entrycache (ltaccount_root, getaccountrootindex (usenderid))
        : sle::pointer ();
    sle::pointer receiver = ureceiverid != beast::zero
        ? entrycache (ltaccount_root, getaccountrootindex (ureceiverid))
        : sle::pointer ();

    if (shouldlog (lstrace, ledgerentryset))
    {
        std::string sender_bal ("-");
        std::string receiver_bal ("-");

        bool bvbc = isvbc(saamount);

        if (sender)
            sender_bal = sender->getfieldamount (bvbc?sfbalancevbc:sfbalance).getfulltext();

        if (receiver)
            receiver_bal = receiver->getfieldamount (bvbc?sfbalancevbc:sfbalance).getfulltext();

        writelog (lstrace, ledgerentryset) << "accountsend> " <<
            to_string (usenderid) << " (" << sender_bal <<
            ") -> " << to_string (ureceiverid) << " (" << receiver_bal <<
            ") : " << saamount.getfulltext ();
    }

    bool const bvbc (isvbc(saamount));

    if (sender)
    {
        if (sender->getfieldamount (bvbc?sfbalancevbc:sfbalance) < saamount)
        {
            terresult = (mparams & tapopen_ledger)
                ? telfailed_processing
                : tecfailed_processing;
        }
        else
        {
            // decrement xrp balance.
            sender->setfieldamount (bvbc?sfbalancevbc:sfbalance,
                sender->getfieldamount (bvbc?sfbalancevbc:sfbalance) - saamount);
            entrymodify (sender);
        }
    }

    if (tessuccess == terresult && receiver)
    {
        // increment xrp balance.
        receiver->setfieldamount (bvbc?sfbalancevbc:sfbalance,
            receiver->getfieldamount (bvbc?sfbalancevbc:sfbalance) + saamount);
        entrymodify (receiver);
    }

    if (shouldlog (lstrace, ledgerentryset))
    {
        std::string sender_bal ("-");
        std::string receiver_bal ("-");

        if (sender)
            sender_bal = sender->getfieldamount (bvbc?sfbalancevbc:sfbalance).getfulltext ();

        if (receiver)
            receiver_bal = receiver->getfieldamount (bvbc?sfbalancevbc:sfbalance).getfulltext ();

        writelog (lstrace, ledgerentryset) << "accountsend< " <<
            to_string (usenderid) << " (" << sender_bal <<
            ") -> " << to_string (ureceiverid) << " (" << receiver_bal <<
            ") : " << saamount.getfulltext ();
    }

    return terresult;
}

std::uint32_t
rippletransferrate (ledgerentryset& ledger, account const& issuer)
{
    sle::pointer sleaccount (ledger.entrycache (
        ltaccount_root, getaccountrootindex (issuer)));

    std::uint32_t quality = quality_one;

    if (sleaccount && sleaccount->isfieldpresent (sftransferrate))
        quality = sleaccount->getfieldu32 (sftransferrate);

    return quality;
}

std::uint32_t
rippletransferrate (ledgerentryset& ledger, account const& usenderid,
    account const& ureceiverid, account const& issuer)
{
    // if calculating the transfer rate from or to the issuer of the currency
    // no fees are assessed.
    return (usenderid == issuer || ureceiverid == issuer)
           ? quality_one
           : rippletransferrate (ledger, issuer);
}

} // ripple
