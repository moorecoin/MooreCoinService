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

#ifndef ripple_peerfinder_tuning_h_included
#define ripple_peerfinder_tuning_h_included

#include <array>

namespace ripple {
namespace peerfinder {

/** heuristically tuned constants. */
/** @{ */
namespace tuning {

enum
{
    //---------------------------------------------------------
    //
    // automatic connection policy
    //
    //---------------------------------------------------------

    /** time to wait between making batches of connection attempts */
    secondsperconnect = 10

    /** maximum number of simultaneous connection attempts. */
    ,maxconnectattempts = 20

    /** the percentage of total peer slots that are outbound.
        the number of outbound peers will be the larger of the
        minoutcount and outpercent * config::maxpeers specially
        rounded.
    */
    ,outpercent = 15

    /** a hard minimum on the number of outgoing connections.
        this is enforced outside the logic, so that the unit test
        can use any settings it wants.
    */
    ,minoutcount = 10

    /** the default value of config::maxpeers. */
    ,defaultmaxpeers = 21

    /** max redirects we will accept from one connection.
        redirects are limited for security purposes, to prevent
        the address caches from getting flooded.
    */
    ,maxredirects = 30
};

//------------------------------------------------------------------------------
//
// fixed
//
//------------------------------------------------------------------------------

static std::array <int, 10> const connectionbackoff
    {{ 1, 1, 2, 3, 5, 8, 13, 21, 34, 55 }};

//------------------------------------------------------------------------------
//
// bootcache
//
//------------------------------------------------------------------------------

enum
{
    // threshold of cache entries above which we trim.
    bootcachesize = 1000

    // the percentage of addresses we prune when we trim the cache.
    ,bootcacheprunepercent = 10
};

// the cool down wait between database updates
// ideally this should be larger than the time it takes a full
// peer to send us a set of addresses and then disconnect.
//
static std::chrono::seconds const bootcachecooldowntime (60);

//------------------------------------------------------------------------------
//
// livecache
//
//------------------------------------------------------------------------------

enum
{
    // drop incoming messages with hops greater than this number
    maxhops = 6

    // how many endpoint to send in each mtendpoints
    ,numberofendpoints = 2 * maxhops

    // the most endpoint we will accept in mtendpoints
    ,numberofendpointsmax = 20

    // the number of peers that we want by default, unless an
    // explicit value is set in the config file.
    ,defaultmaxpeercount = 21

    /** number of addresses we provide when redirecting. */
    ,redirectendpointcount = 10
};

// how often we send or accept mtendpoints messages per peer
static std::chrono::seconds const secondspermessage (5);

// how long an endpoint will stay in the cache
// this should be a small multiple of the broadcast frequency
static std::chrono::seconds const livecachesecondstolive (30);

//
//
//

// how much time to wait before trying an outgoing address again.
// note that we ignore the port for purposes of comparison.
static std::chrono::seconds const recentattemptduration (60);

}
/** @} */

}
}

#endif
