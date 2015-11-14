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

#ifndef ripple_peerset_h
#define ripple_peerset_h

#include <ripple/basics/log.h>
#include <ripple/core/job.h>
#include <ripple/overlay/peer.h>
#include <beast/chrono/abstract_clock.h>
#include <beast/utility/leakchecked.h>
#include <beast/utility/journal.h>
#include <boost/asio/deadline_timer.hpp>

namespace ripple {

/** a set of peers used to acquire data.

    a peer set is used to acquire a ledger or a transaction set.
*/
class peerset : beast::leakchecked <peerset>
{
public:
    typedef beast::abstract_clock <std::chrono::steady_clock> clock_type;

    uint256 const& gethash () const
    {
        return mhash;
    }

    bool iscomplete () const
    {
        return mcomplete;
    }

    bool isfailed () const
    {
        return mfailed;
    }

    int gettimeouts () const
    {
        return mtimeouts;
    }

    bool isactive ();

    void progress ()
    {
        mprogress = true;
        maggressive = false;
    }

    void clearprogress ()
    {
        mprogress = false;
    }

    bool isprogress ()
    {
        return mprogress;
    }

    void touch ()
    {
        mlastaction = m_clock.now();
    }

    clock_type::time_point getlastaction () const
    {
        return mlastaction;
    }

    // vfalco todo rename this to addpeertoset
    //
    bool peerhas (peer::ptr const&);

    // vfalco workaround for msvc std::function which doesn't swallow return types.
    void peerhasvoid (peer::ptr const& peer)
    {
        peerhas (peer);
    }

    void badpeer (peer::ptr const&);

    void settimer ();

    std::size_t takepeersetfrom (const peerset& s);
    std::size_t getpeercount () const;
    virtual bool isdone () const
    {
        return mcomplete || mfailed;
    }

private:
    static void timerentry (std::weak_ptr<peerset>, const boost::system::error_code& result);
    static void timerjobentry (job&, std::shared_ptr<peerset>);

protected:
    // vfalco todo try to make some of these private
    typedef ripplerecursivemutex locktype;
    typedef std::unique_lock <locktype> scopedlocktype;

    peerset (uint256 const& hash, int interval, bool txndata,
        clock_type& clock, beast::journal journal);
    virtual ~peerset () = 0;

    virtual void newpeer (peer::ptr const&) = 0;
    virtual void ontimer (bool progress, scopedlocktype&) = 0;
    virtual std::weak_ptr<peerset> pmdowncast () = 0;

    void setcomplete ()
    {
        mcomplete = true;
    }
    void setfailed ()
    {
        mfailed = true;
    }
    void invokeontimer ();

    void sendrequest (const protocol::tmgetledger& message);
    void sendrequest (const protocol::tmgetledger& message, peer::ptr const& peer);

protected:
    beast::journal m_journal;
    clock_type& m_clock;

    locktype mlock;

    uint256 mhash;
    int mtimerinterval;
    int mtimeouts;
    bool mcomplete;
    bool mfailed;
    bool maggressive;
    bool mtxndata;
    clock_type::time_point mlastaction;
    bool mprogress;


    // vfalco todo move the responsibility for the timer to a higher level
    boost::asio::deadline_timer             mtimer;

    // vfalco todo verify that these are used in the way that the names suggest.
    typedef peer::id_t peeridentifier;
    typedef int receivedchunkcount;
    typedef hash_map <peeridentifier, receivedchunkcount> peersetmap;

    peersetmap mpeers;
};

} // ripple

#endif
