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

#ifndef ripple_peerfinder_reporting_h_included
#define ripple_peerfinder_reporting_h_included

namespace ripple {
namespace peerfinder {

/** severity levels for test reporting.
    this allows more fine grained control over reporting for diagnostics.
*/
struct reporting
{
    // report simulation parameters
    static bool const params = true;

    // report simulation crawl time-evolution
    static bool const crawl = true;

    // report nodes aggregate statistics
    static bool const nodes = true;

    // report nodes detailed information
    static bool const dump_nodes = false;

    //
    //
    //

    // reports from network (and children)
    static beast::journal::severity const network = beast::journal::kwarning;

    // reports from simulation node (and children)
    static beast::journal::severity const node = beast::journal::kall;

    //
    //
    //

    // reports from logic
    static beast::journal::severity const logic = beast::journal::kall;

    // reports from livecache
    static beast::journal::severity const livecache = beast::journal::kall;

    // reports from bootcache
    static beast::journal::severity const bootcache = beast::journal::kall;
};

}
}

#endif
