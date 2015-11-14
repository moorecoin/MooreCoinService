//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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

#ifndef ripple_peerfinder_fixed_h_included
#define ripple_peerfinder_fixed_h_included

#include <ripple/peerfinder/impl/tuning.h>

namespace ripple {
namespace peerfinder {

/** metadata for a fixed slot. */
class fixed
{
public:
    explicit fixed (clock_type& clock)
        : m_when (clock.now ())
        , m_failures (0)
    {
    }

    fixed (fixed const&) = default;

    /** returns the time after which we shoud allow a connection attempt. */
    clock_type::time_point const& when () const
    {
        return m_when;
    }

    /** updates metadata to reflect a failed connection. */
    void failure (clock_type::time_point const& now)
    {
        m_failures = std::min (m_failures + 1,
            tuning::connectionbackoff.size() - 1);
        m_when = now + std::chrono::minutes (
            tuning::connectionbackoff [m_failures]);
    }

    /** updates metadata to reflect a successful connection. */
    void success (clock_type::time_point const& now)
    {
        m_failures = 0;
        m_when = now;
    }

private:
    clock_type::time_point m_when;
    std::size_t m_failures;
};

}
}

#endif
