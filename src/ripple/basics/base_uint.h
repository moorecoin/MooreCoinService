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

// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2011 the bitcoin developers
// distributed under the mit/x11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef ripple_protocol_base_uint_h_included
#define ripple_protocol_base_uint_h_included

#include <ripple/basics/byteorder.h>
#include <ripple/basics/blob.h>
#include <ripple/basics/strhex.h>
#include <ripple/basics/hardened_hash.h>
#include <beast/utility/zero.h>

#include <boost/functional/hash.hpp>

#include <functional>

using beast::zero;
using beast::zero;

namespace ripple {

// this class stores its values internally in big-endian form

template <std::size_t bits, class tag = void>
class base_uint
{
    static_assert ((bits % 32) == 0,
        "the length of a base_uint in bits must be a multiple of 32.");

    static_assert (bits >= 64,
        "the length of a base_uint in bits must be at least 64.");

protected:
    enum { width = bits / 32 };

    // this is really big-endian in byte order.
    // we sometimes use unsigned int for speed.

    // nikb todo: migrate to std::array
    unsigned int pn[width];

public:
    //--------------------------------------------------------------------------
    //
    // stl container interface
    //

    static std::size_t const        bytes = bits / 8;

    typedef std::size_t             size_type;
    typedef std::ptrdiff_t          difference_type;
    typedef unsigned char           value_type;
    typedef value_type*             pointer;
    typedef value_type&             reference;
    typedef value_type const*       const_pointer;
    typedef value_type const&       const_reference;
    typedef pointer                 iterator;
    typedef const_pointer           const_iterator;
    typedef std::reverse_iterator
        <iterator>                  reverse_iterator;
    typedef std::reverse_iterator
        <const_iterator>            const_reverse_iterator;

    typedef tag                     tag_type;

    pointer data() { return reinterpret_cast<pointer>(pn); }
    const_pointer data() const { return reinterpret_cast<const_pointer>(pn); }

    iterator begin() { return data(); }
    iterator end()   { return data()+bytes; }
    const_iterator begin()  const { return data(); }
    const_iterator end()    const { return data()+bytes; }
    const_iterator cbegin() const { return data(); }
    const_iterator cend()   const { return data()+bytes; }

    /** value hashing function.
        the seed prevents crafted inputs from causing degenarate parent containers.
    */
    typedef hardened_hash <> hasher;

    /** container equality testing function. */
    class key_equal
    {
    public:
        bool operator() (base_uint const& lhs, base_uint const& rhs) const
        {
            return lhs == rhs;
        }
    };

    //--------------------------------------------------------------------------

private:
    /** construct from a raw pointer.
        the buffer pointed to by `data` must be at least bits/8 bytes.

        @note the structure is used to disambiguate this from the std::uint64_t
              constructor: something like base_uint(0) is ambiguous.
    */
    // nikb todo remove the need for this constructor.
    struct voidhelper {};

    explicit base_uint (void const* data, voidhelper)
    {
        memcpy (&pn [0], data, bytes);
    }

public:
    base_uint () { *this = beast::zero; }

    explicit base_uint (blob const& vch)
    {
        assert (vch.size () == size ());

        if (vch.size () == size ())
            memcpy (pn, &vch[0], size ());
        else
            *this = beast::zero;
    }

    explicit base_uint (std::uint64_t b)
    {
        *this = b;
    }

    // nikb todo remove the need for this constructor - have a free function
    //           to handle the hex string parsing.
    explicit base_uint (std::string const& str)
    {
        sethex (str);
    }

    base_uint (base_uint<bits, tag> const& other) = default;

    template <class othertag>
    void copyfrom (base_uint<bits, othertag> const& other)
    {
        memcpy (&pn [0], other.data(), bytes);
    }

    /* construct from a raw pointer.
        the buffer pointed to by `data` must be at least bits/8 bytes.
    */
    static base_uint
    fromvoid (void const* data)
    {
        return base_uint (data, voidhelper ());
    }

    int signum() const
    {
        for (int i = 0; i < width; i++)
            if (pn[i] != 0)
                return 1;

        return 0;
    }

    bool operator! () const
    {
        return *this == beast::zero;
    }

    const base_uint operator~ () const
    {
        base_uint ret;

        for (int i = 0; i < width; i++)
            ret.pn[i] = ~pn[i];

        return ret;
    }

    base_uint& operator= (const base_uint& b)
    {
        for (int i = 0; i < width; i++)
            pn[i] = b.pn[i];

        return *this;
    }

    base_uint& operator= (std::uint64_t uhost)
    {
        *this = beast::zero;
        union
        {
            unsigned u[2];
            std::uint64_t ul;
        };
        // put in least significant bits.
        ul = htobe64 (uhost);
        pn[width-2] = u[0];
        pn[width-1] = u[1];
        return *this;
    }

    base_uint& operator^= (const base_uint& b)
    {
        for (int i = 0; i < width; i++)
            pn[i] ^= b.pn[i];

        return *this;
    }

    base_uint& operator&= (const base_uint& b)
    {
        for (int i = 0; i < width; i++)
            pn[i] &= b.pn[i];

        return *this;
    }

    base_uint& operator|= (const base_uint& b)
    {
        for (int i = 0; i < width; i++)
            pn[i] |= b.pn[i];

        return *this;
    }

    base_uint& operator++ ()
    {
        // prefix operator
        for (int i = width - 1; i >= 0; --i)
        {
            pn[i] = htobe32 (be32toh (pn[i]) + 1);

            if (pn[i] != 0)
                break;
        }

        return *this;
    }

    const base_uint operator++ (int)
    {
        // postfix operator
        const base_uint ret = *this;
        ++ (*this);

        return ret;
    }

    base_uint& operator-- ()
    {
        for (int i = width - 1; i >= 0; --i)
        {
            std::uint32_t prev = pn[i];
            pn[i] = htobe32 (be32toh (pn[i]) - 1);

            if (prev != 0)
                break;
        }

        return *this;
    }

    const base_uint operator-- (int)
    {
        // postfix operator
        const base_uint ret = *this;
        -- (*this);

        return ret;
    }

    base_uint& operator+= (const base_uint& b)
    {
        std::uint64_t carry = 0;

        for (int i = width; i--;)
        {
            std::uint64_t n = carry + be32toh (pn[i]) + be32toh (b.pn[i]);

            pn[i] = htobe32 (n & 0xffffffff);
            carry = n >> 32;
        }

        return *this;
    }

    template <class hasher>
    friend void hash_append(hasher& h, base_uint const& a) noexcept
    {
        using beast::hash_append;
        hash_append (h, a.pn);
    }

    bool sethexexact (const char* psz)
    {
        // must be precisely the correct number of hex digits
        unsigned char* pout  = begin ();

        for (int i = 0; i < sizeof (pn); ++i)
        {
            auto chigh = charunhex(*psz++);
            auto clow  = charunhex(*psz++);

            if (chigh == -1 || clow == -1)
                return false;

            *pout++ = (chigh << 4) | clow;
        }

        assert (*psz == 0);
        assert (pout == end ());

        return true;
    }

    // allow leading whitespace.
    // allow leading "0x".
    // to be valid must be '\0' terminated.
    bool sethex (const char* psz, bool bstrict = false)
    {
        // skip leading spaces
        if (!bstrict)
            while (isspace (*psz))
                psz++;

        // skip 0x
        if (!bstrict && psz[0] == '0' && tolower (psz[1]) == 'x')
            psz += 2;

        const unsigned char* pend   = reinterpret_cast<const unsigned char*> (psz);
        const unsigned char* pbegin = pend;

        // find end.
        while (charunhex(*pend) != -1)
            pend++;

        // take only last digits of over long string.
        if ((unsigned int) (pend - pbegin) > 2 * size ())
            pbegin = pend - 2 * size ();

        unsigned char* pout = end () - ((pend - pbegin + 1) / 2);

        *this = beast::zero;

        if ((pend - pbegin) & 1)
            *pout++ = charunhex(*pbegin++);

        while (pbegin != pend)
        {
            auto chigh = charunhex(*pbegin++);
            auto clow  = pbegin == pend
                            ? 0
                            : charunhex(*pbegin++);

            if (chigh == -1 || clow == -1)
                return false;

            *pout++ = (chigh << 4) | clow;
        }

        return !*pend;
    }

    bool sethex (std::string const& str, bool bstrict = false)
    {
        return sethex (str.c_str (), bstrict);
    }

    void sethexexact (std::string const& str)
    {
        sethexexact (str.c_str ());
    }

    unsigned int size () const
    {
        return sizeof (pn);
    }

    base_uint<bits, tag>& operator=(zero)
    {
        memset (&pn[0], 0, sizeof (pn));
        return *this;
    }

    // deprecated.
    bool iszero () const { return *this == beast::zero; }
    bool isnonzero () const { return *this != beast::zero; }
    void zero () { *this = beast::zero; }
};

using uint128 = base_uint<128>;
using uint160 = base_uint<160> ;
using uint256 = base_uint<256> ;

template <std::size_t bits, class tag>
inline int compare (
    base_uint<bits, tag> const& a, base_uint<bits, tag> const& b)
{
    auto ret = std::mismatch (a.cbegin (), a.cend (), b.cbegin ());

    if (ret.first == a.cend ())
        return 0;

    // a > b
    if (*ret.first > *ret.second)
        return 1;

    // a < b
    return -1;
}

template <std::size_t bits, class tag>
inline bool operator< (
    base_uint<bits, tag> const& a, base_uint<bits, tag> const& b)
{
    return compare (a, b) < 0;
}

template <std::size_t bits, class tag>
inline bool operator<= (
    base_uint<bits, tag> const& a, base_uint<bits, tag> const& b)
{
    return compare (a, b) <= 0;
}

template <std::size_t bits, class tag>
inline bool operator> (
    base_uint<bits, tag> const& a, base_uint<bits, tag> const& b)
{
    return compare (a, b) > 0;
}

template <std::size_t bits, class tag>
inline bool operator>= (
    base_uint<bits, tag> const& a, base_uint<bits, tag> const& b)
{
    return compare (a, b) >= 0;
}

template <std::size_t bits, class tag>
inline bool operator== (
    base_uint<bits, tag> const& a, base_uint<bits, tag> const& b)
{
    return compare (a, b) == 0;
}

template <std::size_t bits, class tag>
inline bool operator!= (
    base_uint<bits, tag> const& a, base_uint<bits, tag> const& b)
{
    return compare (a, b) != 0;
}

//------------------------------------------------------------------------------
template <std::size_t bits, class tag>
inline bool operator== (base_uint<bits, tag> const& a, std::uint64_t b)
{
    return a == base_uint<bits, tag>(b);
}

template <std::size_t bits, class tag>
inline bool operator!= (base_uint<bits, tag> const& a, std::uint64_t b)
{
    return !(a == b);
}

//------------------------------------------------------------------------------
template <std::size_t bits, class tag>
inline const base_uint<bits, tag> operator^ (
    base_uint<bits, tag> const& a, base_uint<bits, tag> const& b)
{
    return base_uint<bits, tag> (a) ^= b;
}

template <std::size_t bits, class tag>
inline const base_uint<bits, tag> operator& (
    base_uint<bits, tag> const& a, base_uint<bits, tag> const& b)
{
    return base_uint<bits, tag> (a) &= b;
}

template <std::size_t bits, class tag>
inline const base_uint<bits, tag> operator| (
    base_uint<bits, tag> const& a, base_uint<bits, tag> const& b)
{
    return base_uint<bits, tag> (a) |= b;
}

template <std::size_t bits, class tag>
inline const base_uint<bits, tag> operator+ (
    base_uint<bits, tag> const& a, base_uint<bits, tag> const& b)
{
    return base_uint<bits, tag> (a) += b;
}

//------------------------------------------------------------------------------
template <std::size_t bits, class tag>
inline std::string to_string (base_uint<bits, tag> const& a)
{
    return strhex (a.begin (), a.size ());
}

template <std::size_t bits, class tag>
inline std::ostream& operator<< (
    std::ostream& out, base_uint<bits, tag> const& u)
{
    return out << to_string (u);
}

} // rippled

namespace boost
{

template <std::size_t bits, class tag>
struct hash<ripple::base_uint<bits, tag>>
{
    using argument_type = ripple::base_uint<bits, tag>;

    std::size_t
    operator()(argument_type const& u) const
    {
        return ripple::hardened_hash<>{}(u);
    }
};

}  // boost

#endif
