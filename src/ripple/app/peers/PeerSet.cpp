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
#include <ripple/app/main/application.h>
#include <ripple/app/peers/peerset.h>
#include <ripple/core/jobqueue.h>
#include <ripple/overlay/overlay.h>
#include <beast/asio/placeholders.h>

namespace ripple {

class inboundledger;

// vfalco note the txndata constructor parameter is a code smell.
//             it is true if we are the base of a transactionacquire,
//             or false if we are base of inboundledger. all it does
//             is change the behavior of the timer depending on the
//             derived class. why not just make the timer callback
//             function pure virtual?
//
peerset::peerset (uint256 const& hash, int interval, bool txndata,
    clock_type& clock, beast::journal journal)
    : m_journal (journal)
    , m_clock (clock)
    , mhash (hash)
    , mtimerinterval (interval)
    , mtimeouts (0)
    , mcomplete (false)
    , mfailed (false)
    , maggressive (false)
    , mtxndata (txndata)
    , mprogress (false)
    , mtimer (getapp().getioservice ())
{
    mlastaction = m_clock.now();
    assert ((mtimerinterval > 10) && (mtimerinterval < 30000));
}

peerset::~peerset ()
{
}

bool peerset::peerhas (peer::ptr const& ptr)
{
    scopedlocktype sl (mlock);

    if (!mpeers.insert (std::make_pair (ptr->id (), 0)).second)
        return false;

    newpeer (ptr);
    return true;
}

void peerset::badpeer (peer::ptr const& ptr)
{
    scopedlocktype sl (mlock);
    mpeers.erase (ptr->id ());
}

void peerset::settimer ()
{
    mtimer.expires_from_now (boost::posix_time::milliseconds (mtimerinterval));
    mtimer.async_wait (std::bind (&peerset::timerentry, pmdowncast (), beast::asio::placeholders::error));
}

void peerset::invokeontimer ()
{
    scopedlocktype sl (mlock);

    if (isdone ())
        return;

    if (!isprogress())
    {
        ++mtimeouts;
        writelog (lswarning, inboundledger) << "timeout(" << mtimeouts << ") pc=" << mpeers.size () << " acquiring " << mhash;
        ontimer (false, sl);
    }
    else
    {
        clearprogress ();
        ontimer (true, sl);
    }

    if (!isdone ())
        settimer ();
}

void peerset::timerentry (std::weak_ptr<peerset> wptr, const boost::system::error_code& result)
{
    if (result == boost::asio::error::operation_aborted)
        return;

    std::shared_ptr<peerset> ptr = wptr.lock ();

    if (ptr)
    {
        // vfalco note so this function is really two different functions depending on
        //             the value of mtxndata, which is directly tied to whether we are
        //             a base class of incomingledger or transactionacquire
        //
        if (ptr->mtxndata)
        {
            getapp().getjobqueue ().addjob (jttxn_data, "timerentrytxn",
                std::bind (&peerset::timerjobentry, std::placeholders::_1,
                           ptr));
        }
        else
        {
            int jc = getapp().getjobqueue ().getjobcounttotal (jtledger_data);

            if (jc > 4)
            {
                writelog (lsdebug, inboundledger) << "deferring peerset timer due to load";
                ptr->settimer ();
            }
            else
                getapp().getjobqueue ().addjob (jtledger_data, "timerentrylgr",
                    std::bind (&peerset::timerjobentry, std::placeholders::_1,
                               ptr));
        }
    }
}

void peerset::timerjobentry (job&, std::shared_ptr<peerset> ptr)
{
    ptr->invokeontimer ();
}

bool peerset::isactive ()
{
    scopedlocktype sl (mlock);
    return !isdone ();
}

void peerset::sendrequest (const protocol::tmgetledger& tmgl, peer::ptr const& peer)
{
    if (!peer)
        sendrequest (tmgl);
    else
        peer->send (std::make_shared<message> (tmgl, protocol::mtget_ledger));
}

void peerset::sendrequest (const protocol::tmgetledger& tmgl)
{
    scopedlocktype sl (mlock);

    if (mpeers.empty ())
        return;

    message::pointer packet (
        std::make_shared<message> (tmgl, protocol::mtget_ledger));

    for (auto const& p : mpeers)
    {
        peer::ptr peer (getapp().overlay ().findpeerbyshortid (p.first));

        if (peer)
            peer->send (packet);
    }
}

std::size_t peerset::takepeersetfrom (const peerset& s)
{
    std::size_t ret = 0;
    mpeers.clear ();

    for (auto const& p : s.mpeers)
    {
        mpeers.insert (std::make_pair (p.first, 0));
        ++ret;
    }

    return ret;
}

std::size_t peerset::getpeercount () const
{
    std::size_t ret (0);

    for (auto const& p : mpeers)
    {
        if (getapp ().overlay ().findpeerbyshortid (p.first))
            ++ret;
    }

    return ret;
}

} // ripple
