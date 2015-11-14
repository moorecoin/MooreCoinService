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

#ifndef beast_core_thread_mutextraits_h_included
#define beast_core_thread_mutextraits_h_included

namespace beast
{

/** adapt a mutex type to meet the boost::mutex concept requirements.
    the default implementation assumes adherance to the boost::mutex concepts,
    with one important exception. we make the member functions const, for
    convenience.
*/
template <typename mutex>
struct mutextraits
{
    // basiclockable
    // http://www.boost.org/doc/libs/1_54_0/doc/html/thread/synchronization.html#thread.synchronization.mutex_concepts.basic_lockable
    //
    static inline void lock (mutex const& mutex) noexcept
    {
        (const_cast <mutex&> (mutex)).lock ();
    }

    static inline void unlock (mutex const& mutex) noexcept
    {
        (const_cast <mutex&> (mutex)).unlock ();
    }

    // lockable
    // http://www.boost.org/doc/libs/1_54_0/doc/html/thread/synchronization.html#thread.synchronization.mutex_concepts.basic_lockable
    //
    static inline bool try_lock (mutex const& mutex) noexcept
    {
        return (const_cast <mutex&> (mutex)).try_lock ();
    }
};

//------------------------------------------------------------------------------

/** mutextraits specialization for a beast criticalsection. */
template <>
struct mutextraits <criticalsection>
{
    static inline void lock (criticalsection const& mutex) noexcept
    {
        mutex.lock ();
    }

    static inline void unlock (criticalsection const& mutex) noexcept
    {
        mutex.unlock ();
    }

    static inline bool try_lock (criticalsection const& mutex) noexcept
    {
        return mutex.try_lock ();
    }
};

} // beast

#endif
