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
#define boost_test_module basic_log
#include <boost/test/unit_test.hpp>

#include <string>

#include <websocketpp/logger/basic.hpp>
#include <websocketpp/concurrency/none.hpp>
#include <websocketpp/concurrency/basic.hpp>

boost_auto_test_case( is_token_char ) {
    typedef websocketpp::log::basic<websocketpp::concurrency::none,websocketpp::log::elevel> error_log;

    error_log elog;

    boost_check( elog.static_test(websocketpp::log::elevel::info ) == true );
    boost_check( elog.static_test(websocketpp::log::elevel::warn ) == true );
    boost_check( elog.static_test(websocketpp::log::elevel::rerror ) == true );
    boost_check( elog.static_test(websocketpp::log::elevel::fatal ) == true );

    elog.set_channels(websocketpp::log::elevel::info);

    elog.write(websocketpp::log::elevel::info,"information");
    elog.write(websocketpp::log::elevel::warn,"a warning");
    elog.write(websocketpp::log::elevel::rerror,"a error");
    elog.write(websocketpp::log::elevel::fatal,"a critical error");
}

boost_auto_test_case( access_clear ) {
    typedef websocketpp::log::basic<websocketpp::concurrency::none,websocketpp::log::alevel> access_log;

    std::stringstream out;
    access_log logger(0xffffffff,&out);

    // clear all channels
    logger.clear_channels(0xffffffff);

    // writes shouldn't happen
    logger.write(websocketpp::log::alevel::devel,"devel");
    //std::cout << "|" << out.str() << "|" << std::endl;
    boost_check( out.str().size() == 0 );
}

boost_auto_test_case( basic_concurrency ) {
    typedef websocketpp::log::basic<websocketpp::concurrency::basic,websocketpp::log::alevel> access_log;

    std::stringstream out;
    access_log logger(0xffffffff,&out);

    logger.set_channels(0xffffffff);

    logger.write(websocketpp::log::alevel::devel,"devel");
    //std::cout << "|" << out.str() << "|" << std::endl;
    boost_check( out.str().size() > 0 );
}
