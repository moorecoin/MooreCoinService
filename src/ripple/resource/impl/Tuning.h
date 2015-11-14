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

#ifndef ripple_resource_tuning_h_included
#define ripple_resource_tuning_h_included

namespace ripple {
namespace resource {

/** tunable constants. */
enum
{
    // balance at which a warning is issued
     warningthreshold           = 500

    // balance at which the consumer is disconnected
    ,dropthreshold              = 1500

    // the number of seconds until an inactive table item is removed
    ,secondsuntilexpiration     = 300

    // the number of seconds in the exponential decay window
    // (this should be a power of two)
    ,decaywindowseconds         = 32

    // the minimum balance required in order to include a load source in gossip
    ,minimumgossipbalance       = 100

    // number of seconds until imported gossip expires
    ,gossipexpirationseconds    = 30
};

}
}

#endif
