//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#if beast_include_beastconfig
#include "../../beastconfig.h"
#endif

#include <beast/net/ipaddressv4.h>
#include <beast/net/detail/parse.h>

#include <sstream>
#include <stdexcept>
    
namespace beast {
namespace ip {

addressv4::addressv4 ()
    : value (0)
{
}

addressv4::addressv4 (std::uint32_t value_)
    : value (value_)
{
}

addressv4::addressv4 (std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d)
    : value ((a<<24)|(b<<16)|(c<<8)|d)
{
}

std::pair <addressv4, bool> addressv4::from_string (std::string const& s)
{
    std::stringstream is (s);
    addressv4 addr;
    is >> addr;
    if (! is.fail() && is.rdbuf()->in_avail() == 0)
        return std::make_pair (addr, true);
    return std::make_pair (addressv4 (), false);
}

addressv4 addressv4::broadcast (addressv4 const& address)
{
    return broadcast (address, netmask (address));
}

addressv4 addressv4::broadcast (
    addressv4 const& address, addressv4 const& mask)
{
    return addressv4 (address.value | (mask.value ^ 0xffffffff));
}

char addressv4::get_class (addressv4 const& addr)
{
    static char const* table = "aaaabbcd";
    return table [(addr.value & 0xe0000000) >> 29];
}

addressv4 addressv4::netmask (char address_class)
{
    switch (address_class)
    {
    case 'a': return addressv4 (0xff000000);
    case 'b': return addressv4 (0xffff0000);
    case 'c': return addressv4 (0xffffff00);
    case 'd':
    default:
        break;
    }
    return addressv4 (0xffffffff);
}

addressv4 addressv4::netmask (addressv4 const& addr)
{
    return netmask (get_class (addr));
}

addressv4::proxy <true> addressv4::operator[] (std::size_t index) const
{
    switch (index)
    {
    default:
        throw std::out_of_range ("bad array index");
    case 0: return proxy <true> (24, &value);
    case 1: return proxy <true> (16, &value);
    case 2: return proxy <true> ( 8, &value);
    case 3: return proxy <true> ( 0, &value);
    };
};

addressv4::proxy <false> addressv4::operator[] (std::size_t index)
{
    switch (index)
    {
    default:
        throw std::out_of_range ("bad array index");
    case 0: return proxy <false> (24, &value);
    case 1: return proxy <false> (16, &value);
    case 2: return proxy <false> ( 8, &value);
    case 3: return proxy <false> ( 0, &value);
    };
};

//------------------------------------------------------------------------------

bool is_loopback (addressv4 const& addr)
{
    return (addr.value & 0xff000000) == 0x7f000000;
}

bool is_unspecified (addressv4 const& addr)
{
    return addr.value == 0;
}

bool is_multicast (addressv4 const& addr)
{
    return (addr.value & 0xf0000000) == 0xe0000000;
}

bool is_private (addressv4 const& addr)
{
    return
        ((addr.value & 0xff000000) == 0x0a000000) || // prefix /8,    10.  #.#.#
        ((addr.value & 0xfff00000) == 0xac100000) || // prefix /12   172. 16.#.# - 172.31.#.#
        ((addr.value & 0xffff0000) == 0xc0a80000) || // prefix /16   192.168.#.#
        is_loopback (addr);
}

bool is_public (addressv4 const& addr)
{
    return
        ! is_private (addr) &&
        ! is_multicast (addr) &&
        (addr != addressv4::broadcast (addr));
}

//------------------------------------------------------------------------------

std::string to_string (addressv4 const& addr)
{
    std::string s;
    s.reserve (15);
    s =
        std::to_string (addr[0]) + "." +
        std::to_string (addr[1]) + "." +
        std::to_string (addr[2]) + "." +
        std::to_string (addr[3]);
    return s;
}

std::istream& operator>> (std::istream& is, addressv4& addr)
{
    std::uint8_t octet [4];
    is >> ip::detail::integer (octet [0]);
    for (int i = 1; i < 4; ++i)
    {
        if (!is || !ip::detail::expect (is, '.'))
            return is;
        is >> ip::detail::integer (octet [i]);
        if (!is)
            return is;
    }
    addr = addressv4 (octet[0], octet[1], octet[2], octet[3]);
    return is;
}

}
}
