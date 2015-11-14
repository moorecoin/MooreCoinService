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

#ifndef ripple_shamap_shamaptreenode_h
#define ripple_shamap_shamaptreenode_h

#include <ripple/shamap/shamapitem.h>
#include <ripple/shamap/shamapnodeid.h>
#include <ripple/shamap/treenodecache.h>
#include <ripple/basics/countedobject.h>
#include <ripple/basics/taggedcache.h>
#include <beast/utility/journal.h>

namespace ripple {

class shamap;

enum shanodeformat
{
    snfprefix   = 1, // form that hashes to its official hash
    snfwire     = 2, // compressed form used on the wire
    snfhash     = 3, // just the hash
};

class shamaptreenode
    : public countedobject <shamaptreenode>
{
public:
    static char const* getcountedobjectname () { return "shamaptreenode"; }

    typedef std::shared_ptr<shamaptreenode>           pointer;
    typedef const std::shared_ptr<shamaptreenode>&    ref;

    enum tntype
    {
        tnerror             = 0,
        tninner             = 1,
        tntransaction_nm    = 2, // transaction, no metadata
        tntransaction_md    = 3, // transaction, with metadata
        tnaccount_state     = 4
    };

public:
    shamaptreenode (const shamaptreenode&) = delete;
    shamaptreenode& operator= (const shamaptreenode&) = delete;

    shamaptreenode (std::uint32_t seq); // empty node
    shamaptreenode (const shamaptreenode & node, std::uint32_t seq); // copy node from older tree
    shamaptreenode (shamapitem::ref item, tntype type, std::uint32_t seq);

    // raw node functions
    shamaptreenode (blob const & data, std::uint32_t seq,
                    shanodeformat format, uint256 const& hash, bool hashvalid);
    void addraw (serializer&, shanodeformat format);

    // node functions
    std::uint32_t getseq () const
    {
        return mseq;
    }
    void setseq (std::uint32_t s)
    {
        mseq = s;
    }
    uint256 const& getnodehash () const
    {
        return mhash;
    }
    tntype gettype () const
    {
        return mtype;
    }

    // type functions
    bool isleaf () const
    {
        return (mtype == tntransaction_nm) || (mtype == tntransaction_md) ||
               (mtype == tnaccount_state);
    }
    bool isinner () const
    {
        return mtype == tninner;
    }
    bool isinbounds (shamapnodeid const &id) const
    {
        // nodes at depth 64 must be leaves
        return (!isinner() || (id.getdepth() < 64));
    }
    bool isvalid () const
    {
        return mtype != tnerror;
    }
    bool istransaction () const
    {
        return (mtype == tntransaction_nm) || (mtype == tntransaction_md);
    }
    bool hasmetadata () const
    {
        return mtype == tntransaction_md;
    }
    bool isaccountstate () const
    {
        return mtype == tnaccount_state;
    }

    // inner node functions
    bool isinnernode () const
    {
        return !mitem;
    }

    // we are modifying the child hash
    bool setchild (int m, uint256 const& hash, std::shared_ptr<shamaptreenode> const& child);

    // we are sharing/unsharing the child
    void sharechild (int m, std::shared_ptr<shamaptreenode> const& child);

    bool isemptybranch (int m) const
    {
        return (misbranch & (1 << m)) == 0;
    }
    bool isempty () const;
    int getbranchcount () const;
    void makeinner ();
    uint256 const& getchildhash (int m) const
    {
        assert ((m >= 0) && (m < 16) && (mtype == tninner));
        return mhashes[m];
    }

    // item node function
    bool hasitem () const
    {
        return bool(mitem);
    }
    shamapitem::ref peekitem ()
    {
        // caution: do not modify the item todo(tom): a comment in the code does
        // nothing - this should return a const reference.
        return mitem;
    }
    bool setitem (shamapitem::ref i, tntype type);
    uint256 const& gettag () const
    {
        return mitem->gettag ();
    }
    blob const& peekdata ()
    {
        return mitem->peekdata ();
    }

    // sync functions
    bool isfullbelow (std::uint32_t generation) const
    {
        return mfullbelowgen == generation;
    }
    void setfullbelowgen (std::uint32_t gen)
    {
        mfullbelowgen = gen;
    }

    // vfalco why is this virtual?
    virtual void dump (shamapnodeid const&, beast::journal journal);
    virtual std::string getstring (shamapnodeid const&) const;

    shamaptreenode* getchildpointer (int branch);
    shamaptreenode::pointer getchild (int branch);
    void canonicalizechild (int branch, shamaptreenode::pointer& node);

private:

    // vfalco todo remove the use of friend
    friend class shamap;

    uint256                 mhash;
    uint256                 mhashes[16];
    shamaptreenode::pointer mchildren[16];
    shamapitem::pointer     mitem;
    std::uint32_t           mseq;
    tntype                  mtype;
    int                     misbranch;
    std::uint32_t           mfullbelowgen;

    bool updatehash ();

    static std::mutex       childlock;
};

} // ripple

#endif
