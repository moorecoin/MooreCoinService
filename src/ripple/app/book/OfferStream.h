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

#ifndef ripple_core_offerstream_h_included
#define ripple_core_offerstream_h_included

#include <ripple/app/book/booktip.h>
#include <ripple/app/book/offer.h>
#include <ripple/app/book/quality.h>
#include <ripple/app/book/types.h>

#include <beast/utility/noexcept.h>

#include <functional>

namespace ripple {
namespace core {

/** presents and consumes the offers in an order book.

    two `ledgerview` objects accumulate changes to the ledger. `view`
    is applied when the calling transaction succeeds. if the calling
    transaction fails, then `view_cancel` is applied.

    certain invalid offers are automatically removed:
        - offers with missing ledger entries
        - offers that expired
        - offers found unfunded:
            an offer is found unfunded when the corresponding balance is zero
            and the caller has not modified the balance. this is accomplished
            by also looking up the balance in the cancel view.

    when an offer is removed, it is removed from both views. this grooms the
    order book regardless of whether or not the transaction is successful.
*/
class offerstream
{
private:
    beast::journal m_journal;
    std::reference_wrapper <ledgerview> m_view;
    std::reference_wrapper <ledgerview> m_view_cancel;
    book m_book;
    clock::time_point m_when;
    booktip m_tip;
    offer m_offer;

    void
    erase (ledgerview& view);

public:
    offerstream (ledgerview& view, ledgerview& view_cancel, bookref book,
        clock::time_point when, beast::journal journal);

    ledgerview&
    view () noexcept
    {
        return m_view;
    }

    ledgerview&
    view_cancel () noexcept
    {
        return m_view_cancel;
    }

    book const&
    book () const noexcept
    {
        return m_book;
    }

    /** returns the offer at the tip of the order book.
        offers are always presented in decreasing quality.
        only valid if step() returned `true`.
    */
    offer const&
    tip () const
    {
        return m_offer;
    }

    /** advance to the next valid offer.
        this automatically removes:
            - offers with missing ledger entries
            - offers found unfunded
            - expired offers
        @return `true` if there is a valid offer.
    */
    bool
    step ();

    /** advance to the next valid offer that is not from the specified account.
        this automatically removes:
            - offers with missing ledger entries
            - offers found unfunded
            - offers from the same account
            - expired offers
        @return `true` if there is a valid offer.
    */
    bool
    step_account (account const& account);
};

}
}

#endif

