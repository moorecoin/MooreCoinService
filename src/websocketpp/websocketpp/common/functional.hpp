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

#ifndef websocketpp_common_functional_hpp
#define websocketpp_common_functional_hpp

#if defined _websocketpp_cpp11_stl_ && !defined _websocketpp_no_cpp11_functional_
    #ifndef _websocketpp_cpp11_functional_
        #define _websocketpp_cpp11_functional_
    #endif
#endif

#ifdef _websocketpp_cpp11_functional_
    #include <functional>
#else
    #include <boost/bind.hpp>
    #include <boost/function.hpp>
    #include <boost/ref.hpp>
#endif



namespace websocketpp {
namespace lib {

#ifdef _websocketpp_cpp11_functional_
    using std::function;
    using std::bind;
    using std::ref;
    namespace placeholders = std::placeholders;
    
    // there are some cases where a c++11 compiler balks at using std::ref
    // but a c++03 compiler using boost function requires boost::ref. as such
    // lib::ref is not useful in these cases. instead this macro allows the use
    // of boost::ref in the case of a boost compile or no reference wrapper at
    // all in the case of a c++11 compile
    #define _websocketpp_ref(x) x
    
    template <typename t>
    void clear_function(t & x) {
        x = nullptr;
    }
#else
    using boost::function;
    using boost::bind;
    using boost::ref;
    namespace placeholders {
        /// \todo this feels hacky, is there a better way?
        using ::_1;
        using ::_2;
    }
    
    // see above definition for more details on what this is and why it exists
    #define _websocketpp_ref(x) boost::ref(x)
    
    template <typename t>
    void clear_function(t & x) {
        x.clear();
    }
#endif

} // namespace lib
} // namespace websocketpp

#endif // websocketpp_common_functional_hpp
