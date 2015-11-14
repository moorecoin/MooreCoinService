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

#ifndef ripple_server_peer_h_included
#define ripple_server_peer_h_included

#include <ripple/server/impl/door.h>
#include <ripple/server/session.h>
#include <ripple/server/impl/serverimpl.h>
#include <beast/asio/ipaddressconversion.h>
#include <beast/asio/placeholders.h>
#include <beast/asio/ssl.h> // for is_short_read?
#include <beast/http/message.h>
#include <beast/http/parser.h>
#include <beast/module/core/time/time.h>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/spawn.hpp>
#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <beast/cxx14/type_traits.h> // <type_traits>

namespace ripple {
namespace http {

/** represents an active connection. */
template <class impl>
class peer
    : public door::child
    , public session
{
protected:
    using clock_type = std::chrono::system_clock;
    using error_code = boost::system::error_code;
    using endpoint_type = boost::asio::ip::tcp::endpoint;
    using waitable_timer = boost::asio::basic_waitable_timer <clock_type>;
    using yield_context = boost::asio::yield_context;

    enum
    {
        // size of our read/write buffer
        buffersize = 4 * 1024,

        // max seconds without completing a message
        timeoutseconds = 30

    };

    struct buffer
    {
        buffer (void const* ptr, std::size_t len)
            : data (new char[len])
            , bytes (len)
            , used (0)
        {
            memcpy (data.get(), ptr, len);
        }

        std::unique_ptr <char[]> data;
        std::size_t bytes;
        std::size_t used;
    };

    boost::asio::io_service::work work_;
    boost::asio::io_service::strand strand_;
    waitable_timer timer_;
    endpoint_type remote_address_;
    beast::journal journal_;

    std::string id_;
    std::size_t nid_;

    boost::asio::streambuf read_buf_;
    beast::http::message message_;
    beast::http::body body_;
    std::list <buffer> write_queue_;
    std::mutex mutex_;
    bool graceful_ = false;
    bool complete_ = false;
    boost::system::error_code ec_;

    clock_type::time_point when_;
    std::string when_str_;
    int request_count_ = 0;
    std::size_t bytes_in_ = 0;
    std::size_t bytes_out_ = 0;

    //--------------------------------------------------------------------------

public:
    template <class constbuffersequence>
    peer (door& door, boost::asio::io_service& io_service,
        beast::journal journal, endpoint_type remote_address,
            constbuffersequence const& buffers);

    virtual
    ~peer();

    session&
    session()
    {
        return *this;
    }

    void close() override;

protected:
    impl&
    impl()
    {
        return *static_cast<impl*>(this);
    }

    void
    fail (error_code ec, char const* what);

    void
    start_timer();

    void
    cancel_timer();

    void
    on_timer (error_code ec);

    void
    do_read (yield_context yield);

    void
    do_write (yield_context yield);

    void
    do_writer (std::shared_ptr <writer> const& writer,
        bool keep_alive, yield_context yield);

    virtual
    void
    do_request() = 0;

    virtual
    void
    do_close() = 0;

    // session

    beast::journal
    journal() override
    {
        return door_.server().journal();
    }

    port const&
    port() override
    {
        return door_.port();
    }

    beast::ip::endpoint
    remoteaddress() override
    {
        return beast::ipaddressconversion::from_asio(remote_address_);
    }

    beast::http::message&
    request() override
    {
        return message_;
    }

    beast::http::body const&
    body() override
    {
        return body_;
    }

    void
    write (void const* buffer, std::size_t bytes) override;

    void
    write (std::shared_ptr <writer> const& writer,
        bool keep_alive) override;

    std::shared_ptr<session>
    detach() override;

    void
    complete() override;

    void
    close (bool graceful) override;
};

//------------------------------------------------------------------------------

template <class impl>
template <class constbuffersequence>
peer<impl>::peer (door& door, boost::asio::io_service& io_service,
    beast::journal journal, endpoint_type remote_address,
        constbuffersequence const& buffers)
    : child(door)
    , work_ (io_service)
    , strand_ (io_service)
    , timer_ (io_service)
    , remote_address_ (remote_address)
    , journal_ (journal)
{
    read_buf_.commit(boost::asio::buffer_copy(read_buf_.prepare (
        boost::asio::buffer_size (buffers)), buffers));
    static std::atomic <int> sid;
    nid_ = ++sid;
    id_ = std::string("#") + std::to_string(nid_) + " ";
    if (journal_.trace) journal_.trace << id_ <<
        "accept:    " << remote_address_.address();
    when_ = clock_type::now();
    when_str_ = beast::time::getcurrenttime().formatted (
        "%y-%b-%d %h:%m:%s").tostdstring();
}

template <class impl>
peer<impl>::~peer()
{
    stat stat;
    stat.id = nid_;
    stat.when = std::move (when_str_);
    stat.elapsed = std::chrono::duration_cast <
        std::chrono::seconds> (clock_type::now() - when_);
    stat.requests = request_count_;
    stat.bytes_in = bytes_in_;
    stat.bytes_out = bytes_out_;
    stat.ec = std::move (ec_);
    door_.server().report (std::move (stat));
    door_.server().handler().onclose (session(), ec_);
    if (journal_.trace) journal_.trace << id_ <<
        "destroyed: " << request_count_ <<
            ((request_count_ == 1) ? " request" : " requests");
}

template <class impl>
void
peer<impl>::close()
{
    if (! strand_.running_in_this_thread())
        return strand_.post(std::bind(
            (void(peer::*)(void))&peer::close,
                impl().shared_from_this()));
    error_code ec;
    impl().stream_.lowest_layer().close(ec);
}

//------------------------------------------------------------------------------

template <class impl>
void
peer<impl>::fail (error_code ec, char const* what)
{
    if (! ec_ && ec != boost::asio::error::operation_aborted)
    {
        ec_ = ec;
        if (journal_.trace) journal_.trace << id_ <<
            std::string(what) << ": " << ec.message();
        impl().stream_.lowest_layer().close (ec);
    }
}

template <class impl>
void
peer<impl>::start_timer()
{
    error_code ec;
    timer_.expires_from_now (std::chrono::seconds(timeoutseconds), ec);
    if (ec)
        return fail (ec, "start_timer");
    timer_.async_wait (strand_.wrap (std::bind (
        &peer<impl>::on_timer, impl().shared_from_this(),
            beast::asio::placeholders::error)));
}

// convenience for discarding the error code
template <class impl>
void
peer<impl>::cancel_timer()
{
    error_code ec;
    timer_.cancel(ec);
}

// called when session times out
template <class impl>
void
peer<impl>::on_timer (error_code ec)
{
    if (ec == boost::asio::error::operation_aborted)
        return;
    if (! ec)
        ec = boost::system::errc::make_error_code (
            boost::system::errc::timed_out);
    fail (ec, "timer");
}

//------------------------------------------------------------------------------

template <class impl>
void
peer<impl>::do_read (yield_context yield)
{
    complete_ = false;

    error_code ec;
    bool eof = false;
    body_.clear();
    beast::http::parser parser (message_, body_, true);
    for(;;)
    {
        if (read_buf_.size() == 0)
        {
            start_timer();
            auto const bytes_transferred = boost::asio::async_read (
                impl().stream_, read_buf_.prepare (buffersize),
                    boost::asio::transfer_at_least(1), yield[ec]);
            cancel_timer();

            eof = ec == boost::asio::error::eof;
            if (eof)
            {
                ec = error_code{};
            }
            else if (! ec)
            {
                bytes_in_ += bytes_transferred;
                read_buf_.commit (bytes_transferred);
            }
        }

        if (! ec)
        {
            if (! eof)
            {
                // vfalco todo currently parsing errors are treated the
                //             same as the connection dropping. instead, we
                // should request that the handler compose a proper http error
                // response. this requires refactoring httpreply() into
                // something sensible.
                std::size_t used;
                std::tie (ec, used) = parser.write (read_buf_.data());
                if (! ec)
                    read_buf_.consume (used);
            }
            else
            {
                ec = parser.write_eof();
            }
        }

        if (! ec)
        {
            if (parser.complete())
                return do_request();
            else if (eof)
                ec = boost::asio::error::eof; // incomplete request
        }

        if (ec)
            return fail (ec, "read");
    }
}

// send everything in the write queue.
// the write queue must not be empty upon entry.
template <class impl>
void
peer<impl>::do_write (yield_context yield)
{
    error_code ec;
    std::size_t bytes = 0;
    for(;;)
    {
        bytes_out_ += bytes;
        void const* data;
        {
            std::lock_guard <std::mutex> lock (mutex_);
            assert(! write_queue_.empty());
            buffer& b1 = write_queue_.front();
            b1.used += bytes;
            if (b1.used >= b1.bytes)
            {
                write_queue_.pop_front();
                if (write_queue_.empty())
                    break;
                buffer& b2 = write_queue_.front();
                data = b2.data.get();
                bytes = b2.bytes;
            }
            else
            {
                data = b1.data.get() + b1.used;
                bytes = b1.bytes - b1.used;
            }
        }

        start_timer();
        bytes = boost::asio::async_write (impl().stream_,
            boost::asio::buffer (data, bytes),
                boost::asio::transfer_at_least(1), yield[ec]);
        cancel_timer();
        if (ec)
            return fail (ec, "write");
    }

    if (! complete_)
        return;

    if (graceful_)
        return do_close();

    boost::asio::spawn (strand_, std::bind (&peer<impl>::do_read,
        impl().shared_from_this(), std::placeholders::_1));
}

template <class impl>
void
peer<impl>::do_writer (std::shared_ptr <writer> const& writer,
    bool keep_alive, yield_context yield)
{
    std::function <void(void)> resume;
    {
        auto const p = impl().shared_from_this();
        resume = std::function <void(void)>(
            [this, p, writer, keep_alive]()
            {
                boost::asio::spawn (strand_, std::bind (
                    &peer<impl>::do_writer, p, writer, keep_alive,
                        std::placeholders::_1));
            });
    }

    for(;;)
    {
        if (! writer->prepare (buffersize, resume))
            return;
        error_code ec;
        auto const bytes_transferred = boost::asio::async_write (
            impl().stream_, writer->data(), boost::asio::transfer_at_least(1),
                yield[ec]);
        if (ec)
            return fail (ec, "writer");
        writer->consume(bytes_transferred);
        if (writer->complete())
            break;
    }

    if (! keep_alive)
        return do_close();

    boost::asio::spawn (strand_, std::bind (&peer<impl>::do_read,
        impl().shared_from_this(), std::placeholders::_1));
}

//------------------------------------------------------------------------------

// send a copy of the data.
template <class impl>
void
peer<impl>::write (void const* buffer, std::size_t bytes)
{
    if (bytes == 0)
        return;

    bool empty;
    {
        std::lock_guard <std::mutex> lock (mutex_);
        empty = write_queue_.empty();
        write_queue_.emplace_back (buffer, bytes);
    }

    if (empty)
        boost::asio::spawn (strand_, std::bind (&peer<impl>::do_write,
            impl().shared_from_this(), std::placeholders::_1));
}

template <class impl>
void
peer<impl>::write (std::shared_ptr <writer> const& writer,
    bool keep_alive)
{
    boost::asio::spawn (strand_, std::bind (
        &peer<impl>::do_writer, impl().shared_from_this(),
            writer, keep_alive, std::placeholders::_1));
}

// deprecated
// make the session asynchronous
template <class impl>
std::shared_ptr<session>
peer<impl>::detach()
{
    return impl().shared_from_this();
}

// deprecated
// called to indicate the response has been written (but not sent)
template <class impl>
void
peer<impl>::complete()
{
    if (! strand_.running_in_this_thread())
        return strand_.post(std::bind (&peer<impl>::complete,
            impl().shared_from_this()));

    message_ = beast::http::message{};
    complete_ = true;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (! write_queue_.empty())
            return;
    }

    // keep-alive
    boost::asio::spawn (strand_, std::bind (&peer<impl>::do_read,
        impl().shared_from_this(), std::placeholders::_1));
}

// deprecated
// called from the handler to close the session.
template <class impl>
void
peer<impl>::close (bool graceful)
{
    if (! strand_.running_in_this_thread())
        return strand_.post(std::bind(
            (void(peer::*)(bool))&peer<impl>::close,
                impl().shared_from_this(), graceful));

    complete_ = true;
    if (graceful)
    {
        graceful_ = true;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (! write_queue_.empty())
                return;
        }
        return do_close();
    }

    error_code ec;
    impl().stream_.lowest_layer().close (ec);
}

}
}

#endif
