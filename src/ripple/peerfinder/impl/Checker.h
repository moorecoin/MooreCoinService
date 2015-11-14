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

#ifndef ripple_peerfinder_checker_h_included
#define ripple_peerfinder_checker_h_included

#include <beast/asio/ipaddressconversion.h>
#include <beast/asio/placeholders.h>
#include <boost/asio/detail/handler_invoke_helpers.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/system/error_code.hpp>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <utility>

namespace ripple {
namespace peerfinder {

/** tests remote listening sockets to make sure they are connectible. */
    template <class protocol = boost::asio::ip::tcp>
class checker
{
private:
    using error_code = boost::system::error_code;

    struct basic_async_op : boost::intrusive::list_base_hook <
        boost::intrusive::link_mode <boost::intrusive::normal_link>>
    {
        virtual
        ~basic_async_op() = default;

        virtual
        void
        stop() = 0;

        virtual
        void
        operator() (error_code const& ec) = 0;
    };

    template <class handler>
    struct async_op : basic_async_op
    {
        using socket_type = typename protocol::socket;
        using endpoint_type = typename protocol::endpoint;

        checker& checker_;
        socket_type socket_;
        handler handler_;

        async_op (checker& owner, boost::asio::io_service& io_service,
            handler&& handler);

        ~async_op();

        void
        stop() override;

        void
        operator() (error_code const& ec) override;
    };

    //--------------------------------------------------------------------------

    using list_type = typename boost::intrusive::make_list <basic_async_op,
        boost::intrusive::constant_time_size <true>>::type;

    std::mutex mutex_;
    std::condition_variable cond_;
    boost::asio::io_service& io_service_;
    list_type list_;
    bool stop_ = false;

public:
    explicit
    checker (boost::asio::io_service& io_service);

    /** destroy the service.
        any pending i/o operations will be canceled. this call blocks until
        all pending operations complete (either with success or with
        operation_aborted) and the associated thread and io_service have
        no more work remaining.
    */
    ~checker();

    /** stop the service.
        pending i/o operations will be canceled.
        this issues cancel orders for all pending i/o operations and then
        returns immediately. handlers will receive operation_aborted errors,
        or if they were already queued they will complete normally.
    */
    void
    stop();

    /** block until all pending i/o completes. */
    void
    wait();

    /** performs an async connection test on the specified endpoint.
        the port must be non-zero. note that the execution guarantees
        offered by asio handlers are not enforced.
    */
    template <class handler>
    void
    async_connect (beast::ip::endpoint const& endpoint, handler&& handler);

private:
    void
    remove (basic_async_op& op);
};

//------------------------------------------------------------------------------

template <class protocol>
template <class handler>
checker<protocol>::async_op<handler>::async_op (checker& owner,
        boost::asio::io_service& io_service, handler&& handler)
    : checker_ (owner)
    , socket_ (io_service)
    , handler_ (std::forward<handler>(handler))
{
}

template <class protocol>
template <class handler>
checker<protocol>::async_op<handler>::~async_op()
{
    checker_.remove (*this);
}

template <class protocol>
template <class handler>
void
checker<protocol>::async_op<handler>::stop()
{
    error_code ec;
    socket_.cancel(ec);
}

template <class protocol>
template <class handler>
void
checker<protocol>::async_op<handler>::operator() (
    error_code const& ec)
{
    handler_(ec);
}

//------------------------------------------------------------------------------

template <class protocol>
checker<protocol>::checker (boost::asio::io_service& io_service)
    : io_service_(io_service)
{
}

template <class protocol>
checker<protocol>::~checker()
{
    wait();
}

template <class protocol>
void
checker<protocol>::stop()
{
    std::lock_guard<std::mutex> lock (mutex_);
    if (! stop_)
    {
        stop_ = true;
        for (auto& c : list_)
            c.stop();
    }
}

template <class protocol>
void
checker<protocol>::wait()
{
    std::unique_lock<std::mutex> lock (mutex_);
    while (! list_.empty())
        cond_.wait (lock);
}

template <class protocol>
template <class handler>
void
checker<protocol>::async_connect (
    beast::ip::endpoint const& endpoint, handler&& handler)
{
    auto const op = std::make_shared<async_op<handler>> (
        *this, io_service_, std::forward<handler>(handler));
    {
        std::lock_guard<std::mutex> lock (mutex_);
        list_.push_back (*op);
    }
    op->socket_.async_connect (beast::ipaddressconversion::to_asio_endpoint (
        endpoint), std::bind (&basic_async_op::operator(), op,
            beast::asio::placeholders::error));
}

template <class protocol>
void
checker<protocol>::remove (basic_async_op& op)
{
    std::lock_guard <std::mutex> lock (mutex_);
    list_.erase (list_.iterator_to (op));
    if (list_.size() == 0)
        cond_.notify_all();
}

}
}

#endif
