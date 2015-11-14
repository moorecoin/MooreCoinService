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

#ifndef ripple_ripplestate_h
#define ripple_ripplestate_h

#include <ripple/app/book/types.h>
#include <ripple/protocol/stamount.h>
#include <cstdint>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

//
// a ripple line's state.
// - isolate ledger entry format.
//

class ripplestate
{
public:
    typedef std::shared_ptr <ripplestate> pointer;

public:
    ripplestate () = delete;

    virtual ~ripplestate () { }

    static ripplestate::pointer makeitem (
        account const& accountid, stledgerentry::ref ledgerentry);

    ledgerentrytype gettype ()
    {
        return ltripple_state;
    }

    account const& getaccountid () const
    {
        return  mviewlowest ? mlowid : mhighid;
    }

    account const& getaccountidpeer () const
    {
        return !mviewlowest ? mlowid : mhighid;
    }

    // true, provided auth to peer.
    bool getauth () const
    {
        return mflags & (mviewlowest ? lsflowauth : lsfhighauth);
    }

    bool getauthpeer () const
    {
        return mflags & (!mviewlowest ? lsflowauth : lsfhighauth);
    }

    bool getnoripple () const
    {
        return mflags & (mviewlowest ? lsflownoripple : lsfhighnoripple);
    }

    bool getnoripplepeer () const
    {
        return mflags & (!mviewlowest ? lsflownoripple : lsfhighnoripple);
    }

    /** have we set the freeze flag on our peer */
    bool getfreeze () const
    {
        return mflags & (mviewlowest ? lsflowfreeze : lsfhighfreeze);
    }

    /** has the peer set the freeze flag on us */
    bool getfreezepeer () const
    {
        return mflags & (!mviewlowest ? lsflowfreeze : lsfhighfreeze);
    }

    stamount const& getbalance () const
    {
        return mbalance;
    }

    stamount const& getlimit () const
    {
        return  mviewlowest ? mlowlimit : mhighlimit;
    }

    stamount const& getlimitpeer () const
    {
        return !mviewlowest ? mlowlimit : mhighlimit;
    }

    std::uint32_t getqualityin () const
    {
        return ((std::uint32_t) (mviewlowest ? mlowqualityin : mhighqualityin));
    }

    std::uint32_t getqualityout () const
    {
        return ((std::uint32_t) (mviewlowest ? mlowqualityout : mhighqualityout));
    }

    stledgerentry::pointer getsle ()
    {
        return mledgerentry;
    }

    const stledgerentry& peeksle () const
    {
        return *mledgerentry;
    }

    stledgerentry& peeksle ()
    {
        return *mledgerentry;
    }

    json::value getjson (int);

    blob getraw () const;

private:
    ripplestate (
        stledgerentry::ref ledgerentry,
        account const& viewaccount);

private:
    stledgerentry::pointer  mledgerentry;

    bool                            mviewlowest;

    std::uint32_t                   mflags;

    stamount const&                 mlowlimit;
    stamount const&                 mhighlimit;

    account const&                  mlowid;
    account const&                  mhighid;

    std::uint64_t                   mlowqualityin;
    std::uint64_t                   mlowqualityout;
    std::uint64_t                   mhighqualityin;
    std::uint64_t                   mhighqualityout;

    stamount                        mbalance;
};

std::vector <ripplestate::pointer>
getripplestateitems (
    account const& accountid,
    ledger::ref ledger);

} // ripple

#endif
