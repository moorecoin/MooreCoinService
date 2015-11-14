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

#ifndef beast_charpointer_ascii_h_included
#define beast_charpointer_ascii_h_included

#include <beast/config.h>

#include <beast/strings/characterfunctions.h>

#include <cstdlib>
#include <cstring>

namespace beast {

//==============================================================================
/**
    wraps a pointer to a null-terminated ascii character string, and provides
    various methods to operate on the data.

    a valid ascii string is assumed to not contain any characters above 127.

    @see charpointer_utf8, charpointer_utf16, charpointer_utf32
*/
class charpointer_ascii
{
public:
    typedef char chartype;

    inline explicit charpointer_ascii (const chartype* const rawpointer) noexcept
        : data (const_cast <chartype*> (rawpointer))
    {
    }

    inline charpointer_ascii (const charpointer_ascii& other) noexcept
        : data (other.data)
    {
    }

    inline charpointer_ascii operator= (const charpointer_ascii other) noexcept
    {
        data = other.data;
        return *this;
    }

    inline charpointer_ascii operator= (const chartype* text) noexcept
    {
        data = const_cast <chartype*> (text);
        return *this;
    }

    /** this is a pointer comparison, it doesn't compare the actual text. */
    inline bool operator== (charpointer_ascii other) const noexcept     { return data == other.data; }
    inline bool operator!= (charpointer_ascii other) const noexcept     { return data != other.data; }
    inline bool operator<= (charpointer_ascii other) const noexcept     { return data <= other.data; }
    inline bool operator<  (charpointer_ascii other) const noexcept     { return data <  other.data; }
    inline bool operator>= (charpointer_ascii other) const noexcept     { return data >= other.data; }
    inline bool operator>  (charpointer_ascii other) const noexcept     { return data >  other.data; }

    /** returns the address that this pointer is pointing to. */
    inline chartype* getaddress() const noexcept        { return data; }

    /** returns the address that this pointer is pointing to. */
    inline operator const chartype*() const noexcept    { return data; }

    /** returns true if this pointer is pointing to a null character. */
    inline bool isempty() const noexcept                { return *data == 0; }

    /** returns the unicode character that this pointer is pointing to. */
    inline beast_wchar operator*() const noexcept        { return (beast_wchar) (std::uint8_t) *data; }

    /** moves this pointer along to the next character in the string. */
    inline charpointer_ascii operator++() noexcept
    {
        ++data;
        return *this;
    }

    /** moves this pointer to the previous character in the string. */
    inline charpointer_ascii operator--() noexcept
    {
        --data;
        return *this;
    }

    /** returns the character that this pointer is currently pointing to, and then
        advances the pointer to point to the next character. */
    inline beast_wchar getandadvance() noexcept  { return (beast_wchar) (std::uint8_t) *data++; }

    /** moves this pointer along to the next character in the string. */
    charpointer_ascii operator++ (int) noexcept
    {
        charpointer_ascii temp (*this);
        ++data;
        return temp;
    }

    /** moves this pointer forwards by the specified number of characters. */
    inline void operator+= (const int numtoskip) noexcept
    {
        data += numtoskip;
    }

    inline void operator-= (const int numtoskip) noexcept
    {
        data -= numtoskip;
    }

    /** returns the character at a given character index from the start of the string. */
    inline beast_wchar operator[] (const int characterindex) const noexcept
    {
        return (beast_wchar) (unsigned char) data [characterindex];
    }

    /** returns a pointer which is moved forwards from this one by the specified number of characters. */
    charpointer_ascii operator+ (const int numtoskip) const noexcept
    {
        return charpointer_ascii (data + numtoskip);
    }

    /** returns a pointer which is moved backwards from this one by the specified number of characters. */
    charpointer_ascii operator- (const int numtoskip) const noexcept
    {
        return charpointer_ascii (data - numtoskip);
    }

    /** writes a unicode character to this string, and advances this pointer to point to the next position. */
    inline void write (const beast_wchar chartowrite) noexcept
    {
        *data++ = (char) chartowrite;
    }

    inline void replacechar (const beast_wchar newchar) noexcept
    {
        *data = (char) newchar;
    }

    /** writes a null character to this string (leaving the pointer's position unchanged). */
    inline void writenull() const noexcept
    {
        *data = 0;
    }

    /** returns the number of characters in this string. */
    size_t length() const noexcept
    {
        return (size_t) strlen (data);
    }

    /** returns the number of characters in this string, or the given value, whichever is lower. */
    size_t lengthupto (const size_t maxcharstocount) const noexcept
    {
        return characterfunctions::lengthupto (*this, maxcharstocount);
    }

    /** returns the number of characters in this string, or up to the given end pointer, whichever is lower. */
    size_t lengthupto (const charpointer_ascii end) const noexcept
    {
        return characterfunctions::lengthupto (*this, end);
    }

    /** returns the number of bytes that are used to represent this string.
        this includes the terminating null character.
    */
    size_t sizeinbytes() const noexcept
    {
        return length() + 1;
    }

    /** returns the number of bytes that would be needed to represent the given
        unicode character in this encoding format.
    */
    static inline size_t getbytesrequiredfor (const beast_wchar) noexcept
    {
        return 1;
    }

    /** returns the number of bytes that would be needed to represent the given
        string in this encoding format.
        the value returned does not include the terminating null character.
    */
    template <class charpointer>
    static size_t getbytesrequiredfor (const charpointer text) noexcept
    {
        return text.length();
    }

    /** returns a pointer to the null character that terminates this string. */
    charpointer_ascii findterminatingnull() const noexcept
    {
        return charpointer_ascii (data + length());
    }

    /** copies a source string to this pointer, advancing this pointer as it goes. */
    template <typename charpointer>
    void writeall (const charpointer src) noexcept
    {
        characterfunctions::copyall (*this, src);
    }

    /** copies a source string to this pointer, advancing this pointer as it goes. */
    void writeall (const charpointer_ascii src) noexcept
    {
        strcpy (data, src.data);
    }

    /** copies a source string to this pointer, advancing this pointer as it goes.
        the maxdestbytes parameter specifies the maximum number of bytes that can be written
        to the destination buffer before stopping.
    */
    template <typename charpointer>
    size_t writewithdestbytelimit (const charpointer src, const size_t maxdestbytes) noexcept
    {
        return characterfunctions::copywithdestbytelimit (*this, src, maxdestbytes);
    }

    /** copies a source string to this pointer, advancing this pointer as it goes.
        the maxchars parameter specifies the maximum number of characters that can be
        written to the destination buffer before stopping (including the terminating null).
    */
    template <typename charpointer>
    void writewithcharlimit (const charpointer src, const int maxchars) noexcept
    {
        characterfunctions::copywithcharlimit (*this, src, maxchars);
    }

    /** compares this string with another one. */
    template <typename charpointer>
    int compare (const charpointer other) const noexcept
    {
        return characterfunctions::compare (*this, other);
    }

    /** compares this string with another one. */
    int compare (const charpointer_ascii other) const noexcept
    {
        return strcmp (data, other.data);
    }

    /** compares this string with another one, up to a specified number of characters. */
    template <typename charpointer>
    int compareupto (const charpointer other, const int maxchars) const noexcept
    {
        return characterfunctions::compareupto (*this, other, maxchars);
    }

    /** compares this string with another one, up to a specified number of characters. */
    int compareupto (const charpointer_ascii other, const int maxchars) const noexcept
    {
        return strncmp (data, other.data, (size_t) maxchars);
    }

    /** compares this string with another one. */
    template <typename charpointer>
    int compareignorecase (const charpointer other) const
    {
        return characterfunctions::compareignorecase (*this, other);
    }

    int compareignorecase (const charpointer_ascii other) const
    {
       #if beast_windows
        return stricmp (data, other.data);
       #else
        return strcasecmp (data, other.data);
       #endif
    }

    /** compares this string with another one, up to a specified number of characters. */
    template <typename charpointer>
    int compareignorecaseupto (const charpointer other, const int maxchars) const noexcept
    {
        return characterfunctions::compareignorecaseupto (*this, other, maxchars);
    }

    /** returns the character index of a substring, or -1 if it isn't found. */
    template <typename charpointer>
    int indexof (const charpointer stringtofind) const noexcept
    {
        return characterfunctions::indexof (*this, stringtofind);
    }

    /** returns the character index of a unicode character, or -1 if it isn't found. */
    int indexof (const beast_wchar chartofind) const noexcept
    {
        int i = 0;

        while (data[i] != 0)
        {
            if (data[i] == (char) chartofind)
                return i;

            ++i;
        }

        return -1;
    }

    /** returns the character index of a unicode character, or -1 if it isn't found. */
    int indexof (const beast_wchar chartofind, const bool ignorecase) const noexcept
    {
        return ignorecase ? characterfunctions::indexofcharignorecase (*this, chartofind)
                          : characterfunctions::indexofchar (*this, chartofind);
    }

    /** returns true if the first character of this string is whitespace. */
    bool iswhitespace() const               { return characterfunctions::iswhitespace (*data) != 0; }
    /** returns true if the first character of this string is a digit. */
    bool isdigit() const                    { return characterfunctions::isdigit (*data) != 0; }
    /** returns true if the first character of this string is a letter. */
    bool isletter() const                   { return characterfunctions::isletter (*data) != 0; }
    /** returns true if the first character of this string is a letter or digit. */
    bool isletterordigit() const            { return characterfunctions::isletterordigit (*data) != 0; }
    /** returns true if the first character of this string is upper-case. */
    bool isuppercase() const                { return characterfunctions::isuppercase ((beast_wchar) (std::uint8_t) *data) != 0; }
    /** returns true if the first character of this string is lower-case. */
    bool islowercase() const                { return characterfunctions::islowercase ((beast_wchar) (std::uint8_t) *data) != 0; }

    /** returns an upper-case version of the first character of this string. */
    beast_wchar touppercase() const noexcept { return characterfunctions::touppercase ((beast_wchar) (std::uint8_t) *data); }
    /** returns a lower-case version of the first character of this string. */
    beast_wchar tolowercase() const noexcept { return characterfunctions::tolowercase ((beast_wchar) (std::uint8_t) *data); }

    /** parses this string as a 32-bit integer. */
    int getintvalue32() const noexcept      { return atoi (data); }

    /** parses this string as a 64-bit integer. */
    std::int64_t getintvalue64() const noexcept
    {
       #if beast_linux || beast_android
        return atoll (data);
       #elif beast_windows
        return _atoi64 (data);
       #else
        return characterfunctions::getintvalue <std::int64_t, charpointer_ascii> (*this);
       #endif
    }

    /** parses this string as a floating point double. */
    double getdoublevalue() const noexcept  { return characterfunctions::getdoublevalue (*this); }

    /** returns the first non-whitespace character in the string. */
    charpointer_ascii findendofwhitespace() const noexcept   { return characterfunctions::findendofwhitespace (*this); }

    /** returns true if the given unicode character can be represented in this encoding. */
    static bool canrepresent (beast_wchar character) noexcept
    {
        return ((unsigned int) character) < (unsigned int) 128;
    }

    /** returns true if this data contains a valid string in this encoding. */
    static bool isvalidstring (const chartype* datatotest, int maxbytestoread)
    {
        while (--maxbytestoread >= 0)
        {
            if (((signed char) *datatotest) <= 0)
                return *datatotest == 0;

            ++datatotest;
        }

        return true;
    }

private:
    chartype* data;
};

}

#endif

