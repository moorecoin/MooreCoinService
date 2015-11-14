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
 * this makefile was derived from a similar one included in the libjson project
 * it's authors were jonathan wallace and bernhard fluehmann.
 */

#ifndef boost_rng_hpp
#define boost_rng_hpp

#ifndef __stdc_limit_macros
  #define __stdc_limit_macros
#endif
#include <stdint.h>

#include <boost/random.hpp>
#include <boost/random/random_device.hpp>

namespace websocketpp_02 {
    class boost_rng {
    public:
        boost_rng();
        int32_t gen();
    private:
        boost::random::random_device m_rng;
        boost::random::variate_generator
        <
            boost::random::random_device&,
            boost::random::uniform_int_distribution<>
        > m_gen;
    };
}

#endif // boost_rng_hpp
