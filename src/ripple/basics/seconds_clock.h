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

#ifndef ripple_basics_seconds_clock_h_included
#define ripple_basics_seconds_clock_h_included

#include <beast/chrono/abstract_clock.h>
#include <beast/chrono/basic_seconds_clock.h>

#include <chrono>

namespace ripple {

using days = std::chrono::duration
    <int, std::ratio_multiply<std::chrono::hours::period, std::ratio<24>>>;

using weeks = std::chrono::duration
    <int, std::ratio_multiply<days::period, std::ratio<7>>>;

/** returns an abstract_clock optimized for counting seconds. */
inline
beast::abstract_clock<std::chrono::steady_clock>&
get_seconds_clock()
{
    return beast::get_abstract_clock<std::chrono::steady_clock,
        beast::basic_seconds_clock<std::chrono::steady_clock>>();
}

}

#endif
