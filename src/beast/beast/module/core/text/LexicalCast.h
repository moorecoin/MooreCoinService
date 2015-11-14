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

#ifndef beast_lexicalcast_h_included
#define beast_lexicalcast_h_included

#include <beast/config.h>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <beast/cxx14/type_traits.h> // <type_traits>
#include <typeinfo>
#include <utility>

namespace beast {

namespace detail {

#ifdef _msc_ver
#pragma warning(push)
#pragma warning(disable: 4800)
#pragma warning(disable: 4804)
#endif

template <class int, class fwdit, class accumulator>
bool
parse_integral (int& num, fwdit first, fwdit last, accumulator accumulator)
{
    num = 0;

    if (first == last)
        return false;

    while (first != last)
    {
        auto const c = *first++;
        if (c < '0' || c > '9')
            return false;
        if (!accumulator(num, int(c - '0')))
            return false;
    }

    return true;
}

template <class int, class fwdit>
bool
parse_negative_integral (int& num, fwdit first, fwdit last)
{
    int limit_value = std::numeric_limits <int>::min() / 10;
    int limit_digit = std::numeric_limits <int>::min() % 10;

    if (limit_digit < 0)
        limit_digit = -limit_digit;

    return parse_integral<int> (num, first, last, 
        [limit_value, limit_digit](int& value, int digit)
        {
            assert ((digit >= 0) && (digit <= 9));
            if (value < limit_value || (value == limit_value && digit > limit_digit))
                return false;
            value = (value * 10) - digit;
            return true;
        });
}

template <class int, class fwdit>
bool
parse_positive_integral (int& num, fwdit first, fwdit last)
{
    int limit_value = std::numeric_limits <int>::max() / 10;
    int limit_digit = std::numeric_limits <int>::max() % 10;

    return parse_integral<int> (num, first, last,
        [limit_value, limit_digit](int& value, int digit)
        {
            assert ((digit >= 0) && (digit <= 9));
            if (value > limit_value || (value == limit_value && digit > limit_digit))
                return false;
            value = (value * 10) + digit;
            return true;
        });
}

template <class inttype, class fwdit>
bool
parsesigned (inttype& result, fwdit first, fwdit last)
{
    static_assert(std::is_signed<inttype>::value,
        "you may only call parsesigned with a signed integral type.");

    if (first != last && *first == '-')
        return parse_negative_integral (result, first + 1, last);

    if (first != last && *first == '+')
        return parse_positive_integral (result, first + 1, last);

    return parse_positive_integral (result, first, last);
}

template <class uinttype, class fwdit>
bool
parseunsigned (uinttype& result, fwdit first, fwdit last)
{
    static_assert(std::is_unsigned<uinttype>::value,
        "you may only call parseunsigned with an unsigned integral type.");

    if (first != last && *first == '+')
        return parse_positive_integral (result, first + 1, last);

    return parse_positive_integral (result, first, last);
}

//------------------------------------------------------------------------------

// these specializatons get called by the non-member functions to do the work
template <class out, class in>
struct lexicalcast;

// conversion to std::string
template <class in>
struct lexicalcast <std::string, in>
{
    template <class arithmetic = in>
    std::enable_if_t <std::is_arithmetic <arithmetic>::value, bool>
    operator () (std::string& out, arithmetic in)
    {
        out = std::to_string (in);
        return true;
    }

    template <class enumeration = in>
    std::enable_if_t <std::is_enum <enumeration>::value, bool>
    operator () (std::string& out, enumeration in)
    {
        out = std::to_string (
            static_cast <std::underlying_type_t <enumeration>> (in));
        return true;
    }
};

// parse std::string to number
template <class out>
struct lexicalcast <out, std::string>
{
    static_assert (std::is_integral <out>::value,
        "beast::lexicalcast can only be used with integral types");

    template <class integral = out>
    std::enable_if_t <std::is_unsigned <integral>::value, bool>
    operator () (integral& out, std::string const& in) const
    {
        return parseunsigned (out, in.begin(), in.end());
    }

    template <class integral = out>
    std::enable_if_t <std::is_signed <integral>::value, bool>
    operator () (integral& out, std::string const& in) const
    {
        return parsesigned (out, in.begin(), in.end());
    }

    bool
    operator () (bool& out, std::string in) const
    {
        // convert the input to lowercase
        std::transform(in.begin (), in.end (), in.begin (), ::tolower);

        if (in == "1" || in == "true")
        {
            out = true;
            return true;
        }

        if (in == "0" || in == "false")
        {
            out = false;
            return true;
        }

        return false;
    }
};

//------------------------------------------------------------------------------

// conversion from null terminated char const*
template <class out>
struct lexicalcast <out, char const*>
{
    bool operator() (out& out, char const* in) const
    {
        return lexicalcast <out, std::string>()(out, in);
    }
};

// conversion from null terminated char*
// the string is not modified.
template <class out>
struct lexicalcast <out, char*>
{
    bool operator() (out& out, char* in) const
    {
        return lexicalcast <out, std::string>()(out, in);
    }
};

#ifdef _msc_ver
#pragma warning(pop)
#endif

} // detail

//------------------------------------------------------------------------------

/** thrown when a conversion is not possible with lexicalcast.
    only used in the throw variants of lexicalcast.
*/
struct badlexicalcast : public std::bad_cast
{
};

/** intelligently convert from one type to another.
    @return `false` if there was a parsing or range error
*/
template <class out, class in>
bool lexicalcastchecked (out& out, in in)
{
    return detail::lexicalcast <out, in> () (out, in);
}

/** convert from one type to another, throw on error

    an exception of type badlexicalcast is thrown if the conversion fails.

    @return the new type.
*/
template <class out, class in>
out lexicalcastthrow (in in)
{
    out out;

    if (lexicalcastchecked (out, in))
        return out;

    throw badlexicalcast ();
}

/** convert from one type to another.

    @param defaultvalue the value returned if parsing fails
    @return the new type.
*/
template <class out, class in>
out lexicalcast (in in, out defaultvalue = out ())
{
    out out;

    if (lexicalcastchecked (out, in))
        return out;

    return defaultvalue;
}

} // beast

#endif
