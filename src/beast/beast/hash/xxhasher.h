//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_hash_xxhasher_h_included
#define beast_hash_xxhasher_h_included

#ifndef beast_no_xxhash
#define beast_no_xxhash 0
#endif

#if ! beast_no_xxhash

#include <beast/hash/impl/xxhash.h>
#include <beast/utility/noexcept.h>
#include <beast/cxx14/type_traits.h> // <type_traits>
#include <cstddef>

namespace beast {

class xxhasher
{
private:
    // requires 64-bit std::size_t
    static_assert(sizeof(std::size_t)==8, "");

    detail::xxh64_state_t state_;

public:
    using result_type = std::size_t;

    xxhasher() noexcept
    {
        detail::xxh64_reset (&state_, 1);
    }

    template <class seed,
        std::enable_if_t<
            std::is_unsigned<seed>::value>* = nullptr>
    explicit
    xxhasher (seed seed)
    {
        detail::xxh64_reset (&state_, seed);
    }

    template <class seed,
        std::enable_if_t<
            std::is_unsigned<seed>::value>* = nullptr>
    xxhasher (seed seed, seed)
    {
        detail::xxh64_reset (&state_, seed);
    }

    void
    append (void const* key, std::size_t len) noexcept
    {
        detail::xxh64_update (&state_, key, len);
    }

    explicit
    operator std::size_t() noexcept
    {
        return detail::xxh64_digest(&state_);
    }
};

} // beast

#endif

#endif
