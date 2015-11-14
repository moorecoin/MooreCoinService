/*
 * copyright (c) 2011, peter thorson. all rights reserved.
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
#define boost_test_module transport_integration
#include <boost/test/unit_test.hpp>

#include <websocketpp/common/thread.hpp>

#include <websocketpp/config/core.hpp>
#include <websocketpp/config/core_client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/debug_asio.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

struct config : public websocketpp::config::asio_client {
    typedef config type;
    typedef websocketpp::config::asio base;

    typedef base::concurrency_type concurrency_type;

    typedef base::request_type request_type;
    typedef base::response_type response_type;

    typedef base::message_type message_type;
    typedef base::con_msg_manager_type con_msg_manager_type;
    typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef base::alog_type alog_type;
    typedef base::elog_type elog_type;

    typedef base::rng_type rng_type;

    struct transport_config : public base::transport_config {
        typedef type::concurrency_type concurrency_type;
        typedef type::alog_type alog_type;
        typedef type::elog_type elog_type;
        typedef type::request_type request_type;
        typedef type::response_type response_type;
        typedef websocketpp::transport::asio::basic_socket::endpoint
            socket_type;
    };

    typedef websocketpp::transport::asio::endpoint<transport_config>
        transport_type;

    //static const websocketpp::log::level elog_level = websocketpp::log::elevel::all;
    //static const websocketpp::log::level alog_level = websocketpp::log::alevel::all;

    /// length of time before an opening handshake is aborted
    static const long timeout_open_handshake = 500;
    /// length of time before a closing handshake is aborted
    static const long timeout_close_handshake = 500;
    /// length of time to wait for a pong after a ping
    static const long timeout_pong = 500;
};

struct config_tls : public websocketpp::config::asio_tls_client {
    typedef config type;
    typedef websocketpp::config::asio base;

    typedef base::concurrency_type concurrency_type;

    typedef base::request_type request_type;
    typedef base::response_type response_type;

    typedef base::message_type message_type;
    typedef base::con_msg_manager_type con_msg_manager_type;
    typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef base::alog_type alog_type;
    typedef base::elog_type elog_type;

    typedef base::rng_type rng_type;

    struct transport_config : public base::transport_config {
        typedef type::concurrency_type concurrency_type;
        typedef type::alog_type alog_type;
        typedef type::elog_type elog_type;
        typedef type::request_type request_type;
        typedef type::response_type response_type;
        typedef websocketpp::transport::asio::basic_socket::endpoint
            socket_type;
    };

    typedef websocketpp::transport::asio::endpoint<transport_config>
        transport_type;

    //static const websocketpp::log::level elog_level = websocketpp::log::elevel::all;
    //static const websocketpp::log::level alog_level = websocketpp::log::alevel::all;

    /// length of time before an opening handshake is aborted
    static const long timeout_open_handshake = 500;
    /// length of time before a closing handshake is aborted
    static const long timeout_close_handshake = 500;
    /// length of time to wait for a pong after a ping
    static const long timeout_pong = 500;
};

typedef websocketpp::server<config> server;
typedef websocketpp::client<config> client;

typedef websocketpp::server<config_tls> server_tls;
typedef websocketpp::client<config_tls> client_tls;

typedef websocketpp::server<websocketpp::config::core> iostream_server;
typedef websocketpp::client<websocketpp::config::core_client> iostream_client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

template <typename t>
void close_after_timeout(t & e, websocketpp::connection_hdl hdl, long timeout) {
    sleep(timeout);

    websocketpp::lib::error_code ec;
    e.close(hdl,websocketpp::close::status::normal,"",ec);
    boost_check(!ec);
}

void run_server(server * s, int port, bool log = false) {
    if (log) {
        s->set_access_channels(websocketpp::log::alevel::all);
        s->set_error_channels(websocketpp::log::elevel::all);
    } else {
        s->clear_access_channels(websocketpp::log::alevel::all);
        s->clear_error_channels(websocketpp::log::elevel::all);
    }

    s->init_asio();
    s->set_reuse_addr(true);

    s->listen(port);
    s->start_accept();
    s->run();
}

void run_client(client & c, std::string uri, bool log = false) {
    if (log) {
        c.set_access_channels(websocketpp::log::alevel::all);
        c.set_error_channels(websocketpp::log::elevel::all);
    } else {
        c.clear_access_channels(websocketpp::log::alevel::all);
        c.clear_error_channels(websocketpp::log::elevel::all);
    }
    websocketpp::lib::error_code ec;
    c.init_asio(ec);
    c.set_reuse_addr(true);
    boost_check(!ec);

    client::connection_ptr con = c.get_connection(uri,ec);
    boost_check( !ec );
    c.connect(con);

    c.run();
}

void run_client_and_mark(client * c, bool * flag, websocketpp::lib::mutex * mutex) {
    c->run();
    boost_check( true );
    websocketpp::lib::lock_guard<websocketpp::lib::mutex> lock(*mutex);
    *flag = true;
    boost_check( true );
}

void run_time_limited_client(client & c, std::string uri, long timeout,
    bool log)
{
    if (log) {
        c.set_access_channels(websocketpp::log::alevel::all);
        c.set_error_channels(websocketpp::log::elevel::all);
    } else {
        c.clear_access_channels(websocketpp::log::alevel::all);
        c.clear_error_channels(websocketpp::log::elevel::all);
    }
    c.init_asio();

    websocketpp::lib::error_code ec;
    client::connection_ptr con = c.get_connection(uri,ec);
    boost_check( !ec );
    c.connect(con);

    websocketpp::lib::thread tthread(websocketpp::lib::bind(
        &close_after_timeout<client>,
        websocketpp::lib::ref(c),
        con->get_handle(),
        timeout
    ));
    tthread.detach();

    c.run();
}

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

void run_dummy_client(std::string port) {
    using boost::asio::ip::tcp;

    try {
        boost::asio::io_service io_service;
        tcp::resolver resolver(io_service);
        tcp::resolver::query query("localhost", port);
        tcp::resolver::iterator iterator = resolver.resolve(query);
        tcp::socket socket(io_service);

        boost::asio::connect(socket, iterator);
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

bool on_ping(websocketpp::connection_hdl, std::string) {
    return false;
}

void cancel_on_open(server * s, websocketpp::connection_hdl) {
    s->stop_listening();
}

void stop_on_close(server * s, websocketpp::connection_hdl hdl) {
    server::connection_ptr con = s->get_con_from_hdl(hdl);
    //boost_check_equal( con->get_local_close_code(), websocketpp::close::status::normal );
    //boost_check_equal( con->get_remote_close_code(), websocketpp::close::status::normal );
    s->stop();
}

template <typename t>
void ping_on_open(t * c, std::string payload, websocketpp::connection_hdl hdl) {
    typename t::connection_ptr con = c->get_con_from_hdl(hdl);
    con->ping(payload);
}

void fail_on_pong(websocketpp::connection_hdl, std::string) {
    boost_fail( "expected no pong handler" );
}

void fail_on_pong_timeout(websocketpp::connection_hdl, std::string) {
    boost_fail( "expected no pong timeout" );
}

void req_pong(std::string expected_payload, websocketpp::connection_hdl,
    std::string payload)
{
    boost_check_equal( expected_payload, payload );
}

void fail_on_open(websocketpp::connection_hdl) {
    boost_fail( "expected no open handler" );
}

void delay(websocketpp::connection_hdl, long duration) {
    sleep(duration);
}

template <typename t>
void check_ec(t * c, websocketpp::lib::error_code ec,
    websocketpp::connection_hdl hdl)
{
    typename t::connection_ptr con = c->get_con_from_hdl(hdl);
    boost_check_equal( con->get_ec(), ec );
    //boost_check_equal( con->get_local_close_code(), websocketpp::close::status::normal );
    //boost_check_equal( con->get_remote_close_code(), websocketpp::close::status::normal );
}

template <typename t>
void check_ec_and_stop(t * e, websocketpp::lib::error_code ec,
    websocketpp::connection_hdl hdl)
{
    typename t::connection_ptr con = e->get_con_from_hdl(hdl);
    boost_check_equal( con->get_ec(), ec );
    //boost_check_equal( con->get_local_close_code(), websocketpp::close::status::normal );
    //boost_check_equal( con->get_remote_close_code(), websocketpp::close::status::normal );
    e->stop();
}

template <typename t>
void req_pong_timeout(t * c, std::string expected_payload,
    websocketpp::connection_hdl hdl, std::string payload)
{
    typename t::connection_ptr con = c->get_con_from_hdl(hdl);
    boost_check_equal( payload, expected_payload );
    con->close(websocketpp::close::status::normal,"");
}

template <typename t>
void close(t * e, websocketpp::connection_hdl hdl) {
    e->get_con_from_hdl(hdl)->close(websocketpp::close::status::normal,"");
}

// wait for the specified time period then fail the test
void run_test_timer(long value) {
    sleep(value);
    boost_fail( "test timed out" );
}

boost_auto_test_case( pong_no_timeout ) {
    server s;
    client c;

    s.set_close_handler(bind(&stop_on_close,&s,::_1));

    // send a ping when the connection is open
    c.set_open_handler(bind(&ping_on_open<client>,&c,"foo",::_1));
    // require that a pong with matching payload is received
    c.set_pong_handler(bind(&req_pong,"foo",::_1,::_2));
    // require that a pong timeout is not received
    c.set_pong_timeout_handler(bind(&fail_on_pong_timeout,::_1,::_2));

    websocketpp::lib::thread sthread(websocketpp::lib::bind(&run_server,&s,9005,false));

    // run a client that closes the connection after 1 seconds
    run_time_limited_client(c, "http://localhost:9005", 1, false);

    sthread.join();
}

boost_auto_test_case( pong_timeout ) {
    server s;
    client c;

    s.set_ping_handler(on_ping);
    s.set_close_handler(bind(&stop_on_close,&s,::_1));

    c.set_fail_handler(bind(&check_ec<client>,&c,
        websocketpp::lib::error_code(),::_1));

    c.set_pong_handler(bind(&fail_on_pong,::_1,::_2));
    c.set_open_handler(bind(&ping_on_open<client>,&c,"foo",::_1));
    c.set_pong_timeout_handler(bind(&req_pong_timeout<client>,&c,"foo",::_1,::_2));
    c.set_close_handler(bind(&check_ec<client>,&c,
        websocketpp::lib::error_code(),::_1));

    websocketpp::lib::thread sthread(websocketpp::lib::bind(&run_server,&s,9005,false));
    websocketpp::lib::thread tthread(websocketpp::lib::bind(&run_test_timer,6));
    tthread.detach();

    run_client(c, "http://localhost:9005",false);

    sthread.join();
}

boost_auto_test_case( client_open_handshake_timeout ) {
    client c;

    // set open handler to fail test
    c.set_open_handler(bind(&fail_on_open,::_1));
    // set fail hander to test for the right fail error code
    c.set_fail_handler(bind(&check_ec<client>,&c,
        websocketpp::error::open_handshake_timeout,::_1));

    websocketpp::lib::thread sthread(websocketpp::lib::bind(&run_dummy_server,9005));
    websocketpp::lib::thread tthread(websocketpp::lib::bind(&run_test_timer,6));
    sthread.detach();
    tthread.detach();

    run_client(c, "http://localhost:9005");
}

boost_auto_test_case( server_open_handshake_timeout ) {
    server s;

    // set open handler to fail test
    s.set_open_handler(bind(&fail_on_open,::_1));
    // set fail hander to test for the right fail error code
    s.set_fail_handler(bind(&check_ec_and_stop<server>,&s,
        websocketpp::error::open_handshake_timeout,::_1));

    websocketpp::lib::thread sthread(websocketpp::lib::bind(&run_server,&s,9005,false));
    websocketpp::lib::thread tthread(websocketpp::lib::bind(&run_test_timer,6));
    tthread.detach();

    run_dummy_client("9005");

    sthread.join();
}

boost_auto_test_case( client_self_initiated_close_handshake_timeout ) {
    server s;
    client c;

    // on open server sleeps for longer than the timeout
    // on open client sends close handshake
    // client handshake timer should be triggered
    s.set_open_handler(bind(&delay,::_1,1));
    s.set_close_handler(bind(&stop_on_close,&s,::_1));

    c.set_open_handler(bind(&close<client>,&c,::_1));
    c.set_close_handler(bind(&check_ec<client>,&c,
        websocketpp::error::close_handshake_timeout,::_1));

    websocketpp::lib::thread sthread(websocketpp::lib::bind(&run_server,&s,9005,false));
    websocketpp::lib::thread tthread(websocketpp::lib::bind(&run_test_timer,6));
    tthread.detach();

    run_client(c, "http://localhost:9005", false);

    sthread.join();
}

boost_auto_test_case( client_peer_initiated_close_handshake_timeout ) {
    // on open server sends close
    // client should ack normally and then wait
    // server leaves tcp connection open
    // client handshake timer should be triggered

    // todo: how to make a mock server that leaves the tcp connection open?
}

boost_auto_test_case( server_self_initiated_close_handshake_timeout ) {
    server s;
    client c;

    // on open server sends close
    // on open client sleeps for longer than the timeout
    // server handshake timer should be triggered

    s.set_open_handler(bind(&close<server>,&s,::_1));
    s.set_close_handler(bind(&check_ec_and_stop<server>,&s,
        websocketpp::error::close_handshake_timeout,::_1));

    c.set_open_handler(bind(&delay,::_1,1));

    websocketpp::lib::thread sthread(websocketpp::lib::bind(&run_server,&s,9005,false));
    websocketpp::lib::thread tthread(websocketpp::lib::bind(&run_test_timer,6));
    tthread.detach();

    run_client(c, "http://localhost:9005",false);

    sthread.join();
}

boost_auto_test_case( client_runs_out_of_work ) {
    client c;

    websocketpp::lib::thread tthread(websocketpp::lib::bind(&run_test_timer,3));
    tthread.detach();

    websocketpp::lib::error_code ec;
    c.init_asio(ec);
    boost_check(!ec);

    c.run();

    // this test checks that an io_service with no work ends immediately.
    boost_check(true);
}




boost_auto_test_case( client_is_perpetual ) {
    client c;
    bool flag = false;
    websocketpp::lib::mutex mutex;

    websocketpp::lib::error_code ec;
    c.init_asio(ec);
    boost_check(!ec);

    c.start_perpetual();

    websocketpp::lib::thread cthread(websocketpp::lib::bind(&run_client_and_mark,&c,&flag,&mutex));

    sleep(1);

    {
        // checks that the thread hasn't exited yet
        websocketpp::lib::lock_guard<websocketpp::lib::mutex> lock(mutex);
        boost_check( !flag );
    }

    c.stop_perpetual();

    sleep(1);

    {
        // checks that the thread has exited
        websocketpp::lib::lock_guard<websocketpp::lib::mutex> lock(mutex);
        boost_check( flag );
    }

    cthread.join();
}

boost_auto_test_case( client_failed_connection ) {
    client c;

    run_time_limited_client(c,"http://localhost:9005", 5, false);
}

boost_auto_test_case( stop_listening ) {
    server s;
    client c;

    // the first connection stops the server from listening
    s.set_open_handler(bind(&cancel_on_open,&s,::_1));

    // client immediately closes after opening a connection
    c.set_open_handler(bind(&close<client>,&c,::_1));

    websocketpp::lib::thread sthread(websocketpp::lib::bind(&run_server,&s,9005,false));
    websocketpp::lib::thread tthread(websocketpp::lib::bind(&run_test_timer,2));
    tthread.detach();

    run_client(c, "http://localhost:9005",false);

    sthread.join();
}

boost_auto_test_case( pause_reading ) {
    iostream_server s;
    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\n\r\n";
    char buffer[2] = { char(0x81), char(0x80) };
    
    // suppress output (it needs a place to go to avoid error but we don't care what it is)
    std::stringstream null_output;
    s.register_ostream(&null_output);
    
    iostream_server::connection_ptr con = s.get_connection();
    con->start();

    // read handshake, should work
    boost_check_equal( con->read_some(handshake.data(), handshake.length()), handshake.length());
    
    // pause reading and try again. the first read should work, the second should return 0
    // the first read was queued already after the handshake so it will go through because
    // reading wasn't paused when it was queued. the byte it reads wont be enough to
    // complete the frame so another read will be requested. this one wont actually happen
    // because the connection is paused now.
    con->pause_reading();
    boost_check_equal( con->read_some(buffer, 1), 1);
    boost_check_equal( con->read_some(buffer+1, 1), 0);
    // resume reading and try again. should work this time because the resume should have
    // re-queued a read.
    con->resume_reading();
    boost_check_equal( con->read_some(buffer+1, 1), 1);
}


boost_auto_test_case( server_connection_cleanup ) {
    server_tls s;
}