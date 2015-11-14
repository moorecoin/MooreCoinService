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
#define boost_test_module hybi_13_processor
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/processors/hybi13.hpp>

#include <websocketpp/http/request.hpp>
#include <websocketpp/http/response.hpp>
#include <websocketpp/message_buffer/message.hpp>
#include <websocketpp/message_buffer/alloc.hpp>
#include <websocketpp/random/none.hpp>

#include <websocketpp/extensions/permessage_deflate/disabled.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>

struct stub_config {
    typedef websocketpp::http::parser::request request_type;
    typedef websocketpp::http::parser::response response_type;

    typedef websocketpp::message_buffer::message
        <websocketpp::message_buffer::alloc::con_msg_manager> message_type;
    typedef websocketpp::message_buffer::alloc::con_msg_manager<message_type>
        con_msg_manager_type;

    typedef websocketpp::random::none::int_generator<uint32_t> rng_type;

    struct permessage_deflate_config {
        typedef stub_config::request_type request_type;
    };

    typedef websocketpp::extensions::permessage_deflate::disabled
        <permessage_deflate_config> permessage_deflate_type;

    static const size_t max_message_size = 16000000;
    static const bool enable_extensions = false;
};

struct stub_config_ext {
    typedef websocketpp::http::parser::request request_type;
    typedef websocketpp::http::parser::response response_type;

    typedef websocketpp::message_buffer::message
        <websocketpp::message_buffer::alloc::con_msg_manager> message_type;
    typedef websocketpp::message_buffer::alloc::con_msg_manager<message_type>
        con_msg_manager_type;

    typedef websocketpp::random::none::int_generator<uint32_t> rng_type;

    struct permessage_deflate_config {
        typedef stub_config_ext::request_type request_type;
    };

    typedef websocketpp::extensions::permessage_deflate::enabled
        <permessage_deflate_config> permessage_deflate_type;

    static const size_t max_message_size = 16000000;
    static const bool enable_extensions = true;
};

typedef stub_config::con_msg_manager_type con_msg_manager_type;
typedef stub_config::message_type::ptr message_ptr;

// set up a structure that constructs new copies of all of the support structure
// for using connection processors
struct processor_setup {
    processor_setup(bool server)
      : msg_manager(new con_msg_manager_type())
      , p(false,server,msg_manager,rng) {}

    websocketpp::lib::error_code ec;
    con_msg_manager_type::ptr msg_manager;
    stub_config::rng_type rng;
    stub_config::request_type req;
    stub_config::response_type res;
    websocketpp::processor::hybi13<stub_config> p;
};

struct processor_setup_ext {
    processor_setup_ext(bool server)
      : msg_manager(new con_msg_manager_type())
      , p(false,server,msg_manager,rng) {}

    websocketpp::lib::error_code ec;
    con_msg_manager_type::ptr msg_manager;
    stub_config::rng_type rng;
    stub_config::request_type req;
    stub_config::response_type res;
    websocketpp::processor::hybi13<stub_config_ext> p;
};

boost_auto_test_case( exact_match ) {
    processor_setup env(true);

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\n\r\n";

    env.req.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::is_websocket_handshake(env.req));
    boost_check_equal(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    boost_check(!env.p.validate_handshake(env.req));

    websocketpp::uri_ptr u;

    boost_check_no_throw( u = env.p.get_uri(env.req) );

    boost_check_equal(u->get_secure(), false);
    boost_check_equal(u->get_host(), "www.example.com");
    boost_check_equal(u->get_resource(), "/");
    boost_check_equal(u->get_port(), websocketpp::uri_default_port);

    env.p.process_handshake(env.req,"",env.res);

    boost_check_equal(env.res.get_header("connection"), "upgrade");
    boost_check_equal(env.res.get_header("upgrade"), "websocket");
    boost_check_equal(env.res.get_header("sec-websocket-accept"), "s3pplmbitxaq9kygzzhzrbk+xoo=");
}

boost_auto_test_case( non_get_method ) {
    processor_setup env(true);

    std::string handshake = "post / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: foo\r\n\r\n";

    env.req.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::is_websocket_handshake(env.req));
    boost_check_equal(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    boost_check( env.p.validate_handshake(env.req) == websocketpp::processor::error::invalid_http_method );
}

boost_auto_test_case( old_http_version ) {
    processor_setup env(true);

    std::string handshake = "get / http/1.0\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: foo\r\n\r\n";

    env.req.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::is_websocket_handshake(env.req));
    boost_check_equal(websocketpp::processor::get_websocket_version(env.req), env.p.get_version());
    boost_check_equal( env.p.validate_handshake(env.req), websocketpp::processor::error::invalid_http_version );
}

boost_auto_test_case( missing_handshake_key1 ) {
    processor_setup env(true);

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\n\r\n";

    env.req.consume(handshake.c_str(),handshake.size());

    boost_check( websocketpp::processor::is_websocket_handshake(env.req) );
    boost_check_equal( websocketpp::processor::get_websocket_version(env.req), env.p.get_version() );
    boost_check_equal( env.p.validate_handshake(env.req), websocketpp::processor::error::missing_required_header );
}

boost_auto_test_case( missing_handshake_key2 ) {
    processor_setup env(true);

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\n\r\n";

    env.req.consume(handshake.c_str(),handshake.size());

    boost_check( websocketpp::processor::is_websocket_handshake(env.req) );
    boost_check_equal( websocketpp::processor::get_websocket_version(env.req), env.p.get_version() );
    boost_check_equal( env.p.validate_handshake(env.req), websocketpp::processor::error::missing_required_header );
}

boost_auto_test_case( bad_host ) {
    processor_setup env(true);

    std::string handshake = "get / http/1.1\r\nhost: www.example.com:70000\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\nsec-websocket-key: foo\r\n\r\n";

    env.req.consume(handshake.c_str(),handshake.size());

    boost_check( websocketpp::processor::is_websocket_handshake(env.req) );
    boost_check_equal( websocketpp::processor::get_websocket_version(env.req), env.p.get_version() );
    boost_check( !env.p.validate_handshake(env.req) );
    boost_check( !env.p.get_uri(env.req)->get_valid() );
}

// frame tests to do
//
// unmasked, 0 length, binary
// 0x82 0x00
//
// masked, 0 length, binary
// 0x82 0x80
//
// unmasked, 0 length, text
// 0x81 0x00
//
// masked, 0 length, text
// 0x81 0x80

boost_auto_test_case( frame_empty_binary_unmasked ) {
    uint8_t frame[2] = {0x82, 0x00};

    // all in one chunk
    processor_setup env1(false);

    size_t ret1 = env1.p.consume(frame,2,env1.ec);

    boost_check_equal( ret1, 2 );
    boost_check( !env1.ec );
    boost_check_equal( env1.p.ready(), true );

    // two separate chunks
    processor_setup env2(false);

    boost_check_equal( env2.p.consume(frame,1,env2.ec), 1 );
    boost_check( !env2.ec );
    boost_check_equal( env2.p.ready(), false );

    boost_check_equal( env2.p.consume(frame+1,1,env2.ec), 1 );
    boost_check( !env2.ec );
    boost_check_equal( env2.p.ready(), true );
}

boost_auto_test_case( frame_small_binary_unmasked ) {
    processor_setup env(false);

    uint8_t frame[4] = {0x82, 0x02, 0x2a, 0x2a};

    boost_check_equal( env.p.get_message(), message_ptr() );
    boost_check_equal( env.p.consume(frame,4,env.ec), 4 );
    boost_check( !env.ec );
    boost_check_equal( env.p.ready(), true );

    message_ptr foo = env.p.get_message();

    boost_check_equal( env.p.get_message(), message_ptr() );
    boost_check_equal( foo->get_payload(), "**" );

}

boost_auto_test_case( frame_extended_binary_unmasked ) {
    processor_setup env(false);

    uint8_t frame[130] = {0x82, 0x7e, 0x00, 0x7e};
    frame[0] = 0x82;
    frame[1] = 0x7e;
    frame[2] = 0x00;
    frame[3] = 0x7e;
    std::fill_n(frame+4,126,0x2a);

    boost_check_equal( env.p.get_message(), message_ptr() );
    boost_check_equal( env.p.consume(frame,130,env.ec), 130 );
    boost_check( !env.ec );
    boost_check_equal( env.p.ready(), true );

    message_ptr foo = env.p.get_message();

    boost_check_equal( env.p.get_message(), message_ptr() );
    boost_check_equal( foo->get_payload().size(), 126 );
}

boost_auto_test_case( frame_jumbo_binary_unmasked ) {
    processor_setup env(false);

    uint8_t frame[130] = {0x82, 0x7e, 0x00, 0x7e};
    std::fill_n(frame+4,126,0x2a);

    boost_check_equal( env.p.get_message(), message_ptr() );
    boost_check_equal( env.p.consume(frame,130,env.ec), 130 );
    boost_check( !env.ec );
    boost_check_equal( env.p.ready(), true );

    message_ptr foo = env.p.get_message();

    boost_check_equal( env.p.get_message(), message_ptr() );
    boost_check_equal( foo->get_payload().size(), 126 );
}

boost_auto_test_case( control_frame_too_large ) {
    processor_setup env(false);

    uint8_t frame[130] = {0x88, 0x7e, 0x00, 0x7e};
    std::fill_n(frame+4,126,0x2a);

    boost_check_equal( env.p.get_message(), message_ptr() );
    boost_check_gt( env.p.consume(frame,130,env.ec), 0 );
    boost_check_equal( env.ec, websocketpp::processor::error::control_too_big  );
    boost_check_equal( env.p.ready(), false );
}

boost_auto_test_case( rsv_bits_used ) {
    uint8_t frame[3][2] = {{0x90, 0x00},
                           {0xa0, 0x00},
                           {0xc0, 0x00}};

    for (int i = 0; i < 3; i++) {
        processor_setup env(false);

        boost_check_equal( env.p.get_message(), message_ptr() );
        boost_check_gt( env.p.consume(frame[i],2,env.ec), 0 );
        boost_check_equal( env.ec, websocketpp::processor::error::invalid_rsv_bit  );
        boost_check_equal( env.p.ready(), false );
    }
}


boost_auto_test_case( reserved_opcode_used ) {
    uint8_t frame[10][2] = {{0x83, 0x00},
                            {0x84, 0x00},
                            {0x85, 0x00},
                            {0x86, 0x00},
                            {0x87, 0x00},
                            {0x8b, 0x00},
                            {0x8c, 0x00},
                            {0x8d, 0x00},
                            {0x8e, 0x00},
                            {0x8f, 0x00}};

    for (int i = 0; i < 10; i++) {
        processor_setup env(false);

        boost_check_equal( env.p.get_message(), message_ptr() );
        boost_check_gt( env.p.consume(frame[i],2,env.ec), 0 );
        boost_check_equal( env.ec, websocketpp::processor::error::invalid_opcode  );
        boost_check_equal( env.p.ready(), false );
    }
}

boost_auto_test_case( fragmented_control_message ) {
    processor_setup env(false);

    uint8_t frame[2] = {0x08, 0x00};

    boost_check_equal( env.p.get_message(), message_ptr() );
    boost_check_gt( env.p.consume(frame,2,env.ec), 0 );
    boost_check_equal( env.ec, websocketpp::processor::error::fragmented_control );
    boost_check_equal( env.p.ready(), false );
}

boost_auto_test_case( fragmented_binary_message ) {
    processor_setup env0(false);
    processor_setup env1(false);

    uint8_t frame0[6] = {0x02, 0x01, 0x2a, 0x80, 0x01, 0x2a};
    uint8_t frame1[8] = {0x02, 0x01, 0x2a, 0x89, 0x00, 0x80, 0x01, 0x2a};

    // read fragmented message in one chunk
    boost_check_equal( env0.p.get_message(), message_ptr() );
    boost_check_equal( env0.p.consume(frame0,6,env0.ec), 6 );
    boost_check( !env0.ec );
    boost_check_equal( env0.p.ready(), true );
    boost_check_equal( env0.p.get_message()->get_payload(), "**" );

    // read fragmented message in two chunks
    boost_check_equal( env0.p.get_message(), message_ptr() );
    boost_check_equal( env0.p.consume(frame0,3,env0.ec), 3 );
    boost_check( !env0.ec );
    boost_check_equal( env0.p.ready(), false );
    boost_check_equal( env0.p.consume(frame0+3,3,env0.ec), 3 );
    boost_check( !env0.ec );
    boost_check_equal( env0.p.ready(), true );
    boost_check_equal( env0.p.get_message()->get_payload(), "**" );

    // read fragmented message with control message in between
    boost_check_equal( env0.p.get_message(), message_ptr() );
    boost_check_equal( env0.p.consume(frame1,8,env0.ec), 5 );
    boost_check( !env0.ec );
    boost_check_equal( env0.p.ready(), true );
    boost_check_equal( env0.p.get_message()->get_opcode(), websocketpp::frame::opcode::ping);
    boost_check_equal( env0.p.consume(frame1+5,3,env0.ec), 3 );
    boost_check( !env0.ec );
    boost_check_equal( env0.p.ready(), true );
    boost_check_equal( env0.p.get_message()->get_payload(), "**" );

    // read lone continuation frame
    boost_check_equal( env0.p.get_message(), message_ptr() );
    boost_check_gt( env0.p.consume(frame0+3,3,env0.ec), 0);
    boost_check_equal( env0.ec, websocketpp::processor::error::invalid_continuation );

    // read two start frames in a row
    boost_check_equal( env1.p.get_message(), message_ptr() );
    boost_check_equal( env1.p.consume(frame0,3,env1.ec), 3);
    boost_check( !env1.ec );
    boost_check_gt( env1.p.consume(frame0,3,env1.ec), 0);
    boost_check_equal( env1.ec, websocketpp::processor::error::invalid_continuation );
}

boost_auto_test_case( unmasked_client_frame ) {
    processor_setup env(true);

    uint8_t frame[2] = {0x82, 0x00};

    boost_check_equal( env.p.get_message(), message_ptr() );
    boost_check_gt( env.p.consume(frame,2,env.ec), 0 );
    boost_check_equal( env.ec, websocketpp::processor::error::masking_required );
    boost_check_equal( env.p.ready(), false );
}

boost_auto_test_case( masked_server_frame ) {
    processor_setup env(false);

    uint8_t frame[8] = {0x82, 0x82, 0xff, 0xff, 0xff, 0xff, 0xd5, 0xd5};

    boost_check_equal( env.p.get_message(), message_ptr() );
    boost_check_gt( env.p.consume(frame,8,env.ec), 0 );
    boost_check_equal( env.ec, websocketpp::processor::error::masking_forbidden );
    boost_check_equal( env.p.ready(), false );
}

boost_auto_test_case( frame_small_binary_masked ) {
    processor_setup env(true);

    uint8_t frame[8] = {0x82, 0x82, 0xff, 0xff, 0xff, 0xff, 0xd5, 0xd5};

    boost_check_equal( env.p.get_message(), message_ptr() );
    boost_check_equal( env.p.consume(frame,8,env.ec), 8 );
    boost_check( !env.ec );
    boost_check_equal( env.p.ready(), true );
    boost_check_equal( env.p.get_message()->get_payload(), "**" );
}

boost_auto_test_case( masked_fragmented_binary_message ) {
    processor_setup env(true);

    uint8_t frame0[14] = {0x02, 0x81, 0xab, 0x23, 0x98, 0x45, 0x81,
                         0x80, 0x81, 0xb8, 0x34, 0x12, 0xff, 0x92};

    // read fragmented message in one chunk
    boost_check_equal( env.p.get_message(), message_ptr() );
    boost_check_equal( env.p.consume(frame0,14,env.ec), 14 );
    boost_check( !env.ec );
    boost_check_equal( env.p.ready(), true );
    boost_check_equal( env.p.get_message()->get_payload(), "**" );
}

boost_auto_test_case( prepare_data_frame ) {
    processor_setup env(true);

    message_ptr in = env.msg_manager->get_message();
    message_ptr out = env.msg_manager->get_message();
    message_ptr invalid;

    // empty pointers arguements should return sane error
    boost_check_equal( env.p.prepare_data_frame(invalid,invalid), websocketpp::processor::error::invalid_arguments );

    boost_check_equal( env.p.prepare_data_frame(in,invalid), websocketpp::processor::error::invalid_arguments );

    boost_check_equal( env.p.prepare_data_frame(invalid,out), websocketpp::processor::error::invalid_arguments );

    // test valid opcodes
    // control opcodes should return an error, data ones shouldn't
    for (int i = 0; i < 0xf; i++) {
        in->set_opcode(websocketpp::frame::opcode::value(i));

        env.ec = env.p.prepare_data_frame(in,out);

        if (websocketpp::frame::opcode::is_control(in->get_opcode())) {
            boost_check_equal( env.ec, websocketpp::processor::error::invalid_opcode );
        } else {
            boost_check_ne( env.ec, websocketpp::processor::error::invalid_opcode );
        }
    }


    //in.set_payload("foo");

    //e = prepare_data_frame(in,out);


}

boost_auto_test_case( single_frame_message_too_large ) {
    processor_setup env(true);
    
    env.p.set_max_message_size(3);
    
    uint8_t frame0[10] = {0x82, 0x84, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01};

    // read message that is one byte too large
    boost_check_equal( env.p.consume(frame0,10,env.ec), 6 );
    boost_check_equal( env.ec, websocketpp::processor::error::message_too_big );
}

boost_auto_test_case( multiple_frame_message_too_large ) {
    processor_setup env(true);
    
    env.p.set_max_message_size(4);
    
    uint8_t frame0[8] = {0x02, 0x82, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01};
    uint8_t frame1[9] = {0x80, 0x83, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01};

    // read first message frame with size under the limit
    boost_check_equal( env.p.consume(frame0,8,env.ec), 8 );
    boost_check( !env.ec );
    
    // read second message frame that puts the size over the limit
    boost_check_equal( env.p.consume(frame1,9,env.ec), 6 );
    boost_check_equal( env.ec, websocketpp::processor::error::message_too_big );
}



boost_auto_test_case( client_handshake_request ) {
    processor_setup env(false);

    websocketpp::uri_ptr u(new websocketpp::uri("ws://localhost/"));

    env.p.client_handshake_request(env.req,u, std::vector<std::string>());

    boost_check_equal( env.req.get_method(), "get" );
    boost_check_equal( env.req.get_version(), "http/1.1");
    boost_check_equal( env.req.get_uri(), "/");

    boost_check_equal( env.req.get_header("host"), "localhost");
    boost_check_equal( env.req.get_header("sec-websocket-version"), "13");
    boost_check_equal( env.req.get_header("connection"), "upgrade");
    boost_check_equal( env.req.get_header("upgrade"), "websocket");
}

// todo:
// test cases
// - adding headers
// - adding upgrade header
// - adding connection header
// - adding sec-websocket-version, sec-websocket-key, or host header
// - other sec* headers?
// - user agent header?

// origin support
// subprotocol requests

//websocketpp::uri_ptr u(new websocketpp::uri("ws://localhost/"));
    //env.p.client_handshake_request(env.req,u);

boost_auto_test_case( client_handshake_response_404 ) {
    processor_setup env(false);

    std::string res = "http/1.1 404 not found\r\n\r\n";
    env.res.consume(res.data(),res.size());

    boost_check_equal( env.p.validate_server_handshake_response(env.req,env.res), websocketpp::processor::error::invalid_http_status );
}

boost_auto_test_case( client_handshake_response_no_upgrade ) {
    processor_setup env(false);

    std::string res = "http/1.1 101 switching protocols\r\n\r\n";
    env.res.consume(res.data(),res.size());

    boost_check_equal( env.p.validate_server_handshake_response(env.req,env.res), websocketpp::processor::error::missing_required_header );
}

boost_auto_test_case( client_handshake_response_no_connection ) {
    processor_setup env(false);

    std::string res = "http/1.1 101 switching protocols\r\nupgrade: foo, websocket\r\n\r\n";
    env.res.consume(res.data(),res.size());

    boost_check_equal( env.p.validate_server_handshake_response(env.req,env.res), websocketpp::processor::error::missing_required_header );
}

boost_auto_test_case( client_handshake_response_no_accept ) {
    processor_setup env(false);

    std::string res = "http/1.1 101 switching protocols\r\nupgrade: foo, websocket\r\nconnection: bar, upgrade\r\n\r\n";
    env.res.consume(res.data(),res.size());

    boost_check_equal( env.p.validate_server_handshake_response(env.req,env.res), websocketpp::processor::error::missing_required_header );
}

boost_auto_test_case( client_handshake_response ) {
    processor_setup env(false);

    env.req.append_header("sec-websocket-key", "dghlihnhbxbszsbub25jzq==");

    std::string res = "http/1.1 101 switching protocols\r\nupgrade: foo, websocket\r\nconnection: bar, upgrade\r\nsec-websocket-accept: s3pplmbitxaq9kygzzhzrbk+xoo=\r\n\r\n";
    env.res.consume(res.data(),res.size());

    boost_check( !env.p.validate_server_handshake_response(env.req,env.res) );
}

boost_auto_test_case( extensions_disabled ) {
    processor_setup env(true);

    env.req.replace_header("sec-websocket-extensions","");

    std::pair<websocketpp::lib::error_code,std::string> neg_results;
    neg_results = env.p.negotiate_extensions(env.req);

    boost_check_equal( neg_results.first, websocketpp::processor::error::extensions_disabled );
    boost_check_equal( neg_results.second, "" );
}

boost_auto_test_case( extension_negotiation_blank ) {
    processor_setup_ext env(true);

    env.req.replace_header("sec-websocket-extensions","");

    std::pair<websocketpp::lib::error_code,std::string> neg_results;
    neg_results = env.p.negotiate_extensions(env.req);

    boost_check( !neg_results.first );
    boost_check_equal( neg_results.second, "" );
}

boost_auto_test_case( extension_negotiation_unknown ) {
    processor_setup_ext env(true);

    env.req.replace_header("sec-websocket-extensions","foo");

    std::pair<websocketpp::lib::error_code,std::string> neg_results;
    neg_results = env.p.negotiate_extensions(env.req);

    boost_check( !neg_results.first );
    boost_check_equal( neg_results.second, "" );
}

boost_auto_test_case( extract_subprotocols_empty ) {
    processor_setup env(true);
    std::vector<std::string> subps;

    boost_check( !env.p.extract_subprotocols(env.req,subps) );
    boost_check_equal( subps.size(), 0 );
}

boost_auto_test_case( extract_subprotocols_one ) {
    processor_setup env(true);
    std::vector<std::string> subps;

    env.req.replace_header("sec-websocket-protocol","foo");

    boost_check( !env.p.extract_subprotocols(env.req,subps) );
    boost_require_equal( subps.size(), 1 );
    boost_check_equal( subps[0], "foo" );
}

boost_auto_test_case( extract_subprotocols_multiple ) {
    processor_setup env(true);
    std::vector<std::string> subps;

    env.req.replace_header("sec-websocket-protocol","foo,bar");

    boost_check( !env.p.extract_subprotocols(env.req,subps) );
    boost_require_equal( subps.size(), 2 );
    boost_check_equal( subps[0], "foo" );
    boost_check_equal( subps[1], "bar" );
}

boost_auto_test_case( extract_subprotocols_invalid) {
    processor_setup env(true);
    std::vector<std::string> subps;

    env.req.replace_header("sec-websocket-protocol","foo,bar,,,,");

    boost_check_equal( env.p.extract_subprotocols(env.req,subps), websocketpp::processor::error::make_error_code(websocketpp::processor::error::subprotocol_parse_error) );
    boost_check_equal( subps.size(), 0 );
}

boost_auto_test_case( extension_negotiation_permessage_deflate ) {
    processor_setup_ext env(true);

    env.req.replace_header("sec-websocket-extensions",
        "permessage-deflate; c2s_max_window_bits");

    std::pair<websocketpp::lib::error_code,std::string> neg_results;
    neg_results = env.p.negotiate_extensions(env.req);

    boost_check( !neg_results.first );
    boost_check_equal( neg_results.second, "permessage-deflate" );
}

