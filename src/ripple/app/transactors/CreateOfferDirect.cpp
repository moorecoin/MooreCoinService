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
#include <ripple/app/transactors/createoffer.h>
#include <beast/streams/debug_ostream.h>

namespace ripple {

std::pair<ter, core::amounts>
createoffer::crossoffersdirect (
    core::ledgerview& view,
    core::amounts const& taker_amount)
{
    core::taker::options const options (mtxn.getflags());

    core::clock::time_point const when (
        mengine->getledger ()->getparentclosetimenc ());

    core::ledgerview view_cancel (view.duplicate());
    core::offerstream offers (
        view, view_cancel,
        book (taker_amount.in.issue(), taker_amount.out.issue()),
        when, m_journal);
    core::taker taker (offers.view(), mtxnaccountid, taker_amount, options);

    ter cross_result (tessuccess);

    while (true)
    {
        // modifying the order or logic of these
        // operations causes a protocol breaking change.

        // checks which remove offers are performed early so we
        // can reduce the size of the order book as much as possible
        // before terminating the loop.

        if (taker.done())
        {
            m_journal.debug << "the taker reports he's done during crossing!";
            break;
        }

        if (! offers.step ())
        {
            // place the order since there are no
            // more offers and the order has a balance.
            m_journal.debug << "no more offers to consider during crossing!";
            break;
        }

        auto const& offer (offers.tip());

        if (taker.reject (offer.quality()))
        {
            // place the order since there are no more offers
            // at the desired quality, and the order has a balance.
            break;
        }

        if (offer.account() == taker.account())
        {
            // skip offer from self. the offer will be considered expired and
            // will get deleted.
            continue;
        }

        if (m_journal.debug) m_journal.debug <<
                "  offer: " << offer.entry()->getindex() << std::endl <<
                "         " << offer.amount().in << " : " << offer.amount().out;

        cross_result = taker.cross (offer);

        if (cross_result != tessuccess)
        {
            cross_result = tecfailed_processing;
            break;
        }
    }

    return std::make_pair(cross_result, taker.remaining_offer ());
}

}
