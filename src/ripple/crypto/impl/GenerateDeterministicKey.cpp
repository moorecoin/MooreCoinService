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
#include <ripple/crypto/generatedeterministickey.h>
#include <ripple/crypto/base58.h>
#include <ripple/crypto/cbignum.h>
#include <array>
#include <string>
#include <openssl/ec.h>
#include <openssl/pem.h>
#include <openssl/sha.h>

namespace ripple {

using openssl::ec_key;

uint256
getsha512half (void const* data, std::size_t bytes)
{
    uint256 j[2];
    sha512 (reinterpret_cast<unsigned char const*>(data), bytes, 
        reinterpret_cast<unsigned char*> (j));
    return j[0];
}

template <class fwdit>
void
copy_uint32 (fwdit out, std::uint32_t v)
{
    *out++ =  v >> 24;
    *out++ = (v >> 16) & 0xff;
    *out++ = (v >>  8) & 0xff;
    *out   =  v        & 0xff;
}

// #define ec_debug

// functions to add support for deterministic ec keys

// --> seed
// <-- private root generator + public root generator
ec_key generaterootdeterministickey (uint128 const& seed)
{
    bn_ctx* ctx = bn_ctx_new ();

    if (!ctx) return ec_key::invalid;

    ec_key* pkey = ec_key_new_by_curve_name (nid_secp256k1);

    if (!pkey)
    {
        bn_ctx_free (ctx);
        return ec_key::invalid;
    }

    ec_key_set_conv_form (pkey, point_conversion_compressed);

    bignum* order = bn_new ();

    if (!order)
    {
        bn_ctx_free (ctx);
        ec_key_free (pkey);
        return ec_key::invalid;
    }

    if (!ec_group_get_order (ec_key_get0_group (pkey), order, ctx))
    {
        assert (false);
        bn_free (order);
        ec_key_free (pkey);
        bn_ctx_free (ctx);
        return ec_key::invalid;
    }

    // find non-zero private key less than the curve's order
    bignum* privkey = nullptr;
    std::uint32_t seq = 0;
    do
    {
        // buf: 0                seed               16  seq  20
        //      |<--------------------------------->|<------>|
        std::array<std::uint8_t, 20> buf;
        std::copy(seed.begin(), seed.end(), buf.begin());
        copy_uint32 (buf.begin() + 16, seq++);
        uint256 root = getsha512half (buf.data(), buf.size());
        std::fill (buf.begin(), buf.end(), 0); // security erase
        privkey = bn_bin2bn ((const unsigned char*) &root, sizeof (root), privkey);
        if (privkey == nullptr)
        {
            ec_key_free (pkey);
            bn_free (order);
            bn_ctx_free (ctx);
            return ec_key::invalid;
        }

        root.zero(); // security erase
    }
    while (bn_is_zero (privkey) || (bn_cmp (privkey, order) >= 0));

    bn_free (order);

    if (!ec_key_set_private_key (pkey, privkey))
    {
        // set the random point as the private key
        assert (false);
        ec_key_free (pkey);
        bn_clear_free (privkey);
        bn_ctx_free (ctx);
        return ec_key::invalid;
    }

    ec_point* pubkey = ec_point_new (ec_key_get0_group (pkey));

    if (!ec_point_mul (ec_key_get0_group (pkey), pubkey, privkey, nullptr, nullptr, ctx))
    {
        // compute the corresponding public key point
        assert (false);
        bn_clear_free (privkey);
        ec_point_free (pubkey);
        ec_key_free (pkey);
        bn_ctx_free (ctx);
        return ec_key::invalid;
    }

    bn_clear_free (privkey);

    if (!ec_key_set_public_key (pkey, pubkey))
    {
        assert (false);
        ec_point_free (pubkey);
        ec_key_free (pkey);
        bn_ctx_free (ctx);
        return ec_key::invalid;
    }

    ec_point_free (pubkey);

    bn_ctx_free (ctx);

#ifdef ec_debug
    assert (ec_key_check_key (pkey) == 1); // caution: this check is *very* expensive
#endif
    return ec_key::acquire ((ec_key::pointer_t) pkey);
}

// take ripple address.
// --> root public generator (consumes)
// <-- root public generator in ec format
static ec_key* generaterootpubkey (bignum* pubgenerator)
{
    if (pubgenerator == nullptr)
    {
        assert (false);
        return nullptr;
    }

    ec_key* pkey = ec_key_new_by_curve_name (nid_secp256k1);

    if (!pkey)
    {
        bn_free (pubgenerator);
        return nullptr;
    }

    ec_key_set_conv_form (pkey, point_conversion_compressed);

    ec_point* pubpoint = ec_point_bn2point (ec_key_get0_group (pkey), pubgenerator, nullptr, nullptr);
    bn_free (pubgenerator);

    if (!pubpoint)
    {
        assert (false);
        ec_key_free (pkey);
        return nullptr;
    }

    if (!ec_key_set_public_key (pkey, pubpoint))
    {
        assert (false);
        ec_point_free (pubpoint);
        ec_key_free (pkey);
        return nullptr;
    }

    ec_point_free (pubpoint);

    return pkey;
}

// --> public generator
static bignum* makehash (blob const& pubgen, int seq, bignum const* order)
{
    int subseq = 0;
    bignum* ret = nullptr;

    assert(pubgen.size() == 33);
    do
    {
        // buf: 0          pubgen             33 seq   37 subseq  41
        //      |<--------------------------->|<------>|<-------->|
        std::array<std::uint8_t, 41> buf;
        std::copy (pubgen.begin(), pubgen.end(), buf.begin());
        copy_uint32 (buf.begin() + 33, seq);
        copy_uint32 (buf.begin() + 37, subseq++);
        uint256 root = getsha512half (buf.data(), buf.size());
        std::fill(buf.begin(), buf.end(), 0); // security erase
        ret = bn_bin2bn ((const unsigned char*) &root, sizeof (root), ret);
        if (!ret) return nullptr;
        root.zero(); // security erase
    }
    while (bn_is_zero (ret) || (bn_cmp (ret, order) >= 0));

    return ret;
}

// --> public generator
ec_key generatepublicdeterministickey (blob const& pubgen, int seq)
{
    // publickey(n) = rootpublickey ec_point_+ hash(pubhash|seq)*point
    bignum* generator = bn_bin2bn (
        pubgen.data(),
        pubgen.size(),
        nullptr);

    if (generator == nullptr)
        return ec_key::invalid;

    ec_key*         rootkey     = generaterootpubkey (generator);
    const ec_point* rootpubkey  = ec_key_get0_public_key (rootkey);
    bn_ctx*         ctx         = bn_ctx_new ();
    ec_key*         pkey        = ec_key_new_by_curve_name (nid_secp256k1);
    ec_point*       newpoint    = 0;
    bignum*         order       = 0;
    bignum*         hash        = 0;
    bool            success     = true;

    if (!ctx || !pkey)  success = false;

    if (success)
        ec_key_set_conv_form (pkey, point_conversion_compressed);

    if (success)
    {
        newpoint    = ec_point_new (ec_key_get0_group (pkey));

        if (!newpoint)   success = false;
    }

    if (success)
    {
        order       = bn_new ();

        if (!order || !ec_group_get_order (ec_key_get0_group (pkey), order, ctx))
            success = false;
    }

    // calculate the private additional key.
    if (success)
    {
        hash        = makehash (pubgen, seq, order);

        if (!hash)   success = false;
    }

    if (success)
    {
        // calculate the corresponding public key.
        ec_point_mul (ec_key_get0_group (pkey), newpoint, hash, nullptr, nullptr, ctx);

        // add the master public key and set.
        ec_point_add (ec_key_get0_group (pkey), newpoint, newpoint, rootpubkey, ctx);
        ec_key_set_public_key (pkey, newpoint);
    }

    if (order)              bn_free (order);

    if (hash)               bn_free (hash);

    if (newpoint)           ec_point_free (newpoint);

    if (ctx)                bn_ctx_free (ctx);

    if (rootkey)            ec_key_free (rootkey);

    if (pkey && !success)   ec_key_free (pkey);

    return success ? ec_key::acquire ((ec_key::pointer_t) pkey) : ec_key::invalid;
}

// --> root private key
ec_key generateprivatedeterministickey (blob const& pubgen, const bignum* rootprivkey, int seq)
{
    // privatekey(n) = (rootprivatekey + hash(pubhash|seq)) % order
    bn_ctx* ctx = bn_ctx_new ();

    if (ctx == nullptr) return ec_key::invalid;

    ec_key* pkey = ec_key_new_by_curve_name (nid_secp256k1);

    if (pkey == nullptr)
    {
        bn_ctx_free (ctx);
        return ec_key::invalid;
    }

    ec_key_set_conv_form (pkey, point_conversion_compressed);

    bignum* order = bn_new ();

    if (order == nullptr)
    {
        bn_ctx_free (ctx);
        ec_key_free (pkey);
        return ec_key::invalid;
    }

    if (!ec_group_get_order (ec_key_get0_group (pkey), order, ctx))
    {
        bn_free (order);
        bn_ctx_free (ctx);
        ec_key_free (pkey);
        return ec_key::invalid;
    }

    // calculate the private additional key
    bignum* privkey = makehash (pubgen, seq, order);

    if (privkey == nullptr)
    {
        bn_free (order);
        bn_ctx_free (ctx);
        ec_key_free (pkey);
        return ec_key::invalid;
    }

    // calculate the final private key
    bn_mod_add (privkey, privkey, rootprivkey, order, ctx);
    bn_free (order);
    ec_key_set_private_key (pkey, privkey);

    // compute the corresponding public key
    ec_point* pubkey = ec_point_new (ec_key_get0_group (pkey));

    if (!pubkey)
    {
        bn_clear_free (privkey);
        bn_ctx_free (ctx);
        ec_key_free (pkey);
        return ec_key::invalid;
    }

    if (ec_point_mul (ec_key_get0_group (pkey), pubkey, privkey, nullptr, nullptr, ctx) == 0)
    {
        bn_clear_free (privkey);
        ec_point_free (pubkey);
        ec_key_free (pkey);
        bn_ctx_free (ctx);
        return ec_key::invalid;
    }

    bn_clear_free (privkey);
    ec_key_set_public_key (pkey, pubkey);

    ec_point_free (pubkey);
    bn_ctx_free (ctx);

    return ec_key::acquire ((ec_key::pointer_t) pkey);
}

} // ripple
