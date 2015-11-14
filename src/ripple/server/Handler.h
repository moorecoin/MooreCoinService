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

#ifndef ripple_server_handler_h_included
#define ripple_server_handler_h_included

#include <ripple/server/handoff.h>
#include <beast/asio/ssl_bundle.h>
#include <beast/http/message.h>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <memory>

namespace ripple {
namespace http {

class server;
class session;

/** processes all sessions.
    thread safety:
        must be safe to call concurrently from any number of foreign threads.
*/
struct handler
{
    /** called when the connection is accepted and we know remoteaddress. */
    // deprecated
    virtual void onaccept (session& session) = 0;

    /** called when a connection is accepted.
        @return `true` if we should keep the connection.
    */
    virtual
    bool
    onaccept (session& session,
        boost::asio::ip::tcp::endpoint remote_address) = 0;

    /** called when a legacy peer protocol handshake is detected.
        if the called function does not take ownership, then the
        connection is closed.
        @param buffer the unconsumed bytes in the protocol handshake
        @param ssl_bundle the active connection.
    */
    virtual
    void
    onlegacypeerhello (std::unique_ptr<beast::asio::ssl_bundle>&& ssl_bundle,
        boost::asio::const_buffer buffer,
            boost::asio::ip::tcp::endpoint remote_address) = 0;

    /** called to process a complete http request.
        the handler can do one of three things:
            - ignore the request (return default constructed what)
            - return a response (by setting response in the what)
            - take ownership of the socket by using rvalue move
              and setting moved = `true` in the what.
        if the handler ignores the request, the legacy onrequest
        is called.
    */
    /** @{ */
    virtual
    handoff
    onhandoff (session& session,
        std::unique_ptr <beast::asio::ssl_bundle>&& bundle,
            beast::http::message&& request,
                boost::asio::ip::tcp::endpoint remote_address) = 0;

    virtual
    handoff
    onhandoff (session& session, boost::asio::ip::tcp::socket&& socket,
        beast::http::message&& request,
            boost::asio::ip::tcp::endpoint remote_address) = 0;
    /** @} */

    /** called when we have a complete http request. */
    // vfalco todo pass the beast::http::message as a parameter
    virtual void onrequest (session& session) = 0;

    /** called when the session ends.
        guaranteed to be called once.
        @param errorcode non zero for a failed connection.
    */
    virtual void onclose (session& session,
        boost::system::error_code const& ec) = 0;

    /** called when the server has finished its stop. */
    virtual void onstopped (server& server) = 0;
};

} // http
} // ripple

#endif
