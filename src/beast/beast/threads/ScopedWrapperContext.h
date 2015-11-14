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

#ifndef beast_threads_wrapscoped_h_included
#define beast_threads_wrapscoped_h_included

namespace beast {

/** wraps a function object so invocation happens during a scoped container lifetime. */
/** @{ */
namespace detail {

template <typename scopedtype, typename context, typename handler>
class scopedwrapper
{
public:
    scopedwrapper (context& context, handler const& handler)
        : m_context (context)
        , m_handler (handler)
    {
    }

    void operator() ()
    {
        scopedtype scope (m_context);
        m_handler();
    }

private:
    context& m_context;
    handler m_handler;
};

}

//------------------------------------------------------------------------------

/** helper to eliminate the template argument at call sites. */
template <typename context, typename scopedtype>
class scopedwrappercontext
{
public:
    typedef context context_type;
    typedef scopedtype scoped_type;

    class scope
    {
    public:
        explicit scope (scopedwrappercontext const& owner)
            : m_scope (owner.m_context)
        {
        }

    private:
        scoped_type mutable m_scope;
    };

    scopedwrappercontext ()
        { }

    template <typename arg>
    explicit scopedwrappercontext (arg& arg)
        : m_context (arg)
    {
    }

    template <typename handler>
    detail::scopedwrapper <scopedtype, context, handler> wrap (
        handler const& handler)
    {
        return detail::scopedwrapper <scopedtype, context, handler> (
            m_context, handler);
    }

private:
    context mutable m_context;
};

}

#endif
