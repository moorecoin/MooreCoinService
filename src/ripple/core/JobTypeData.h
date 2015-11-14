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

#ifndef ripple_core_jobtypedata_h_included
#define ripple_core_jobtypedata_h_included

#include <ripple/core/jobtypeinfo.h>

namespace ripple
{

struct jobtypedata
{
private:
    loadmonitor m_load;

    /* support for insight */
    beast::insight::collector::ptr m_collector;

public:
    /* the job category which we represent */
    jobtypeinfo const& info;

    /* the number of jobs waiting */
    int waiting;

    /* the number presently running */
    int running;

    /* and the number we deferred executing because of job limits */
    int deferred;

    /* notification callbacks */
    beast::insight::event dequeue;
    beast::insight::event execute;

    explicit jobtypedata (jobtypeinfo const& info_,
            beast::insight::collector::ptr const& collector) noexcept
        : m_collector (collector)
        , info (info_)
        , waiting (0)
        , running (0)
        , deferred (0)
    {
        m_load.settargetlatency (
            info.getaveragelatency (),
            info.getpeaklatency());

        if (!info.special ())
        {
            dequeue = m_collector->make_event (info.name () + "_q");
            execute = m_collector->make_event (info.name ());
        }
    }

    /* not copy-constructible or assignable */
    jobtypedata (jobtypedata const& other) = delete;
    jobtypedata& operator= (jobtypedata const& other) = delete;

    std::string name () const
    {
        return info.name ();
    }

    jobtype type () const
    {
        return info.type ();
    }

    loadmonitor& load ()
    {
        return m_load;
    }

    loadmonitor::stats stats ()
    {
        return m_load.getstats ();
    }
};

}

#endif
