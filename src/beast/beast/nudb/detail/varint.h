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

#ifndef beast_nudb_varint_h_included
#define beast_nudb_varint_h_included

#include <beast/config/compilerconfig.h> // for beast_constexpr
#include <beast/nudb/detail/stream.h>
#include <cstdint>
#include <beast/cxx14/type_traits.h> // <type_traits>

namespace beast {
namespace nudb {
namespace detail {

// base128 varint format is from
// google protocol buffers:
// https://developers.google.com/protocol-buffers/docs/encoding#varints

// field tag
struct varint;

// metafuncton to return largest
// possible size of t represented as varint.
// t must be unsigned
template <class t,
    bool = std::is_unsigned<t>::value> 
struct varint_traits;

template <class t>
struct varint_traits<t, true>
{
    static std::size_t beast_constexpr max =
        (8 * sizeof(t) + 6) / 7;
};

// returns: number of bytes consumed or 0 on error,
//          if the buffer was too small or t overflowed.
//
template <class = void>
std::size_t
read_varint (void const* buf,
    std::size_t buflen, std::size_t& t)
{
    t = 0;
    std::uint8_t const* p =
        reinterpret_cast<
            std::uint8_t const*>(buf);
    std::size_t n = 0;
    while (p[n] & 0x80)
        if (++n >= buflen)
            return 0;
    if (++n > buflen)
        return 0;
    // special case for 0
    if (n == 1 && *p == 0)
    {
        t = 0;
        return 1;
    }
    auto const used = n;
    while (n--)
    {
        auto const d = p[n];
        auto const t0 = t;
        t *= 127;
        t += d & 0x7f;
        if (t <= t0)
            return 0; // overflow
    }
    return used;
}

template <class t,
    std::enable_if_t<std::is_unsigned<
        t>::value>* = nullptr>
std::size_t
size_varint (t v)
{
    std::size_t n = 0;
    do
    {
        v /= 127;
        ++n;
    }
    while (v != 0);
    return n;
}

template <class = void>
std::size_t
write_varint (void* p0, std::size_t v)
{
    std::uint8_t* p = reinterpret_cast<
        std::uint8_t*>(p0);
    do
    {
        std::uint8_t d =
            v % 127;
        v /= 127;
        if (v != 0)
            d |= 0x80;
        *p++ = d;
    }
    while (v != 0);
    return p - reinterpret_cast<
        std::uint8_t*>(p0);
}

// input stream

template <class t, std::enable_if_t<
    std::is_same<t, varint>::value>* = nullptr>
void
read (istream& is, std::size_t& u)
{
    auto p0 = is(1);
    auto p1 = p0;
    while (*p1++ & 0x80)
        is(1);
    read_varint(p0, p1 - p0, u);
}

// output stream

template <class t, std::enable_if_t<
    std::is_same<t, varint>::value>* = nullptr>
void
write (ostream& os, std::size_t t)
{
    write_varint(os.data(
        size_varint(t)), t);
}

} // detail
} // nudb
} // beast

#endif
