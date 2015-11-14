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

#ifndef beast_chrono_util_h_included
#define beast_chrono_util_h_included

// from howard hinnant
// http://home.roadrunner.com/~hinnant/duration_io/chrono_util.html

// round down
template <class to, class rep, class period>
to floor(std::chrono::duration <rep, period> const& d)
{
    to t = std::chrono::duration_cast<to>(d);
    if (t > d)
        --t;
    return t;
}

// round to nearest, to even on tie
template <class to, class rep, class period>
to round (std::chrono::duration <rep, period> const& d)
{
    to t0 = std::chrono::duration_cast<to>(d);
    to t1 = t0;
    ++t1;
    auto diff0 = d - t0;
    auto diff1 = t1 - d;
    if (diff0 == diff1)
    {
        if (t0.count() & 1)
            return t1;
        return t0;
    }
    else if (diff0 < diff1)
        return t0;
    return t1;
}

// round up
template <class to, class rep, class period>
to ceil (std::chrono::duration <rep, period> const& d)
{
    to t = std::chrono::duration_cast<to>(d);
    if (t < d)
        ++t;
    return t;
}

#endif
