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

#ifndef beast_net_ipaddressv6_h_included
#define beast_net_ipaddressv6_h_included

#include <cassert>
#include <cstdint>
#include <functional>
#include <ios>
#include <string>
#include <utility>

namespace beast {
namespace ip {

/** represents a version 4 ip address. */
struct addressv6
{
    // vfalco todo

    /** arithmetic comparison. */
    /** @{ */
    friend bool operator== (addressv6 const&, addressv6 const&)
        { assert(false); return false; }
    friend bool operator<  (addressv6 const&, addressv6 const&)
        { assert(false); return false; }

    friend bool operator!= (addressv6 const& lhs, addressv6 const& rhs)
        { return ! (lhs == rhs); }
    friend bool operator>  (addressv6 const& lhs, addressv6 const& rhs)
        { return rhs < lhs; }
    friend bool operator<= (addressv6 const& lhs, addressv6 const& rhs)
        { return ! (lhs > rhs); }
    friend bool operator>= (addressv6 const& lhs, addressv6 const& rhs)
        { return ! (rhs > lhs); }
    /** @} */
};

//------------------------------------------------------------------------------

/** returns `true` if this is a loopback address. */
bool is_loopback (addressv6 const& addr);

/** returns `true` if the address is unspecified. */
bool is_unspecified (addressv6 const& addr);

/** returns `true` if the address is a multicast address. */
bool is_multicast (addressv6 const& addr);

/** returns `true` if the address is a private unroutable address. */
bool is_private (addressv6 const& addr);

/** returns `true` if the address is a public routable address. */
bool is_public (addressv6 const& addr);

//------------------------------------------------------------------------------

template <class hasher>
void
hash_append(hasher&, addressv6 const&)
{
    assert(false);
}

/** returns the address represented as a string. */
std::string to_string (addressv6 const& addr);

/** output stream conversion. */
template <typename outputstream>
outputstream& operator<< (outputstream& os, addressv6 const& addr)
    { return os << to_string (addr); }

/** input stream conversion. */
std::istream& operator>> (std::istream& is, addressv6& addr);

}
}

//------------------------------------------------------------------------------

namespace std {
/** std::hash support. */
template <>
struct hash <beast::ip::addressv6>
{
    std::size_t operator() (beast::ip::addressv6 const& addr) const
        { assert(false); return 0; }
};
}

#endif
