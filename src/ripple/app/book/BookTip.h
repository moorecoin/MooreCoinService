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

#ifndef ripple_core_booktip_h_included
#define ripple_core_booktip_h_included

#include <ripple/app/book/quality.h>
#include <ripple/app/book/types.h>
#include <ripple/protocol/indexes.h>
#include <beast/utility/noexcept.h>

#include <functional>

namespace ripple {
namespace core {

/** iterates and consumes raw offers in an order book.
    offers are presented from highest quality to lowest quality. this will
    return all offers present including missing, invalid, unfunded, etc.
*/
class booktip
{
private:
    std::reference_wrapper <ledgerview> m_view;
    bool m_valid;
    uint256 m_book;
    uint256 m_end;
    uint256 m_dir;
    uint256 m_index;
    sle::pointer m_entry;

    ledgerview&
    view() const noexcept
    {
        return m_view;
    }

public:
    /** create the iterator. */
    booktip (ledgerview& view, bookref book);

    uint256 const&
    dir() const noexcept
    {
        return m_dir;
    }

    uint256 const&
    index() const noexcept
    {
        return m_index;
    }

    quality const
    quality() const noexcept
    {
        return quality (getquality (m_book+uint256(1)));
    }

    sle::pointer const&
    entry() const noexcept
    {
        return m_entry;
    }

    /** erases the current offer and advance to the next offer.
        complexity: constant
        @return `true` if there is a next offer
    */
    bool
    step ();
};

}
}

#endif
