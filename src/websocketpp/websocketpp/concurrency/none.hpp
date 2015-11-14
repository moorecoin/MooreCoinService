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

#ifndef websocketpp_concurrency_none_hpp
#define websocketpp_concurrency_none_hpp

namespace websocketpp {

/// concurrency handling support
namespace concurrency {

/// implementation for no-op locking primitives
namespace none_impl {
/// a fake mutex implementation that does nothing
class fake_mutex {
public:
    fake_mutex() {}
    ~fake_mutex() {}
};

/// a fake lock guard implementation that does nothing
class fake_lock_guard {
public:
    explicit fake_lock_guard(fake_mutex) {}
    ~fake_lock_guard() {}
};
} // namespace none_impl

/// stub concurrency policy that implements the interface using no-ops.
/**
 * this policy documents the concurrency policy interface using no-ops. it can
 * be used as a reference or base for building a new concurrency policy. it can
 * also be used as is to disable all locking for endpoints used in purely single
 * threaded programs.
 */
class none {
public:
    /// the type of a mutex primitive
    /**
     * std::mutex is an example.
     */
    typedef none_impl::fake_mutex mutex_type;

    /// the type of a scoped/raii lock primitive.
    /**
     * the scoped lock constructor should take a mutex_type as a parameter,
     * acquire that lock, and release it in its destructor. std::lock_guard is
     * an example.
     */
    typedef none_impl::fake_lock_guard scoped_lock_type;
};

} // namespace concurrency
} // namespace websocketpp

#endif // websocketpp_concurrency_async_hpp
