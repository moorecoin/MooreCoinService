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

#ifndef ripple_acceptedledger_h
#define ripple_acceptedledger_h

#include <ripple/app/ledger/acceptedledgertx.h>

namespace ripple {

/** a ledger that has become irrevocable.

    an accepted ledger is a ledger that has a sufficient number of
    validations to convince the local server that it is irrevocable.

    the existence of an accepted ledger implies all preceding ledgers
    are accepted.
*/
/* vfalco todo digest this terminology clarification:
    closed and accepted refer to ledgers that have not passed the
    validation threshold yet. once they pass the threshold, they are
    "validated". closed just means its close time has passed and no
    new transactions can get in. "accepted" means we believe it to be
    the result of the a consensus process (though haven't validated
    it yet).
*/
class acceptedledger
{
public:
    typedef std::shared_ptr<acceptedledger>       pointer;
    typedef const pointer&                          ret;
    typedef std::map<int, acceptedledgertx::pointer>    map_t;              // must be an ordered map!
    typedef map_t::value_type                       value_type;
    typedef map_t::const_iterator                   const_iterator;

public:
    static pointer makeacceptedledger (ledger::ref ledger);
    static void sweep ()
    {
        s_cache.sweep ();
    }

    ledger::ref getledger () const
    {
        return mledger;
    }
    const map_t& getmap () const
    {
        return mmap;
    }

    int getledgerseq () const
    {
        return mledger->getledgerseq ();
    }
    int gettxncount () const
    {
        return mmap.size ();
    }

    static float getcachehitrate ()
    {
        return s_cache.gethitrate ();
    }

    acceptedledgertx::pointer gettxn (int) const;

private:
    explicit acceptedledger (ledger::ref ledger);

    void insert (acceptedledgertx::ref);

private:
    static taggedcache <uint256, acceptedledger> s_cache;

    ledger::pointer     mledger;
    map_t               mmap;
};

} // ripple

#endif
