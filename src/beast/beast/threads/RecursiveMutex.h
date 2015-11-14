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

#ifndef beast_threads_recursivemutex_h_included
#define beast_threads_recursivemutex_h_included

#include <beast/config.h>
#include <beast/threads/unlockguard.h>
#include <beast/threads/trylockguard.h>

#include <mutex>

#if ! beast_windows
#include <pthread.h>
#endif

namespace beast {

class recursivemutex
{
public:
    typedef std::lock_guard <recursivemutex>      scopedlocktype;
    typedef unlockguard <recursivemutex>    scopedunlocktype;
    typedef trylockguard <recursivemutex>   scopedtrylocktype;

    /** create the mutex.
        the mutux is initially unowned.
    */
    recursivemutex ();

    /** destroy the mutex.
        if the lock is owned, the result is undefined.
    */
    ~recursivemutex ();

    // boost concept compatibility:
    // http://www.boost.org/doc/libs/1_54_0/doc/html/thread/synchronization.html#thread.synchronization.mutex_concepts

    /** basiclockable */
    /** @{ */
    void lock () const;
    void unlock () const;
    /** @} */

    /** lockable */
    bool try_lock () const;

private:
// to avoid including windows.h in the public beast headers, we'll just
// reserve storage here that's big enough to be used internally as a windows
// critical_section structure.
#if beast_windows
# if beast_64bit
    char section[44];
# else
    char section[24];
# endif
#else
    mutable pthread_mutex_t mutex;
#endif
};

}

#endif
