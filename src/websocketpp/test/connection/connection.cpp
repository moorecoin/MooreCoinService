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
#define boost_test_module connection
#include <boost/test/unit_test.hpp>

#include "connection_tu2.hpp"

// note: these tests currently test against hardcoded output values. i am not
// sure how problematic this will be. if issues arise like order of headers the
// output should be parsed by http::response and have values checked directly

boost_auto_test_case( basic_http_request ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\n\r\n";
    std::string output = "http/1.1 426 upgrade required\r\nserver: " +
                         std::string(websocketpp::user_agent)+"\r\n\r\n";

    std::string o2 = run_server_test(input);

    boost_check(o2 == output);
}

struct connection_extension {
    connection_extension() : extension_value(5) {}

    int extension_method() {
        return extension_value;
    }

    bool is_server() const {
        return false;
    }

    int extension_value;
};

struct stub_config : public websocketpp::config::core {
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
    typedef connection_extension connection_base;
};

struct connection_setup {
    connection_setup(bool p_is_server) : c(p_is_server, "", alog, elog, rng) {}

    websocketpp::lib::error_code ec;
    stub_config::alog_type alog;
    stub_config::elog_type elog;
    stub_config::rng_type rng;
    websocketpp::connection<stub_config> c;
};

/*void echo_func(server* s, websocketpp::connection_hdl hdl, message_ptr msg) {
    s->send(hdl, msg->get_payload(), msg->get_opcode());
}*/

void validate_func(server* s, websocketpp::connection_hdl hdl, message_ptr msg) {
    s->send(hdl, msg->get_payload(), msg->get_opcode());
}

bool validate_set_ua(server* s, websocketpp::connection_hdl hdl) {
    server::connection_ptr con = s->get_con_from_hdl(hdl);
    con->replace_header("server","foo");
    return true;
}

void http_func(server* s, websocketpp::connection_hdl hdl) {
    server::connection_ptr con = s->get_con_from_hdl(hdl);

    std::string res = con->get_resource();

    con->set_body(res);
    con->set_status(websocketpp::http::status_code::ok);
}

boost_auto_test_case( connection_extensions ) {
    connection_setup env(true);

    boost_check( env.c.extension_value == 5 );
    boost_check( env.c.extension_method() == 5 );

    boost_check( env.c.is_server() == true );
}

boost_auto_test_case( basic_websocket_request ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example.com\r\n\r\n";
    std::string output = "http/1.1 101 switching protocols\r\nconnection: upgrade\r\nsec-websocket-accept: s3pplmbitxaq9kygzzhzrbk+xoo=\r\nserver: ";
    output+=websocketpp::user_agent;
    output+="\r\nupgrade: websocket\r\n\r\n";

    server s;
    s.set_message_handler(bind(&echo_func,&s,::_1,::_2));

    boost_check(run_server_test(s,input) == output);
}

boost_auto_test_case( http_request ) {
    std::string input = "get /foo/bar http/1.1\r\nhost: www.example.com\r\norigin: http://www.example.com\r\n\r\n";
    std::string output = "http/1.1 200 ok\r\ncontent-length: 8\r\nserver: ";
    output+=websocketpp::user_agent;
    output+="\r\n\r\n/foo/bar";

    server s;
    s.set_http_handler(bind(&http_func,&s,::_1));

    boost_check_equal(run_server_test(s,input), output);
}

boost_auto_test_case( request_no_server_header ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example.com\r\n\r\n";
    std::string output = "http/1.1 101 switching protocols\r\nconnection: upgrade\r\nsec-websocket-accept: s3pplmbitxaq9kygzzhzrbk+xoo=\r\nupgrade: websocket\r\n\r\n";

    server s;
    s.set_user_agent("");
    s.set_message_handler(bind(&echo_func,&s,::_1,::_2));

    boost_check_equal(run_server_test(s,input), output);
}

boost_auto_test_case( request_no_server_header_override ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example.com\r\n\r\n";
    std::string output = "http/1.1 101 switching protocols\r\nconnection: upgrade\r\nsec-websocket-accept: s3pplmbitxaq9kygzzhzrbk+xoo=\r\nserver: foo\r\nupgrade: websocket\r\n\r\n";

    server s;
    s.set_user_agent("");
    s.set_message_handler(bind(&echo_func,&s,::_1,::_2));
    s.set_validate_handler(bind(&validate_set_ua,&s,::_1));

    boost_check_equal(run_server_test(s,input), output);
}

boost_auto_test_case( basic_client_websocket ) {
    std::string uri = "ws://localhost";

    //std::string output = "http/1.1 101 switching protocols\r\nconnection: upgrade\r\nsec-websocket-accept: s3pplmbitxaq9kygzzhzrbk+xoo=\r\nserver: foo\r\nupgrade: websocket\r\n\r\n";

    std::string ref = "get / http/1.1\r\nconnection: upgrade\r\nfoo: bar\r\nhost: localhost\r\nsec-websocket-key: aaaaaaaaaaaaaaaaaaaaaa==\r\nsec-websocket-version: 13\r\nupgrade: websocket\r\nuser-agent: foo\r\n\r\n";

    std::stringstream output;

    client e;
    e.set_access_channels(websocketpp::log::alevel::none);
    e.set_error_channels(websocketpp::log::elevel::none);
    e.set_user_agent("foo");
    e.register_ostream(&output);

    client::connection_ptr con;
    websocketpp::lib::error_code ec;
    con = e.get_connection(uri, ec);
    con->append_header("foo","bar");
    e.connect(con);

    boost_check_equal(ref, output.str());
}

boost_auto_test_case( set_max_message_size ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\n\r\n";
    
    // after the handshake, add a single frame with a message that is too long.
    char frame0[10] = {char(0x82), char(0x83), 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01};
    input.append(frame0, 10);
    
    std::string output = "http/1.1 101 switching protocols\r\nconnection: upgrade\r\nsec-websocket-accept: s3pplmbitxaq9kygzzhzrbk+xoo=\r\nserver: foo\r\nupgrade: websocket\r\n\r\n";

    // after the handshake, add a single frame with a close message with message too big
    // error code.
    char frame1[4] = {char(0x88), 0x19, 0x03, char(0xf1)};
    output.append(frame1, 4);
    output.append("a message was too large");

    server s;
    s.set_user_agent("");
    s.set_validate_handler(bind(&validate_set_ua,&s,::_1));
    s.set_max_message_size(2);

    boost_check_equal(run_server_test(s,input), output);
}

// todo: set max message size in client endpoint test case
// todo: set max message size mid connection test case
// todo: [maybe] set max message size in open handler

/*

boost_auto_test_case( user_reject_origin ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example2.com\r\n\r\n";
    std::string output = "http/1.1 403 forbidden\r\nserver: "+websocketpp::user_agent+"\r\n\r\n";

    boost_check(run_server_test(input) == output);
}

boost_auto_test_case( basic_text_message ) {
    std::string input = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\norigin: http://www.example.com\r\n\r\n";

    unsigned char frames[8] = {0x82,0x82,0xff,0xff,0xff,0xff,0xd5,0xd5};
    input.append(reinterpret_cast<char*>(frames),8);

    std::string output = "http/1.1 101 switching protocols\r\nconnection: upgrade\r\nsec-websocket-accept: s3pplmbitxaq9kygzzhzrbk+xoo=\r\nserver: "+websocketpp::user_agent+"\r\nupgrade: websocket\r\n\r\n**";

    boost_check( run_server_test(input) == output);
}
*/
