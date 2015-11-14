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

#ifndef beast_insight_meter_h_included
#define beast_insight_meter_h_included

#include <memory>

#include <beast/insight/base.h>
#include <beast/insight/meterimpl.h>

namespace beast {
namespace insight {

/** a metric for measuring an integral value.
    
    a meter may be thought of as an increment-only counter.

    this is a lightweight reference wrapper which is cheap to copy and assign.
    when the last reference goes away, the metric is no longer collected.
*/
class meter : public base
{
public:
    typedef meterimpl::value_type value_type;

    /** create a null metric.
        a null metric reports no information.
    */
    meter ()
        { }

    /** create the metric reference the specified implementation.
        normally this won't be called directly. instead, call the appropriate
        factory function in the collector interface.
        @see collector.
    */
    explicit meter (std::shared_ptr <meterimpl> const& impl)
        : m_impl (impl)
        { }

    /** increment the meter. */
    /** @{ */
    void increment (value_type amount) const
    {
        if (m_impl)
            m_impl->increment (amount);
    }

    meter const& operator+= (value_type amount) const
    {
        increment (amount);
        return *this;
    }

    meter const& operator++ () const
    {
        increment (1);
        return *this;
    }

    meter const& operator++ (int) const
    {
        increment (1);
        return *this;
    }
    /** @} */

    std::shared_ptr <meterimpl> const& impl () const
    {
        return m_impl;
    }

private:
    std::shared_ptr <meterimpl> m_impl;
};

}
}

#endif
