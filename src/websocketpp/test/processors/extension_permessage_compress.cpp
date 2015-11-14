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
#define boost_test_module extension_permessage_deflate
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>
#include <websocketpp/common/memory.hpp>

#include <websocketpp/http/request.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>

struct config {
    typedef websocketpp::http::parser::request request_type;
};
typedef websocketpp::extensions::permessage_deflate::enabled<config>
    compressor_type;

using namespace websocketpp;

boost_auto_test_case( deflate_init ) {
    /*compressor_type compressor;
    websocketpp::http::parser::attribute_list attributes;
    std::pair<lib::error_code,std::string> neg_ret;

    neg_ret = compressor.negotiate(attributes);

    boost_check_equal( neg_ret.first,
        extensions::permessage_deflate::error::invalid_parameters );*/

    /**
     * window size is primarily controlled by the writer. a stream can only be
     * read by a window size equal to or greater than the one use to compress
     * it initially. the default windows size is also the maximum window size.
     * thus:
     *
     * outbound window size can be limited unilaterally under the assumption
     * that the opposite end will be using the default (maximum size which can
     * read anything)
     *
     * inbound window size must be limited by asking the remote endpoint to
     * do so and it agreeing.
     *
     * context takeover is also primarily controlled by the writer. if the
     * compressor does not clear its context between messages then the reader
     * can't either.
     *
     * outbound messages may clear context between messages unilaterally.
     * inbound messages must retain state unless the remote endpoint signals
     * otherwise.
     *
     * negotiation options:
     * client must choose from the following options:
     * - whether or not to request an inbound window limit
     * - whether or not to signal that it will honor an outbound window limit
     * - whether or not to request that the server disallow context takeover
     *
     * server must answer in the following ways
     * - if client requested a window size limit, is the window size limit
     *   acceptable?
     * - if client allows window limit requests, should we send one?
     * - if client requested no context takeover, should we accept?
     *
     *
     *
     * all defaults
     * req: permessage-compress; method=deflate
     * ans: permessage-compress; method=deflate
     *
     * # client wants to limit the size of inbound windows from server
     * permessage-compress; method="deflate; s2c_max_window_bits=8, deflate"
     * ans: permessage-compress; method="deflate; s2c_max_window_bits=8"
     * or
     * ans: permessage-compress; method=deflate
     *
     * # server wants to limit the size of inbound windows from client
     * client:
     * permessage-compress; method="deflate; c2s_max_window_bits, deflate"
     *
     * server:
     * permessage-compress; method="deflate; c2s_max_window_bits=8"
     *
     * # client wants to
     *
     *
     *
     *
     *
     *
     */




   /* processor::extensions::deflate_method d(true);
    http::parser::attribute_list attributes;
    lib::error_code ec;

    attributes.push_back(http::parser::attribute("foo","bar"));
    ec = d.init(attributes);
    boost_check(ec == processor::extensions::error::unknown_method_parameter);

    attributes.clear();
    attributes.push_back(http::parser::attribute("s2c_max_window_bits","bar"));
    ec = d.init(attributes);
    boost_check(ec == processor::extensions::error::invalid_algorithm_settings);

    attributes.clear();
    attributes.push_back(http::parser::attribute("s2c_max_window_bits","7"));
    ec = d.init(attributes);
    boost_check(ec == processor::extensions::error::invalid_algorithm_settings);

    attributes.clear();
    attributes.push_back(http::parser::attribute("s2c_max_window_bits","16"));
    ec = d.init(attributes);
    boost_check(ec == processor::extensions::error::invalid_algorithm_settings);

    attributes.clear();
    attributes.push_back(http::parser::attribute("s2c_max_window_bits","9"));
    ec = d.init(attributes);
    boost_check( !ec);

    attributes.clear();
    ec = d.init(attributes);
    boost_check( !ec);

    processor::extensions::deflate_engine de;

    unsigned char test_in[] = "hellohellohellohello";
    unsigned char test_out[30];

    ulongf test_out_size = 30;

    int ret;

    ret = compress(test_out, &test_out_size, test_in, 20);

    std::cout << ret << std::endl
              << websocketpp::utility::to_hex(test_in,20) << std::endl
              << websocketpp::utility::to_hex(test_out,test_out_size) << std::endl;

    std::string input = "hello";
    std::string output = "";
    ec = de.compress(input,output);

    boost_check( ec == processor::extensions::error::uninitialized );

    //std::cout << ec.message() << websocketpp::utility::to_hex(output) << std::endl;

    ec = de.init(15,15,z_default_compression,8,z_fixed);
    //ec = de.init();
    boost_check( !ec );

    ec = de.compress(input,output);
    std::cout << ec.message() << std::endl
              << websocketpp::utility::to_hex(input) << std::endl
              << websocketpp::utility::to_hex(output) << std::endl;

    output = "";

    ec = de.compress(input,output);
    std::cout << ec.message() << std::endl
              << websocketpp::utility::to_hex(input) << std::endl
              << websocketpp::utility::to_hex(output) << std::endl;

    input = output;
    output = "";
    ec = de.decompress(input,output);
    std::cout << ec.message() << std::endl
              << websocketpp::utility::to_hex(input) << std::endl
              << websocketpp::utility::to_hex(output) << std::endl;
    */
}
