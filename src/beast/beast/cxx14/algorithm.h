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

#ifndef beast_cxx14_algorithm_h_included
#define beast_cxx14_algorithm_h_included

#include <beast/cxx14/config.h>
#include <beast/cxx14/functional.h>

#include <algorithm>

#if ! beast_no_cxx14_equal

namespace std {

namespace detail {

template <class pred, class fwdit1, class fwdit2>
bool equal (fwdit1 first1, fwdit1 last1,
    fwdit2 first2, fwdit2 last2, pred pred,
        std::input_iterator_tag, std::input_iterator_tag)
{
    for (; first1 != last1 && first2 != last2; ++first1, ++first2)
        if (! pred (*first1, *first2))
            return false;
    return first1 == last1 && first2 == last2;
}

template <class pred, class ranit1, class ranit2>
bool equal (ranit1 first1, ranit1 last1,
    ranit2 first2, ranit2 last2, pred pred,
        random_access_iterator_tag,
            random_access_iterator_tag )
{
    if (std::distance (first1, last1) !=
            std::distance (first2, last2))
        return false;
    for (; first1 != last1; ++first1, ++first2)
        if (! pred (*first1, *first2))
            return false;
    return true;
}

}

/** c++14 implementation of std::equal. */
/** @{ */
template <class fwdit1, class fwdit2>
bool equal (fwdit1 first1, fwdit1 last1,
    fwdit2 first2, fwdit2 last2)
{
    return std::detail::equal (first1, last1,
        first2, last2, std::equal_to <void>(),
            typename std::iterator_traits <
                fwdit1>::iterator_category(),
                    typename std::iterator_traits <
                        fwdit2>::iterator_category());
}

template <class fwdit1, class fwdit2, class pred>
bool equal (fwdit1 first1, fwdit1 last1,
    fwdit2 first2, fwdit2 last2, pred pred)
{
    return std::detail::equal <
        typename std::add_lvalue_reference <pred>::type> (
            first1, last1, first2, last2, pred,
                typename std::iterator_traits <
                    fwdit1>::iterator_category(),
                        typename std::iterator_traits <
                            fwdit2>::iterator_category());
}
/** @} */

}

#endif

#endif
