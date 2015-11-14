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

#ifndef ripple_overlay_peer_h_included
#define ripple_overlay_peer_h_included

#include <ripple/overlay/message.h>
#include <ripple/basics/base_uint.h>
#include <ripple/json/json_value.h>
#include <ripple/protocol/rippleaddress.h>
#include <beast/net/ipendpoint.h>

namespace ripple {

namespace resource {
class charge;
}

/** represents a peer connection in the overlay. */
class peer
{
public:
    using ptr = std::shared_ptr<peer>;

    /** uniquely identifies a peer.
        this can be stored in tables to find the peer later. callers
        can discover if the peer is no longer connected and make
        adjustments as needed.
    */
    using id_t = std::uint32_t;

    virtual ~peer() = default;

    //
    // network
    //

    virtual
    void
    send (message::pointer const& m) = 0;

    virtual
    beast::ip::endpoint
    getremoteaddress() const = 0;

    /** adjust this peer's load balance based on the type of load imposed. */
    virtual
    void
    charge (resource::charge const& fee) = 0;

    //
    // identity
    //

    virtual
    id_t
    id() const = 0;

    /** returns `true` if this connection is a member of the cluster. */
    virtual
    bool
    cluster() const = 0;

    virtual
    rippleaddress const&
    getnodepublic() const = 0;

    virtual
    json::value json() = 0;

    //
    // ledger
    //

    virtual uint256 const& getclosedledgerhash () const = 0;
    virtual bool hasledger (uint256 const& hash, std::uint32_t seq) const = 0;
    virtual void ledgerrange (std::uint32_t& minseq, std::uint32_t& maxseq) const = 0;
    virtual bool hastxset (uint256 const& hash) const = 0;
    virtual void cyclestatus () = 0;
    virtual bool supportsversion (int version) = 0;
    virtual bool hasrange (std::uint32_t umin, std::uint32_t umax) = 0;
};

}

#endif
