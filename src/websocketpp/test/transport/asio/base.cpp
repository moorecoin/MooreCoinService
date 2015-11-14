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
#define boost_test_module transport_asio_base
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <websocketpp/transport/asio/base.hpp>

boost_auto_test_case( blank_error ) {
    websocketpp::lib::error_code ec;

    boost_check( !ec );
}

boost_auto_test_case( asio_error ) {
    using websocketpp::transport::asio::error::make_error_code;
    using websocketpp::transport::asio::error::general;

    websocketpp::lib::error_code ec = make_error_code(general);

    boost_check( ec == general );
    boost_check( ec.value() == 1 );
}
