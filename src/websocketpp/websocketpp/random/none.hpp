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

#ifndef websocketpp_random_none_hpp
#define websocketpp_random_none_hpp

namespace websocketpp {
/// random number generation policies
namespace random {
/// stub rng policy that always returns 0
namespace none {

/// thread safe stub "random" integer generator.
/**
 * this template class provides a random integer stub. the interface mimics the
 * websocket++ rng generator classes but the generater function always returns
 * zero. this can be used to stub out the rng for unit and performance testing.
 *
 * call operator() to generate the next number
 */
template <typename int_type>
class int_generator {
    public:
        int_generator() {}

        /// advances the engine's state and returns the generated value
        int_type operator()() {
            return 0;
        }
};

} // namespace none
} // namespace random
} // namespace websocketpp

#endif //websocketpp_random_none_hpp
