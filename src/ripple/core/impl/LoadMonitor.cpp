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
#include <ripple/basics/log.h>
#include <ripple/basics/uptimetimer.h>
#include <ripple/core/loadmonitor.h>

namespace ripple {

/*

todo
----

- use journal for logging

*/

//------------------------------------------------------------------------------

loadmonitor::stats::stats()
    : count (0)
    , latencyavg (0)
    , latencypeak (0)
    , isoverloaded (false)
{
}

//------------------------------------------------------------------------------

loadmonitor::loadmonitor ()
    : mcounts (0)
    , mlatencyevents (0)
    , mlatencymsavg (0)
    , mlatencymspeak (0)
    , mtargetlatencyavg (0)
    , mtargetlatencypk (0)
    , mlastupdate (uptimetimer::getinstance ().getelapsedseconds ())
{
}

// vfalco note why do we need "the mutex?" this dependence on
//         a hidden global, especially a synchronization primitive,
//         is a flawed design.
//         it's not clear exactly which data needs to be protected.
//
// call with the mutex
void loadmonitor::update ()
{
    int now = uptimetimer::getinstance ().getelapsedseconds ();

    // vfalco todo stop returning from the middle of functions.

    if (now == mlastupdate) // current
        return;

    // vfalco todo why 8?
    if ((now < mlastupdate) || (now > (mlastupdate + 8)))
    {
        // way out of date
        mcounts = 0;
        mlatencyevents = 0;
        mlatencymsavg = 0;
        mlatencymspeak = 0;
        mlastupdate = now;
        // vfalco todo don't return from the middle...
        return;
    }

    // do exponential decay
    /*
        david:

        "imagine if you add 10 to something every second. and you
         also reduce it by 1/4 every second. it will "idle" at 40,
         correponding to 10 counts per second."
    */
    do
    {
        ++mlastupdate;
        mcounts -= ((mcounts + 3) / 4);
        mlatencyevents -= ((mlatencyevents + 3) / 4);
        mlatencymsavg -= (mlatencymsavg / 4);
        mlatencymspeak -= (mlatencymspeak / 4);
    }
    while (mlastupdate < now);
}

void loadmonitor::addcount ()
{
    scopedlocktype sl (mlock);

    update ();
    ++mcounts;
}

void loadmonitor::addlatency (int latency)
{
    // vfalco note why does 1 become 0?
    if (latency == 1)
        latency = 0;

    scopedlocktype sl (mlock);

    update ();

    ++mlatencyevents;
    mlatencymsavg += latency;
    mlatencymspeak += latency;

    // units are quarters of a millisecond
    int const latencypeak = mlatencyevents * latency * 4;

    if (mlatencymspeak < latencypeak)
        mlatencymspeak = latencypeak;
}

std::string loadmonitor::printelapsed (double seconds)
{
    std::stringstream ss;
    ss << (std::size_t (seconds * 1000 + 0.5)) << " ms";
    return ss.str();
}

void loadmonitor::addloadsample (loadevent const& sample)
{
    std::string const& name (sample.name());
    beast::relativetime const latency (sample.getsecondstotal());

    if (latency.inseconds() > 0.5)
    {
        writelog ((latency.inseconds() > 1.0) ? lswarning : lsinfo, loadmonitor)
            << "job: " << name << " executiontime: " << printelapsed (sample.getsecondsrunning()) <<
            " waitingtime: " << printelapsed (sample.getsecondswaiting());
    }

    // vfalco note why does 1 become 0?
    std::size_t latencymilliseconds (latency.inmilliseconds());
    if (latencymilliseconds == 1)
        latencymilliseconds = 0;

    scopedlocktype sl (mlock);

    update ();
    ++mcounts;
    ++mlatencyevents;
    mlatencymsavg += latencymilliseconds;
    mlatencymspeak += latencymilliseconds;

    // vfalco note why are we multiplying by 4?
    int const latencypeak = mlatencyevents * latencymilliseconds * 4;

    if (mlatencymspeak < latencypeak)
        mlatencymspeak = latencypeak;
}

/* add multiple samples
   @param count the number of samples to add
   @param latencyms the total number of milliseconds
*/
void loadmonitor::addsamples (int count, std::chrono::milliseconds latency)
{
    scopedlocktype sl (mlock);

    update ();
    mcounts += count;
    mlatencyevents += count;
    mlatencymsavg += latency.count();
    mlatencymspeak += latency.count();

    int const latencypeak = mlatencyevents * latency.count() * 4 / count;

    if (mlatencymspeak < latencypeak)
        mlatencymspeak = latencypeak;
}

void loadmonitor::settargetlatency (std::uint64_t avg, std::uint64_t pk)
{
    mtargetlatencyavg  = avg;
    mtargetlatencypk = pk;
}

bool loadmonitor::isovertarget (std::uint64_t avg, std::uint64_t peak)
{
    return (mtargetlatencypk && (peak > mtargetlatencypk)) ||
           (mtargetlatencyavg && (avg > mtargetlatencyavg));
}

bool loadmonitor::isover ()
{
    scopedlocktype sl (mlock);

    update ();

    if (mlatencyevents == 0)
        return 0;

    return isovertarget (mlatencymsavg / (mlatencyevents * 4), mlatencymspeak / (mlatencyevents * 4));
}

loadmonitor::stats loadmonitor::getstats ()
{
    stats stats;

    scopedlocktype sl (mlock);

    update ();

    stats.count = mcounts / 4;

    if (mlatencyevents == 0)
    {
        stats.latencyavg = 0;
        stats.latencypeak = 0;
    }
    else
    {
        stats.latencyavg = mlatencymsavg / (mlatencyevents * 4);
        stats.latencypeak = mlatencymspeak / (mlatencyevents * 4);
    }

    stats.isoverloaded = isovertarget (stats.latencyavg, stats.latencypeak);

    return stats;
}

} // ripple
