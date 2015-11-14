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

#ifndef websocketpp_common_system_error_hpp
#define websocketpp_common_system_error_hpp

#if defined _websocketpp_cpp11_stl_ && !defined _websocketpp_no_cpp11_system_error_
    #ifndef _websocketpp_cpp11_system_error_
        #define _websocketpp_cpp11_system_error_
    #endif
#endif

#ifdef _websocketpp_cpp11_system_error_
    #include <system_error>
#else
    #include <boost/system/error_code.hpp>
    #include <boost/system/system_error.hpp>
#endif

namespace websocketpp {
namespace lib {

#ifdef _websocketpp_cpp11_system_error_
    using std::error_code;
    using std::error_category;
    using std::error_condition;
    using std::system_error;
    #define _websocketpp_error_code_enum_ns_start_ namespace std {
    #define _websocketpp_error_code_enum_ns_end_ }
#else
    using boost::system::error_code;
    using boost::system::error_category;
    using boost::system::error_condition;
    using boost::system::system_error;
    #define _websocketpp_error_code_enum_ns_start_ namespace boost { namespace system {
    #define _websocketpp_error_code_enum_ns_end_ }}
#endif

} // namespace lib
} // namespace websocketpp

#endif // websocketpp_common_system_error_hpp
