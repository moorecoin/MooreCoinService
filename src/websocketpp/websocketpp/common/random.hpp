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

#ifndef websocketpp_common_random_device_hpp
#define websocketpp_common_random_device_hpp

#if defined _websocketpp_cpp11_stl_ && !defined _websocketpp_no_cpp11_random_device_
    #ifndef _websocketpp_cpp11_random_device_
        #define _websocketpp_cpp11_random_device_
    #endif
#endif

#ifdef _websocketpp_cpp11_random_device_
    #include <random>
#else
    #include <boost/version.hpp>

    #if (boost_version/100000) == 1 && ((boost_version/100)%1000) > 46
        #include <boost/random/uniform_int_distribution.hpp>
        #include <boost/random/random_device.hpp>
    #elif (boost_version/100000) == 1 && ((boost_version/100)%1000) >= 43
        #include <boost/nondet_random.hpp>
    #else
        // todo: static_assert(false, "could not find a suitable random_device")
    #endif
#endif

namespace websocketpp {
namespace lib {

#ifdef _websocketpp_cpp11_random_device_
    using std::random_device;
    using std::uniform_int_distribution;
#else
    using boost::random::random_device;
    using boost::random::uniform_int_distribution;
#endif

} // namespace lib
} // namespace websocketpp

#endif // websocketpp_common_random_device_hpp
