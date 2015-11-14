//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2014 ripple labs inc.

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

#ifndef ripple_ecdsa_h
#define ripple_ecdsa_h

#include <ripple/crypto/ec_key.h>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/blob.h>

namespace ripple {

openssl::ec_key ecdsaprivatekey (uint256 const& serialized);
openssl::ec_key ecdsapublickey  (blob    const& serialized);

blob ecdsasign (uint256 const& hash, const openssl::ec_key& key);

bool ecdsaverify (uint256 const& hash, blob const& sig, const openssl::ec_key& key);

} // ripple

#endif
