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
#define boost_test_module hybi_00_processor
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/processors/hybi00.hpp>
#include <websocketpp/http/request.hpp>
#include <websocketpp/http/response.hpp>
#include <websocketpp/message_buffer/message.hpp>
#include <websocketpp/message_buffer/alloc.hpp>

struct stub_config {
    typedef websocketpp::http::parser::request request_type;
    typedef websocketpp::http::parser::response response_type;

    typedef websocketpp::message_buffer::message
        <websocketpp::message_buffer::alloc::con_msg_manager> message_type;
    typedef websocketpp::message_buffer::alloc::con_msg_manager<message_type>
        con_msg_manager_type;
        
    static const size_t max_message_size = 16000000;
};

struct processor_setup {
    processor_setup(bool server)
      : msg_manager(new stub_config::con_msg_manager_type())
      , p(false,server,msg_manager) {}

    websocketpp::lib::error_code ec;
    stub_config::con_msg_manager_type::ptr msg_manager;
    stub_config::request_type req;
    stub_config::response_type res;
    websocketpp::processor::hybi00<stub_config> p;
};

typedef stub_config::message_type::ptr message_ptr;

boost_auto_test_case( exact_match ) {
    processor_setup env(true);

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\norigin: http://example.com\r\nsec-websocket-key1: 3e6b263  4 17 80\r\nsec-websocket-key2: 17  9 g`zd9   2 2b 7x 3 /r90\r\n\r\n";

    env.req.consume(handshake.c_str(),handshake.size());
    env.req.replace_header("sec-websocket-key3","wjn}|m(6");

    boost_check(websocketpp::processor::is_websocket_handshake(env.req));
    boost_check_equal(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    env.ec = env.p.validate_handshake(env.req);
    boost_check(!env.ec);

    websocketpp::uri_ptr u;

    boost_check_no_throw( u = env.p.get_uri(env.req) );

    boost_check_equal(u->get_secure(), false);
    boost_check_equal(u->get_host(), "www.example.com");
    boost_check_equal(u->get_resource(), "/");
    boost_check_equal(u->get_port(), websocketpp::uri_default_port);

    env.p.process_handshake(env.req,"",env.res);

    boost_check_equal(env.res.get_header("connection"), "upgrade");
    boost_check_equal(env.res.get_header("upgrade"), "websocket");
    boost_check_equal(env.res.get_header("sec-websocket-origin"), "http://example.com");

    boost_check_equal(env.res.get_header("sec-websocket-location"), "ws://www.example.com/");
    boost_check_equal(env.res.get_header("sec-websocket-key3"), "n`9ebk9z$r8potvb");
}

boost_auto_test_case( non_get_method ) {
    processor_setup env(true);

    std::string handshake = "post / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-key1: 3e6b263  4 17 80\r\nsec-websocket-key2: 17  9 g`zd9   2 2b 7x 3 /r90\r\n\r\n";

    env.req.consume(handshake.c_str(),handshake.size());
    env.req.replace_header("sec-websocket-key3","janelle!");

    boost_check(websocketpp::processor::is_websocket_handshake(env.req));
    boost_check_equal(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    boost_check_equal( env.p.validate_handshake(env.req), websocketpp::processor::error::invalid_http_method );
}

boost_auto_test_case( old_http_version ) {
    processor_setup env(true);

    std::string handshake = "get / http/1.0\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-key1: 3e6b263  4 17 80\r\nsec-websocket-key2: 17  9 g`zd9   2 2b 7x 3 /r90\r\n\r\n";

    env.req.consume(handshake.c_str(),handshake.size());
    env.req.replace_header("sec-websocket-key3","janelle!");

    boost_check(websocketpp::processor::is_websocket_handshake(env.req));
    boost_check_equal(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    boost_check_equal( env.p.validate_handshake(env.req), websocketpp::processor::error::invalid_http_version );
}

boost_auto_test_case( missing_handshake_key1 ) {
    processor_setup env(true);

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-key1: 3e6b263  4 17 80\r\n\r\n";

    env.req.consume(handshake.c_str(),handshake.size());
    env.req.replace_header("sec-websocket-key3","janelle!");

    boost_check(websocketpp::processor::is_websocket_handshake(env.req));
    boost_check_equal(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    boost_check_equal( env.p.validate_handshake(env.req), websocketpp::processor::error::missing_required_header );
}

boost_auto_test_case( missing_handshake_key2 ) {
    processor_setup env(true);

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-key2: 17  9 g`zd9   2 2b 7x 3 /r90\r\n\r\n";

    env.req.consume(handshake.c_str(),handshake.size());
    env.req.replace_header("sec-websocket-key3","janelle!");

    boost_check(websocketpp::processor::is_websocket_handshake(env.req));
    boost_check_equal(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    boost_check_equal( env.p.validate_handshake(env.req), websocketpp::processor::error::missing_required_header );
}

boost_auto_test_case( bad_host ) {
    processor_setup env(true);
    websocketpp::uri_ptr u;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com:70000\r\nconnection: upgrade\r\nupgrade: websocket\r\norigin: http://example.com\r\nsec-websocket-key1: 3e6b263  4 17 80\r\nsec-websocket-key2: 17  9 g`zd9   2 2b 7x 3 /r90\r\n\r\n";

    env.req.consume(handshake.c_str(),handshake.size());
    env.req.replace_header("sec-websocket-key3","janelle!");

    boost_check(websocketpp::processor::is_websocket_handshake(env.req));
    boost_check_equal(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    boost_check( !env.p.validate_handshake(env.req) );

    boost_check( !env.p.get_uri(env.req)->get_valid() );
}

boost_auto_test_case( extract_subprotocols ) {
    processor_setup env(true);

    std::vector<std::string> subps;

    boost_check( !env.p.extract_subprotocols(env.req,subps) );
    boost_check_equal( subps.size(), 0 );
}

boost_auto_test_case( prepare_data_frame_null ) {
    processor_setup env(true);

    message_ptr in = env.msg_manager->get_message();
    message_ptr out = env.msg_manager->get_message();
    message_ptr invalid;


    // empty pointers arguements should return sane error
    boost_check_equal( env.p.prepare_data_frame(invalid,invalid), websocketpp::processor::error::invalid_arguments );

    boost_check_equal( env.p.prepare_data_frame(in,invalid), websocketpp::processor::error::invalid_arguments );

    boost_check_equal( env.p.prepare_data_frame(invalid,out), websocketpp::processor::error::invalid_arguments );

    // test valid opcodes
    // text (1) should be the only valid opcode
    for (int i = 0; i < 0xf; i++) {
        in->set_opcode(websocketpp::frame::opcode::value(i));

        env.ec = env.p.prepare_data_frame(in,out);

        if (i != 1) {
            boost_check_equal( env.ec, websocketpp::processor::error::invalid_opcode );
        } else {
            boost_check_ne( env.ec, websocketpp::processor::error::invalid_opcode );
        }
    }

    /*
     * todo: tests for invalid utf8
    char buf[2] = {0x00, 0x00};

    in->set_opcode(websocketpp::frame::opcode::text);
    in->set_payload("foo");

    env.ec = env.p.prepare_data_frame(in,out);
    boost_check_equal( env.ec, websocketpp::processor::error::invalid_payload );
    */
}

boost_auto_test_case( prepare_data_frame ) {
    processor_setup env(true);

    message_ptr in = env.msg_manager->get_message();
    message_ptr out = env.msg_manager->get_message();

    in->set_opcode(websocketpp::frame::opcode::text);
    in->set_payload("foo");

    env.ec = env.p.prepare_data_frame(in,out);

    unsigned char raw_header[1] = {0x00};
    unsigned char raw_payload[4] = {0x66,0x6f,0x6f,0xff};

    boost_check( !env.ec );
    boost_check_equal( out->get_header(), std::string(reinterpret_cast<char*>(raw_header),1) );
    boost_check_equal( out->get_payload(), std::string(reinterpret_cast<char*>(raw_payload),4) );
}


boost_auto_test_case( empty_consume ) {
    uint8_t frame[2] = {0x00,0x00};

    processor_setup env(true);

    size_t ret = env.p.consume(frame,0,env.ec);

    boost_check_equal( ret, 0);
    boost_check( !env.ec );
    boost_check_equal( env.p.ready(), false );
}

boost_auto_test_case( empty_frame ) {
    uint8_t frame[2] = {0x00, 0xff};

    processor_setup env(true);

    size_t ret = env.p.consume(frame,2,env.ec);

    boost_check_equal( ret, 2);
    boost_check( !env.ec );
    boost_check_equal( env.p.ready(), true );
    boost_check_equal( env.p.get_message()->get_payload(), "" );
    boost_check_equal( env.p.ready(), false );
}

boost_auto_test_case( short_frame ) {
    uint8_t frame[5] = {0x00, 0x66, 0x6f, 0x6f, 0xff};

    processor_setup env(true);

    size_t ret = env.p.consume(frame,5,env.ec);

    boost_check_equal( ret, 5);
    boost_check( !env.ec );
    boost_check_equal( env.p.ready(), true );
    boost_check_equal( env.p.get_message()->get_payload(), "foo" );
    boost_check_equal( env.p.ready(), false );
}
