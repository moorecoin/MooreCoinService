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

#ifndef beast_threads_sharedmutexadapter_h_included
#define beast_threads_sharedmutexadapter_h_included

#include <beast/threads/sharedlockguard.h>

#include <mutex>

namespace beast {
   
/** adapts a regular lockable to conform to the sharedmutex concept.
    shared locks become unique locks with this interface. two threads may not
    simultaneously acquire ownership of the lock. typically the mutex template
    parameter will be a criticalsection.
*/
template <class mutex>
class sharedmutexadapter
{
public:
    typedef mutex mutextype;
    typedef std::lock_guard <sharedmutexadapter> lockguardtype;
    typedef sharedlockguard <sharedmutexadapter> sharedlockguardtype;
    
    void lock() const
    {
        m_mutex.lock();
    }

    void unlock() const
    {
        m_mutex.unlock();
    }

    void lock_shared() const
    {
        m_mutex.lock();
    }

    void unlock_shared() const
    {
        m_mutex.unlock();
    }

private:
    mutex mutable m_mutex;
};

}

#endif
