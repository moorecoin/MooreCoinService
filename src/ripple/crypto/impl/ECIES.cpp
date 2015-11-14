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
#include <ripple/crypto/ecies.h>
#include <ripple/crypto/randomnumbers.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>

namespace ripple {

// ecies uses elliptic curve keys to send an encrypted message.

// a shared secret is generated from one public key and one private key.
// the same key results regardless of which key is public and which private.

// anonymous messages can be sent by generating an ephemeral public/private
// key pair, using that private key with the recipient's public key to
// encrypt and publishing the ephemeral public key. non-anonymous messages
// can be sent by using your own private key with the recipient's public key.

// a random iv is used to encrypt the message and an hmac is used to ensure
// message integrity. if you need timestamps or need to tell the recipient
// which key to use (his, yours, or ephemeral) you must add that data.
// (obviously, key information can't go in the encrypted portion anyway.)

// our ciphertext is all encrypted except the iv. the encrypted data decodes as follows:
// 1) iv (unencrypted)
// 2) encrypted: hmac of original plaintext
// 3) encrypted: original plaintext
// 4) encrypted: rest of block/padding

// ecies operations throw on any error such as a corrupt message or incorrect
// key. they *must* be called in try/catch blocks.

// algorithmic choices:
#define ecies_key_hash      sha512              // hash used to expand shared secret
#define ecies_key_length    (512/8)             // size of expanded shared secret
#define ecies_min_sec       (128/8)             // the minimum equivalent security
#define ecies_enc_algo      evp_aes_256_cbc()   // encryption algorithm
#define ecies_enc_key_type  uint256             // type used to hold shared secret
#define ecies_enc_key_size  (256/8)             // encryption key size
#define ecies_enc_blk_size  (128/8)             // encryption block size
#define ecies_enc_iv_type   uint128             // type used to hold iv
#define ecies_hmac_algo     evp_sha256()        // hmac algorithm
#define ecies_hmac_key_type uint256             // type used to hold hmac key
#define ecies_hmac_key_size (256/8)             // size of hmac key
#define ecies_hmac_type     uint256             // type used to hold hmac value
#define ecies_hmac_size     (256/8)             // size of hmac value

// returns a 32-byte secret unique to these two keys. at least one private key must be known.
static void geteciessecret (const openssl::ec_key& secretkey, const openssl::ec_key& publickey, ecies_enc_key_type& enc_key, ecies_hmac_key_type& hmac_key)
{
    ec_key* privkey = (ec_key*) secretkey.get();
    ec_key* pubkey  = (ec_key*) publickey.get();

    // retrieve a secret generated from an ec key pair. at least one private key must be known.
    if (privkey == nullptr || pubkey == nullptr)
        throw std::runtime_error ("missing key");

    if (! ec_key_get0_private_key (privkey))
    {
        throw std::runtime_error ("not a private key");
    }

    unsigned char rawbuf[512];
    int buflen = ecdh_compute_key (rawbuf, 512, ec_key_get0_public_key (pubkey), privkey, nullptr);

    if (buflen < ecies_min_sec)
        throw std::runtime_error ("ecdh key failed");

    unsigned char hbuf[ecies_key_length];
    ecies_key_hash (rawbuf, buflen, hbuf);
    memset (rawbuf, 0, ecies_hmac_key_size);

    assert ((ecies_enc_key_size + ecies_hmac_key_size) >= ecies_key_length);
    memcpy (enc_key.begin (), hbuf, ecies_enc_key_size);
    memcpy (hmac_key.begin (), hbuf + ecies_enc_key_size, ecies_hmac_key_size);
    memset (hbuf, 0, ecies_key_length);
}

static ecies_hmac_type makehmac (const ecies_hmac_key_type& secret, blob const& data)
{
    hmac_ctx ctx;
    hmac_ctx_init (&ctx);

    if (hmac_init_ex (&ctx, secret.begin (), ecies_hmac_key_size, ecies_hmac_algo, nullptr) != 1)
    {
        hmac_ctx_cleanup (&ctx);
        throw std::runtime_error ("init hmac");
    }

    if (hmac_update (&ctx, & (data.front ()), data.size ()) != 1)
    {
        hmac_ctx_cleanup (&ctx);
        throw std::runtime_error ("update hmac");
    }

    ecies_hmac_type ret;
    unsigned int ml = ecies_hmac_size;

    if (hmac_final (&ctx, ret.begin (), &ml) != 1)
    {
        hmac_ctx_cleanup (&ctx);
        throw std::runtime_error ("finalize hmac");
    }

    assert (ml == ecies_hmac_size);
    hmac_ctx_cleanup (&ctx);

    return ret;
}

blob encryptecies (const openssl::ec_key& secretkey, const openssl::ec_key& publickey, blob const& plaintext)
{

    ecies_enc_iv_type iv;
    random_fill (iv.begin (), ecies_enc_blk_size);

    ecies_enc_key_type secret;
    ecies_hmac_key_type hmackey;

    geteciessecret (secretkey, publickey, secret, hmackey);
    ecies_hmac_type hmac = makehmac (hmackey, plaintext);
    hmackey.zero ();

    evp_cipher_ctx ctx;
    evp_cipher_ctx_init (&ctx);

    if (evp_encryptinit_ex (&ctx, ecies_enc_algo, nullptr, secret.begin (), iv.begin ()) != 1)
    {
        evp_cipher_ctx_cleanup (&ctx);
        secret.zero ();
        throw std::runtime_error ("init cipher ctx");
    }

    secret.zero ();

    blob out (plaintext.size () + ecies_hmac_size + ecies_enc_key_size + ecies_enc_blk_size, 0);
    int len = 0, byteswritten;

    // output iv
    memcpy (& (out.front ()), iv.begin (), ecies_enc_blk_size);
    len = ecies_enc_blk_size;

    // encrypt/output hmac
    byteswritten = out.capacity () - len;
    assert (byteswritten > 0);

    if (evp_encryptupdate (&ctx, & (out.front ()) + len, &byteswritten, hmac.begin (), ecies_hmac_size) < 0)
    {
        evp_cipher_ctx_cleanup (&ctx);
        throw std::runtime_error ("");
    }

    len += byteswritten;

    // encrypt/output plaintext
    byteswritten = out.capacity () - len;
    assert (byteswritten > 0);

    if (evp_encryptupdate (&ctx, & (out.front ()) + len, &byteswritten, & (plaintext.front ()), plaintext.size ()) < 0)
    {
        evp_cipher_ctx_cleanup (&ctx);
        throw std::runtime_error ("");
    }

    len += byteswritten;

    // finalize
    byteswritten = out.capacity () - len;

    if (evp_encryptfinal_ex (&ctx, & (out.front ()) + len, &byteswritten) < 0)
    {
        evp_cipher_ctx_cleanup (&ctx);
        throw std::runtime_error ("encryption error");
    }

    len += byteswritten;

    // output contains: iv, encrypted hmac, encrypted data, encrypted padding
    assert (len <= (plaintext.size () + ecies_hmac_size + (2 * ecies_enc_blk_size)));
    assert (len >= (plaintext.size () + ecies_hmac_size + ecies_enc_blk_size)); // iv, hmac, data
    out.resize (len);
    evp_cipher_ctx_cleanup (&ctx);
    return out;
}

blob decryptecies (const openssl::ec_key& secretkey, const openssl::ec_key& publickey, blob const& ciphertext)
{
    // minimum ciphertext = iv + hmac + 1 block
    if (ciphertext.size () < ((2 * ecies_enc_blk_size) + ecies_hmac_size) )
        throw std::runtime_error ("ciphertext too short");

    // extract iv
    ecies_enc_iv_type iv;
    memcpy (iv.begin (), & (ciphertext.front ()), ecies_enc_blk_size);

    // begin decrypting
    evp_cipher_ctx ctx;
    evp_cipher_ctx_init (&ctx);

    ecies_enc_key_type secret;
    ecies_hmac_key_type hmackey;
    geteciessecret (secretkey, publickey, secret, hmackey);

    if (evp_decryptinit_ex (&ctx, ecies_enc_algo, nullptr, secret.begin (), iv.begin ()) != 1)
    {
        secret.zero ();
        hmackey.zero ();
        evp_cipher_ctx_cleanup (&ctx);
        throw std::runtime_error ("unable to init cipher");
    }

    // decrypt mac
    ecies_hmac_type hmac;
    int outlen = ecies_hmac_size;

    if ( (evp_decryptupdate (&ctx, hmac.begin (), &outlen,
                             & (ciphertext.front ()) + ecies_enc_blk_size, ecies_hmac_size + 1) != 1) || (outlen != ecies_hmac_size) )
    {
        secret.zero ();
        hmackey.zero ();
        evp_cipher_ctx_cleanup (&ctx);
        throw std::runtime_error ("unable to extract hmac");
    }

    // decrypt plaintext (after iv and encrypted mac)
    blob plaintext (ciphertext.size () - ecies_hmac_size - ecies_enc_blk_size);
    outlen = plaintext.size ();

    if (evp_decryptupdate (&ctx, & (plaintext.front ()), &outlen,
                           & (ciphertext.front ()) + ecies_enc_blk_size + ecies_hmac_size + 1,
                           ciphertext.size () - ecies_enc_blk_size - ecies_hmac_size - 1) != 1)
    {
        secret.zero ();
        hmackey.zero ();
        evp_cipher_ctx_cleanup (&ctx);
        throw std::runtime_error ("unable to extract plaintext");
    }

    // decrypt padding
    int flen = 0;

    if (evp_decryptfinal (&ctx, & (plaintext.front ()) + outlen, &flen) != 1)
    {
        secret.zero ();
        hmackey.zero ();
        evp_cipher_ctx_cleanup (&ctx);
        throw std::runtime_error ("plaintext had bad padding");
    }

    plaintext.resize (flen + outlen);

    // verify integrity
    if (hmac != makehmac (hmackey, plaintext))
    {
        secret.zero ();
        hmackey.zero ();
        evp_cipher_ctx_cleanup (&ctx);
        throw std::runtime_error ("plaintext had bad hmac");
    }

    secret.zero ();
    hmackey.zero ();

    evp_cipher_ctx_cleanup (&ctx);
    return plaintext;
}

} // ripple
