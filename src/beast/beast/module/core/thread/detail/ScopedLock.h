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

#ifndef beast_core_thread_detail_scopedlock_h_included
#define beast_core_thread_detail_scopedlock_h_included

#include <beast/module/beast_core/thread/mutextraits.h>

namespace beast
{

namespace detail
{

template <typename mutex>
class trackedscopedlock
{
public:
    inline explicit trackedscopedlock (mutex const& mutex,
        char const* filename, int linenumber) noexcept
        : m_mutex (mutex)
        , m_lock_count (0)
    {
        lock (filename, linenumber);
    }

    trackedscopedlock (trackedscopedlock const&) = delete;
    trackedscopedlock& operator= (trackedscopedlock const&) = delete;

    inline ~trackedscopedlock () noexcept
    {
        if (m_lock_count > 0)
            unlock ();
    }    

    inline void lock (char const* filename, int linenumber) noexcept
    {
        ++m_lock_count;
        m_mutex.lock (filename, linenumber);
    }

    inline void unlock () noexcept
    {
        m_mutex.unlock ();
        --m_lock_count;
    }

private:
    mutex const& m_mutex;
    int m_lock_count;
};

//--------------------------------------------------------------------------

template <typename mutex>
class trackedscopedtrylock
{
public:
    inline explicit trackedscopedtrylock (mutex const& mutex,
        char const* filename, int linenumber) noexcept
        : m_mutex (mutex)
        , m_lock_count (0)
    {
        try_lock (filename, linenumber);
    }

    trackedscopedtrylock (trackedscopedtrylock const&) = delete;
    trackedscopedtrylock& operator= (trackedscopedtrylock const&) = delete;

    inline ~trackedscopedtrylock () noexcept
    {
        if (m_lock_count > 0)
            unlock ();
    }

    inline bool owns_lock () const noexcept
    {
        return m_lock_count > 0;
    }

    inline bool try_lock (char const* filename, int linenumber) noexcept
    {
        bool const success = m_mutex.try_lock (filename, linenumber);
        if (success)
            ++m_lock_count;
        return success;
    }

    inline void unlock () noexcept
    {
        m_mutex.unlock ();
        --m_lock_count;
    }

private:
    mutex const& m_mutex;
    int m_lock_count;
};

//--------------------------------------------------------------------------

template <typename mutex>
class trackedscopedunlock
{
public:
    inline explicit trackedscopedunlock (mutex const& mutex,
        char const* filename, int linenumber) noexcept
        : m_mutex (mutex)
        , m_filename (filename)
        , m_linenumber (linenumber)
    {
        m_mutex.unlock ();
    }

    trackedscopedunlock (trackedscopedunlock const&) = delete;
    trackedscopedunlock& operator= (trackedscopedunlock const&) = delete;

    inline ~trackedscopedunlock () noexcept
    {
        m_mutex.lock (m_filename, m_linenumber);
    }

private:
    mutex const& m_mutex;
    char const* const m_filename;
    int const m_linenumber;
};

//--------------------------------------------------------------------------

template <typename mutex>
class untrackedscopedlock
{
public:
    inline explicit untrackedscopedlock (mutex const& mutex,
        char const*, int) noexcept
        : m_mutex (mutex)
        , m_lock_count (0)
    {
        lock ();
    }

    untrackedscopedlock (untrackedscopedlock const&) = delete;
    untrackedscopedlock& operator= (untrackedscopedlock const&) = delete;

    inline ~untrackedscopedlock () noexcept
    {
        if (m_lock_count > 0)
            unlock ();
    }

    inline void lock () noexcept
    {
        ++m_lock_count;
        m_mutex.lock ();
    }

    inline void lock (char const*, int) noexcept
    {
        lock ();
    }

    inline void unlock () noexcept
    {
        m_mutex.unlock ();
        --m_lock_count;
    }

private:
    mutex const& m_mutex;
    int m_lock_count;
};

//--------------------------------------------------------------------------

template <typename mutex>
class untrackedscopedtrylock
{
public:
    inline explicit untrackedscopedtrylock (mutex const& mutex,
        char const*, int) noexcept
        : m_mutex (mutex)
        , m_lock_count (0)
    {
        try_lock ();
    }

    untrackedscopedtrylock (untrackedscopedtrylock const&) = delete;
    untrackedscopedtrylock& operator= (untrackedscopedtrylock const&) = delete;

    inline ~untrackedscopedtrylock () noexcept
    {
        if (m_lock_count > 0)
        unlock ();
    }

    inline bool owns_lock () const noexcept
    {
        return m_lock_count > 0;
    }

    inline bool try_lock () noexcept
    {
        bool const success = m_mutex.try_lock ();
        if (success)
            ++m_lock_count;
        return success;
    }

    inline bool try_lock (char const*, int) noexcept
    {
        return try_lock ();
    }

    inline void unlock () noexcept
    {
        m_mutex.unlock ();
        --m_lock_count;
    }
  
private:
    mutex const& m_mutex;
    int m_lock_count;
};

//--------------------------------------------------------------------------

template <typename mutex>
class untrackedscopedunlock
{
public:
    untrackedscopedunlock (mutex const& mutex,
        char const*, int) noexcept
        : m_mutex (mutex)
        , m_owns_lock (true)
    {
        mutextraits <mutex>::unlock (m_mutex);
        m_owns_lock = false;
    }

    untrackedscopedunlock (untrackedscopedunlock const&) = delete;
    untrackedscopedunlock& operator= (untrackedscopedunlock const&) = delete;

    ~untrackedscopedunlock () noexcept
    {
        mutextraits <mutex>::lock (m_mutex);
        m_owns_lock = true;
    }

private:
    mutex const& m_mutex;
    bool m_owns_lock;
};

} // detail

} // beast

#endif
