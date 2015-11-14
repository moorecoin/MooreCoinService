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

#ifndef ripple_protocol_protocol_h_included
#define ripple_protocol_protocol_h_included

#include <ripple/basics/base_uint.h>
#include <cstdint>

namespace ripple {

/** protocol specific constants, types, and data.

    this information is part of the ripple protocol. specifically,
    it is required for peers to be able to communicate with each other.

    @note changing these will create a hard fork.

    @ingroup protocol
    @defgroup protocol
*/
struct protocol
{
    /** smallest legal byte size of a transaction.
    */
    static int const txminsizebytes = 32;

    /** largest legal byte size of a transaction.
    */
    static int const txmaxsizebytes = 1024 * 1024; // 1048576
};

/** a ledger index.
*/
// vfalco todo pick one. i like index since its not an abbreviation
typedef std::uint32_t ledgerindex;
// vfalco note "ledgerseq" appears in some sql statement text
typedef std::uint32_t ledgerseq;

/** a transaction identifier.
*/
// vfalco todo maybe rename to txhash
typedef uint256 txid;

/** a transaction index.
*/
typedef std::uint32_t txseq; // vfalco note should read txindex or txnum

} // ripple

#endif
