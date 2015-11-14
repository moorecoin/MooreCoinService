//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#include <beast/unit_test/suite.h>

namespace beast {

workers::workers (
    callback& callback,
    std::string const& threadnames,
    int numberofthreads)
        : m_callback (callback)
        , m_threadnames (threadnames)
        , m_allpaused (true, true)
        , m_semaphore (0)
        , m_numberofthreads (0)
        , m_activecount (0)
        , m_pausecount (0)
        , m_runningtaskcount (0)
{
    setnumberofthreads (numberofthreads);
}

workers::~workers ()
{
    pauseallthreadsandwait ();

    deleteworkers (m_everyone);
}

int workers::getnumberofthreads () const noexcept
{
    return m_numberofthreads;
}

// vfalco note if this function is called quickly to reduce then
//             increase the number of threads, it could result in
//             more paused threads being created than expected.
//
void workers::setnumberofthreads (int numberofthreads)
{
    if (m_numberofthreads != numberofthreads)
    {
        if (numberofthreads > m_numberofthreads)
        {
            // increasing the number of working threads

            int const amount = numberofthreads - m_numberofthreads;

            for (int i = 0; i < amount; ++i)
            {
                // see if we can reuse a paused worker
                worker* worker = m_paused.pop_front ();

                if (worker != nullptr)
                {
                    // if we got here then the worker thread is at [1]
                    // this will unblock their call to wait()
                    //
                    worker->notify ();
                }
                else
                {
                    worker = new worker (*this, m_threadnames);
                }

                m_everyone.push_front (worker);
            }
        }
        else if (numberofthreads < m_numberofthreads)
        {
            // decreasing the number of working threads

            int const amount = m_numberofthreads - numberofthreads;

            for (int i = 0; i < amount; ++i)
            {
                ++m_pausecount;

                // pausing a thread counts as one "internal task"
                m_semaphore.signal ();
            }
        }

        m_numberofthreads = numberofthreads;
    }
}

void workers::pauseallthreadsandwait ()
{
    setnumberofthreads (0);

    m_allpaused.wait ();

    bassert (numberofcurrentlyrunningtasks () == 0);
}

void workers::addtask ()
{
    m_semaphore.signal ();
}

int workers::numberofcurrentlyrunningtasks () const noexcept
{
    return m_runningtaskcount.load ();
}

void workers::deleteworkers (lockfreestack <worker>& stack)
{
    for (;;)
    {
        worker* const worker = stack.pop_front ();

        if (worker != nullptr)
        {
            // this call blocks until the thread orderly exits 
            delete worker;
        }
        else
        {
            break;
        }
    }
}

//------------------------------------------------------------------------------

workers::worker::worker (workers& workers, std::string const& threadname)
    : thread (threadname)
    , m_workers (workers)
{
    startthread ();
}

workers::worker::~worker ()
{
    stopthread ();
}

void workers::worker::run ()
{
    while (! threadshouldexit ())
    {
        // increment the count of active workers, and if
        // we are the first one then reset the "all paused" event
        //
        if (++m_workers.m_activecount == 1)
            m_workers.m_allpaused.reset ();

        for (;;)
        {
            // acquire a task or "internal task."
            //
            m_workers.m_semaphore.wait ();

            // see if there's a pause request. this
            // counts as an "internal task."
            //
            int pausecount = m_workers.m_pausecount.load ();

            if (pausecount > 0)
            {
                // try to decrement
                pausecount = --m_workers.m_pausecount;

                if (pausecount >= 0)
                {
                    // we got paused
                    break;
                }
                else
                {
                    // undo our decrement
                    ++m_workers.m_pausecount;
                }
            }

            // we couldn't pause so we must have gotten
            // unblocked in order to process a task.
            //
            ++m_workers.m_runningtaskcount;
            m_workers.m_callback.processtask ();
            --m_workers.m_runningtaskcount;

            // put the name back in case the callback changed it
            thread::setcurrentthreadname (thread::getthreadname());
        }

        // any worker that goes into the paused list must
        // guarantee that it will eventually block on its
        // event object.
        //
        m_workers.m_paused.push_front (this);

        // decrement the count of active workers, and if we
        // are the last one then signal the "all paused" event.
        //
        if (--m_workers.m_activecount == 0)
            m_workers.m_allpaused.signal ();

        thread::setcurrentthreadname ("(" + getthreadname() + ")");

        // [1] we will be here when the paused list is popped
        //
        // we block on our event object, a requirement of being
        // put into the paused list.
        //
        // this will get signaled on either a reactivate or a stopthread()
        //
        wait ();
    }
}

//------------------------------------------------------------------------------

class workers_test : public unit_test::suite
{
public:
    struct testcallback : workers::callback
    {
        explicit testcallback (int count_)
            : finished (false, count_ == 0)
            , count (count_)
        {
        }

        void processtask ()
        {
            if (--count == 0)
                finished.signal ();
        }

        waitableevent finished;
        std::atomic <int> count;
    };

    void testthreads (int const threadcount)
    {
        testcase ("threadcount = " + std::to_string (threadcount));

        testcallback cb (threadcount);

        workers w (cb, "test", 0);
        expect (w.getnumberofthreads () == 0);

        w.setnumberofthreads (threadcount);
        expect (w.getnumberofthreads () == threadcount);

        for (int i = 0; i < threadcount; ++i)
            w.addtask ();

        // 10 seconds should be enough to finish on any system
        //
        bool signaled = cb.finished.wait (10 * 1000);
        expect (signaled, "timed out");

        w.pauseallthreadsandwait ();

        // we had better finished all our work!
        expect (cb.count.load () == 0, "did not complete task!");
    }

    void run ()
    {
        testthreads (0);
        testthreads (1);
        testthreads (2);
        testthreads (4);
        testthreads (16);
        testthreads (64);
    }
};

beast_define_testsuite(workers,beast_core,beast);

} // beast
