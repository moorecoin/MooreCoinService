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

#ifndef beast_threads_trylockguard_h_included
#define beast_threads_trylockguard_h_included

namespace beast {

template <typename mutex>
class trylockguard
{
public:
    typedef mutex mutextype;

    explicit trylockguard (mutex const& mutex)
        : m_mutex (mutex)
        , m_owns_lock (m_mutex.try_lock())
    {
    }

    ~trylockguard ()
    {
        if (m_owns_lock)
            m_mutex.unlock();
    }

    trylockguard (trylockguard const&) = delete;
    trylockguard& operator= (trylockguard const&) = delete;

    bool owns_lock() const
        { return m_owns_lock; }

private:
    mutex const& m_mutex;
    bool m_owns_lock;
};

}

#endif
