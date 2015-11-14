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

#ifndef ripple_peerfinder_slotimp_h_included
#define ripple_peerfinder_slotimp_h_included

#include <ripple/peerfinder/slot.h>
#include <ripple/peerfinder/manager.h>
#include <beast/container/aged_unordered_map.h>
#include <beast/container/aged_container_utility.h>
#include <boost/optional.hpp>

namespace ripple {
namespace peerfinder {

class slotimp : public slot
{
private:
    typedef beast::aged_unordered_map <beast::ip::endpoint, int> recent_type;

public:
    typedef std::shared_ptr <slotimp> ptr;

    // inbound
    slotimp (beast::ip::endpoint const& local_endpoint,
        beast::ip::endpoint const& remote_endpoint, bool fixed,
            clock_type& clock);

    // outbound
    slotimp (beast::ip::endpoint const& remote_endpoint,
        bool fixed, clock_type& clock);

    bool inbound () const
    {
        return m_inbound;
    }

    bool fixed () const
    {
        return m_fixed;
    }

    bool cluster () const
    {
        return m_cluster;
    }

    state state () const
    {
        return m_state;
    }

    beast::ip::endpoint const& remote_endpoint () const
    {
        return m_remote_endpoint;
    }

    boost::optional <beast::ip::endpoint> const& local_endpoint () const
    {
        return m_local_endpoint;
    }

    boost::optional <ripplepublickey> const& public_key () const
    {
        return m_public_key;
    }

    void local_endpoint (beast::ip::endpoint const& endpoint)
    {
        m_local_endpoint = endpoint;
    }

    void remote_endpoint (beast::ip::endpoint const& endpoint)
    {
        m_remote_endpoint = endpoint;
    }

    void public_key (ripplepublickey const& key)
    {
        m_public_key = key;
    }

    void cluster (bool cluster_)
    {
        m_cluster = cluster_;
    }

    //--------------------------------------------------------------------------

    void state (state state_);

    void activate (clock_type::time_point const& now);

    // "memberspace"
    //
    // the set of all recent addresses that we have seen from this peer.
    // we try to avoid sending a peer the same addresses they gave us.
    //
    class recent_t
    {
    public:
        explicit recent_t (clock_type& clock);

        /** called for each valid endpoint received for a slot.
            we also insert messages that we send to the slot to prevent
            sending a slot the same address too frequently.
        */
        void insert (beast::ip::endpoint const& ep, int hops);

        /** returns `true` if we should not send endpoint to the slot. */
        bool filter (beast::ip::endpoint const& ep, int hops);

    private:
        void expire ();

        friend class slotimp;
        recent_type cache;
    } recent;

    void expire()
    {
        recent.expire();
    }

private:
    bool const m_inbound;
    bool const m_fixed;
    bool m_cluster;
    state m_state;
    beast::ip::endpoint m_remote_endpoint;
    boost::optional <beast::ip::endpoint> m_local_endpoint;
    boost::optional <ripplepublickey> m_public_key;

public:
    // deprecated public data members

    // tells us if we checked the connection. outbound connections
    // are always considered checked since we successfuly connected.
    bool checked;

    // set to indicate if the connection can receive incoming at the
    // address advertised in mtendpoints. only valid if checked is true.
    bool canaccept;

    // set to indicate that a connection check for this peer is in
    // progress. valid always.
    bool connectivitycheckinprogress;

    // the time after which we will accept mtendpoints from the peer
    // this is to prevent flooding or spamming. receipt of mtendpoints
    // sooner than the allotted time should impose a load charge.
    //
    clock_type::time_point whenacceptendpoints;
};

}
}

#endif
