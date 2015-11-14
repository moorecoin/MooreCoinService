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

#ifndef beast_strings_characterfunctions_h_included
#define beast_strings_characterfunctions_h_included

#include <limits>

#include <beast/config.h>
#include <beast/memory.h>

#include <cstdint>

namespace beast {

//==============================================================================
#if beast_windows && ! doxygen
 #define beast_native_wchar_is_utf8      0
 #define beast_native_wchar_is_utf16     1
 #define beast_native_wchar_is_utf32     0
#else
 /** this macro will be set to 1 if the compiler's native wchar_t is an 8-bit type. */
 #define beast_native_wchar_is_utf8      0
 /** this macro will be set to 1 if the compiler's native wchar_t is a 16-bit type. */
 #define beast_native_wchar_is_utf16     0
 /** this macro will be set to 1 if the compiler's native wchar_t is a 32-bit type. */
 #define beast_native_wchar_is_utf32     1
#endif

#if beast_native_wchar_is_utf32 || doxygen
 /** a platform-independent 32-bit unicode character type. */
 typedef wchar_t        beast_wchar;
#else
 typedef std::uint32_t         beast_wchar;
#endif

#ifndef doxygen
 /** this macro is deprecated, but preserved for compatibility with old code. */
 #define beast_t(stringliteral)   (l##stringliteral)
#endif

#if beast_define_t_macro
 /** the 't' macro is an alternative for using the "l" prefix in front of a string literal.

     this macro is deprecated, but available for compatibility with old code if you set
     beast_define_t_macro = 1. the fastest, most portable and best way to write your string
     literals is as standard char strings, using escaped utf-8 character sequences for extended
     characters, rather than trying to store them as wide-char strings.
 */
 #define t(stringliteral)   beast_t(stringliteral)
#endif

//==============================================================================
/**
    a collection of functions for manipulating characters and character strings.

    most of these methods are designed for internal use by the string and charpointer
    classes, but some of them may be useful to call directly.

    @see string, charpointer_utf8, charpointer_utf16, charpointer_utf32
*/
class characterfunctions
{
public:
    //==============================================================================
    /** converts a character to upper-case. */
    static beast_wchar touppercase (beast_wchar character) noexcept;
    /** converts a character to lower-case. */
    static beast_wchar tolowercase (beast_wchar character) noexcept;

    /** checks whether a unicode character is upper-case. */
    static bool isuppercase (beast_wchar character) noexcept;
    /** checks whether a unicode character is lower-case. */
    static bool islowercase (beast_wchar character) noexcept;

    /** checks whether a character is whitespace. */
    static bool iswhitespace (char character) noexcept;
    /** checks whether a character is whitespace. */
    static bool iswhitespace (beast_wchar character) noexcept;

    /** checks whether a character is a digit. */
    static bool isdigit (char character) noexcept;
    /** checks whether a character is a digit. */
    static bool isdigit (beast_wchar character) noexcept;

    /** checks whether a character is alphabetic. */
    static bool isletter (char character) noexcept;
    /** checks whether a character is alphabetic. */
    static bool isletter (beast_wchar character) noexcept;

    /** checks whether a character is alphabetic or numeric. */
    static bool isletterordigit (char character) noexcept;
    /** checks whether a character is alphabetic or numeric. */
    static bool isletterordigit (beast_wchar character) noexcept;

    /** returns 0 to 16 for '0' to 'f", or -1 for characters that aren't a legal hex digit. */
    static int gethexdigitvalue (beast_wchar digit) noexcept;

    //==============================================================================
    /** parses a character string to read a floating-point number.
        note that this will advance the pointer that is passed in, leaving it at
        the end of the number.
    */
    template <typename charpointertype>
    static double readdoublevalue (charpointertype& text) noexcept
    {
        double result[3] = { 0 }, accumulator[2] = { 0 };
        int exponentadjustment[2] = { 0 }, exponentaccumulator[2] = { -1, -1 };
        int exponent = 0, decpointindex = 0, digit = 0;
        int lastdigit = 0, numsignificantdigits = 0;
        bool isnegative = false, digitsfound = false;
        const int maxsignificantdigits = 15 + 2;

        text = text.findendofwhitespace();
        beast_wchar c = *text;

        switch (c)
        {
            case '-':   isnegative = true; // fall-through..
            case '+':   c = *++text;
        }

        switch (c)
        {
            case 'n':
            case 'n':
                if ((text[1] == 'a' || text[1] == 'a') && (text[2] == 'n' || text[2] == 'n'))
                    return std::numeric_limits<double>::quiet_nan();
                break;

            case 'i':
            case 'i':
                if ((text[1] == 'n' || text[1] == 'n') && (text[2] == 'f' || text[2] == 'f'))
                    return std::numeric_limits<double>::infinity();
                break;
        }

        for (;;)
        {
            if (text.isdigit())
            {
                lastdigit = digit;
                digit = (int) text.getandadvance() - '0';
                digitsfound = true;

                if (decpointindex != 0)
                    exponentadjustment[1]++;

                if (numsignificantdigits == 0 && digit == 0)
                    continue;

                if (++numsignificantdigits > maxsignificantdigits)
                {
                    if (digit > 5)
                        ++accumulator [decpointindex];
                    else if (digit == 5 && (lastdigit & 1) != 0)
                        ++accumulator [decpointindex];

                    if (decpointindex > 0)
                        exponentadjustment[1]--;
                    else
                        exponentadjustment[0]++;

                    while (text.isdigit())
                    {
                        ++text;
                        if (decpointindex == 0)
                            exponentadjustment[0]++;
                    }
                }
                else
                {
                    const double maxaccumulatorvalue = (double) ((std::numeric_limits<unsigned int>::max() - 9) / 10);
                    if (accumulator [decpointindex] > maxaccumulatorvalue)
                    {
                        result [decpointindex] = mulexp10 (result [decpointindex], exponentaccumulator [decpointindex])
                                                    + accumulator [decpointindex];
                        accumulator [decpointindex] = 0;
                        exponentaccumulator [decpointindex] = 0;
                    }

                    accumulator [decpointindex] = accumulator[decpointindex] * 10 + digit;
                    exponentaccumulator [decpointindex]++;
                }
            }
            else if (decpointindex == 0 && *text == '.')
            {
                ++text;
                decpointindex = 1;

                if (numsignificantdigits > maxsignificantdigits)
                {
                    while (text.isdigit())
                        ++text;
                    break;
                }
            }
            else
            {
                break;
            }
        }

        result[0] = mulexp10 (result[0], exponentaccumulator[0]) + accumulator[0];

        if (decpointindex != 0)
            result[1] = mulexp10 (result[1], exponentaccumulator[1]) + accumulator[1];

        c = *text;
        if ((c == 'e' || c == 'e') && digitsfound)
        {
            bool negativeexponent = false;

            switch (*++text)
            {
                case '-':   negativeexponent = true; // fall-through..
                case '+':   ++text;
            }

            while (text.isdigit())
                exponent = (exponent * 10) + ((int) text.getandadvance() - '0');

            if (negativeexponent)
                exponent = -exponent;
        }

        double r = mulexp10 (result[0], exponent + exponentadjustment[0]);
        if (decpointindex != 0)
            r += mulexp10 (result[1], exponent - exponentadjustment[1]);

        return isnegative ? -r : r;
    }

    /** parses a character string, to read a floating-point value. */
    template <typename charpointertype>
    static double getdoublevalue (charpointertype text) noexcept
    {
        return readdoublevalue (text);
    }

    //==============================================================================
    /** parses a character string, to read an integer value. */
    template <typename inttype, typename charpointertype>
    static inttype getintvalue (const charpointertype text) noexcept
    {
        inttype v = 0;
        charpointertype s (text.findendofwhitespace());

        const bool isneg = *s == '-';
        if (isneg)
            ++s;

        for (;;)
        {
            const beast_wchar c = s.getandadvance();

            if (c >= '0' && c <= '9')
                v = v * 10 + (inttype) (c - '0');
            else
                break;
        }

        return isneg ? -v : v;
    }

    //==============================================================================
    /** counts the number of characters in a given string, stopping if the count exceeds
        a specified limit. */
    template <typename charpointertype>
    static size_t lengthupto (charpointertype text, const size_t maxcharstocount) noexcept
    {
        size_t len = 0;

        while (len < maxcharstocount && text.getandadvance() != 0)
            ++len;

        return len;
    }

    /** counts the number of characters in a given string, stopping if the count exceeds
        a specified end-pointer. */
    template <typename charpointertype>
    static size_t lengthupto (charpointertype start, const charpointertype end) noexcept
    {
        size_t len = 0;

        while (start < end && start.getandadvance() != 0)
            ++len;

        return len;
    }

    /** copies null-terminated characters from one string to another. */
    template <typename destcharpointertype, typename srccharpointertype>
    static void copyall (destcharpointertype& dest, srccharpointertype src) noexcept
    {
        for (;;)
        {
            const beast_wchar c = src.getandadvance();

            if (c == 0)
                break;

            dest.write (c);
        }

        dest.writenull();
    }

    /** copies characters from one string to another, up to a null terminator
        or a given byte size limit. */
    template <typename destcharpointertype, typename srccharpointertype>
    static size_t copywithdestbytelimit (destcharpointertype& dest, srccharpointertype src, size_t maxbytestowrite) noexcept
    {
        typename destcharpointertype::chartype const* const startaddress = dest.getaddress();
        size_t maxbytes = maxbytestowrite;
        if (maxbytes >= sizeof (typename destcharpointertype::chartype))
            maxbytes -= sizeof (typename destcharpointertype::chartype); // (allow for a terminating null)
        else
            maxbytes = 0;

        for (;;)
        {
            const beast_wchar c = src.getandadvance();
            const size_t bytesneeded = destcharpointertype::getbytesrequiredfor (c);

            if (c == 0 || maxbytes < bytesneeded)
                break;
            maxbytes -= bytesneeded;

            dest.write (c);
        }

        dest.writenull();

        return (size_t) getaddressdifference (dest.getaddress(), startaddress)
                 + sizeof (typename destcharpointertype::chartype);
    }

    /** copies characters from one string to another, up to a null terminator
        or a given maximum number of characters. */
    template <typename destcharpointertype, typename srccharpointertype>
    static void copywithcharlimit (destcharpointertype& dest, srccharpointertype src, int maxchars) noexcept
    {
        while (--maxchars > 0)
        {
            const beast_wchar c = src.getandadvance();
            if (c == 0)
                break;

            dest.write (c);
        }

        dest.writenull();
    }

    /** compares two null-terminated character strings. */
    template <typename charpointertype1, typename charpointertype2>
    static int compare (charpointertype1 s1, charpointertype2 s2) noexcept
    {
        for (;;)
        {
            const int c1 = (int) s1.getandadvance();
            const int c2 = (int) s2.getandadvance();
            const int diff = c1 - c2;

            if (diff != 0)  return diff < 0 ? -1 : 1;
            if (c1 == 0)    break;
        }

        return 0;
    }

    /** compares two null-terminated character strings, up to a given number of characters. */
    template <typename charpointertype1, typename charpointertype2>
    static int compareupto (charpointertype1 s1, charpointertype2 s2, int maxchars) noexcept
    {
        while (--maxchars >= 0)
        {
            const int c1 = (int) s1.getandadvance();
            const int c2 = (int) s2.getandadvance();
            const int diff = c1 - c2;

            if (diff != 0)  return diff < 0 ? -1 : 1;
            if (c1 == 0)    break;
        }

        return 0;
    }

    /** compares two null-terminated character strings, using a case-independant match. */
    template <typename charpointertype1, typename charpointertype2>
    static int compareignorecase (charpointertype1 s1, charpointertype2 s2) noexcept
    {
        for (;;)
        {
            const int c1 = (int) s1.touppercase(); ++s1;
            const int c2 = (int) s2.touppercase(); ++s2;
            const int diff = c1 - c2;

            if (diff != 0)  return diff < 0 ? -1 : 1;
            if (c1 == 0)    break;
        }

        return 0;
    }

    /** compares two null-terminated character strings, using a case-independent match. */
    template <typename charpointertype1, typename charpointertype2>
    static int compareignorecaseupto (charpointertype1 s1, charpointertype2 s2, int maxchars) noexcept
    {
        while (--maxchars >= 0)
        {
            const int c1 = (int) s1.touppercase(); ++s1;
            const int c2 = (int) s2.touppercase(); ++s2;
            const int diff = c1 - c2;

            if (diff != 0)  return diff < 0 ? -1 : 1;
            if (c1 == 0)    break;
        }

        return 0;
    }

    /** finds the character index of a given substring in another string.
        returns -1 if the substring is not found.
    */
    template <typename charpointertype1, typename charpointertype2>
    static int indexof (charpointertype1 texttosearch, const charpointertype2 substringtolookfor) noexcept
    {
        int index = 0;
        const int substringlength = (int) substringtolookfor.length();

        for (;;)
        {
            if (texttosearch.compareupto (substringtolookfor, substringlength) == 0)
                return index;

            if (texttosearch.getandadvance() == 0)
                return -1;

            ++index;
        }
    }

    /** returns a pointer to the first occurrence of a substring in a string.
        if the substring is not found, this will return a pointer to the string's
        null terminator.
    */
    template <typename charpointertype1, typename charpointertype2>
    static charpointertype1 find (charpointertype1 texttosearch, const charpointertype2 substringtolookfor) noexcept
    {
        const int substringlength = (int) substringtolookfor.length();

        while (texttosearch.compareupto (substringtolookfor, substringlength) != 0
                 && ! texttosearch.isempty())
            ++texttosearch;

        return texttosearch;
    }

    /** finds the character index of a given substring in another string, using
        a case-independent match.
        returns -1 if the substring is not found.
    */
    template <typename charpointertype1, typename charpointertype2>
    static int indexofignorecase (charpointertype1 haystack, const charpointertype2 needle) noexcept
    {
        int index = 0;
        const int needlelength = (int) needle.length();

        for (;;)
        {
            if (haystack.compareignorecaseupto (needle, needlelength) == 0)
                return index;

            if (haystack.getandadvance() == 0)
                return -1;

            ++index;
        }
    }

    /** finds the character index of a given character in another string.
        returns -1 if the character is not found.
    */
    template <typename type>
    static int indexofchar (type text, const beast_wchar chartofind) noexcept
    {
        int i = 0;

        while (! text.isempty())
        {
            if (text.getandadvance() == chartofind)
                return i;

            ++i;
        }

        return -1;
    }

    /** finds the character index of a given character in another string, using
        a case-independent match.
        returns -1 if the character is not found.
    */
    template <typename type>
    static int indexofcharignorecase (type text, beast_wchar chartofind) noexcept
    {
        chartofind = characterfunctions::tolowercase (chartofind);
        int i = 0;

        while (! text.isempty())
        {
            if (text.tolowercase() == chartofind)
                return i;

            ++text;
            ++i;
        }

        return -1;
    }

    /** returns a pointer to the first non-whitespace character in a string.
        if the string contains only whitespace, this will return a pointer
        to its null terminator.
    */
    template <typename type>
    static type findendofwhitespace (const type& text) noexcept
    {
        type p (text);

        while (p.iswhitespace())
            ++p;

        return p;
    }

    /** returns a pointer to the first character in the string which is found in
        the breakcharacters string.
    */
    template <typename type>
    static type findendoftoken (const type& text, const type& breakcharacters, const type& quotecharacters)
    {
        type t (text);
        beast_wchar currentquotechar = 0;

        while (! t.isempty())
        {
            const beast_wchar c = t.getandadvance();

            if (currentquotechar == 0 && breakcharacters.indexof (c) >= 0)
            {
                --t;
                break;
            }

            if (quotecharacters.indexof (c) >= 0)
            {
                if (currentquotechar == 0)
                    currentquotechar = c;
                else if (currentquotechar == c)
                    currentquotechar = 0;
            }
        }

        return t;
    }

private:
    static double mulexp10 (const double value, int exponent) noexcept;
};

}

#endif

