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
#include <ripple/basics/base_uint.h>
#include <beast/unit_test/suite.h>

namespace ripple {

// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2011 the bitcoin developers
// distributed under the mit/x11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.


class ckey_test : public beast::unit_test::suite
{
public:
    void
    run ()
    {
        uint128 seed1, seed2;
        seed1.sethex ("71ed064155ffadfa38782c5e0158cb26");
        seed2.sethex ("cf0c3be4485961858c4198515ae5b965");

        openssl::ec_key root1 = generaterootdeterministickey (seed1);
        openssl::ec_key root2 = generaterootdeterministickey (seed2);

        uint256 const priv1 = root1.get_private_key();
        uint256 const priv2 = root2.get_private_key();

        unexpected (to_string (priv1) != "7cfba64f771e93e817e15039215430b53f7401c34931d111eab3510b22dbb0d8",
            "incorrect private key for generator");

        unexpected (to_string (priv2) != "98bc2eacb26eb021d1a6293c044d88ba2f0b6729a2772deebf2e21a263c1740b",
            "incorrect private key for generator");
    }
};

beast_define_testsuite(ckey,ripple_data,ripple);

} // ripple

