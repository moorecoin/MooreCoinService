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

#ifndef beast_threads_semaphore_h_included
#define beast_threads_semaphore_h_included

#include <condition_variable>
#include <mutex>

namespace beast {

template <class mutex, class condvar>
class basic_semaphore
{
private:
    typedef std::unique_lock <mutex> scoped_lock;

    mutex m_mutex;
    condvar m_cond;
    std::size_t m_count;

public:
    typedef std::size_t size_type;

    /** create the semaphore, with an optional initial count.
        if unspecified, the initial count is zero.
    */
    explicit basic_semaphore (size_type count = 0)
        : m_count (count)
    {
    }

    /** increment the count and unblock one waiting thread. */
    void notify ()
    {
        scoped_lock lock (m_mutex);
        ++m_count;
        m_cond.notify_one ();
    }

    // deprecated, for backward compatibility
    void signal () { notify (); }

    /** block until notify is called. */
    void wait ()
    {
        scoped_lock lock (m_mutex);
        while (m_count == 0)
            m_cond.wait (lock);
        --m_count;
    }

    /** perform a non-blocking wait.
        @return `true` if the wait would be satisfied.
    */
    bool try_wait ()
    {
        scoped_lock lock (m_mutex);
        if (m_count == 0)
            return false;
        --m_count;
        return true;
    }
};

typedef basic_semaphore <std::mutex, std::condition_variable> semaphore;
}

#endif

