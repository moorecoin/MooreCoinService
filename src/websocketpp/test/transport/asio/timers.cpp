/*
 * copyright (c) 2014, peter thorson. all rights reserved.
 *
 * redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * neither the name of the websocket++ project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * this software is provided by the copyright holders and contributors "as is"
 * and any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose
 * are disclaimed. in no event shall peter thorson be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential damages
 * (including, but not limited to, procurement of substitute goods or services;
 * loss of use, data, or profits; or business interruption) however caused and
 * on any theory of liability, whether in contract, strict liability, or tort
 * (including negligence or otherwise) arising in any way out of the use of this
 * software, even if advised of the possibility of such damage.
 *
 */
//#define boost_test_dyn_link
#define boost_test_module transport_asio_timers
#include <boost/test/unit_test.hpp>

#include <exception>
#include <iostream>

#include <websocketpp/common/thread.hpp>

#include <websocketpp/transport/asio/endpoint.hpp>
#include <websocketpp/transport/asio/security/tls.hpp>

// concurrency
#include <websocketpp/concurrency/none.hpp>

// http
#include <websocketpp/http/request.hpp>
#include <websocketpp/http/response.hpp>

// loggers
#include <websocketpp/logger/stub.hpp>
//#include <websocketpp/logger/basic.hpp>

#include <boost/asio.hpp>

// accept a connection, read data, and discard until eof
void run_dummy_server(int port) {
    using boost::asio::ip::tcp;

    try {
        boost::asio::io_service io_service;
        tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v6(), port));
        tcp::socket socket(io_service);

        acceptor.accept(socket);
        for (;;) {
            char data[512];
            boost::system::error_code ec;
            socket.read_some(boost::asio::buffer(data), ec);
            if (ec == boost::asio::error::eof) {
                break;
            } else if (ec) {
                // other error
                throw ec;
            }
        }
    } catch (std::exception & e) {
        std::cout << e.what() << std::endl;
    } catch (boost::system::error_code & ec) {
        std::cout << ec.message() << std::endl;
    }
}

// wait for the specified time period then fail the test
void run_test_timer(long value) {
    boost::asio::io_service ios;
    boost::asio::deadline_timer t(ios,boost::posix_time::milliseconds(value));
    boost::system::error_code ec;
    t.wait(ec);
    boost_fail( "test timed out" );
}

struct config {
    typedef websocketpp::concurrency::none concurrency_type;
    //typedef websocketpp::log::basic<concurrency_type,websocketpp::log::alevel> alog_type;
    typedef websocketpp::log::stub alog_type;
    typedef websocketpp::log::stub elog_type;
    typedef websocketpp::http::parser::request request_type;
    typedef websocketpp::http::parser::response response_type;
    typedef websocketpp::transport::asio::tls_socket::endpoint socket_type;

    static const bool enable_multithreading = true;

    static const long timeout_socket_pre_init = 1000;
    static const long timeout_proxy = 1000;
    static const long timeout_socket_post_init = 1000;
    static const long timeout_dns_resolve = 1000;
    static const long timeout_connect = 1000;
    static const long timeout_socket_shutdown = 1000;
};

// mock context that does no validation
typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
context_ptr on_tls_init(websocketpp::connection_hdl) {
    return context_ptr(new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1));
}

// mock connection
struct mock_con: public websocketpp::transport::asio::connection<config> {
    typedef websocketpp::transport::asio::connection<config> base;

    mock_con(bool a, config::alog_type& b, config::elog_type& c) : base(a,b,c) {}

    void start() {
        base::init(websocketpp::lib::bind(&mock_con::handle_start,this,
            websocketpp::lib::placeholders::_1));
    }

    void handle_start(const websocketpp::lib::error_code& ec) {
        using websocketpp::transport::asio::socket::make_error_code;
        using websocketpp::transport::asio::socket::error::tls_handshake_timeout;

        boost_check_equal( ec, make_error_code(tls_handshake_timeout) );

        base::cancel_socket();
    }
};

typedef websocketpp::transport::asio::connection<config> con_type;
typedef websocketpp::lib::shared_ptr<mock_con> connection_ptr;

struct mock_endpoint : public websocketpp::transport::asio::endpoint<config> {
    typedef websocketpp::transport::asio::endpoint<config> base;

    mock_endpoint() {
        alog.set_channels(websocketpp::log::alevel::all);
        base::init_logging(&alog,&elog);
        init_asio();
    }

    void connect(std::string u) {
        m_con.reset(new mock_con(false,alog,elog));
        websocketpp::uri_ptr uri(new websocketpp::uri(u));

        boost_check( uri->get_valid() );
        boost_check_equal( base::init(m_con), websocketpp::lib::error_code() );

        base::async_connect(
            m_con,
            uri,
            websocketpp::lib::bind(
                &mock_endpoint::handle_connect,
                this,
                m_con,
                websocketpp::lib::placeholders::_1
            )
        );
    }

    void handle_connect(connection_ptr con, websocketpp::lib::error_code const & ec)
    {
        boost_check( !ec );
        con->start();
    }

    connection_ptr m_con;
    config::alog_type alog;
    config::elog_type elog;
};

boost_auto_test_case( tls_handshake_timeout ) {
    websocketpp::lib::thread dummy_server(websocketpp::lib::bind(&run_dummy_server,9005));
    websocketpp::lib::thread timer(websocketpp::lib::bind(&run_test_timer,5000));
    dummy_server.detach();
    timer.detach();

    mock_endpoint endpoint;
    endpoint.set_tls_init_handler(&on_tls_init);
    endpoint.connect("wss://localhost:9005");
    endpoint.run();
}
