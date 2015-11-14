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
#include <ripple/protocol/indexes.h>

namespace ripple {

// get the index of the node that holds the last 256 ledgers
uint256
getledgerhashindex ()
{
    serializer s (2);
    s.add16 (spaceskiplist);
    return s.getsha512half ();
}

// get the index of the node that holds the set of 256 ledgers that includes
// this ledger's hash (or the first ledger after it if it's not a multiple
// of 256).
uint256
getledgerhashindex (std::uint32_t desiredledgerindex)
{
    serializer s (6);
    s.add16 (spaceskiplist);
    s.add32 (desiredledgerindex >> 16);
    return s.getsha512half ();
}

// get the index of the node that holds the enabled amendments
uint256
getledgeramendmentindex ()
{
    serializer s (2);
    s.add16 (spaceamendment);
    return s.getsha512half ();
}

// get the index of the node that holds the fee schedule
uint256
getledgerfeeindex ()
{
    serializer s (2);
    s.add16 (spacefee);
    return s.getsha512half ();
}

uint256
getaccountrootindex (account const& account)
{
    serializer  s (22);

    s.add16 (spaceaccount);
    s.add160 (account);

    return s.getsha512half ();
}
    
uint256
getaccountrootindex (const rippleaddress & account)
{
    return getaccountrootindex (account.getaccountid ());
}

uint256
getaccountreferindex (account const& account)
{
    serializer  s (22);
    
    s.add16 (spacerefer);
    s.add160 (account);
    
    return s.getsha512half ();
}

uint256
getledgerdividendindex ()
{
    serializer s(2);
    s.add16(spacedividend);
    return s.getsha512half();
}

uint256
getgeneratorindex (account const& ugeneratorid)
{
    serializer  s (22);

    s.add16 (spacegenerator);
    s.add160 (ugeneratorid);

    return s.getsha512half ();
}

uint256
getbookbase (book const& book)
{
    serializer  s (82);

    assert (isconsistent (book));

    s.add16 (spacebookdir);
    s.add160 (book.in.currency);
    s.add160 (book.out.currency);
    s.add160 (book.in.account);
    s.add160 (book.out.account);

    // return with quality 0.
    return getqualityindex (s.getsha512half ());
}

uint256
getofferindex (account const& account, std::uint32_t usequence)
{
    serializer  s (26);

    s.add16 (spaceoffer);
    s.add160 (account);
    s.add32 (usequence);

    return s.getsha512half ();
}

uint256
getownerdirindex (account const& account)
{
    serializer  s (22);

    s.add16 (spaceownerdir);
    s.add160 (account);

    return s.getsha512half ();
}


uint256
getdirnodeindex (uint256 const& udirroot, const std::uint64_t unodeindex)
{
    if (unodeindex)
    {
        serializer  s (42);

        s.add16 (spacedirnode);
        s.add256 (udirroot);
        s.add64 (unodeindex);

        return s.getsha512half ();
    }
    else
    {
        return udirroot;
    }
}

uint256
getqualityindex (uint256 const& ubase, const std::uint64_t unodedir)
{
    // indexes are stored in big endian format: they print as hex as stored.
    // most significant bytes are first.  least significant bytes represent
    // adjacent entries.  we place unodedir in the 8 right most bytes to be
    // adjacent.  want unodedir in big endian format so ++ goes to the next
    // entry for indexes.
    uint256 unode (ubase);

    // todo(tom): there must be a better way.
    ((std::uint64_t*) unode.end ())[-1] = htobe64 (unodedir);

    return unode;
}

uint256
getqualitynext (uint256 const& ubase)
{
    static uint256 unext ("10000000000000000");
    return ubase + unext;
}

std::uint64_t
getquality (uint256 const& ubase)
{
    return be64toh (((std::uint64_t*) ubase.end ())[-1]);
}

uint256
getticketindex (account const& account, std::uint32_t usequence)
{
    serializer  s (26);

    s.add16 (spaceticket);
    s.add160 (account);
    s.add32 (usequence);

    return s.getsha512half ();
}

uint256
getripplestateindex (account const& a, account const& b, currency const& currency)
{
    serializer  s (62);

    s.add16 (spaceripple);

    if (a < b)
    {
        s.add160 (a);
        s.add160 (b);
    }
    else
    {
        s.add160 (b);
        s.add160 (a);
    }

    s.add160 (currency);

    return s.getsha512half ();
}

uint256
getripplestateindex (account const& a, issue const& issue)
{
    return getripplestateindex (a, issue.account, issue.currency);
}

uint256
getassetindex (account const& a, currency const& currency)
{
    serializer  s (42);
    
    s.add16 (spaceasset);
    
    s.add160 (a);
    s.add160 (currency);
    
    return s.getsha512half ();
}

uint256
getassetindex (issue const& issue)
{
    return getassetindex (issue.account, issue.currency);
}

uint256
getassetstateindex (account const& a, account const& b, currency const& currency)
{
    serializer  s (62);
    
    s.add16 (spaceassetstate);

    if (a < b)
    {
        s.add160 (a);
        s.add160 (b);
    }
    else
    {
        s.add160 (b);
        s.add160 (a);
    }

    s.add160 (currency);
    
    return s.getsha512half ();
}

uint256
getassetstateindex (account const& a, issue const& issue)
{
    return getassetstateindex (a, issue.account, issue.currency);
}

}
