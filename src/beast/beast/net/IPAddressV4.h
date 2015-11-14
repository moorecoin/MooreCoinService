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

#ifndef beast_net_ipaddressv4_h_included
#define beast_net_ipaddressv4_h_included

#include <beast/hash/hash_append.h>

#include <cstdint>
#include <functional>
#include <ios>
#include <string>
#include <utility>

namespace beast {
namespace ip {

/** represents a version 4 ip address. */
struct addressv4
{
    /** default constructor represents the 'any' address. */
    addressv4 ();

    /** construct from a 32-bit unsigned.
        @note octets are formed in order from the msb to the lsb.       
    */
    explicit addressv4 (std::uint32_t value_);

    /** construct from four individual octets..
        @note the resulting address is a.b.c.d
    */
    addressv4 (std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d);

    /** create an address from an ipv4 address string in dotted decimal form.
        @return a pair with the address, and bool set to `true` on success.
    */
    static std::pair <addressv4, bool> from_string (std::string const& s);

    /** returns an address that represents 'any' address. */
    static addressv4 any ()
        { return addressv4(); }

    /** returns an address that represents the loopback address. */
    static addressv4 loopback ()
        { return addressv4 (0x7f000001); }

    /** returns an address that represents the broadcast address. */
    static addressv4 broadcast ()
        { return addressv4 (0xffffffff); }

    /** returns the broadcast address for the specified address. */
    static addressv4 broadcast (addressv4 const& address);

    /** returns the broadcast address corresponding to the address and mask. */
    static addressv4 broadcast (
        addressv4 const& address, addressv4 const& mask);

    /** returns `true` if this is a broadcast address. */
    bool is_broadcast () const
        { return *this == broadcast (*this); }

    /** returns the address class for the given address.
        @note class 'd' represents multicast addresses (224.*.*.*).
    */
    static char get_class (addressv4 const& address);

    /** returns the netmask for the address class or address. */
    /** @{ */
    static addressv4 netmask (char address_class);
    static addressv4 netmask (addressv4 const& v);
    /** @} */

    /** arithmetic comparison. */
    /** @{ */
    friend bool operator== (addressv4 const& lhs, addressv4 const& rhs)
        { return lhs.value == rhs.value; }
    friend bool operator<  (addressv4 const& lhs, addressv4 const& rhs)
        { return lhs.value < rhs.value; }

    friend bool operator!= (addressv4 const& lhs, addressv4 const& rhs)
        { return ! (lhs == rhs); }
    friend bool operator>  (addressv4 const& lhs, addressv4 const& rhs)
        { return rhs < lhs; }
    friend bool operator<= (addressv4 const& lhs, addressv4 const& rhs)
        { return ! (lhs > rhs); }
    friend bool operator>= (addressv4 const& lhs, addressv4 const& rhs)
        { return ! (rhs > lhs); }
    /** @} */

    /** array indexing for reading and writing indiviual octets. */
    /** @{ */
    template <bool isconst>
    class proxy
    {
    public:
        typedef typename std::conditional <
            isconst, std::uint32_t const*, std::uint32_t*>::type pointer;

        proxy (int shift, pointer value)
            : m_shift (shift)
            , m_value (value)
        {
        }

        operator std::uint8_t() const
        {
            return ((*m_value)>>m_shift) & 0xff;
        }

        template <typename integraltype>
        proxy& operator= (integraltype v)
        {
            (*m_value) =
                    ( (*m_value) & (~((0xff)<<m_shift)) )
                | ((v&0xff) << m_shift);

            return *this;
        }

    private:
        int m_shift;
        pointer m_value;
    };

    proxy <true> operator[] (std::size_t index) const;
    proxy <false> operator[] (std::size_t index);
    /** @} */

    /** the value as a 32 bit unsigned. */
    std::uint32_t value;
};

//------------------------------------------------------------------------------

/** returns `true` if this is a loopback address. */
bool is_loopback (addressv4 const& addr);

/** returns `true` if the address is unspecified. */
bool is_unspecified (addressv4 const& addr);

/** returns `true` if the address is a multicast address. */
bool is_multicast (addressv4 const& addr);

/** returns `true` if the address is a private unroutable address. */
bool is_private (addressv4 const& addr);

/** returns `true` if the address is a public routable address. */
bool is_public (addressv4 const& addr);

//------------------------------------------------------------------------------

/** returns the address represented as a string. */
std::string to_string (addressv4 const& addr);

/** output stream conversion. */
template <typename outputstream>
outputstream& operator<< (outputstream& os, addressv4 const& addr)
    { return os << to_string (addr); }

/** input stream conversion. */
std::istream& operator>> (std::istream& is, addressv4& addr);

}

template <>
struct is_contiguously_hashable<ip::addressv4>
    : public std::integral_constant<bool, sizeof(ip::addressv4) == sizeof(std::uint32_t)>
{
};

}

//------------------------------------------------------------------------------

namespace std {
/** std::hash support. */
template <>
struct hash <beast::ip::addressv4>
{
    std::size_t operator() (beast::ip::addressv4 const& addr) const
        { return addr.value; }
};
}

#endif
