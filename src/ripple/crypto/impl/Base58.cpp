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
#include <ripple/crypto/base58.h>
#include <ripple/crypto/cautobn_ctx.h>
#include <ripple/crypto/cbignum.h>
#include <openssl/sha.h>
#include <stdexcept>
#include <string>

// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2011 the bitcoin developers
// distributed under the mit/x11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

namespace ripple {

uint256 sha256hash (unsigned char const* pbegin, unsigned char const* pend)
{
    uint256 hash1;
    sha256 (pbegin, pend - pbegin, hash1.begin ());

    uint256 hash2;
    sha256 (hash1.begin (), hash1.size (), hash2.begin());

    return hash2;
}

void base58::fourbyte_hash256 (void* out, void const* in, std::size_t bytes)
{
    auto p = static_cast <unsigned char const*>(in);
    uint256 hash (sha256hash (p, p + bytes));
    memcpy (out, hash.begin(), 4);
}

base58::alphabet const& base58::getbitcoinalphabet ()
{
    static alphabet alphabet (
        "123456789abcdefghjklmnpqrstuvwxyzabcdefghijkmnopqrstuvwxyz"
        );
    return alphabet;
}

base58::alphabet const& base58::getripplealphabet ()
{
    static alphabet alphabet (
        "rpshnaf39wbudneghjklm4pqrst7vwxyz2bcdecg65jkm8ofqi1tuvaxyz"
        );
    return alphabet;
}

std::string base58::raw_encode (unsigned char const* begin,
    unsigned char const* end, alphabet const& alphabet)
{
    cautobn_ctx pctx;
    cbignum bn58 = 58;
    cbignum bn0 = 0;

    // convert little endian data to bignum
    cbignum bn (begin, end);
    std::size_t const size (std::distance (begin, end));

    // convert bignum to std::string
    std::string str;
    // expected size increase from base58 conversion is approximately 137%
    // use 138% to be safe
    str.reserve (size * 138 / 100 + 1);
    cbignum dv;
    cbignum rem;

    while (bn > bn0)
    {
        if (!bn_div (&dv, &rem, &bn, &bn58, pctx))
            throw std::runtime_error ("encodebase58 : bn_div failed");

        bn = dv;
        unsigned int c = rem.getuint ();
        str += alphabet [c];
    }

    for (const unsigned char* p = end-2; p >= begin && *p == 0; p--)
        str += alphabet [0];

    // convert little endian std::string to big endian
    reverse (str.begin (), str.end ());
    return str;
}

//------------------------------------------------------------------------------

bool base58::raw_decode (char const* first, char const* last, void* dest,
    std::size_t size, bool checked, alphabet const& alphabet)
{
    cautobn_ctx pctx;
    cbignum bn58 = 58;
    cbignum bn = 0;
    cbignum bnchar;

    // convert big endian string to bignum
    for (char const* p = first; p != last; ++p)
    {
        int i (alphabet.from_char (*p));
        if (i == -1)
            return false;
        bnchar.setuint ((unsigned int) i);

        int const success (bn_mul (&bn, &bn, &bn58, pctx));

        assert (success);
        (void) success;

        bn += bnchar;
    }

    // get bignum as little endian data
    blob vchtmp = bn.getvch ();

    // trim off sign byte if present
    if (vchtmp.size () >= 2 && vchtmp.end ()[-1] == 0 && vchtmp.end ()[-2] >= 0x80)
        vchtmp.erase (vchtmp.end () - 1);

    char* const out (static_cast <char*> (dest));

    // count leading zeros
    int nleadingzeros = 0;
    for (char const* p = first; p!=last && *p==alphabet[0]; p++)
        nleadingzeros++;

    // verify that the size is correct
    if (vchtmp.size() + nleadingzeros != size)
        return false;

    // fill the leading zeros
    memset (out, 0, nleadingzeros);

    // copy little endian data to big endian
    std::reverse_copy (vchtmp.begin (), vchtmp.end (),
        out + nleadingzeros);

    if (checked)
    {
        char hash4 [4];
        fourbyte_hash256 (hash4, out, size - 4);
        if (memcmp (hash4, out + size - 4, 4) != 0)
            return false;
    }

    return true;
}

bool base58::decode (const char* psz, blob& vchret, alphabet const& alphabet)
{
    cautobn_ctx pctx;
    vchret.clear ();
    cbignum bn58 = 58;
    cbignum bn = 0;
    cbignum bnchar;

    while (isspace (*psz))
        psz++;

    // convert big endian string to bignum
    for (const char* p = psz; *p; p++)
    {
        // vfalco todo make this use the inverse table!
        //             or better yet ditch this and call raw_decode
        //
        const char* p1 = strchr (alphabet.chars(), *p);

        if (p1 == nullptr)
        {
            while (isspace (*p))
                p++;

            if (*p != '\0')
                return false;

            break;
        }

        bnchar.setuint (p1 - alphabet.chars());

        if (!bn_mul (&bn, &bn, &bn58, pctx))
            throw std::runtime_error ("decodebase58 : bn_mul failed");

        bn += bnchar;
    }

    // get bignum as little endian data
    blob vchtmp = bn.getvch ();

    // trim off sign byte if present
    if (vchtmp.size () >= 2 && vchtmp.end ()[-1] == 0 && vchtmp.end ()[-2] >= 0x80)
        vchtmp.erase (vchtmp.end () - 1);

    // restore leading zeros
    int nleadingzeros = 0;

    for (const char* p = psz; *p == alphabet.chars()[0]; p++)
        nleadingzeros++;

    vchret.assign (nleadingzeros + vchtmp.size (), 0);

    // convert little endian data to big endian
    std::reverse_copy (vchtmp.begin (), vchtmp.end (), vchret.end () - vchtmp.size ());
    return true;
}

bool base58::decode (std::string const& str, blob& vchret)
{
    return decode (str.c_str (), vchret);
}

bool base58::decodewithcheck (const char* psz, blob& vchret, alphabet const& alphabet)
{
    if (!decode (psz, vchret, alphabet))
        return false;

    auto size = vchret.size ();

    if (size < 4)
    {
        vchret.clear ();
        return false;
    }

    uint256 hash = sha256hash (vchret.data (), vchret.data () + size - 4);

    if (memcmp (&hash, &vchret.end ()[-4], 4) != 0)
    {
        vchret.clear ();
        return false;
    }

    vchret.resize (size - 4);
    return true;
}

bool base58::decodewithcheck (std::string const& str, blob& vchret, alphabet const& alphabet)
{
    return decodewithcheck (str.c_str (), vchret, alphabet);
}

}
