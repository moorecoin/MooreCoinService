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

#ifndef beast_workers_h_included
#define beast_workers_h_included

#include <beast/threads/thread.h>
#include <beast/threads/semaphore.h>

#include <atomic>
#include <string>
#include <thread>

namespace beast {

/** a group of threads that process tasks.
*/
class workers
{
public:
    /** called to perform tasks as needed. */
    struct callback
    {
        /** perform a task.

            the call is made on a thread owned by workers. it is important
            that you only process one task from inside your callback. each
            call to addtask will result in exactly one call to processtask.

            @see workers::addtask
        */
        virtual void processtask () = 0;
    };

    /** create the object.

        a number of initial threads may be optionally specified. the
        default is to create one thread per cpu.

        @param threadnames the name given to each created worker thread.
    */
    explicit workers (callback& callback,
                      std::string const& threadnames = "worker",
                      int numberofthreads =
                         static_cast<int>(std::thread::hardware_concurrency()));

    ~workers ();

    /** retrieve the desired number of threads.

        this just returns the number of active threads that were requested. if
        there was a recent call to setnumberofthreads, the actual number of active
        threads may be temporarily different from what was last requested.

        @note this function is not thread-safe.
    */
    int getnumberofthreads () const noexcept;

    /** set the desired number of threads.
        @note this function is not thread-safe.
    */
    void setnumberofthreads (int numberofthreads);

    /** pause all threads and wait until they are paused.

        if a thread is processing a task it will pause as soon as the task
        completes. there may still be tasks signaled even after all threads
        have paused.

        @note this function is not thread-safe.
    */
    void pauseallthreadsandwait ();

    /** add a task to be performed.

        every call to addtask will eventually result in a call to
        callback::processtask unless the workers object is destroyed or
        the number of threads is never set above zero.

        @note this function is thread-safe.
    */
    void addtask ();

    /** get the number of currently executing calls of callback::processtask.
        while this function is thread-safe, the value may not stay
        accurate for very long. it's mainly for diagnostic purposes.
    */
    int numberofcurrentlyrunningtasks () const noexcept;

    //--------------------------------------------------------------------------

private:
    struct pausedtag { };

    /*  a worker executes tasks on its provided thread.

        these are the states:

        active: running the task processing loop.
        idle:   active, but blocked on waiting for a task.
        pausd:  blocked waiting to exit or become active.
    */
    class worker
        : public lockfreestack <worker>::node
        , public lockfreestack <worker, pausedtag>::node
        , public thread
    {
    public:
        worker (workers& workers, std::string const& threadname);

        ~worker ();

    private:
        void run ();

    private:
        workers& m_workers;
    };

private:
    static void deleteworkers (lockfreestack <worker>& stack);

private:
    callback& m_callback;
    std::string m_threadnames;                   // the name to give each thread
    waitableevent m_allpaused;                   // signaled when all threads paused
    semaphore m_semaphore;                       // each pending task is 1 resource
    int m_numberofthreads;                       // how many we want active now
    std::atomic <int> m_activecount;             // to know when all are paused
    std::atomic <int> m_pausecount;              // how many threads need to pause now
    std::atomic <int> m_runningtaskcount;        // how many calls to processtask() active
    lockfreestack <worker> m_everyone;           // holds all created workers
    lockfreestack <worker, pausedtag> m_paused;  // holds just paused workers
};

} // beast

#endif
