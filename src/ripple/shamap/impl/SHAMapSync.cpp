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
#include <ripple/nodestore/database.h>
#include <beast/unit_test/suite.h>

namespace ripple {

// vfalco todo tidy up this global

static const uint256 uzero;

static bool visitleaveshelper (
    std::function <void (shamapitem::ref)> const& function,
    shamaptreenode& node)
{
    // adapt visitnodes to visitleaves
    if (!node.isinner ())
        function (node.peekitem ());

    return false;
}

void shamap::visitleaves (std::function<void (shamapitem::ref item)> const& leaffunction)
{
    visitnodes (std::bind (visitleaveshelper,
            std::cref (leaffunction), std::placeholders::_1));
}

void shamap::visitnodes(std::function<bool (shamaptreenode&)> const& function)
{
    // visit every node in a shamap
    assert (root->isvalid ());

    if (!root || root->isempty ())
        return;

    function (*root);

    if (!root->isinner ())
        return;

    using stackentry = std::pair <int, shamaptreenode::pointer>;
    std::stack <stackentry, std::vector <stackentry>> stack;

    shamaptreenode::pointer node = root;
    int pos = 0;

    while (1)
    {
        while (pos < 16)
        {
            uint256 childhash;
            if (!node->isemptybranch (pos))
            {
                shamaptreenode::pointer child = descendnostore (node, pos);
                if (function (*child))
                    return;

                if (child->isleaf ())
                    ++pos;
                else
                {
                    // if there are no more children, don't push this node
                    while ((pos != 15) && (node->isemptybranch (pos + 1)))
                           ++pos;

                    if (pos != 15)
                    {
                        // save next position to resume at
                        stack.push (std::make_pair(pos + 1, std::move (node)));
                    }

                    // descend to the child's first position
                    node = child;
                    pos = 0;
                }
            }
            else
            {
                ++pos; // move to next position
            }
        }

        if (stack.empty ())
            break;

        std::tie(pos, node) = stack.top ();
        stack.pop ();
    }
}

/** get a list of node ids and hashes for nodes that are part of this shamap
    but not available locally.  the filter can hold alternate sources of
    nodes that are not permanently stored locally
*/
void shamap::getmissingnodes (std::vector<shamapnodeid>& nodeids, std::vector<uint256>& hashes, int max,
                              shamapsyncfilter* filter)
{
    assert (root->isvalid ());
    assert (root->getnodehash().isnonzero ());

    std::uint32_t generation = m_fullbelowcache.getgeneration();
    if (root->isfullbelow (generation))
    {
        clearsynching ();
        return;
    }

    if (!root->isinner ())
    {
        if (journal_.warning) journal_.warning <<
            "synching empty tree";
        return;
    }

    int const maxdefer = db_.getdesiredasyncreadcount ();

    // track the missing hashes we have found so far
    std::set <uint256> missinghashes;


    while (1)
    {
        std::vector <std::tuple <shamaptreenode*, int, shamapnodeid>> deferredreads;
        deferredreads.reserve (maxdefer + 16);

        using stackentry = std::tuple<shamaptreenode*, shamapnodeid, int, int, bool>;
        std::stack <stackentry, std::vector<stackentry>> stack;

        // traverse the map without blocking

        shamaptreenode *node = root.get ();
        shamapnodeid nodeid;

        // the firstchild value is selected randomly so if multiple threads
        // are traversing the map, each thread will start at a different
        // (randomly selected) inner node.  this increases the likelihood
        // that the two threads will produce different request sets (which is
        // more efficient than sending identical requests).
        int firstchild = rand() % 256;
        int currentchild = 0;
        bool fullbelow = true;

        do
        {
            while (currentchild < 16)
            {
                int branch = (firstchild + currentchild++) % 16;
                if (!node->isemptybranch (branch))
                {
                    uint256 const& childhash = node->getchildhash (branch);

                    if (! mbacked || ! m_fullbelowcache.touch_if_exists (childhash))
                    {
                        shamapnodeid childid = nodeid.getchildnodeid (branch);
                        bool pending = false;
                        shamaptreenode* d = descendasync (node, branch, childid, filter, pending);

                        if (!d)
                        {
                            if (!pending)
                            { // node is not in the database
                                if (missinghashes.insert (childhash).second)
                                {
                                    nodeids.push_back (childid);
                                    hashes.push_back (childhash);

                                    if (--max <= 0)
                                        return;
                                }
                            }
                            else
                            {
                                // read is deferred
                                deferredreads.emplace_back (node, branch, childid);
                            }

                            fullbelow = false; // this node is not known full below
                        }
                        else if (d->isinner () && !d->isfullbelow (generation))
                        {
                            stack.push (std::make_tuple (node, nodeid,
                                          firstchild, currentchild, fullbelow));

                            // switch to processing the child node
                            node = d;
                            nodeid = childid;
                            firstchild = rand() % 256;
                            currentchild = 0;
                            fullbelow = true;
                        }
                    }
                }
            }

            // we are done with this inner node (and thus all of its children)

            if (fullbelow)
            { // no partial node encountered below this node
                node->setfullbelowgen (generation);
                if (mbacked)
                    m_fullbelowcache.insert (node->getnodehash ());
            }

            if (stack.empty ())
                node = nullptr; // finished processing the last node, we are done
            else
            { // pick up where we left off (above this node)
                bool was;
                std::tie(node, nodeid, firstchild, currentchild, was) = stack.top ();
                fullbelow = was && fullbelow; // was and still is
                stack.pop ();
            }

        }
        while ((node != nullptr) && (deferredreads.size () <= maxdefer));

        // if we didn't defer any reads, we're done
        if (deferredreads.empty ())
            break;

        db_.waitreads();

        // process all deferred reads
        for (auto const& node : deferredreads)
        {
            auto parent = std::get<0>(node);
            auto branch = std::get<1>(node);
            auto const& nodeid = std::get<2>(node);
            auto const& nodehash = parent->getchildhash (branch);

            shamaptreenode::pointer nodeptr = fetchnodent (nodeid, nodehash, filter);
            if (nodeptr)
            {
                if (mbacked)
                    canonicalize (nodehash, nodeptr);
                parent->canonicalizechild (branch, nodeptr);
            }
            else if (missinghashes.insert (nodehash).second)
            {
                nodeids.push_back (nodeid);
                hashes.push_back (nodehash);

                if (--max <= 0)
                    return;
            }
        }

    }

    if (nodeids.empty ())
        clearsynching ();
}

std::vector<uint256> shamap::getneededhashes (int max, shamapsyncfilter* filter)
{
    std::vector<uint256> nodehashes;
    nodehashes.reserve(max);

    std::vector<shamapnodeid> nodeids;
    nodeids.reserve(max);

    getmissingnodes(nodeids, nodehashes, max, filter);
    return nodehashes;
}

bool shamap::getnodefat (shamapnodeid wanted, std::vector<shamapnodeid>& nodeids,
                         std::list<blob >& rawnodes, bool fatroot, bool fatleaves)
{
    // gets a node and some of its children

    shamaptreenode* node = root.get ();

    shamapnodeid nodeid;

    while (node && node->isinner () && (nodeid.getdepth() < wanted.getdepth()))
    {
        int branch = nodeid.selectbranch (wanted.getnodeid());
        node = descendthrow (node, branch);
        nodeid = nodeid.getchildnodeid (branch);
    }

    if (!node || (nodeid != wanted))
    {
        if (journal_.warning) journal_.warning <<
            "peer requested node that is not in the map: " << wanted;
        throw std::runtime_error ("peer requested node not in map");
    }

    if (node->isinner () && node->isempty ())
    {
        if (journal_.warning) journal_.warning <<
            "peer requests empty node";
        return false;
    }

    int count;
    bool skipnode = false;
    do
    {

        if (skipnode)
            skipnode = false;
        else
        {
            serializer s;
            node->addraw (s, snfwire);
            nodeids.push_back (wanted);
            rawnodes.push_back (std::move (s.peekdata ()));
        }

        if ((!fatroot && wanted.isroot ()) || node->isleaf ()) // don't get a fat root, can't get a fat leaf
            return true;

        shamaptreenode* nextnode = nullptr;
        shamapnodeid nextnodeid;

        count = 0;
        for (int i = 0; i < 16; ++i)
        {
            if (!node->isemptybranch (i))
            {
                shamapnodeid nextnodeid = wanted.getchildnodeid (i);
                nextnode = descendthrow (node, i);
                ++count;
                if (fatleaves || nextnode->isinner ())
                {
                    serializer s;
                    nextnode->addraw (s, snfwire);
                    nodeids.push_back (nextnodeid);
                    rawnodes.push_back (std::move (s.peekdata ()));
                    skipnode = true; // don't add this node again if we loop
                }
            }
        }

        node = nextnode;
        wanted = nextnodeid;

    // so long as there's exactly one inner node, we take it
    } while ((count == 1) && node->isinner());

    return true;
}

bool shamap::getrootnode (serializer& s, shanodeformat format)
{
    root->addraw (s, format);
    return true;
}

shamapaddnode shamap::addrootnode (blob const& rootnode,
    shanodeformat format, shamapsyncfilter* filter)
{
    // we already have a root node
    if (root->getnodehash ().isnonzero ())
    {
        if (journal_.trace) journal_.trace <<
            "got root node, already have one";
        return shamapaddnode::duplicate ();
    }

    assert (mseq >= 1);
    shamaptreenode::pointer node =
        std::make_shared<shamaptreenode> (rootnode, 0,
                                          format, uzero, false);

    if (!node)
        return shamapaddnode::invalid ();

#ifdef beast_debug
    node->dump (shamapnodeid (), journal_);
#endif

    if (mbacked)
        canonicalize (node->getnodehash (), node);

    root = node;

    if (root->isleaf())
        clearsynching ();

    if (filter)
    {
        serializer s;
        root->addraw (s, snfprefix);
        filter->gotnode (false, shamapnodeid{}, root->getnodehash (),
                         s.moddata (), root->gettype ());
    }

    return shamapaddnode::useful ();
}

shamapaddnode shamap::addrootnode (uint256 const& hash, blob const& rootnode, shanodeformat format,
                                   shamapsyncfilter* filter)
{
    // we already have a root node
    if (root->getnodehash ().isnonzero ())
    {
        if (journal_.trace) journal_.trace <<
            "got root node, already have one";
        assert (root->getnodehash () == hash);
        return shamapaddnode::duplicate ();
    }

    assert (mseq >= 1);
    shamaptreenode::pointer node =
        std::make_shared<shamaptreenode> (rootnode, 0,
                                          format, uzero, false);

    if (!node || node->getnodehash () != hash)
        return shamapaddnode::invalid ();

    if (mbacked)
        canonicalize (hash, node);

    root = node;

    if (root->isleaf())
        clearsynching ();

    if (filter)
    {
        serializer s;
        root->addraw (s, snfprefix);
        filter->gotnode (false, shamapnodeid{}, root->getnodehash (), s.moddata (),
                         root->gettype ());
    }

    return shamapaddnode::useful ();
}

shamapaddnode
shamap::addknownnode (const shamapnodeid& node, blob const& rawnode,
                      shamapsyncfilter* filter)
{
    // return value: true=okay, false=error
    assert (!node.isroot ());

    if (!issynching ())
    {
        if (journal_.trace) journal_.trace <<
            "addknownnode while not synching";
        return shamapaddnode::duplicate ();
    }

    std::uint32_t generation = m_fullbelowcache.getgeneration();
    shamapnodeid inodeid;
    shamaptreenode* inode = root.get ();

    while (inode->isinner () && !inode->isfullbelow (generation) &&
           (inodeid.getdepth () < node.getdepth ()))
    {
        int branch = inodeid.selectbranch (node.getnodeid ());
        assert (branch >= 0);

        if (inode->isemptybranch (branch))
        {
            if (journal_.warning) journal_.warning <<
                "add known node for empty branch" << node;
            return shamapaddnode::invalid ();
        }

        uint256 childhash = inode->getchildhash (branch);
        if (m_fullbelowcache.touch_if_exists (childhash))
            return shamapaddnode::duplicate ();

        shamaptreenode* prevnode = inode;
        std::tie (inode, inodeid) = descend (inode, inodeid, branch, filter);

        if (!inode)
        {
            if (inodeid != node)
            {
                // either this node is broken or we didn't request it (yet)
                if (journal_.warning) journal_.warning <<
                    "unable to hook node " << node;
                if (journal_.info) journal_.info <<
                    " stuck at " << inodeid;
                if (journal_.info) journal_.info <<
                    "got depth=" << node.getdepth () <<
                        ", walked to= " << inodeid.getdepth ();
                return shamapaddnode::invalid ();
            }

            shamaptreenode::pointer newnode =
                std::make_shared<shamaptreenode> (rawnode, 0, snfwire,
                                                  uzero, false);

            if (!newnode->isinbounds (inodeid))
            {
                // map is provably invalid
                mstate = smsinvalid;
                return shamapaddnode::useful ();
            }

            if (childhash != newnode->getnodehash ())
            {
                if (journal_.warning) journal_.warning <<
                    "corrupt node received";
                return shamapaddnode::invalid ();
            }

            if (mbacked)
                canonicalize (childhash, newnode);

            prevnode->canonicalizechild (branch, newnode);

            if (filter)
            {
                serializer s;
                newnode->addraw (s, snfprefix);
                filter->gotnode (false, node, childhash,
                                 s.moddata (), newnode->gettype ());
            }

            return shamapaddnode::useful ();
        }
    }

    if (journal_.trace) journal_.trace <<
        "got node, already had it (late)";
    return shamapaddnode::duplicate ();
}

bool shamap::deepcompare (shamap& other)
{
    // intended for debug/test only
    std::stack <std::pair <shamaptreenode*, shamaptreenode*> > stack;

    stack.push ({root.get(), other.root.get()});

    while (!stack.empty ())
    {
        shamaptreenode *node, *othernode;
        std::tie(node, othernode) = stack.top ();
        stack.pop ();

        if (!node || !othernode)
        {
            if (journal_.info) journal_.info <<
                "unable to fetch node";
            return false;
        }
        else if (othernode->getnodehash () != node->getnodehash ())
        {
            if (journal_.warning) journal_.warning <<
                "node hash mismatch";
            return false;
        }

        if (node->isleaf ())
        {
            if (!othernode->isleaf ())
                 return false;
            auto nodepeek = node->peekitem();
            auto othernodepeek = othernode->peekitem();
            if (nodepeek->gettag() != othernodepeek->gettag())
                return false;
            if (nodepeek->peekdata() != othernodepeek->peekdata())
                return false;
        }
        else if (node->isinner ())
        {
            if (!othernode->isinner ())
                return false;

            for (int i = 0; i < 16; ++i)
            {
                if (node->isemptybranch (i))
                {
                    if (!othernode->isemptybranch (i))
                        return false;
                }
                else
                {
                    if (othernode->isemptybranch (i))
                       return false;

                    shamaptreenode *next = descend (node, i);
                    shamaptreenode *othernext = other.descend (othernode, i);
                    if (!next || !othernext)
                    {
                        if (journal_.warning) journal_.warning <<
                            "unable to fetch inner node";
                        return false;
                    }
                    stack.push ({next, othernext});
                }
            }
        }
    }

    return true;
}

/** does this map have this inner node?
*/
bool
shamap::hasinnernode (shamapnodeid const& targetnodeid,
                      uint256 const& targetnodehash)
{
    shamaptreenode* node = root.get ();
    shamapnodeid nodeid;

    while (node->isinner () && (nodeid.getdepth () < targetnodeid.getdepth ()))
    {
        int branch = nodeid.selectbranch (targetnodeid.getnodeid ());

        if (node->isemptybranch (branch))
            return false;

        node = descendthrow (node, branch);
        nodeid = nodeid.getchildnodeid (branch);
    }

    return (node->isinner()) && (node->getnodehash() == targetnodehash);
}

/** does this map have this leaf node?
*/
bool shamap::hasleafnode (uint256 const& tag, uint256 const& targetnodehash)
{
    shamaptreenode* node = root.get ();
    shamapnodeid nodeid;

    if (!node->isinner()) // only one leaf node in the tree
        return node->getnodehash() == targetnodehash;

    do
    {
        int branch = nodeid.selectbranch (tag);

        if (node->isemptybranch (branch))
            return false;   // dead end, node must not be here

        if (node->getchildhash (branch) == targetnodehash) // matching leaf, no need to retrieve it
            return true;

        node = descendthrow (node, branch);
        nodeid = nodeid.getchildnodeid (branch);
    }
    while (node->isinner());

    return false; // if this was a matching leaf, we would have caught it already
}

/**
@param have a pointer to the map that the recipient already has (if any).
@param includeleaves true if leaf nodes should be included.
@param max the maximum number of nodes to return.
@param func the functor to call for each node added to the fetchpack.

note: a caller should set includeleaves to false for transaction trees.
there's no point in including the leaves of transaction trees.
*/
void shamap::getfetchpack (shamap* have, bool includeleaves, int max,
                           std::function<void (uint256 const&, const blob&)> func)
{
    if (root->getnodehash ().iszero ())
        return;

    if (have && (root->getnodehash () == have->root->getnodehash ()))
        return;

    if (root->isleaf ())
    {
        if (includeleaves &&
                (!have || !have->hasleafnode (root->gettag (), root->getnodehash ())))
        {
            serializer s;
            root->addraw (s, snfprefix);
            func (std::cref(root->getnodehash ()), std::cref(s.peekdata ()));
            --max;
        }

        return;
    }
    // contains unexplored non-matching inner node entries
    using stackentry = std::pair <shamaptreenode*, shamapnodeid>;
    std::stack <stackentry, std::vector<stackentry>> stack;

    stack.push ({root.get(), shamapnodeid{}});

    while (!stack.empty() && (max > 0))
    {
        shamaptreenode* node;
        shamapnodeid nodeid;
        std::tie (node, nodeid) = stack.top ();
        stack.pop ();

        // 1) add this node to the pack
        serializer s;
        node->addraw (s, snfprefix);
        func (std::cref(node->getnodehash ()), std::cref(s.peekdata ()));
        --max;

        // 2) push non-matching child inner nodes
        for (int i = 0; i < 16; ++i)
        {
            if (!node->isemptybranch (i))
            {
                uint256 const& childhash = node->getchildhash (i);
                shamapnodeid childid = nodeid.getchildnodeid (i);
                shamaptreenode* next = descendthrow (node, i);

                if (next->isinner ())
                {
                    if (!have || !have->hasinnernode (childid, childhash))
                        stack.push ({next, childid});
                }
                else if (includeleaves && (!have || !have->hasleafnode (next->gettag(), childhash)))
                {
                    serializer s;
                    next->addraw (s, snfprefix);
                    func (std::cref(childhash), std::cref(s.peekdata ()));
                    --max;
                }
            }
        }
    }
}

std::list <blob> shamap::gettrustedpath (uint256 const& index)
{
    auto stack = getstack (index, false);
    if (stack.empty () || !stack.top ().first->isleaf ())
        throw std::runtime_error ("requested leaf not present");

    std::list< blob > path;
    serializer s;

    while (!stack.empty ())
    {
        stack.top ().first->addraw (s, snfwire);
        path.push_back (s.getdata ());
        s.erase ();
        stack.pop ();
    }

    return path;
}

} // ripple
