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
#define boost_test_module endpoint
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <sstream>

#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>

boost_auto_test_case( construct_server_iostream ) {
    websocketpp::server<websocketpp::config::core> s;
}

boost_auto_test_case( construct_server_asio_plain ) {
    websocketpp::server<websocketpp::config::asio> s;
}

boost_auto_test_case( construct_server_asio_tls ) {
    websocketpp::server<websocketpp::config::asio_tls> s;
}

boost_auto_test_case( initialize_server_asio ) {
    websocketpp::server<websocketpp::config::asio> s;
    s.init_asio();
}

boost_auto_test_case( initialize_server_asio_external ) {
    websocketpp::server<websocketpp::config::asio> s;
    boost::asio::io_service ios;
    s.init_asio(&ios);
}

struct endpoint_extension {
    endpoint_extension() : extension_value(5) {}

    int extension_method() {
        return extension_value;
    }

    bool is_server() const {
        return false;
    }

    int extension_value;
};

struct stub_config : public websocketpp::config::core {
    typedef core::concurrency_type concurrency_type;

    typedef core::request_type request_type;
    typedef core::response_type response_type;

    typedef core::message_type message_type;
    typedef core::con_msg_manager_type con_msg_manager_type;
    typedef core::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef core::alog_type alog_type;
    typedef core::elog_type elog_type;

    typedef core::rng_type rng_type;

    typedef core::transport_type transport_type;

    typedef endpoint_extension endpoint_base;
};

boost_auto_test_case( endpoint_extensions ) {
    websocketpp::server<stub_config> s;

    boost_check( s.extension_value == 5 );
    boost_check( s.extension_method() == 5 );

    boost_check( s.is_server() == true );
}
