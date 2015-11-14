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

#ifndef websocketpp_random_random_device_hpp
#define websocketpp_random_random_device_hpp

#include <websocketpp/common/random.hpp>

namespace websocketpp {
namespace random {
/// rng policy based on std::random_device or boost::random_device
namespace random_device {

/// thread safe non-deterministic random integer generator.
/**
 * this template class provides thread safe non-deterministic random integer
 * generation. numbers are produced in a uniformly distributed range from the
 * smallest to largest value that int_type can store.
 *
 * thread-safety is provided via locking based on the concurrency template
 * parameter.
 *
 * non-deterministic rng is provided via websocketpp::lib which uses either
 * c++11 or boost 1.47+'s random_device class.
 *
 * call operator() to generate the next number
 */
template <typename int_type, typename concurrency>
class int_generator {
    public:
        typedef typename concurrency::scoped_lock_type scoped_lock_type;
        typedef typename concurrency::mutex_type mutex_type;

        /// constructor
        //mac todo: figure out if signed types present a range problem
        int_generator() {}

        /// advances the engine's state and returns the generated value
        int_type operator()() {
            scoped_lock_type guard(m_lock);
            return m_dis(m_rng);
        }
    private:


        lib::random_device m_rng;
        lib::uniform_int_distribution<int_type> m_dis;

        mutex_type m_lock;
};

} // namespace random_device
} // namespace random
} // namespace websocketpp

#endif //websocketpp_random_random_device_hpp
