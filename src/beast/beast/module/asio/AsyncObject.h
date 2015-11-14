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

#ifndef beast_asio_asyncobject_h_included
#define beast_asio_asyncobject_h_included

#include <atomic>
#include <cassert>

namespace beast {
namespace asio {

/** mix-in to track when all pending i/o is complete.
    derived classes must be callable with this signature:
        void asynchandlerscomplete()
*/
template <class derived>
class asyncobject
{
protected:
    asyncobject ()
        : m_pending (0)
    { }

public:
    ~asyncobject ()
    {
        // destroying the object with i/o pending? not a clean exit!
        assert (m_pending.load () == 0);
    }

    /** raii container that maintains the count of pending i/o.
        bind this into the argument list of every handler passed
        to an initiating function.
    */
    class completioncounter
    {
    public:
        explicit completioncounter (derived* owner)
            : m_owner (owner)
        {
            ++m_owner->m_pending;
        }

        completioncounter (completioncounter const& other)
            : m_owner (other.m_owner)
        {
            ++m_owner->m_pending;
        }

        ~completioncounter ()
        {
            if (--m_owner->m_pending == 0)
                m_owner->asynchandlerscomplete ();
        }

        completioncounter& operator= (completioncounter const&) = delete;

    private:
        derived* m_owner;
    };

    void addreference ()
    {
        ++m_pending;
    }

    void removereference ()
    {
        if (--m_pending == 0)
            (static_cast<derived*>(this))->asynchandlerscomplete();
    }

private:
    // the number of handlers pending.
    std::atomic <int> m_pending;
};

}
}

#endif
