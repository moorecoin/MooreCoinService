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

#ifndef ripple_basics_decayingsample_h_included
#define ripple_basics_decayingsample_h_included

#include <chrono>
    
namespace ripple {

/** sampling function using exponential decay to provide a continuous value.
    @tparam the number of seconds in the decay window.
*/
template <int window, typename clock>
class decayingsample
{
public:
    typedef typename clock::duration::rep value_type;
    typedef typename clock::time_point time_point;

    decayingsample () = delete;

    /**
        @param now start time of decayingsample.
    */
    explicit decayingsample (time_point now)
        : m_value (value_type())
        , m_when (now)
    {
    }

    /** add a new sample.
        the value is first aged according to the specified time.
    */
    value_type add (value_type value, time_point now)
    {
        decay (now);
        m_value += value;
        return m_value / window;
    }

    /** retrieve the current value in normalized units.
        the samples are first aged according to the specified time.
    */
    value_type value (time_point now)
    {
        decay (now);
        return m_value / window;
    }

private:
    // apply exponential decay based on the specified time.
    void decay (time_point now)
    {
        if (now == m_when)
            return;

        if (m_value != value_type())
        {
            std::size_t elapsed = std::chrono::duration_cast<
                std::chrono::seconds>(now - m_when).count();

            // a span larger than four times the window decays the
            // value to an insignificant amount so just reset it.
            //
            if (elapsed > 4 * window)
            {
                m_value = value_type();
            }
            else
            {
                while (elapsed--)
                    m_value -= (m_value + window - 1) / window;
            }
        }

        m_when = now;
    }

    // current value in exponential units
    value_type m_value;

    // last time the aging function was applied
    time_point m_when;
};

}

#endif
