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

#ifndef ripple_overlay_overlay_h_included
#define ripple_overlay_overlay_h_included

#include <ripple/json/json_value.h>
#include <ripple/overlay/peer.h>
#include <ripple/server/handoff.h>
#include <beast/asio/ssl_bundle.h>
#include <beast/http/message.h>
#include <beast/threads/stoppable.h>
#include <beast/utility/propertystream.h>
#include <memory>
#include <beast/cxx14/type_traits.h> // <type_traits>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace boost { namespace asio { namespace ssl { class context; } } }

namespace ripple {

/** manages the set of connected peers. */
class overlay
    : public beast::stoppable
    , public beast::propertystream::source
{
protected:
    // vfalco note the requirement of this constructor is an
    //             unfortunate problem with the api for
    //             stoppable and propertystream
    //
    overlay (stoppable& parent)
        : stoppable ("overlay", parent)
        , beast::propertystream::source ("peers")
    {
    }

public:
    enum class promote
    {
        automatic,
        never,
        always
    };

    struct setup
    {
        bool auto_connect = true;
        bool http_handshake = false;
        promote promote = promote::automatic;
        std::shared_ptr<boost::asio::ssl::context> context;
    };

    typedef std::vector <peer::ptr> peersequence;

    virtual ~overlay() = default;

    /** accept a legacy protocol handshake connection. */
    virtual
    void
    onlegacypeerhello (std::unique_ptr<beast::asio::ssl_bundle>&& ssl_bundle,
        boost::asio::const_buffer buffer,
            boost::asio::ip::tcp::endpoint remote_address) = 0;

    /** conditionally accept an incoming http request. */
    virtual
    handoff
    onhandoff (std::unique_ptr <beast::asio::ssl_bundle>&& bundle,
        beast::http::message&& request,
            boost::asio::ip::tcp::endpoint remote_address) = 0;

    /** establish a peer connection to the specified endpoint.
        the call returns immediately, the connection attempt is
        performed asynchronously.
    */
    virtual
    void
    connect (beast::ip::endpoint const& address) = 0;

    /** returns the number of active peers.
        active peers are only those peers that have completed the
        handshake and are using the peer protocol.
    */
    virtual
    std::size_t
    size () = 0;

    /** returns information reported to the crawl cgi command. */
    virtual
    json::value
    crawl() = 0;

    /** return diagnostics on the status of all peers.
        @deprecated this is superceded by propertystream
    */
    virtual
    json::value
    json () = 0;

    /** returns a sequence representing the current list of peers.
        the snapshot is made at the time of the call.
    */
    virtual
    peersequence
    getactivepeers () = 0;

    /** returns the peer with the matching short id, or null. */
    virtual
    peer::ptr
    findpeerbyshortid (peer::id_t const& id) = 0;

    /** visit every active peer and return a value
        the functor must:
        - be callable as:
            void operator()(peer::ptr const& peer);
         - must have the following typedef:
            typedef void return_type;
         - be callable as:
            function::return_type operator()() const;

        @param f the functor to call with every peer
        @returns `f()`

        @note the functor is passed by value!
    */
    template<typename function>
    std::enable_if_t <
        ! std::is_void <typename function::return_type>::value,
        typename function::return_type
    >
    foreach(function f)
    {
        peersequence peers (getactivepeers());
        for(peersequence::const_iterator i = peers.begin(); i != peers.end(); ++i)
            f (*i);
        return f();
    }

    /** visit every active peer
        the visitor functor must:
         - be callable as:
            void operator()(peer::ptr const& peer);
         - must have the following typedef:
            typedef void return_type;

        @param f the functor to call with every peer
    */
    template <class function>
    std::enable_if_t <
        std::is_void <typename function::return_type>::value,
        typename function::return_type
    >
    foreach(function f)
    {
        peersequence peers (getactivepeers());

        for(peersequence::const_iterator i = peers.begin(); i != peers.end(); ++i)
            f (*i);
    }
};

}

#endif
