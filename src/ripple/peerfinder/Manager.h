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

#ifndef ripple_peerfinder_manager_h_included
#define ripple_peerfinder_manager_h_included

#include <ripple/peerfinder/slot.h>
#include <beast/chrono/abstract_clock.h>
#include <beast/module/core/files/file.h>
#include <beast/threads/stoppable.h>
#include <beast/utility/propertystream.h>
#include <boost/asio/ip/tcp.hpp>

namespace ripple {
namespace peerfinder {

typedef beast::abstract_clock <std::chrono::steady_clock> clock_type;

/** represents a set of addresses. */
typedef std::vector <beast::ip::endpoint> ipaddresses;

//------------------------------------------------------------------------------

/** peerfinder configuration settings. */
struct config
{
    /** the largest number of public peer slots to allow.
        this includes both inbound and outbound, but does not include
        fixed peers.
    */
    int maxpeers;

    /** the number of automatic outbound connections to maintain.
        outbound connections are only maintained if autoconnect
        is `true`. the value can be fractional; the decision to round up
        or down will be made using a per-process pseudorandom number and
        a probability proportional to the fractional part.
        example:
            if outpeers is 9.3, then 30% of nodes will maintain 9 outbound
            connections, while 70% of nodes will maintain 10 outbound
            connections.
    */
    double outpeers;

    /** `true` if we want our ip address kept private. */
    bool peerprivate = true;

    /** `true` if we want to accept incoming connections. */
    bool wantincoming;

    /** `true` if we want to establish connections automatically */
    bool autoconnect;

    /** the listening port number. */
    std::uint16_t listeningport;

    /** the set of features we advertise. */
    std::string features;

    //--------------------------------------------------------------------------

    /** create a configuration with default values. */
    config ();

    /** returns a suitable value for outpeers according to the rules. */
    double calcoutpeers () const;

    /** adjusts the values so they follow the business rules. */
    void applytuning ();

    /** write the configuration into a property stream */
    void onwrite (beast::propertystream::map& map);
};

//------------------------------------------------------------------------------

/** describes a connectible peer address along with some metadata. */
struct endpoint
{
    endpoint ();

    endpoint (beast::ip::endpoint const& ep, int hops_);

    int hops;
    beast::ip::endpoint address;
};

bool operator< (endpoint const& lhs, endpoint const& rhs);

/** a set of endpoint used for connecting. */
typedef std::vector <endpoint> endpoints;

//------------------------------------------------------------------------------

/** possible results from activating a slot. */
enum class result
{
    duplicate,
    full,
    success
};

/** maintains a set of ip addresses used for getting into the network. */
class manager
    : public beast::stoppable
    , public beast::propertystream::source
{
protected:
    explicit manager (stoppable& parent);

public:
    /** destroy the object.
        any pending source fetch operations are aborted.
        there may be some listener calls made before the
        destructor returns.
    */
    virtual ~manager() = default;

    /** set the configuration for the manager.
        the new settings will be applied asynchronously.
        thread safety:
            can be called from any threads at any time.
    */
    virtual void setconfig (config const& config) = 0;

    /** returns the configuration for the manager. */
    virtual
    config
    config() = 0;

    /** add a peer that should always be connected.
        this is useful for maintaining a private cluster of peers.
        the string is the name as specified in the configuration
        file, along with the set of corresponding ip addresses.
    */
    virtual void addfixedpeer (std::string const& name,
        std::vector <beast::ip::endpoint> const& addresses) = 0;

    /** add a set of strings as fallback ip::endpoint sources.
        @param name a label used for diagnostics.
    */
    virtual void addfallbackstrings (std::string const& name,
        std::vector <std::string> const& strings) = 0;

    /** add a url as a fallback location to obtain ip::endpoint sources.
        @param name a label used for diagnostics.
    */
    /* vfalco note unimplemented
    virtual void addfallbackurl (std::string const& name,
        std::string const& url) = 0;
    */

    //--------------------------------------------------------------------------

    /** create a new inbound slot with the specified remote endpoint.
        if nullptr is returned, then the slot could not be assigned.
        usually this is because of a detected self-connection.
    */
    virtual slot::ptr new_inbound_slot (
        beast::ip::endpoint const& local_endpoint,
            beast::ip::endpoint const& remote_endpoint) = 0;

    /** create a new outbound slot with the specified remote endpoint.
        if nullptr is returned, then the slot could not be assigned.
        usually this is because of a duplicate connection.
    */
    virtual slot::ptr new_outbound_slot (
        beast::ip::endpoint const& remote_endpoint) = 0;

    /** called when mtendpoints is received. */
    virtual void on_endpoints (slot::ptr const& slot,
        endpoints const& endpoints) = 0;

    /** called when legacy ip/port addresses are received. */
    virtual void on_legacy_endpoints (ipaddresses const& addresses) = 0;

    /** called when the slot is closed.
        this always happens when the socket is closed, unless the socket
        was canceled.
    */
    virtual void on_closed (slot::ptr const& slot) = 0;

    /** called when we received redirect ips from a busy peer. */
    virtual
    void
    onredirects (boost::asio::ip::tcp::endpoint const& remote_address,
        std::vector<boost::asio::ip::tcp::endpoint> const& eps) = 0;

    //--------------------------------------------------------------------------

    /** called when an outbound connection attempt succeeds.
        the local endpoint must be valid. if the caller receives an error
        when retrieving the local endpoint from the socket, it should
        proceed as if the connection attempt failed by calling on_closed
        instead of on_connected.
        @return `true` if the connection should be kept
    */
    virtual
    bool
    onconnected (slot::ptr const& slot,
        beast::ip::endpoint const& local_endpoint) = 0;

    /** request an active slot type. */
    virtual
    result
    activate (slot::ptr const& slot,
        ripplepublickey const& key, bool cluster) = 0;

    /** returns a set of endpoints suitable for redirection. */
    virtual
    std::vector <endpoint>
    redirect (slot::ptr const& slot) = 0;

    /** return a set of addresses we should connect to. */
    virtual
    std::vector <beast::ip::endpoint>
    autoconnect() = 0;

    virtual
    std::vector<std::pair<slot::ptr, std::vector<endpoint>>>
    buildendpointsforpeers() = 0;

    /** perform periodic activity.
        this should be called once per second.
    */
    virtual
    void
    once_per_second() = 0;
};

}
}

#endif
