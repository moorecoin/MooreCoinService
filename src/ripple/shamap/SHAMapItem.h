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

#ifndef ripple_shamapitem_h
#define ripple_shamapitem_h

#include <ripple/basics/countedobject.h>
#include <ripple/protocol/serializer.h>
#include <ripple/basics/base_uint.h>
#include <beast/utility/journal.h>
#include <memory>

namespace ripple {

// an item stored in a shamap
class shamapitem
    : public countedobject <shamapitem>
{
public:
    static char const* getcountedobjectname () { return "shamapitem"; }

    typedef std::shared_ptr<shamapitem>           pointer;
    typedef const std::shared_ptr<shamapitem>&    ref;

public:
    explicit shamapitem (uint256 const& tag) : mtag (tag)
    {
        ;
    }
    explicit shamapitem (blob const & data); // tag by hash
    shamapitem (uint256 const& tag, blob const & data);
    shamapitem (uint256 const& tag, const serializer & s);

    uint256 const& gettag () const
    {
        return mtag;
    }
    blob const& peekdata () const
    {
        return mdata.peekdata ();
    }
    serializer& peekserializer ()
    {
        return mdata;
    }
    void addraw (blob & s) const
    {
        s.insert (s.end (), mdata.begin (), mdata.end ());
    }

    void updatedata (blob const & data)
    {
        mdata = data;
    }

    bool operator== (const shamapitem & i) const
    {
        return mtag == i.mtag;
    }
    bool operator!= (const shamapitem & i) const
    {
        return mtag != i.mtag;
    }
    bool operator== (uint256 const& i) const
    {
        return mtag == i;
    }
    bool operator!= (uint256 const& i) const
    {
        return mtag != i;
    }

#if 0
    // this code is comment out because it is unused.  it could work.
    bool operator< (const shamapitem & i) const
    {
        return mtag < i.mtag;
    }
    bool operator> (const shamapitem & i) const
    {
        return mtag > i.mtag;
    }
    bool operator<= (const shamapitem & i) const
    {
        return mtag <= i.mtag;
    }
    bool operator>= (const shamapitem & i) const
    {
        return mtag >= i.mtag;
    }

    bool operator< (uint256 const& i) const
    {
        return mtag < i;
    }
    bool operator> (uint256 const& i) const
    {
        return mtag > i;
    }
    bool operator<= (uint256 const& i) const
    {
        return mtag <= i;
    }
    bool operator>= (uint256 const& i) const
    {
        return mtag >= i;
    }
#endif

    // vfalco why is this virtual?
    virtual void dump (beast::journal journal);

private:
    uint256 mtag;
    serializer mdata;
};

} // ripple

#endif
