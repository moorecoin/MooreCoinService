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
#include <ripple/app/main/nodestorescheduler.h>

namespace ripple {

nodestorescheduler::nodestorescheduler (stoppable& parent)
    : stoppable ("nodestorescheduler", parent)
    , m_jobqueue (nullptr)
    , m_taskcount (0)
{
}

void nodestorescheduler::setjobqueue (jobqueue& jobqueue)
{
    m_jobqueue = &jobqueue;
}

void nodestorescheduler::onstop ()
{
}

void nodestorescheduler::onchildrenstopped ()
{
    bassert (m_taskcount == 0);
    stopped ();
}

void nodestorescheduler::scheduletask (nodestore::task& task)
{
    ++m_taskcount;
    m_jobqueue->addjob (
        jtwrite,
        "nodeobject::store",
        std::bind (&nodestorescheduler::dotask,
            this, std::ref(task), std::placeholders::_1));
}

void nodestorescheduler::dotask (nodestore::task& task, job&)
{
    task.performscheduledtask ();
    if ((--m_taskcount == 0) && isstopping())
        stopped();
}

void nodestorescheduler::onfetch (nodestore::fetchreport const& report)
{
    if (report.wenttodisk)
        m_jobqueue->addloadevents (
            report.isasync ? jtns_async_read : jtns_sync_read,
                1, report.elapsed);
}

void nodestorescheduler::onbatchwrite (nodestore::batchwritereport const& report)
{
    m_jobqueue->addloadevents (jtns_write,
        report.writecount, report.elapsed);
}

} // ripple
