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

#ifndef beast_chrono_abstract_clock_h_included
#define beast_chrono_abstract_clock_h_included

#include <chrono>
#include <string>

namespace beast {

/** abstract interface to a clock.

    this makes now() a member function instead of a static member, so
    an instance of the class can be dependency injected, facilitating
    unit tests where time may be controlled.

    an abstract_clock inherits all the nested types of the clock
    template parameter.

    example:

    @code

    struct implementation
    {
        using clock_type = abstract_clock <std::chrono::steady_clock>;
        clock_type& clock_;
        explicit implementation (clock_type& clock)
            : clock_(clock)
        {
        }
    };

    @endcode

    @tparam clock a type meeting these requirements:
        http://en.cppreference.com/w/cpp/concept/clock
*/
template <class clock>
class abstract_clock
{
public:
    using rep = typename clock::rep;
    using period = typename clock::period;
    using duration = typename clock::duration;
    using time_point = typename clock::time_point;

    static bool const is_steady = clock::is_steady;

    virtual ~abstract_clock() = default;

    /** returns the current time. */
    virtual time_point now() const = 0;

    /** returning elapsed ticks since the epoch. */
    rep elapsed()
    {
        return now().time_since_epoch().count();
    }
};

//------------------------------------------------------------------------------

namespace detail {

template <class facade, class clock>
struct abstract_clock_wrapper
    : public abstract_clock<facade>
{
    using typename abstract_clock<facade>::duration;
    using typename abstract_clock<facade>::time_point;

    time_point
    now() const override
    {
        return clock::now();
    }
};

}

//------------------------------------------------------------------------------

/** returns a global instance of an abstract clock.
    @tparam facade a type meeting these requirements:
        http://en.cppreference.com/w/cpp/concept/clock
    @tparam clock the actual concrete clock to use.
*/
template<class facade, class clock = facade>
abstract_clock<facade>&
get_abstract_clock()
{
    static detail::abstract_clock_wrapper<facade, clock> clock;
    return clock;
}

}

#endif
