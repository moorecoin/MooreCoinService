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

#ifndef beast_utility_ci_char_traits_h_included
#define beast_utility_ci_char_traits_h_included

#include <beast/cxx14/algorithm.h> // <algorithm>
#include <beast/cxx14/type_traits.h> // <type_traits>
#include <cctype>
#include <iterator>
#include <locale>
#include <string>

namespace beast {

/** case-insensitive function object for performing less than comparisons. */
struct ci_less
{
    static bool const is_transparent = true;

    template <class string>
    bool
    operator() (string const& lhs, string const& rhs) const
    {
        using std::begin;
        using std::end;
        using char_type = typename string::value_type;
        return std::lexicographical_compare (
            begin(lhs), end(lhs), begin(rhs), end(rhs),
            [] (char_type lhs, char_type rhs)
            {
                return std::tolower(lhs) < std::tolower(rhs);
            }
        );
    }
};

/** returns `true` if strings are case-insensitive equal. */
template <class lhs, class rhs>
std::enable_if_t<std::is_same<typename lhs::value_type, char>::value &&
    std::is_same<typename lhs::value_type, char>::value, bool>
ci_equal(lhs const& lhs, rhs const& rhs)
{
    using std::begin;
    using std::end;
    return std::equal (begin(lhs), end(lhs), begin(rhs), end(rhs),
        [] (char lhs, char rhs)
        {
            return std::tolower(lhs) == std::tolower(rhs);
        }
    );
}

}

#endif
