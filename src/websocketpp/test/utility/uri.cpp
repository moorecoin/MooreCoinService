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
#define boost_test_module uri
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/uri.hpp>

// test a regular valid ws uri
boost_auto_test_case( uri_valid ) {
    websocketpp::uri uri("ws://localhost:9000/chat");

    boost_check( uri.get_valid() );
    boost_check( !uri.get_secure() );
    boost_check_equal( uri.get_scheme(), "ws");
    boost_check_equal( uri.get_host(), "localhost");
    boost_check_equal( uri.get_port(), 9000 );
    boost_check_equal( uri.get_resource(), "/chat" );
    boost_check_equal( uri.get_query(), "" );
}

// test a regular valid ws uri
boost_auto_test_case( uri_valid_no_port_unsecure ) {
    websocketpp::uri uri("ws://localhost/chat");

    boost_check( uri.get_valid() );
    boost_check( !uri.get_secure() );
    boost_check_equal( uri.get_scheme(), "ws");
    boost_check_equal( uri.get_host(), "localhost");
    boost_check_equal( uri.get_port(), 80 );
    boost_check_equal( uri.get_resource(), "/chat" );
}

// valid uri with no port (secure)
boost_auto_test_case( uri_valid_no_port_secure ) {
    websocketpp::uri uri("wss://localhost/chat");

    boost_check( uri.get_valid() );
    boost_check( uri.get_secure() );
    boost_check_equal( uri.get_scheme(), "wss");
    boost_check_equal( uri.get_host(), "localhost");
    boost_check_equal( uri.get_port(), 443 );
    boost_check_equal( uri.get_resource(), "/chat" );
}

// valid uri with no resource
boost_auto_test_case( uri_valid_no_resource ) {
    websocketpp::uri uri("wss://localhost:9000");

    boost_check( uri.get_valid() );
    boost_check( uri.get_secure() );
    boost_check_equal( uri.get_scheme(), "wss");
    boost_check_equal( uri.get_host(), "localhost");
    boost_check_equal( uri.get_port(), 9000 );
    boost_check_equal( uri.get_resource(), "/" );
}

// valid uri ipv6 literal
boost_auto_test_case( uri_valid_ipv6_literal ) {
    websocketpp::uri uri("wss://[::1]:9000/chat");

    boost_check( uri.get_valid() );
    boost_check( uri.get_secure() );
    boost_check_equal( uri.get_scheme(), "wss");
    boost_check_equal( uri.get_host(), "::1");
    boost_check_equal( uri.get_port(), 9000 );
    boost_check_equal( uri.get_resource(), "/chat" );
}

// valid uri with more complicated host
boost_auto_test_case( uri_valid_2 ) {
    websocketpp::uri uri("wss://thor-websocket.zaphoyd.net:88/");

    boost_check( uri.get_valid() );
    boost_check( uri.get_secure() );
    boost_check_equal( uri.get_scheme(), "wss");
    boost_check_equal( uri.get_host(), "thor-websocket.zaphoyd.net");
    boost_check_equal( uri.get_port(), 88 );
    boost_check_equal( uri.get_resource(), "/" );
}


// invalid uri (port too long)
boost_auto_test_case( uri_invalid_long_port ) {
    websocketpp::uri uri("wss://localhost:900000/chat");

    boost_check( !uri.get_valid() );
}

// invalid uri (bogus scheme method)
boost_auto_test_case( uri_invalid_scheme ) {
    websocketpp::uri uri("foo://localhost:9000/chat");

    boost_check( !uri.get_valid() );
}

// valid uri (http method)
boost_auto_test_case( uri_http_scheme ) {
    websocketpp::uri uri("http://localhost:9000/chat");

    boost_check( uri.get_valid() );
    boost_check( !uri.get_secure() );
    boost_check_equal( uri.get_scheme(), "http");
    boost_check_equal( uri.get_host(), "localhost");
    boost_check_equal( uri.get_port(), 9000 );
    boost_check_equal( uri.get_resource(), "/chat" );
}

// valid uri ipv4 literal
boost_auto_test_case( uri_valid_ipv4_literal ) {
    websocketpp::uri uri("wss://127.0.0.1:9000/chat");

    boost_check( uri.get_valid() );
    boost_check( uri.get_secure() );
    boost_check_equal( uri.get_scheme(), "wss");
    boost_check_equal( uri.get_host(), "127.0.0.1");
    boost_check_equal( uri.get_port(), 9000 );
    boost_check_equal( uri.get_resource(), "/chat" );
}

// valid uri complicated resource path
boost_auto_test_case( uri_valid_3 ) {
    websocketpp::uri uri("wss://localhost:9000/chat/foo/bar");

    boost_check( uri.get_valid() );
    boost_check( uri.get_secure() );
    boost_check_equal( uri.get_scheme(), "wss");
    boost_check_equal( uri.get_host(), "localhost");
    boost_check_equal( uri.get_port(), 9000 );
    boost_check_equal( uri.get_resource(), "/chat/foo/bar" );
}

// invalid uri broken method separator
boost_auto_test_case( uri_invalid_method_separator ) {
    websocketpp::uri uri("wss:/localhost:9000/chat");

    boost_check( !uri.get_valid() );
}

// invalid uri port > 65535
boost_auto_test_case( uri_invalid_gt_16_bit_port ) {
    websocketpp::uri uri("wss:/localhost:70000/chat");

    boost_check( !uri.get_valid() );
}

// invalid uri includes uri fragment
boost_auto_test_case( uri_invalid_fragment ) {
    websocketpp::uri uri("wss:/localhost:70000/chat#foo");

    boost_check( !uri.get_valid() );
}

// invalid uri with no brackets around ipv6 literal
boost_auto_test_case( uri_invalid_bad_v6_literal_1 ) {
    websocketpp::uri uri("wss://::1/chat");

    boost_check( !uri.get_valid() );
}

// invalid uri with port and no brackets around ipv6 literal
boost_auto_test_case( uri_invalid_bad_v6_literal_2 ) {
    websocketpp::uri uri("wss://::1:2009/chat");

    boost_check( !uri.get_valid() );
}

// valid uri complicated resource path with query
boost_auto_test_case( uri_valid_4 ) {
    websocketpp::uri uri("wss://localhost:9000/chat/foo/bar?foo=bar");

    boost_check( uri.get_valid() );
    boost_check( uri.get_secure() );
    boost_check_equal( uri.get_scheme(), "wss" );
    boost_check_equal( uri.get_host(), "localhost");
    boost_check_equal( uri.get_port(), 9000 );
    boost_check_equal( uri.get_resource(), "/chat/foo/bar?foo=bar" );
    boost_check_equal( uri.get_query(), "foo=bar" );
}

// valid uri with a mapped v4 ipv6 literal
boost_auto_test_case( uri_valid_v4_mapped ) {
    websocketpp::uri uri("wss://[0000:0000:0000:0000:0000:0000:192.168.1.1]:9000/");

    boost_check( uri.get_valid() );
    boost_check( uri.get_secure() );
    boost_check_equal( uri.get_scheme(), "wss" );
    boost_check_equal( uri.get_host(), "0000:0000:0000:0000:0000:0000:192.168.1.1");
    boost_check_equal( uri.get_port(), 9000 );
    boost_check_equal( uri.get_resource(), "/" );
}

// valid uri with a v6 address with mixed case
boost_auto_test_case( uri_valid_v6_mixed_case ) {
    websocketpp::uri uri("wss://[::10ab]:9000/");

    boost_check( uri.get_valid() == true );
    boost_check( uri.get_secure() == true );
    boost_check_equal( uri.get_scheme(), "wss" );
    boost_check_equal( uri.get_host(), "::10ab");
    boost_check_equal( uri.get_port(), 9000 );
    boost_check_equal( uri.get_resource(), "/" );
}

// valid uri with a v6 address with mixed case
boost_auto_test_case( uri_invalid_no_scheme ) {
    websocketpp::uri uri("myserver.com");

    boost_check( !uri.get_valid() );
}

// invalid ipv6 literal
/*boost_auto_test_case( uri_invalid_v6_nonhex ) {
    websocketpp::uri uri("wss://[g::1]:9000/");

    boost_check( uri.get_valid() == false );
}*/

// todo: tests for the other two constructors
