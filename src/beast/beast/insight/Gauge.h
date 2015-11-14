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

#ifndef beast_insight_gauge_h_included
#define beast_insight_gauge_h_included

#include <beast/insight/base.h>
#include <beast/insight/gaugeimpl.h>

#include <memory>

namespace beast {
namespace insight {

/** a metric for measuring an integral value.
    
    a gauge is an instantaneous measurement of a value, like the gas gauge
    in a car. the caller directly sets the value, or adjusts it by a
    specified amount. the value is kept in the client rather than the collector.

    this is a lightweight reference wrapper which is cheap to copy and assign.
    when the last reference goes away, the metric is no longer collected.
*/
class gauge : public base
{
public:
    typedef gaugeimpl::value_type value_type;
    typedef gaugeimpl::difference_type difference_type;

    /** create a null metric.
        a null metric reports no information.
    */
    gauge ()
    {
    }

    /** create the metric reference the specified implementation.
        normally this won't be called directly. instead, call the appropriate
        factory function in the collector interface.
        @see collector.
    */
    explicit gauge (std::shared_ptr <gaugeimpl> const& impl)
        : m_impl (impl)
    {
    }

    /** set the value on the gauge.
        a collector implementation should combine multiple calls to value
        changes into a single change if the calls occur within a single
        collection interval.
    */
    /** @{ */
    void set (value_type value) const
    {
        if (m_impl)
            m_impl->set (value);
    }

    gauge const& operator= (value_type value) const
        { set (value); return *this; }
    /** @} */

    /** adjust the value of the gauge. */
    /** @{ */
    void increment (difference_type amount) const
    {
        if (m_impl)
            m_impl->increment (amount);
    }

    gauge const&
    operator+= (difference_type amount) const
    {
        increment (amount);
        return *this;
    }

    gauge const&
    operator-= (difference_type amount) const
    {
        increment (-amount);
        return *this;
    }

    gauge const&
    operator++ () const
    {
        increment (1);
        return *this;
    }

    gauge const&
    operator++ (int) const
    {
        increment (1);
        return *this;
    }

    gauge const&
    operator-- () const
    {
        increment (-1);
        return *this;
    }

    gauge const&
    operator-- (int) const
    {
        increment (-1);
        return *this;
    }
    /** @} */

    std::shared_ptr <gaugeimpl> const&
    impl () const
    {
        return m_impl;
    }

private:
    std::shared_ptr <gaugeimpl> m_impl;
};

}
}

#endif
