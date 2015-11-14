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

#ifndef ripple_server_sslpeer_h_included
#define ripple_server_sslpeer_h_included

#include <ripple/server/impl/peer.h>
#include <beast/asio/ssl_bundle.h>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {
namespace http {

class sslpeer
    : public peer <sslpeer>
    , public std::enable_shared_from_this <sslpeer>
{
private:
    friend class peer <sslpeer>;
    using socket_type = boost::asio::ip::tcp::socket;
    using stream_type = boost::asio::ssl::stream <socket_type&>;

    std::unique_ptr<beast::asio::ssl_bundle> ssl_bundle_;
    stream_type& stream_;

public:
    template <class constbuffersequence>
    sslpeer (door& door, beast::journal journal, endpoint_type remote_address,
        constbuffersequence const& buffers, socket_type&& socket);

    void
    run();

private:
    void
    do_handshake (yield_context yield);

    void
    do_request();

    void
    do_close();

    void
    on_shutdown (error_code ec);
};

//------------------------------------------------------------------------------

// detects the legacy peer protocol handshake. */
template <class socket, class streambuf, class yield>
static
std::pair <boost::system::error_code, bool>
detect_peer_protocol (socket& socket, streambuf& buf, yield yield)
{
    std::pair<boost::system::error_code, bool> result;
    result.second = false;
    for(;;)
    {
        std::size_t const max = 6; // max bytes needed
        unsigned char data[max];
        auto const n = boost::asio::buffer_copy(
            boost::asio::buffer(data), buf.data());

        /* protocol messages are framed by a 6 byte header which includes
           a big-endian 4-byte length followed by a big-endian 2-byte type.
           the type for 'hello' is 1.
        */
        if (n>=1 && data[0] != 0)
            break;
        if (n>=2 && data[1] != 0)
            break;
        if (n>=5 && data[4] != 0)
            break;
        if (n>=6)
        {
            if (data[5] == 181) 
                result.second = true;
            break;
        }
        std::size_t const bytes_transferred = boost::asio::async_read(
            socket, buf.prepare(max - n), boost::asio::transfer_at_least(1),
                yield[result.first]);
        if (result.first)
            break;
        buf.commit(bytes_transferred);
    }
    return result;
}

template <class constbuffersequence>
sslpeer::sslpeer (door& door, beast::journal journal,
    endpoint_type remote_address, constbuffersequence const& buffers,
        socket_type&& socket)
    : peer (door, socket.get_io_service(), journal, remote_address, buffers)
    , ssl_bundle_(std::make_unique<beast::asio::ssl_bundle>(
        port().context, std::move(socket)))
    , stream_(ssl_bundle_->stream)
{
}

// called when the acceptor accepts our socket.
void
sslpeer::run()
{
    door_.server().handler().onaccept (session());
    if (! stream_.lowest_layer().is_open())
        return;

    boost::asio::spawn (strand_, std::bind (&sslpeer::do_handshake,
        shared_from_this(), std::placeholders::_1));
}

void
sslpeer::do_handshake (yield_context yield)
{
    error_code ec;
    stream_.set_verify_mode (boost::asio::ssl::verify_none);
    start_timer();
    read_buf_.consume(stream_.async_handshake(
        stream_type::server, read_buf_.data(), yield[ec]));
    cancel_timer();
    if (ec)
        return fail (ec, "handshake");
    bool const legacy = port().protocol.count("peer") > 0;
    bool const http =
        port().protocol.count("peer") > 0 ||
        //|| port().protocol.count("wss") > 0
        port().protocol.count("https") > 0;
    if (legacy)
    {
        auto const result = detect_peer_protocol(stream_, read_buf_, yield);
        if (result.first)
            return fail (result.first, "detect_legacy_handshake");
        if (result.second)
        {
            std::vector<std::uint8_t> storage (read_buf_.size());
            boost::asio::mutable_buffers_1 buffer (
                boost::asio::mutable_buffer(storage.data(), storage.size()));
            boost::asio::buffer_copy(buffer, read_buf_.data());
            return door_.server().handler().onlegacypeerhello(
                std::move(ssl_bundle_), buffer, remote_address_);
        }
    }
    if (http)
    {
        boost::asio::spawn (strand_, std::bind (&sslpeer::do_read,
            shared_from_this(), std::placeholders::_1));
        return;
    }
    // this will be destroyed
}

void
sslpeer::do_request()
{
    ++request_count_;
    auto const what = door_.server().handler().onhandoff (session(),
        std::move(ssl_bundle_), std::move(message_), remote_address_);
    if (what.moved)
        return;
    if (what.response)
        return write(what.response, what.keep_alive);
    // legacy
    door_.server().handler().onrequest (session());
}

void
sslpeer::do_close()
{
    start_timer();
    stream_.async_shutdown (strand_.wrap (std::bind (
        &sslpeer::on_shutdown, shared_from_this(),
            std::placeholders::_1)));
}

void
sslpeer::on_shutdown (error_code ec)
{
    cancel_timer();
    stream_.lowest_layer().close(ec);
}

}
}

#endif
