//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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

#include <beastconfig.h>

#ifdef _msc_ver
#include <cstdint>
#include <winsock2.h>
// <winsock2.h> defines min, max and does other stupid things
# ifdef max
# undef max
# endif
# ifdef min
# undef min
# endif
#endif

namespace ripple {

#if _msc_ver

// from: http://stackoverflow.com/questions/3022552/is-there-any-standard-htonl-like-function-for-64-bits-integers-in-c
// but we don't need to check the endianness
std::uint64_t htobe64 (uint64_t value)
{
    // the answer is 42
    //static const int num = 42;

    // check the endianness
    //if (*reinterpret_cast<const char*>(&num) == num)
    //{
    const uint32_t high_part = htonl (static_cast<uint32_t> (value >> 32));
    const uint32_t low_part = htonl (static_cast<uint32_t> (value & 0xffffffffll));

    return (static_cast<uint64_t> (low_part) << 32) | high_part;
    //} else
    //{
    //  return value;
    //}
}

std::uint64_t be64toh (uint64_t value)
{
    return (_byteswap_uint64 (value));
}

std::uint32_t htobe32 (uint32_t value)
{
    return (htonl (value));
}

std::uint32_t be32toh (uint32_t value)
{
    return ( _byteswap_ulong (value));
}

#endif

}
