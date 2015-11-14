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
#include <ripple/nodestore/impl/batchwriter.h>
    
namespace ripple {
namespace nodestore {

batchwriter::batchwriter (callback& callback, scheduler& scheduler)
    : m_callback (callback)
    , m_scheduler (scheduler)
    , mwriteload (0)
    , mwritepending (false)
{
    mwriteset.reserve (batchwritepreallocationsize);
}

batchwriter::~batchwriter ()
{
    waitforwriting ();
}

void
batchwriter::store (nodeobject::ref object)
{
    std::lock_guard<decltype(mwritemutex)> sl (mwritemutex);

    mwriteset.push_back (object);

    if (! mwritepending)
    {
        mwritepending = true;

        m_scheduler.scheduletask (*this);
    }
}

int
batchwriter::getwriteload ()
{
    std::lock_guard<decltype(mwritemutex)> sl (mwritemutex);

    return std::max (mwriteload, static_cast<int> (mwriteset.size ()));
}

void
batchwriter::performscheduledtask ()
{
    writebatch ();
}

void
batchwriter::writebatch ()
{
    for (;;)
    {
        std::vector< std::shared_ptr<nodeobject> > set;

        set.reserve (batchwritepreallocationsize);

        {
            std::lock_guard<decltype(mwritemutex)> sl (mwritemutex);

            mwriteset.swap (set);
            assert (mwriteset.empty ());
            mwriteload = set.size ();

            if (set.empty ())
            {
                mwritepending = false;
                mwritecondition.notify_all ();

                // vfalco note fix this function to not return from the middle
                return;
            }

        }

        batchwritereport report;
        report.writecount = set.size();
        auto const before = std::chrono::steady_clock::now();

        m_callback.writebatch (set);

        report.elapsed = std::chrono::duration_cast <std::chrono::milliseconds>
            (std::chrono::steady_clock::now() - before);

        m_scheduler.onbatchwrite (report);
    }
}

void
batchwriter::waitforwriting ()
{
    std::unique_lock <decltype(mwritemutex)> sl (mwritemutex);

    while (mwritepending)
        mwritecondition.wait (sl);
}

}
}
