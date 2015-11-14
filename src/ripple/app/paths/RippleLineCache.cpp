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
#include <ripple/app/paths/ripplelinecache.h>

namespace ripple {

ripplelinecache::ripplelinecache (ledger::ref l)
    : mledger (l)
{
}

ripplelinecache::ripplestatevector const&
ripplelinecache::getripplelines (account const& accountid)
{
    accountkey key (accountid, hasher_ (accountid));

    scopedlocktype sl (mlock);

    auto it = mrlmap.emplace (key, ripplestatevector ());

    if (it.second)
        it.first->second = ripple::getripplestateitems (accountid, mledger);

    return it.first->second;
}

} // ripple