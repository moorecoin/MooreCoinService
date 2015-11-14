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

#ifndef ripple_disputedtx_h
#define ripple_disputedtx_h

#include <ripple/protocol/uinttypes.h>
#include <ripple/protocol/serializer.h>
#include <ripple/basics/base_uint.h>
#include <memory>

namespace ripple {

/** a transaction discovered to be in dispute during conensus.

    during consensus, a @ref disputedtx is created when a transaction
    is discovered to be disputed. the object persists only as long as
    the dispute.

    undisputed transactions have no corresponding @ref disputedtx object.
*/
class disputedtx
{
public:
    typedef std::shared_ptr <disputedtx> pointer;

    disputedtx (uint256 const& txid,
                blob const& tx,
                bool ourvote) :
        mtransactionid (txid), myays (0), mnays (0), mourvote (ourvote), transaction (tx)
    {
        ;
    }

    uint256 const& gettransactionid () const
    {
        return mtransactionid;
    }

    bool getourvote () const
    {
        return mourvote;
    }

    // vfalco todo make this const
    serializer& peektransaction ()
    {
        return transaction;
    }

    void setourvote (bool o)
    {
        mourvote = o;
    }

    // vfalco note its not really a peer, its the 160 bit hash of the validator's public key
    //
    void setvote (nodeid const& peer, bool votesyes);
    void unvote (nodeid const& peer);

    bool updatevote (int percenttime, bool proposing);
    json::value getjson ();

private:
    uint256 mtransactionid;
    int myays;
    int mnays;
    bool mourvote;
    serializer transaction;

    hash_map <nodeid, bool> mvotes;
};

// how many total extra passes we make
// we must ensure we make at least one non-retriable pass
#define ledger_total_passes 3

// how many extra retry passes we
// make if the previous retry pass made changes
#define ledger_retry_passes 1

} // ripple

#endif
