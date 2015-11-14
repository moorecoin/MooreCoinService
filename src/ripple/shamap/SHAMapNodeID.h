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

#ifndef ripple_shamapnodeid_h
#define ripple_shamapnodeid_h

#include <ripple/protocol/serializer.h>
#include <ripple/basics/base_uint.h>
#include <beast/utility/journal.h>
#include <ostream>
#include <string>
#include <tuple>

namespace ripple {

// identifies a node in a sha256 hash map
class shamapnodeid
{
private:
    uint256 mnodeid;
    int mdepth;
    mutable size_t  mhash;

public:
    shamapnodeid () : mdepth (0), mhash (0)
    {
    }

    shamapnodeid (int depth, uint256 const& hash);
    shamapnodeid (void const* ptr, int len);

protected:
    shamapnodeid (int depth, uint256 const& id, bool)
        : mnodeid (id), mdepth (depth), mhash (0)
    {
    }

public:
    int getdepth () const
    {
        return mdepth;
    }

    uint256 const& getnodeid ()  const
    {
        return mnodeid;
    }

    bool isvalid () const
    {
        return (mdepth >= 0) && (mdepth < 64);
    }

    bool isroot () const
    {
        return mdepth == 0;
    }

    size_t getmhash () const
    {
        if (mhash == 0)
            mhash = calculate_hash (mnodeid, mdepth);
        return mhash;
    }

    shamapnodeid getparentnodeid () const
    {
        assert (mdepth);
        return shamapnodeid (mdepth - 1, mnodeid);
    }

    shamapnodeid getchildnodeid (int m) const;
    int selectbranch (uint256 const& hash) const;

    bool operator< (const shamapnodeid& n) const
    {
        return std::tie(mdepth, mnodeid) < std::tie(n.mdepth, n.mnodeid);
    }
    bool operator> (const shamapnodeid& n) const {return n < *this;}
    bool operator<= (const shamapnodeid& n) const {return !(*this < n);}
    bool operator>= (const shamapnodeid& n) const {return !(n < *this);}

    bool operator== (const shamapnodeid& n) const
    {
        return (mdepth == n.mdepth) && (mnodeid == n.mnodeid);
    }
    bool operator!= (const shamapnodeid& n) const {return !(*this == n);}

    bool operator== (uint256 const& n) const
    {
        return n == mnodeid;
    }
    bool operator!= (uint256 const& n) const {return !(*this == n);}

    virtual std::string getstring () const;
    void dump (beast::journal journal) const;

    static uint256 getnodeid (int depth, uint256 const& hash);

    // convert to/from wire format (256-bit nodeid, 1-byte depth)
    void addidraw (serializer& s) const;
    std::string getrawstring () const;
    static int getrawidlength (void)
    {
        return 33;
    }

private:
    static
    uint256 const&
    masks (int depth);

    static
    std::size_t
    calculate_hash (uint256 const& node, int depth);
};

//------------------------------------------------------------------------------

inline std::ostream& operator<< (std::ostream& out, shamapnodeid const& node)
{
    return out << node.getstring ();
}

//------------------------------------------------------------------------------

class shamapnode_hash
{
public:
    typedef ripple::shamapnodeid argument_type;
    typedef std::size_t result_type;

    result_type
    operator() (argument_type const& key) const noexcept
    {
        return key.getmhash ();
    }
};

} // ripple

#endif
