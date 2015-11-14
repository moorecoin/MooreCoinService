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
#define boost_test_module client
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <websocketpp/random/random_device.hpp>

#include <websocketpp/config/core.hpp>
#include <websocketpp/client.hpp>

#include <websocketpp/http/request.hpp>

struct stub_config : public websocketpp::config::core {
    typedef core::concurrency_type concurrency_type;

    typedef core::request_type request_type;
    typedef core::response_type response_type;

    typedef core::message_type message_type;
    typedef core::con_msg_manager_type con_msg_manager_type;
    typedef core::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef core::alog_type alog_type;
    typedef core::elog_type elog_type;

    //typedef core::rng_type rng_type;
    typedef websocketpp::random::random_device::int_generator<uint32_t,concurrency_type> rng_type;

    typedef core::transport_type transport_type;

    typedef core::endpoint_base endpoint_base;

    static const websocketpp::log::level elog_level = websocketpp::log::elevel::none;
    static const websocketpp::log::level alog_level = websocketpp::log::alevel::none;
};

typedef websocketpp::client<stub_config> client;
typedef client::connection_ptr connection_ptr;

boost_auto_test_case( invalid_uri ) {
    client c;
    websocketpp::lib::error_code ec;

    connection_ptr con = c.get_connection("foo", ec);

    boost_check_equal( ec , websocketpp::error::make_error_code(websocketpp::error::invalid_uri) );
}

boost_auto_test_case( unsecure_endpoint ) {
    client c;
    websocketpp::lib::error_code ec;

    connection_ptr con = c.get_connection("wss://localhost/", ec);

    boost_check_equal( ec , websocketpp::error::make_error_code(websocketpp::error::endpoint_not_secure) );
}

boost_auto_test_case( get_connection ) {
    client c;
    websocketpp::lib::error_code ec;

    connection_ptr con = c.get_connection("ws://localhost/", ec);

    boost_check( con );
    boost_check_equal( con->get_host() , "localhost" );
    boost_check_equal( con->get_port() , 80 );
    boost_check_equal( con->get_secure() , false );
    boost_check_equal( con->get_resource() , "/" );
}

boost_auto_test_case( connect_con ) {
    client c;
    websocketpp::lib::error_code ec;
    std::stringstream out;
    std::string o;

    c.register_ostream(&out);

    connection_ptr con = c.get_connection("ws://localhost/", ec);
    c.connect(con);

    o = out.str();
    websocketpp::http::parser::request r;
    r.consume(o.data(),o.size());

    boost_check( r.ready() );
    boost_check_equal( r.get_method(), "get");
    boost_check_equal( r.get_version(), "http/1.1");
    boost_check_equal( r.get_uri(), "/");

    boost_check_equal( r.get_header("host"), "localhost");
    boost_check_equal( r.get_header("sec-websocket-version"), "13");
    boost_check_equal( r.get_header("connection"), "upgrade");
    boost_check_equal( r.get_header("upgrade"), "websocket");

    // key is randomly generated & user-agent will change so just check that
    // they are not empty.
    boost_check_ne( r.get_header("sec-websocket-key"), "");
    boost_check_ne( r.get_header("user-agent"), "" );

    // connection should have written out an opening handshake request and be in
    // the read response internal state

    // todo: more tests related to reading the http response
    std::stringstream channel2;
    channel2 << "e\r\n\r\n";
    channel2 >> *con;
}

boost_auto_test_case( select_subprotocol ) {
    client c;
    websocketpp::lib::error_code ec;
    using websocketpp::error::make_error_code;

    connection_ptr con = c.get_connection("ws://localhost/", ec);

    boost_check( con );

    con->select_subprotocol("foo",ec);
    boost_check_equal( ec , make_error_code(websocketpp::error::server_only) );
    boost_check_throw( con->select_subprotocol("foo") , websocketpp::exception );
}

boost_auto_test_case( add_subprotocols_invalid ) {
    client c;
    websocketpp::lib::error_code ec;
    using websocketpp::error::make_error_code;

    connection_ptr con = c.get_connection("ws://localhost/", ec);
    boost_check( con );

    con->add_subprotocol("",ec);
    boost_check_equal( ec , make_error_code(websocketpp::error::invalid_subprotocol) );
    boost_check_throw( con->add_subprotocol("") , websocketpp::exception );

    con->add_subprotocol("foo,bar",ec);
    boost_check_equal( ec , make_error_code(websocketpp::error::invalid_subprotocol) );
    boost_check_throw( con->add_subprotocol("foo,bar") , websocketpp::exception );
}

boost_auto_test_case( add_subprotocols ) {
    client c;
    websocketpp::lib::error_code ec;
    std::stringstream out;
    std::string o;

    c.register_ostream(&out);

    connection_ptr con = c.get_connection("ws://localhost/", ec);
    boost_check( con );

    con->add_subprotocol("foo",ec);
    boost_check( !ec );

    boost_check_no_throw( con->add_subprotocol("bar") );

    c.connect(con);

    o = out.str();
    websocketpp::http::parser::request r;
    r.consume(o.data(),o.size());

    boost_check( r.ready() );
    boost_check_equal( r.get_header("sec-websocket-protocol"), "foo, bar");
}


