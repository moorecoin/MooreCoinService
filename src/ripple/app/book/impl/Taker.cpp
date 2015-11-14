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

#include <beastconfig.h>
#include <ripple/app/book/taker.h>

namespace ripple {
namespace core {

taker::taker (ledgerview& view, account const& account,
    amounts const& amount, options const& options)
    : m_view (view)
    , m_account (account)
    , m_options (options)
    , m_quality (amount)
    , m_threshold (m_quality)
    , m_amount (amount)
    , m_remain (amount)
{
    assert (m_remain.in > zero);
    assert (m_remain.out > zero);

    // if this is a passive order (tfpassive), this prevents
    // offers at the same quality level from being consumed.
    if (m_options.passive)
        ++m_threshold;
}

amounts
taker::remaining_offer () const
{
    // if the taker is done, then there's no offer to place.
    if (done ())
        return amounts (m_amount.in.zeroed(), m_amount.out.zeroed());

    // avoid math altogether if we didn't cross.
   if (m_amount == m_remain)
       return m_amount;

    if (m_options.sell)
    {
        assert (m_remain.in > zero);

        // we scale the output based on the remaining input:
        return amounts (m_remain.in, divround (
            m_remain.in, m_quality.rate (), m_remain.out, true));
    }

    assert (m_remain.out > zero);

    // we scale the input based on the remaining output:
    return amounts (mulround (
        m_remain.out, m_quality.rate (), m_remain.in, true), m_remain.out);
}

/** calculate the amount particular user could get through an offer.
    @param amount the maximum flow that is available to the taker.
    @param offer the offer to flow through.
    @param taker the person taking the offer.
    @return the maximum amount that can flow through this offer.
*/
amounts
taker::flow (amounts amount, offer const& offer, account const& taker)
{
    // limit taker's input by available funds less fees
    amount const taker_funds (view ().accountfunds (
        taker, amount.in, fhzero_if_frozen));

    // get fee rate paid by taker
    std::uint32_t const taker_charge_rate (
        assetcurrency () == taker_funds.getcurrency () ? quality_one : rippletransferrate (view (),
        taker, offer.account (), amount.in.getissuer ()));

    // skip some math when there's no fee
    if (taker_charge_rate == quality_one)
    {
        amount = offer.quality ().ceil_in (amount, taker_funds);
    }
    else
    {
        amount const taker_charge (amountfromrate (taker_charge_rate));
        amount = offer.quality ().ceil_in (amount,
            divide (taker_funds, taker_charge));
    }

    // best flow the owner can get.
    // start out assuming entire offer will flow.
    amounts owner_amount (amount);

    // limit owner's output by available funds less fees
    amount const owner_funds (view ().accountfunds (
        offer.account (), owner_amount.out, fhzero_if_frozen));

    // get fee rate paid by owner
    std::uint32_t const owner_charge_rate (
        assetcurrency () == owner_funds.getcurrency () ? quality_one : rippletransferrate (view (),
        offer.account (), taker, amount.out.getissuer ()));

    if (owner_charge_rate == quality_one)
    {
        // skip some math when there's no fee
        owner_amount = offer.quality ().ceil_out (owner_amount, owner_funds);
    }
    else
    {
        amount const owner_charge (amountfromrate (owner_charge_rate));
        owner_amount = offer.quality ().ceil_out (owner_amount,
            divide (owner_funds, owner_charge));
    }

    // calculate the amount that will flow through the offer
    // this does not include the fees.
    return (owner_amount.in < amount.in)
        ? owner_amount
        : amount;
}

// adjust an offer to indicate that we are consuming some (or all) of it.
void
taker::consume (offer const& offer, amounts const& consumed) const
{
    amounts const& remaining (offer.amount ());

    assert (remaining.in > zero && remaining.out > zero);
    assert (remaining.in >= consumed.in && remaining.out >= consumed.out);

    offer.entry ()->setfieldamount (sftakerpays, remaining.in - consumed.in);
    offer.entry ()->setfieldamount (sftakergets, remaining.out - consumed.out);

    view ().entrymodify (offer.entry());

    assert (offer.entry ()->getfieldamount (sftakerpays) >= zero);
    assert (offer.entry ()->getfieldamount (sftakergets) >= zero);
}

// fill a direct offer.
//   @param offer the offer we are going to use.
//   @param amount the amount to flow through the offer.
//   @returns: tessuccess if successful, or an error code otherwise.
ter
taker::fill (offer const& offer, amounts const& amount)
{
    consume (offer, amount);

    // pay the taker, then the owner
    ter result = view ().accountsend (offer.account(), account(), amount.out);

    if (result == tessuccess)
        result = view ().accountsend (account(), offer.account(), amount.in);

    return result;
}

// fill a bridged offer.
//   @param leg1 the first leg we are going to use.
//   @param amount1 the amount to flow through the first leg of the offer.
//   @param leg2 the second leg we are going to use.
//   @param amount2 the amount to flow through the second leg of the offer.
//   @return tessuccess if successful, or an error code otherwise.
ter
taker::fill (
    offer const& leg1, amounts const& amount1,
    offer const& leg2, amounts const& amount2)
{
    assert (amount1.out == amount2.in);

    consume (leg1, amount1);
    consume (leg2, amount2);

    /* it is possible that m_account is the same as leg1.account, leg2.account
     * or both. this could happen when bridging over one's own offer. in that
     * case, accountsend won't actually do a send, which is what we want.
     */
    ter result = view ().accountsend (m_account, leg1.account (), amount1.in);

    if (result == tessuccess)
        result = view ().accountsend (leg1.account (), leg2.account (), amount1.out);

    if (result == tessuccess)
        result = view ().accountsend (leg2.account (), m_account, amount2.out);

    return result;
}

bool
taker::done () const
{
    if (m_options.sell && (m_remain.in <= zero))
    {
        // sell semantics: we consumed all the input currency
        return true;
    }

    if (!m_options.sell && (m_remain.out <= zero))
    {
        // buy semantics: we received the desired amount of output currency
        return true;
    }

    // we are finished if the taker is out of funds
    return view().accountfunds (
        account(), m_remain.in, fhzero_if_frozen) <= zero;
}

ter
taker::cross (offer const& offer)
{
    assert (!done ());

    /* before we call flow we must set the limit right; for buy semantics we
       need to clamp the output. and we always want to clamp the input.
     */
    amounts limit (offer.amount());

    if (! m_options.sell)
        limit = offer.quality ().ceil_out (limit, m_remain.out);
    limit = offer.quality().ceil_in (limit, m_remain.in);

    assert (limit.in <= offer.amount().in);
    assert (limit.out <= offer.amount().out);
    assert (limit.in <= m_remain.in);

    amounts amount (flow (limit, offer, account ()));
    if (assetcurrency () == amount.out.getcurrency ())
    {
        if (!amount.out.ismathematicalinteger ())
        {
            amount.out.floor ();
            limit = offer.quality ().ceil_out (limit, amount.out);
            amount = flow (limit, offer, account ());
        }
    }
    else if (assetcurrency () == amount.in.getcurrency ())
    {
        if (!amount.in.ismathematicalinteger ())
        {
            amount.in.floor ();
            limit = offer.quality ().ceil_in (limit, amount.in);
            amount = flow (limit, offer, account ());
        }
    }

    m_remain.out -= amount.out;
    m_remain.in -= amount.in;

    assert (m_remain.in >= zero);
    return fill (offer, amount);
}

ter
taker::cross (offer const& leg1, offer const& leg2)
{
    assert (!done ());

    assert (leg1.amount ().out.isnative ());
    assert (leg2.amount ().in.isnative ());

    amounts amount1 (leg1.amount());
    amounts amount2 (leg2.amount());

    if (m_options.sell)
        amount1 = leg1.quality().ceil_in (amount1, m_remain.in);
    else
        amount2 = leg2.quality().ceil_out (amount2, m_remain.out);

    if (amount1.out <= amount2.in)
        amount2 = leg2.quality().ceil_in (amount2, amount1.out);
    else
        amount1 = leg1.quality().ceil_out (amount1, amount2.in);

    assert (amount1.out == amount2.in);

    // as written, flow can't handle a 3-party transfer, but this works for
    // us because the output of leg1 and the input leg2 are xrp.
    amounts flow1 (flow (amount1, leg1, m_account));

    amount2 = leg2.quality().ceil_in (amount2, flow1.out);

    amounts flow2 (flow (amount2, leg2, m_account));

    m_remain.out -= amount2.out;
    m_remain.in -= amount1.in;

    return fill (leg1, flow1, leg2, flow2);
}

}
}
