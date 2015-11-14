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

#ifndef ripple_transactionacquire_h
#define ripple_transactionacquire_h

#include <ripple/app/peers/peerset.h>
#include <ripple/shamap/shamap.h>

namespace ripple {

// vfalco todo rename to peertxrequest
// a transaction set we are trying to acquire
class transactionacquire
    : public peerset
    , public std::enable_shared_from_this <transactionacquire>
    , public countedobject <transactionacquire>
{
public:
    static char const* getcountedobjectname () { return "transactionacquire"; }

    typedef std::shared_ptr<transactionacquire> pointer;

public:
    transactionacquire (uint256 const& hash, clock_type& clock);
    ~transactionacquire ();

    shamap::ref getmap ()
    {
        return mmap;
    }

    shamapaddnode takenodes (const std::list<shamapnodeid>& ids,
                             const std::list< blob >& data, peer::ptr const&);

private:
    shamap::pointer     mmap;
    bool                mhaveroot;

    void ontimer (bool progress, scopedlocktype& peersetlock);
    void newpeer (peer::ptr const& peer)
    {
        trigger (peer);
    }

    void done ();
    void trigger (peer::ptr const&);
    std::weak_ptr<peerset> pmdowncast ();
};

} // ripple

#endif
