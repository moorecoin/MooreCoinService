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

#ifndef websocketpp_common_stdint_hpp
#define websocketpp_common_stdint_hpp

#ifndef __stdc_limit_macros
    #define __stdc_limit_macros 1
#endif

#if defined (_win32) && defined (_msc_ver) && (_msc_ver < 1600)
    #include <boost/cstdint.hpp>

    using boost::int8_t;
    using boost::int_least8_t;
    using boost::int_fast8_t;
    using boost::uint8_t;
    using boost::uint_least8_t;
    using boost::uint_fast8_t;

    using boost::int16_t;
    using boost::int_least16_t;
    using boost::int_fast16_t;
    using boost::uint16_t;
    using boost::uint_least16_t;
    using boost::uint_fast16_t;

    using boost::int32_t;
    using boost::int_least32_t;
    using boost::int_fast32_t;
    using boost::uint32_t;
    using boost::uint_least32_t;
    using boost::uint_fast32_t;

    #ifndef boost_no_int64_t
    using boost::int64_t;
    using boost::int_least64_t;
    using boost::int_fast64_t;
    using boost::uint64_t;
    using boost::uint_least64_t;
    using boost::uint_fast64_t;
    #endif
    using boost::intmax_t;
    using boost::uintmax_t;
#else
    #include <stdint.h>
#endif

#endif // websocketpp_common_stdint_hpp
