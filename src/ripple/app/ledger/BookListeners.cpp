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
#include <ripple/app/ledger/orderbookdb.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/json/to_string.h>

namespace ripple {

void booklisteners::addsubscriber (infosub::ref sub)
{
    scopedlocktype sl (mlock);
    mlisteners[sub->getseq ()] = sub;
}

void booklisteners::removesubscriber (std::uint64_t seq)
{
    scopedlocktype sl (mlock);
    mlisteners.erase (seq);
}

void booklisteners::publish (json::value const& jvobj)
{
    std::string sobj = to_string (jvobj);

    scopedlocktype sl (mlock);
    networkops::submaptype::const_iterator it = mlisteners.begin ();

    while (it != mlisteners.end ())
    {
        infosub::pointer p = it->second.lock ();

        if (p)
        {
            p->send (jvobj, sobj, true);
            ++it;
        }
        else
            it = mlisteners.erase (it);
    }
}

} // ripple
