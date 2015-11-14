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
#include <tuple>

namespace ripple {
namespace path {

ter pathcursor::liquidity (ledgerentryset const& lescheckpoint) const
{
    ter resultcode = tecpath_dry;
    pathcursor pc = *this;

    ledger() = lescheckpoint.duplicate ();
    for (pc.nodeindex_ = pc.nodesize(); pc.nodeindex_--; )
    {
        writelog (lstrace, ripplecalc)
            << "reverseliquidity>"
            << " nodeindex=" << pc.nodeindex_
            << ".issue_.account=" << to_string (pc.node().issue_.account);

        resultcode  = pc.reverseliquidity();

        writelog (lstrace, ripplecalc)
            << "reverseliquidity< "
            << "nodeindex=" << pc.nodeindex_
            << " resultcode=" << transtoken (resultcode)
            << " transferrate_=" << pc.node().transferrate_
            << "/" << resultcode;

        if (resultcode != tessuccess)
            break;
    }

    // vfalco-fixme this generates errors
    // writelog (lstrace, ripplecalc)
    //     << "nextincrement: path after reverse: " << pathstate_.getjson ();

    if (resultcode != tessuccess)
        return resultcode;

    // do forward.
    ledger() = lescheckpoint.duplicate ();
    for (pc.nodeindex_ = 0; pc.nodeindex_ < pc.nodesize(); ++pc.nodeindex_)
    {
        writelog (lstrace, ripplecalc)
            << "forwardliquidity> nodeindex=" << nodeindex_;

        resultcode = pc.forwardliquidity();
        if (resultcode != tessuccess)
            return resultcode;

        writelog (lstrace, ripplecalc)
            << "forwardliquidity<"
            << " nodeindex:" << pc.nodeindex_
            << " resultcode:" << resultcode;

        if (pathstate_.isdry())
            resultcode = tecpath_dry;
    }
    return resultcode;
}

} // path
} // ripple
