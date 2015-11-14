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
#include <ripple/app/paths/cursor/pathcursor.h>
#include <ripple/basics/log.h>
#include <tuple>

namespace ripple {
namespace path {

ter pathcursor::forwardliquidity () const
{
    if (node().isaccount())
        return forwardliquidityforaccount ();

    // otherwise, node is an offer.
    if (previousnode().account_ == zero)
        return tessuccess;

    // previous is an account node, resolve its deliver.
    stamount sainact;
    stamount sainfees;

    auto resultcode = delivernodeforward (
        previousnode().account_,
        previousnode().safwddeliver, // previous is sending this much.
        sainact,
        sainfees);

    assert (resultcode != tessuccess ||
            previousnode().safwddeliver == sainact + sainfees);
    return resultcode;
}

} // path
} // ripple

// original comments:

// called to drive the from the first offer node in a chain.
//
// - offer input is in issuer/limbo.
// - current offers consumed.
//   - current offer owners debited.
//   - transfer fees credited to issuer.
//   - payout to issuer or limbo.
// - deliver is set without transfer fees.
