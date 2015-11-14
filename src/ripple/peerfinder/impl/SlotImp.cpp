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
#include <ripple/peerfinder/impl/slotimp.h>
#include <ripple/peerfinder/manager.h>
#include <ripple/peerfinder/impl/tuning.h>

namespace ripple {
namespace peerfinder {

slotimp::slotimp (beast::ip::endpoint const& local_endpoint,
    beast::ip::endpoint const& remote_endpoint, bool fixed,
        clock_type& clock)
    : recent (clock)
    , m_inbound (true)
    , m_fixed (fixed)
    , m_cluster (false)
    , m_state (accept)
    , m_remote_endpoint (remote_endpoint)
    , m_local_endpoint (local_endpoint)
    , checked (false)
    , canaccept (false)
    , connectivitycheckinprogress (false)
{
}

slotimp::slotimp (beast::ip::endpoint const& remote_endpoint,
    bool fixed, clock_type& clock)
    : recent (clock)
    , m_inbound (false)
    , m_fixed (fixed)
    , m_cluster (false)
    , m_state (connect)
    , m_remote_endpoint (remote_endpoint)
    , checked (true)
    , canaccept (true)
    , connectivitycheckinprogress (false)
{
}

void
slotimp::state (state state_)
{
    // must go through activate() to set active state
    assert (state_ != active);

    // the state must be different
    assert (state_ != m_state);

    // you can't transition into the initial states
    assert (state_ != accept && state_ != connect);

    // can only become connected from outbound connect state
    assert (state_ != connected || (! m_inbound && m_state == connect));

    // can't gracefully close on an outbound connection attempt
    assert (state_ != closing || m_state != connect);

    m_state = state_;
}

void
slotimp::activate (clock_type::time_point const& now)
{
    // can only become active from the accept or connected state
    assert (m_state == accept || m_state == connected);

    m_state = active;
    whenacceptendpoints = now;
}

//------------------------------------------------------------------------------

slot::~slot ()
{
}

//------------------------------------------------------------------------------

slotimp::recent_t::recent_t (clock_type& clock)
    : cache (clock)
{
}

void
slotimp::recent_t::insert (beast::ip::endpoint const& ep, int hops)
{
    auto const result (cache.emplace (ep, hops));
    if (! result.second)
    {
        // note other logic depends on this <= inequality.
        if (hops <= result.first->second)
        {
            result.first->second = hops;
            cache.touch (result.first);
        }
    }
}

bool
slotimp::recent_t::filter (beast::ip::endpoint const& ep, int hops)
{
    auto const iter (cache.find (ep));
    if (iter == cache.end())
        return false;
    // we avoid sending an endpoint if we heard it
    // from them recently at the same or lower hop count.
    // note other logic depends on this <= inequality.
    return iter->second <= hops;
}

void
slotimp::recent_t::expire ()
{
    beast::expire (cache,
        tuning::livecachesecondstolive);
}

}
}
