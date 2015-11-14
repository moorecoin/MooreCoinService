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

// calculate a node and its previous nodes.  the eventual goal is to determine1
// how much input currency we need in the forward direction to satisfy the
// output.
//
// from the destination work in reverse towards the source calculating how much
// must be asked for.  as we move backwards, individual nodes may further limit
// the amount of liquidity available.
//
// this is just a controlling loop that sets things up and then hands the work
// off to either reverseliquidityforaccount or
// reverseliquidityforoffer.
//
// later on the result of this will be used to work forward, figuring out how
// much can actually be delivered.
//
// <-- resultcode: tessuccess or tecpath_dry
// <-> pnnodes:
//     --> [end]sawanted.mamount
//     --> [all]sawanted.mcurrency
//     --> [all]saaccount
//     <-> [0]sawanted.mamount : --> limit, <-- actual

ter pathcursor::reverseliquidity () const
{
    // every account has a transfer rate for its issuances.

    // tomove: the account charges
    // a fee when third parties transfer that account's own issuances.

    // node.transferrate_ caches the output transfer rate for this node.
    node().transferrate_ = amountfromrate (
        rippletransferrate (ledger(), node().issue_.account));

    if (node().isaccount ())
        return reverseliquidityforaccount ();

    // otherwise the node is an offer.
    if (isnative (nextnode().account_))
    {
        writelog (lstrace, ripplecalc)
            << "reverseliquidityforoffer: "
            << "offer --> offer: nodeindex_=" << nodeindex_;
        return tessuccess;

        // this control structure ensures delivernodereverse is only called for the
        // rightmost offer in a chain of offers - which means that
        // delivernodereverse has to take all of those offers into consideration.
    }

    // next is an account node, resolve current offer node's deliver.
    stamount sadeliveract;

    writelog (lstrace, ripplecalc)
        << "reverseliquidityforoffer: offer --> account:"
        << " nodeindex_=" << nodeindex_
        << " sarevdeliver=" << node().sarevdeliver;

    // the next node wants the current node to deliver this much:
    return delivernodereverse (
        nextnode().account_,
        node().sarevdeliver,
        sadeliveract);
}

} // path
} // ripple
