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

#ifndef beast_chrono_manual_clock_h_included
#define beast_chrono_manual_clock_h_included

#include <beast/chrono/abstract_clock.h>

namespace beast {

/** manual clock implementation.

    this concrete class implements the @ref abstract_clock interface and
    allows the time to be advanced manually, mainly for the purpose of
    providing a clock in unit tests.

    @tparam clock a type meeting these requirements:
        http://en.cppreference.com/w/cpp/concept/clock
*/
template <class clock>
class manual_clock
    : public abstract_clock<clock>
{
public:
    using typename abstract_clock<clock>::rep;
    using typename abstract_clock<clock>::duration;
    using typename abstract_clock<clock>::time_point;

private:
    time_point now_;

public:
    explicit
    manual_clock (time_point const& now = time_point(duration(0)))
        : now_(now)
    {
    }

    time_point
    now() const override
    {
        return now_;
    }

    /** set the current time of the manual clock. */
    void
    set (time_point const& when)
    {
        assert(! clock::is_steady || when >= now_);
        now_ = when;
    }

    /** convenience for setting the time in seconds from epoch. */
    template <class integer>
    void
    set(integer seconds_from_epoch)
    {
        set(time_point(duration(
            std::chrono::seconds(seconds_from_epoch))));
    }

    /** advance the clock by a duration. */
    template <class rep, class period>
    void
    advance(std::chrono::duration<rep, period> const& elapsed)
    {
        assert(! clock::is_steady ||
            (now_ + elapsed) >= now_);
        now_ += elapsed;
    }

    /** convenience for advancing the clock by one second. */
    manual_clock&
    operator++ ()
    {
        advance(std::chrono::seconds(1));
        return *this;
    }
};

}

#endif
