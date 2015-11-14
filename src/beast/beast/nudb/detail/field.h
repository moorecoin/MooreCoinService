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

#ifndef beast_nudb_field_h_included
#define beast_nudb_field_h_included

#include <beast/nudb/detail/stream.h>
#include <beast/config/compilerconfig.h> // for beast_constexpr
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <beast/cxx14/type_traits.h> // <type_traits>

namespace beast {
namespace nudb {
namespace detail {

// a 24-bit integer
struct uint24_t;

// a 48-bit integer
struct uint48_t;

// these metafunctions describe the binary format of fields on disk

template <class t>
struct field;

template <>
struct field <std::uint8_t>
{
    static std::size_t beast_constexpr size = 1;
    static std::size_t beast_constexpr max = 0xff;
};

template <>
struct field <std::uint16_t>
{
    static std::size_t beast_constexpr size = 2;
    static std::size_t beast_constexpr max = 0xffff;
};

template <>
struct field <uint24_t>
{
    static std::size_t beast_constexpr size = 3;
    static std::size_t beast_constexpr max = 0xffffff;
};

template <>
struct field <std::uint32_t>
{
    static std::size_t beast_constexpr size = 4;
    static std::size_t beast_constexpr max = 0xffffffff;
};

template <>
struct field <uint48_t>
{
    static std::size_t beast_constexpr size = 6;
    static std::size_t beast_constexpr max = 0x0000ffffffffffff;
};

template <>
struct field <std::uint64_t>
{
    static std::size_t beast_constexpr size = 8;
    static std::size_t beast_constexpr max = 0xffffffffffffffff;
};

// read field from memory

template <class t, class u, std::enable_if_t<
    std::is_same<t, std::uint8_t>::value>* = nullptr>
void
readp (void const* v, u& u)
{
    std::uint8_t const* p =
        reinterpret_cast<std::uint8_t const*>(v);
    u = *p;
}

template <class t, class u, std::enable_if_t<
    std::is_same<t, std::uint16_t>::value>* = nullptr>
void
readp (void const* v, u& u)
{
    std::uint8_t const* p =
        reinterpret_cast<std::uint8_t const*>(v);
    t t;
    t =  t(*p++)<< 8;
    t =  t(*p  )      | t;
    u = t;
}

template <class t, class u, std::enable_if_t<
    std::is_same<t, uint24_t>::value>* = nullptr>
void
readp (void const* v, u& u)
{
    std::uint8_t const* p =
        reinterpret_cast<std::uint8_t const*>(v);
    std::uint32_t t;
    t =  std::uint32_t(*p++)<<16;
    t = (std::uint32_t(*p++)<< 8) | t;
    t =  std::uint32_t(*p  )      | t;
    u = t;
}

template <class t, class u, std::enable_if_t<
    std::is_same<t, std::uint32_t>::value>* = nullptr>
void
readp (void const* v, u& u)
{
    std::uint8_t const* p =
        reinterpret_cast<std::uint8_t const*>(v);
    t t;
    t =  t(*p++)<<24;
    t = (t(*p++)<<16) | t;
    t = (t(*p++)<< 8) | t;
    t =  t(*p  )      | t;
    u = t;
}

template <class t, class u, std::enable_if_t<
    std::is_same<t, uint48_t>::value>* = nullptr>
void
readp (void const* v, u& u)
{
    std::uint8_t const* p =
        reinterpret_cast<std::uint8_t const*>(v);
    std::uint64_t t;
    t = (std::uint64_t(*p++)<<40);
    t = (std::uint64_t(*p++)<<32) | t;
    t = (std::uint64_t(*p++)<<24) | t;
    t = (std::uint64_t(*p++)<<16) | t;
    t = (std::uint64_t(*p++)<< 8) | t;
    t =  std::uint64_t(*p  )      | t;
    u = t;
}

template <class t, class u, std::enable_if_t<
    std::is_same<t, std::uint64_t>::value>* = nullptr>
void
readp (void const* v, u& u)
{
    std::uint8_t const* p =
        reinterpret_cast<std::uint8_t const*>(v);
    t t;
    t =  t(*p++)<<56;
    t = (t(*p++)<<48) | t;
    t = (t(*p++)<<40) | t;
    t = (t(*p++)<<32) | t;
    t = (t(*p++)<<24) | t;
    t = (t(*p++)<<16) | t;
    t = (t(*p++)<< 8) | t;
    t =  t(*p  )      | t;
    u = t;
}

// read field from istream

template <class t, class u>
void
read (istream& is, u& u)
{
    readp<t>(is.data(field<t>::size), u);
}

// write field to ostream

template <class t, class u, std::enable_if_t<
    std::is_same<t, std::uint8_t>::value>* = nullptr>
void
write (ostream& os, u const& u)
{
    std::uint8_t* p =
        os.data(field<t>::size);
    *p = u;
}

template <class t, class u, std::enable_if_t<
    std::is_same<t, std::uint16_t>::value>* = nullptr>
void
write (ostream& os, u const& u)
{
    t t = u;
    std::uint8_t* p =
        os.data(field<t>::size);
    *p++ = (t>> 8)&0xff;
    *p   =  t     &0xff;
}

template <class t, class u, std::enable_if_t<
    std::is_same<t, uint24_t>::value>* = nullptr>
void
write (ostream& os, u const& u)
{
    t t = u;
    std::uint8_t* p =
        os.data(field<t>::size);
    *p++ = (t>>16)&0xff;
    *p++ = (t>> 8)&0xff;
    *p   =  t     &0xff;
}

template <class t, class u, std::enable_if_t<
    std::is_same<t, std::uint32_t>::value>* = nullptr>
void
write (ostream& os, u const& u)
{
    t t = u;
    std::uint8_t* p =
        os.data(field<t>::size);
    *p++ = (t>>24)&0xff;
    *p++ = (t>>16)&0xff;
    *p++ = (t>> 8)&0xff;
    *p   =  t     &0xff;
}

template <class t, class u, std::enable_if_t<
    std::is_same<t, uint48_t>::value>* = nullptr>
void
write (ostream& os, u const& u)
{
    std::uint64_t const t = u;
    std::uint8_t* p =
        os.data(field<t>::size);
    *p++ = (t>>40)&0xff;
    *p++ = (t>>32)&0xff;
    *p++ = (t>>24)&0xff;
    *p++ = (t>>16)&0xff;
    *p++ = (t>> 8)&0xff;
    *p   =  t     &0xff;
}

template <class t, class u, std::enable_if_t<
    std::is_same<t, std::uint64_t>::value>* = nullptr>
void
write (ostream& os, u const& u)
{
    t t = u;
    std::uint8_t* p =
        os.data(field<t>::size);
    *p++ = (t>>56)&0xff;
    *p++ = (t>>48)&0xff;
    *p++ = (t>>40)&0xff;
    *p++ = (t>>32)&0xff;
    *p++ = (t>>24)&0xff;
    *p++ = (t>>16)&0xff;
    *p++ = (t>> 8)&0xff;
    *p   =  t     &0xff;
}

} // detail
} // nudb
} // beast

#endif
