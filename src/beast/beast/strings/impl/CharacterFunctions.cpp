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

#if beast_include_beastconfig
#include "../../beastconfig.h"
#endif

#include <beast/strings/characterfunctions.h>

#include <cctype>
#include <cwctype>

namespace beast {

#if beast_msvc
 #pragma warning (push)
 #pragma warning (disable: 4514 4996)
#endif

beast_wchar characterfunctions::touppercase (const beast_wchar character) noexcept
{
    return towupper ((wchar_t) character);
}

beast_wchar characterfunctions::tolowercase (const beast_wchar character) noexcept
{
    return towlower ((wchar_t) character);
}

bool characterfunctions::isuppercase (const beast_wchar character) noexcept
{
   #if beast_windows
    return iswupper ((wchar_t) character) != 0;
   #else
    return tolowercase (character) != character;
   #endif
}

bool characterfunctions::islowercase (const beast_wchar character) noexcept
{
   #if beast_windows
    return iswlower ((wchar_t) character) != 0;
   #else
    return touppercase (character) != character;
   #endif
}

#if beast_msvc
 #pragma warning (pop)
#endif

//==============================================================================
bool characterfunctions::iswhitespace (const char character) noexcept
{
    return character == ' ' || (character <= 13 && character >= 9);
}

bool characterfunctions::iswhitespace (const beast_wchar character) noexcept
{
    return iswspace ((wchar_t) character) != 0;
}

bool characterfunctions::isdigit (const char character) noexcept
{
    return (character >= '0' && character <= '9');
}

bool characterfunctions::isdigit (const beast_wchar character) noexcept
{
    return iswdigit ((wchar_t) character) != 0;
}

bool characterfunctions::isletter (const char character) noexcept
{
    return (character >= 'a' && character <= 'z')
        || (character >= 'a' && character <= 'z');
}

bool characterfunctions::isletter (const beast_wchar character) noexcept
{
    return iswalpha ((wchar_t) character) != 0;
}

bool characterfunctions::isletterordigit (const char character) noexcept
{
    return (character >= 'a' && character <= 'z')
        || (character >= 'a' && character <= 'z')
        || (character >= '0' && character <= '9');
}

bool characterfunctions::isletterordigit (const beast_wchar character) noexcept
{
    return iswalnum ((wchar_t) character) != 0;
}

int characterfunctions::gethexdigitvalue (const beast_wchar digit) noexcept
{
    unsigned int d = (unsigned int) digit - '0';
    if (d < (unsigned int) 10)
        return (int) d;

    d += (unsigned int) ('0' - 'a');
    if (d < (unsigned int) 6)
        return (int) d + 10;

    d += (unsigned int) ('a' - 'a');
    if (d < (unsigned int) 6)
        return (int) d + 10;

    return -1;
}

double characterfunctions::mulexp10 (const double value, int exponent) noexcept
{
    if (exponent == 0)
        return value;

    if (value == 0)
        return 0;

    const bool negative = (exponent < 0);
    if (negative)
        exponent = -exponent;

    double result = 1.0, power = 10.0;
    for (int bit = 1; exponent != 0; bit <<= 1)
    {
        if ((exponent & bit) != 0)
        {
            exponent ^= bit;
            result *= power;
            if (exponent == 0)
                break;
        }
        power *= power;
    }

    return negative ? (value / result) : (value * result);
}

}
