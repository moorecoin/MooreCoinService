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

#ifndef beast_net_ipaddress_h_included
#define beast_net_ipaddress_h_included

#include <beast/net/ipaddressv4.h>
#include <beast/net/ipaddressv6.h>
#include <beast/hash/hash_append.h>
#include <beast/hash/uhash.h>
#include <beast/utility/noexcept.h>
#include <boost/functional/hash.hpp>
#include <cassert>
#include <cstdint>
#include <ios>
#include <string>
#include <sstream>
#include <typeinfo>

//------------------------------------------------------------------------------

namespace beast {
namespace ip {

/** a version-independent ip address.
    this object can represent either an ipv4 or ipv6 address.
*/
class address
{
public:
    /** create an unspecified ipv4 address. */
    address ()
        : m_type (ipv4)
    {
    }

    /** create an ipv4 address. */
    address (addressv4 const& addr)
        : m_type (ipv4)
        , m_v4 (addr)
    {
    }

    /** create an ipv6 address. */
    address (addressv6 const& addr)
        : m_type (ipv6)
        , m_v6 (addr)
    {
    }

    /** assign a copy from another address in any format. */
    /** @{ */
    address&
    operator= (addressv4 const& addr)
    {
        m_type = ipv4;
        m_v6 = addressv6();
        m_v4 = addr;
        return *this;
    }

    address&
    operator= (addressv6 const& addr)
    {
        m_type = ipv6;
        m_v4 = addressv4();
        m_v6 = addr;
        return *this;
    }
    /** @} */

    /** create an address from a string.
        @return a pair with the address, and bool set to `true` on success.
    */
    static
    std::pair <address, bool>
    from_string (std::string const& s);

    /** returns a string representing the address. */
    std::string
    to_string () const
    {
        return (is_v4 ())
            ? ip::to_string (to_v4())
            : ip::to_string (to_v6());
    }

    /** returns `true` if this address represents an ipv4 address. */
    bool
    is_v4 () const noexcept
    {
        return m_type == ipv4;
    }

    /** returns `true` if this address represents an ipv6 address. */
    bool
    is_v6() const noexcept
    {
        return m_type == ipv6;
    }

    /** returns the ipv4 address.
        precondition:
            is_v4() == `true`
    */
    addressv4 const&
    to_v4 () const
    {
        if (!is_v4 ())
            throw std::bad_cast();
        return m_v4;
    }

    /** returns the ipv6 address.
        precondition:
            is_v6() == `true`
    */
    addressv6 const&
    to_v6 () const
    {
        if (!is_v6 ())
            throw std::bad_cast();
        return m_v6;
    }

    template <class hasher>
    friend
    void
    hash_append(hasher& h, address const& addr) noexcept
    {
        using beast::hash_append;
        if (addr.is_v4 ())
            hash_append(h, addr.to_v4 ());
        else if (addr.is_v6 ())
            hash_append(h, addr.to_v6 ());
        else
            assert (false);
    }

    /** arithmetic comparison. */
    /** @{ */
    friend
    bool
    operator== (address const& lhs, address const& rhs)
    {
        if (lhs.is_v4 ())
        {
            if (rhs.is_v4 ())
                return lhs.to_v4() == rhs.to_v4();
        }
        else
        {
            if (rhs.is_v6 ())
                return lhs.to_v6() == rhs.to_v6();
        }

        return false;
    }

    friend
    bool
    operator< (address const& lhs, address const& rhs)
    {
        if (lhs.m_type < rhs.m_type)
            return true;
        if (lhs.is_v4 ())
            return lhs.to_v4() < rhs.to_v4();
        return lhs.to_v6() < rhs.to_v6();
    }

    friend
    bool
    operator!= (address const& lhs, address const& rhs)
    {
        return ! (lhs == rhs);
    }
    
    friend
    bool
    operator>  (address const& lhs, address const& rhs)
    {
        return rhs < lhs;
    }
    
    friend
    bool
    operator<= (address const& lhs, address const& rhs)
    {
        return ! (lhs > rhs);
    }
    
    friend
    bool
    operator>= (address const& lhs, address const& rhs)
    {
        return ! (rhs > lhs);
    }
    /** @} */

private:
    enum type
    {
        ipv4,
        ipv6
    };

    type m_type;
    addressv4 m_v4;
    addressv6 m_v6;
};

//------------------------------------------------------------------------------

// properties

/** returns `true` if this is a loopback address. */
inline
bool
is_loopback (address const& addr)
{
    return (addr.is_v4 ())
        ? is_loopback (addr.to_v4 ())
        : is_loopback (addr.to_v6 ());
}

/** returns `true` if the address is unspecified. */
inline
bool
is_unspecified (address const& addr)
{
    return (addr.is_v4 ())
        ? is_unspecified (addr.to_v4 ())
        : is_unspecified (addr.to_v6 ());
}

/** returns `true` if the address is a multicast address. */
inline
bool
is_multicast (address const& addr)
{
    return (addr.is_v4 ())
        ? is_multicast (addr.to_v4 ())
        : is_multicast (addr.to_v6 ());
}

/** returns `true` if the address is a private unroutable address. */
inline
bool
is_private (address const& addr)
{
    return (addr.is_v4 ())
        ? is_private (addr.to_v4 ())
        : is_private (addr.to_v6 ());
}

/** returns `true` if the address is a public routable address. */
inline
bool
is_public (address const& addr)
{
    return (addr.is_v4 ())
        ? is_public (addr.to_v4 ())
        : is_public (addr.to_v6 ());
}

//------------------------------------------------------------------------------

/** returns the address represented as a string. */
inline std::string to_string (address const& addr)
{
    return addr.to_string ();
}

/** output stream conversion. */
template <typename outputstream>
outputstream&
operator<< (outputstream& os, address const& addr)
{
    return os << to_string (addr);
}

/** input stream conversion. */
inline
std::istream&
operator>> (std::istream& is, address& addr)
{
    // vfalco todo support ipv6!
    addressv4 addrv4;
    is >> addrv4;
    addr = address (addrv4);
    return is;
}

inline
std::pair <address, bool>
address::from_string (std::string const& s)
{
    std::stringstream is (s);
    address addr;
    is >> addr;
    if (! is.fail() && is.rdbuf()->in_avail() == 0)
        return std::make_pair (addr, true);
    return std::make_pair (address (), false);
}

}
}

//------------------------------------------------------------------------------

namespace std {
template <>
struct hash <beast::ip::address>
{
    std::size_t
    operator() (beast::ip::address const& addr) const
    {
        return beast::uhash<>{} (addr);
    }
};
}

namespace boost {
template <>
struct hash <beast::ip::address>
{
    std::size_t
    operator() (beast::ip::address const& addr) const
    {
        return beast::uhash<>{} (addr);
    }
};
}

#endif
