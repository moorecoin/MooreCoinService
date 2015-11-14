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

#ifndef ripple_core_taker_h_included
#define ripple_core_taker_h_included

#include <ripple/app/book/amounts.h>
#include <ripple/app/book/quality.h>
#include <ripple/app/book/offer.h>
#include <ripple/app/book/types.h>
#include <ripple/protocol/txflags.h>
#include <beast/utility/noexcept.h>
#include <functional>

namespace ripple {
namespace core {

/** state for the active party during order book or payment operations. */
class taker
{
public:
    struct options
    {
        options() = delete;

        explicit
        options (std::uint32_t tx_flags)
            : sell (tx_flags & tfsell)
            , passive (tx_flags & tfpassive)
            , fill_or_kill (tx_flags & tffillorkill)
            , immediate_or_cancel (tx_flags & tfimmediateorcancel)
        {
        }

        bool const sell;
        bool const passive;
        bool const fill_or_kill;
        bool const immediate_or_cancel;
    };

private:
    std::reference_wrapper <ledgerview> m_view;
    account m_account;
    options m_options;
    quality m_quality;
    quality m_threshold;

    // the original in and out quantities.
    amounts const m_amount;

    // the amounts still left over for us to try and take.
    amounts m_remain;

    amounts
    flow (amounts amount, offer const& offer, account const& taker);

    ter
    fill (offer const& offer, amounts const& amount);

    ter
    fill (offer const& leg1, amounts const& amount1,
        offer const& leg2, amounts const& amount2);

    void
    consume (offer const& offer, amounts const& consumed) const;

public:
    taker (ledgerview& view, account const& account,
        amounts const& amount, options const& options);

    ledgerview&
    view () const noexcept
    {
        return m_view;
    }

    /** returns the amount remaining on the offer.
        this is the amount at which the offer should be placed. it may either
        be for the full amount when there were no crossing offers, or for zero
        when the offer fully crossed, or any amount in between.
        it is always at the original offer quality (m_quality)
    */
    amounts
    remaining_offer () const;

    /** returns the account identifier of the taker. */
    account const&
    account () const noexcept
    {
        return m_account;
    }

    /** returns `true` if the quality does not meet the taker's requirements. */
    bool
    reject (quality const& quality) const noexcept
    {
        return quality < m_threshold;
    }

    /** returns `true` if order crossing should not continue.
        order processing is stopped if the taker's order quantities have
        been reached, or if the taker has run out of input funds.
    */
    bool
    done () const;

    /** perform direct crossing through given offer.
        @return tessuccess on success, error code otherwise.
    */
    ter
    cross (offer const& offer);

    /** perform bridged crossing through given offers.
        @return tessuccess on success, error code otherwise.
    */
    ter
    cross (offer const& leg1, offer const& leg2);
};

inline
std::ostream&
operator<< (std::ostream& os, taker const& taker)
{
    return os << taker.account();
}

}
}

#endif
