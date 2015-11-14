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

#ifndef ripple_server_plainpeer_h_included
#define ripple_server_plainpeer_h_included

#include <ripple/server/impl/peer.h>
#include <memory>

namespace ripple {
namespace http {

class plainpeer
    : public peer <plainpeer>
    , public std::enable_shared_from_this <plainpeer>
{
private:
    friend class peer <plainpeer>;
    using socket_type = boost::asio::ip::tcp::socket;

    socket_type stream_;

public:
    template <class constbuffersequence>
    plainpeer (door& door, beast::journal journal, endpoint_type endpoint,
        constbuffersequence const& buffers, socket_type&& socket);

    void
    run();

private:
    void
    do_request();

    void
    do_close();
};

//------------------------------------------------------------------------------

template <class constbuffersequence>
plainpeer::plainpeer (door& door, beast::journal journal,
    endpoint_type remote_address, constbuffersequence const& buffers,
        socket_type&& socket)
    : peer (door, socket.get_io_service(), journal, remote_address, buffers)
    , stream_(std::move(socket))
{
}

void
plainpeer::run ()
{
    door_.server().handler().onaccept (session());
    if (! stream_.is_open())
        return;

    boost::asio::spawn (strand_, std::bind (&plainpeer::do_read,
        shared_from_this(), std::placeholders::_1));
}

void
plainpeer::do_request()
{
    ++request_count_;
    auto const what = door_.server().handler().onhandoff (session(),
        std::move(stream_), std::move(message_), remote_address_);
    if (what.moved)
        return;
    error_code ec;
    if (what.response)
    {
        // half-close on connection: close
        if (! what.keep_alive)
            stream_.shutdown (socket_type::shutdown_receive, ec);
        if (ec)
            return fail (ec, "request");
        return write(what.response, what.keep_alive);
    }

    // perform half-close when connection: close and not ssl
    if (! message_.keep_alive())
        stream_.shutdown (socket_type::shutdown_receive, ec);
    if (ec)
        return fail (ec, "request");
    // legacy
    door_.server().handler().onrequest (session());
}

void
plainpeer::do_close()
{
    error_code ec;
    stream_.shutdown (socket_type::shutdown_send, ec);
}

}
}

#endif
