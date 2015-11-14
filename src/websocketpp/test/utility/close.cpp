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
#define boost_test_module close
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/close.hpp>
#include <websocketpp/utilities.hpp>

using namespace websocketpp;

boost_auto_test_case( reserved_values ) {
    boost_check( !close::status::reserved(999) );
    boost_check( close::status::reserved(1004) );
    boost_check( close::status::reserved(1014) );
    boost_check( close::status::reserved(1016) );
    boost_check( close::status::reserved(2999) );
    boost_check( !close::status::reserved(1000) );
}

boost_auto_test_case( invalid_values ) {
    boost_check( close::status::invalid(0) );
    boost_check( close::status::invalid(999) );
    boost_check( !close::status::invalid(1000) );
    boost_check( close::status::invalid(1005) );
    boost_check( close::status::invalid(1006) );
    boost_check( close::status::invalid(1015) );
    boost_check( !close::status::invalid(2999) );
    boost_check( !close::status::invalid(3000) );
    boost_check( close::status::invalid(5000) );
}

boost_auto_test_case( value_extraction ) {
    lib::error_code ec;
    std::string payload = "oo";

    // value = 1000
    payload[0] = 0x03;
    payload[1] = char(0xe8);
    boost_check( close::extract_code(payload,ec) == close::status::normal );
    boost_check( !ec );

    // value = 1004
    payload[0] = 0x03;
    payload[1] = char(0xec);
    boost_check( close::extract_code(payload,ec) == 1004 );
    boost_check( ec == error::reserved_close_code );

    // value = 1005
    payload[0] = 0x03;
    payload[1] = char(0xed);
    boost_check( close::extract_code(payload,ec) == close::status::no_status );
    boost_check( ec == error::invalid_close_code );

    // value = 3000
    payload[0] = 0x0b;
    payload[1] = char(0xb8);
    boost_check( close::extract_code(payload,ec) == 3000 );
    boost_check( !ec );
}

boost_auto_test_case( extract_empty ) {
    lib::error_code ec;
    std::string payload = "";

    boost_check( close::extract_code(payload,ec) == close::status::no_status );
    boost_check( !ec );
}

boost_auto_test_case( extract_short ) {
    lib::error_code ec;
    std::string payload = "0";

    boost_check( close::extract_code(payload,ec) == close::status::protocol_error );
    boost_check( ec == error::bad_close_code );
}

boost_auto_test_case( extract_reason ) {
    lib::error_code ec;
    std::string payload = "00foo";

    boost_check( close::extract_reason(payload,ec) == "foo" );
    boost_check( !ec );

    payload = "";
    boost_check( close::extract_reason(payload,ec) == "" );
    boost_check( !ec );

    payload = "00";
    boost_check( close::extract_reason(payload,ec) == "" );
    boost_check( !ec );

    payload = "000";
    payload[2] = char(0xff);

    close::extract_reason(payload,ec);
    boost_check( ec == error::invalid_utf8 );
}
