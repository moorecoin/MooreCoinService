//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    portions of this file are from juce.
    copyright (c) 2013 - raw material software ltd.
    please visit http://www.juce.com

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

#ifndef beast_threads_thread_h_included
#define beast_threads_thread_h_included

#include <beast/utility/leakchecked.h>
#include <beast/threads/recursivemutex.h>
#include <beast/threads/waitableevent.h>

#include <string>

namespace beast {

//==============================================================================
/**
    encapsulates a thread.

    subclasses derive from thread and implement the run() method, in which they
    do their business. the thread can then be started with the startthread() method
    and controlled with various other methods.

    @see criticalsection, waitableevent, process, threadwithprogresswindow,
         messagemanagerlock
*/
class thread : leakchecked <thread>
{
public:
    //==============================================================================
    /**
        creates a thread.

        when first created, the thread is not running. use the startthread()
        method to start it.
    */
    explicit thread (std::string const& threadname);

    thread (thread const&) = delete;
    thread& operator= (thread const&) = delete;
    
    /** destructor.

        if the thread has not been stopped first, this will generate a fatal error.
    */
    virtual ~thread();

    //==============================================================================
    /** must be implemented to perform the thread's actual code.

        remember that the thread must regularly check the threadshouldexit()
        method whilst running, and if this returns true it should return from
        the run() method as soon as possible to avoid being forcibly killed.

        @see threadshouldexit, startthread
    */
    virtual void run() = 0;

    //==============================================================================
    // thread control functions..

    /** starts the thread running.

        this will start the thread's run() method.
        (if it's already started, startthread() won't do anything).

        @see stopthread
    */
    void startthread();

    /** attempts to stop the thread running.

        this method will cause the threadshouldexit() method to return true
        and call notify() in case the thread is currently waiting.
    */
    void stopthread ();

    /** stop the thread without blocking.
        this calls signalthreadshouldexit followed by notify.
    */
    void stopthreadasync ();

    //==============================================================================
    /** returns true if the thread is currently active */
    bool isthreadrunning() const;

    /** sets a flag to tell the thread it should stop.

        calling this means that the threadshouldexit() method will then return true.
        the thread should be regularly checking this to see whether it should exit.

        if your thread makes use of wait(), you might want to call notify() after calling
        this method, to interrupt any waits that might be in progress, and allow it
        to reach a point where it can exit.

        @see threadshouldexit
        @see waitforthreadtoexit
    */
    void signalthreadshouldexit();

    /** checks whether the thread has been told to stop running.

        threads need to check this regularly, and if it returns true, they should
        return from their run() method at the first possible opportunity.

        @see signalthreadshouldexit
    */
    inline bool threadshouldexit() const { return shouldexit; }

    /** waits for the thread to stop.

        this will waits until isthreadrunning() is false.
    */
    void waitforthreadtoexit () const;

    //==============================================================================
    /** makes the thread wait for a notification.

        this puts the thread to sleep until either the timeout period expires, or
        another thread calls the notify() method to wake it up.

        a negative time-out value means that the method will wait indefinitely.

        @returns    true if the event has been signalled, false if the timeout expires.
    */
    bool wait (int timeoutmilliseconds = -1) const;

    /** wakes up the thread.

        if the thread has called the wait() method, this will wake it up.

        @see wait
    */
    void notify() const;

    //==============================================================================
    /** returns the name of the thread.

        this is the name that gets set in the constructor.
    */
    std::string const& getthreadname() const { return threadname; }

    /** changes the name of the caller thread.
        different oses may place different length or content limits on this name.
    */
    static void setcurrentthreadname (std::string const& newthreadname);


private:
    //==============================================================================
    std::string const threadname;
    void* volatile threadhandle;
    recursivemutex startstoplock;
    waitableevent startsuspensionevent, defaultevent;
    bool volatile shouldexit;

   #ifndef doxygen
    friend void beast_threadentrypoint (void*);
   #endif

    void launchthread();
    void closethreadhandle();
    void threadentrypoint();
};

}

#endif

