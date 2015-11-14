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

#ifndef ripple_app_nodestorescheduler_h_included
#define ripple_app_nodestorescheduler_h_included

#include <ripple/nodestore/scheduler.h>
#include <ripple/core/jobqueue.h>
#include <beast/threads/stoppable.h>
#include <atomic>

namespace ripple {

/** a nodestore::scheduler which uses the jobqueue and implements the stoppable api. */
class nodestorescheduler
    : public nodestore::scheduler
    , public beast::stoppable
{
public:
    nodestorescheduler (stoppable& parent);

    // vfalco note this is a temporary hack to solve the problem
    //             of circular dependency.
    //
    void setjobqueue (jobqueue& jobqueue);

    void onstop ();
    void onchildrenstopped ();
    void scheduletask (nodestore::task& task);
    void onfetch (nodestore::fetchreport const& report) override;
    void onbatchwrite (nodestore::batchwritereport const& report) override;

private:
    void dotask (nodestore::task& task, job&);

    jobqueue* m_jobqueue;
    std::atomic <int> m_taskcount;
};

} // ripple

#endif
