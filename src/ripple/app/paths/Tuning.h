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

#ifndef ripple_app_paths_tuning_h
#define ripple_app_paths_tuning_h

namespace ripple {

int const calc_node_deliver_max_loops = 40;
int const node_advance_max_loops = 100;
int const pathfinder_high_priority = 100000;
int const pathfinder_max_paths = 50;
int const pathfinder_max_complete_paths = 1000;
int const pathfinder_max_paths_from_source = 10;

} // ripple

#endif
