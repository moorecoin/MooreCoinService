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

#ifndef ripple_shamapmissingnode_h
#define ripple_shamapmissingnode_h

#include <ripple/basics/base_uint.h>
    
namespace ripple {

enum shamaptype
{
    smttransaction  = 1,    // a tree of transactions
    smtstate        = 2,    // a tree of state nodes
    smtfree         = 3,    // a tree not part of a ledger
};

class shamapmissingnode : public std::runtime_error
{
public:
    shamapmissingnode (shamaptype t,
                       uint256 const& nodehash)
        : std::runtime_error ("shamapmissingnode")
        , mtype (t)
        , mnodehash (nodehash)
    {
    }

    virtual ~shamapmissingnode () throw ()
    {
    }

    shamaptype getmaptype () const
    {
        return mtype;
    }

    uint256 const& getnodehash () const
    {
        return mnodehash;
    }

private:
    shamaptype mtype;
    uint256 mnodehash;
};

extern std::ostream& operator<< (std::ostream&, shamapmissingnode const&);

} // ripple

#endif
