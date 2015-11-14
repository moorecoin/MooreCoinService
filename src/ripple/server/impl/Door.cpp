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
#include <ripple/server/impl/door.h>
#include <ripple/server/impl/plainpeer.h>
#include <ripple/server/impl/sslpeer.h>
#include <boost/asio/buffer.hpp>
#include <beast/asio/placeholders.h>
#include <beast/asio/ssl_bundle.h>
#include <functional>

namespace ripple {
namespace http {

/** detect ssl client handshakes.
    analyzes the bytes in the provided buffer to detect the ssl client
    handshake. if the buffer contains insufficient data, more data will be
    read from the stream until there is enough to determine a result.
    no bytes are discarded from buf. any additional bytes read are retained.
    buf must provide an interface compatible with boost::asio::streambuf
        http://www.boost.org/doc/libs/1_56_0/doc/html/boost_asio/reference/streambuf.html
    see
        http://www.ietf.org/rfc/rfc2246.txt
        section 7.4. handshake protocol
    @param socket the stream to read from
    @param buf a buffer to hold the received data
    @param yield a yield context
    @return the error code if an error occurs, otherwise `true` if
            the data read indicates the ssl client handshake.
*/
template <class socket, class streambuf, class yield>
std::pair <boost::system::error_code, bool>
detect_ssl (socket& socket, streambuf& buf, yield yield)
{
    std::pair <boost::system::error_code, bool> result;
    result.second = false;
    for(;;)
    {
        std::size_t const max = 4; // the most bytes we could need
        unsigned char data[max];
        auto const bytes = boost::asio::buffer_copy (
            boost::asio::buffer(data), buf.data());

        if (bytes > 0)
        {
            if (data[0] != 0x16) // message type 0x16 = "ssl handshake"
                break;
        }

        if (bytes >= max)
        {
            result.second = true;
            break;
        }

        buf.commit(boost::asio::async_read (socket,
            buf.prepare(max - bytes), boost::asio::transfer_at_least(1),
                yield[result.first]));
        if (result.first)
            break;
    }
    return result;
}

//------------------------------------------------------------------------------

door::child::child(door& door)
    : door_(door)
{
}

door::child::~child()
{
    door_.remove(*this);
}

//------------------------------------------------------------------------------

door::detector::detector (door& door, socket_type&& socket,
        endpoint_type remote_address)
    : child(door)
    , socket_(std::move(socket))
    , timer_(socket_.get_io_service())
    , remote_address_(remote_address)
{
}

void
door::detector::run()
{
    // do_detect must be called before do_timer or else
    // the timer can be canceled before it gets set.
    boost::asio::spawn (door_.strand_, std::bind (&detector::do_detect,
        shared_from_this(), std::placeholders::_1));

    boost::asio::spawn (door_.strand_, std::bind (&detector::do_timer,
        shared_from_this(), std::placeholders::_1));
}

void
door::detector::close()
{
    error_code ec;
    socket_.close(ec);
    timer_.cancel(ec);
}

void
door::detector::do_timer (yield_context yield)
{
    error_code ec; // ignored
    while (socket_.is_open())
    {
        timer_.async_wait (yield[ec]);
        if (timer_.expires_from_now() <= std::chrono::seconds(0))
            socket_.close(ec);
    }
}

void
door::detector::do_detect (boost::asio::yield_context yield)
{
    bool ssl;
    error_code ec;
    beast::asio::streambuf buf(16);
    timer_.expires_from_now(std::chrono::seconds(15));
    std::tie(ec, ssl) = detect_ssl(socket_, buf, yield);
    error_code unused;
    timer_.cancel(unused);
    if (! ec)
        return door_.create(ssl, buf.data(),
            std::move(socket_), remote_address_);
    if (ec != boost::asio::error::operation_aborted)
        if (door_.server_.journal().trace) door_.server_.journal().trace <<
            "error detecting ssl: " << ec.message() <<
                " from " << remote_address_;
}

//------------------------------------------------------------------------------

door::door (boost::asio::io_service& io_service,
        serverimpl& server, port const& port)
    : port_(std::make_shared<port>(port))
    , server_(server)
    , acceptor_(io_service)
    , strand_(io_service)
    , ssl_ (
        port_->protocol.count("https") > 0 ||
        //port_->protocol.count("wss") > 0 ||
        port_->protocol.count("peer") > 0)
    , plain_ (
        //port_->protocol.count("ws") > 0 ||
        port_->protocol.count("http") > 0)
{
    server_.add (*this);

    error_code ec;
    endpoint_type const local_address =
        endpoint_type(port.ip, port.port);

    acceptor_.open(local_address.protocol(), ec);
    if (ec)
    {
        if (server_.journal().error) server_.journal().error <<
            "open port '" << port.name << "' failed:" << ec.message();
        throw std::exception();
        return;
    }

    acceptor_.set_option(
        boost::asio::ip::tcp::acceptor::reuse_address(true), ec);
    if (ec)
    {
        if (server_.journal().error) server_.journal().error <<
            "option for port '" << port.name << "' failed:" << ec.message();
        throw std::exception();
        return;
    }

    acceptor_.bind(local_address, ec);
    if (ec)
    {
        if (server_.journal().error) server_.journal().error <<
            "bind port '" << port.name << "' failed:" << ec.message();
        throw std::exception();
        return;
    }

    acceptor_.listen(boost::asio::socket_base::max_connections, ec);
    if (ec)
    {
        if (server_.journal().error) server_.journal().error <<
            "listen on port '" << port.name << "' failed:" << ec.message();
        throw std::exception();
        return;
    }

    if (server_.journal().info) server_.journal().info <<
        "opened " << port;
}

door::~door()
{
    {
        // block until all detector, peer objects destroyed
        std::unique_lock<std::mutex> lock(mutex_);
        while (! list_.empty())
            cond_.wait(lock);
    }
    server_.remove (*this);
}

void
door::run()
{
    boost::asio::spawn (strand_, std::bind (&door::do_accept,
        shared_from_this(), std::placeholders::_1));
}

void
door::close()
{
    if (! strand_.running_in_this_thread())
        return strand_.post(std::bind(
            &door::close, shared_from_this()));
    error_code ec;
    acceptor_.close(ec);
    // close all detector, peer objects
    std::lock_guard<std::mutex> lock(mutex_);
    for(auto& _ : list_)
    {
        auto const peer = _.second.lock();
        if (peer != nullptr)
            peer->close();
    }
}

void
door::remove (child& c)
{
    std::lock_guard<std::mutex> lock(mutex_);
    list_.erase(&c);
    if (list_.empty())
        cond_.notify_all();
}

//------------------------------------------------------------------------------

void
door::add (std::shared_ptr<child> const& child)
{
    std::lock_guard<std::mutex> lock(mutex_);
    list_.emplace(child.get(), child);
}

template <class constbuffersequence>
void
door::create (bool ssl, constbuffersequence const& buffers,
    socket_type&& socket, endpoint_type remote_address)
{
    if (server_.closed())
        return;

    if (ssl)
    {
        auto const peer = std::make_shared <sslpeer> (*this,
            server_.journal(), remote_address, buffers,
                std::move(socket));
        add(peer);
        return peer->run();
    }

    auto const peer = std::make_shared <plainpeer> (*this,
        server_.journal(), remote_address, buffers,
            std::move(socket));
    add(peer);
    peer->run();
}

void
door::do_accept (boost::asio::yield_context yield)
{
    for(;;)
    {
        error_code ec;
        endpoint_type remote_address;
        socket_type socket (acceptor_.get_io_service());
        acceptor_.async_accept (socket, remote_address, yield[ec]);
        if (ec && ec != boost::asio::error::operation_aborted)
            if (server_.journal().error) server_.journal().error <<
                "accept: " << ec.message();
        if (ec == boost::asio::error::operation_aborted || server_.closed())
            break;
        if (ec)
            continue;

        if (ssl_ && plain_)
        {
            auto const c = std::make_shared <detector> (
                *this, std::move(socket), remote_address);
            add(c);
            c->run();
        }
        else if (ssl_ || plain_)
        {
            create(ssl_, boost::asio::null_buffers{},
                std::move(socket), remote_address);
        }
    }
}

}
}
