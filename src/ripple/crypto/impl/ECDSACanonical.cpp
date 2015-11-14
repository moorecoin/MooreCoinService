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
#include <ripple/crypto/ecdsacanonical.h>
#include <beast/unit_test/suite.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <algorithm>
#include <cstring>
#include <iterator>

namespace ripple {

namespace detail {

// a simple wrapper for a bignum to make it
// easier to allocate, construct, and free them
struct bignum
{
    bignum* num;

    bignum& operator=(bignum const&) = delete;

    bignum ()
        : num (bn_new ())
    {

    }

    bignum (const char *hex)
        : num (bn_new ())
    {
        bn_hex2bn (&num, hex);
    }

    bignum (unsigned char const* ptr, std::size_t len)
        : num (bn_new ())
    {
        set (ptr, len);
    }

    bignum (bignum const& other)
        : num (bn_new ())
    {
        if (bn_copy (num, other.num) == nullptr)
            bn_clear (num);
    }

    ~bignum ()
    {
        bn_free (num);
    }

    operator bignum* ()
    {
        return num;
    }

    operator bignum const* () const
    {
        return num;
    }

    bool set (unsigned char const* ptr, std::size_t len)
    {
        if (bn_bin2bn (ptr, len, num) == nullptr)
            return false;

        return true;
    }
};

class signaturepart
{
private:
    std::size_t m_skip;
    bignum m_bn;

public:
    signaturepart (unsigned char const* sig, std::size_t size)
        : m_skip (0)
    {
        // the format is: <02> <length of signature> <signature>
        if ((sig[0] != 0x02) || (size < 3))
            return;

        std::size_t const len (sig[1]);

        // claimed length can't be longer than amount of data available
        if (len > (size - 2))
            return;

        // signature must be between 1 and 33 bytes.
        if ((len < 1) || (len > 33))
            return;

        // the signature can't be negative
        if ((sig[2] & 0x80) != 0)
            return;

        // it can't be zero
        if ((sig[2] == 0) && (len == 1))
            return;

        // and it can't be padded
        if ((sig[2] == 0) && ((sig[3] & 0x80) == 0))
            return;

        // load the signature (ignore the marker prefix and length) and if
        // successful, count the number of bytes we consumed.
        if (m_bn.set (sig + 2, len))
            m_skip = len + 2;
    }

    bool valid () const
    {
        return m_skip != 0;
    }

    // the signature as a bignum
    bignum getbignum () const
    {
        return m_bn;
    }

    // returns the number of bytes to skip for this signature part
    std::size_t skip () const
    {
        return m_skip;
    }
};

// the secp256k1 modulus
static bignum const modulus (
    "fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141");

} // detail

/** determine whether a signature is canonical.
    canonical signatures are important to protect against signature morphing
    attacks.
    @param vsig the signature data
    @param siglen the length of the signature
    @param strict_param whether to enforce strictly canonical semantics

    @note for more details please see:
    https://ripple.com/wiki/transaction_malleability
    https://bitcointalk.org/index.php?topic=8392.msg127623#msg127623
    https://github.com/sipa/bitcoin/commit/58bc86e37fda1aec270bccb3df6c20fbd2a6591c
*/
bool iscanonicalecdsasig (void const* vsig, std::size_t siglen, ecdsa strict_param)
{
    // the format of a signature should be:
    // <30> <len> [ <02> <lenr> <r> ] [ <02> <lens> <s> ]

    unsigned char const* sig = reinterpret_cast<unsigned char const*> (vsig);

    if ((siglen < 8) || (siglen > 72))
        return false;

    if ((sig[0] != 0x30) || (sig[1] != (siglen - 2)))
        return false;

    // the first two bytes are verified. eat them.
    sig += 2;
    siglen -= 2;

    // verify the r signature
    detail::signaturepart sigr (sig, siglen);

    if (!sigr.valid ())
        return false;

    // eat the number of bytes we consumed
    sig += sigr.skip ();
    siglen -= sigr.skip ();

    // verify the s signature
    detail::signaturepart sigs (sig, siglen);

    if (!sigs.valid ())
        return false;

    // eat the number of bytes we consumed
    sig += sigs.skip ();
    siglen -= sigs.skip ();

    // nothing should remain at this point.
    if (siglen != 0)
        return false;

    // check whether r or s are greater than the modulus.
    auto bnr (sigr.getbignum ());
    auto bns (sigs.getbignum ());

    if (bn_cmp (bnr, detail::modulus) != -1)
        return false;

    if (bn_cmp (bns, detail::modulus) != -1)
        return false;

    // for a given signature, (r,s), the signature (r, n-s) is also valid. for
    // a signature to be fully-canonical, the smaller of these two values must
    // be specified. if operating in strict mode, check that as well.
    if (strict_param == ecdsa::strict)
    {
        detail::bignum ms;

        if (bn_sub (ms, detail::modulus, bns) == 0)
            return false;

        if (bn_cmp (bns, ms) == 1)
            return false;
    }

    return true;
}

/** convert a signature into strictly canonical form.
    given the signature (r, s) then (r, g-s) is also valid. for a signature
    to be canonical, the smaller of { s, g-s } must be specified.
    @param vsig the signature we wish to convert
    @param siglen the length of the signature
    @returns true if the signature was already canonical, false otherwise
*/
bool makecanonicalecdsasig (void* vsig, std::size_t& siglen)
{
    unsigned char * sig = reinterpret_cast<unsigned char *> (vsig);
    bool ret = false;

    // find internals
    int rlen = sig[3];
    int spos = rlen + 6, slen = sig[rlen + 5];

    detail::bignum origs, news;
    bn_bin2bn (&sig[spos], slen, origs);
    bn_sub (news, detail::modulus, origs);

    if (bn_cmp (origs, news) == 1)
    { // original signature is not fully canonical
        unsigned char newsbuf [64];
        int newslen = bn_bn2bin (news, newsbuf);

        if ((newsbuf[0] & 0x80) == 0)
        { // no extra padding byte is needed
            sig[1] = sig[1] - slen + newslen;
            sig[spos - 1] = newslen;
            std::memcpy (&sig[spos], newsbuf, newslen);
        }
        else
        { // an extra padding byte is needed
            sig[1] = sig[1] - slen + newslen + 1;
            sig[spos - 1] = newslen + 1;
            sig[spos] = 0;
            std::memcpy (&sig[spos + 1], newsbuf, newslen);
        }
        siglen = sig[1] + 2;
    }
    else
        ret = true;

    return ret;
}

template <class fwditer, class container>
void hex_to_binary (fwditer first, fwditer last, container& out)
{
    struct table
    {
        int val[256];
        table ()
        {
            std::fill (val, val+256, 0);
            for (int i = 0; i < 10; ++i)
                val ['0'+i] = i;
            for (int i = 0; i < 6; ++i)
            {
                val ['a'+i] = 10 + i;
                val ['a'+i] = 10 + i;
            }
        }
        int operator[] (int i)
        {
           return val[i];
        }
    };

    static table lut;
    out.reserve (std::distance (first, last) / 2);
    while (first != last)
    {
        auto const hi (lut[(*first++)]);
        auto const lo (lut[(*first++)]);
        out.push_back ((hi*16)+lo);
    }
}

}
