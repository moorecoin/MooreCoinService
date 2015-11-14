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

#ifndef ripple_nodestore_scheduler_h_included
#define ripple_nodestore_scheduler_h_included

#include <ripple/nodestore/task.h>
#include <chrono>

namespace ripple {
namespace nodestore {

/** contains information about a fetch operation. */
struct fetchreport
{
    std::chrono::milliseconds elapsed;
    bool isasync;
    bool wenttodisk;
    bool wasfound;
};

/** contains information about a batch write operation. */
struct batchwritereport
{
    std::chrono::milliseconds elapsed;
    int writecount;
};

/** scheduling for asynchronous backend activity

    for improved performance, a backend has the option of performing writes
    in batches. these writes can be scheduled using the provided scheduler
    object.

    @see batchwriter
*/
class scheduler
{
public:
    virtual ~scheduler() = default;

    /** schedules a task.
        depending on the implementation, the task may be invoked either on
        the current thread of execution, or an unspecified implementation-defined
        foreign thread.
    */
    virtual void scheduletask (task& task) = 0;

    /** reports completion of a fetch
        allows the scheduler to monitor the node store's performance
    */
    virtual void onfetch (fetchreport const& report) = 0;

    /** reports the completion of a batch write
        allows the scheduler to monitor the node store's performance
    */
    virtual void onbatchwrite (batchwritereport const& report) = 0;
};

}
}

#endif
