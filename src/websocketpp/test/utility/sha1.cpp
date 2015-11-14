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
#define boost_test_module sha1
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/sha1/sha1.hpp>
#include <websocketpp/utilities.hpp>

boost_auto_test_suite ( sha1 )

boost_auto_test_case( sha1_test_a ) {
    unsigned char hash[20];
    unsigned char reference[20] = {0xa9, 0x99, 0x3e, 0x36, 0x47,
                                   0x06, 0x81, 0x6a, 0xba, 0x3e,
                                   0x25, 0x71, 0x78, 0x50, 0xc2,
                                   0x6c, 0x9c, 0xd0, 0xd8, 0x9d};

    websocketpp::sha1::calc("abc",3,hash);

    boost_check_equal_collections(hash, hash+20, reference, reference+20);
}

boost_auto_test_case( sha1_test_b ) {
    unsigned char hash[20];
    unsigned char reference[20] = {0x84, 0x98, 0x3e, 0x44, 0x1c,
                                   0x3b, 0xd2, 0x6e, 0xba, 0xae,
                                   0x4a, 0xa1, 0xf9, 0x51, 0x29,
                                   0xe5, 0xe5, 0x46, 0x70, 0xf1};

    websocketpp::sha1::calc(
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",56,hash);

    boost_check_equal_collections(hash, hash+20, reference, reference+20);
}

boost_auto_test_case( sha1_test_c ) {
    std::string input;
    unsigned char hash[20];
    unsigned char reference[20] = {0x34, 0xaa, 0x97, 0x3c, 0xd4,
                                   0xc4, 0xda, 0xa4, 0xf6, 0x1e,
                                   0xeb, 0x2b, 0xdb, 0xad, 0x27,
                                   0x31, 0x65, 0x34, 0x01, 0x6f};

    for (int i = 0; i < 1000000; i++) {
        input += 'a';
    }

    websocketpp::sha1::calc(input.c_str(),input.size(),hash);

    boost_check_equal_collections(hash, hash+20, reference, reference+20);
}

boost_auto_test_suite_end()
