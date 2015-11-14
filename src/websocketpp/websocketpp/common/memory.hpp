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

#ifndef websocketpp_common_memory_hpp
#define websocketpp_common_memory_hpp

#if defined _websocketpp_cpp11_stl_ && !defined _websocketpp_no_cpp11_memory_
    #ifndef _websocketpp_cpp11_memory_
        #define _websocketpp_cpp11_memory_
    #endif
#endif

#ifdef _websocketpp_cpp11_memory_
    #include <memory>
#else
    #include <boost/shared_ptr.hpp>
	#include <boost/make_shared.hpp>
    #include <boost/scoped_array.hpp>
    #include <boost/enable_shared_from_this.hpp>
    #include <boost/pointer_cast.hpp>
#endif

namespace websocketpp {
namespace lib {

#ifdef _websocketpp_cpp11_memory_
    using std::shared_ptr;
    using std::weak_ptr;
    using std::enable_shared_from_this;
    using std::static_pointer_cast;
    using std::make_shared;

    typedef std::unique_ptr<unsigned char[]> unique_ptr_uchar_array;
#else
    using boost::shared_ptr;
    using boost::weak_ptr;
    using boost::enable_shared_from_this;
    using boost::static_pointer_cast;
    using boost::make_shared;

    typedef boost::scoped_array<unsigned char> unique_ptr_uchar_array;
#endif

} // namespace lib
} // namespace websocketpp

#endif // websocketpp_common_memory_hpp
