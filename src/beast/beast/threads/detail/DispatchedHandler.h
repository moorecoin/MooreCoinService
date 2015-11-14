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

#ifndef beast_threads_dispatchedhandler_h_included
#define beast_threads_dispatchedhandler_h_included

#include <beast/threads/detail/bindhandler.h>

namespace beast {
namespace detail {

/** a wrapper that packages function call arguments into a dispatch. */
template <typename dispatcher, typename handler>
class dispatchedhandler
{
private:
    dispatcher m_dispatcher;
    handler m_handler;

public:
    typedef void result_type;

    dispatchedhandler (dispatcher dispatcher, handler& handler)
        : m_dispatcher (dispatcher)
        , m_handler (beast_move_cast(handler)(handler))
    {
    }

#if beast_compiler_supports_move_semantics
    dispatchedhandler (dispatchedhandler const& other)
        : m_dispatcher (other.m_dispatcher)
        , m_handler (other.m_handler)
    {
    }

    dispatchedhandler (dispatchedhandler&& other)
        : m_dispatcher (other.m_dispatcher)
        , m_handler (beast_move_cast(handler)(other.m_handler))
    {
    }
#endif

    void operator()()
    {
        m_dispatcher.dispatch (m_handler);
    }

    void operator()() const
    {
        m_dispatcher.dispatch (m_handler);
    }

    template <class p1>
    void operator() (p1 const& p1)
    {
        m_dispatcher.dispatch (
            detail::bindhandler (m_handler,
                p1));
    }

    template <class p1>
    void operator() (p1 const& p1) const
    {
        m_dispatcher.dispatch (
            detail::bindhandler (m_handler,
                p1));
    }

    template <class p1, class p2>
    void operator() (p1 const& p1, p2 const& p2)
    {
        m_dispatcher.dispatch (
            detail::bindhandler (m_handler,
                p1, p2));
    }

    template <class p1, class p2>
    void operator() (p1 const& p1, p2 const& p2) const
    {
        m_dispatcher.dispatch (
            detail::bindhandler (m_handler,
                p1, p2));
    }

    template <class p1, class p2, class p3>
    void operator() (p1 const& p1, p2 const& p2, p3 const& p3)
    {
        m_dispatcher.dispatch (
            detail::bindhandler (m_handler,
                p1, p2, p3));
    }

    template <class p1, class p2, class p3>
    void operator() (p1 const& p1, p2 const& p2, p3 const& p3) const
    {
        m_dispatcher.dispatch (
            detail::bindhandler (m_handler,
                p1, p2, p3));
    }

    template <class p1, class p2, class p3, class p4>
    void operator() (p1 const& p1, p2 const& p2, p3 const& p3,  p4 const& p4)
    {
        m_dispatcher.dispatch (
            detail::bindhandler (m_handler,
                p1, p2, p3, p4));
    }

    template <class p1, class p2, class p3, class p4>
    void operator() (p1 const& p1, p2 const& p2, p3 const& p3,  p4 const& p4) const
    {
        m_dispatcher.dispatch (
            detail::bindhandler (m_handler,
                p1, p2, p3, p4));
    }

    template <class p1, class p2, class p3, class p4, class p5>
    void operator() (p1 const& p1, p2 const& p2, p3 const& p3, 
                     p4 const& p4, p5 const& p5)
    {
        m_dispatcher.dispatch (
            detail::bindhandler (m_handler,
                p1, p2, p3, p4, p5));
    }

    template <class p1, class p2, class p3, class p4, class p5>
    void operator() (p1 const& p1, p2 const& p2, p3 const& p3, 
                     p4 const& p4, p5 const& p5) const
    {
        m_dispatcher.dispatch (
            detail::bindhandler (m_handler,
                p1, p2, p3, p4, p5));
    }

    template <class p1, class p2, class p3, class p4, class p5, class p6>
    void operator() (p1 const& p1, p2 const& p2, p3 const& p3, 
                     p4 const& p4, p5 const& p5, p6 const& p6)
    {
        m_dispatcher.dispatch (
            detail::bindhandler (m_handler,
                p1, p2, p3, p4, p5, p6));
    }

    template <class p1, class p2, class p3, class p4, class p5, class p6>
    void operator() (p1 const& p1, p2 const& p2, p3 const& p3, 
                     p4 const& p4, p5 const& p5, p6 const& p6) const
    {
        m_dispatcher.dispatch (
            detail::bindhandler (m_handler,
                p1, p2, p3, p4, p5, p6));
    }
};

}
}

#endif
