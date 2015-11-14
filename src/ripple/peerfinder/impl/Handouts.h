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

#ifndef ripple_peerfinder_handouts_h_included
#define ripple_peerfinder_handouts_h_included

#include <ripple/peerfinder/impl/slotimp.h>
#include <ripple/peerfinder/impl/tuning.h>
#include <beast/container/aged_set.h>
#include <cassert>
#include <iterator>
#include <beast/cxx14/type_traits.h> // <type_traits>

namespace ripple {
namespace peerfinder {

namespace detail {

/** try to insert one object in the target.
    when an item is handed out it is moved to the end of the container.
    @return the number of objects inserted
*/
// vfalco todo specialization that handles std::list for sequencecontainer
//             using splice for optimization over erase/push_back
//
template <class target, class hopcontainer>
std::size_t
handout_one (target& t, hopcontainer& h)
{
    assert (! t.full());
    for (auto it = h.begin(); it != h.end(); ++it)
    {
        auto const& e = *it;
        if (t.try_insert (e))
        {
            h.move_back (it);
            return 1;
        }
    }
    return 0;
}

}

/** distributes objects to targets according to business rules.
    a best effort is made to evenly distribute items in the sequence
    container list into the target sequence list.
*/
template <class targetfwditer, class seqfwditer>
void
handout (targetfwditer first, targetfwditer last,
    seqfwditer seq_first, seqfwditer seq_last)
{
    for (;;)
    {
        std::size_t n (0);
        for (auto si = seq_first; si != seq_last; ++si)
        {
            auto c = *si;
            bool all_full (true);
            for (auto ti = first; ti != last; ++ti)
            {
                auto& t = *ti;
                if (! t.full())
                {
                    n += detail::handout_one (t, c);
                    all_full = false;
                }
            }
            if (all_full)
                return;
        }
        if (! n)
            break;
    }
}

//------------------------------------------------------------------------------

/** receives handouts for redirecting a connection.
    an incoming connection request is redirected when we are full on slots.
*/
class redirecthandouts
{
public:
    template <class = void>
    explicit
    redirecthandouts (slotimp::ptr const& slot);

    template <class = void>
    bool try_insert (endpoint const& ep);

    bool full () const
    {
        return list_.size() >= tuning::redirectendpointcount;
    }

    slotimp::ptr const& slot () const
    {
        return slot_;
    }

    std::vector <endpoint>& list()
    {
        return list_;
    }

    std::vector <endpoint> const& list() const
    {
        return list_;
    }

private:
    slotimp::ptr slot_;
    std::vector <endpoint> list_;
};

template <class>
redirecthandouts::redirecthandouts (slotimp::ptr const& slot)
    : slot_ (slot)
{
    list_.reserve (tuning::redirectendpointcount);
}

template <class>
bool
redirecthandouts::try_insert (endpoint const& ep)
{
    if (full ())
        return false;

    // vfalco note this check can be removed when we provide the
    //             addresses in a peer http handshake instead of
    //             the tmendpoints message.
    //
    if (ep.hops > tuning::maxhops)
        return false;

    // don't send them our address
    if (ep.hops == 0)
        return false;

    // don't send them their own address
    if (slot_->remote_endpoint().address() ==
        ep.address.address())
        return false;

    // make sure the address isn't already in our list
    if (std::any_of (list_.begin(), list_.end(),
        [&ep](endpoint const& other)
        {
            // ignore port for security reasons
            return other.address.address() == ep.address.address();
        }))
    {
        return false;
    }

    list_.emplace_back (ep.address, ep.hops);

    return true;
}

//------------------------------------------------------------------------------

/** receives endpoints for a slot during periodic handouts. */
class slothandouts
{
public:
    template <class = void>
    explicit
    slothandouts (slotimp::ptr const& slot);

    template <class = void>
    bool try_insert (endpoint const& ep);

    bool full () const
    {
        return list_.size() >= tuning::numberofendpoints;
    }

    void insert (endpoint const& ep)
    {
        list_.push_back (ep);
    }

    slotimp::ptr const& slot () const
    {
        return slot_;
    }

    std::vector <endpoint> const& list() const
    {
        return list_;
    }

private:
    slotimp::ptr slot_;
    std::vector <endpoint> list_;
};

template <class>
slothandouts::slothandouts (slotimp::ptr const& slot)
    : slot_ (slot)
{
    list_.reserve (tuning::numberofendpoints);
}

template <class>
bool
slothandouts::try_insert (endpoint const& ep)
{
    if (full ())
        return false;

    if (ep.hops > tuning::maxhops)
        return false;

    if (slot_->recent.filter (ep.address, ep.hops))
        return false;

    // don't send them their own address
    if (slot_->remote_endpoint().address() ==
        ep.address.address())
        return false;

    // make sure the address isn't already in our list
    if (std::any_of (list_.begin(), list_.end(),
        [&ep](endpoint const& other)
        {
            // ignore port for security reasons
            return other.address.address() == ep.address.address();
        }))
        return false;

    list_.emplace_back (ep.address, ep.hops);

    // insert into this slot's recent table. although the endpoint
    // didn't come from the slot, adding it to the slot's table
    // prevents us from sending it again until it has expired from
    // the other end's cache.
    //
    slot_->recent.insert (ep.address, ep.hops);

    return true;
}

//------------------------------------------------------------------------------

/** receives handouts for making automatic connections. */
class connecthandouts
{
public:
    // keeps track of addresses we have made outgoing connections
    // to, for the purposes of not connecting to them too frequently.
    typedef beast::aged_set <beast::ip::address> squelches;

    typedef std::vector <beast::ip::endpoint> list_type;

private:
    std::size_t m_needed;
    squelches& m_squelches;
    list_type m_list;

public:
    template <class = void>
    connecthandouts (std::size_t needed, squelches& squelches);

    template <class = void>
    bool try_insert (beast::ip::endpoint const& endpoint);

    bool empty() const
    {
        return m_list.empty();
    }

    bool full() const
    {
        return m_list.size() >= m_needed;
    }

    bool try_insert (endpoint const& endpoint)
    {
        return try_insert (endpoint.address);
    }

    list_type& list()
    {
        return m_list;
    }

    list_type const& list() const
    {
        return m_list;
    }
};

template <class>
connecthandouts::connecthandouts (
    std::size_t needed, squelches& squelches)
    : m_needed (needed)
    , m_squelches (squelches)
{
    m_list.reserve (needed);
}

template <class>
bool
connecthandouts::try_insert (beast::ip::endpoint const& endpoint)
{
    if (full ())
        return false;

    // make sure the address isn't already in our list
    if (std::any_of (m_list.begin(), m_list.end(),
        [&endpoint](beast::ip::endpoint const& other)
        {
            // ignore port for security reasons
            return other.address() ==
                endpoint.address();
        }))
    {
        return false;
    }

    // add to squelch list so we don't try it too often.
    // if its already there, then make try_insert fail.
    auto const result (m_squelches.insert (
        endpoint.address()));
    if (! result.second)
        return false;

    m_list.push_back (endpoint);

    return true;
}

}
}

#endif
