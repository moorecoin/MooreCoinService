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

#ifndef ripple_peerfinder_slot_h_included
#define ripple_peerfinder_slot_h_included

#include <ripple/protocol/ripplepublickey.h>
#include <beast/net/ipendpoint.h>
#include <boost/optional.hpp>
#include <memory>

namespace ripple {
namespace peerfinder {

/** properties and state associated with a peer to peer overlay connection. */
class slot
{
public:
    typedef std::shared_ptr <slot> ptr;

    enum state
    {
        accept,
        connect,
        connected,
        active,
        closing
    };

    virtual ~slot () = 0;

    /** returns `true` if this is an inbound connection. */
    virtual bool inbound () const = 0;

    /** returns `true` if this is a fixed connection.
        a connection is fixed if its remote endpoint is in the list of
        remote endpoints for fixed connections.
    */
    virtual bool fixed () const = 0;

    /** returns `true` if this is a cluster connection.
        this is only known after then handshake completes.
    */
    virtual bool cluster () const = 0;

    /** returns the state of the connection. */
    virtual state state () const = 0;

    /** the remote endpoint of socket. */
    virtual beast::ip::endpoint const& remote_endpoint () const = 0;

    /** the local endpoint of the socket, when known. */
    virtual boost::optional <beast::ip::endpoint> const& local_endpoint () const = 0;

    /** the peer's public key, when known.
        the public key is established when the handshake is complete.
    */
    virtual boost::optional <ripplepublickey> const& public_key () const = 0;
};

}
}

#endif
