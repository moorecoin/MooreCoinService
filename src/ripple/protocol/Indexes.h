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

#ifndef ripple_protocol_indexes_h_included
#define ripple_protocol_indexes_h_included

#include <ripple/protocol/ledgerformats.h>
#include <ripple/protocol/rippleaddress.h>
#include <ripple/protocol/serializer.h>
#include <ripple/protocol/uinttypes.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/book.h>

namespace ripple {

// get the index of the node that holds the last 256 ledgers
uint256
getledgerhashindex ();

// get the index of the node that holds the set of 256 ledgers that includes
// this ledger's hash (or the first ledger after it if it's not a multiple
// of 256).
uint256
getledgerhashindex (std::uint32_t desiredledgerindex);

// get the index of the node that holds the enabled amendments
uint256
getledgeramendmentindex ();

// get the index of the node that holds the fee schedule
uint256
getledgerfeeindex ();

uint256
getaccountrootindex (account const& account);

uint256
getaccountrootindex (const rippleaddress & account);

uint256
getaccountreferindex (account const& account);

uint256
getledgerdividendindex ();

uint256
getgeneratorindex (account const& ugeneratorid);

uint256
getbookbase (book const& book);

uint256
getofferindex (account const& account, std::uint32_t usequence);

uint256
getownerdirindex (account const& account);

uint256
getdirnodeindex (uint256 const& udirroot, const std::uint64_t unodeindex);

uint256
getqualityindex (uint256 const& ubase, const std::uint64_t unodedir = 0);

uint256
getqualitynext (uint256 const& ubase);

// vfalco this name could be better
std::uint64_t
getquality (uint256 const& ubase);

uint256
getticketindex (account const& account, std::uint32_t usequence);

uint256
getripplestateindex (account const& a, account const& b, currency const& currency);

uint256
getripplestateindex (account const& a, issue const& issue);

uint256
getassetindex (account const& a, currency const& currency);

uint256
getassetindex (issue const& issue);

uint256
getassetstateindex (account const& a, account const& b, currency const& currency);

uint256
getassetstateindex (account const& a, issue const& issue);

}

#endif
