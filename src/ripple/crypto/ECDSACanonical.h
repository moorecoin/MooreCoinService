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

#ifndef ripple_crypto_ecdsacanonical_h_included
#define ripple_crypto_ecdsacanonical_h_included

#include <ripple/basics/blob.h>

namespace ripple {

enum class ecdsa
{
    not_strict = 0,
    strict
};

/** checks whether a secp256k1 ecdsa signature is canonical.
    return value is true if the signature is canonical.
    if mustbestrict is specified, the signature must be
    strictly canonical (one and only one valid form).
    the return value for something that is not an ecdsa
    signature is unspecified. (but the function will not crash.)
*/
bool iscanonicalecdsasig (void const* signature,
    std::size_t siglen, ecdsa mustbestrict);

inline bool iscanonicalecdsasig (blob const& signature,
    ecdsa mustbestrict)
{
    return signature.empty() ? false :
        iscanonicalecdsasig (&signature[0], signature.size(), mustbestrict);
}

/** converts a canonical secp256k1 ecdsa signature to a
    fully-canonical one. returns true if the original signature
    was already fully-canonical. the behavior if something
    that is not a canonical secp256k1 ecdsa signature is
    passed is unspecified. the signature buffer must be large
    enough to accommodate the largest valid fully-canonical
    secp256k1 ecdsa signature (72 bytes).
*/
bool makecanonicalecdsasig (void *signature, std::size_t& siglen);

}

#endif
