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

#ifndef ripple_ckeydeterministic_h
#define ripple_ckeydeterministic_h

#include <ripple/crypto/ec_key.h>
#include <ripple/basics/base_uint.h>
#include <openssl/bn.h>

namespace ripple {

openssl::ec_key generaterootdeterministickey (const uint128& passphrase);
openssl::ec_key generatepublicdeterministickey (blob const& generator, int n);
openssl::ec_key generateprivatedeterministickey (blob const& family, const bignum* rootpriv, int n);

} // ripple

#endif
