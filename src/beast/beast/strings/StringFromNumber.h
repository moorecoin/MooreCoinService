//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    portions of this file are from juce.
    copyright (c) 2013 - raw material software ltd.
    please visit http://www.beast.com

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

#ifndef beast_strings_stringfromnumber_h_included
#define beast_strings_stringfromnumber_h_included

#include <beast/config.h>
#include <beast/arithmetic.h>

#include <beast/strings/stringcharpointertype.h>

#include <limits>
#include <ostream>
#include <streambuf>

namespace beast {

// vfalco todo put this in namespace detail
//
class numbertostringconverters
{
public:
    enum
    {
        charsneededforint = 32,
        charsneededfordouble = 48
    };

    // pass in a pointer to the end of a buffer..
    template <typename type>
    static char* printdigits (char* t, type v) noexcept
    {
        *--t = 0;
        do
        {
            *--t = '0' + (char) (v % 10);
            v /= 10;
        }
        while (v > 0);
        return t;
    }

#ifdef _msc_ver
#pragma warning (push)
#pragma warning (disable: 4127) // conditional expression is constant
#pragma warning (disable: 4146) // unary minus operator applied to unsigned type, result still unsigned
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-wtautological-compare"
#endif
    // pass in a pointer to the end of a buffer..
    template <typename integertype>
    static char* numbertostring (char* t, integertype const n) noexcept
    {
        if (std::numeric_limits <integertype>::is_signed)
        {
            if (n >= 0)
                return printdigits (t, static_cast <std::uint64_t> (n));

            // nb: this needs to be careful not to call
            // -std::numeric_limits<std::int64_t>::min(),
            // which has undefined behaviour
            //
            t = printdigits (t, static_cast <std::uint64_t> (-(n + 1)) + 1);
            *--t = '-';
            return t;
        }
        return printdigits (t, n);
    }
#ifdef _msc_ver
#pragma warning (pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

    struct stackarraystream : public std::basic_streambuf <char, std::char_traits <char> >
    {
        explicit stackarraystream (char* d)
        {
            imbue (std::locale::classic());
            setp (d, d + charsneededfordouble);
        }

        size_t writedouble (double n, int numdecplaces)
        {
            {
                std::ostream o (this);

                if (numdecplaces > 0)
                    o.precision ((std::streamsize) numdecplaces);

                o << n;
            }

            return (size_t) (pptr() - pbase());
        }
    };

    static char* doubletostring (char* buffer,
        const int numchars, double n, int numdecplaces, size_t& len) noexcept
    {
        if (numdecplaces > 0 && numdecplaces < 7 && n > -1.0e20 && n < 1.0e20)
        {
            char* const end = buffer + numchars;
            char* t = end;
            std::int64_t v = (std::int64_t) (pow (10.0, numdecplaces) * std::abs (n) + 0.5);
            *--t = (char) 0;

            while (numdecplaces >= 0 || v > 0)
            {
                if (numdecplaces == 0)
                    *--t = '.';

                *--t = (char) ('0' + (v % 10));

                v /= 10;
                --numdecplaces;
            }

            if (n < 0)
                *--t = '-';

            len = (size_t) (end - t - 1);
            return t;
        }

        stackarraystream strm (buffer);
        len = strm.writedouble (n, numdecplaces);
        bassert (len <= charsneededfordouble);
        return buffer;
    }

    static stringcharpointertype createfromfixedlength (
        const char* const src, const size_t numchars);

    template <typename integertype>
    static stringcharpointertype createfrominteger (const integertype number)
    {
        char buffer [charsneededforint];
        char* const end = buffer + numelementsinarray (buffer);
        char* const start = numbertostring (end, number);
        return createfromfixedlength (start, (size_t) (end - start - 1));
    }

    static stringcharpointertype createfromdouble (
        const double number, const int numberofdecimalplaces)
    {
        char buffer [charsneededfordouble];
        size_t len;
        char* const start = doubletostring (buffer, numelementsinarray (buffer), (double) number, numberofdecimalplaces, len);
        return createfromfixedlength (start, len);
    }
};

}

#endif
