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
#define boost_test_module processors
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/processors/processor.hpp>
#include <websocketpp/http/request.hpp>

boost_auto_test_case( exact_match ) {
    websocketpp::http::parser::request r;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::is_websocket_handshake(r));
}

boost_auto_test_case( non_match ) {
    websocketpp::http::parser::request r;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\n\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(!websocketpp::processor::is_websocket_handshake(r));
}

boost_auto_test_case( ci_exact_match ) {
    websocketpp::http::parser::request r;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade\r\nupgrade: websocket\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::is_websocket_handshake(r));
}

boost_auto_test_case( non_exact_match1 ) {
    websocketpp::http::parser::request r;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: upgrade,foo\r\nupgrade: websocket,foo\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::is_websocket_handshake(r));
}

boost_auto_test_case( non_exact_match2 ) {
    websocketpp::http::parser::request r;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nconnection: keep-alive,upgrade,foo\r\nupgrade: foo,websocket,bar\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::is_websocket_handshake(r));
}

boost_auto_test_case( version_blank ) {
    websocketpp::http::parser::request r;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nupgrade: websocket\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::get_websocket_version(r) == 0);
}

boost_auto_test_case( version_7 ) {
    websocketpp::http::parser::request r;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nupgrade: websocket\r\nsec-websocket-version: 7\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::get_websocket_version(r) == 7);
}

boost_auto_test_case( version_8 ) {
    websocketpp::http::parser::request r;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nupgrade: websocket\r\nsec-websocket-version: 8\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::get_websocket_version(r) == 8);
}

boost_auto_test_case( version_13 ) {
    websocketpp::http::parser::request r;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nupgrade: websocket\r\nsec-websocket-version: 13\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::get_websocket_version(r) == 13);
}

boost_auto_test_case( version_non_numeric ) {
    websocketpp::http::parser::request r;

    std::string handshake = "get / http/1.1\r\nhost: www.example.com\r\nupgrade: websocket\r\nsec-websocket-version: abc\r\n\r\n";

    r.consume(handshake.c_str(),handshake.size());

    boost_check(websocketpp::processor::get_websocket_version(r) == -1);
}