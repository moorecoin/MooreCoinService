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
#include <ripple/app/book/booktip.h>

namespace ripple {
namespace core {

booktip::booktip (ledgerview& view, bookref book)
    : m_view (view)
    , m_valid (false)
    , m_book (getbookbase (book))
    , m_end (getqualitynext (m_book))
{
}

bool
booktip::step ()
{
    if (m_valid)
    {
        if (m_entry)
        {
            view().offerdelete (m_index);
            m_entry = nullptr;
        }
    }

    for(;;)
    {
        // see if there's an entry at or worse than current quality.
        auto const page (
            view().getnextledgerindex (m_book, m_end));

        if (page.iszero())
            return false;

        unsigned int di (0);
        sle::pointer dir;
        if (view().dirfirst (page, dir, di, m_index))
        {
            m_dir = dir->getindex();
            m_entry = view().entrycache (ltoffer, m_index);
            m_valid = true;

            // next query should start before this directory
            m_book = page;

            // the quality immediately before the next quality
            --m_book;

            break;
        }
        // there should never be an empty directory but just in case,
        // we handle that case by advancing to the next directory.
        m_book = page;
    }

    return true;
}

}
}
