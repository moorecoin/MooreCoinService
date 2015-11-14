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
#define boost_test_module hybi_08_processor
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/processors/hybi08.hpp>
#include <websocketpp/http/request.hpp>
#include <websocketpp/http/response.hpp>
#include <websocketpp/message_buffer/message.hpp>
#include <websocketpp/message_buffer/alloc.hpp>
#include <websocketpp/extensions/permessage_deflate/disabled.hpp>
#include <websocketpp/random/none.hpp>

struct stub_config {
    typedef websocketpp::http::parser::request request_type;
    typedef websocketpp::http::parser::response response_type;

    typedef websocketpp::message_buffer::message
        <websocketpp::message_buffer::alloc::con_msg_manager> message_type;
    typedef websocketpp::message_buffer::alloc::con_msg_manager<message_type>
        con_msg_manager_type;

    typedef websocketpp::random::none::int_generator<uint32_t> rng_type;

    static const size_t max_message_size = 16000000;

    /// extension related config
    static const bool enable_extensions = false;

    /// extension specific config

    /// permessage_deflate_config
    struct permessage_deflate_config {
        typedef stub_config::request_type request_type;
    };

    typedef websocketpp::extensions::permessage_deflate::disabled
        <permessage_deflate_config> permessage_deflate_type;
};

boost_auto_test_case( exact_match ) {
    stub_config::request_type r;
    stub_config::response_type response;
    stub_config::con_msg_manager_type::ptr msg_manager;
    stub_config::rng_type rng;
    websocketpp::processor::hybi08<stub_config> p(false,true,msg_manager,rng);
    websocketpp::lib::error_code ec;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 8\r\nsec-websocket-key: dghlihnhbxbszsbub25jzq==\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::is_websocket_handshake(r));
    boost_check(websocketpp::processor::get_websocket_version(r) == p.get_version());
    ec = p.validate_handshake(r);
    boost_check(!ec);

    websocketpp::uri_ptr u;

    u = p.get_uri(r);

    boost_check(u->get_valid() == true);
    boost_check(u->get_secure() == false);
    boost_check(u->get_host() == "www.example.com");
    boost_check(u->get_resource() == "/");
    boost_check(u->get_port() == websocketpp::uri_default_port);

    p.process_handshake(r,"",response);

    boost_check(response.get_header("connection") == "upgrade");
    boost_check(response.get_header("upgrade") == "websocket");
    boost_check(response.get_header("sec-websocket-accept") == "s3pplmbitxaq9kygzzhzrbk+xoo=");
}

boost_auto_test_case( non_get_method ) {
    stub_config::request_type r;
    stub_config::response_type response;
    stub_config::rng_type rng;
    stub_config::con_msg_manager_type::ptr msg_manager;
    websocketpp::processor::hybi08<stub_config> p(false,true,msg_manager,rng);
    websocketpp::lib::error_code ec;

    std::string handshake = "post / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 8\r\nsec-websocket-key: foo\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::is_websocket_handshake(r));
    boost_check(websocketpp::processor::get_websocket_version(r) == p.get_version());
    ec = p.validate_handshake(r);
    boost_check( ec == websocketpp::processor::error::invalid_http_method );
}

boost_auto_test_case( old_http_version ) {
    stub_config::request_type r;
    stub_config::response_type response;
    stub_config::con_msg_manager_type::ptr msg_manager;
    stub_config::rng_type rng;
    websocketpp::processor::hybi08<stub_config> p(false,true,msg_manager,rng);
    websocketpp::lib::error_code ec;

    std::string handshake = "get / http/1.0\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 8\r\nsec-websocket-key: foo\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::is_websocket_handshake(r));
    boost_check(websocketpp::processor::get_websocket_version(r) == p.get_version());
    ec = p.validate_handshake(r);
    boost_check( ec == websocketpp::processor::error::invalid_http_version );
}

boost_auto_test_case( missing_handshake_key1 ) {
    stub_config::request_type r;
    stub_config::response_type response;
    stub_config::con_msg_manager_type::ptr msg_manager;
    stub_config::rng_type rng;
    websocketpp::processor::hybi08<stub_config> p(false,true,msg_manager,rng);
    websocketpp::lib::error_code ec;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 8\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::is_websocket_handshake(r));
    boost_check(websocketpp::processor::get_websocket_version(r) == p.get_version());
    ec = p.validate_handshake(r);
    boost_check( ec == websocketpp::processor::error::missing_required_header );
}

boost_auto_test_case( missing_handshake_key2 ) {
    stub_config::request_type r;
    stub_config::response_type response;
    stub_config::con_msg_manager_type::ptr msg_manager;
    stub_config::rng_type rng;
    websocketpp::processor::hybi08<stub_config> p(false,true,msg_manager,rng);
    websocketpp::lib::error_code ec;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 8\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::is_websocket_handshake(r));
    boost_check(websocketpp::processor::get_websocket_version(r) == p.get_version());
    ec = p.validate_handshake(r);
    boost_check( ec == websocketpp::processor::error::missing_required_header );
}

boost_auto_test_case( bad_host ) {
    stub_config::request_type r;
    stub_config::response_type response;
    stub_config::con_msg_manager_type::ptr msg_manager;
    stub_config::rng_type rng;
    websocketpp::processor::hybi08<stub_config> p(false,true,msg_manager,rng);
    websocketpp::uri_ptr u;
    websocketpp::lib::error_code ec;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com:70000\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-version: 8\r\nsec-websocket-key: foo\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::is_websocket_handshake(r));
    boost_check(websocketpp::processor::get_websocket_version(r) == p.get_version());
    ec = p.validate_handshake(r);
    boost_check( !ec );

    u = p.get_uri(r);

    boost_check( !u->get_valid() );
}

