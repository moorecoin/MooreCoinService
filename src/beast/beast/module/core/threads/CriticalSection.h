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

#ifndef beast_criticalsection_h_included
#define beast_criticalsection_h_included

#include <beast/module/core/threads/scopedlock.h>
#include <cstdint>

namespace beast {

//==============================================================================
/**
    a re-entrant mutex.

    a criticalsection acts as a re-entrant mutex object. the best way to lock and unlock
    one of these is by using raii in the form of a local scopedlock object - have a look
    through the codebase for many examples of how to do this.

    @see scopedlock, scopedtrylock, scopedunlock, spinlock, thread
*/
class criticalsection
{
public:
    //==============================================================================
    /** creates a criticalsection object. */
    criticalsection() noexcept;

    criticalsection (criticalsection const&) = delete;
    criticalsection& operator= (criticalsection const&) = delete;

    /** destructor.
        if the critical section is deleted whilst locked, any subsequent behaviour
        is unpredictable.
    */
    ~criticalsection() noexcept;

    //==============================================================================
    /** acquires the lock.

        if the lock is already held by the caller thread, the method returns immediately.
        if the lock is currently held by another thread, this will wait until it becomes free.

        it's strongly recommended that you never call this method directly - instead use the
        scopedlock class to manage the locking using an raii pattern instead.

        @see exit, tryenter, scopedlock
    */
    void enter() const noexcept;

    /** attempts to lock this critical section without blocking.

        this method behaves identically to criticalsection::enter, except that the caller thread
        does not wait if the lock is currently held by another thread but returns false immediately.

        @returns false if the lock is currently held by another thread, true otherwise.
        @see enter
    */
    bool tryenter() const noexcept;

    /** releases the lock.

        if the caller thread hasn't got the lock, this can have unpredictable results.

        if the enter() method has been called multiple times by the thread, each
        call must be matched by a call to exit() before other threads will be allowed
        to take over the lock.

        @see enter, scopedlock
    */
    void exit() const noexcept;

    //==============================================================================
    /** provides the type of scoped lock to use with a criticalsection. */
    typedef genericscopedlock <criticalsection>       scopedlocktype;

    /** provides the type of scoped unlocker to use with a criticalsection. */
    typedef genericscopedunlock <criticalsection>     scopedunlocktype;

    /** provides the type of scoped try-locker to use with a criticalsection. */
    typedef genericscopedtrylock <criticalsection>    scopedtrylocktype;

    //--------------------------------------------------------------------------
    //
    // boost concept compatibility
    // http://www.boost.org/doc/libs/1_54_0/doc/html/thread/synchronization.html#thread.synchronization.mutex_concepts
    //

    // basiclockable
    inline void lock () const noexcept { enter (); }
    inline void unlock () const noexcept { exit (); }

    // lockable
    inline bool try_lock () const noexcept { return tryenter (); }

    //--------------------------------------------------------------------------

private:
    //==============================================================================
   #if beast_windows
    // to avoid including windows.h in the public beast headers, we'll just allocate
    // a block of memory here that's big enough to be used internally as a windows
    // critical_section structure.
    #if beast_64bit
     std::uint8_t section[44];
    #else
     std::uint8_t section[24];
    #endif
   #else
    mutable pthread_mutex_t mutex;
   #endif
};

//==============================================================================
/**
    a class that can be used in place of a real criticalsection object, but which
    doesn't perform any locking.

    this is currently used by some templated classes, and most compilers should
    manage to optimise it out of existence.

    @see criticalsection, array, sharedobjectarray
*/
class dummycriticalsection
{
public:
    dummycriticalsection() = default;
    dummycriticalsection (dummycriticalsection const&) = delete;
    dummycriticalsection& operator= (dummycriticalsection const&) = delete;
    ~dummycriticalsection() = default;

    inline void enter() const noexcept          {}
    inline bool tryenter() const noexcept       { return true; }
    inline void exit() const noexcept           {}

    //==============================================================================
    /** a dummy scoped-lock type to use with a dummy critical section. */
    struct scopedlocktype
    {
        scopedlocktype (const dummycriticalsection&) noexcept {}
    };

    /** a dummy scoped-unlocker type to use with a dummy critical section. */
    typedef scopedlocktype scopedunlocktype;
};

//==============================================================================
/**
    automatically locks and unlocks a criticalsection object.

    use one of these as a local variable to provide raii-based locking of a criticalsection.

    e.g. @code

    criticalsection mycriticalsection;

    for (;;)
    {
        const scopedlock myscopedlock (mycriticalsection);
        // mycriticalsection is now locked

        ...do some stuff...

        // mycriticalsection gets unlocked here.
    }
    @endcode

    @see criticalsection, scopedunlock
*/
typedef criticalsection::scopedlocktype  scopedlock;

//==============================================================================
/**
    automatically unlocks and re-locks a criticalsection object.

    this is the reverse of a scopedlock object - instead of locking the critical
    section for the lifetime of this object, it unlocks it.

    make sure you don't try to unlock critical sections that aren't actually locked!

    e.g. @code

    criticalsection mycriticalsection;

    for (;;)
    {
        const scopedlock myscopedlock (mycriticalsection);
        // mycriticalsection is now locked

        ... do some stuff with it locked ..

        while (xyz)
        {
            ... do some stuff with it locked ..

            const scopedunlock unlocker (mycriticalsection);

            // mycriticalsection is now unlocked for the remainder of this block,
            // and re-locked at the end.

            ...do some stuff with it unlocked ...
        }

        // mycriticalsection gets unlocked here.
    }
    @endcode

    @see criticalsection, scopedlock
*/
typedef criticalsection::scopedunlocktype  scopedunlock;

//==============================================================================
/**
    automatically tries to lock and unlock a criticalsection object.

    use one of these as a local variable to control access to a criticalsection.

    e.g. @code
    criticalsection mycriticalsection;

    for (;;)
    {
        const scopedtrylock myscopedtrylock (mycriticalsection);

        // unlike using a scopedlock, this may fail to actually get the lock, so you
        // should test this with the islocked() method before doing your thread-unsafe
        // action..
        if (myscopedtrylock.islocked())
        {
           ...do some stuff...
        }
        else
        {
            ..our attempt at locking failed because another thread had already locked it..
        }

        // mycriticalsection gets unlocked here (if it was locked)
    }
    @endcode

    @see criticalsection::tryenter, scopedlock, scopedunlock, scopedreadlock
*/
typedef criticalsection::scopedtrylocktype  scopedtrylock;

} // beast

#endif   // beast_criticalsection_h_included
