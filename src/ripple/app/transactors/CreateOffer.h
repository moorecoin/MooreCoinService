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

#ifndef ripple_app_createoffer_h_included
#define ripple_app_createoffer_h_included

#include <ripple/app/book/offerstream.h>
#include <ripple/app/book/taker.h>
#include <ripple/app/book/types.h>
#include <ripple/app/book/amounts.h>
#include <ripple/app/transactors/transactor.h>
#include <ripple/basics/log.h>
#include <ripple/json/to_string.h>
#include <beast/cxx14/memory.h>

namespace ripple {

class createoffer
    : public transactor
{
private:
    // what kind of offer we are placing
#if ripple_enable_autobridging
    bool autobridging_;
#endif

    // determine if we are authorized to hold the asset we want to get
    ter
    checkacceptasset(issueref issue) const;

    /*  fill offer as much as possible by consuming offers already on the books.
        we adjusts account balances and charges fees on top to taker.

        @param taker_amount.in how much the taker offers
        @param taker_amount.out how much the taker wants

        @return result.first crossing operation success/failure indicator.
                result.second amount of offer left unfilled - only meaningful
                              if result.first is tessuccess.
    */
    std::pair<ter, core::amounts>
    crossoffersbridged (core::ledgerview& view,
        core::amounts const& taker_amount);

    std::pair<ter, core::amounts>
    crossoffersdirect (core::ledgerview& view,
        core::amounts const& taker_amount);

    std::pair<ter, core::amounts>
    crossoffers (core::ledgerview& view,
        core::amounts const& taker_amount);

public:
    createoffer (bool autobridging, sttx const& txn,
        transactionengineparams params, transactionengine* engine);

    ter
    doapply() override;
};

ter
transact_createoffer (sttx const& txn,
    transactionengineparams params, transactionengine* engine);

}

#endif
