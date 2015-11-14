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

#ifndef beast_asio_bind_handler_h_included
#define beast_asio_bind_handler_h_included

#include <boost/asio/detail/handler_alloc_helpers.hpp>
#include <boost/asio/detail/handler_cont_helpers.hpp>
#include <boost/asio/detail/handler_invoke_helpers.hpp>

#include <functional>
#include <beast/cxx14/type_traits.h> // <type_traits>
#include <beast/cxx14/utility.h> // <utility>

namespace beast {
namespace asio {

namespace detail {

/** nullary handler that calls handler with bound arguments.
    the rebound handler provides the same io_service execution
    guarantees as the original handler.
*/
template <class deducedhandler, class... args>
class bound_handler
{
private:
    typedef std::tuple <std::decay_t <args>...> args_type;

    std::decay_t <deducedhandler> m_handler;
    args_type m_args;

    template <class handler, class tuple, std::size_t... s>
    static void invoke (handler& h, tuple& args,
        std::index_sequence <s...>)
    {
        h (std::get <s> (args)...);
    }

public:
    typedef void result_type;

    explicit
    bound_handler (deducedhandler&& handler, args&&... args)
        : m_handler (std::forward <deducedhandler> (handler))
        , m_args (std::forward <args> (args)...)
    {
    }

    void
    operator() ()
    {
        invoke (m_handler, m_args,
            std::index_sequence_for <args...> ());
    }

    void
    operator() () const
    {
        invoke (m_handler, m_args,
            std::index_sequence_for <args...> ());
    }

    template <class function>
    friend
    void
    asio_handler_invoke (function& f, bound_handler* h)
    {
        boost_asio_handler_invoke_helpers::
            invoke (f, h->m_handler);
    }

    template <class function>
    friend
    void
    asio_handler_invoke (function const& f, bound_handler* h)
    {
        boost_asio_handler_invoke_helpers::
            invoke (f, h->m_handler);
    }

    friend
    void*
    asio_handler_allocate (std::size_t size, bound_handler* h)
    {
        return boost_asio_handler_alloc_helpers::
            allocate (size, h->m_handler);
    }

    friend
    void
    asio_handler_deallocate (void* p, std::size_t size, bound_handler* h)
    {
        boost_asio_handler_alloc_helpers::
            deallocate (p, size, h->m_handler);
    }

    friend
    bool
    asio_handler_is_continuation (bound_handler* h)
    {
        return boost_asio_handler_cont_helpers::
            is_continuation (h->m_handler);
    }
};

}

//------------------------------------------------------------------------------

/** binds parameters to a handler to produce a nullary functor.
    the returned handler provides the same io_service execution guarantees
    as the original handler. this is designed to use as a replacement for
    io_service::wrap, to ensure that the handler will not be invoked
    immediately by the calling function.
*/
template <class deducedhandler, class... args>
detail::bound_handler <deducedhandler, args...>
bind_handler (deducedhandler&& handler, args&&... args)
{
    return detail::bound_handler <deducedhandler, args...> (
        std::forward <deducedhandler> (handler),
            std::forward <args> (args)...);
}

}
}

//------------------------------------------------------------------------------

namespace std {

template <class handler, class... args>
void bind (beast::asio::detail::bound_handler <
    handler, args...>, ...)  = delete;

#if 0
template <class handler, class... args>
struct is_bind_expression <
    beast::asio::detail::bound_handler <handler, args...>
> : std::true_type
{
};
#endif

}

#endif
