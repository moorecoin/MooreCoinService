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

 // this header defines websocket++ macros for c++11 compatibility based on the 
 // boost.config library. this will correctly configure most target platforms
 // simply by including this header before any other websocket++ header.

#ifndef websocketpp_config_boost_config_hpp
#define websocketpp_config_boost_config_hpp

#include <boost/config.hpp>

//  _websocketpp_cpp11_memory_ and _websocketpp_cpp11_functional_ presently
//  only work if either both or neither is defined.
#if !defined boost_no_cxx11_smart_ptr && !defined boost_no_cxx11_hdr_functional
    #define _websocketpp_cpp11_memory_
    #define _websocketpp_cpp11_functional_
#endif

#ifdef boost_asio_has_std_chrono
    #define _websocketpp_cpp11_chrono_
#endif

#ifndef boost_no_cxx11_hdr_random
    #define _websocketpp_cpp11_random_device_
#endif

#ifndef boost_no_cxx11_hdr_regex
    #define _websocketpp_cpp11_regex_
#endif

#ifndef boost_no_cxx11_hdr_system_error
    #define _websocketpp_cpp11_system_error_
#endif

#ifndef boost_no_cxx11_hdr_thread
    #define _websocketpp_cpp11_thread_
#endif

#ifndef boost_no_cxx11_hdr_initializer_list
    #define _websocketpp_initializer_lists_
#endif

#define _websocketpp_noexcept_token_  boost_noexcept
#define _websocketpp_constexpr_token_  boost_constexpr
// todo: nullptr support

#endif // websocketpp_config_boost_config_hpp
