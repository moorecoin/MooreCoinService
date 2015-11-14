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

#ifndef beast_threads_spinlock_h_included
#define beast_threads_spinlock_h_included

#include <beast/threads/unlockguard.h>
#include <beast/utility/noexcept.h>
#include <atomic>
#include <cassert>
#include <mutex>
#include <thread>

namespace beast {

//==============================================================================
/**
    a simple spin-lock class that can be used as a simple, low-overhead mutex for
    uncontended situations.

    note that unlike a criticalsection, this type of lock is not re-entrant, and may
    be less efficient when used it a highly contended situation, but it's very small and
    requires almost no initialisation.
    it's most appropriate for simple situations where you're only going to hold the
    lock for a very brief time.

    @see criticalsection
*/
class spinlock
{
public:
    /** provides the type of scoped lock to use for locking a spinlock. */
    typedef std::lock_guard <spinlock> scopedlocktype;

    /** provides the type of scoped unlocker to use with a spinlock. */
    typedef unlockguard <spinlock>     scopedunlocktype;

    spinlock()
        : m_lock (0)
    {
    }

    ~spinlock() = default;

    spinlock (spinlock const&) = delete;
    spinlock& operator= (spinlock const&) = delete;

    /** attempts to acquire the lock, returning true if this was successful. */
    inline bool tryenter() const noexcept
    {
        return (m_lock.exchange (1) == 0);
    }

    /** acquires the lock.
        this will block until the lock has been successfully acquired by this thread.
        note that a spinlock is not re-entrant, and is not smart enough to know whether the
        caller thread already has the lock - so if a thread tries to acquire a lock that it
        already holds, this method will never return!

        it's strongly recommended that you never call this method directly - instead use the
        scopedlocktype class to manage the locking using an raii pattern instead.
    */
    void enter() const noexcept
    {
        if (! tryenter())
        {
            for (int i = 20; --i >= 0;)
            {
                if (tryenter())
                    return;
            }

            while (! tryenter())
                std::this_thread::yield ();
        }
    }

    /** releases the lock. */
    inline void exit() const noexcept
    {
        // agh! releasing a lock that isn't currently held!
        assert (m_lock.load () == 1);
        m_lock.store (0);
    }

    void lock () const
        { enter(); }
    void unlock () const
        { exit(); }
    bool try_lock () const
        { return tryenter(); }

private:
    mutable std::atomic<int> m_lock;
};

}

#endif

