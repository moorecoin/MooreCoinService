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
#include <ripple/app/ledger/acceptedledger.h>
#include <ripple/basics/log.h>
#include <ripple/basics/seconds_clock.h>

namespace ripple {

// vfalco todo remove this global and make it a member of the app
//             use a dependency injection to give acceptedledger access.
//
taggedcache <uint256, acceptedledger> acceptedledger::s_cache (
    "acceptedledger", 4, 600, get_seconds_clock (),
        deprecatedlogs().journal("taggedcache"));

acceptedledger::acceptedledger (ledger::ref ledger) : mledger (ledger)
{
    shamap& txset = *ledger->peektransactionmap ();

    for (shamapitem::pointer item = txset.peekfirstitem (); item;
         item = txset.peeknextitem (item->gettag ()))
    {
        serializeriterator sit (item->peekserializer ());
        insert (std::make_shared<acceptedledgertx> (ledger, std::ref (sit)));
    }
}

acceptedledger::pointer acceptedledger::makeacceptedledger (ledger::ref ledger)
{
    acceptedledger::pointer ret = s_cache.fetch (ledger->gethash ());

    if (ret)
        return ret;

    ret = acceptedledger::pointer (new acceptedledger (ledger));
    s_cache.canonicalize (ledger->gethash (), ret);
    return ret;
}

void acceptedledger::insert (acceptedledgertx::ref at)
{
    assert (mmap.find (at->getindex ()) == mmap.end ());
    mmap.insert (std::make_pair (at->getindex (), at));
}

acceptedledgertx::pointer acceptedledger::gettxn (int i) const
{
    map_t::const_iterator it = mmap.find (i);

    if (it == mmap.end ())
        return acceptedledgertx::pointer ();

    return it->second;
}

} // ripple
