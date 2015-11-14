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
#include <ripple/shamap/shamap.h>
#include <beast/unit_test/suite.h>
#include <beast/chrono/manual_clock.h>

namespace ripple {

shamap::shamap (
    shamaptype t,
    fullbelowcache& fullbelowcache,
    treenodecache& treenodecache,
    nodestore::database& db,
    missingnodehandler missing_node_handler,
    beast::journal journal,
    std::uint32_t seq)
    : journal_(journal)
    , db_(db)
    , m_fullbelowcache (fullbelowcache)
    , mseq (seq)
    , mledgerseq (0)
    , mtreenodecache (treenodecache)
    , mstate (smsmodifying)
    , mtype (t)
    , m_missing_node_handler (missing_node_handler)
{
    assert (mseq != 0);

    root = std::make_shared<shamaptreenode> (mseq);
    root->makeinner ();
}

shamap::shamap (
    shamaptype t,
    uint256 const& hash,
    fullbelowcache& fullbelowcache,
    treenodecache& treenodecache,
    nodestore::database& db,
    missingnodehandler missing_node_handler,
    beast::journal journal)
    : journal_(journal)
    , db_(db)
    , m_fullbelowcache (fullbelowcache)
    , mseq (1)
    , mledgerseq (0)
    , mtreenodecache (treenodecache)
    , mstate (smssynching)
    , mtype (t)
    , m_missing_node_handler (missing_node_handler)
{
    root = std::make_shared<shamaptreenode> (mseq);
    root->makeinner ();
}

shamap::~shamap ()
{
    mstate = smsinvalid;
}

shamap::pointer shamap::snapshot (bool ismutable)
{
    shamap::pointer ret = std::make_shared<shamap> (mtype,
        m_fullbelowcache, mtreenodecache, db_, m_missing_node_handler,
            journal_);
    shamap& newmap = *ret;

    if (!ismutable)
        newmap.mstate = smsimmutable;

    newmap.mseq = mseq + 1;
    newmap.root = root;

    if ((mstate != smsimmutable) || !ismutable)
    {
        // if either map may change, they cannot share nodes
        newmap.unshare ();
    }

    return ret;
}

shamap::sharedptrnodestack
shamap::getstack (uint256 const& id, bool include_nonmatching_leaf)
{
    // walk the tree as far as possible to the specified identifier
    // produce a stack of nodes along the way, with the terminal node at the top
    sharedptrnodestack stack;

    shamaptreenode::pointer node = root;
    shamapnodeid nodeid;

    while (!node->isleaf ())
    {
        stack.push ({node, nodeid});

        int branch = nodeid.selectbranch (id);
        assert (branch >= 0);

        if (node->isemptybranch (branch))
            return stack;

        node = descendthrow (node, branch);
        nodeid = nodeid.getchildnodeid (branch);
    }

    if (include_nonmatching_leaf || (node->peekitem ()->gettag () == id))
        stack.push ({node, nodeid});

    return stack;
}

void
shamap::dirtyup (sharedptrnodestack& stack,
                 uint256 const& target, shamaptreenode::pointer child)
{
    // walk the tree up from through the inner nodes to the root
    // update hashes and links
    // stack is a path of inner nodes up to, but not including, child
    // child can be an inner node or a leaf

    assert ((mstate != smssynching) && (mstate != smsimmutable));
    assert (child && (child->getseq() == mseq));

    while (!stack.empty ())
    {
        shamaptreenode::pointer node = stack.top ().first;
        shamapnodeid nodeid = stack.top ().second;
        stack.pop ();
        assert (node->isinnernode ());

        int branch = nodeid.selectbranch (target);
        assert (branch >= 0);

        unsharenode (node, nodeid);

        if (! node->setchild (branch, child->getnodehash(), child))
        {
            journal_.fatal <<
                "dirtyup terminates early";
            assert (false);
            return;
        }

    #ifdef st_debug
        if (journal_.trace) journal_.trace <<
            "dirtyup sets branch " << branch << " to " << prevhash;
    #endif
        child = std::move (node);
    }
}

shamaptreenode* shamap::walktopointer (uint256 const& id)
{
    shamaptreenode* innode = root.get ();
    shamapnodeid nodeid;
    uint256 nodehash;

    while (innode->isinner ())
    {
        int branch = nodeid.selectbranch (id);

        if (innode->isemptybranch (branch))
            return nullptr;

        innode = descendthrow (innode, branch);
        nodeid = nodeid.getchildnodeid (branch);
    }

    return (innode->gettag () == id) ? innode : nullptr;
}

shamaptreenode::pointer shamap::fetchnodefromdb (uint256 const& hash)
{
    shamaptreenode::pointer node;

    if (mbacked)
    {
        nodeobject::pointer obj = db_.fetch (hash);
        if (obj)
        {
            try
            {
                node = std::make_shared <shamaptreenode> (obj->getdata(),
                    0, snfprefix, hash, true);
                canonicalize (hash, node);
            }
            catch (...)
            {
                if (journal_.warning) journal_.warning <<
                    "invalid db node " << hash;
                return shamaptreenode::pointer ();
            }
        }
        else if (mledgerseq != 0)
        {
            m_missing_node_handler (mledgerseq);
            mledgerseq = 0;
        }
    }

    return node;
}

// see if a sync filter has a node
shamaptreenode::pointer shamap::checkfilter (
    uint256 const& hash,
    shamapnodeid const& id,
    shamapsyncfilter* filter)
{
    shamaptreenode::pointer node;
    blob nodedata;

    if (filter->havenode (id, hash, nodedata))
    {
        node = std::make_shared <shamaptreenode> (
            nodedata, 0, snfprefix, hash, true);

       filter->gotnode (true, id, hash, nodedata, node->gettype ());

       if (mbacked)
           canonicalize (hash, node);
    }

    return node;
}

// get a node without throwing
// used on maps where missing nodes are expected
shamaptreenode::pointer shamap::fetchnodent(
    shamapnodeid const& id,
    uint256 const& hash,
    shamapsyncfilter* filter)
{
    shamaptreenode::pointer node = getcache (hash);
    if (node)
        return node;

    if (mbacked)
    {
        node = fetchnodefromdb (hash);
        if (node)
        {
            canonicalize (hash, node);
            return node;
        }
    }

    if (filter)
        node = checkfilter (hash, id, filter);

    return node;
}

shamaptreenode::pointer shamap::fetchnodent (uint256 const& hash)
{
    shamaptreenode::pointer node = getcache (hash);

    if (!node && mbacked)
        node = fetchnodefromdb (hash);

    return node;
}

// throw if the node is missing
shamaptreenode::pointer shamap::fetchnode (uint256 const& hash)
{
    shamaptreenode::pointer node = fetchnodent (hash);

    if (!node)
        throw shamapmissingnode (mtype, hash);

    return node;
}

shamaptreenode* shamap::descendthrow (shamaptreenode* parent, int branch)
{
    shamaptreenode* ret = descend (parent, branch);

    if (! ret && ! parent->isemptybranch (branch))
        throw shamapmissingnode (mtype, parent->getchildhash (branch));

    return ret;
}

shamaptreenode::pointer shamap::descendthrow (shamaptreenode::ref parent, int branch)
{
    shamaptreenode::pointer ret = descend (parent, branch);

    if (! ret && ! parent->isemptybranch (branch))
        throw shamapmissingnode (mtype, parent->getchildhash (branch));

    return ret;
}

shamaptreenode* shamap::descend (shamaptreenode* parent, int branch)
{
    shamaptreenode* ret = parent->getchildpointer (branch);
    if (ret || !mbacked)
        return ret;

    shamaptreenode::pointer node = fetchnodent (parent->getchildhash (branch));
    if (!node)
        return nullptr;

    parent->canonicalizechild (branch, node);
    return node.get ();
}

shamaptreenode::pointer shamap::descend (shamaptreenode::ref parent, int branch)
{
    shamaptreenode::pointer node = parent->getchild (branch);
    if (node || !mbacked)
        return node;

    node = fetchnode (parent->getchildhash (branch));
    if (!node)
        return nullptr;

    parent->canonicalizechild (branch, node);
    return node;
}

// gets the node that would be hooked to this branch,
// but doesn't hook it up.
shamaptreenode::pointer shamap::descendnostore (shamaptreenode::ref parent, int branch)
{
    shamaptreenode::pointer ret = parent->getchild (branch);
    if (!ret && mbacked)
        ret = fetchnode (parent->getchildhash (branch));
    return ret;
}

std::pair <shamaptreenode*, shamapnodeid>
shamap::descend (shamaptreenode * parent, shamapnodeid const& parentid,
    int branch, shamapsyncfilter * filter)
{
    assert (parent->isinner ());
    assert ((branch >= 0) && (branch < 16));
    assert (!parent->isemptybranch (branch));

    shamapnodeid childid = parentid.getchildnodeid (branch);
    shamaptreenode* child = parent->getchildpointer (branch);
    uint256 const& childhash = parent->getchildhash (branch);

    if (!child)
    {
        shamaptreenode::pointer childnode = fetchnodent (childid, childhash, filter);

        if (childnode)
        {
            parent->canonicalizechild (branch, childnode);
            child = childnode.get ();
        }
    }

    return std::make_pair (child, childid);
}

shamaptreenode* shamap::descendasync (shamaptreenode* parent, int branch,
    shamapnodeid const& childid, shamapsyncfilter * filter, bool & pending)
{
    pending = false;

    shamaptreenode* ret = parent->getchildpointer (branch);
    if (ret)
        return ret;

    uint256 const& hash = parent->getchildhash (branch);

    shamaptreenode::pointer ptr = getcache (hash);
    if (!ptr)
    {
        if (filter)
            ptr = checkfilter (hash, childid, filter);

        if (!ptr && mbacked)
        {
            nodeobject::pointer obj;
            if (! db_.asyncfetch (hash, obj))
            {
                pending = true;
                return nullptr;
            }
            if (!obj)
                return nullptr;

            ptr = std::make_shared <shamaptreenode> (obj->getdata(), 0, snfprefix, hash, true);

            if (mbacked)
                canonicalize (hash, ptr);
        }
    }

    if (ptr)
        parent->canonicalizechild (branch, ptr);

    return ptr.get ();
}

void
shamap::unsharenode (shamaptreenode::pointer& node, shamapnodeid const& nodeid)
{
    // make sure the node is suitable for the intended operation (copy on write)
    assert (node->isvalid ());
    assert (node->getseq () <= mseq);

    if (node->getseq () != mseq)
    {
        // have a cow
        assert (mstate != smsimmutable);

        node = std::make_shared<shamaptreenode> (*node, mseq); // here's to the new node, same as the old node
        assert (node->isvalid ());

        if (nodeid.isroot ())
            root = node;
    }
}

shamaptreenode*
shamap::firstbelow (shamaptreenode* node)
{
    // return the first item below this node
    do
    {
        assert(node != nullptr);

        if (node->hasitem ())
            return node;

        // walk down the tree
        bool foundnode = false;
        for (int i = 0; i < 16; ++i)
        {
            if (!node->isemptybranch (i))
            {
                node = descendthrow (node, i);
                foundnode = true;
                break;
            }
        }
        if (!foundnode)
            return nullptr;
    }
    while (true);
}

shamaptreenode*
shamap::lastbelow (shamaptreenode* node)
{
    do
    {
        if (node->hasitem ())
            return node;

        // walk down the tree
        bool foundnode = false;
        for (int i = 15; i >= 0; --i)
        {
            if (!node->isemptybranch (i))
            {
                node = descendthrow (node, i);
                foundnode = true;
                break;
            }
        }
        if (!foundnode)
            return nullptr;
    }
    while (true);
}

shamapitem::pointer
shamap::onlybelow (shamaptreenode* node)
{
    // if there is only one item below this node, return it

    while (!node->isleaf ())
    {
        shamaptreenode* nextnode = nullptr;
        for (int i = 0; i < 16; ++i)
        {
            if (!node->isemptybranch (i))
            {
                if (nextnode)
                    return shamapitem::pointer ();

                nextnode = descendthrow (node, i);
            }
        }

        if (!nextnode)
        {
            assert (false);
            return shamapitem::pointer ();
        }

        node = nextnode;
    }

    // an inner node must have at least one leaf
    // below it, unless it's the root
    assert (node->hasitem () || (node == root.get ()));

    return node->peekitem ();
}

static const shamapitem::pointer no_item;

shamapitem::pointer shamap::peekfirstitem ()
{
    shamaptreenode* node = firstbelow (root.get ());

    if (!node)
        return no_item;

    return node->peekitem ();
}

shamapitem::pointer shamap::peekfirstitem (shamaptreenode::tntype& type)
{
    shamaptreenode* node = firstbelow (root.get ());

    if (!node)
        return no_item;

    type = node->gettype ();
    return node->peekitem ();
}

shamapitem::pointer shamap::peeklastitem ()
{
    shamaptreenode* node = lastbelow (root.get ());

    if (!node)
        return no_item;

    return node->peekitem ();
}

shamapitem::pointer shamap::peeknextitem (uint256 const& id)
{
    shamaptreenode::tntype type;
    return peeknextitem (id, type);
}

shamapitem::pointer shamap::peeknextitem (uint256 const& id, shamaptreenode::tntype& type)
{
    // get a pointer to the next item in the tree after a given item - item need not be in tree

    auto stack = getstack (id, true);

    while (!stack.empty ())
    {
        shamaptreenode* node = stack.top().first.get();
        shamapnodeid nodeid = stack.top().second;
        stack.pop ();

        if (node->isleaf ())
        {
            if (node->peekitem ()->gettag () > id)
            {
                type = node->gettype ();
                return node->peekitem ();
            }
        }
        else
        {
            // breadth-first
            for (int i = nodeid.selectbranch (id) + 1; i < 16; ++i)
                if (!node->isemptybranch (i))
                {
                    node = descendthrow (node, i);
                    node = firstbelow (node);

                    if (!node || node->isinner ())
                        throw (std::runtime_error ("missing/corrupt node"));

                    type = node->gettype ();
                    return node->peekitem ();
                }
        }
    }

    // must be last item
    return no_item;
}

// get a pointer to the previous item in the tree after a given item - item need not be in tree
shamapitem::pointer shamap::peekprevitem (uint256 const& id)
{
    auto stack = getstack (id, true);

    while (!stack.empty ())
    {
        shamaptreenode* node = stack.top ().first.get();
        shamapnodeid nodeid = stack.top ().second;
        stack.pop ();

        if (node->isleaf ())
        {
            if (node->peekitem ()->gettag () < id)
                return node->peekitem ();
        }
        else
        {
            for (int i = nodeid.selectbranch (id) - 1; i >= 0; --i)
            {
                if (!node->isemptybranch (i))
                {
                    node = descendthrow (node, i);
                    node = lastbelow (node);
                    return node->peekitem ();
                }
            }
        }
    }

    // must be first item
    return no_item;
}

shamapitem::pointer shamap::peekitem (uint256 const& id)
{
    shamaptreenode* leaf = walktopointer (id);

    if (!leaf)
        return no_item;

    return leaf->peekitem ();
}

shamapitem::pointer shamap::peekitem (uint256 const& id, shamaptreenode::tntype& type)
{
    shamaptreenode* leaf = walktopointer (id);

    if (!leaf)
        return no_item;

    type = leaf->gettype ();
    return leaf->peekitem ();
}

shamapitem::pointer shamap::peekitem (uint256 const& id, uint256& hash)
{
    shamaptreenode* leaf = walktopointer (id);

    if (!leaf)
        return no_item;

    hash = leaf->getnodehash ();
    return leaf->peekitem ();
}


bool shamap::hasitem (uint256 const& id)
{
    // does the tree have an item with this id
    shamaptreenode* leaf = walktopointer (id);
    return (leaf != nullptr);
}

bool shamap::delitem (uint256 const& id)
{
    // delete the item with this id
    assert (mstate != smsimmutable);

    auto stack = getstack (id, true);

    if (stack.empty ())
        throw (std::runtime_error ("missing node"));

    shamaptreenode::pointer leaf = stack.top ().first;
    shamapnodeid leafid = stack.top ().second;
    stack.pop ();

    if (!leaf || !leaf->hasitem () || (leaf->peekitem ()->gettag () != id))
        return false;

    shamaptreenode::tntype type = leaf->gettype ();

    // what gets attached to the end of the chain
    // (for now, nothing, since we deleted the leaf)
    uint256 prevhash;
    shamaptreenode::pointer prevnode;

    while (!stack.empty ())
    {
        shamaptreenode::pointer node = stack.top ().first;
        shamapnodeid nodeid = stack.top ().second;
        stack.pop ();

        assert (node->isinner ());

        unsharenode (node, nodeid);
        if (! node->setchild (nodeid.selectbranch (id), prevhash, prevnode))
        {
            assert (false);
            return true;
        }

        if (!nodeid.isroot ())
        {
            // we may have made this a node with 1 or 0 children
            // and, if so, we need to remove this branch
            int bc = node->getbranchcount ();

            if (bc == 0)
            {
                // no children below this branch
                prevhash = uint256 ();
                prevnode.reset ();
            }
            else if (bc == 1)
            {
                // if there's only one item, pull up on the thread
                shamapitem::pointer item = onlybelow (node.get ());

                if (item)
                {
                    for (int i = 0; i < 16; ++i)
                    {
                        if (!node->isemptybranch (i))
                        {
                            if (! node->setchild (i, uint256(), nullptr))
                            {
                                assert (false);
                            }
                            break;
                        }
                    }
                    node->setitem (item, type);
                }

                prevhash = node->getnodehash ();
                prevnode = std::move (node);
                assert (prevhash.isnonzero ());
            }
            else
            {
                // this node is now the end of the branch
                prevhash = node->getnodehash ();
                prevnode = std::move (node);
                assert (prevhash.isnonzero ());
            }
        }
    }

    return true;
}

bool shamap::addgiveitem (shamapitem::ref item, bool istransaction, bool hasmeta)
{
    // add the specified item, does not update
    uint256 tag = item->gettag ();
    shamaptreenode::tntype type = !istransaction ? shamaptreenode::tnaccount_state :
        (hasmeta ? shamaptreenode::tntransaction_md : shamaptreenode::tntransaction_nm);

    assert (mstate != smsimmutable);

    auto stack = getstack (tag, true);

    if (stack.empty ())
        throw (std::runtime_error ("missing node"));

    shamaptreenode::pointer node = stack.top ().first;
    shamapnodeid nodeid = stack.top ().second;
    stack.pop ();

    if (node->isleaf () && (node->peekitem ()->gettag () == tag))
        return false;

    unsharenode (node, nodeid);
    if (node->isinner ())
    {
        // easy case, we end on an inner node
        int branch = nodeid.selectbranch (tag);
        assert (node->isemptybranch (branch));
        shamaptreenode::pointer newnode =
            std::make_shared<shamaptreenode> (item, type, mseq);
        if (! node->setchild (branch, newnode->getnodehash (), newnode))
        {
            assert (false);
        }
    }
    else
    {
        // this is a leaf node that has to be made an inner node holding two items
        shamapitem::pointer otheritem = node->peekitem ();
        assert (otheritem && (tag != otheritem->gettag ()));

        node->makeinner ();

        int b1, b2;

        while ((b1 = nodeid.selectbranch (tag)) ==
               (b2 = nodeid.selectbranch (otheritem->gettag ())))
        {
            stack.push ({node, nodeid});

            // we need a new inner node, since both go on same branch at this level
            nodeid = nodeid.getchildnodeid (b1);
            node = std::make_shared<shamaptreenode> (mseq);
            node->makeinner ();
        }

        // we can add the two leaf nodes here
        assert (node->isinner ());

        shamaptreenode::pointer newnode =
            std::make_shared<shamaptreenode> (item, type, mseq);
        assert (newnode->isvalid () && newnode->isleaf ());
        if (!node->setchild (b1, newnode->getnodehash (), newnode))
        {
            assert (false);
        }

        newnode = std::make_shared<shamaptreenode> (otheritem, type, mseq);
        assert (newnode->isvalid () && newnode->isleaf ());
        if (!node->setchild (b2, newnode->getnodehash (), newnode))
        {
            assert (false);
        }
    }

    dirtyup (stack, tag, node);
    return true;
}

bool shamap::additem (const shamapitem& i, bool istransaction, bool hasmetadata)
{
    return addgiveitem (std::make_shared<shamapitem> (i), istransaction, hasmetadata);
}

bool shamap::updategiveitem (shamapitem::ref item, bool istransaction, bool hasmeta)
{
    // can't change the tag but can change the hash
    uint256 tag = item->gettag ();

    assert (mstate != smsimmutable);

    auto stack = getstack (tag, true);

    if (stack.empty ())
        throw (std::runtime_error ("missing node"));

    shamaptreenode::pointer node = stack.top ().first;
    shamapnodeid nodeid = stack.top ().second;
    stack.pop ();

    if (!node->isleaf () || (node->peekitem ()->gettag () != tag))
    {
        assert (false);
        return false;
    }

    unsharenode (node, nodeid);

    if (!node->setitem (item, !istransaction ? shamaptreenode::tnaccount_state :
                        (hasmeta ? shamaptreenode::tntransaction_md : shamaptreenode::tntransaction_nm)))
    {
        if (journal_.warning) journal_.warning <<
            "shamap setitem, no change";
        return true;
    }

    dirtyup (stack, tag, node);
    return true;
}

bool shamap::fetchroot (uint256 const& hash, shamapsyncfilter* filter)
{
    if (hash == root->getnodehash ())
        return true;

    if (journal_.trace)
    {
        if (mtype == smttransaction)
        {
            journal_.trace
                << "fetch root txn node " << hash;
        }
        else if (mtype == smtstate)
        {
            journal_.trace <<
                "fetch root state node " << hash;
        }
        else
        {
            journal_.trace <<
                "fetch root shamap node " << hash;
        }
    }

    shamaptreenode::pointer newroot = fetchnodent (shamapnodeid(), hash, filter);

    if (newroot)
    {
        root = newroot;
        assert (root->getnodehash () == hash);
        return true;
    }

    return false;
}

// replace a node with a shareable node.
//
// this code handles two cases:
//
// 1) an unshared, unshareable node needs to be made shareable
// so immutable shamap's can have references to it.
//
// 2) an unshareable node is shared. this happens when you make
// a mutable snapshot of a mutable shamap.
void shamap::writenode (
    nodeobjecttype t, std::uint32_t seq, shamaptreenode::pointer& node)
{
    // node is ours, so we can just make it shareable
    assert (node->getseq() == mseq);
    assert (mbacked);
    node->setseq (0);

    canonicalize (node->getnodehash(), node);

    serializer s;
    node->addraw (s, snfprefix);
    db_.store (t, std::move (s.moddata()),
        node->getnodehash ());
}

// we can't modify an inner node someone else might have a
// pointer to because flushing modifies inner nodes -- it
// makes them point to canonical/shared nodes.
void shamap::preflushnode (shamaptreenode::pointer& node)
{
    // a shared node should never need to be flushed
    // because that would imply someone modified it
    assert (node->getseq() != 0);

    if (node->getseq() != mseq)
    {
        // node is not uniquely ours, so unshare it before
        // possibly modifying it
        node = std::make_shared <shamaptreenode> (*node, mseq);
    }
}

int shamap::unshare ()
{
    return walksubtree (false, hotunknown, 0);
}

/** convert all modified nodes to shared nodes */
// if requested, write them to the node store
int shamap::flushdirty (nodeobjecttype t, std::uint32_t seq)
{
    return walksubtree (true, t, seq);
}

int shamap::walksubtree (bool dowrite, nodeobjecttype t, std::uint32_t seq)
{
    int flushed = 0;
    serializer s;

    if (!root || (root->getseq() == 0) || root->isempty ())
        return flushed;

    if (root->isleaf())
    { // special case -- root is leaf
        preflushnode (root);
        if (dowrite && mbacked)
            writenode (t, seq, root);
        return 1;
    }

    // stack of {parent,index,child} pointers representing
    // inner nodes we are in the process of flushing
    using stackentry = std::pair <shamaptreenode::pointer, int>;
    std::stack <stackentry, std::vector<stackentry>> stack;

    shamaptreenode::pointer node = root;
    preflushnode (node);

    int pos = 0;

    // we can't flush an inner node until we flush its children
    while (1)
    {
        while (pos < 16)
        {
            if (node->isemptybranch (pos))
            {
                ++pos;
            }
            else
            {
                // no need to do i/o. if the node isn't linked,
                // it can't need to be flushed
                int branch = pos;
                shamaptreenode::pointer child = node->getchild (pos++);

                if (child && (child->getseq() != 0))
                {
                    // this is a node that needs to be flushed

                    if (child->isinner ())
                    {
                        // save our place and work on this node
                        preflushnode (child);

                        stack.emplace (std::move (node), branch);

                        node = std::move (child);
                        pos = 0;
                    }
                    else
                    {
                        // flush this leaf
                        ++flushed;

                        preflushnode (child);

                        assert (node->getseq() == mseq);

                        if (dowrite && mbacked)
                            writenode (t, seq, child);

                        node->sharechild (branch, child);
                    }
                }
            }
        }

        // this inner node can now be shared
        if (dowrite && mbacked)
            writenode (t, seq, node);

        ++flushed;

        if (stack.empty ())
           break;

        shamaptreenode::pointer parent = std::move (stack.top().first);
        pos = stack.top().second;
        stack.pop();

        // hook this inner node to its parent
        assert (parent->getseq() == mseq);
        parent->sharechild (pos, node);

        // continue with parent's next child, if any
        node = std::move (parent);
        ++pos;
    }

    // last inner node is the new root
    root = std::move (node);

    return flushed;
}

bool shamap::getpath (uint256 const& index, std::vector< blob >& nodes, shanodeformat format)
{
    // return the path of nodes to the specified index in the specified format
    // return value: true = node present, false = node not present

    shamaptreenode* innode = root.get ();
    shamapnodeid nodeid;

    while (innode->isinner ())
    {
        serializer s;
        innode->addraw (s, format);
        nodes.push_back (s.peekdata ());

        int branch = nodeid.selectbranch (index);
        if (innode->isemptybranch (branch))
            return false;

        innode = descendthrow (innode, branch);
        nodeid = nodeid.getchildnodeid (branch);
    }

    if (innode->gettag () != index) // path leads to different leaf
        return false;

    // path leads to the requested leaf
    serializer s;
    innode->addraw (s, format);
    nodes.push_back (std::move(s.peekdata ()));
    return true;
}

void shamap::dump (bool hash)
{
    int leafcount = 0;
    if (journal_.info) journal_.info <<
        " map contains";

    std::stack <std::pair <shamaptreenode*, shamapnodeid> > stack;
    stack.push ({root.get (), shamapnodeid ()});

    do
    {
        shamaptreenode* node = stack.top().first;
        shamapnodeid nodeid = stack.top().second;
        stack.pop();

        if (journal_.info) journal_.info <<
            node->getstring (nodeid);
        if (hash)
           if (journal_.info) journal_.info <<
               "hash: " << node->getnodehash();

        if (node->isinner ())
        {
            for (int i = 0; i < 16; ++i)
            {
                if (!node->isemptybranch (i))
                {
                    shamaptreenode* child = node->getchildpointer (i);
                    if (child)
                    {
                        assert (child->getnodehash() == node->getchildhash (i));
                        stack.push ({child, nodeid.getchildnodeid (i)});
                     }
                }
            }
        }
        else
            ++leafcount;
    }
    while (!stack.empty ());

    if (journal_.info) journal_.info <<
        leafcount << " resident leaves";
}

shamaptreenode::pointer shamap::getcache (uint256 const& hash)
{
    shamaptreenode::pointer ret = mtreenodecache.fetch (hash);
    assert (!ret || !ret->getseq());
    return ret;
}

void shamap::canonicalize (uint256 const& hash, shamaptreenode::pointer& node)
{
    assert (mbacked);
    assert (node->getseq() == 0);
    assert (node->getnodehash() == hash);

    mtreenodecache.canonicalize (hash, node);

}

} // ripple
