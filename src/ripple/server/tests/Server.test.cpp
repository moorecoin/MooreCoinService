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
#include <ripple/basics/make_sslcontext.h>
#include <ripple/server/server.h>
#include <ripple/server/session.h>
#include <beast/unit_test/suite.h>
#include <boost/asio/ip/tcp.hpp>
#include <boost/optional.hpp>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace ripple {
namespace http {

class server_test : public beast::unit_test::suite
{
public:
    enum
    {
        testport = 1001
    };

    class testthread
    {
    private:
        boost::asio::io_service io_service_;
        boost::optional<boost::asio::io_service::work> work_;
        std::thread thread_;

    public:
        testthread()
            : work_(boost::in_place(std::ref(io_service_)))
            , thread_([&]() { this->io_service_.run(); })
        {
        }

        ~testthread()
        {
            work_ = boost::none;
            thread_.join();
        }

        boost::asio::io_service&
        get_io_service()
        {
            return io_service_;
        }
    };

    //--------------------------------------------------------------------------

    class testsink : public beast::journal::sink
    {
        beast::unit_test::suite& suite_;

    public:
        testsink (beast::unit_test::suite& suite)
            : suite_ (suite)
        {
        }

        void
        write (beast::journal::severity level,
            std::string const& text) override
        {
            suite_.log << text;
        }
    };

    //--------------------------------------------------------------------------

    struct testhandler : handler
    {
        void
        onaccept (session& session) override
        {
        }

        bool
        onaccept (session& session,
            boost::asio::ip::tcp::endpoint endpoint) override
        {
            return true;
        }

        void
        onlegacypeerhello (std::unique_ptr<beast::asio::ssl_bundle>&& ssl_bundle,
            boost::asio::const_buffer buffer,
                boost::asio::ip::tcp::endpoint remote_address) override
        {
        }

        handoff
        onhandoff (session& session,
            std::unique_ptr <beast::asio::ssl_bundle>&& bundle,
                beast::http::message&& request,
                    boost::asio::ip::tcp::endpoint remote_address) override
        {
            return handoff{};
        }

        handoff
        onhandoff (session& session, boost::asio::ip::tcp::socket&& socket,
            beast::http::message&& request,
                boost::asio::ip::tcp::endpoint remote_address) override
        {
            return handoff{};
        }

        void
        onrequest (session& session) override
        {
            session.write (std::string ("hello, world!\n"));
            if (session.request().keep_alive())
                session.complete();
            else
                session.close (true);
        }

        void
        onclose (session& session,
            boost::system::error_code const&) override
        {
        }

        void
        onstopped (server& server)
        {
        }
    };

    //--------------------------------------------------------------------------

    // connect to an address
    template <class socket>
    bool
    connect (socket& s, std::string const& addr, int port)
    {
        try
        {
            typename socket::endpoint_type ep (
                boost::asio::ip::address::from_string (addr), port);
            s.connect (ep);
            pass();
            return true;
        }
        catch (std::exception const& e)
        {
            fail (e.what());
        }

        return false;
    }

    // write a string to the stream
    template <class syncwritestream>
    bool
    write (syncwritestream& s, std::string const& text)
    {
        try
        {
            boost::asio::write (s, boost::asio::buffer (text));
            pass();
            return true;
        }
        catch (std::exception const& e)
        {
            fail (e.what());
        }
        return false;
    }

    // expect that reading the stream produces a matching string
    template <class syncreadstream>
    bool
    expect_read (syncreadstream& s, std::string const& match)
    {
        boost::asio::streambuf b (1000); // limit on read
        try
        {
            auto const n = boost::asio::read_until (s, b, '\n');
            if (expect (n == match.size()))
            {
                std::string got;
                got.resize (n);
                boost::asio::buffer_copy (boost::asio::buffer (
                    &got[0], n), b.data());
                return expect (got == match);
            }
        }
        catch (std::length_error const& e)
        {
            fail(e.what());
        }
        catch (std::exception const& e)
        {
            fail(e.what());
        }
        return false;
    }

    void
    test_request()
    {
        boost::asio::io_service ios;
        typedef boost::asio::ip::tcp::socket socket;
        socket s (ios);

        if (! connect (s, "127.0.0.1", testport))
            return;

        if (! write (s,
            "get / http/1.1\r\n"
            "connection: close\r\n"
            "\r\n"))
            return;

        if (! expect_read (s, "hello, world!\n"))
            return ;

        try
        {
            s.shutdown (socket::shutdown_both);
            pass();
        }
        catch (std::exception const& e)
        {
            fail (e.what());
        }

        std::this_thread::sleep_for (std::chrono::seconds (1));
    }

    void
    test_keepalive()
    {
        boost::asio::io_service ios;
        typedef boost::asio::ip::tcp::socket socket;
        socket s (ios);

        if (! connect (s, "127.0.0.1", testport))
            return;

        if (! write (s,
            "get / http/1.1\r\n"
            "connection: keep-alive\r\n"
            "\r\n"))
            return;

        if (! expect_read (s, "hello, world!\n"))
            return ;

        if (! write (s,
            "get / http/1.1\r\n"
            "connection: close\r\n"
            "\r\n"))
            return;

        if (! expect_read (s, "hello, world!\n"))
            return ;

        try
        {
            s.shutdown (socket::shutdown_both);
            pass();
        }
        catch (std::exception const& e)
        {
            fail (e.what());
        }
    }

    void
    run()
    {
        testsink sink {*this};
        testthread thread;
        sink.severity (beast::journal::severity::kall);
        beast::journal journal {sink};
        testhandler handler;
        auto s = make_server (handler,
            thread.get_io_service(), journal);
        std::vector<port> list;
        list.resize(1);
        list.back().port = testport;
        list.back().ip = boost::asio::ip::address::from_string (
            "127.0.0.1");
        list.back().protocol.insert("http");
        s->ports (list);

        test_request();
        //test_keepalive();
        //s->close();
        s = nullptr;

        pass();
    }
};

beast_define_testsuite_manual(server,http,ripple);

}
}
