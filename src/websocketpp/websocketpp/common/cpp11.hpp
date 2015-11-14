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

#ifndef websocketpp_common_cpp11_hpp
#define websocketpp_common_cpp11_hpp

/**
 * this header sets up some constants based on the state of c++11 support
 */

// hide clang feature detection from other compilers
#ifndef __has_feature         // optional of course.
  #define __has_feature(x) 0  // compatibility with non-clang compilers.
#endif
#ifndef __has_extension
  #define __has_extension __has_feature // compatibility with pre-3.0 compilers.
#endif

// the code below attempts to use information provided by the build system or
// user supplied defines to selectively enable c++11 language and library
// features. in most cases features that are targeted individually may also be
// selectively disabled via an associated _websocketpp_noxxx_ define.

#if defined(_websocketpp_cpp11_stl_) || __cplusplus >= 201103l || defined(_websocketpp_cpp11_strict_)
    // this check tests for blanket c++11 coverage. it can be activated in one
    // of three ways. either the compiler itself reports that it is a full 
    // c++11 compiler via the __cplusplus macro or the user/build system
    // supplies one of the two preprocessor defines below:
    
    // _websocketpp_cpp11_strict_
    //
    // this define reports to websocket++ that 100% of the language and library
    // features of c++11 are available. using this define on a non-c++11
    // compiler will result in problems.
    
    // _websocketpp_cpp11_stl_ 
    //
    // this define enables *most* c++11 options that were implemented early on
    // by compilers. it is typically used for compilers that have many, but not
    // all c++11 features. it should be safe to use on gcc 4.7-4.8 and perhaps
    // earlier. 
    #ifndef _websocketpp_noexcept_token_
        #define _websocketpp_noexcept_token_ noexcept
    #endif
    #ifndef _websocketpp_constexpr_token_
        #define _websocketpp_constexpr_token_ constexpr
    #endif
    #ifndef _websocketpp_initializer_lists_
        #define _websocketpp_initializer_lists_
    #endif
    #ifndef _websocketpp_nullptr_token_
        #define _websocketpp_nullptr_token_ nullptr
    #endif
    
    #ifndef __gnuc__
        // gcc as of version 4.9 (latest) does not support std::put_time yet.
        // so ignore it
        #define _websocketpp_puttime_
    #endif
#else
    // in the absence of a blanket define, try to use compiler versions or
    // feature testing macros to selectively enable what we can.

    // test for noexcept
    #ifndef _websocketpp_noexcept_token_
        #ifdef _websocketpp_noexcept_
            // build system says we have noexcept
            #define _websocketpp_noexcept_token_ noexcept
        #else
            #if __has_feature(cxx_noexcept)
                // clang feature detect says we have noexcept
                #define _websocketpp_noexcept_token_ noexcept
            #else
                // assume we don't have noexcept
                #define _websocketpp_noexcept_token_
            #endif
        #endif
    #endif

    // test for constexpr
    #ifndef _websocketpp_constexpr_token_
        #ifdef _websocketpp_constexpr_
            // build system says we have constexpr
            #define _websocketpp_constexpr_token_ constexpr
        #else
            #if __has_feature(cxx_constexpr)
                // clang feature detect says we have constexpr
                #define _websocketpp_constexpr_token_ constexpr
            #else
                // assume we don't have constexpr
                #define _websocketpp_constexpr_token_
            #endif
        #endif
    #endif

    // enable initializer lists on clang when available.
    #if __has_feature(cxx_generalized_initializers) && !defined(_websocketpp_initializer_lists_)
        #define _websocketpp_initializer_lists_
    #endif
    
    // test for nullptr
    #ifndef _websocketpp_nullptr_token_
        #ifdef _websocketpp_nullptr_
            // build system says we have nullptr
            #define _websocketpp_nullptr_token_ nullptr
        #else
            #if __has_feature(cxx_nullptr)
                // clang feature detect says we have nullptr
                #define _websocketpp_nullptr_token_ nullptr
            #elif _msc_ver >= 1600
                // visual studio version that has nullptr
                #define _websocketpp_nullptr_token_ nullptr
            #else
                // assume we don't have nullptr
                #define _websocketpp_nullptr_token_ 0
            #endif
        #endif
    #endif
#endif

#endif // websocketpp_common_cpp11_hpp
