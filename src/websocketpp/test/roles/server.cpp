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
#define boost_test_module server
#include <boost/test/unit_test.hpp>

#include <iostream>

// test environment:
// server, no tls, no locks, iostream based transport
#include <websocketpp/config/core.hpp>
#include <websocketpp/server.hpp>

typedef websocketpp::server<websocketpp::config::core> server;
typedef websocketpp::config::core::message_type::ptr message_ptr;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

/*struct stub_config : public websocketpp::config::core {
    typedef core::concurrency_type concurrency_type;

    typedef core::request_type request_type;
    typedef core::response_type response_type;

    typedef core::message_type message_type;
    typedef core::con_msg_manager_type con_msg_manager_type;
    typedef core::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef core::alog_type alog_type;
    typedef core::elog_type elog_type;

    typedef core::rng_type rng_type;

    typedef core::transport_type transport_type;

    typedef core::endpoint_base endpoint_base;
};*/

/* run server and return output test rig */
std::string run_server_test(server& s, std::string input) {
    server::connection_ptr con;
    std::stringstream output;

    s.register_ostream(&output);
    s.clear_access_channels(websocketpp::log::alevel::all);
    s.clear_error_channels(websocketpp::log::elevel::all);

    con = s.get_connection();
    con->start();

    std::stringstream channel;

    channel << input;
    channel >> *con;

    return output.str();
}

/* handler library*/
void echo_func(server* s, websocketpp::connection_hdl hdl, message_ptr msg) {
    s->send(hdl, msg->get_payload(), msg->get_opcode());
}

bool validate_func_subprotocol(server* s, std::string* out, std::string accept,
    websocketpp::connection_hdl hdl)
{
    server::connection_ptr con = s->get_con_from_hdl(hdl);

    std::stringstream o;

    const std::vector<std::string> & protocols = con->get_requested_subprotocols();
    std::vector<std::string>::const_iterator it;

    for (it = protocols.begin(); it != protocols.end(); ++it) {
        o << *it << ",";
    }

    *out = o.str();

    if (accept != "") {
        con->select_subprotocol(accept);
    }

    return true;
}

void open_func_subprotocol(server* s, std::string* out, websocketpp::connection_hdl hdl) {
    server::connection_ptr con = s->get_con_from_hdl(hdl);

    *out = con->get_subprotocol();
}

/* tests */
boost_auto_test_case( basic_websocket_request ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example.com\r\n\r\n";
    std::string output = "http/1.1 101 switching protocols\r\nconnection: upgrade\r\nsec-websocket-accept: s3pplmbitxaq9kygzzhzrbk+xoo=\r\nserver: test\r\nupgrade: websocket\r\n\r\n";

    server s;
    s.set_user_agent("test");

    boost_check_equal(run_server_test(s,input), output);
}

boost_auto_test_case( invalid_websocket_version ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: a\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example.com\r\n\r\n";
    std::string output = "http/1.1 400 bad request\r\nserver: test\r\n\r\n";

    server s;
    s.set_user_agent("test");
    //s.set_message_handler(bind(&echo_func,&s,::_1,::_2));

    boost_check_equal(run_server_test(s,input), output);
}

boost_auto_test_case( unimplemented_websocket_version ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 14\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example.com\r\n\r\n";

    std::string output = "http/1.1 400 bad request\r\nsec-websocket-version: 0,7,8,13\r\nserver: test\r\n\r\n";

    server s;
    s.set_user_agent("test");

    boost_check_equal(run_server_test(s,input), output);
}

boost_auto_test_case( list_subprotocol_empty ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example.com\r\nsec-websocket-protocol: foo\r\n\r\n";

    std::string output = "http/1.1 101 switching protocols\r\nconnection: upgrade\r\nsec-websocket-accept: s3pplmbitxaq9kygzzhzrbk+xoo=\r\nserver: test\r\nupgrade: websocket\r\n\r\n";

    std::string subprotocol;

    server s;
    s.set_user_agent("test");
    s.set_open_handler(bind(&open_func_subprotocol,&s,&subprotocol,::_1));

    boost_check_equal(run_server_test(s,input), output);
    boost_check_equal(subprotocol, "");
}

boost_auto_test_case( list_subprotocol_one ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example.com\r\nsec-websocket-protocol: foo\r\n\r\n";

    std::string output = "http/1.1 101 switching protocols\r\nconnection: upgrade\r\nsec-websocket-accept: s3pplmbitxaq9kygzzhzrbk+xoo=\r\nserver: test\r\nupgrade: websocket\r\n\r\n";

    std::string validate;
    std::string open;

    server s;
    s.set_user_agent("test");
    s.set_validate_handler(bind(&validate_func_subprotocol,&s,&validate,"",::_1));
    s.set_open_handler(bind(&open_func_subprotocol,&s,&open,::_1));

    boost_check_equal(run_server_test(s,input), output);
    boost_check_equal(validate, "foo,");
    boost_check_equal(open, "");
}

boost_auto_test_case( accept_subprotocol_one ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example.com\r\nsec-websocket-protocol: foo\r\n\r\n";

    std::string output = "http/1.1 101 switching protocols\r\nconnection: upgrade\r\nsec-websocket-accept: s3pplmbitxaq9kygzzhzrbk+xoo=\r\nsec-websocket-protocol: foo\r\nserver: test\r\nupgrade: websocket\r\n\r\n";

    std::string validate;
    std::string open;

    server s;
    s.set_user_agent("test");
    s.set_validate_handler(bind(&validate_func_subprotocol,&s,&validate,"foo",::_1));
    s.set_open_handler(bind(&open_func_subprotocol,&s,&open,::_1));

    boost_check_equal(run_server_test(s,input), output);
    boost_check_equal(validate, "foo,");
    boost_check_equal(open, "foo");
}

boost_auto_test_case( accept_subprotocol_invalid ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example.com\r\nsec-websocket-protocol: foo\r\n\r\n";

    std::string output = "http/1.1 101 switching protocols\r\nconnection: upgrade\r\nsec-websocket-accept: s3pplmbitxaq9kygzzhzrbk+xoo=\r\nsec-websocket-protocol: foo\r\nserver: test\r\nupgrade: websocket\r\n\r\n";

    std::string validate;
    std::string open;

    server s;
    s.set_user_agent("test");
    s.set_validate_handler(bind(&validate_func_subprotocol,&s,&validate,"foo2",::_1));
    s.set_open_handler(bind(&open_func_subprotocol,&s,&open,::_1));

    std::string o;

    boost_check_throw(o = run_server_test(s,input), websocketpp::exception);
}

boost_auto_test_case( accept_subprotocol_two ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example.com\r\nsec-websocket-protocol: foo, bar\r\n\r\n";

    std::string output = "http/1.1 101 switching protocols\r\nconnection: upgrade\r\nsec-websocket-accept: s3pplmbitxaq9kygzzhzrbk+xoo=\r\nsec-websocket-protocol: bar\r\nserver: test\r\nupgrade: websocket\r\n\r\n";

    std::string validate;
    std::string open;

    server s;
    s.set_user_agent("test");
    s.set_validate_handler(bind(&validate_func_subprotocol,&s,&validate,"bar",::_1));
    s.set_open_handler(bind(&open_func_subprotocol,&s,&open,::_1));

    boost_check_equal(run_server_test(s,input), output);
    boost_check_equal(validate, "foo,bar,");
    boost_check_equal(open, "bar");
}

/*boost_auto_test_case( user_reject_origin ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example2.com\r\n\r\n";
    std::string output = "http/1.1 403 forbidden\r\nserver: test\r\n\r\n";

    server s;
    s.set_user_agent("test");

    boost_check(run_server_test(s,input) == output);
}*/
