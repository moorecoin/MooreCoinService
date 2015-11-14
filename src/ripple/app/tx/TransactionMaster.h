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

#ifndef __transactionmaster__
#define __transactionmaster__

#include <ripple/app/tx/transaction.h>
#include <ripple/shamap/shamapitem.h>
#include <ripple/shamap/shamaptreenode.h>

namespace ripple {

// tracks all transactions in memory

class transactionmaster
{
public:
    transactionmaster ();

    transaction::pointer            fetch (uint256 const& , bool checkdisk);
    sttx::pointer  fetch (shamapitem::ref item, shamaptreenode:: tntype type,
                                           bool checkdisk, std::uint32_t ucommitledger);

    // return value: true = we had the transaction already
    bool inledger (uint256 const& hash, std::uint32_t ledger);
    bool canonicalize (transaction::pointer* ptransaction);
    void sweep (void);
    taggedcache <uint256, transaction>& getcache();

private:
    taggedcache <uint256, transaction> mcache;
};

} // ripple

#endif
