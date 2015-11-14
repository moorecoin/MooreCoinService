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

#ifndef beast_scopedlock_h_included
#define beast_scopedlock_h_included

namespace beast
{

//==============================================================================
/**
    automatically locks and unlocks a mutex object.

    use one of these as a local variable to provide raii-based locking of a mutex.

    the templated class could be a criticalsection, spinlock, or anything else that
    provides enter() and exit() methods.

    e.g. @code
    criticalsection mycriticalsection;

    for (;;)
    {
        const genericscopedlock<criticalsection> myscopedlock (mycriticalsection);
        // mycriticalsection is now locked

        ...do some stuff...

        // mycriticalsection gets unlocked here.
    }
    @endcode

    @see genericscopedunlock, criticalsection, spinlock, scopedlock, scopedunlock
*/
template <class locktype>
class genericscopedlock
{
public:
    //==============================================================================
    /** creates a genericscopedlock.

        as soon as it is created, this will acquire the lock, and when the genericscopedlock
        object is deleted, the lock will be released.

        make sure this object is created and deleted by the same thread,
        otherwise there are no guarantees what will happen! best just to use it
        as a local stack object, rather than creating one with the new() operator.
    */
    inline explicit genericscopedlock (const locktype& lock) noexcept
        : lock_ (lock)
    {
        lock.enter();
    }

    genericscopedlock (genericscopedlock const&) = delete;
    genericscopedlock& operator= (genericscopedlock const&) = delete;

    /** destructor.
        the lock will be released when the destructor is called.
        make sure this object is created and deleted by the same thread, otherwise there are
        no guarantees what will happen!
    */
    inline ~genericscopedlock() noexcept
    {
        lock_.exit();
    }

private:
    //==============================================================================
    const locktype& lock_;
};

//==============================================================================
/**
    automatically unlocks and re-locks a mutex object.

    this is the reverse of a genericscopedlock object - instead of locking the mutex
    for the lifetime of this object, it unlocks it.

    make sure you don't try to unlock mutexes that aren't actually locked!

    e.g. @code

    criticalsection mycriticalsection;

    for (;;)
    {
        const genericscopedlock<criticalsection> myscopedlock (mycriticalsection);
        // mycriticalsection is now locked

        ... do some stuff with it locked ..

        while (xyz)
        {
            ... do some stuff with it locked ..

            const genericscopedunlock<criticalsection> unlocker (mycriticalsection);

            // mycriticalsection is now unlocked for the remainder of this block,
            // and re-locked at the end.

            ...do some stuff with it unlocked ...
        }

        // mycriticalsection gets unlocked here.
    }
    @endcode

    @see genericscopedlock, criticalsection, scopedlock, scopedunlock
*/
template <class locktype>
class genericscopedunlock
{
public:
    //==============================================================================
    /** creates a genericscopedunlock.

        as soon as it is created, this will unlock the criticalsection, and
        when the scopedlock object is deleted, the criticalsection will
        be re-locked.

        make sure this object is created and deleted by the same thread,
        otherwise there are no guarantees what will happen! best just to use it
        as a local stack object, rather than creating one with the new() operator.
    */
    inline explicit genericscopedunlock (locktype& lock) noexcept
        : lock_ (lock)
    { 
        lock.unlock();
    }

    genericscopedunlock (genericscopedunlock const&) = delete;
    genericscopedunlock& operator= (genericscopedunlock const&) = delete;

    /** destructor.

        the criticalsection will be unlocked when the destructor is called.

        make sure this object is created and deleted by the same thread,
        otherwise there are no guarantees what will happen!
    */
    inline ~genericscopedunlock() noexcept
    {
        lock_.lock();
    }


private:
    //==============================================================================
    locktype& lock_;
};

//==============================================================================
/**
    automatically locks and unlocks a mutex object.

    use one of these as a local variable to provide raii-based locking of a mutex.

    the templated class could be a criticalsection, spinlock, or anything else that
    provides enter() and exit() methods.

    e.g. @code

    criticalsection mycriticalsection;

    for (;;)
    {
        const genericscopedtrylock<criticalsection> myscopedtrylock (mycriticalsection);

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

    @see criticalsection::tryenter, genericscopedlock, genericscopedunlock
*/
template <class locktype>
class genericscopedtrylock
{
public:
    //==============================================================================
    /** creates a genericscopedtrylock.

        as soon as it is created, this will attempt to acquire the lock, and when the
        genericscopedtrylock is deleted, the lock will be released (if the lock was
        successfully acquired).

        make sure this object is created and deleted by the same thread,
        otherwise there are no guarantees what will happen! best just to use it
        as a local stack object, rather than creating one with the new() operator.
    */
    inline explicit genericscopedtrylock (const locktype& lock) noexcept
        : lock_ (lock), lockwassuccessful (lock.tryenter()) {}

    genericscopedtrylock (genericscopedtrylock const&) = delete;
    genericscopedtrylock& operator= (genericscopedtrylock const&) = delete;

    /** destructor.

        the mutex will be unlocked (if it had been successfully locked) when the
        destructor is called.

        make sure this object is created and deleted by the same thread,
        otherwise there are no guarantees what will happen!
    */
    inline ~genericscopedtrylock() noexcept
    {
        if (lockwassuccessful)
            lock_.exit();
    }

    /** returns true if the mutex was successfully locked. */
    bool islocked() const noexcept
    {
        return lockwassuccessful;
    }

private:
    //==============================================================================
    const locktype& lock_;
    const bool lockwassuccessful;
};

} // beast

#endif

