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
#include <ripple/app/paths/cursor/rippleliquidity.h>
#include <ripple/basics/log.h>

namespace ripple {
namespace path {

// calculate the next increment of a path.
//
// the increment is what can satisfy a portion or all of the requested output at
// the best quality.
//
// <-- pathstate.uquality
//
// this is the wrapper that restores a checkpointed version of the ledger so we
// can write all over it without consequence.

void pathcursor::nextincrement (ledgerentryset const& lescheckpoint) const
{
    // the next state is what is available in preference order.
    // this is calculated when referenced accounts changed.
    // vfalco-fixme this generates errors
    // writelog (lstrace, ripplecalc)
    //     << "nextincrement: path in: " << pathstate_.getjson ();

    auto status = liquidity(lescheckpoint);

    if (status == tessuccess)
    {
        auto isdry = pathstate_.isdry();
        condlog (isdry, lsdebug, ripplecalc)
            << "nextincrement: error forwardliquidity reported success"
            << " on dry path:"
            << " saoutpass=" << pathstate_.outpass()
            << " inpass()=" << pathstate_.inpass();

        if (isdry)
            throw std::runtime_error ("made no progress.");

        // calculate relative quality.
        pathstate_.setquality(getrate (
            pathstate_.outpass(), pathstate_.inpass()));

        // vfalco-fixme this generates errors
        // writelog (lstrace, ripplecalc)
        //     << "nextincrement: path after forward: " << pathstate_.getjson ();
    }
    else
    {
        pathstate_.setquality(0);
    }
    pathstate_.setstatus (status);
}

} // path
} // ripple
