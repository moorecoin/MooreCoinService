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

#ifndef beast_threads_waitableevent_h_included
#define beast_threads_waitableevent_h_included

#include <beast/config.h>

#if ! beast_windows
#include <pthread.h>
#endif

namespace beast {

/** allows threads to wait for events triggered by other threads.
    a thread can call wait() on a waitableevent, and this will suspend the
    calling thread until another thread wakes it up by calling the signal()
    method.
*/
class waitableevent
    //, leakchecked <waitableevent> // vfalco todo move leakchecked to beast/
{
public:
    //==============================================================================
    /** creates a waitableevent object.

        @param manualreset  if this is false, the event will be reset automatically when the wait()
                            method is called. if manualreset is true, then once the event is signalled,
                            the only way to reset it will be by calling the reset() method.

        @param initiallysignaled if this is true then the event will be signaled when
                                 the constructor returns.
    */
    explicit waitableevent (bool manualreset = false, bool initiallysignaled = false);

    /** destructor.

        if other threads are waiting on this object when it gets deleted, this
        can cause nasty errors, so be careful!
    */
    ~waitableevent();

    waitableevent (waitableevent const&) = delete;
    waitableevent& operator= (waitableevent const&) = delete;

    //==============================================================================
    /** suspends the calling thread until the event has been signalled.

        this will wait until the object's signal() method is called by another thread,
        or until the timeout expires.

        after the event has been signalled, this method will return true and if manualreset
        was set to false in the waitableevent's constructor, then the event will be reset.

        @param timeoutmilliseconds  the maximum time to wait, in milliseconds. a negative
                                    value will cause it to wait forever.

        @returns    true if the object has been signalled, false if the timeout expires first.
        @see signal, reset
    */
    /** @{ */
    bool wait () const;                             // wait forever
    // vfalco todo change wait() to seconds instead of millis
    bool wait (int timeoutmilliseconds) const; // deprecated
    /** @} */

    //==============================================================================
    /** wakes up any threads that are currently waiting on this object.

        if signal() is called when nothing is waiting, the next thread to call wait()
        will return immediately and reset the signal.

        if the waitableevent is manual reset, all current and future threads that wait upon this
        object will be woken, until reset() is explicitly called.

        if the waitableevent is automatic reset, and one or more threads is waiting upon the object,
        then one of them will be woken up. if no threads are currently waiting, then the next thread
        to call wait() will be woken up. as soon as a thread is woken, the signal is automatically
        reset.

        @see wait, reset
    */
    void signal() const;

    //==============================================================================
    /** resets the event to an unsignalled state.

        if it's not already signalled, this does nothing.
    */
    void reset() const;

private:
#if beast_windows
    void* handle;
#else
    mutable pthread_cond_t condition;
    mutable pthread_mutex_t mutex;
    mutable bool triggered;
    mutable bool manualreset;
#endif
};

}

#endif
