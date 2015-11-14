//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2014 ripple labs inc.

    permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    the  software is provided "as is" and the author disclaims all warranties
    with  regard  to  this  software  including  all  implied  warranties  of
    merchantability  and  fitness. in no event shall the author be liable for
    any  special ,  direct, indirect, or consequential damages or any damages
    whatsoever  resulting  from  loss  of use, data or profits, whether in an
    action  of  contract, negligence or other tortious action, arising out of
    or in connection with the use or performance of this software.
*/
//==============================================================================

#ifndef ripple_basics_hardened_hash_h_included
#define ripple_basics_hardened_hash_h_included

#include <beast/hash/hash_append.h>
#include <beast/hash/xxhasher.h>
#include <beast/cxx14/utility.h> // <utility>
#include <beast/cxx14/type_traits.h> // <type_traits>
#include <beast/utility/noexcept.h>
#include <beast/utility/static_initializer.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <random>
#include <unordered_map>
#include <unordered_set>

// when set to 1, makes the seed per-process instead
// of per default-constructed instance of hardened_hash
//
#ifndef ripple_no_hardened_hash_instance_seed
# ifdef __glibcxx__
#  define ripple_no_hardened_hash_instance_seed 1
# else
#  define ripple_no_hardened_hash_instance_seed 0
# endif
#endif

namespace ripple {

namespace detail {

using seed_pair = std::pair<std::uint64_t, std::uint64_t>;

template <bool = true>
seed_pair
make_seed_pair() noexcept
{
    struct state_t
    {
        std::mutex mutex;
        std::random_device rng;
        std::mt19937_64 gen;
        std::uniform_int_distribution <std::uint64_t> dist;

        state_t() : gen(rng()) {}
        // state_t(state_t const&) = delete;
        // state_t& operator=(state_t const&) = delete;
    };
    static beast::static_initializer <state_t> state;
    std::lock_guard <std::mutex> lock (state->mutex);
    return {state->dist(state->gen), state->dist(state->gen)};
}

}

template <class hashalgorithm, bool processseeded>
class basic_hardened_hash;

/**
 * seed functor once per process
*/
template <class hashalgorithm>
class basic_hardened_hash <hashalgorithm, true>
{
private:
    static
    detail::seed_pair const&
    init_seed_pair()
    {
        static beast::static_initializer <detail::seed_pair,
            basic_hardened_hash> const p (
                detail::make_seed_pair<>());
        return *p;
    }

public:
    using result_type = typename hashalgorithm::result_type;

    template <class t>
    result_type
    operator()(t const& t) const noexcept
    {
        std::uint64_t seed0;
        std::uint64_t seed1;
        std::tie(seed0, seed1) = init_seed_pair();
        hashalgorithm h(seed0, seed1);
        hash_append(h, t);
        return static_cast<result_type>(h);
    }
};

/**
 * seed functor once per construction
*/
template <class hashalgorithm>
class basic_hardened_hash<hashalgorithm, false>
{
private:
    detail::seed_pair m_seeds;

public:
    using result_type = typename hashalgorithm::result_type;

    basic_hardened_hash()
        : m_seeds (detail::make_seed_pair<>())
    {}

    template <class t>
    result_type
    operator()(t const& t) const noexcept
    {
        hashalgorithm h(m_seeds.first, m_seeds.second);
        hash_append(h, t);
        return static_cast<result_type>(h);
    }
};

//------------------------------------------------------------------------------

/** a std compatible hash adapter that resists adversarial inputs.
    for this to work, t must implement in its own namespace:

    @code

    template <class hasher>
    void
    hash_append (hasher& h, t const& t) noexcept
    {
        // hash_append each base and member that should
        //  participate in forming the hash
        using beast::hash_append;
        hash_append (h, static_cast<t::base1 const&>(t));
        hash_append (h, static_cast<t::base2 const&>(t));
        // ...
        hash_append (h, t.member1);
        hash_append (h, t.member2);
        // ...
    }

    @endcode

    do not use any version of murmur or cityhash for the hasher
    template parameter (the hashing algorithm).  for details
    see https://131002.net/siphash/#at
*/
#if ripple_no_hardened_hash_instance_seed
template <class hashalgorithm = beast::xxhasher>
    using hardened_hash = basic_hardened_hash<hashalgorithm, true>;
#else
template <class hashalgorithm = beast::xxhasher>
    using hardened_hash = basic_hardened_hash<hashalgorithm, false>;
#endif

} // ripple

#endif
