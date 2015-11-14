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
#define boost_test_module http_parser
#include <boost/test/unit_test.hpp>

#include <string>

#include <websocketpp/http/request.hpp>
#include <websocketpp/http/response.hpp>

boost_auto_test_case( is_token_char ) {
    // valid characters

    // misc
    boost_check( websocketpp::http::is_token_char('!') );
    boost_check( websocketpp::http::is_token_char('#') );
    boost_check( websocketpp::http::is_token_char('$') );
    boost_check( websocketpp::http::is_token_char('%') );
    boost_check( websocketpp::http::is_token_char('&') );
    boost_check( websocketpp::http::is_token_char('\'') );
    boost_check( websocketpp::http::is_token_char('*') );
    boost_check( websocketpp::http::is_token_char('+') );
    boost_check( websocketpp::http::is_token_char('-') );
    boost_check( websocketpp::http::is_token_char('.') );
    boost_check( websocketpp::http::is_token_char('^') );
    boost_check( websocketpp::http::is_token_char('_') );
    boost_check( websocketpp::http::is_token_char('`') );
    boost_check( websocketpp::http::is_token_char('~') );

    // numbers
    for (int i = 0x30; i < 0x3a; i++) {
        boost_check( websocketpp::http::is_token_char((unsigned char)(i)) );
    }

    // upper
    for (int i = 0x41; i < 0x5b; i++) {
        boost_check( websocketpp::http::is_token_char((unsigned char)(i)) );
    }

    // lower
    for (int i = 0x61; i < 0x7b; i++) {
        boost_check( websocketpp::http::is_token_char((unsigned char)(i)) );
    }

    // invalid characters

    // lower unprintable
    for (int i = 0; i < 33; i++) {
        boost_check( !websocketpp::http::is_token_char((unsigned char)(i)) );
    }

    // misc
    boost_check( !websocketpp::http::is_token_char('(') );
    boost_check( !websocketpp::http::is_token_char(')') );
    boost_check( !websocketpp::http::is_token_char('<') );
    boost_check( !websocketpp::http::is_token_char('>') );
    boost_check( !websocketpp::http::is_token_char('@') );
    boost_check( !websocketpp::http::is_token_char(',') );
    boost_check( !websocketpp::http::is_token_char(';') );
    boost_check( !websocketpp::http::is_token_char(':') );
    boost_check( !websocketpp::http::is_token_char('\\') );
    boost_check( !websocketpp::http::is_token_char('"') );
    boost_check( !websocketpp::http::is_token_char('/') );
    boost_check( !websocketpp::http::is_token_char('[') );
    boost_check( !websocketpp::http::is_token_char(']') );
    boost_check( !websocketpp::http::is_token_char('?') );
    boost_check( !websocketpp::http::is_token_char('=') );
    boost_check( !websocketpp::http::is_token_char('{') );
    boost_check( !websocketpp::http::is_token_char('}') );

    // upper unprintable and out of ascii range
    for (int i = 127; i < 256; i++) {
        boost_check( !websocketpp::http::is_token_char((unsigned char)(i)) );
    }

    // is not
    boost_check( !websocketpp::http::is_not_token_char('!') );
    boost_check( websocketpp::http::is_not_token_char('(') );
}

boost_auto_test_case( extract_token ) {
    std::string d1 = "foo";
    std::string d2 = " foo ";

    std::pair<std::string,std::string::const_iterator> ret;

    ret = websocketpp::http::parser::extract_token(d1.begin(),d1.end());
    boost_check( ret.first == "foo" );
    boost_check( ret.second == d1.begin()+3 );

    ret = websocketpp::http::parser::extract_token(d2.begin(),d2.end());
    boost_check( ret.first == "" );
    boost_check( ret.second == d2.begin()+0 );

    ret = websocketpp::http::parser::extract_token(d2.begin()+1,d2.end());
    boost_check( ret.first == "foo" );
    boost_check( ret.second == d2.begin()+4 );
}

boost_auto_test_case( extract_quoted_string ) {
    std::string d1 = "\"foo\"";
    std::string d2 = "\"foo\\\"bar\\\"baz\"";
    std::string d3 = "\"foo\"     ";
    std::string d4 = "";
    std::string d5 = "foo";

    std::pair<std::string,std::string::const_iterator> ret;

    using websocketpp::http::parser::extract_quoted_string;

    ret = extract_quoted_string(d1.begin(),d1.end());
    boost_check( ret.first == "foo" );
    boost_check( ret.second == d1.end() );

    ret = extract_quoted_string(d2.begin(),d2.end());
    boost_check( ret.first == "foo\"bar\"baz" );
    boost_check( ret.second == d2.end() );

    ret = extract_quoted_string(d3.begin(),d3.end());
    boost_check( ret.first == "foo" );
    boost_check( ret.second == d3.begin()+5 );

    ret = extract_quoted_string(d4.begin(),d4.end());
    boost_check( ret.first == "" );
    boost_check( ret.second == d4.begin() );

    ret = extract_quoted_string(d5.begin(),d5.end());
    boost_check( ret.first == "" );
    boost_check( ret.second == d5.begin() );
}

boost_auto_test_case( extract_all_lws ) {
    std::string d1 = " foo     bar";
    d1.append(1,char(9));
    d1.append("baz\r\n d\r\n  \r\n  e\r\nf");

    std::string::const_iterator ret;

    ret = websocketpp::http::parser::extract_all_lws(d1.begin(),d1.end());
    boost_check( ret == d1.begin()+1 );

    ret = websocketpp::http::parser::extract_all_lws(d1.begin()+1,d1.end());
    boost_check( ret == d1.begin()+1 );

    ret = websocketpp::http::parser::extract_all_lws(d1.begin()+4,d1.end());
    boost_check( ret == d1.begin()+9 );

    ret = websocketpp::http::parser::extract_all_lws(d1.begin()+12,d1.end());
    boost_check( ret == d1.begin()+13 );

    ret = websocketpp::http::parser::extract_all_lws(d1.begin()+16,d1.end());
    boost_check( ret == d1.begin()+19 );

    ret = websocketpp::http::parser::extract_all_lws(d1.begin()+20,d1.end());
    boost_check( ret == d1.begin()+28 );

    ret = websocketpp::http::parser::extract_all_lws(d1.begin()+29,d1.end());
    boost_check( ret == d1.begin()+29 );
}

boost_auto_test_case( extract_attributes_blank ) {
    std::string s = "";

    websocketpp::http::attribute_list a;
    std::string::const_iterator it;

    it = websocketpp::http::parser::extract_attributes(s.begin(),s.end(),a);
    boost_check( it == s.begin() );
    boost_check_equal( a.size(), 0 );
}

boost_auto_test_case( extract_attributes_simple ) {
    std::string s = "foo";

    websocketpp::http::attribute_list a;
    std::string::const_iterator it;

    it = websocketpp::http::parser::extract_attributes(s.begin(),s.end(),a);
    boost_check( it == s.end() );
    boost_check_equal( a.size(), 1 );
    boost_check( a.find("foo") != a.end() );
    boost_check_equal( a.find("foo")->second, "" );
}

boost_auto_test_case( extract_parameters ) {
    std::string s1 = "";
    std::string s2 = "foo";
    std::string s3 = " foo \r\nabc";
    std::string s4 = "  \r\n   foo  ";
    std::string s5 = "foo,bar";
    std::string s6 = "foo;bar";
    std::string s7 = "foo;baz,bar";
    std::string s8 = "foo;bar;baz";
    std::string s9 = "foo;bar=baz";
    std::string s10 = "foo;bar=baz;boo";
    std::string s11 = "foo;bar=baz;boo,bob";
    std::string s12 = "foo;bar=\"a b c\"";
    std::string s13 = "foo;bar=\"a \\\"b\\\" c\"";


    std::string sx = "foo;bar=\"a \\\"b\\\" c\"";
    websocketpp::http::parameter_list p;
    websocketpp::http::attribute_list a;
    std::string::const_iterator it;

    using websocketpp::http::parser::extract_parameters;

    it = extract_parameters(s1.begin(),s1.end(),p);
    boost_check( it == s1.begin() );

    p.clear();
    it = extract_parameters(s2.begin(),s2.end(),p);
    boost_check( it == s2.end() );
    boost_check_equal( p.size(), 1 );
    boost_check( p[0].first == "foo" );
    boost_check_equal( p[0].second.size(), 0 );

    p.clear();
    it = extract_parameters(s3.begin(),s3.end(),p);
    boost_check( it == s3.begin()+5 );
    boost_check_equal( p.size(), 1 );
    boost_check( p[0].first == "foo" );
    boost_check_equal( p[0].second.size(), 0 );

    p.clear();
    it = extract_parameters(s4.begin(),s4.end(),p);
    boost_check( it == s4.end() );
    boost_check_equal( p.size(), 1 );
    boost_check( p[0].first == "foo" );
    boost_check_equal( p[0].second.size(), 0 );

    p.clear();
    it = extract_parameters(s5.begin(),s5.end(),p);
    boost_check( it == s5.end() );
    boost_check_equal( p.size(), 2 );
    boost_check( p[0].first == "foo" );
    boost_check_equal( p[0].second.size(), 0 );
    boost_check( p[1].first == "bar" );
    boost_check_equal( p[1].second.size(), 0 );

    p.clear();
    it = extract_parameters(s6.begin(),s6.end(),p);
    boost_check( it == s6.end() );
    boost_check_equal( p.size(), 1 );
    boost_check( p[0].first == "foo" );
    a = p[0].second;
    boost_check_equal( a.size(), 1 );
    boost_check( a.find("bar") != a.end() );
    boost_check_equal( a.find("bar")->second, "" );

    p.clear();
    it = extract_parameters(s7.begin(),s7.end(),p);
    boost_check( it == s7.end() );
    boost_check_equal( p.size(), 2 );
    boost_check( p[0].first == "foo" );
    a = p[0].second;
    boost_check_equal( a.size(), 1 );
    boost_check( a.find("baz") != a.end() );
    boost_check_equal( a.find("baz")->second, "" );
    boost_check( p[1].first == "bar" );
    a = p[1].second;
    boost_check_equal( a.size(), 0 );

    p.clear();
    it = extract_parameters(s8.begin(),s8.end(),p);
    boost_check( it == s8.end() );
    boost_check_equal( p.size(), 1 );
    boost_check( p[0].first == "foo" );
    a = p[0].second;
    boost_check_equal( a.size(), 2 );
    boost_check( a.find("bar") != a.end() );
    boost_check_equal( a.find("bar")->second, "" );
    boost_check( a.find("baz") != a.end() );
    boost_check_equal( a.find("baz")->second, "" );

    p.clear();
    it = extract_parameters(s9.begin(),s9.end(),p);
    boost_check( it == s9.end() );
    boost_check_equal( p.size(), 1 );
    boost_check( p[0].first == "foo" );
    a = p[0].second;
    boost_check_equal( a.size(), 1 );
    boost_check( a.find("bar") != a.end() );
    boost_check_equal( a.find("bar")->second, "baz" );

    p.clear();
    it = extract_parameters(s10.begin(),s10.end(),p);
    boost_check( it == s10.end() );
    boost_check_equal( p.size(), 1 );
    boost_check( p[0].first == "foo" );
    a = p[0].second;
    boost_check_equal( a.size(), 2 );
    boost_check( a.find("bar") != a.end() );
    boost_check_equal( a.find("bar")->second, "baz" );
    boost_check( a.find("boo") != a.end() );
    boost_check_equal( a.find("boo")->second, "" );

    p.clear();
    it = extract_parameters(s11.begin(),s11.end(),p);
    boost_check( it == s11.end() );
    boost_check_equal( p.size(), 2 );
    boost_check( p[0].first == "foo" );
    a = p[0].second;
    boost_check_equal( a.size(), 2 );
    boost_check( a.find("bar") != a.end() );
    boost_check_equal( a.find("bar")->second, "baz" );
    boost_check( a.find("boo") != a.end() );
    boost_check_equal( a.find("boo")->second, "" );
    a = p[1].second;
    boost_check_equal( a.size(), 0 );

    p.clear();
    it = extract_parameters(s12.begin(),s12.end(),p);
    boost_check( it == s12.end() );
    boost_check_equal( p.size(), 1 );
    boost_check( p[0].first == "foo" );
    a = p[0].second;
    boost_check_equal( a.size(), 1 );
    boost_check( a.find("bar") != a.end() );
    boost_check_equal( a.find("bar")->second, "a b c" );

    p.clear();
    it = extract_parameters(s13.begin(),s13.end(),p);
    boost_check( it == s13.end() );
    boost_check_equal( p.size(), 1 );
    boost_check( p[0].first == "foo" );
    a = p[0].second;
    boost_check_equal( a.size(), 1 );
    boost_check( a.find("bar") != a.end() );
    boost_check_equal( a.find("bar")->second, "a \"b\" c" );
}

boost_auto_test_case( strip_lws ) {
    std::string test1 = "foo";
    std::string test2 = " foo ";
    std::string test3 = "foo ";
    std::string test4 = " foo";
    std::string test5 = "    foo     ";
    std::string test6 = "  \r\n  foo     ";
    std::string test7 = "  \t  foo     ";
    std::string test8 = "  \t       ";

    boost_check_equal( websocketpp::http::parser::strip_lws(test1), "foo" );
    boost_check_equal( websocketpp::http::parser::strip_lws(test2), "foo" );
    boost_check_equal( websocketpp::http::parser::strip_lws(test3), "foo" );
    boost_check_equal( websocketpp::http::parser::strip_lws(test4), "foo" );
    boost_check_equal( websocketpp::http::parser::strip_lws(test5), "foo" );
    boost_check_equal( websocketpp::http::parser::strip_lws(test6), "foo" );
    boost_check_equal( websocketpp::http::parser::strip_lws(test7), "foo" );
    boost_check_equal( websocketpp::http::parser::strip_lws(test8), "" );
}

boost_auto_test_case( case_insensitive_headers ) {
    websocketpp::http::parser::parser r;

    r.replace_header("foo","bar");

    boost_check_equal( r.get_header("foo"), "bar" );
    boost_check_equal( r.get_header("foo"), "bar" );
    boost_check_equal( r.get_header("foo"), "bar" );
}

boost_auto_test_case( case_insensitive_headers_overwrite ) {
    websocketpp::http::parser::parser r;

    r.replace_header("foo","bar");

    boost_check_equal( r.get_header("foo"), "bar" );
    boost_check_equal( r.get_header("foo"), "bar" );

    r.replace_header("foo","baz");

    boost_check_equal( r.get_header("foo"), "baz" );
    boost_check_equal( r.get_header("foo"), "baz" );

    r.remove_header("foo");

    boost_check_equal( r.get_header("foo"), "" );
    boost_check_equal( r.get_header("foo"), "" );
}

boost_auto_test_case( blank_consume ) {
    websocketpp::http::parser::request r;

    std::string raw = "";

    bool exception = false;

    try {
        r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == false );
    boost_check( r.ready() == false );
}

boost_auto_test_case( blank_request ) {
    websocketpp::http::parser::request r;

    std::string raw = "\r\n\r\n";

    bool exception = false;

    try {
        r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == true );
    boost_check( r.ready() == false );
}

boost_auto_test_case( bad_request_no_host ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.1\r\n\r\n";

    bool exception = false;

    try {
        r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == true );
    boost_check( r.ready() == false );
}

boost_auto_test_case( basic_request ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.1\r\nhost: www.example.com\r\n\r\n";

    bool exception = false;
    size_t pos = 0;

    try {
        pos = r.consume(raw.c_str(),raw.size());
    } catch (std::exception &e) {
        exception = true;
        std::cout << e.what() << std::endl;
    }

    boost_check( exception == false );
    boost_check( pos == 41 );
    boost_check( r.ready() == true );
    boost_check( r.get_version() == "http/1.1" );
    boost_check( r.get_method() == "get" );
    boost_check( r.get_uri() == "/" );
    boost_check( r.get_header("host") == "www.example.com" );
}

boost_auto_test_case( trailing_body_characters ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.1\r\nhost: www.example.com\r\n\r\na";

    bool exception = false;
    size_t pos = 0;

    try {
        pos = r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == false );
    boost_check( pos == 41 );
    boost_check( r.ready() == true );
    boost_check( r.get_version() == "http/1.1" );
    boost_check( r.get_method() == "get" );
    boost_check( r.get_uri() == "/" );
    boost_check( r.get_header("host") == "www.example.com" );
}

boost_auto_test_case( basic_split1 ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.1\r\n";
    std::string raw2 = "host: www.example.com\r\n\r\na";

    bool exception = false;
    size_t pos = 0;

    try {
        pos += r.consume(raw.c_str(),raw.size());
        pos += r.consume(raw2.c_str(),raw2.size());
    } catch (std::exception &e) {
        exception = true;
        std::cout << e.what() << std::endl;
    }

    boost_check( exception == false );
    boost_check( pos == 41 );
    boost_check( r.ready() == true );
    boost_check( r.get_version() == "http/1.1" );
    boost_check( r.get_method() == "get" );
    boost_check( r.get_uri() == "/" );
    boost_check( r.get_header("host") == "www.example.com" );
}

boost_auto_test_case( basic_split2 ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.1\r\nhost: www.example.com\r";
    std::string raw2 = "\n\r\na";

    bool exception = false;
    size_t pos = 0;

    try {
        pos += r.consume(raw.c_str(),raw.size());
        pos += r.consume(raw2.c_str(),raw2.size());
    } catch (std::exception &e) {
        exception = true;
        std::cout << e.what() << std::endl;
    }

    boost_check( exception == false );
    boost_check( pos == 41 );
    boost_check( r.ready() == true );
    boost_check( r.get_version() == "http/1.1" );
    boost_check( r.get_method() == "get" );
    boost_check( r.get_uri() == "/" );
    boost_check( r.get_header("host") == "www.example.com" );
}

boost_auto_test_case( max_header_len ) {
    websocketpp::http::parser::request r;

    std::string raw(websocketpp::http::max_header_size+1,'*');

    bool exception = false;
    size_t pos = 0;

    try {
        pos += r.consume(raw.c_str(),raw.size());
    } catch (const websocketpp::http::exception& e) {
        if (e.m_error_code == websocketpp::http::status_code::request_header_fields_too_large) {
            exception = true;
        }
    }

    boost_check( exception == true );
}

boost_auto_test_case( max_header_len_split ) {
    websocketpp::http::parser::request r;

    std::string raw(websocketpp::http::max_header_size-1,'*');
    std::string raw2(2,'*');

    bool exception = false;
    size_t pos = 0;

    try {
        pos += r.consume(raw.c_str(),raw.size());
        pos += r.consume(raw2.c_str(),raw2.size());
    } catch (const websocketpp::http::exception& e) {
        if (e.m_error_code == websocketpp::http::status_code::request_header_fields_too_large) {
            exception = true;
        }
    }

    boost_check( exception == true );
}

boost_auto_test_case( firefox_full_request ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.1\r\nhost: localhost:5000\r\nuser-agent: mozilla/5.0 (macintosh; intel mac os x 10.7; rv:10.0) gecko/20100101 firefox/10.0\r\naccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\naccept-language: en-us,en;q=0.5\r\naccept-encoding: gzip, deflate\r\nconnection: keep-alive, upgrade\r\nsec-websocket-version: 8\r\nsec-websocket-origin: http://zaphoyd.com\r\nsec-websocket-key: pfik//fxwfk0rin4zipfjq==\r\npragma: no-cache\r\ncache-control: no-cache\r\nupgrade: websocket\r\n\r\n";

    bool exception = false;
    size_t pos = 0;

    try {
        pos += r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == false );
    boost_check( pos == 482 );
    boost_check( r.ready() == true );
    boost_check( r.get_version() == "http/1.1" );
    boost_check( r.get_method() == "get" );
    boost_check( r.get_uri() == "/" );
    boost_check( r.get_header("host") == "localhost:5000" );
    boost_check( r.get_header("user-agent") == "mozilla/5.0 (macintosh; intel mac os x 10.7; rv:10.0) gecko/20100101 firefox/10.0" );
    boost_check( r.get_header("accept") == "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8" );
    boost_check( r.get_header("accept-language") == "en-us,en;q=0.5" );
    boost_check( r.get_header("accept-encoding") == "gzip, deflate" );
    boost_check( r.get_header("connection") == "keep-alive, upgrade" );
    boost_check( r.get_header("sec-websocket-version") == "8" );
    boost_check( r.get_header("sec-websocket-origin") == "http://zaphoyd.com" );
    boost_check( r.get_header("sec-websocket-key") == "pfik//fxwfk0rin4zipfjq==" );
    boost_check( r.get_header("pragma") == "no-cache" );
    boost_check( r.get_header("cache-control") == "no-cache" );
    boost_check( r.get_header("upgrade") == "websocket" );
}

boost_auto_test_case( bad_method ) {
    websocketpp::http::parser::request r;

    std::string raw = "ge]t / http/1.1\r\nhost: www.example.com\r\n\r\n";

    bool exception = false;

    try {
        r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == true );
}

boost_auto_test_case( bad_header_name ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.1\r\nho]st: www.example.com\r\n\r\n";

    bool exception = false;

    try {
        r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == true );
}

boost_auto_test_case( old_http_version ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.0\r\nhost: www.example.com\r\n\r\n";

    bool exception = false;
    size_t pos = 0;

    try {
        pos = r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == false );
    boost_check_equal( pos, 41 );
    boost_check( r.ready() == true );
    boost_check_equal( r.get_version(), "http/1.0" );
    boost_check_equal( r.get_method(), "get" );
    boost_check_equal( r.get_uri(), "/" );
    boost_check_equal( r.get_header("host"), "www.example.com" );
}

boost_auto_test_case( new_http_version1 ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.12\r\nhost: www.example.com\r\n\r\n";

    bool exception = false;
    size_t pos = 0;

    try {
        pos = r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == false );
    boost_check_equal( pos, 42 );
    boost_check( r.ready() == true );
    boost_check_equal( r.get_version(), "http/1.12" );
    boost_check_equal( r.get_method(), "get" );
    boost_check_equal( r.get_uri(), "/" );
    boost_check_equal( r.get_header("host"), "www.example.com" );
}

boost_auto_test_case( new_http_version2 ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/12.12\r\nhost: www.example.com\r\n\r\n";

    bool exception = false;
    size_t pos = 0;

    try {
        pos = r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == false );
    boost_check_equal( pos, 43 );
    boost_check( r.ready() == true );
    boost_check_equal( r.get_version(), "http/12.12" );
    boost_check_equal( r.get_method(), "get" );
    boost_check_equal( r.get_uri(), "/" );
    boost_check_equal( r.get_header("host"), "www.example.com" );
}

/* commented out due to not being implemented yet

boost_auto_test_case( new_http_version3 ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / https/12.12\r\nhost: www.example.com\r\n\r\n";

    bool exception = false;
    size_t pos = 0;

    try {
        pos = r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == true );
}*/

boost_auto_test_case( header_whitespace1 ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.1\r\nhost:  www.example.com \r\n\r\n";

    bool exception = false;
    size_t pos = 0;

    try {
        pos = r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == false );
    boost_check_equal( pos, 43 );
    boost_check( r.ready() == true );
    boost_check_equal( r.get_version(), "http/1.1" );
    boost_check_equal( r.get_method(), "get" );
    boost_check_equal( r.get_uri(), "/" );
    boost_check_equal( r.get_header("host"), "www.example.com" );
}

boost_auto_test_case( header_whitespace2 ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.1\r\nhost:www.example.com\r\n\r\n";

    bool exception = false;
    size_t pos = 0;

    try {
        pos = r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == false );
    boost_check_equal( pos, 40 );
    boost_check( r.ready() == true );
    boost_check_equal( r.get_version(), "http/1.1" );
    boost_check_equal( r.get_method(), "get" );
    boost_check_equal( r.get_uri(), "/" );
    boost_check_equal( r.get_header("host"), "www.example.com" );
}

boost_auto_test_case( header_aggregation ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.1\r\nhost: www.example.com\r\nfoo: bar\r\nfoo: bat\r\n\r\n";

    bool exception = false;
    size_t pos = 0;

    try {
        pos = r.consume(raw.c_str(),raw.size());
    } catch (...) {
        exception = true;
    }

    boost_check( exception == false );
    boost_check_equal( pos, 61 );
    boost_check( r.ready() == true );
    boost_check_equal( r.get_version(), "http/1.1" );
    boost_check_equal( r.get_method(), "get" );
    boost_check_equal( r.get_uri(), "/" );
    boost_check_equal( r.get_header("foo"), "bar, bat" );
}

boost_auto_test_case( wikipedia_example_response ) {
    websocketpp::http::parser::response r;

    std::string raw = "http/1.1 101 switching protocols\r\nupgrade: websocket\r\nconnection: upgrade\r\nsec-websocket-accept: hsmrc0smlyukagmm5oppg2hagwk=\r\nsec-websocket-protocol: chat\r\n\r\n";

    bool exception = false;
    size_t pos = 0;

    try {
        pos += r.consume(raw.c_str(),raw.size());
    } catch (std::exception &e) {
        exception = true;
        std::cout << e.what() << std::endl;
    }

    boost_check( exception == false );
    boost_check_equal( pos, 159 );
    boost_check( r.headers_ready() == true );
    boost_check_equal( r.get_version(), "http/1.1" );
    boost_check_equal( r.get_status_code(), websocketpp::http::status_code::switching_protocols );
    boost_check_equal( r.get_status_msg(), "switching protocols" );
    boost_check_equal( r.get_header("upgrade"), "websocket" );
    boost_check_equal( r.get_header("connection"), "upgrade" );
    boost_check_equal( r.get_header("sec-websocket-accept"), "hsmrc0smlyukagmm5oppg2hagwk=" );
    boost_check_equal( r.get_header("sec-websocket-protocol"), "chat" );
}

boost_auto_test_case( response_with_non_standard_lws ) {
    websocketpp::http::parser::response r;

    std::string raw = "http/1.1 101 switching protocols\r\nupgrade: websocket\r\nconnection: upgrade\r\nsec-websocket-accept:hsmrc0smlyukagmm5oppg2hagwk=\r\nsec-websocket-protocol: chat\r\n\r\n";

    bool exception = false;
    size_t pos = 0;

    try {
        pos += r.consume(raw.c_str(),raw.size());
    } catch (std::exception &e) {
        exception = true;
        std::cout << e.what() << std::endl;
    }

    boost_check( exception == false );
    boost_check_equal( pos, 158 );
    boost_check( r.headers_ready() );
    boost_check_equal( r.get_version(), "http/1.1" );
    boost_check_equal( r.get_status_code(), websocketpp::http::status_code::switching_protocols );
    boost_check_equal( r.get_status_msg(), "switching protocols" );
    boost_check_equal( r.get_header("upgrade"), "websocket" );
    boost_check_equal( r.get_header("connection"), "upgrade" );
    boost_check_equal( r.get_header("sec-websocket-accept"), "hsmrc0smlyukagmm5oppg2hagwk=" );
    boost_check_equal( r.get_header("sec-websocket-protocol"), "chat" );
}

boost_auto_test_case( plain_http_response ) {
    websocketpp::http::parser::response r;

    std::string raw = "http/1.1 200 ok\r\ndate: thu, 10 may 2012 11:59:25 gmt\r\nserver: apache/2.2.21 (unix) mod_ssl/2.2.21 openssl/0.9.8r dav/2 php/5.3.8 with suhosin-patch\r\nlast-modified: tue, 30 mar 2010 17:41:28 gmt\r\netag: \"16799d-55-4830823a78200\"\r\naccept-ranges: bytes\r\ncontent-length: 85\r\nvary: accept-encoding\r\ncontent-type: text/html\r\n\r\n<!doctype html>\n<html>\n<head>\n<title>thor</title>\n</head>\n<body> \n<p>thor</p>\n</body>";

    bool exception = false;
    size_t pos = 0;

    try {
        pos += r.consume(raw.c_str(),raw.size());
    } catch (std::exception &e) {
        exception = true;
        std::cout << e.what() << std::endl;
    }

    boost_check( exception == false );
    boost_check_equal( pos, 405 );
    boost_check( r.headers_ready() == true );
    boost_check( r.ready() == true );
    boost_check_equal( r.get_version(), "http/1.1" );
    boost_check_equal( r.get_status_code(), websocketpp::http::status_code::ok );
    boost_check_equal( r.get_status_msg(), "ok" );
    boost_check_equal( r.get_header("date"), "thu, 10 may 2012 11:59:25 gmt" );
    boost_check_equal( r.get_header("server"), "apache/2.2.21 (unix) mod_ssl/2.2.21 openssl/0.9.8r dav/2 php/5.3.8 with suhosin-patch" );
    boost_check_equal( r.get_header("last-modified"), "tue, 30 mar 2010 17:41:28 gmt" );
    boost_check_equal( r.get_header("etag"), "\"16799d-55-4830823a78200\"" );
    boost_check_equal( r.get_header("accept-ranges"), "bytes" );
    boost_check_equal( r.get_header("content-length"), "85" );
    boost_check_equal( r.get_header("vary"), "accept-encoding" );
    boost_check_equal( r.get_header("content-type"), "text/html" );
    boost_check_equal( r.get_body(), "<!doctype html>\n<html>\n<head>\n<title>thor</title>\n</head>\n<body> \n<p>thor</p>\n</body>" );
}

boost_auto_test_case( parse_istream ) {
    websocketpp::http::parser::response r;

    std::stringstream s;

    s << "http/1.1 200 ok\r\ndate: thu, 10 may 2012 11:59:25 gmt\r\nserver: apache/2.2.21 (unix) mod_ssl/2.2.21 openssl/0.9.8r dav/2 php/5.3.8 with suhosin-patch\r\nlast-modified: tue, 30 mar 2010 17:41:28 gmt\r\netag: \"16799d-55-4830823a78200\"\r\naccept-ranges: bytes\r\ncontent-length: 85\r\nvary: accept-encoding\r\ncontent-type: text/html\r\n\r\n<!doctype html>\n<html>\n<head>\n<title>thor</title>\n</head>\n<body> \n<p>thor</p>\n</body>";

    bool exception = false;
    size_t pos = 0;

    try {
        pos += r.consume(s);
    } catch (std::exception &e) {
        exception = true;
        std::cout << e.what() << std::endl;
    }

    boost_check_equal( exception, false );
    boost_check_equal( pos, 405 );
    boost_check_equal( r.headers_ready(), true );
    boost_check_equal( r.ready(), true );
}

boost_auto_test_case( write_request_basic ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.1\r\n\r\n";

    r.set_version("http/1.1");
    r.set_method("get");
    r.set_uri("/");

    boost_check_equal( r.raw(), raw );
}

boost_auto_test_case( write_request_with_header ) {
    websocketpp::http::parser::request r;

    std::string raw = "get / http/1.1\r\nhost: http://example.com\r\n\r\n";

    r.set_version("http/1.1");
    r.set_method("get");
    r.set_uri("/");
    r.replace_header("host","http://example.com");

    boost_check_equal( r.raw(), raw );
}

boost_auto_test_case( write_request_with_body ) {
    websocketpp::http::parser::request r;

    std::string raw = "post / http/1.1\r\ncontent-length: 48\r\ncontent-type: application/x-www-form-urlencoded\r\nhost: http://example.com\r\n\r\nlicenseid=string&content=string&paramsxml=string";

    r.set_version("http/1.1");
    r.set_method("post");
    r.set_uri("/");
    r.replace_header("host","http://example.com");
    r.replace_header("content-type","application/x-www-form-urlencoded");
    r.set_body("licenseid=string&content=string&paramsxml=string");

    boost_check_equal( r.raw(), raw );
}
