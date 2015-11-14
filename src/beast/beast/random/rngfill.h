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

#ifndef beast_random_rngfill_h_included
#define beast_random_rngfill_h_included

#include <array>
#include <cstdint>
#include <cstring>
#include <beast/cxx14/type_traits.h> // <type_traits>

namespace beast {

template <class generator>
void
rngfill (void* buffer, std::size_t bytes,
    generator& g)
{
    using result_type =
        typename generator::result_type;
    while (bytes >= sizeof(result_type))
    {
        auto const v = g();
        std::memcpy(buffer, &v, sizeof(v));
        buffer = reinterpret_cast<
            std::uint8_t*>(buffer) + sizeof(v);
        bytes -= sizeof(v);
    }
    if (bytes > 0)
    {
        auto const v = g();
        std::memcpy(buffer, &v, bytes);
    }
}

template <class generator, std::size_t n,
    class = std::enable_if_t<
        n % sizeof(typename generator::result_type) == 0>>
void
rngfill (std::array<std::uint8_t, n>& a, generator& g)
{
    using result_type =
        typename generator::result_type;
    auto i = n / sizeof(result_type);
    result_type* p =
        reinterpret_cast<result_type*>(a.data());
    while (i--)
        *p++ = g();
}

} // beast

#endif
