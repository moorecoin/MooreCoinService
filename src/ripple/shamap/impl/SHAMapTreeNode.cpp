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
#include <ripple/shamap/shamaptreenode.h>
#include <ripple/basics/log.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/protocol/hashprefix.h>
#include <beast/module/core/text/lexicalcast.h>
#include <mutex>

namespace ripple {

std::mutex shamaptreenode::childlock;

shamaptreenode::shamaptreenode (std::uint32_t seq)
    : mseq (seq)
    , mtype (tnerror)
    , misbranch (0)
    , mfullbelowgen (0)
{
}

shamaptreenode::shamaptreenode (const shamaptreenode& node, std::uint32_t seq)
    : mhash (node.mhash)
    , mseq (seq)
    , mtype (node.mtype)
    , misbranch (node.misbranch)
    , mfullbelowgen (0)
{
    if (node.mitem)
        mitem = node.mitem;
    else
    {
        memcpy (mhashes, node.mhashes, sizeof (mhashes));

        std::unique_lock <std::mutex> lock (childlock);

        for (int i = 0; i < 16; ++i)
            mchildren[i] = node.mchildren[i];
    }
}

shamaptreenode::shamaptreenode (shamapitem::ref item,
                                tntype type, std::uint32_t seq)
    : mitem (item)
    , mseq (seq)
    , mtype (type)
    , misbranch (0)
    , mfullbelowgen (0)
{
    assert (item->peekdata ().size () >= 12);
    updatehash ();
}

shamaptreenode::shamaptreenode (blob const& rawnode,
                                std::uint32_t seq, shanodeformat format,
                                uint256 const& hash, bool hashvalid)
    : mseq (seq)
    , mtype (tnerror)
    , misbranch (0)
    , mfullbelowgen (0)
{
    if (format == snfwire)
    {
        serializer s (rawnode);
        int type = s.removelastbyte ();
        int len = s.getlength ();

        if ((type < 0) || (type > 4))
        {
#ifdef beast_debug
            deprecatedlogs().journal("shamaptreenode").fatal <<
                "invalid wire format node" << strhex (rawnode);
            assert (false);
#endif
            throw std::runtime_error ("invalid node aw type");
        }

        if (type == 0)
        {
            // transaction
            mitem = std::make_shared<shamapitem> (s.getprefixhash (hashprefix::transactionid), s.peekdata ());
            mtype = tntransaction_nm;
        }
        else if (type == 1)
        {
            // account state
            if (len < (256 / 8))
                throw std::runtime_error ("short as node");

            uint256 u;
            s.get256 (u, len - (256 / 8));
            s.chop (256 / 8);

            if (u.iszero ()) throw std::runtime_error ("invalid as node");

            mitem = std::make_shared<shamapitem> (u, s.peekdata ());
            mtype = tnaccount_state;
        }
        else if (type == 2)
        {
            // full inner
            if (len != 512)
                throw std::runtime_error ("invalid fi node");

            for (int i = 0; i < 16; ++i)
            {
                s.get256 (mhashes[i], i * 32);

                if (mhashes[i].isnonzero ())
                    misbranch |= (1 << i);
            }

            mtype = tninner;
        }
        else if (type == 3)
        {
            // compressed inner
            for (int i = 0; i < (len / 33); ++i)
            {
                int pos;
                s.get8 (pos, 32 + (i * 33));

                if ((pos < 0) || (pos >= 16)) throw std::runtime_error ("invalid ci node");

                s.get256 (mhashes[pos], i * 33);

                if (mhashes[pos].isnonzero ())
                    misbranch |= (1 << pos);
            }

            mtype = tninner;
        }
        else if (type == 4)
        {
            // transaction with metadata
            if (len < (256 / 8))
                throw std::runtime_error ("short tm node");

            uint256 u;
            s.get256 (u, len - (256 / 8));
            s.chop (256 / 8);

            if (u.iszero ())
                throw std::runtime_error ("invalid tm node");

            mitem = std::make_shared<shamapitem> (u, s.peekdata ());
            mtype = tntransaction_md;
        }
    }

    else if (format == snfprefix)
    {
        if (rawnode.size () < 4)
        {
            writelog (lsinfo, shamapnodeid) << "size < 4";
            throw std::runtime_error ("invalid p node");
        }

        std::uint32_t prefix = rawnode[0];
        prefix <<= 8;
        prefix |= rawnode[1];
        prefix <<= 8;
        prefix |= rawnode[2];
        prefix <<= 8;
        prefix |= rawnode[3];
        serializer s (rawnode.begin () + 4, rawnode.end ());

        if (prefix == hashprefix::transactionid)
        {
            mitem = std::make_shared<shamapitem> (serializer::getsha512half (rawnode), s.peekdata ());
            mtype = tntransaction_nm;
        }
        else if (prefix == hashprefix::leafnode)
        {
            if (s.getlength () < 32)
                throw std::runtime_error ("short pln node");

            uint256 u;
            s.get256 (u, s.getlength () - 32);
            s.chop (32);

            if (u.iszero ())
            {
                writelog (lsinfo, shamapnodeid) << "invalid pln node";
                throw std::runtime_error ("invalid pln node");
            }

            mitem = std::make_shared<shamapitem> (u, s.peekdata ());
            mtype = tnaccount_state;
        }
        else if (prefix == hashprefix::innernode)
        {
            if (s.getlength () != 512)
                throw std::runtime_error ("invalid pin node");

            for (int i = 0; i < 16; ++i)
            {
                s.get256 (mhashes[i], i * 32);

                if (mhashes[i].isnonzero ())
                    misbranch |= (1 << i);
            }

            mtype = tninner;
        }
        else if (prefix == hashprefix::txnode)
        {
            // transaction with metadata
            if (s.getlength () < 32)
                throw std::runtime_error ("short txn node");

            uint256 txid;
            s.get256 (txid, s.getlength () - 32);
            s.chop (32);
            mitem = std::make_shared<shamapitem> (txid, s.peekdata ());
            mtype = tntransaction_md;
        }
        else
        {
            writelog (lsinfo, shamapnodeid) << "unknown node prefix " << std::hex << prefix << std::dec;
            throw std::runtime_error ("invalid node prefix");
        }
    }

    else
    {
        assert (false);
        throw std::runtime_error ("unknown format");
    }

    if (hashvalid)
    {
        mhash = hash;
#if ripple_verify_nodeobject_keys
        updatehash ();
        assert (mhash == hash);
#endif
    }
    else
        updatehash ();
}

bool shamaptreenode::updatehash ()
{
    uint256 nh;

    if (mtype == tninner)
    {
        if (misbranch != 0)
        {
            nh = serializer::getprefixhash (hashprefix::innernode, reinterpret_cast<unsigned char*> (mhashes), sizeof (mhashes));
#if ripple_verify_nodeobject_keys
            serializer s;
            s.add32 (hashprefix::innernode);

            for (int i = 0; i < 16; ++i)
                s.add256 (mhashes[i]);

            assert (nh == s.getsha512half ());
#endif
        }
        else
            nh.zero ();
    }
    else if (mtype == tntransaction_nm)
    {
        nh = serializer::getprefixhash (hashprefix::transactionid, mitem->peekdata ());
    }
    else if (mtype == tnaccount_state)
    {
        serializer s (mitem->peekserializer ().getdatalength () + (256 + 32) / 8);
        s.add32 (hashprefix::leafnode);
        s.addraw (mitem->peekdata ());
        s.add256 (mitem->gettag ());
        nh = s.getsha512half ();
    }
    else if (mtype == tntransaction_md)
    {
        serializer s (mitem->peekserializer ().getdatalength () + (256 + 32) / 8);
        s.add32 (hashprefix::txnode);
        s.addraw (mitem->peekdata ());
        s.add256 (mitem->gettag ());
        nh = s.getsha512half ();
    }
    else
        assert (false);

    if (nh == mhash)
        return false;

    mhash = nh;
    return true;
}

void shamaptreenode::addraw (serializer& s, shanodeformat format)
{
    assert ((format == snfprefix) || (format == snfwire) || (format == snfhash));

    if (mtype == tnerror)
        throw std::runtime_error ("invalid i node type");

    if (format == snfhash)
    {
        s.add256 (getnodehash ());
    }
    else if (mtype == tninner)
    {
        assert (!isempty ());

        if (format == snfprefix)
        {
            s.add32 (hashprefix::innernode);

            for (int i = 0; i < 16; ++i)
                s.add256 (mhashes[i]);
        }
        else
        {
            if (getbranchcount () < 12)
            {
                // compressed node
                for (int i = 0; i < 16; ++i)
                    if (!isemptybranch (i))
                    {
                        s.add256 (mhashes[i]);
                        s.add8 (i);
                    }

                s.add8 (3);
            }
            else
            {
                for (int i = 0; i < 16; ++i)
                    s.add256 (mhashes[i]);

                s.add8 (2);
            }
        }
    }
    else if (mtype == tnaccount_state)
    {
        if (format == snfprefix)
        {
            s.add32 (hashprefix::leafnode);
            s.addraw (mitem->peekdata ());
            s.add256 (mitem->gettag ());
        }
        else
        {
            s.addraw (mitem->peekdata ());
            s.add256 (mitem->gettag ());
            s.add8 (1);
        }
    }
    else if (mtype == tntransaction_nm)
    {
        if (format == snfprefix)
        {
            s.add32 (hashprefix::transactionid);
            s.addraw (mitem->peekdata ());
        }
        else
        {
            s.addraw (mitem->peekdata ());
            s.add8 (0);
        }
    }
    else if (mtype == tntransaction_md)
    {
        if (format == snfprefix)
        {
            s.add32 (hashprefix::txnode);
            s.addraw (mitem->peekdata ());
            s.add256 (mitem->gettag ());
        }
        else
        {
            s.addraw (mitem->peekdata ());
            s.add256 (mitem->gettag ());
            s.add8 (4);
        }
    }
    else
        assert (false);
}

bool shamaptreenode::setitem (shamapitem::ref i, tntype type)
{
    mtype = type;
    mitem = i;
    assert (isleaf ());
    assert (mseq != 0);
    return updatehash ();
}

bool shamaptreenode::isempty () const
{
    return misbranch == 0;
}

int shamaptreenode::getbranchcount () const
{
    assert (isinner ());
    int count = 0;

    for (int i = 0; i < 16; ++i)
        if (!isemptybranch (i))
            ++count;

    return count;
}

void shamaptreenode::makeinner ()
{
    mitem.reset ();
    misbranch = 0;
    memset (mhashes, 0, sizeof (mhashes));
    mtype = tninner;
    mhash.zero ();
}

void shamaptreenode::dump (const shamapnodeid & id, beast::journal journal)
{
    if (journal.debug) journal.debug <<
        "shamaptreenode(" << id.getnodeid () << ")";
}

std::string shamaptreenode::getstring (const shamapnodeid & id) const
{
    std::string ret = "nodeid(";
    ret += beast::lexicalcastthrow <std::string> (id.getdepth ());
    ret += ",";
    ret += to_string (id.getnodeid ());
    ret += ")";

    if (isinner ())
    {
        for (int i = 0; i < 16; ++i)
            if (!isemptybranch (i))
            {
                ret += "\nb";
                ret += beast::lexicalcastthrow <std::string> (i);
                ret += " = ";
                ret += to_string (mhashes[i]);
            }
    }

    if (isleaf ())
    {
        if (mtype == tntransaction_nm)
            ret += ",txn\n";
        else if (mtype == tntransaction_md)
            ret += ",txn+md\n";
        else if (mtype == tnaccount_state)
            ret += ",as\n";
        else
            ret += ",leaf\n";

        ret += "  tag=";
        ret += to_string (gettag ());
        ret += "\n  hash=";
        ret += to_string (mhash);
        ret += "/";
        ret += beast::lexicalcast <std::string> (mitem->peekserializer ().getdatalength ());
    }

    return ret;
}

// we are modifying an inner node
bool shamaptreenode::setchild (int m, uint256 const& hash, shamaptreenode::ref child)
{
    assert ((m >= 0) && (m < 16));
    assert (mtype == tninner);
    assert (mseq != 0);
    assert (child.get() != this);

    if (mhashes[m] == hash)
        return false;

    mhashes[m] = hash;

    if (hash.isnonzero ())
    {
        assert (child && (child->getnodehash() == hash));
        misbranch |= (1 << m);
    }
    else
    {
        assert (!child);
        misbranch &= ~ (1 << m);
    }

    mchildren[m] = child;

    return updatehash ();
}

// finished modifying, now make shareable
void shamaptreenode::sharechild (int m, shamaptreenode::ref child)
{
    assert ((m >= 0) && (m < 16));
    assert (mtype == tninner);
    assert (mseq != 0);
    assert (child);
    assert (child.get() != this);
    assert (child->getnodehash() == mhashes[m]);

    mchildren[m] = child;
}

shamaptreenode* shamaptreenode::getchildpointer (int branch)
{
    assert (branch >= 0 && branch < 16);
    assert (isinnernode ());

    std::unique_lock <std::mutex> lock (childlock);
    return mchildren[branch].get ();
}

shamaptreenode::pointer shamaptreenode::getchild (int branch)
{
    assert (branch >= 0 && branch < 16);
    assert (isinnernode ());

    std::unique_lock <std::mutex> lock (childlock);
    assert (!mchildren[branch] || (mhashes[branch] == mchildren[branch]->getnodehash()));
    return mchildren[branch];
}

void shamaptreenode::canonicalizechild (int branch, shamaptreenode::pointer& node)
{
    assert (branch >= 0 && branch < 16);
    assert (isinnernode ());
    assert (node);
    assert (node->getnodehash() == mhashes[branch]);

    std::unique_lock <std::mutex> lock (childlock);
    if (mchildren[branch])
    {
        // there is already a node hooked up, return it
        node = mchildren[branch];
    }
    else
    {
        // hook this node up
        mchildren[branch] = node;
    }
}


} // ripple
