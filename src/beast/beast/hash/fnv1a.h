//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, howard hinnant <howard.hinnant@gmail.com>,
        vinnie falco <vinnie.falco@gmail.com

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

#ifndef beast_hash_fnv1a_h_included
#define beast_hash_fnv1a_h_included

#include <beast/utility/noexcept.h>
#include <cstddef>
#include <cstdint>
#include <beast/cxx14/type_traits.h> // <type_traits>

namespace beast {

// see http://www.isthe.com/chongo/tech/comp/fnv/
//
class fnv1a
{
private:
    std::uint64_t state_ = 14695981039346656037ull;

public:
    using result_type = std::size_t;

    fnv1a() = default;

    template <class seed,
        std::enable_if_t<
            std::is_unsigned<seed>::value>* = nullptr>
    explicit
    fnv1a (seed seed)
    {
        append (&seed, sizeof(seed));
    }

    void
    append (void const* key, std::size_t len) noexcept
    {
        unsigned char const* p =
            static_cast<unsigned char const*>(key);
        unsigned char const* const e = p + len;
        for (; p < e; ++p)
            state_ = (state_ ^ *p) * 1099511628211ull;
    }

    explicit
    operator std::size_t() noexcept
    {
        return static_cast<std::size_t>(state_);
    }
};

} // beast

#endif
