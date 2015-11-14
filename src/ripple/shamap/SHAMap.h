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

#ifndef ripple_shamap_shamap_h_included
#define ripple_shamap_shamap_h_included

#include <ripple/shamap/fullbelowcache.h>
#include <ripple/shamap/shamapaddnode.h>
#include <ripple/shamap/shamapitem.h>
#include <ripple/shamap/shamapmissingnode.h>
#include <ripple/shamap/shamapnodeid.h>
#include <ripple/shamap/shamapsyncfilter.h>
#include <ripple/shamap/shamaptreenode.h>
#include <ripple/basics/unorderedcontainers.h>
#include <ripple/nodestore/database.h>
#include <ripple/nodestore/nodeobject.h>
#include <beast/utility/journal.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_lock_guard.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <stack>

namespace std {

template <>
struct hash <ripple::shamapnodeid>
{
    std::size_t operator() (ripple::shamapnodeid const& value) const
    {
        return value.getmhash ();
    }
};

}

//------------------------------------------------------------------------------

namespace boost {

template <>
struct hash <ripple::shamapnodeid> : std::hash <ripple::shamapnodeid>
{
};

}

//------------------------------------------------------------------------------

namespace ripple {

enum shamapstate
{
    smsmodifying = 0,       // objects can be added and removed (like an open ledger)
    smsimmutable = 1,       // map cannot be changed (like a closed ledger)
    smssynching = 2,        // map's hash is locked in, valid nodes can be added (like a peer's closing ledger)
    smsfloating = 3,        // map is free to change hash (like a synching open ledger)
    smsinvalid = 4,         // map is known not to be valid (usually synching a corrupt ledger)
};

/** function object which handles missing nodes. */
using missingnodehandler = std::function <void (std::uint32_t refnum)>;

/** a shamap is both a radix tree with a fan-out of 16 and a merkle tree.

    a radix tree is a tree with two properties:

      1. the key for a node is represented by the node's position in the tree
         (the "prefix property").
      2. a node with only one child is merged with that child
         (the "merge property")

    these properties in a significantly smaller memory footprint for a radix tree.

    and a fan-out of 16 means that each node in the tree has at most 16 children.
    see https://en.wikipedia.org/wiki/radix_tree

    a merkle tree is a tree where each non-leaf node is labelled with the hash
    of the combined labels of its children nodes.

    a key property of a merkle tree is that testing for node inclusion is
    o(log(n)) where n is the number of nodes in the tree.

    see https://en.wikipedia.org/wiki/merkle_tree
 */
class shamap
{
public:
    enum
    {
        state_map_buckets = 1024
    };

    static char const* getcountedobjectname () { return "shamap"; }

    typedef std::shared_ptr<shamap> pointer;
    typedef const std::shared_ptr<shamap>& ref;

    typedef std::pair<shamapitem::pointer, shamapitem::pointer> deltaitem;
    typedef std::pair<shamapitem::ref, shamapitem::ref> deltaref;
    typedef std::map<uint256, deltaitem> delta;
    typedef hash_map<shamapnodeid, shamaptreenode::pointer, shamapnode_hash> nodemap;

    typedef std::stack<std::pair<shamaptreenode::pointer, shamapnodeid>> sharedptrnodestack;

public:
    // build new map
    shamap (
        shamaptype t,
        fullbelowcache& fullbelowcache,
        treenodecache& treenodecache,
        nodestore::database& db,
        missingnodehandler missing_node_handler,
        beast::journal journal,
        std::uint32_t seq = 1
        );

    shamap (
        shamaptype t,
        uint256 const& hash,
        fullbelowcache& fullbelowcache,
        treenodecache& treenodecache,
        nodestore::database& db,
        missingnodehandler missing_node_handler,
        beast::journal journal);

    ~shamap ();

    // returns a new map that's a snapshot of this one.
    // handles copy on write for mutable snapshots.
    shamap::pointer snapshot (bool ismutable);

    void setledgerseq (std::uint32_t lseq)
    {
        mledgerseq = lseq;
    }

    bool fetchroot (uint256 const& hash, shamapsyncfilter * filter);

    // normal hash access functions
    bool hasitem (uint256 const& id);
    bool delitem (uint256 const& id);
    bool additem (shamapitem const& i, bool istransaction, bool hasmeta);

    uint256 gethash () const
    {
        return root->getnodehash ();
    }

    // save a copy if you have a temporary anyway
    bool updategiveitem (shamapitem::ref, bool istransaction, bool hasmeta);
    bool addgiveitem (shamapitem::ref, bool istransaction, bool hasmeta);

    // save a copy if you only need a temporary
    shamapitem::pointer peekitem (uint256 const& id);
    shamapitem::pointer peekitem (uint256 const& id, uint256 & hash);
    shamapitem::pointer peekitem (uint256 const& id, shamaptreenode::tntype & type);

    // traverse functions
    shamapitem::pointer peekfirstitem ();
    shamapitem::pointer peekfirstitem (shamaptreenode::tntype & type);
    shamapitem::pointer peeklastitem ();
    shamapitem::pointer peeknextitem (uint256 const& );
    shamapitem::pointer peeknextitem (uint256 const& , shamaptreenode::tntype & type);
    shamapitem::pointer peekprevitem (uint256 const& );

    void visitnodes (std::function<bool (shamaptreenode&)> const&);
    void visitleaves(std::function<void (shamapitem::ref)> const&);

    // comparison/sync functions
    void getmissingnodes (std::vector<shamapnodeid>& nodeids, std::vector<uint256>& hashes, int max,
                          shamapsyncfilter * filter);
    bool getnodefat (shamapnodeid node, std::vector<shamapnodeid>& nodeids,
                     std::list<blob >& rawnode, bool fatroot, bool fatleaves);
    bool getrootnode (serializer & s, shanodeformat format);
    std::vector<uint256> getneededhashes (int max, shamapsyncfilter * filter);
    shamapaddnode addrootnode (uint256 const& hash, blob const& rootnode, shanodeformat format,
                               shamapsyncfilter * filter);
    shamapaddnode addrootnode (blob const& rootnode, shanodeformat format,
                               shamapsyncfilter * filter);
    shamapaddnode addknownnode (shamapnodeid const& nodeid, blob const& rawnode,
                                shamapsyncfilter * filter);

    // status functions
    void setimmutable ()
    {
        assert (mstate != smsinvalid);
        mstate = smsimmutable;
    }
    bool issynching () const
    {
        return (mstate == smsfloating) || (mstate == smssynching);
    }
    void setsynching ()
    {
        mstate = smssynching;
    }
    void clearsynching ()
    {
        mstate = smsmodifying;
    }
    bool isvalid ()
    {
        return mstate != smsinvalid;
    }

    // caution: othermap must be accessed only by this function
    // return value: true=successfully completed, false=too different
    bool compare (shamap::ref othermap, delta & differences, int maxcount);

    int flushdirty (nodeobjecttype t, std::uint32_t seq);
    int unshare ();

    void walkmap (std::vector<shamapmissingnode>& missingnodes, int maxmissing);

    bool deepcompare (shamap & other);

    typedef std::pair <uint256, blob> fetchpackentry_t;

    void getfetchpack (shamap * have, bool includeleaves, int max, std::function<void (uint256 const&, const blob&)>);

    void setunbacked ()
    {
        mbacked = false;
    }

    void dump (bool withhashes = false);

private:
    // trusted path operations - prove a particular node is in a particular ledger
    std::list<blob > gettrustedpath (uint256 const& index);

    bool getpath (uint256 const& index, std::vector< blob >& nodes, shanodeformat format);

     // tree node cache operations
    shamaptreenode::pointer getcache (uint256 const& hash);
    void canonicalize (uint256 const& hash, shamaptreenode::pointer&);

    // database operations
    shamaptreenode::pointer fetchnodefromdb (uint256 const& hash);

    shamaptreenode::pointer fetchnodent (uint256 const& hash);

    shamaptreenode::pointer fetchnodent (
        shamapnodeid const& id,
        uint256 const& hash,
        shamapsyncfilter *filter);

    shamaptreenode::pointer fetchnode (uint256 const& hash);

    shamaptreenode::pointer checkfilter (uint256 const& hash, shamapnodeid const& id,
        shamapsyncfilter* filter);

    /** update hashes up to the root */
    void dirtyup (sharedptrnodestack& stack,
                  uint256 const& target, shamaptreenode::pointer terminal);

    /** get the path from the root to the specified node */
    sharedptrnodestack
        getstack (uint256 const& id, bool include_nonmatching_leaf);

    /** walk to the specified index, returning the node */
    shamaptreenode* walktopointer (uint256 const& id);

    /** unshare the node, allowing it to be modified */
    void unsharenode (shamaptreenode::pointer&, shamapnodeid const& nodeid);

    /** prepare a node to be modified before flushing */
    void preflushnode (shamaptreenode::pointer& node);

    /** write and canonicalize modified node */
    void writenode (nodeobjecttype t, std::uint32_t seq,
        shamaptreenode::pointer& node);

    shamaptreenode* firstbelow (shamaptreenode*);
    shamaptreenode* lastbelow (shamaptreenode*);

    // simple descent
    // get a child of the specified node
    shamaptreenode* descend (shamaptreenode*, int branch);
    shamaptreenode* descendthrow (shamaptreenode*, int branch);
    shamaptreenode::pointer descend (shamaptreenode::ref, int branch);
    shamaptreenode::pointer descendthrow (shamaptreenode::ref, int branch);

    // descend with filter
    shamaptreenode* descendasync (shamaptreenode* parent, int branch,
        shamapnodeid const& childid, shamapsyncfilter* filter, bool& pending);

    std::pair <shamaptreenode*, shamapnodeid>
        descend (shamaptreenode* parent, shamapnodeid const& parentid,
        int branch, shamapsyncfilter* filter);

    // non-storing
    // does not hook the returned node to its parent
    shamaptreenode::pointer descendnostore (shamaptreenode::ref, int branch);

    /** if there is only one leaf below this node, get its contents */
    shamapitem::pointer onlybelow (shamaptreenode*);

    bool hasinnernode (shamapnodeid const& nodeid, uint256 const& hash);
    bool hasleafnode (uint256 const& tag, uint256 const& hash);

    bool walkbranch (shamaptreenode* node,
                     shamapitem::ref othermapitem, bool isfirstmap,
                     delta & differences, int & maxcount);

    void visitleavesinternal (std::function<void (shamapitem::ref item)>& function);

    int walksubtree (bool dowrite, nodeobjecttype t, std::uint32_t seq);

private:
    beast::journal journal_;
    nodestore::database& db_;
    fullbelowcache& m_fullbelowcache;
    std::uint32_t mseq;
    std::uint32_t mledgerseq; // sequence number of ledger this is part of
    treenodecache& mtreenodecache;
    shamaptreenode::pointer root;
    shamapstate mstate;
    shamaptype mtype;
    bool mbacked = true; // map is backed by the database
    missingnodehandler m_missing_node_handler;
};

}

#endif
