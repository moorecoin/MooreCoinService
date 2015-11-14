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
#include <ripple/validators/impl/connectionimp.h>
#include <ripple/validators/impl/logic.h>

/*

questions the code should answer:

most important thing that we do:
    determine the new last fully validated ledger



- are we robustly connected to the ripple network?

- given a new recent validation for a ledger with a sequence number higher
  than the last fully validated ledger, do we have a new last fully validated
  ledger?

- what's the latest fully validated ledger?

    sequence number must always be known to set a fully validated ledger

    accumulate validations from nodes you trust at least a little bit,
    and that aren't stale.

    if you have a last fully validated ledger then validations for ledgers
    with lower sequence numbers can be ignored.

    flow of validations recent in time for sequence numbers greater or equal than
    the last fully validated ledger.

- what ledger is the current consenus round built on?

- determine when the current consensus round is over?
    criteria: number of validations for a ledger that comes after.

*/

namespace ripple {
namespace validators {
    
logic::logic (store& store, beast::journal journal)
    : /*store_ (store)
    , */journal_ (journal)
    , ledgers_(get_seconds_clock())
{
}

void
logic::stop()
{
}

void
logic::load()
{
}

void
logic::add (connectionimp& c)
{
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.insert(&c);
}

void
logic::remove (connectionimp& c)
{
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.erase(&c);
}

bool
logic::isstale (stvalidation const& v)
{
    return false;
}

void
logic::ontimer()
{
    std::lock_guard<std::mutex> lock(mutex_);
    beast::expire(ledgers_, std::chrono::minutes(5));
}

void
logic::onvalidation (stvalidation const& v)
{
    assert(v.isfieldpresent (sfledgersequence));
    auto const seq_no =
        v.getfieldu32 (sfledgersequence);
    auto const key = v.getsignerpublic();
    auto const ledger = v.getledgerhash();

    std::lock_guard<std::mutex> lock(mutex_);
    if (journal_.trace) journal_.trace <<
        "onvalidation: " << ledger;
    auto const result = ledgers_.emplace (std::piecewise_construct,
        std::make_tuple(ledger), std::make_tuple());
    auto& meta = result.first->second;
    assert(result.second || seq_no == meta.seq_no);
    if (result.second)
        meta.seq_no = seq_no;
    meta.keys.insert (v.getsignerpublic());
    if (meta.seq_no > latest_.second.seq_no)
    {
        if (policy_.acceptledgermeta (*result.first))
        {
            //ledgers_.clear();
            latest_ = *result.first;
            if (journal_.info) journal_.info <<
                "accepted " << latest_.second.seq_no <<
                    " (" << ledger << ")";
            for (auto& _ : connections_)
                _->onledger(latest_.first);
        }
    }
}

void
logic::onledgerclosed (ledgerindex index,
    ledgerhash const& hash, ledgerhash const& parent)
{
    if (journal_.info) journal_.info <<
        "onledgerclosed: " << index << " " << hash << " (parent " << parent << ")";
}

}
}
