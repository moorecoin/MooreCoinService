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

#include <beastconfig.h>
#include <ripple/crypto/cbignum.h>
#include <ripple/crypto/cautobn_ctx.h>
#include <stdexcept>
#include <string>

namespace ripple {

cbignum::cbignum ()
{
    bn_init (this);
}

cbignum::cbignum (const cbignum& b)
{
    bn_init (this);

    if (!bn_copy (this, &b))
    {
        bn_clear_free (this);
        throw std::runtime_error ("cbignum::cbignum(const cbignum&) : bn_copy failed");
    }
}

cbignum& cbignum::operator= (const cbignum& b)
{
    if (!bn_copy (this, &b))
        throw std::runtime_error ("cbignum::operator= : bn_copy failed");

    return (*this);
}

cbignum::~cbignum ()
{
    bn_clear_free (this);
}

cbignum::cbignum (char n)
{
    bn_init (this);

    if (n >= 0) setulong (n);
    else setint64 (n);
}
cbignum::cbignum (short n)
{
    bn_init (this);

    if (n >= 0) setulong (n);
    else setint64 (n);
}
cbignum::cbignum (int n)
{
    bn_init (this);

    if (n >= 0) setulong (n);
    else setint64 (n);
}
cbignum::cbignum (long n)
{
    bn_init (this);

    if (n >= 0) setulong (n);
    else setint64 (n);
}
cbignum::cbignum (long long n)
{
    bn_init (this);
    setint64 (n);
}
cbignum::cbignum (unsigned char n)
{
    bn_init (this);
    setulong (n);
}
cbignum::cbignum (unsigned short n)
{
    bn_init (this);
    setulong (n);
}
cbignum::cbignum (unsigned int n)
{
    bn_init (this);
    setulong (n);
}
cbignum::cbignum (unsigned long long n)
{
    bn_init (this);
    setuint64 (n);
}
cbignum::cbignum (uint256 n)
{
    bn_init (this);
    setuint256 (n);
}

cbignum::cbignum (blob const& vch)
{
    bn_init (this);
    setvch (&vch.front(), &vch.back()+1);
}

cbignum::cbignum (unsigned char const* begin, unsigned char const* end)
{
    bn_init (this);
    setvch (begin, end);
}

void cbignum::setuint (unsigned int n)
{
    setulong (static_cast<unsigned long> (n));
}

unsigned int cbignum::getuint () const
{
    return bn_get_word (this);
}

int cbignum::getint () const
{
    unsigned long n = bn_get_word (this);

    if (!bn_is_negative (this))
        return (n > int_max ? int_max : n);
    else
        return (n > int_max ? int_min : - (int)n);
}

void cbignum::setint64 (std::int64_t n)
{
    unsigned char pch[sizeof (n) + 6];
    unsigned char* p = pch + 4;
    bool fnegative = false;

    if (n < (std::int64_t)0)
    {
        n = -n;
        fnegative = true;
    }

    bool fleadingzeroes = true;

    for (int i = 0; i < 8; i++)
    {
        unsigned char c = (n >> 56) & 0xff;
        n <<= 8;

        if (fleadingzeroes)
        {
            if (c == 0)
                continue;

            if (c & 0x80)
                *p++ = (fnegative ? 0x80 : 0);
            else if (fnegative)
                c |= 0x80;

            fleadingzeroes = false;
        }

        *p++ = c;
    }

    unsigned int nsize = p - (pch + 4);
    pch[0] = (nsize >> 24) & 0xff;
    pch[1] = (nsize >> 16) & 0xff;
    pch[2] = (nsize >> 8) & 0xff;
    pch[3] = (nsize) & 0xff;
    bn_mpi2bn (pch, p - pch, this);
}

std::uint64_t cbignum::getuint64 () const
{
#if (ulong_max > uint_max)
    return static_cast<std::uint64_t> (getulong ());
#else
    int len = bn_num_bytes (this);

    if (len > 8)
        throw std::runtime_error ("bn getuint64 overflow");

    unsigned char buf[8];
    memset (buf, 0, sizeof (buf));
    bn_bn2bin (this, buf + 8 - len);
    return
        static_cast<std::uint64_t> (buf[0]) << 56 | static_cast<std::uint64_t> (buf[1]) << 48 |
        static_cast<std::uint64_t> (buf[2]) << 40 | static_cast<std::uint64_t> (buf[3]) << 32 |
        static_cast<std::uint64_t> (buf[4]) << 24 | static_cast<std::uint64_t> (buf[5]) << 16 |
        static_cast<std::uint64_t> (buf[6]) << 8 | static_cast<std::uint64_t> (buf[7]);
#endif
}

void cbignum::setuint64 (std::uint64_t n)
{
#if (ulong_max > uint_max)
    setulong (static_cast<unsigned long> (n));
#else
    unsigned char buf[8];
    buf[0] = static_cast<unsigned char> ((n >> 56) & 0xff);
    buf[1] = static_cast<unsigned char> ((n >> 48) & 0xff);
    buf[2] = static_cast<unsigned char> ((n >> 40) & 0xff);
    buf[3] = static_cast<unsigned char> ((n >> 32) & 0xff);
    buf[4] = static_cast<unsigned char> ((n >> 24) & 0xff);
    buf[5] = static_cast<unsigned char> ((n >> 16) & 0xff);
    buf[6] = static_cast<unsigned char> ((n >> 8) & 0xff);
    buf[7] = static_cast<unsigned char> ((n) & 0xff);
    bn_bin2bn (buf, 8, this);
#endif
}

void cbignum::setuint256 (uint256 const& n)
{
    bn_bin2bn (n.begin (), n.size (), this);
}

uint256 cbignum::getuint256 ()
{
    uint256 ret;
    unsigned int size = bn_num_bytes (this);

    if (size > ret.size ())
        return ret;

    bn_bn2bin (this, ret.begin () + (ret.size () - bn_num_bytes (this)));
    return ret;
}

void cbignum::setvch (unsigned char const* begin, unsigned char const* end)
{
    std::size_t const size (std::distance (begin, end));
    blob vch2 (size + 4);
    unsigned int nsize (size);
    // bignum's byte stream format expects 4 bytes of
    // big endian size data info at the front
    vch2[0] = (nsize >> 24) & 0xff;
    vch2[1] = (nsize >> 16) & 0xff;
    vch2[2] = (nsize >> 8) & 0xff;
    vch2[3] = (nsize >> 0) & 0xff;
    // swap data to big endian
    std::reverse_copy (begin, end, vch2.begin() + 4);
    bn_mpi2bn (&vch2[0], vch2.size (), this);
}

void cbignum::setvch (blob const& vch)
{
    setvch (&vch.front(), &vch.back()+1);
}

blob cbignum::getvch () const
{
    unsigned int nsize = bn_bn2mpi (this, nullptr);

    if (nsize < 4)
        return blob ();

    blob vch (nsize);
    bn_bn2mpi (this, &vch[0]);
    vch.erase (vch.begin (), vch.begin () + 4);
    reverse (vch.begin (), vch.end ());
    return vch;
}

cbignum& cbignum::setcompact (unsigned int ncompact)
{
    unsigned int nsize = ncompact >> 24;
    blob vch (4 + nsize);
    vch[3] = nsize;

    if (nsize >= 1) vch[4] = (ncompact >> 16) & 0xff;

    if (nsize >= 2) vch[5] = (ncompact >> 8) & 0xff;

    if (nsize >= 3) vch[6] = (ncompact >> 0) & 0xff;

    bn_mpi2bn (&vch[0], vch.size (), this);
    return *this;
}

unsigned int cbignum::getcompact () const
{
    unsigned int nsize = bn_bn2mpi (this, nullptr);
    blob vch (nsize);
    nsize -= 4;
    bn_bn2mpi (this, &vch[0]);
    unsigned int ncompact = nsize << 24;

    if (nsize >= 1) ncompact |= (vch[4] << 16);

    if (nsize >= 2) ncompact |= (vch[5] << 8);

    if (nsize >= 3) ncompact |= (vch[6] << 0);

    return ncompact;
}

void cbignum::sethex (std::string const& str)
{
    // skip 0x
    const char* psz = str.c_str ();

    while (isspace (*psz))
        psz++;

    bool fnegative = false;

    if (*psz == '-')
    {
        fnegative = true;
        psz++;
    }

    if (psz[0] == '0' && tolower (psz[1]) == 'x')
        psz += 2;

    while (isspace (*psz))
        psz++;

    // hex string to bignum
    static char phexdigit[256] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
        0, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    *this = 0;

    while (isxdigit (*psz))
    {
        *this <<= 4;
        int n = phexdigit[ (int) * psz++];
        *this += n;
    }

    if (fnegative)
        *this = 0 - *this;
}

std::string cbignum::tostring (int nbase) const
{
    cautobn_ctx pctx;
    cbignum bnbase = nbase;
    cbignum bn0 = 0;
    std::string str;
    cbignum bn = *this;
    bn_set_negative (&bn, false);
    cbignum dv;
    cbignum rem;

    if (bn_cmp (&bn, &bn0) == 0)
        return "0";

    while (bn_cmp (&bn, &bn0) > 0)
    {
        if (!bn_div (&dv, &rem, &bn, &bnbase, pctx))
            throw std::runtime_error ("cbignum::tostring() : bn_div failed");

        bn = dv;
        unsigned int c = rem.getuint ();
        str += "0123456789abcdef"[c];
    }

    if (bn_is_negative (this))
        str += "-";

    reverse (str.begin (), str.end ());
    return str;
}

std::string cbignum::gethex () const
{
    return tostring (16);
}

bool cbignum::operator! () const
{
    return bn_is_zero (this);
}

cbignum& cbignum::operator+= (const cbignum& b)
{
    if (!bn_add (this, this, &b))
        throw std::runtime_error ("cbignum::operator+= : bn_add failed");

    return *this;
}

cbignum& cbignum::operator-= (const cbignum& b)
{
    *this = *this - b;
    return *this;
}

cbignum& cbignum::operator*= (const cbignum& b)
{
    cautobn_ctx pctx;

    if (!bn_mul (this, this, &b, pctx))
        throw std::runtime_error ("cbignum::operator*= : bn_mul failed");

    return *this;
}

cbignum& cbignum::operator/= (const cbignum& b)
{
    *this = *this / b;
    return *this;
}

cbignum& cbignum::operator%= (const cbignum& b)
{
    *this = *this % b;
    return *this;
}

cbignum& cbignum::operator<<= (unsigned int shift)
{
    if (!bn_lshift (this, this, shift))
        throw std::runtime_error ("cbignum:operator<<= : bn_lshift failed");

    return *this;
}

cbignum& cbignum::operator>>= (unsigned int shift)
{
    // note: bn_rshift segfaults on 64-bit if 2^shift is greater than the number
    //   if built on ubuntu 9.04 or 9.10, probably depends on version of openssl
    cbignum a = 1;
    a <<= shift;

    if (bn_cmp (&a, this) > 0)
    {
        *this = 0;
        return *this;
    }

    if (!bn_rshift (this, this, shift))
        throw std::runtime_error ("cbignum:operator>>= : bn_rshift failed");

    return *this;
}


cbignum& cbignum::operator++ ()
{
    // prefix operator
    if (!bn_add (this, this, bn_value_one ()))
        throw std::runtime_error ("cbignum::operator++ : bn_add failed");

    return *this;
}

const cbignum cbignum::operator++ (int)
{
    // postfix operator
    const cbignum ret = *this;
    ++ (*this);
    return ret;
}

cbignum& cbignum::operator-- ()
{
    // prefix operator
    cbignum r;

    if (!bn_sub (&r, this, bn_value_one ()))
        throw std::runtime_error ("cbignum::operator-- : bn_sub failed");

    *this = r;
    return *this;
}

const cbignum cbignum::operator-- (int)
{
    // postfix operator
    const cbignum ret = *this;
    -- (*this);
    return ret;
}

void cbignum::setulong (unsigned long n)
{
    if (!bn_set_word (this, n))
        throw std::runtime_error ("cbignum conversion from unsigned long : bn_set_word failed");
}

unsigned long cbignum::getulong () const
{
    return bn_get_word (this);
}

const cbignum operator+ (const cbignum& a, const cbignum& b)
{
    cbignum r;

    if (!bn_add (&r, &a, &b))
        throw std::runtime_error ("cbignum::operator+ : bn_add failed");

    return r;
}

const cbignum operator- (const cbignum& a, const cbignum& b)
{
    cbignum r;

    if (!bn_sub (&r, &a, &b))
        throw std::runtime_error ("cbignum::operator- : bn_sub failed");

    return r;
}

const cbignum operator- (const cbignum& a)
{
    cbignum r (a);
    bn_set_negative (&r, !bn_is_negative (&r));
    return r;
}

const cbignum operator* (const cbignum& a, const cbignum& b)
{
    cautobn_ctx pctx;
    cbignum r;

    if (!bn_mul (&r, &a, &b, pctx))
        throw std::runtime_error ("cbignum::operator* : bn_mul failed");

    return r;
}

const cbignum operator/ (const cbignum& a, const cbignum& b)
{
    cautobn_ctx pctx;
    cbignum r;

    if (!bn_div (&r, nullptr, &a, &b, pctx))
        throw std::runtime_error ("cbignum::operator/ : bn_div failed");

    return r;
}

const cbignum operator% (const cbignum& a, const cbignum& b)
{
    cautobn_ctx pctx;
    cbignum r;

    if (!bn_mod (&r, &a, &b, pctx))
        throw std::runtime_error ("cbignum::operator% : bn_div failed");

    return r;
}

const cbignum operator<< (const cbignum& a, unsigned int shift)
{
    cbignum r;

    if (!bn_lshift (&r, &a, shift))
        throw std::runtime_error ("cbignum:operator<< : bn_lshift failed");

    return r;
}

const cbignum operator>> (const cbignum& a, unsigned int shift)
{
    cbignum r = a;
    r >>= shift;
    return r;
}

bool operator== (const cbignum& a, const cbignum& b)
{
    return (bn_cmp (&a, &b) == 0);
}

bool operator!= (const cbignum& a, const cbignum& b)
{
    return (bn_cmp (&a, &b) != 0);
}

bool operator<= (const cbignum& a, const cbignum& b)
{
    return (bn_cmp (&a, &b) <= 0);
}

bool operator>= (const cbignum& a, const cbignum& b)
{
    return (bn_cmp (&a, &b) >= 0);
}

bool operator<  (const cbignum& a, const cbignum& b)
{
    return (bn_cmp (&a, &b) < 0);
}

bool operator>  (const cbignum& a, const cbignum& b)
{
    return (bn_cmp (&a, &b) > 0);
}

#if (ulong_max > uint_max)

int bn_add_word64 (bignum* bn, std::uint64_t word)
{
    return bn_add_word (bn, word);
}

int bn_sub_word64 (bignum* bn, std::uint64_t word)
{
    return bn_sub_word (bn, word);
}

int bn_mul_word64 (bignum* bn, std::uint64_t word)
{
    return bn_mul_word (bn, word);
}

std::uint64_t bn_div_word64 (bignum* bn, std::uint64_t word)
{
    return bn_div_word (bn, word);
}

#else

int bn_add_word64 (bignum* a, std::uint64_t w)
{
    cbignum bn (w);
    return bn_add (a, &bn, a);
}

int bn_sub_word64 (bignum* a, std::uint64_t w)
{
    cbignum bn (w);
    return bn_sub (a, &bn, a);
}

int bn_mul_word64 (bignum* a, std::uint64_t w)
{
    cbignum bn (w);
    cautobn_ctx ctx;
    return bn_mul (a, &bn, a, ctx);
}

std::uint64_t bn_div_word64 (bignum* a, std::uint64_t w)
{
    cbignum bn (w);
    cautobn_ctx ctx;
    return (bn_div (a, nullptr, a, &bn, ctx) == 1) ? 0 : ((std::uint64_t) - 1);
}

#endif

}
