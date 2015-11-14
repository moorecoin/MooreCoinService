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
#include <ripple/app/book/offerstream.h>
#include <ripple/app/book/taker.h>
#include <ripple/app/book/quality.h>
#include <ripple/app/transactors/createoffer.h>
#include <beast/streams/debug_ostream.h>

namespace ripple {

std::pair<ter, core::amounts>
createoffer::crossoffersbridged (
    core::ledgerview& view,
    core::amounts const& taker_amount)
{
    assert (!taker_amount.in.isnative () && !taker_amount.out.isnative ());

    if (taker_amount.in.isnative () || taker_amount.out.isnative ())
        return std::make_pair (tefinternal, core::amounts ());

    core::clock::time_point const when (
        mengine->getledger ()->getparentclosetimenc ());

    core::taker::options const options (mtxn.getflags());

    core::ledgerview view_cancel (view.duplicate());

    auto& asset_in = taker_amount.in.issue();
    auto& asset_out = taker_amount.out.issue();

    core::offerstream offers_direct (view, view_cancel,
        book (asset_in, asset_out), when, m_journal);

    core::offerstream offers_leg1 (view, view_cancel,
        book (asset_in, xrpissue ()), when, m_journal);

    core::offerstream offers_leg2 (view, view_cancel,
        book (xrpissue (), asset_out), when, m_journal);

    core::taker taker (view, mtxnaccountid, taker_amount, options);

    if (m_journal.debug) m_journal.debug <<
        "process_order: " <<
        (options.sell? "sell" : "buy") << " " <<
        (options.passive? "passive" : "") << std::endl <<
        "     taker: " << taker.account() << std::endl <<
        "  balances: " <<
        view.accountfunds (taker.account(), taker_amount.in, fhignore_freeze)
        << ", " <<
        view.accountfunds (taker.account(), taker_amount.out, fhignore_freeze);

    ter cross_result (tessuccess);

    /* note the subtle distinction here: self-offers encountered in the bridge
     * are taken, but self-offers encountered in the direct book are not.
     */
    bool have_bridged (offers_leg1.step () && offers_leg2.step ());
    bool have_direct (offers_direct.step_account (taker.account ()));
    bool place_order (true);

    while (have_direct || have_bridged)
    {
        core::quality quality;
        bool use_direct;
        bool leg1_consumed(false);
        bool leg2_consumed(false);
        bool direct_consumed(false);

        // logic:
        // we calculate the qualities of any direct and bridged offers at the
        // tip of the order book, and choose the best one of the two.

        if (have_direct)
        {
            core::quality const direct_quality (offers_direct.tip ().quality ());

            if (have_bridged)
            {
                core::quality const bridged_quality (core::composed_quality (
                    offers_leg1.tip ().quality (),
                    offers_leg2.tip ().quality ()));

                if (bridged_quality < direct_quality)
                {
                    use_direct = true;
                    quality = direct_quality;
                }
                else
                {
                    use_direct = false;
                    quality = bridged_quality;
                }
            }
            else
            {
                use_direct = true;
                quality = offers_direct.tip ().quality ();
            }
        }
        else
        {
            use_direct = false;
            quality = core::composed_quality (
                    offers_leg1.tip ().quality (),
                    offers_leg2.tip ().quality ());
        }

        // we are always looking at the best quality available, so if we reject
        // that, we know that we are done.
        if (taker.reject(quality))
            break;

        if (use_direct)
        {
            if (m_journal.debug) m_journal.debug << "direct:" << std::endl <<
                "  offer: " << offers_direct.tip () << std::endl <<
                "         " << offers_direct.tip ().amount().in <<
                " : " << offers_direct.tip ().amount ().out;

            cross_result = taker.cross(offers_direct.tip ());

            if (offers_direct.tip ().fully_consumed ())
            {
                direct_consumed = true;
                have_direct = offers_direct.step_account (taker.account());
            }
        }
        else
        {
            if (m_journal.debug) m_journal.debug << "bridge:" << std::endl <<
                " offer1: " << offers_leg1.tip () << std::endl <<
                "         " << offers_leg1.tip ().amount().in <<
                " : " << offers_leg1.tip ().amount ().out << std::endl <<
                " offer2: " << offers_leg2.tip () << std::endl <<
                "         " << offers_leg2.tip ().amount ().in <<
                " : " << offers_leg2.tip ().amount ().out;

            cross_result = taker.cross(offers_leg1.tip (), offers_leg2.tip ());

            if (offers_leg1.tip ().fully_consumed ())
            {
                leg1_consumed = true;
                have_bridged = offers_leg1.step ();
            }
            if (have_bridged && offers_leg2.tip ().fully_consumed ())
            {
                leg2_consumed = true;
                have_bridged = offers_leg2.step ();
            }
        }

        if (cross_result != tessuccess)
        {
            cross_result = tecfailed_processing;
            break;
        }

        if (taker.done())
        {
            m_journal.debug << "the taker reports he's done during crossing!";
            place_order = false;
            break;
        }

        // postcondition: if we aren't done, then we *must* have consumed at
        //                least one offer fully.
        assert (direct_consumed || leg1_consumed || leg2_consumed);

        if (!direct_consumed && !leg1_consumed && !leg2_consumed)
        {
            cross_result = tefinternal;
            break;
        }
    }

    return std::make_pair(cross_result, taker.remaining_offer ());
}

}
