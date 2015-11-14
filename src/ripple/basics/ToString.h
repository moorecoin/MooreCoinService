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

#ifndef ripple_basics_tostring_h_included
#define ripple_basics_tostring_h_included

namespace ripple {

/** to_string() generalizes std::to_string to handle bools, chars, and strings.

    it's also possible to provide implementation of to_string for a class
    which needs a string implementation.
 */

template <class t>
typename std::enable_if<std::is_arithmetic<t>::value,
                        std::string>::type
to_string(t t)
{
    return std::to_string(t);
}

inline std::string to_string(bool b)
{
    return b ? "true" : "false";
}

inline std::string to_string(char c)
{
    return std::string(1, c);
}

inline std::string to_string(std::string s)
{
    return s;
}

inline std::string to_string(char const* s)
{
    return s;
}

} // ripple

#endif
