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
#define boost_test_module hybi_util
#include <boost/test/unit_test.hpp>

#include <iostream>

#include "../../src/processors/hybi_util.hpp"
#include "../../src/network_utilities.hpp"

boost_auto_test_case( circshift_0 ) {
    if (sizeof(size_t) == 8) {
        size_t test = 0x0123456789abcdef;
        
        test = websocketpp::processor::hybi_util::circshift_prepared_key(test,0);
        
        boost_check( test == 0x0123456789abcdef);
    } else {
        size_t test = 0x01234567;
        
        test = websocketpp::processor::hybi_util::circshift_prepared_key(test,0);
        
        boost_check( test == 0x01234567);
    }
}

boost_auto_test_case( circshift_1 ) {
    if (sizeof(size_t) == 8) {
        size_t test = 0x0123456789abcdef;
        
        test = websocketpp::processor::hybi_util::circshift_prepared_key(test,1);
        
        boost_check( test == 0xef0123456789abcd);
    } else {
        size_t test = 0x01234567;
        
        test = websocketpp::processor::hybi_util::circshift_prepared_key(test,1);
        
        boost_check( test == 0x67012345);
    }
}

boost_auto_test_case( circshift_2 ) {
    if (sizeof(size_t) == 8) {
        size_t test = 0x0123456789abcdef;
        
        test = websocketpp::processor::hybi_util::circshift_prepared_key(test,2);
        
        boost_check( test == 0xcdef0123456789ab);
    } else {
        size_t test = 0x01234567;
        
        test = websocketpp::processor::hybi_util::circshift_prepared_key(test,2);
        
        boost_check( test == 0x45670123);
    }
}

boost_auto_test_case( circshift_3 ) {
    if (sizeof(size_t) == 8) {
        size_t test = 0x0123456789abcdef;
        
        test = websocketpp::processor::hybi_util::circshift_prepared_key(test,3);
        
        boost_check( test == 0xabcdef0123456789);
    } else {
        size_t test = 0x01234567;
        
        test = websocketpp::processor::hybi_util::circshift_prepared_key(test,3);
        
        boost_check( test == 0x23456701);
    }
}
