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

#ifndef websocketpp_common_regex_hpp
#define websocketpp_common_regex_hpp

#if defined _websocketpp_cpp11_stl_ && !defined _websocketpp_no_cpp11_regex_
    #ifndef _websocketpp_cpp11_regex_
        #define _websocketpp_cpp11_regex_
    #endif
#endif

#ifdef _websocketpp_cpp11_regex_
    #include <regex>
#else
    #include <boost/regex.hpp>
#endif

namespace websocketpp {
namespace lib {

#ifdef _websocketpp_cpp11_regex_
    using std::cmatch;
    using std::regex;
    using std::regex_match;
#else
    using boost::cmatch;
    using boost::regex;
    using boost::regex_match;
#endif

} // namespace lib
} // namespace websocketpp

#endif // websocketpp_common_regex_hpp
