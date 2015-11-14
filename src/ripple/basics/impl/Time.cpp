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
#include <ripple/basics/time.h>

namespace ripple {

// vfalco todo tidy this up into a rippletime object

//
// time support
// we have our own epoch.
//

boost::posix_time::ptime ptepoch ()
{
    return boost::posix_time::ptime (boost::gregorian::date (2000, boost::gregorian::jan, 1));
}

int itoseconds (boost::posix_time::ptime ptwhen)
{
    return ptwhen.is_not_a_date_time ()
           ? -1
           : (ptwhen - ptepoch ()).total_seconds ();
}

// convert our time in seconds to a ptime.
boost::posix_time::ptime ptfromseconds (int iseconds)
{
    return iseconds < 0
           ? boost::posix_time::ptime (boost::posix_time::not_a_date_time)
           : ptepoch () + boost::posix_time::seconds (iseconds);
}

// convert from our time to unix time in seconds.
uint64_t utfromseconds (int iseconds)
{
    boost::posix_time::time_duration    tddelta =
        boost::posix_time::ptime (boost::gregorian::date (2000, boost::gregorian::jan, 1))
        - boost::posix_time::ptime (boost::gregorian::date (1970, boost::gregorian::jan, 1))
        + boost::posix_time::seconds (iseconds)
        ;

    return tddelta.total_seconds ();
}

} // ripple
