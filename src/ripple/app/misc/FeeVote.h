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

#ifndef ripple_app_feevote_h_included
#define ripple_app_feevote_h_included

#include <ripple/app/ledger/ledger.h>
#include <ripple/basics/basicconfig.h>
#include <ripple/protocol/systemparameters.h>

namespace ripple {

/** manager to process fee votes. */
class feevote
{
public:
    /** fee schedule to vote for.
        during voting ledgers, the feevote logic will try to move towards
        these values when injecting fee-setting transactions.
        a default-constructed setup contains recommended values.
    */
    struct setup
    {
        /** the cost of a reference transaction in drops. */
        std::uint64_t reference_fee = 1000;

        /** the account reserve requirement in drops. */
        std::uint64_t account_reserve = 0;

        /** the per-owned item reserve requirement in drops. */
        std::uint64_t owner_reserve = 0;
    };

    virtual ~feevote () = default;

    /** add local fee preference to validation.

        @param lastclosedledger
        @param basevalidation
    */
    virtual
    void
    dovalidation (ledger::ref lastclosedledger,
        stobject& basevalidation) = 0;

    /** cast our local vote on the fee.

        @param lastclosedledger
        @param initialposition
    */
    virtual
    void
    dovoting (ledger::ref lastclosedledger,
        shamap::ref initialposition) = 0;
};

/** build feevote::setup from a config section. */
feevote::setup
setup_feevote (section const& section);

/** create an instance of the feevote logic.
    @param setup the fee schedule to vote for.
    @param journal where to log.
*/
std::unique_ptr <feevote>
make_feevote (feevote::setup const& setup, beast::journal journal);

} // ripple

#endif
