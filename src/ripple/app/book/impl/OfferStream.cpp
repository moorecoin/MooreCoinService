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

namespace ripple {
namespace core {

offerstream::offerstream (ledgerview& view, ledgerview& view_cancel,
    bookref book, clock::time_point when, beast::journal journal)
    : m_journal (journal)
    , m_view (view)
    , m_view_cancel (view_cancel)
    , m_book (book)
    , m_when (when)
    , m_tip (view, book)
{
}

// handle the case where a directory item with no corresponding ledger entry
// is found. this shouldn't happen but if it does we clean it up.
void
offerstream::erase (ledgerview& view)
{
    // nikb note this should be using ledgerview::dirdelete, which would
    //           correctly remove the directory if its the last entry.
    //           unfortunately this is a protocol breaking change.

    auto p (view.entrycache (ltdir_node, m_tip.dir()));

    if (p == nullptr)
    {
        if (m_journal.error) m_journal.error <<
            "missing directory " << m_tip.dir() <<
            " for offer " << m_tip.index();
        return;
    }

    auto v (p->getfieldv256 (sfindexes));
    auto& x (v.peekvalue());
    auto it (std::find (x.begin(), x.end(), m_tip.index()));

    if (it == x.end())
    {
        if (m_journal.error) m_journal.error <<
            "missing offer " << m_tip.index() <<
            " for directory " << m_tip.dir();
        return;
    }

    x.erase (it);
    p->setfieldv256 (sfindexes, v);
    view.entrymodify (p);

    if (m_journal.trace) m_journal.trace <<
        "missing offer " << m_tip.index() <<
        " removed from directory " << m_tip.dir();
}

bool
offerstream::step ()
{
    // modifying the order or logic of these
    // operations causes a protocol breaking change.

    for(;;)
    {
        // booktip::step deletes the current offer from the view before
        // advancing to the next (unless the ledger entry is missing).
        if (! m_tip.step())
            return false;

        sle::pointer const& entry (m_tip.entry());

        // remove if missing
        if (! entry)
        {
            erase (view());
            erase (view_cancel());
            continue;
        }

        // remove if expired
        if (entry->isfieldpresent (sfexpiration) &&
            entry->getfieldu32 (sfexpiration) <= m_when)
        {
            view_cancel().offerdelete (entry->getindex());
            if (m_journal.trace) m_journal.trace <<
                "removing expired offer " << entry->getindex();
            continue;
        }

        m_offer = offer (entry, m_tip.quality());

        amounts const amount (m_offer.amount());

        // remove if either amount is zero
        if (amount.empty())
        {
            view_cancel().offerdelete (entry->getindex());
            if (m_journal.warning) m_journal.warning <<
                "removing bad offer " << entry->getindex();
            m_offer = offer{};
            continue;
        }

        // calculate owner funds
        // nikb note the calling code also checks the funds, how expensive is
        //           looking up the funds twice?
        amount const owner_funds (view().accountfunds (
            m_offer.account(), m_offer.amount().out, fhzero_if_frozen));

        // check for unfunded offer
        if (owner_funds <= zero)
        {
            // if the owner's balance in the pristine view is the same,
            // we haven't modified the balance and therefore the
            // offer is "found unfunded" versus "became unfunded"
            if (view_cancel().accountfunds (m_offer.account(),
                m_offer.amount().out, fhzero_if_frozen) == owner_funds)
            {
                view_cancel().offerdelete (entry->getindex());
                if (m_journal.trace) m_journal.trace <<
                    "removing unfunded offer " << entry->getindex();
            }
            else
            {
                if (m_journal.trace) m_journal.trace <<
                    "removing became unfunded offer " << entry->getindex();
            }
            m_offer = offer{};
            continue;
        }

        break;
    }

    return true;
}

bool
offerstream::step_account (account const& account)
{
    while (step ())
    {
        if (tip ().account () != account)
            return true;
    }

    return false;
}

}
}
