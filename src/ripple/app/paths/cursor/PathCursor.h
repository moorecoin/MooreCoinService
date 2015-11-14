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

#ifndef rippled_ripple_module_app_paths_pathcursor_h
#define rippled_ripple_module_app_paths_pathcursor_h

#include <ripple/app/paths/ripplecalc.h>

namespace ripple {
namespace path {

// the next section contains methods that compute the liquidity along a path,
// either backward or forward.
//
// we need to do these computations twice - once backward to figure out the
// maximum possible liqiudity along a path, and then forward to compute the
// actual liquidity of the paths we actually chose.
//
// some of these routines use recursion to loop over all nodes in a path.
// todo(tom): replace this recursion with a loop.

class pathcursor
{
public:
    pathcursor(
        ripplecalc& ripplecalc,
        pathstate& pathstate,
        bool multiquality,
        nodeindex nodeindex = 0)
            : ripplecalc_(ripplecalc),
              pathstate_(pathstate),
              multiquality_(multiquality),
              nodeindex_(restrict(nodeindex))
    {
    }

    void nextincrement(ledgerentryset const& checkpoint) const;

private:
    pathcursor(pathcursor const&) = default;

    pathcursor increment(int delta = 1) const {
        return {ripplecalc_, pathstate_, multiquality_, nodeindex_ + delta};
    }

    ter liquidity(ledgerentryset const& lescheckpoint) const;
    ter reverseliquidity () const;
    ter forwardliquidity () const;

    ter forwardliquidityforaccount () const;
    ter reverseliquidityforoffer () const;
    ter forwardliquidityforoffer () const;
    ter reverseliquidityforaccount () const;

    // to send money out of an account.
    /** advancenode advances through offers in an order book.
        if needed, advance to next funded offer.
     - automatically advances to first offer.
     --> bentryadvance: true, to advance to next entry. false, recalculate.
      <-- uofferindex : 0=end of list.
    */
    ter advancenode (bool reverse) const;
    ter advancenode (stamount const& amount, bool reverse) const;

    // to deliver from an order book, when computing
    ter delivernodereverse (
        account const& uoutaccountid,
        stamount const& saoutreq,
        stamount& saoutact) const;

    ter delivernodeforward (
        account const& uinaccountid,
        stamount const& sainreq,
        stamount& sainact,
        stamount& sainfees) const;

    ripplecalc& ripplecalc_;
    pathstate& pathstate_;
    bool multiquality_;
    nodeindex nodeindex_;

    ledgerentryset& ledger() const
    {
        return ripplecalc_.mactiveledger;
    }

    nodeindex nodesize() const
    {
        return pathstate_.nodes().size();
    }

    nodeindex restrict(nodeindex i) const
    {
        return std::min (i, nodesize() - 1);
    }

    node& node(nodeindex i) const
    {
        return pathstate_.nodes()[i];
    }

    node& node() const
    {
        return node (nodeindex_);
    }

    node& previousnode() const
    {
        return node (restrict (nodeindex_ - 1));
    }

    node& nextnode() const
    {
        return node (restrict (nodeindex_ + 1));
    }
};

} // path
} // ripple

#endif
