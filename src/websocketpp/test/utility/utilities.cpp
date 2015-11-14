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
#define boost_test_module utility
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/utilities.hpp>

boost_auto_test_suite ( utility )

boost_auto_test_case( substr_found ) {
    std::string haystack = "abc123";
    std::string needle = "abc";

    boost_check(websocketpp::utility::ci_find_substr(haystack,needle) ==haystack.begin());
}

boost_auto_test_case( substr_found_ci ) {
    std::string haystack = "abc123";
    std::string needle = "abc";

    boost_check(websocketpp::utility::ci_find_substr(haystack,needle) ==haystack.begin());
}

boost_auto_test_case( substr_not_found ) {
    std::string haystack = "abd123";
    std::string needle = "abcd";

    boost_check(websocketpp::utility::ci_find_substr(haystack,needle) == haystack.end());
}

boost_auto_test_case( to_lower ) {
    std::string in = "abcd";

    boost_check_equal(websocketpp::utility::to_lower(in), "abcd");
}

boost_auto_test_case( string_replace_all ) {
    std::string source = "foo \"bar\" baz";
    std::string dest = "foo \\\"bar\\\" baz";

    using websocketpp::utility::string_replace_all;
    boost_check_equal(string_replace_all(source,"\"","\\\""),dest);
}

boost_auto_test_suite_end()
