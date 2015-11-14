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

#ifndef beast_net_ipendpoint_h_included
#define beast_net_ipendpoint_h_included

#include <beast/net/ipaddress.h>
#include <beast/hash/hash_append.h>
#include <beast/hash/uhash.h>
#include <cstdint>
#include <ios>
#include <string>

namespace beast {
namespace ip {

typedef std::uint16_t port;

/** a version-independent ip address and port combination. */
class endpoint
{
public:
    /** create an unspecified endpoint. */
    endpoint ();

    /** create an endpoint from the address and optional port. */
    explicit endpoint (address const& addr, port port = 0);

    /** create an endpoint from a string.
        if the port is omitted, the endpoint will have a zero port.
        @return a pair with the endpoint, and bool set to `true` on success.
    */
    static std::pair <endpoint, bool> from_string_checked (std::string const& s);
    static endpoint from_string (std::string const& s);
    static endpoint from_string_altform (std::string const& s);

    /** returns a string representing the endpoint. */
    std::string to_string () const;

    /** returns the port number on the endpoint. */
    port port () const
        { return m_port; }

    /** returns a new endpoint with a different port. */
    endpoint at_port (port port) const
        { return endpoint (m_addr, port); }

    /** returns the address portion of this endpoint. */
    address const& address () const
        { return m_addr; }

    /** convenience accessors for the address part. */
    /** @{ */
    bool is_v4 () const
        { return m_addr.is_v4(); }
    bool is_v6 () const
        { return m_addr.is_v6(); }
    addressv4 const& to_v4 () const
        { return m_addr.to_v4 (); }
    addressv6 const& to_v6 () const
        { return m_addr.to_v6 (); }
    /** @} */

    /** arithmetic comparison. */
    /** @{ */
    friend bool operator== (endpoint const& lhs, endpoint const& rhs);
    friend bool operator<  (endpoint const& lhs, endpoint const& rhs);

    friend bool operator!= (endpoint const& lhs, endpoint const& rhs)
        { return ! (lhs == rhs); }
    friend bool operator>  (endpoint const& lhs, endpoint const& rhs)
        { return rhs < lhs; }
    friend bool operator<= (endpoint const& lhs, endpoint const& rhs)
        { return ! (lhs > rhs); }
    friend bool operator>= (endpoint const& lhs, endpoint const& rhs)
        { return ! (rhs > lhs); }
    /** @} */

    template <class hasher>
    friend
    void
    hash_append (hasher& h, endpoint const& endpoint)
    {
        using beast::hash_append;
        hash_append(h, endpoint.m_addr, endpoint.m_port);
    }

private:
    address m_addr;
    port m_port;
};

//------------------------------------------------------------------------------

// properties

/** returns `true` if the endpoint is a loopback address. */
inline bool  is_loopback (endpoint const& endpoint)
    { return is_loopback (endpoint.address ()); }

/** returns `true` if the endpoint is unspecified. */
inline bool  is_unspecified (endpoint const& endpoint)
    { return is_unspecified (endpoint.address ()); }

/** returns `true` if the endpoint is a multicast address. */
inline bool  is_multicast (endpoint const& endpoint)
    { return is_multicast (endpoint.address ()); }

/** returns `true` if the endpoint is a private unroutable address. */
inline bool  is_private (endpoint const& endpoint)
    { return is_private (endpoint.address ()); }

/** returns `true` if the endpoint is a public routable address. */
inline bool  is_public (endpoint const& endpoint)
    { return is_public (endpoint.address ()); }

//------------------------------------------------------------------------------

/** returns the endpoint represented as a string. */
inline std::string to_string (endpoint const& endpoint)
    { return endpoint.to_string(); }

/** output stream conversion. */
template <typename outputstream>
outputstream& operator<< (outputstream& os, endpoint const& endpoint)
{
    os << to_string (endpoint);
    return os;
}

/** input stream conversion. */
std::istream& operator>> (std::istream& is, endpoint& endpoint);

}
}

//------------------------------------------------------------------------------

namespace std {
/** std::hash support. */
template <>
struct hash <beast::ip::endpoint>
{
    std::size_t operator() (beast::ip::endpoint const& endpoint) const
        { return beast::uhash<>{} (endpoint); }
};
}

namespace boost {
/** boost::hash support. */
template <>
struct hash <beast::ip::endpoint>
{
    std::size_t operator() (beast::ip::endpoint const& endpoint) const
        { return beast::uhash<>{} (endpoint); }
};
}

#endif
