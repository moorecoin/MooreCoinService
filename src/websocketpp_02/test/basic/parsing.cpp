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
#define boost_test_module uri
#include <boost/test/unit_test.hpp>

#include <iostream>

#include "../../src/uri.hpp"

// test a regular valid ws uri
boost_auto_test_case( uri_valid ) {
    bool exception = false;
    try {
        websocketpp::uri uri("ws://localhost:9000/chat");
        
        boost_check( uri.get_secure() == false );
        boost_check( uri.get_host() == "localhost");
        boost_check( uri.get_port() == 9000 );
        boost_check( uri.get_resource() == "/chat" );
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check( exception == false);
}

// test a regular valid ws uri
boost_auto_test_case( uri_valid_no_port_unsecure ) {
    bool exception = false;
    try {
        websocketpp::uri uri("ws://localhost/chat");
        
        boost_check( uri.get_secure() == false );
        boost_check( uri.get_host() == "localhost");
        boost_check( uri.get_port() == 80 );
        boost_check( uri.get_resource() == "/chat" );
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check( exception == false);
}



// valid uri with no port (secure)
boost_auto_test_case( uri_valid_no_port_secure ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss://localhost/chat");
        
        boost_check( uri.get_secure() == true );
        boost_check( uri.get_host() == "localhost");
        boost_check( uri.get_port() == 443 );
        boost_check( uri.get_resource() == "/chat" );
    } catch (websocketpp::uri_exception& e) {
        exception = true;
        std::cout << e.what() << std::endl;
    }

    boost_check( exception == false);
}

// valid uri with no resource
boost_auto_test_case( uri_valid_no_resource ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss://localhost:9000");
        
        boost_check( uri.get_secure() == true );
        boost_check( uri.get_host() == "localhost");
        boost_check( uri.get_port() == 9000 );
        boost_check( uri.get_resource() == "/" );       
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check( exception == false);
}

// valid uri ipv6 literal
boost_auto_test_case( uri_valid_ipv6_literal ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss://[::1]:9000/chat");
        
        boost_check( uri.get_secure() == true );
        boost_check( uri.get_host() == "::1");
        boost_check( uri.get_port() == 9000 );
        boost_check( uri.get_resource() == "/chat" );       
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check( exception == false);
}



// valid uri with more complicated host
boost_auto_test_case( uri_valid_2 ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss://thor-websocket.zaphoyd.net:88/");
        
        boost_check( uri.get_secure() == true );
        boost_check( uri.get_host() == "thor-websocket.zaphoyd.net");
        boost_check( uri.get_port() == 88 );
        boost_check( uri.get_resource() == "/" );       
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check( exception == false);
}


// invalid uri (port too long)
boost_auto_test_case( uri_invalid_long_port ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss://localhost:900000/chat");        
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check( exception == true);
}

// invalid uri (http method)
boost_auto_test_case( uri_invalid_http ) {
    bool exception = false;
    try {
        websocketpp::uri uri("http://localhost:9000/chat");     
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check( exception == true);
}

// valid uri ipv4 literal
boost_auto_test_case( uri_valid_ipv4_literal ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss://127.0.0.1:9000/chat");
        
        boost_check( uri.get_secure() == true );
        boost_check( uri.get_host() == "127.0.0.1");
        boost_check( uri.get_port() == 9000 );
        boost_check( uri.get_resource() == "/chat" );       
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check( exception == false);
}

// valid uri complicated resource path
boost_auto_test_case( uri_valid_3 ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss://localhost:9000/chat/foo/bar");
        
        boost_check( uri.get_secure() == true );
        boost_check( uri.get_host() == "localhost");
        boost_check( uri.get_port() == 9000 );
        boost_check( uri.get_resource() == "/chat/foo/bar" );       
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check( exception == false);
}

// invalid uri broken method separator
boost_auto_test_case( uri_invalid_method_separator ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss:/localhost:9000/chat");       
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check( exception == true);
}

// invalid uri port > 65535
boost_auto_test_case( uri_invalid_gt_16_bit_port ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss:/localhost:70000/chat");      
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check( exception == true);
}

// invalid uri includes uri fragment
boost_auto_test_case( uri_invalid_fragment ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss:/localhost:70000/chat#foo");      
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check( exception == true);
}

// invalid uri with no brackets around ipv6 literal
boost_auto_test_case( uri_invalid_bad_v6_literal_1 ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss://::1/chat");     
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }
    
    boost_check( exception == true);
}

// invalid uri with port and no brackets around ipv6 literal 
boost_auto_test_case( uri_invalid_bad_v6_literal_2 ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss://::1:2009/chat");        
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }
    
    boost_check( exception == true);
}

// valid uri complicated resource path with query
boost_auto_test_case( uri_valid_4 ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss://localhost:9000/chat/foo/bar?foo=bar");
        
        boost_check( uri.get_secure() == true );
        boost_check( uri.get_host() == "localhost");
        boost_check( uri.get_port() == 9000 );
        boost_check( uri.get_resource() == "/chat/foo/bar?foo=bar" );       
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }

    boost_check( exception == false);
}

// valid uri with a mapped v4 ipv6 literal
boost_auto_test_case( uri_valid_v4_mapped ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss://[0000:0000:0000:0000:0000:0000:192.168.1.1]:9000/");
        
        boost_check( uri.get_secure() == true );
        boost_check( uri.get_host() == "0000:0000:0000:0000:0000:0000:192.168.1.1");
        boost_check( uri.get_port() == 9000 );
        boost_check( uri.get_resource() == "/" );       
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }
    
    boost_check( exception == false);
}

// valid uri with a v6 address with mixed case
boost_auto_test_case( uri_valid_v6_mixed_case ) {
    bool exception = false;
    try {
        websocketpp::uri uri("wss://[::10ab]:9000/");
        
        boost_check( uri.get_secure() == true );
        boost_check( uri.get_host() == "::10ab");
        boost_check( uri.get_port() == 9000 );
        boost_check( uri.get_resource() == "/" );       
    } catch (websocketpp::uri_exception& e) {
        exception = true;
    }
    
    boost_check( exception == false);
}

// todo: tests for the other two constructors
