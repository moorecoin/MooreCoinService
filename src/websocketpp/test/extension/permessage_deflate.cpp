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
#define boost_test_module permessage_deflate
#include <boost/test/unit_test.hpp>

#include <websocketpp/error.hpp>

#include <websocketpp/extensions/extension.hpp>
#include <websocketpp/extensions/permessage_deflate/disabled.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>

#include <string>

#include <websocketpp/utilities.hpp>
#include <iostream>

class config {};

typedef websocketpp::extensions::permessage_deflate::enabled<config> enabled_type;
typedef websocketpp::extensions::permessage_deflate::disabled<config> disabled_type;

struct ext_vars {
    enabled_type exts;
    enabled_type extc;
    websocketpp::lib::error_code ec;
    websocketpp::err_str_pair esp;
    websocketpp::http::attribute_list attr;
};
namespace pmde = websocketpp::extensions::permessage_deflate::error;
namespace pmd_mode = websocketpp::extensions::permessage_deflate::mode;

// ensure the disabled extension behaves appropriately disabled

boost_auto_test_case( disabled_is_disabled ) {
    disabled_type exts;
    boost_check( !exts.is_implemented() );
}

boost_auto_test_case( disabled_is_off ) {
    disabled_type exts;
    boost_check( !exts.is_enabled() );
}

// ensure the enabled version actually works

boost_auto_test_case( enabled_is_enabled ) {
    ext_vars v;
    boost_check( v.exts.is_implemented() );
    boost_check( v.extc.is_implemented() );
}


boost_auto_test_case( enabled_starts_disabled ) {
    ext_vars v;
    boost_check( !v.exts.is_enabled() );
    boost_check( !v.extc.is_enabled() );
}

boost_auto_test_case( negotiation_empty_attr ) {
    ext_vars v;

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate");
}

boost_auto_test_case( negotiation_invalid_attr ) {
    ext_vars v;
    v.attr["foo"] = "bar";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( !v.exts.is_enabled() );
    boost_check_equal( v.esp.first, pmde::make_error_code(pmde::invalid_attributes) );
    boost_check_equal( v.esp.second, "");
}

// negotiate s2c_no_context_takeover
boost_auto_test_case( negotiate_s2c_no_context_takeover_invalid ) {
    ext_vars v;
    v.attr["s2c_no_context_takeover"] = "foo";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( !v.exts.is_enabled() );
    boost_check_equal( v.esp.first, pmde::make_error_code(pmde::invalid_attribute_value) );
    boost_check_equal( v.esp.second, "");
}

boost_auto_test_case( negotiate_s2c_no_context_takeover ) {
    ext_vars v;
    v.attr["s2c_no_context_takeover"] = "";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_no_context_takeover");
}

boost_auto_test_case( negotiate_s2c_no_context_takeover_server_initiated ) {
    ext_vars v;

    v.exts.enable_s2c_no_context_takeover();
    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_no_context_takeover");
}

// negotiate c2s_no_context_takeover
boost_auto_test_case( negotiate_c2s_no_context_takeover_invalid ) {
    ext_vars v;
    v.attr["c2s_no_context_takeover"] = "foo";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( !v.exts.is_enabled() );
    boost_check_equal( v.esp.first, pmde::make_error_code(pmde::invalid_attribute_value) );
    boost_check_equal( v.esp.second, "");
}

boost_auto_test_case( negotiate_c2s_no_context_takeover ) {
    ext_vars v;
    v.attr["c2s_no_context_takeover"] = "";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; c2s_no_context_takeover");
}

boost_auto_test_case( negotiate_c2s_no_context_takeover_server_initiated ) {
    ext_vars v;

    v.exts.enable_c2s_no_context_takeover();
    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; c2s_no_context_takeover");
}


// negotiate s2c_max_window_bits
boost_auto_test_case( negotiate_s2c_max_window_bits_invalid ) {
    ext_vars v;

    std::vector<std::string> values;
    values.push_back("");
    values.push_back("foo");
    values.push_back("7");
    values.push_back("16");

    std::vector<std::string>::const_iterator it;
    for (it = values.begin(); it != values.end(); ++it) {
        v.attr["s2c_max_window_bits"] = *it;

        v.esp = v.exts.negotiate(v.attr);
        boost_check( !v.exts.is_enabled() );
        boost_check_equal( v.esp.first, pmde::make_error_code(pmde::invalid_attribute_value) );
        boost_check_equal( v.esp.second, "");
    }
}

boost_auto_test_case( negotiate_s2c_max_window_bits_valid ) {
    ext_vars v;
    v.attr["s2c_max_window_bits"] = "8";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_max_window_bits=8");

    v.attr["s2c_max_window_bits"] = "15";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate");
}

boost_auto_test_case( invalid_set_s2c_max_window_bits ) {
    ext_vars v;

    v.ec = v.exts.set_s2c_max_window_bits(7,pmd_mode::decline);
    boost_check_equal(v.ec,pmde::make_error_code(pmde::invalid_max_window_bits));

    v.ec = v.exts.set_s2c_max_window_bits(16,pmd_mode::decline);
    boost_check_equal(v.ec,pmde::make_error_code(pmde::invalid_max_window_bits));
}

boost_auto_test_case( negotiate_s2c_max_window_bits_decline ) {
    ext_vars v;
    v.attr["s2c_max_window_bits"] = "8";

    v.ec = v.exts.set_s2c_max_window_bits(15,pmd_mode::decline);
    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.ec, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate");
}

boost_auto_test_case( negotiate_s2c_max_window_bits_accept ) {
    ext_vars v;
    v.attr["s2c_max_window_bits"] = "8";

    v.ec = v.exts.set_s2c_max_window_bits(15,pmd_mode::accept);
    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.ec, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_max_window_bits=8");
}

boost_auto_test_case( negotiate_s2c_max_window_bits_largest ) {
    ext_vars v;
    v.attr["s2c_max_window_bits"] = "8";

    v.ec = v.exts.set_s2c_max_window_bits(15,pmd_mode::largest);
    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.ec, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_max_window_bits=8");
}

boost_auto_test_case( negotiate_s2c_max_window_bits_smallest ) {
    ext_vars v;
    v.attr["s2c_max_window_bits"] = "8";

    v.ec = v.exts.set_s2c_max_window_bits(15,pmd_mode::smallest);
    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.ec, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_max_window_bits=8");
}

// negotiate s2c_max_window_bits
boost_auto_test_case( negotiate_c2s_max_window_bits_invalid ) {
    ext_vars v;

    std::vector<std::string> values;
    values.push_back("foo");
    values.push_back("7");
    values.push_back("16");

    std::vector<std::string>::const_iterator it;
    for (it = values.begin(); it != values.end(); ++it) {
        v.attr["c2s_max_window_bits"] = *it;

        v.esp = v.exts.negotiate(v.attr);
        boost_check( !v.exts.is_enabled() );
        boost_check_equal( v.esp.first, pmde::make_error_code(pmde::invalid_attribute_value) );
        boost_check_equal( v.esp.second, "");
    }
}

boost_auto_test_case( negotiate_c2s_max_window_bits_valid ) {
    ext_vars v;

    v.attr["c2s_max_window_bits"] = "";
    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate");

    v.attr["c2s_max_window_bits"] = "8";
    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; c2s_max_window_bits=8");

    v.attr["c2s_max_window_bits"] = "15";
    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate");
}

boost_auto_test_case( invalid_set_c2s_max_window_bits ) {
    ext_vars v;

    v.ec = v.exts.set_c2s_max_window_bits(7,pmd_mode::decline);
    boost_check_equal(v.ec,pmde::make_error_code(pmde::invalid_max_window_bits));

    v.ec = v.exts.set_c2s_max_window_bits(16,pmd_mode::decline);
    boost_check_equal(v.ec,pmde::make_error_code(pmde::invalid_max_window_bits));
}

boost_auto_test_case( negotiate_c2s_max_window_bits_decline ) {
    ext_vars v;
    v.attr["c2s_max_window_bits"] = "8";

    v.ec = v.exts.set_c2s_max_window_bits(8,pmd_mode::decline);
    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.ec, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate");
}

boost_auto_test_case( negotiate_c2s_max_window_bits_accept ) {
    ext_vars v;
    v.attr["c2s_max_window_bits"] = "8";

    v.ec = v.exts.set_c2s_max_window_bits(15,pmd_mode::accept);
    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.ec, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; c2s_max_window_bits=8");
}

boost_auto_test_case( negotiate_c2s_max_window_bits_largest ) {
    ext_vars v;
    v.attr["c2s_max_window_bits"] = "8";

    v.ec = v.exts.set_c2s_max_window_bits(15,pmd_mode::largest);
    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.ec, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; c2s_max_window_bits=8");
}

boost_auto_test_case( negotiate_c2s_max_window_bits_smallest ) {
    ext_vars v;
    v.attr["c2s_max_window_bits"] = "8";

    v.ec = v.exts.set_c2s_max_window_bits(15,pmd_mode::smallest);
    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.ec, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; c2s_max_window_bits=8");
}


// combinations with 2
boost_auto_test_case( negotiate_two_client_initiated1 ) {
    ext_vars v;

    v.attr["s2c_no_context_takeover"] = "";
    v.attr["c2s_no_context_takeover"] = "";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_no_context_takeover; c2s_no_context_takeover");
}

boost_auto_test_case( negotiate_two_client_initiated2 ) {
    ext_vars v;

    v.attr["s2c_no_context_takeover"] = "";
    v.attr["s2c_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_no_context_takeover; s2c_max_window_bits=10");
}

boost_auto_test_case( negotiate_two_client_initiated3 ) {
    ext_vars v;

    v.attr["s2c_no_context_takeover"] = "";
    v.attr["c2s_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_no_context_takeover; c2s_max_window_bits=10");
}

boost_auto_test_case( negotiate_two_client_initiated4 ) {
    ext_vars v;

    v.attr["c2s_no_context_takeover"] = "";
    v.attr["s2c_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; c2s_no_context_takeover; s2c_max_window_bits=10");
}

boost_auto_test_case( negotiate_two_client_initiated5 ) {
    ext_vars v;

    v.attr["c2s_no_context_takeover"] = "";
    v.attr["c2s_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; c2s_no_context_takeover; c2s_max_window_bits=10");
}

boost_auto_test_case( negotiate_two_client_initiated6 ) {
    ext_vars v;

    v.attr["s2c_max_window_bits"] = "10";
    v.attr["c2s_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_max_window_bits=10; c2s_max_window_bits=10");
}

boost_auto_test_case( negotiate_three_client_initiated1 ) {
    ext_vars v;

    v.attr["s2c_no_context_takeover"] = "";
    v.attr["c2s_no_context_takeover"] = "";
    v.attr["s2c_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_no_context_takeover; c2s_no_context_takeover; s2c_max_window_bits=10");
}

boost_auto_test_case( negotiate_three_client_initiated2 ) {
    ext_vars v;

    v.attr["s2c_no_context_takeover"] = "";
    v.attr["c2s_no_context_takeover"] = "";
    v.attr["c2s_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_no_context_takeover; c2s_no_context_takeover; c2s_max_window_bits=10");
}

boost_auto_test_case( negotiate_three_client_initiated3 ) {
    ext_vars v;

    v.attr["s2c_no_context_takeover"] = "";
    v.attr["s2c_max_window_bits"] = "10";
    v.attr["c2s_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_no_context_takeover; s2c_max_window_bits=10; c2s_max_window_bits=10");
}

boost_auto_test_case( negotiate_three_client_initiated4 ) {
    ext_vars v;

    v.attr["c2s_no_context_takeover"] = "";
    v.attr["s2c_max_window_bits"] = "10";
    v.attr["c2s_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; c2s_no_context_takeover; s2c_max_window_bits=10; c2s_max_window_bits=10");
}

boost_auto_test_case( negotiate_four_client_initiated ) {
    ext_vars v;

    v.attr["s2c_no_context_takeover"] = "";
    v.attr["c2s_no_context_takeover"] = "";
    v.attr["s2c_max_window_bits"] = "10";
    v.attr["c2s_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    boost_check( v.exts.is_enabled() );
    boost_check_equal( v.esp.first, websocketpp::lib::error_code() );
    boost_check_equal( v.esp.second, "permessage-deflate; s2c_no_context_takeover; c2s_no_context_takeover; s2c_max_window_bits=10; c2s_max_window_bits=10");
}

// compression
/*
boost_auto_test_case( compress_data ) {
    ext_vars v;

    std::string in = "hello";
    std::string out;
    std::string in2;
    std::string out2;

    v.exts.init();

    v.ec = v.exts.compress(in,out);

    std::cout << "in : " << websocketpp::utility::to_hex(in) << std::endl;
    boost_check_equal( v.ec, websocketpp::lib::error_code() );
    std::cout << "out: " << websocketpp::utility::to_hex(out) << std::endl;

    in2 = out;

    v.ec = v.exts.decompress(reinterpret_cast<const uint8_t *>(in2.data()),in2.size(),out2);

    boost_check_equal( v.ec, websocketpp::lib::error_code() );
    std::cout << "out: " << websocketpp::utility::to_hex(out2) << std::endl;
    boost_check_equal( out, out2 );
}

boost_auto_test_case( decompress_data ) {
    ext_vars v;

    uint8_t in[12] = {0xf2, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00, 0x00, 0x00, 0xff, 0xff};
    std::string out;

    v.exts.init();

    v.ec = v.exts.decompress(in,12,out);

    boost_check_equal( v.ec, websocketpp::lib::error_code() );
    std::cout << "out: " << websocketpp::utility::to_hex(out) << std::endl;
    boost_check( false );
}
*/
