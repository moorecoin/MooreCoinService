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

#ifndef ripple_server_door_h_included
#define ripple_server_door_h_included

#include <ripple/server/impl/serverimpl.h>
#include <beast/asio/streambuf.h>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/container/flat_map.hpp>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace ripple {
namespace http {

/** a listening socket. */
class door
    : public serverimpl::child
    , public std::enable_shared_from_this <door>
{
public:
    class child
    {
    protected:
        door& door_;

    public:
        child (door& door);
        virtual ~child();
        virtual void close() = 0;
    };

private:
    using clock_type = std::chrono::steady_clock;
    using timer_type = boost::asio::basic_waitable_timer<clock_type>;
    using error_code = boost::system::error_code;
    using yield_context = boost::asio::yield_context;
    using protocol_type = boost::asio::ip::tcp;
    using acceptor_type = protocol_type::acceptor;
    using endpoint_type = protocol_type::endpoint;
    using socket_type = protocol_type::socket;

    // detects ssl on a socket
    class detector
        : public child
        , public std::enable_shared_from_this <detector>
    {
    private:
        socket_type socket_;
        timer_type timer_;
        endpoint_type remote_address_;

    public:
        detector (door& door, socket_type&& socket,
            endpoint_type remote_address);
        void run();
        void close() override;

    private:
        void do_timer (yield_context yield);
        void do_detect (yield_context yield);
    };

    std::shared_ptr<port> port_;
    serverimpl& server_;
    acceptor_type acceptor_;
    boost::asio::io_service::strand strand_;
    std::mutex mutex_;
    std::condition_variable cond_;
    boost::container::flat_map<
        child*, std::weak_ptr<child>> list_;
    bool ssl_;
    bool plain_;

public:
    door (boost::asio::io_service& io_service,
        serverimpl& server, port const& port);

    /** destroy the door.
        blocks until there are no pending i/o completion
        handlers, and all connections have been destroyed.
        close() must be called before the destructor.
    */
    ~door();

    serverimpl&
    server()
    {
        return server_;
    }

    port const&
    port() const
    {
        return *port_;
    }

    // work-around because we can't call shared_from_this in ctor
    void run();

    /** close the door listening socket and connections.
        the listening socket is closed, and all open connections
        belonging to the door are closed.
        thread safety:
            may be called concurrently
    */
    void close();

    void remove (child& c);

private:
    void add (std::shared_ptr<child> const& child);

    template <class constbuffersequence>
    void create (bool ssl, constbuffersequence const& buffers,
        socket_type&& socket, endpoint_type remote_address);

    void do_accept (yield_context yield);
};

}
}

#endif
