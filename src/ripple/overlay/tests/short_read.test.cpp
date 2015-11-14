//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright 2014 ripple labs inc.

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
#include <ripple/basics/make_sslcontext.h>
#include <beast/asio/placeholders.h>
#include <beast/unit_test/suite.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/optional.hpp>
#include <beast/threads/thread.h>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

namespace ripple {
/*

findings from the test:

if the remote host calls async_shutdown then the local host's
async_read will complete with eof.

if both hosts call async_shutdown then the calls to async_shutdown
will complete with eof.

*/

class short_read_test : public beast::unit_test::suite
{
private:
    using io_service_type = boost::asio::io_service;
    using strand_type = io_service_type::strand;
    using timer_type = boost::asio::basic_waitable_timer<
        std::chrono::steady_clock>;
    using acceptor_type = boost::asio::ip::tcp::acceptor;
    using socket_type = boost::asio::ip::tcp::socket;
    using stream_type = boost::asio::ssl::stream<socket_type&>;
    using error_code = boost::system::error_code;
    using endpoint_type = boost::asio::ip::tcp::endpoint;
    using address_type = boost::asio::ip::address;

    io_service_type io_service_;
    boost::optional<io_service_type::work> work_;
    std::thread thread_;
    std::shared_ptr<boost::asio::ssl::context> context_;

    static
    endpoint_type
    endpoint()
    {
        return endpoint_type(address_type::from_string("127.0.0.1"), 9000);
    }

    template <class streambuf>
    static
    void
    write(streambuf& sb, std::string const& s)
    {
        using boost::asio::buffer;
        using boost::asio::buffer_copy;
        using boost::asio::buffer_size;
        boost::asio::const_buffers_1 buf(s.data(), s.size());
        sb.commit(buffer_copy(sb.prepare(buffer_size(buf)), buf));
    }

    //--------------------------------------------------------------------------

    class base
    {
    protected:
        class child
        {
        private:
            base& base_;

        public:
            child(base& base)
                : base_(base)
            {
            }

            virtual ~child()
            {
                base_.remove(this);
            }

            virtual void close() = 0;
        };

    private:
        std::mutex mutex_;
        std::condition_variable cond_;
        std::map<child*, std::weak_ptr<child>> list_;
        bool closed_ = false;

    public:
        ~base()
        {
            // derived class must call wait() in the destructor
            assert(list_.empty());
        }

        void
        add (std::shared_ptr<child> const& child)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            list_.emplace(child.get(), child);
        }

        void
        remove (child* child)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            list_.erase(child);
            if (list_.empty())
                cond_.notify_one();
        }

        void
        close()
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (closed_)
                return;
            closed_ = true;
            for(auto& c : list_)
                if(auto p = c.second.lock())
                    p->close();
        }

        void
        wait()
        {
            std::unique_lock<std::mutex> lock(mutex_);
            while(! list_.empty())
                cond_.wait(lock);
        }
    };

    //--------------------------------------------------------------------------

    class server : public base
    {
    private:
        short_read_test& test_;

        struct acceptor
            : child, std::enable_shared_from_this<acceptor>
        {
            server& server_;
            short_read_test& test_;
            acceptor_type acceptor_;
            socket_type socket_;
            strand_type strand_;

            acceptor(server& server)
                : child(server)
                , server_(server)
                , test_(server_.test_)
                , acceptor_(test_.io_service_, endpoint())
                , socket_(test_.io_service_)
                , strand_(socket_.get_io_service())
            {
                acceptor_.listen();
            }

            void
            close() override
            {
                if(! strand_.running_in_this_thread())
                    return strand_.post(std::bind(&acceptor::close,
                        shared_from_this()));
                acceptor_.close();
            }

            void
            run()
            {
                acceptor_.async_accept(socket_, strand_.wrap(std::bind(
                    &acceptor::on_accept, shared_from_this(),
                        beast::asio::placeholders::error)));
            }

            void
            fail (std::string const& what, error_code ec)
            {
                if (acceptor_.is_open())
                {
                    if (ec != boost::asio::error::operation_aborted)
                        test_.log << what << ": " << ec.message();
                    acceptor_.close();
                }
            }

            void
            on_accept(error_code ec)
            {
                if (ec)
                    return fail ("accept", ec);
                auto const p = std::make_shared<connection>(
                    server_, std::move(socket_));
                server_.add(p);
                p->run();
                acceptor_.async_accept(socket_, strand_.wrap(std::bind(
                    &acceptor::on_accept, shared_from_this(),
                        beast::asio::placeholders::error)));
            }
        };

        struct connection
            : child, std::enable_shared_from_this<connection>
        {
            server& server_;
            short_read_test& test_;
            socket_type socket_;
            stream_type stream_;
            strand_type strand_;
            timer_type timer_;
            boost::asio::streambuf buf_;

            connection (server& server, socket_type&& socket)
                : child(server)
                , server_(server)
                , test_(server_.test_)
                , socket_(std::move(socket))
                , stream_(socket_, *test_.context_)
                , strand_(socket_.get_io_service())
                , timer_(socket_.get_io_service())
            {
            }

            void
            close() override
            {
                if(! strand_.running_in_this_thread())
                    return strand_.post(std::bind(&connection::close,
                        shared_from_this()));
                if (socket_.is_open())
                {
                    socket_.close();
                    timer_.cancel();
                }
            }

            void
            run()
            {
                timer_.expires_from_now(std::chrono::seconds(3));
                timer_.async_wait(strand_.wrap(std::bind(&connection::on_timer,
                    shared_from_this(), beast::asio::placeholders::error)));
                stream_.async_handshake(stream_type::server, strand_.wrap(
                    std::bind(&connection::on_handshake, shared_from_this(),
                        beast::asio::placeholders::error)));
            }

            void
            fail (std::string const& what, error_code ec)
            {
                if (socket_.is_open())
                {
                    if (ec != boost::asio::error::operation_aborted)
                        test_.log << "[server] " << what << ": " << ec.message();
                    socket_.close();
                    timer_.cancel();
                }
            }

            void
            on_timer(error_code ec)
            {
                if (ec == boost::asio::error::operation_aborted)
                    return;
                if (ec)
                    return fail("timer", ec);
                test_.log << "[server] timeout";
                socket_.close();
            }

            void
            on_handshake(error_code ec)
            {
                if (ec)
                    return fail("handshake", ec);
#if 1
                boost::asio::async_read_until(stream_, buf_, "\n", strand_.wrap(
                    std::bind(&connection::on_read, shared_from_this(),
                        beast::asio::placeholders::error,
                            beast::asio::placeholders::bytes_transferred)));
#else
                close();
#endif
            }

            void
            on_read(error_code ec, std::size_t bytes_transferred)
            {
                if (ec == boost::asio::error::eof)
                {
                    server_.test_.log << "[server] read: eof";
                    return stream_.async_shutdown(strand_.wrap(std::bind(
                        &connection::on_shutdown, shared_from_this(),
                            beast::asio::placeholders::error)));
                }
                if (ec)
                    return fail("read", ec);

                buf_.commit(bytes_transferred);
                buf_.consume(bytes_transferred);
                write(buf_, "bye\n");
                boost::asio::async_write(stream_, buf_.data(), strand_.wrap(
                    std::bind(&connection::on_write, shared_from_this(),
                        beast::asio::placeholders::error,
                            beast::asio::placeholders::bytes_transferred)));
            }

            void
            on_write(error_code ec, std::size_t bytes_transferred)
            {
                buf_.consume(bytes_transferred);
                if (ec)
                    return fail("write", ec);
                stream_.async_shutdown(strand_.wrap(std::bind(
                    &connection::on_shutdown, shared_from_this(),
                        beast::asio::placeholders::error)));
            }

            void
            on_shutdown(error_code ec)
            {
                if (ec)
                    return fail("shutdown", ec);
                socket_.close();
                timer_.cancel();
            }
        };

    public:
        server(short_read_test& test)
            : test_(test)
        {
            auto const p = std::make_shared<acceptor>(*this);
            add(p);
            p->run();
        }

        ~server()
        {
            close();
            wait();
        }
    };

    //--------------------------------------------------------------------------

    class client : public base
    {
    private:
        short_read_test& test_;

        struct connection
            : child, std::enable_shared_from_this<connection>
        {
            client& client_;
            short_read_test& test_;
            socket_type socket_;
            stream_type stream_;
            strand_type strand_;
            timer_type timer_;
            boost::asio::streambuf buf_;

            connection (client& client)
                : child(client)
                , client_(client)
                , test_(client_.test_)
                , socket_(test_.io_service_)
                , stream_(socket_, *test_.context_)
                , strand_(socket_.get_io_service())
                , timer_(socket_.get_io_service())
            {
            }

            void
            close() override
            {
                if(! strand_.running_in_this_thread())
                    return strand_.post(std::bind(&connection::close,
                        shared_from_this()));
                if (socket_.is_open())
                {
                    socket_.close();
                    timer_.cancel();
                }
            }

            void
            run()
            {
                timer_.expires_from_now(std::chrono::seconds(3));
                timer_.async_wait(strand_.wrap(std::bind(&connection::on_timer,
                    shared_from_this(), beast::asio::placeholders::error)));
                socket_.async_connect(endpoint(), strand_.wrap(std::bind(
                    &connection::on_connect, shared_from_this(),
                        beast::asio::placeholders::error)));
            }

            void
            fail (std::string const& what, error_code ec)
            {
                if (socket_.is_open())
                {
                    if (ec != boost::asio::error::operation_aborted)
                        test_.log << "[client] " << what << ": " << ec.message();
                    socket_.close();
                    timer_.cancel();
                }
            }

            void
            on_timer(error_code ec)
            {
                if (ec == boost::asio::error::operation_aborted)
                    return;
                if (ec)
                    return fail("timer", ec);
                test_.log << "[client] timeout";
                socket_.close();
            }

            void
            on_connect(error_code ec)
            {
                if (ec)
                    return fail("connect", ec);
                stream_.async_handshake(stream_type::client, strand_.wrap(
                    std::bind(&connection::on_handshake, shared_from_this(),
                        beast::asio::placeholders::error)));
            }

            void
            on_handshake(error_code ec)
            {
                if (ec)
                    return fail("handshake", ec);
                write(buf_, "hello\n");

#if 1
                boost::asio::async_write(stream_, buf_.data(), strand_.wrap(
                    std::bind(&connection::on_write, shared_from_this(),
                        beast::asio::placeholders::error,
                            beast::asio::placeholders::bytes_transferred)));
#else
                stream_.async_shutdown(strand_.wrap(std::bind(
                    &connection::on_shutdown, shared_from_this(),
                        beast::asio::placeholders::error)));
#endif
            }

            void
            on_write(error_code ec, std::size_t bytes_transferred)
            {
                buf_.consume(bytes_transferred);
                if (ec)
                    return fail("write", ec);
#if 1
                boost::asio::async_read_until(stream_, buf_, "\n", strand_.wrap(
                    std::bind(&connection::on_read, shared_from_this(),
                        beast::asio::placeholders::error,
                            beast::asio::placeholders::bytes_transferred)));
#else
                stream_.async_shutdown(strand_.wrap(std::bind(
                    &connection::on_shutdown, shared_from_this(),
                        beast::asio::placeholders::error)));
#endif
            }

            void
            on_read(error_code ec, std::size_t bytes_transferred)
            {
                if (ec)
                    return fail("read", ec);
                buf_.commit(bytes_transferred);
                stream_.async_shutdown(strand_.wrap(std::bind(
                    &connection::on_shutdown, shared_from_this(),
                        beast::asio::placeholders::error)));
            }

            void
            on_shutdown(error_code ec)
            {

                if (ec)
                    return fail("shutdown", ec);
                socket_.close();
                timer_.cancel();
            }
        };

    public:
        client(short_read_test& test)
            : test_(test)
        {
            auto const p = std::make_shared<connection>(*this);
            add(p);
            p->run();
        }

        ~client()
        {
            close();
            wait();
        }
    };

public:
    short_read_test()
        : work_(boost::in_place(std::ref(io_service_)))
        , thread_(std::thread([this]()
            {
                beast::thread::setcurrentthreadname("io_service");
                this->io_service_.run();
            }))
        , context_(make_sslcontext())
    {
    }

    ~short_read_test()
    {
        work_ = boost::none;
        thread_.join();
    }

    void run() override
    {
        server s(*this);
        client c(*this);
        c.wait();
        pass();
    }
};

beast_define_testsuite_manual(short_read,overlay,ripple);

}
