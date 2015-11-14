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

#ifndef beast_charpointer_utf16_h_included
#define beast_charpointer_utf16_h_included

#include <beast/config.h>
#include <beast/strings/characterfunctions.h>

#include <atomic>
#include <cstdint>

namespace beast {

//==============================================================================
/**
    wraps a pointer to a null-terminated utf-16 character string, and provides
    various methods to operate on the data.
    @see charpointer_utf8, charpointer_utf32
*/
class charpointer_utf16
{
public:
   #if beast_native_wchar_is_utf16
    typedef wchar_t chartype;
   #else
    typedef std::int16_t chartype;
   #endif

    inline explicit charpointer_utf16 (const chartype* const rawpointer) noexcept
        : data (const_cast <chartype*> (rawpointer))
    {
    }

    inline charpointer_utf16 (const charpointer_utf16& other) noexcept
        : data (other.data)
    {
    }

    inline charpointer_utf16 operator= (charpointer_utf16 other) noexcept
    {
        data = other.data;
        return *this;
    }

    inline charpointer_utf16 operator= (const chartype* text) noexcept
    {
        data = const_cast <chartype*> (text);
        return *this;
    }

    /** this is a pointer comparison, it doesn't compare the actual text. */
    inline bool operator== (charpointer_utf16 other) const noexcept     { return data == other.data; }
    inline bool operator!= (charpointer_utf16 other) const noexcept     { return data != other.data; }
    inline bool operator<= (charpointer_utf16 other) const noexcept     { return data <= other.data; }
    inline bool operator<  (charpointer_utf16 other) const noexcept     { return data <  other.data; }
    inline bool operator>= (charpointer_utf16 other) const noexcept     { return data >= other.data; }
    inline bool operator>  (charpointer_utf16 other) const noexcept     { return data >  other.data; }

    /** returns the address that this pointer is pointing to. */
    inline chartype* getaddress() const noexcept        { return data; }

    /** returns the address that this pointer is pointing to. */
    inline operator const chartype*() const noexcept    { return data; }

    /** returns true if this pointer is pointing to a null character. */
    inline bool isempty() const noexcept                { return *data == 0; }

    /** returns the unicode character that this pointer is pointing to. */
    beast_wchar operator*() const noexcept
    {
        std::uint32_t n = (std::uint32_t) (std::uint16_t) *data;

        if (n >= 0xd800 && n <= 0xdfff && ((std::uint32_t) (std::uint16_t) data[1]) >= 0xdc00)
            n = 0x10000 + (((n - 0xd800) << 10) | (((std::uint32_t) (std::uint16_t) data[1]) - 0xdc00));

        return (beast_wchar) n;
    }

    /** moves this pointer along to the next character in the string. */
    charpointer_utf16 operator++() noexcept
    {
        const beast_wchar n = *data++;

        if (n >= 0xd800 && n <= 0xdfff && ((std::uint32_t) (std::uint16_t) *data) >= 0xdc00)
            ++data;

        return *this;
    }

    /** moves this pointer back to the previous character in the string. */
    charpointer_utf16 operator--() noexcept
    {
        const beast_wchar n = *--data;

        if (n >= 0xdc00 && n <= 0xdfff)
            --data;

        return *this;
    }

    /** returns the character that this pointer is currently pointing to, and then
        advances the pointer to point to the next character. */
    beast_wchar getandadvance() noexcept
    {
        std::uint32_t n = (std::uint32_t) (std::uint16_t) *data++;

        if (n >= 0xd800 && n <= 0xdfff && ((std::uint32_t) (std::uint16_t) *data) >= 0xdc00)
            n = 0x10000 + ((((n - 0xd800) << 10) | (((std::uint32_t) (std::uint16_t) *data++) - 0xdc00)));

        return (beast_wchar) n;
    }

    /** moves this pointer along to the next character in the string. */
    charpointer_utf16 operator++ (int) noexcept
    {
        charpointer_utf16 temp (*this);
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
    beast_wchar operator[] (const int characterindex) const noexcept
    {
        charpointer_utf16 p (*this);
        p += characterindex;
        return *p;
    }

    /** returns a pointer which is moved forwards from this one by the specified number of characters. */
    charpointer_utf16 operator+ (const int numtoskip) const noexcept
    {
        charpointer_utf16 p (*this);
        p += numtoskip;
        return p;
    }

    /** returns a pointer which is moved backwards from this one by the specified number of characters. */
    charpointer_utf16 operator- (const int numtoskip) const noexcept
    {
        charpointer_utf16 p (*this);
        p += -numtoskip;
        return p;
    }

    /** writes a unicode character to this string, and advances this pointer to point to the next position. */
    void write (beast_wchar chartowrite) noexcept
    {
        if (chartowrite >= 0x10000)
        {
            chartowrite -= 0x10000;
            *data++ = (chartype) (0xd800 + (chartowrite >> 10));
            *data++ = (chartype) (0xdc00 + (chartowrite & 0x3ff));
        }
        else
        {
            *data++ = (chartype) chartowrite;
        }
    }

    /** writes a null character to this string (leaving the pointer's position unchanged). */
    inline void writenull() const noexcept
    {
        *data = 0;
    }

    /** returns the number of characters in this string. */
    size_t length() const noexcept
    {
        const chartype* d = data;
        size_t count = 0;

        for (;;)
        {
            const int n = *d++;

            if (n >= 0xd800 && n <= 0xdfff)
            {
                if (*d++ == 0)
                    break;
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
    size_t lengthupto (const charpointer_utf16 end) const noexcept
    {
        return characterfunctions::lengthupto (*this, end);
    }

    /** returns the number of bytes that are used to represent this string.
        this includes the terminating null character.
    */
    size_t sizeinbytes() const noexcept
    {
        return sizeof (chartype) * (findnullindex (data) + 1);
    }

    /** returns the number of bytes that would be needed to represent the given
        unicode character in this encoding format.
    */
    static size_t getbytesrequiredfor (const beast_wchar chartowrite) noexcept
    {
        return (chartowrite >= 0x10000) ? (sizeof (chartype) * 2) : sizeof (chartype);
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
    charpointer_utf16 findterminatingnull() const noexcept
    {
        const chartype* t = data;

        while (*t != 0)
            ++t;

        return charpointer_utf16 (t);
    }

    /** copies a source string to this pointer, advancing this pointer as it goes. */
    template <typename charpointer>
    void writeall (const charpointer src) noexcept
    {
        characterfunctions::copyall (*this, src);
    }

    /** copies a source string to this pointer, advancing this pointer as it goes. */
    void writeall (const charpointer_utf16 src) noexcept
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

    /** compares this string with another one, up to a specified number of characters. */
    template <typename charpointer>
    int compareignorecaseupto (const charpointer other, const int maxchars) const noexcept
    {
        return characterfunctions::compareignorecaseupto (*this, other, maxchars);
    }

   #if beast_windows && ! doxygen
    int compareignorecase (const charpointer_utf16 other) const noexcept
    {
        return _wcsicmp (data, other.data);
    }

    int compareignorecaseupto (const charpointer_utf16 other, int maxchars) const noexcept
    {
        return _wcsnicmp (data, other.data, (size_t) maxchars);
    }

    int indexof (const charpointer_utf16 stringtofind) const noexcept
    {
        const chartype* const t = wcsstr (data, stringtofind.getaddress());
        return t == nullptr ? -1 : (int) (t - data);
    }
   #endif

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
    bool iswhitespace() const noexcept      { return characterfunctions::iswhitespace (operator*()) != 0; }
    /** returns true if the first character of this string is a digit. */
    bool isdigit() const noexcept           { return characterfunctions::isdigit (operator*()) != 0; }
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
    int getintvalue32() const noexcept
    {
       #if beast_windows
        return _wtoi (data);
       #else
        return characterfunctions::getintvalue <int, charpointer_utf16> (*this);
       #endif
    }

    /** parses this string as a 64-bit integer. */
    std::int64_t getintvalue64() const noexcept
    {
       #if beast_windows
        return _wtoi64 (data);
       #else
        return characterfunctions::getintvalue <std::int64_t, charpointer_utf16> (*this);
       #endif
    }

    /** parses this string as a floating point double. */
    double getdoublevalue() const noexcept  { return characterfunctions::getdoublevalue (*this); }

    /** returns the first non-whitespace character in the string. */
    charpointer_utf16 findendofwhitespace() const noexcept   { return characterfunctions::findendofwhitespace (*this); }

    /** returns true if the given unicode character can be represented in this encoding. */
    static bool canrepresent (beast_wchar character) noexcept
    {
        return ((unsigned int) character) < (unsigned int) 0x10ffff
                 && (((unsigned int) character) < 0xd800 || ((unsigned int) character) > 0xdfff);
    }

    /** returns true if this data contains a valid string in this encoding. */
    static bool isvalidstring (const chartype* datatotest, int maxbytestoread)
    {
        maxbytestoread /= sizeof (chartype);

        while (--maxbytestoread >= 0 && *datatotest != 0)
        {
            const std::uint32_t n = (std::uint32_t) (std::uint16_t) *datatotest++;

            if (n >= 0xd800)
            {
                if (n > 0x10ffff)
                    return false;

                if (n <= 0xdfff)
                {
                    if (n > 0xdc00)
                        return false;

                    const std::uint32_t nextchar = (std::uint32_t) (std::uint16_t) *datatotest++;

                    if (nextchar < 0xdc00 || nextchar > 0xdfff)
                        return false;
                }
            }
        }

        return true;
    }

    /** atomically swaps this pointer for a new value, returning the previous value. */
    charpointer_utf16 atomicswap (const charpointer_utf16 newvalue)
    {
        return charpointer_utf16 (reinterpret_cast <std::atomic<chartype*>&> (data).exchange (newvalue.data));
    }

    /** these values are the byte-order-mark (bom) values for a utf-16 stream. */
    enum
    {
        byteordermarkbe1 = 0xfe,
        byteordermarkbe2 = 0xff,
        byteordermarkle1 = 0xff,
        byteordermarkle2 = 0xfe
    };

    /** returns true if the first pair of bytes in this pointer are the utf16 byte-order mark (big endian).
        the pointer must not be null, and must contain at least two valid bytes.
    */
    static bool isbyteordermarkbigendian (const void* possiblebyteorder) noexcept
    {
        bassert (possiblebyteorder != nullptr);
        const std::uint8_t* const c = static_cast<const std::uint8_t*> (possiblebyteorder);

        return c[0] == (std::uint8_t) byteordermarkbe1
            && c[1] == (std::uint8_t) byteordermarkbe2;
    }

    /** returns true if the first pair of bytes in this pointer are the utf16 byte-order mark (little endian).
        the pointer must not be null, and must contain at least two valid bytes.
    */
    static bool isbyteordermarklittleendian (const void* possiblebyteorder) noexcept
    {
        bassert (possiblebyteorder != nullptr);
        const std::uint8_t* const c = static_cast<const std::uint8_t*> (possiblebyteorder);

        return c[0] == (std::uint8_t) byteordermarkle1
            && c[1] == (std::uint8_t) byteordermarkle2;
    }

private:
    chartype* data;

    static unsigned int findnullindex (const chartype* const t) noexcept
    {
        unsigned int n = 0;

        while (t[n] != 0)
            ++n;

        return n;
    }
};

}

#endif

