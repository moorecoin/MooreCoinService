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

#ifndef ripple_nodestore_batchwriter_h_included
#define ripple_nodestore_batchwriter_h_included

#include <ripple/nodestore/scheduler.h>
#include <ripple/nodestore/task.h>
#include <ripple/nodestore/types.h>
#include <condition_variable>
#include <mutex>

namespace ripple {
namespace nodestore {

/** batch-writing assist logic.

    the batch writes are performed with a scheduled task. use of the
    class it not required. a backend can implement its own write batching,
    or skip write batching if doing so yields a performance benefit.

    @see scheduler
*/
class batchwriter : private task
{
public:
    /** this callback does the actual writing. */
    struct callback
    {
        virtual void writebatch (batch const& batch) = 0;
    };

    /** create a batch writer. */
    batchwriter (callback& callback, scheduler& scheduler);

    /** destroy a batch writer.

        anything pending in the batch is written out before this returns.
    */
    ~batchwriter ();

    /** store the object.

        this will add to the batch and initiate a scheduled task to
        write the batch out.
    */
    void store (nodeobject::ptr const& object);

    /** get an estimate of the amount of writing i/o pending. */
    int getwriteload ();

private:
    void performscheduledtask ();
    void writebatch ();
    void waitforwriting ();

private:
    typedef std::recursive_mutex locktype;
    typedef std::condition_variable_any condvartype;

    callback& m_callback;
    scheduler& m_scheduler;
    locktype mwritemutex;
    condvartype mwritecondition;
    int mwriteload;
    bool mwritepending;
    batch mwriteset;
};

}
}

#endif
