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

#ifndef beast_core_diagnostic_measurefunctioncalltime_h_included
#define beast_core_diagnostic_measurefunctioncalltime_h_included

namespace beast
{

/** measures the speed of invoking a function. */
/** @{ */
template <typename function>
double measurefunctioncalltime (function f)
{
    std::int64_t const starttime (time::gethighresolutionticks ());
    f ();
    return time::highresolutiontickstoseconds (
            time::gethighresolutionticks () - starttime);
}

#if 0
template <typename function,
    typename p1, typename p2, typename p3, typename p4,
    typename p5, typename p6, typename p7, typename p8>
double measurefunctioncalltime (function f, p1 p1, p2 p2, p3 p3, p4 p4, p5 p5, p6 p6, p7 p7, p8 p8)
{
    std::int64_t const starttime (time::gethighresolutionticks ());
    f (p1, p2, p3, p4, p5 ,p6 ,p7 ,p8);
    return time::highresolutiontickstoseconds (
            time::gethighresolutionticks () - starttime);
}

template <typename function,
    typename p1, typename p2, typename p3, typename p4,
    typename p5, typename p6, typename p7, typename p8>
double measurefunctioncalltime (function f, p1 p1, p2 p2, p3 p3, p4 p4, p5 p5, p6 p6, p7 p7, p8 p8)
{
    std::int64_t const starttime (time::gethighresolutionticks ());
    f (p1, p2, p3, p4, p5 ,p6 ,p7 ,p8);
    return time::highresolutiontickstoseconds (
            time::gethighresolutionticks () - starttime);
}

template <typename function,
    typename p1, typename p2, typename p3, typename p4,
    typename p5, typename p6, typename p7, typename p8>
double measurefunctioncalltime (function f, p1 p1, p2 p2, p3 p3, p4 p4, p5 p5, p6 p6, p7 p7, p8 p8)
{
    std::int64_t const starttime (time::gethighresolutionticks ());
    f (p1, p2, p3, p4, p5 ,p6 ,p7 ,p8);
    return time::highresolutiontickstoseconds (
            time::gethighresolutionticks () - starttime);
}

template <typename function,
    typename p1, typename p2, typename p3, typename p4,
    typename p5, typename p6, typename p7, typename p8>
double measurefunctioncalltime (function f, p1 p1, p2 p2, p3 p3, p4 p4, p5 p5, p6 p6, p7 p7, p8 p8)
{
    std::int64_t const starttime (time::gethighresolutionticks ());
    f (p1, p2, p3, p4, p5 ,p6 ,p7 ,p8);
    return time::highresolutiontickstoseconds (
            time::gethighresolutionticks () - starttime);
}
#endif

} // beast

#endif
