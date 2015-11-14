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

#ifndef ripple_peerfinder_sim_params_h_included
#define ripple_peerfinder_sim_params_h_included

namespace ripple {
namespace peerfinder {
namespace sim {

/** defines the parameters for a network simulation. */
struct params
{
    params ()
        : steps (50)
        , nodes (10)
        , maxpeers (20)
        , outpeers (9.5)
        , firewalled (0)
    {
    }

    int steps;
    int nodes;
    int maxpeers;
    double outpeers;
    double firewalled; // [0, 1)
};

}
}
}

#endif
