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

#ifndef ripple_localtransactions_h
#define ripple_localtransactions_h

#include <ripple/app/tx/transactionengine.h>
#include <ripple/app/ledger/ledger.h>

namespace ripple {

// track transactions issued by local clients
// ensure we always apply them to our open ledger
// hold them until we see them in a fully-validated ledger

class localtxs
{
public:

    virtual ~localtxs () = default;

    static std::unique_ptr<localtxs> new ();

    // add a new local transaction
    virtual void push_back (ledgerindex index, sttx::ref txn) = 0;

    // apply local transactions to a new open ledger
    virtual void apply (transactionengine&) = 0;

    // remove obsolete transactions based on a new fully-valid ledger
    virtual void sweep (ledger::ref validledger) = 0;

    virtual std::size_t size () = 0;
};

} // ripple

#endif
