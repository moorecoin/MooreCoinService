//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    portions of this file are from juce.
    copyright (c) 2013 - raw material software ltd.
    please visit http://www.juce.com

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

#ifndef beast_arithmetic_h_included
#define beast_arithmetic_h_included

#include <beast/config.h>

#include <beast/utility/noexcept.h>

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace beast {

//==============================================================================
/** constrains a value to keep it within a given range.

    this will check that the specified value lies between the lower and upper bounds
    specified, and if not, will return the nearest value that would be in-range. effectively,
    it's like calling bmax (lowerlimit, bmin (upperlimit, value)).

    note that it expects that lowerlimit <= upperlimit. if this isn't true,
    the results will be unpredictable.

    @param lowerlimit           the minimum value to return
    @param upperlimit           the maximum value to return
    @param valuetoconstrain     the value to try to return
    @returns    the closest value to valuetoconstrain which lies between lowerlimit
                and upperlimit (inclusive)
    @see blimit0to, bmin, bmax
*/
template <typename type>
inline type blimit (const type lowerlimit,
                    const type upperlimit,
                    const type valuetoconstrain) noexcept
{
    // if these are in the wrong order, results are unpredictable.
    bassert (lowerlimit <= upperlimit);

    return (valuetoconstrain < lowerlimit) ? lowerlimit
                                           : ((upperlimit < valuetoconstrain) ? upperlimit
                                                                              : valuetoconstrain);
}

/** returns true if a value is at least zero, and also below a specified upper limit.
    this is basically a quicker way to write:
    @code valuetotest >= 0 && valuetotest < upperlimit
    @endcode
*/
template <typename type>
inline bool ispositiveandbelow (type valuetotest, type upperlimit) noexcept
{
    bassert (type() <= upperlimit); // makes no sense to call this if the upper limit is itself below zero..
    return type() <= valuetotest && valuetotest < upperlimit;
}

template <>
inline bool ispositiveandbelow (const int valuetotest, const int upperlimit) noexcept
{
    bassert (upperlimit >= 0); // makes no sense to call this if the upper limit is itself below zero..
    return static_cast <unsigned int> (valuetotest) < static_cast <unsigned int> (upperlimit);
}


//==============================================================================

/** handy function for getting the number of elements in a simple const c array.
    e.g.
    @code
    static int myarray[] = { 1, 2, 3 };

    int numelements = numelementsinarray (myarray) // returns 3
    @endcode
*/
template <typename type, int n>
int numelementsinarray (type (&array)[n])
{
    (void) array; // (required to avoid a spurious warning in ms compilers)
    (void) sizeof (0[array]); // this line should cause an error if you pass an object with a user-defined subscript operator
    return n;
}

}

#endif

