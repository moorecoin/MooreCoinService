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
#include <ripple/crypto/ecdsa.h>
#include <ripple/crypto/ecdsacanonical.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/hmac.h>
#include <cstdint>

namespace ripple  {

using openssl::ec_key;

static ec_key* new_initialized_ec_key()
{
    ec_key* key = ec_key_new_by_curve_name (nid_secp256k1);

    if (key == nullptr)
    {
        throw std::runtime_error ("new_initialized_ec_key() : ec_key_new_by_curve_name failed");
    }

    ec_key_set_conv_form (key, point_conversion_compressed);

    return key;
}

ec_key ecdsaprivatekey (uint256 const& serialized)
{
    ec_key* key = new_initialized_ec_key();

    bignum* bn = bn_bin2bn (serialized.begin(), serialized.size(), nullptr);

    if (bn == nullptr)
    {
        // leaks key
        throw std::runtime_error ("ec_key::ec_key: bn_bin2bn failed");
    }

    const bool ok = ec_key_set_private_key (key, bn);

    bn_clear_free (bn);

    if (! ok)
    {
        ec_key_free (key);
    }

    return ec_key::acquire ((ec_key::pointer_t) key);
}

ec_key ecdsapublickey (blob const& serialized)
{
    ec_key* key = new_initialized_ec_key();

    std::uint8_t const* begin = &serialized[0];

    if (o2i_ecpublickey (&key, &begin, serialized.size()) != nullptr)
    {
        ec_key_set_conv_form (key, point_conversion_compressed);
    }
    else
    {
        ec_key_free (key);
    }

    return ec_key::acquire ((ec_key::pointer_t) key);
}

blob ecdsasign (uint256 const& hash, const openssl::ec_key& key)
{
    blob result;

    unsigned char sig[128];
    unsigned int  siglen = sizeof sig - 1;

    const unsigned char* p = hash.begin();

    if (ecdsa_sign (0, p, hash.size(), sig, &siglen, (ec_key*) key.get()))
    {
        size_t newlen = siglen;

        makecanonicalecdsasig (sig, newlen);

        result.resize (newlen);
        memcpy (&result[0], sig, newlen);
    }

    return result;
}

static bool ecdsaverify (uint256 const& hash, std::uint8_t const* sig, size_t siglen, ec_key* key)
{
    // -1 = error, 0 = bad sig, 1 = good
    return ecdsa_verify (0, hash.begin(), hash.size(), sig, siglen, key) > 0;
}

bool ecdsaverify (uint256 const& hash, blob const& sig, const openssl::ec_key& key)
{
    return ecdsaverify (hash, sig.data(), sig.size(), (ec_key*) key.get());
}

} // ripple
