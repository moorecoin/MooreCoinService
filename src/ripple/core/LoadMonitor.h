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

#ifndef ripple_loadmonitor_h_included
#define ripple_loadmonitor_h_included

#include <ripple/core/loadevent.h>
#include <chrono>
#include <mutex>

namespace ripple {

// monitors load levels and response times

// vfalco todo rename this. having both loadmanager and loadmonitor is confusing.
//
class loadmonitor
{
public:
    loadmonitor ();

    void addcount ();

    void addlatency (int latency);

    void addloadsample (loadevent const& sample);

    void addsamples (int count, std::chrono::milliseconds latency);

    void settargetlatency (std::uint64_t avg, std::uint64_t pk);

    bool isovertarget (std::uint64_t avg, std::uint64_t peak);

    // vfalco todo make this return the values in a struct.
    struct stats
    {
        stats();

        std::uint64_t count;
        std::uint64_t latencyavg;
        std::uint64_t latencypeak;
        bool isoverloaded;
    };

    stats getstats ();

    bool isover ();

private:
    static std::string printelapsed (double seconds);

    void update ();

    typedef std::mutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;
    locktype mlock;

    std::uint64_t mcounts;
    int           mlatencyevents;
    std::uint64_t mlatencymsavg;
    std::uint64_t mlatencymspeak;
    std::uint64_t mtargetlatencyavg;
    std::uint64_t mtargetlatencypk;
    int           mlastupdate;
};

} // ripple

#endif
