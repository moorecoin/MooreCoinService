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

#ifndef ripple_core_offer_h_included
#define ripple_core_offer_h_included

#include <ripple/app/book/amounts.h>
#include <ripple/app/book/quality.h>
#include <ripple/app/book/types.h>

#include <ripple/protocol/stledgerentry.h>
#include <ripple/protocol/sfield.h>

#include <beast/utility/noexcept.h>

#include <ostream>

namespace ripple {
namespace core {

class offer
{
public:
    typedef amount amount_type;

private:
    sle::pointer m_entry;
    quality m_quality;

public:
    offer() = default;

    offer (sle::pointer const& entry, quality quality)
        : m_entry (entry)
        , m_quality (quality)
    {
    }

    /** returns the quality of the offer.
        conceptually, the quality is the ratio of output to input currency.
        the implementation calculates it as the ratio of input to output
        currency (so it sorts ascending). the quality is computed at the time
        the offer is placed, and never changes for the lifetime of the offer.
        this is an important business rule that maintains accuracy when an
        offer is partially filled; subsequent partial fills will use the
        original quality.
    */
    quality const
    quality() const noexcept
    {
        return m_quality;
    }

    /** returns the account id of the offer's owner. */
    account const
    account() const
    {
        return m_entry->getfieldaccount160 (sfaccount);
    }

    /** returns the in and out amounts.
        some or all of the out amount may be unfunded.
    */
    amounts const
    amount() const
    {
        return amounts (
            m_entry->getfieldamount (sftakerpays),
            m_entry->getfieldamount (sftakergets));
    }

    /** returns `true` if no more funds can flow through this offer. */
    bool
    fully_consumed() const
    {
        if (m_entry->getfieldamount (sftakerpays) <= zero)
            return true;
        if (m_entry->getfieldamount (sftakergets) <= zero)
            return true;
        return false;
    }

    /** returns the ledger entry underlying the offer. */
    // avoid using this
    sle::pointer
    entry() const noexcept
    {
        return m_entry;
    }
};

inline
std::ostream&
operator<< (std::ostream& os, offer const& offer)
{
    return os << offer.entry()->getindex();
}

}
}

#endif
