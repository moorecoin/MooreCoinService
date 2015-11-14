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
#define boost_test_module hybi_00_processor
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/processors/hybi00.hpp>
#include <websocketpp/http/request.hpp>
#include <websocketpp/http/response.hpp>


boost_auto_test_case( exact_match ) {
    websocketpp::http::parser::request r;
    websocketpp::http::parser::response response;
    websocketpp::processor::hybi00<websocketpp::http::parser::request,websocketpp::http::parser::response> p(false);

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\norigin: http://example.com\r\nsec-websocket-key1: 3e6b263  4 17 80\r\nsec-websocket-key2: 17  9 g`zd9   2 2b 7x 3 /r90\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());
    r.replace_header("sec-websocket-key3","wjn}|m(6");

    boost_check(websocketpp::processor::is_websocket_handshake(r));
    boost_check(websocketpp::processor::get_websocket_version(r) == p.get_version());
    boost_check(p.validate_handshake(r));

    websocketpp::uri_ptr u;
    bool exception;

    try {
        u = p.get_uri(r);
    } catch (const websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check(exception == false);
    boost_check(u->get_secure() == false);
    boost_check(u->get_host() == "www.example.com");
    boost_check(u->get_resource() == "/");
    boost_check(u->get_port() == websocketpp::uri_default_port);

    p.process_handshake(r,response);

    boost_check(response.get_header("connection") == "upgrade");
    boost_check(response.get_header("upgrade") == "websocket");
    boost_check(response.get_header("sec-websocket-origin") == "http://example.com");

    boost_check(response.get_header("sec-websocket-location") == "ws://www.example.com/");
    boost_check(response.get_header("sec-websocket-key3") == "n`9ebk9z$r8potvb");
}

boost_auto_test_case( non_get_method ) {
    websocketpp::http::parser::request r;
    websocketpp::processor::hybi00<websocketpp::http::parser::request,websocketpp::http::parser::response> p(false);

    std::string handshake = "post / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-key1: 3e6b263  4 17 80\r\nsec-websocket-key2: 17  9 g`zd9   2 2b 7x 3 /r90\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());
    r.replace_header("sec-websocket-key3","janelle!");

    boost_check(websocketpp::processor::is_websocket_handshake(r));
    boost_check(websocketpp::processor::get_websocket_version(r) == p.get_version());
    boost_check(!p.validate_handshake(r));
}

boost_auto_test_case( old_http_version ) {
    websocketpp::http::parser::request r;
    websocketpp::processor::hybi00<websocketpp::http::parser::request,websocketpp::http::parser::response> p(false);

    std::string handshake = "get / http/1.0\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-key1: 3e6b263  4 17 80\r\nsec-websocket-key2: 17  9 g`zd9   2 2b 7x 3 /r90\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());
    r.replace_header("sec-websocket-key3","janelle!");

    boost_check(websocketpp::processor::is_websocket_handshake(r));
    boost_check(websocketpp::processor::get_websocket_version(r) == p.get_version());
    boost_check(!p.validate_handshake(r));
}

boost_auto_test_case( missing_handshake_key1 ) {
    websocketpp::http::parser::request r;
    websocketpp::processor::hybi00<websocketpp::http::parser::request,websocketpp::http::parser::response> p(false);

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-key1: 3e6b263  4 17 80\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());
    r.replace_header("sec-websocket-key3","janelle!");

    boost_check(websocketpp::processor::is_websocket_handshake(r));
    boost_check(websocketpp::processor::get_websocket_version(r) == p.get_version());
    boost_check(!p.validate_handshake(r));
}

boost_auto_test_case( missing_handshake_key2 ) {
    websocketpp::http::parser::request r;
    websocketpp::processor::hybi00<websocketpp::http::parser::request,websocketpp::http::parser::response> p(false);

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-key2: 17  9 g`zd9   2 2b 7x 3 /r90\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());
    r.replace_header("sec-websocket-key3","janelle!");

    boost_check(websocketpp::processor::is_websocket_handshake(r));
    boost_check(websocketpp::processor::get_websocket_version(r) == p.get_version());
    boost_check(!p.validate_handshake(r));
}

boost_auto_test_case( bad_host ) {
    websocketpp::http::parser::request r;
    websocketpp::processor::hybi00<websocketpp::http::parser::request,websocketpp::http::parser::response> p(false);
    websocketpp::uri_ptr u;
    bool exception = false;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com:70000\r\nconnection: upgrade\r\nupgrade: websocket\r\nsec-websocket-key2: 17  9 g`zd9   2 2b 7x 3 /r90\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());
    r.replace_header("sec-websocket-key3","janelle!");

    boost_check(websocketpp::processor::is_websocket_handshake(r));
    boost_check(websocketpp::processor::get_websocket_version(r) == p.get_version());
    boost_check(!p.validate_handshake(r));

    try {
        u = p.get_uri(r);
    } catch (const websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check(exception == true);
}
