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

#ifndef beast_insight_event_h_included
#define beast_insight_event_h_included

#include <beast/insight/base.h>
#include <beast/insight/eventimpl.h>

#include <beast/chrono/chrono_util.h>

#include <chrono>
#include <memory>

namespace beast {
namespace insight {

/** a metric for reporting event timing.
    
    an event is an operation that has an associated millisecond time, or
    other integral value. because events happen at a specific moment, the
    metric only supports a push-style interface.

    this is a lightweight reference wrapper which is cheap to copy and assign.
    when the last reference goes away, the metric is no longer collected.
*/
class event : public base
{
public:
    typedef eventimpl::value_type value_type;

    /** create a null metric.
        a null metric reports no information.
    */
    event ()
        { }

    /** create the metric reference the specified implementation.
        normally this won't be called directly. instead, call the appropriate
        factory function in the collector interface.
        @see collector.
    */
    explicit event (std::shared_ptr <eventimpl> const& impl)
        : m_impl (impl)
        { }

    /** push an event notification. */
    template <class rep, class period>
    void
    notify (std::chrono::duration <rep, period> const& value) const
    {
        if (m_impl)
            m_impl->notify (ceil <value_type> (value));
    }

    std::shared_ptr <eventimpl> const& impl () const
    {
        return m_impl;
    }

private:
    std::shared_ptr <eventimpl> m_impl;
};

}
}

#endif
