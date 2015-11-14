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

#ifndef beast_charpointer_utf8_h_included
#define beast_charpointer_utf8_h_included

#include <beast/config.h>
#include <beast/strings/characterfunctions.h>

#include <atomic>
#include <cstdlib>
#include <cstring>

namespace beast {

//==============================================================================
/**
    wraps a pointer to a null-terminated utf-8 character string, and provides
    various methods to operate on the data.
    @see charpointer_utf16, charpointer_utf32
*/
class charpointer_utf8
{
public:
    typedef char chartype;

    inline explicit charpointer_utf8 (const chartype* const rawpointer) noexcept
        : data (const_cast <chartype*> (rawpointer))
    {
    }

    inline charpointer_utf8 (const charpointer_utf8& other) noexcept
        : data (other.data)
    {
    }

    inline charpointer_utf8 operator= (charpointer_utf8 other) noexcept
    {
        data = other.data;
        return *this;
    }

    inline charpointer_utf8 operator= (const chartype* text) noexcept
    {
        data = const_cast <chartype*> (text);
        return *this;
    }

    /** this is a pointer comparison, it doesn't compare the actual text. */
    inline bool operator== (charpointer_utf8 other) const noexcept      { return data == other.data; }
    inline bool operator!= (charpointer_utf8 other) const noexcept      { return data != other.data; }
    inline bool operator<= (charpointer_utf8 other) const noexcept      { return data <= other.data; }
    inline bool operator<  (charpointer_utf8 other) const noexcept      { return data <  other.data; }
    inline bool operator>= (charpointer_utf8 other) const noexcept      { return data >= other.data; }
    inline bool operator>  (charpointer_utf8 other) const noexcept      { return data >  other.data; }

    /** returns the address that this pointer is pointing to. */
    inline chartype* getaddress() const noexcept        { return data; }

    /** returns the address that this pointer is pointing to. */
    inline operator const chartype*() const noexcept    { return data; }

    /** returns true if this pointer is pointing to a null character. */
    inline bool isempty() const noexcept                { return *data == 0; }

    /** returns the unicode character that this pointer is pointing to. */
    beast_wchar operator*() const noexcept
    {
        const signed char byte = (signed char) *data;

        if (byte >= 0)
            return (beast_wchar) (std::uint8_t) byte;

        std::uint32_t n = (std::uint32_t) (std::uint8_t) byte;
        std::uint32_t mask = 0x7f;
        std::uint32_t bit = 0x40;
        size_t numextravalues = 0;

        while ((n & bit) != 0 && bit > 0x10)
        {
            mask >>= 1;
            ++numextravalues;
            bit >>= 1;
        }

        n &= mask;

        for (size_t i = 1; i <= numextravalues; ++i)
        {
            const std::uint8_t nextbyte = (std::uint8_t) data [i];

            if ((nextbyte & 0xc0) != 0x80)
                break;

            n <<= 6;
            n |= (nextbyte & 0x3f);
        }

        return (beast_wchar) n;
    }

    /** moves this pointer along to the next character in the string. */
    charpointer_utf8& operator++() noexcept
    {
        const signed char n = (signed char) *data++;

        if (n < 0)
        {
            beast_wchar bit = 0x40;

            while ((n & bit) != 0 && bit > 0x8)
            {
                ++data;
                bit >>= 1;
            }
        }

        return *this;
    }

    /** moves this pointer back to the previous character in the string. */
    charpointer_utf8 operator--() noexcept
    {
        int count = 0;

        while ((*--data & 0xc0) == 0x80 && ++count < 4)
        {}

        return *this;
    }

    /** returns the character that this pointer is currently pointing to, and then
        advances the pointer to point to the next character. */
    beast_wchar getandadvance() noexcept
    {
        const signed char byte = (signed char) *data++;

        if (byte >= 0)
            return (beast_wchar) (std::uint8_t) byte;

        std::uint32_t n = (std::uint32_t) (std::uint8_t) byte;
        std::uint32_t mask = 0x7f;
        std::uint32_t bit = 0x40;
        int numextravalues = 0;

        while ((n & bit) != 0 && bit > 0x8)
        {
            mask >>= 1;
            ++numextravalues;
            bit >>= 1;
        }

        n &= mask;

        while (--numextravalues >= 0)
        {
            const std::uint32_t nextbyte = (std::uint32_t) (std::uint8_t) *data++;

            if ((nextbyte & 0xc0) != 0x80)
                break;

            n <<= 6;
            n |= (nextbyte & 0x3f);
        }

        return (beast_wchar) n;
    }

    /** moves this pointer along to the next character in the string. */
    charpointer_utf8 operator++ (int) noexcept
    {
        charpointer_utf8 temp (*this);
        ++*this;
        return temp;
    }

    /** moves this pointer forwards by the specified number of characters. */
    void operator+= (int numtoskip) noexcept
    {
        if (numtoskip < 0)
        {
            while (++numtoskip <= 0)
                --*this;
        }
        else
        {
            while (--numtoskip >= 0)
                ++*this;
        }
    }

    /** moves this pointer backwards by the specified number of characters. */
    void operator-= (int numtoskip) noexcept
    {
        operator+= (-numtoskip);
    }

    /** returns the character at a given character index from the start of the string. */
    beast_wchar operator[] (int characterindex) const noexcept
    {
        charpointer_utf8 p (*this);
        p += characterindex;
        return *p;
    }

    /** returns a pointer which is moved forwards from this one by the specified number of characters. */
    charpointer_utf8 operator+ (int numtoskip) const noexcept
    {
        charpointer_utf8 p (*this);
        p += numtoskip;
        return p;
    }

    /** returns a pointer which is moved backwards from this one by the specified number of characters. */
    charpointer_utf8 operator- (int numtoskip) const noexcept
    {
        charpointer_utf8 p (*this);
        p += -numtoskip;
        return p;
    }

    /** returns the number of characters in this string. */
    size_t length() const noexcept
    {
        const chartype* d = data;
        size_t count = 0;

        for (;;)
        {
            const std::uint32_t n = (std::uint32_t) (std::uint8_t) *d++;

            if ((n & 0x80) != 0)
            {
                std::uint32_t bit = 0x40;

                while ((n & bit) != 0)
                {
                    ++d;
                    bit >>= 1;

                    if (bit == 0)
                        break; // illegal utf-8 sequence
                }
            }
            else if (n == 0)
                break;

            ++count;
        }

        return count;
    }

    /** returns the number of characters in this string, or the given value, whichever is lower. */
    size_t lengthupto (const size_t maxcharstocount) const noexcept
    {
        return characterfunctions::lengthupto (*this, maxcharstocount);
    }

    /** returns the number of characters in this string, or up to the given end pointer, whichever is lower. */
    size_t lengthupto (const charpointer_utf8 end) const noexcept
    {
        return characterfunctions::lengthupto (*this, end);
    }

    /** returns the number of bytes that are used to represent this string.
        this includes the terminating null character.
    */
    size_t sizeinbytes() const noexcept
    {
        bassert (data != nullptr);
        return strlen (data) + 1;
    }

    /** returns the number of bytes that would be needed to represent the given
        unicode character in this encoding format.
    */
    static size_t getbytesrequiredfor (const beast_wchar chartowrite) noexcept
    {
        size_t num = 1;
        const std::uint32_t c = (std::uint32_t) chartowrite;

        if (c >= 0x80)
        {
            ++num;
            if (c >= 0x800)
            {
                ++num;
                if (c >= 0x10000)
                    ++num;
            }
        }

        return num;
    }

    /** returns the number of bytes that would be needed to represent the given
        string in this encoding format.
        the value returned does not include the terminating null character.
    */
    template <class charpointer>
    static size_t getbytesrequiredfor (charpointer text) noexcept
    {
        size_t count = 0;
        beast_wchar n;

        while ((n = text.getandadvance()) != 0)
            count += getbytesrequiredfor (n);

        return count;
    }

    /** returns a pointer to the null character that terminates this string. */
    charpointer_utf8 findterminatingnull() const noexcept
    {
        return charpointer_utf8 (data + strlen (data));
    }

    /** writes a unicode character to this string, and advances this pointer to point to the next position. */
    void write (const beast_wchar chartowrite) noexcept
    {
        const std::uint32_t c = (std::uint32_t) chartowrite;

        if (c >= 0x80)
        {
            int numextrabytes = 1;
            if (c >= 0x800)
            {
                ++numextrabytes;
                if (c >= 0x10000)
                    ++numextrabytes;
            }

            *data++ = (chartype) ((std::uint32_t) (0xff << (7 - numextrabytes)) | (c >> (numextrabytes * 6)));

            while (--numextrabytes >= 0)
                *data++ = (chartype) (0x80 | (0x3f & (c >> (numextrabytes * 6))));
        }
        else
        {
            *data++ = (chartype) c;
        }
    }

    /** writes a null character to this string (leaving the pointer's position unchanged). */
    inline void writenull() const noexcept
    {
        *data = 0;
    }

    /** copies a source string to this pointer, advancing this pointer as it goes. */
    template <typename charpointer>
    void writeall (const charpointer src) noexcept
    {
        characterfunctions::copyall (*this, src);
    }

    /** copies a source string to this pointer, advancing this pointer as it goes. */
    void writeall (const charpointer_utf8 src) noexcept
    {
        const chartype* s = src.data;

        while ((*data = *s) != 0)
        {
            ++data;
            ++s;
        }
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

    /** compares this string with another one, up to a specified number of characters. */
    template <typename charpointer>
    int compareupto (const charpointer other, const int maxchars) const noexcept
    {
        return characterfunctions::compareupto (*this, other, maxchars);
    }

    /** compares this string with another one. */
    template <typename charpointer>
    int compareignorecase (const charpointer other) const noexcept
    {
        return characterfunctions::compareignorecase (*this, other);
    }

    /** compares this string with another one. */
    int compareignorecase (const charpointer_utf8 other) const noexcept
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
        return characterfunctions::indexofchar (*this, chartofind);
    }

    /** returns the character index of a unicode character, or -1 if it isn't found. */
    int indexof (const beast_wchar chartofind, const bool ignorecase) const noexcept
    {
        return ignorecase ? characterfunctions::indexofcharignorecase (*this, chartofind)
                          : characterfunctions::indexofchar (*this, chartofind);
    }

    /** returns true if the first character of this string is whitespace. */
    bool iswhitespace() const noexcept      { return *data == ' ' || (*data <= 13 && *data >= 9); }
    /** returns true if the first character of this string is a digit. */
    bool isdigit() const noexcept           { return *data >= '0' && *data <= '9'; }
    /** returns true if the first character of this string is a letter. */
    bool isletter() const noexcept          { return characterfunctions::isletter (operator*()) != 0; }
    /** returns true if the first character of this string is a letter or digit. */
    bool isletterordigit() const noexcept   { return characterfunctions::isletterordigit (operator*()) != 0; }
    /** returns true if the first character of this string is upper-case. */
    bool isuppercase() const noexcept       { return characterfunctions::isuppercase (operator*()) != 0; }
    /** returns true if the first character of this string is lower-case. */
    bool islowercase() const noexcept       { return characterfunctions::islowercase (operator*()) != 0; }

    /** returns an upper-case version of the first character of this string. */
    beast_wchar touppercase() const noexcept { return characterfunctions::touppercase (operator*()); }
    /** returns a lower-case version of the first character of this string. */
    beast_wchar tolowercase() const noexcept { return characterfunctions::tolowercase (operator*()); }

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
        return characterfunctions::getintvalue <std::int64_t, charpointer_utf8> (*this);
       #endif
    }

    /** parses this string as a floating point double. */
    double getdoublevalue() const noexcept  { return characterfunctions::getdoublevalue (*this); }

    /** returns the first non-whitespace character in the string. */
    charpointer_utf8 findendofwhitespace() const noexcept   { return characterfunctions::findendofwhitespace (*this); }

    /** returns true if the given unicode character can be represented in this encoding. */
    static bool canrepresent (beast_wchar character) noexcept
    {
        return ((unsigned int) character) < (unsigned int) 0x10ffff;
    }

    /** returns true if this data contains a valid string in this encoding. */
    static bool isvalidstring (const chartype* datatotest, int maxbytestoread)
    {
        while (--maxbytestoread >= 0 && *datatotest != 0)
        {
            const signed char byte = (signed char) *datatotest++;

            if (byte < 0)
            {
                std::uint8_t bit = 0x40;
                int numextravalues = 0;

                while ((byte & bit) != 0)
                {
                    if (bit < 8)
                        return false;

                    ++numextravalues;
                    bit >>= 1;

                    if (bit == 8 && (numextravalues > maxbytestoread
                                       || *charpointer_utf8 (datatotest - 1) > 0x10ffff))
                        return false;
                }

                maxbytestoread -= numextravalues;
                if (maxbytestoread < 0)
                    return false;

                while (--numextravalues >= 0)
                    if ((*datatotest++ & 0xc0) != 0x80)
                        return false;
            }
        }

        return true;
    }

    /** atomically swaps this pointer for a new value, returning the previous value. */
    charpointer_utf8 atomicswap (const charpointer_utf8 newvalue)
    {
        return charpointer_utf8 (reinterpret_cast <std::atomic<chartype*>&> (data).exchange (newvalue.data));
    }

    /** these values are the byte-order mark (bom) values for a utf-8 stream. */
    enum
    {
        byteordermark1 = 0xef,
        byteordermark2 = 0xbb,
        byteordermark3 = 0xbf
    };

    /** returns true if the first three bytes in this pointer are the utf8 byte-order mark (bom).
        the pointer must not be null, and must point to at least 3 valid bytes.
    */
    static bool isbyteordermark (const void* possiblebyteorder) noexcept
    {
        bassert (possiblebyteorder != nullptr);
        const std::uint8_t* const c = static_cast<const std::uint8_t*> (possiblebyteorder);

        return c[0] == (std::uint8_t) byteordermark1
            && c[1] == (std::uint8_t) byteordermark2
            && c[2] == (std::uint8_t) byteordermark3;
    }

private:
    chartype* data;
};

}

#endif

