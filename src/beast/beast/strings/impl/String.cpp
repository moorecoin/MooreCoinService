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

#if beast_include_beastconfig
#include "../../beastconfig.h"
#endif

#include <beast/strings/string.h>
#include <beast/strings/newline.h>

#include <beast/byteorder.h>
#include <beast/memory.h>
#include <beast/arithmetic.h>
#include <beast/heapblock.h>

#include <stdarg.h>

#include <algorithm>
#include <atomic>
#include <cstring>

namespace beast {

#if beast_msvc
 #pragma warning (push)
 #pragma warning (disable: 4514 4996)
#endif

newline newline;

#if defined (beast_strings_are_unicode) && ! beast_strings_are_unicode
 #error "beast_strings_are_unicode is deprecated! all strings are now unicode by default."
#endif

static inline charpointer_wchar_t casttocharpointer_wchar_t (const void* t) noexcept
{
    return charpointer_wchar_t (static_cast <const charpointer_wchar_t::chartype*> (t));
}

//==============================================================================
// let me know if any of these assertions fail on your system!
#if beast_native_wchar_is_utf8
static_assert (sizeof (wchar_t) == 1,
    "the size of a wchar_t should be exactly 1 byte for utf-8");
#elif beast_native_wchar_is_utf16
static_assert (sizeof (wchar_t) == 2,
    "the size of a wchar_t should be exactly 2 bytes for utf-16");
#elif beast_native_wchar_is_utf32
static_assert (sizeof (wchar_t) == 4,
    "the size of a wchar_t should be exactly 4 bytes for utf-32");
#else
#error "the size of a wchar_t is not known!"
#endif

//==============================================================================
class stringholder
{
public:
    stringholder() noexcept
        : refcount (0x3fffffff), allocatednumbytes (sizeof (*text))
    {
        text[0] = 0;
    }

    typedef string::charpointertype charpointertype;
    typedef string::charpointertype::chartype chartype;

    //==============================================================================
    static charpointertype createuninitialisedbytes (const size_t numbytes)
    {
        stringholder* const s = reinterpret_cast <stringholder*> (
            new char [sizeof (stringholder) - sizeof (chartype) + numbytes]);
        s->refcount.store (0);
        s->allocatednumbytes = numbytes;
        return charpointertype (s->text);
    }

    template <class charpointer>
    static charpointertype createfromcharpointer (const charpointer text)
    {
        if (text.getaddress() == nullptr || text.isempty())
            return getempty();

        charpointer t (text);
        size_t bytesneeded = sizeof (chartype);

        while (! t.isempty())
            bytesneeded += charpointertype::getbytesrequiredfor (t.getandadvance());

        const charpointertype dest (createuninitialisedbytes (bytesneeded));
        charpointertype (dest).writeall (text);
        return dest;
    }

    template <class charpointer>
    static charpointertype createfromcharpointer (const charpointer text, size_t maxchars)
    {
        if (text.getaddress() == nullptr || text.isempty() || maxchars == 0)
            return getempty();

        charpointer end (text);
        size_t numchars = 0;
        size_t bytesneeded = sizeof (chartype);

        while (numchars < maxchars && ! end.isempty())
        {
            bytesneeded += charpointertype::getbytesrequiredfor (end.getandadvance());
            ++numchars;
        }

        const charpointertype dest (createuninitialisedbytes (bytesneeded));
        charpointertype (dest).writewithcharlimit (text, (int) numchars + 1);
        return dest;
    }

    template <class charpointer>
    static charpointertype createfromcharpointer (const charpointer start, const charpointer end)
    {
        if (start.getaddress() == nullptr || start.isempty())
            return getempty();

        charpointer e (start);
        int numchars = 0;
        size_t bytesneeded = sizeof (chartype);

        while (e < end && ! e.isempty())
        {
            bytesneeded += charpointertype::getbytesrequiredfor (e.getandadvance());
            ++numchars;
        }

        const charpointertype dest (createuninitialisedbytes (bytesneeded));
        charpointertype (dest).writewithcharlimit (start, numchars + 1);
        return dest;
    }

    static charpointertype createfromcharpointer (const charpointertype start, const charpointertype end)
    {
        if (start.getaddress() == nullptr || start.isempty())
            return getempty();

        const size_t numbytes = (size_t) (reinterpret_cast<const char*> (end.getaddress())
                                           - reinterpret_cast<const char*> (start.getaddress()));
        const charpointertype dest (createuninitialisedbytes (numbytes + sizeof (chartype)));
        memcpy (dest.getaddress(), start, numbytes);
        dest.getaddress()[numbytes / sizeof (chartype)] = 0;
        return dest;
    }

    static charpointertype createfromfixedlength (const char* const src, const size_t numchars)
    {
        const charpointertype dest (createuninitialisedbytes (numchars * sizeof (chartype) + sizeof (chartype)));
        charpointertype (dest).writewithcharlimit (charpointer_utf8 (src), (int) (numchars + 1));
        return dest;
    }

    static inline charpointertype getempty() noexcept
    {
        return charpointertype (empty.text);
    }

    //==============================================================================
    static void retain (const charpointertype text) noexcept
    {
        ++(bufferfromtext (text)->refcount);
    }

    static inline void release (stringholder* const b) noexcept
    {
        if (--(b->refcount) == -1 && b != &empty)
            delete[] reinterpret_cast <char*> (b);
    }

    static void release (const charpointertype text) noexcept
    {
        release (bufferfromtext (text));
    }

    //==============================================================================
    static charpointertype makeunique (const charpointertype text)
    {
        stringholder* const b = bufferfromtext (text);

        if (b->refcount.load() <= 0)
            return text;

        charpointertype newtext (createuninitialisedbytes (b->allocatednumbytes));
        memcpy (newtext.getaddress(), text.getaddress(), b->allocatednumbytes);
        release (b);

        return newtext;
    }

    static charpointertype makeuniquewithbytesize (const charpointertype text, size_t numbytes)
    {
        stringholder* const b = bufferfromtext (text);

        if (b->refcount.load() <= 0 && b->allocatednumbytes >= numbytes)
            return text;

        charpointertype newtext (createuninitialisedbytes (std::max (b->allocatednumbytes, numbytes)));
        memcpy (newtext.getaddress(), text.getaddress(), b->allocatednumbytes);
        release (b);

        return newtext;
    }

    static size_t getallocatednumbytes (const charpointertype text) noexcept
    {
        return bufferfromtext (text)->allocatednumbytes;
    }

    //==============================================================================
    std::atomic<int> refcount;
    size_t allocatednumbytes;
    chartype text[1];

    static stringholder empty;

private:
    static inline stringholder* bufferfromtext (const charpointertype text) noexcept
    {
        // (can't use offsetof() here because of warnings about this not being a pod)
        auto const text_offset = reinterpret_cast<char*>(empty.text) - reinterpret_cast<char*>(&empty);
        auto const tmp = reinterpret_cast<char*>(text.getaddress()) - text_offset;
        return static_cast<stringholder*>(std::memmove(tmp, tmp, 0));
    }
};

stringholder stringholder::empty;
const string string::empty;

//------------------------------------------------------------------------------

stringcharpointertype numbertostringconverters::createfromfixedlength (
    const char* const src, const size_t numchars)
{
    return stringholder::createfromfixedlength (src, numchars);
}

//------------------------------------------------------------------------------

void string::preallocatebytes (const size_t numbytesneeded)
{
    text = stringholder::makeuniquewithbytesize (
        text, numbytesneeded + sizeof (charpointertype::chartype));
}

//==============================================================================

string::string() noexcept  : text (stringholder::getempty())
{
}

string::~string() noexcept
{
    stringholder::release (text);
}

string::string (const string& other) noexcept
    : text (other.text)
{
    stringholder::retain (text);
}

void string::swapwith (string& other) noexcept
{
    std::swap (text, other.text);
}

string& string::operator= (const string& other) noexcept
{
    stringholder::retain (other.text);
    stringholder::release (text.atomicswap (other.text));
    return *this;
}

#if beast_compiler_supports_move_semantics
string::string (string&& other) noexcept
    : text (other.text)
{
    other.text = stringholder::getempty();
}

string& string::operator= (string&& other) noexcept
{
    std::swap (text, other.text);
    return *this;
}
#endif

inline string::preallocationbytes::preallocationbytes (const size_t numbytes_) : numbytes (numbytes_) {}

string::string (const preallocationbytes& preallocationsize)
    : text (stringholder::createuninitialisedbytes (preallocationsize.numbytes + sizeof (charpointertype::chartype)))
{
}

//==============================================================================
string::string (const char* const t)
    : text (stringholder::createfromcharpointer (charpointer_ascii (t)))
{
    /*  if you get an assertion here, then you're trying to create a string from 8-bit data
        that contains values greater than 127. these can not be correctly converted to unicode
        because there's no way for the string class to know what encoding was used to
        create them. the source data could be utf-8, ascii or one of many local code-pages.

        to get around this problem, you must be more explicit when you pass an ambiguous 8-bit
        string to the string class - so for example if your source data is actually utf-8,
        you'd call string (charpointer_utf8 ("my utf8 string..")), and it would be able to
        correctly convert the multi-byte characters to unicode. it's *highly* recommended that
        you use utf-8 with escape characters in your source code to represent extended characters,
        because there's no other way to represent these strings in a way that isn't dependent on
        the compiler, source code editor and platform.
    */
    bassert (t == nullptr || charpointer_ascii::isvalidstring (t, std::numeric_limits<int>::max()));
}

string::string (const char* const t, const size_t maxchars)
    : text (stringholder::createfromcharpointer (charpointer_ascii (t), maxchars))
{
    /*  if you get an assertion here, then you're trying to create a string from 8-bit data
        that contains values greater than 127. these can not be correctly converted to unicode
        because there's no way for the string class to know what encoding was used to
        create them. the source data could be utf-8, ascii or one of many local code-pages.

        to get around this problem, you must be more explicit when you pass an ambiguous 8-bit
        string to the string class - so for example if your source data is actually utf-8,
        you'd call string (charpointer_utf8 ("my utf8 string..")), and it would be able to
        correctly convert the multi-byte characters to unicode. it's *highly* recommended that
        you use utf-8 with escape characters in your source code to represent extended characters,
        because there's no other way to represent these strings in a way that isn't dependent on
        the compiler, source code editor and platform.
    */
    bassert (t == nullptr || charpointer_ascii::isvalidstring (t, (int) maxchars));
}

string::string (const wchar_t* const t)      : text (stringholder::createfromcharpointer (casttocharpointer_wchar_t (t))) {}
string::string (const charpointer_utf8  t)   : text (stringholder::createfromcharpointer (t)) {}
string::string (const charpointer_utf16 t)   : text (stringholder::createfromcharpointer (t)) {}
string::string (const charpointer_utf32 t)   : text (stringholder::createfromcharpointer (t)) {}
string::string (const charpointer_ascii t)   : text (stringholder::createfromcharpointer (t)) {}

string::string (const charpointer_utf8  t, const size_t maxchars)   : text (stringholder::createfromcharpointer (t, maxchars)) {}
string::string (const charpointer_utf16 t, const size_t maxchars)   : text (stringholder::createfromcharpointer (t, maxchars)) {}
string::string (const charpointer_utf32 t, const size_t maxchars)   : text (stringholder::createfromcharpointer (t, maxchars)) {}
string::string (const wchar_t* const t, size_t maxchars)            : text (stringholder::createfromcharpointer (casttocharpointer_wchar_t (t), maxchars)) {}

string::string (const charpointer_utf8  start, const charpointer_utf8  end)  : text (stringholder::createfromcharpointer (start, end)) {}
string::string (const charpointer_utf16 start, const charpointer_utf16 end)  : text (stringholder::createfromcharpointer (start, end)) {}
string::string (const charpointer_utf32 start, const charpointer_utf32 end)  : text (stringholder::createfromcharpointer (start, end)) {}

string::string (const std::string& s) : text (stringholder::createfromfixedlength (s.data(), s.size())) {}

string string::chartostring (const beast_wchar character)
{
    string result (preallocationbytes (charpointertype::getbytesrequiredfor (character)));
    charpointertype t (result.text);
    t.write (character);
    t.writenull();
    return result;
}

//==============================================================================

string::string (const int number)            : text (numbertostringconverters::createfrominteger (number)) {}
string::string (const unsigned int number)   : text (numbertostringconverters::createfrominteger (number)) {}
string::string (const short number)          : text (numbertostringconverters::createfrominteger ((int) number)) {}
string::string (const unsigned short number) : text (numbertostringconverters::createfrominteger ((unsigned int) number)) {}
string::string (const long number)           : text (numbertostringconverters::createfrominteger (number)) {}
string::string (const unsigned long number)  : text (numbertostringconverters::createfrominteger (number)) {}
string::string (const long long number)      : text (numbertostringconverters::createfrominteger (number)) {}
string::string (const unsigned long long number) : text (numbertostringconverters::createfrominteger (number)) {}

string::string (const float number)          : text (numbertostringconverters::createfromdouble ((double) number, 0)) {}
string::string (const double number)         : text (numbertostringconverters::createfromdouble (number, 0)) {}
string::string (const float number, const int numberofdecimalplaces)   : text (numbertostringconverters::createfromdouble ((double) number, numberofdecimalplaces)) {}
string::string (const double number, const int numberofdecimalplaces)  : text (numbertostringconverters::createfromdouble (number, numberofdecimalplaces)) {}

//==============================================================================
int string::length() const noexcept
{
    return (int) text.length();
}

size_t string::getbyteoffsetofend() const noexcept
{
    return (size_t) (((char*) text.findterminatingnull().getaddress()) - (char*) text.getaddress());
}

beast_wchar string::operator[] (int index) const noexcept
{
    bassert (index == 0 || (index > 0 && index <= (int) text.lengthupto ((size_t) index + 1)));
    return text [index];
}

//------------------------------------------------------------------------------

namespace detail {

template <typename type>
struct hashgenerator
{
    template <typename charpointer>
    static type calculate (charpointer t) noexcept
    {
        type result = type();

        while (! t.isempty())
            result = multiplier * result + t.getandadvance();

        return result;
    }

    enum { multiplier = sizeof (type) > 4 ? 101 : 31 };
};

}

int string::hashcode() const noexcept
{
    return detail::hashgenerator<int> ::calculate (text);
}

std::int64_t string::hashcode64() const noexcept
{
    return detail::hashgenerator<std::int64_t> ::calculate (text);
}

std::size_t string::hash() const noexcept
{
    return detail::hashgenerator<std::size_t>::calculate (text);
}

//==============================================================================
bool operator== (const string& s1, const string& s2) noexcept            { return s1.compare (s2) == 0; }
bool operator== (const string& s1, const char* const s2) noexcept        { return s1.compare (s2) == 0; }
bool operator== (const string& s1, const wchar_t* const s2) noexcept     { return s1.compare (s2) == 0; }
bool operator== (const string& s1, const charpointer_utf8 s2) noexcept   { return s1.getcharpointer().compare (s2) == 0; }
bool operator== (const string& s1, const charpointer_utf16 s2) noexcept  { return s1.getcharpointer().compare (s2) == 0; }
bool operator== (const string& s1, const charpointer_utf32 s2) noexcept  { return s1.getcharpointer().compare (s2) == 0; }
bool operator!= (const string& s1, const string& s2) noexcept            { return s1.compare (s2) != 0; }
bool operator!= (const string& s1, const char* const s2) noexcept        { return s1.compare (s2) != 0; }
bool operator!= (const string& s1, const wchar_t* const s2) noexcept     { return s1.compare (s2) != 0; }
bool operator!= (const string& s1, const charpointer_utf8 s2) noexcept   { return s1.getcharpointer().compare (s2) != 0; }
bool operator!= (const string& s1, const charpointer_utf16 s2) noexcept  { return s1.getcharpointer().compare (s2) != 0; }
bool operator!= (const string& s1, const charpointer_utf32 s2) noexcept  { return s1.getcharpointer().compare (s2) != 0; }
bool operator>  (const string& s1, const string& s2) noexcept            { return s1.compare (s2) > 0; }
bool operator<  (const string& s1, const string& s2) noexcept            { return s1.compare (s2) < 0; }
bool operator>= (const string& s1, const string& s2) noexcept            { return s1.compare (s2) >= 0; }
bool operator<= (const string& s1, const string& s2) noexcept            { return s1.compare (s2) <= 0; }

bool string::equalsignorecase (const wchar_t* const t) const noexcept
{
    return t != nullptr ? text.compareignorecase (casttocharpointer_wchar_t (t)) == 0
                        : isempty();
}

bool string::equalsignorecase (const char* const t) const noexcept
{
    return t != nullptr ? text.compareignorecase (charpointer_utf8 (t)) == 0
                        : isempty();
}

bool string::equalsignorecase (const string& other) const noexcept
{
    return text == other.text
            || text.compareignorecase (other.text) == 0;
}

int string::compare (const string& other) const noexcept           { return (text == other.text) ? 0 : text.compare (other.text); }
int string::compare (const char* const other) const noexcept       { return text.compare (charpointer_utf8 (other)); }
int string::compare (const wchar_t* const other) const noexcept    { return text.compare (casttocharpointer_wchar_t (other)); }
int string::compareignorecase (const string& other) const noexcept { return (text == other.text) ? 0 : text.compareignorecase (other.text); }

int string::comparelexicographically (const string& other) const noexcept
{
    charpointertype s1 (text);

    while (! (s1.isempty() || s1.isletterordigit()))
        ++s1;

    charpointertype s2 (other.text);

    while (! (s2.isempty() || s2.isletterordigit()))
        ++s2;

    return s1.compareignorecase (s2);
}

//==============================================================================
void string::append (const string& texttoappend, size_t maxcharstotake)
{
    appendcharpointer (texttoappend.text, maxcharstotake);
}

void string::appendcharpointer (const charpointertype texttoappend)
{
    appendcharpointer (texttoappend, texttoappend.findterminatingnull());
}

void string::appendcharpointer (const charpointertype startoftexttoappend,
                                const charpointertype endoftexttoappend)
{
    bassert (startoftexttoappend.getaddress() != nullptr && endoftexttoappend.getaddress() != nullptr);

    const int extrabytesneeded = getaddressdifference (endoftexttoappend.getaddress(),
                                                       startoftexttoappend.getaddress());
    bassert (extrabytesneeded >= 0);

    if (extrabytesneeded > 0)
    {
        const size_t byteoffsetofnull = getbyteoffsetofend();
        preallocatebytes (byteoffsetofnull + (size_t) extrabytesneeded);

        charpointertype::chartype* const newstringstart = addbytestopointer (text.getaddress(), (int) byteoffsetofnull);
        memcpy (newstringstart, startoftexttoappend.getaddress(), extrabytesneeded);
        charpointertype (addbytestopointer (newstringstart, extrabytesneeded)).writenull();
    }
}

string& string::operator+= (const wchar_t* const t)
{
    appendcharpointer (casttocharpointer_wchar_t (t));
    return *this;
}

string& string::operator+= (const char* const t)
{
    /*  if you get an assertion here, then you're trying to create a string from 8-bit data
        that contains values greater than 127. these can not be correctly converted to unicode
        because there's no way for the string class to know what encoding was used to
        create them. the source data could be utf-8, ascii or one of many local code-pages.

        to get around this problem, you must be more explicit when you pass an ambiguous 8-bit
        string to the string class - so for example if your source data is actually utf-8,
        you'd call string (charpointer_utf8 ("my utf8 string..")), and it would be able to
        correctly convert the multi-byte characters to unicode. it's *highly* recommended that
        you use utf-8 with escape characters in your source code to represent extended characters,
        because there's no other way to represent these strings in a way that isn't dependent on
        the compiler, source code editor and platform.
    */
    bassert (t == nullptr || charpointer_ascii::isvalidstring (t, std::numeric_limits<int>::max()));

    appendcharpointer (charpointer_ascii (t));
    return *this;
}

string& string::operator+= (const string& other)
{
    if (isempty())
        return operator= (other);

    appendcharpointer (other.text);
    return *this;
}

string& string::operator+= (const char ch)
{
    const char asstring[] = { ch, 0 };
    return operator+= (asstring);
}

string& string::operator+= (const wchar_t ch)
{
    const wchar_t asstring[] = { ch, 0 };
    return operator+= (asstring);
}

#if ! beast_native_wchar_is_utf32
string& string::operator+= (const beast_wchar ch)
{
    const beast_wchar asstring[] = { ch, 0 };
    appendcharpointer (charpointer_utf32 (asstring));
    return *this;
}
#endif

string& string::operator+= (const int number)
{
    char buffer [16];
    char* const end = buffer + numelementsinarray (buffer);
    char* const start = numbertostringconverters::numbertostring (end, number);

    const int numextrachars = (int) (end - start);

    if (numextrachars > 0)
    {
        const size_t byteoffsetofnull = getbyteoffsetofend();
        const size_t newbytesneeded = sizeof (charpointertype::chartype) + byteoffsetofnull
                                        + sizeof (charpointertype::chartype) * (size_t) numextrachars;

        text = stringholder::makeuniquewithbytesize (text, newbytesneeded);

        charpointertype newend (addbytestopointer (text.getaddress(), (int) byteoffsetofnull));
        newend.writewithcharlimit (charpointer_ascii (start), numextrachars);
    }

    return *this;
}

//==============================================================================
string operator+ (const char* const string1, const string& string2)
{
    string s (string1);
    return s += string2;
}

string operator+ (const wchar_t* const string1, const string& string2)
{
    string s (string1);
    return s += string2;
}

string operator+ (const char s1, const string& s2)       { return string::chartostring ((beast_wchar) (std::uint8_t) s1) + s2; }
string operator+ (const wchar_t s1, const string& s2)    { return string::chartostring (s1) + s2; }
#if ! beast_native_wchar_is_utf32
string operator+ (const beast_wchar s1, const string& s2) { return string::chartostring (s1) + s2; }
#endif

string operator+ (string s1, const string& s2)       { return s1 += s2; }
string operator+ (string s1, const char* const s2)   { return s1 += s2; }
string operator+ (string s1, const wchar_t* s2)      { return s1 += s2; }

string operator+ (string s1, const char s2)          { return s1 += s2; }
string operator+ (string s1, const wchar_t s2)       { return s1 += s2; }
#if ! beast_native_wchar_is_utf32
string operator+ (string s1, const beast_wchar s2)    { return s1 += s2; }
#endif

string& operator<< (string& s1, const char s2)             { return s1 += s2; }
string& operator<< (string& s1, const wchar_t s2)          { return s1 += s2; }
#if ! beast_native_wchar_is_utf32
string& operator<< (string& s1, const beast_wchar s2)       { return s1 += s2; }
#endif

string& operator<< (string& s1, const char* const s2)      { return s1 += s2; }
string& operator<< (string& s1, const wchar_t* const s2)   { return s1 += s2; }
string& operator<< (string& s1, const string& s2)          { return s1 += s2; }

string& operator<< (string& s1, const short number)        { return s1 += (int) number; }
string& operator<< (string& s1, const int number)          { return s1 += number; }
string& operator<< (string& s1, const long number)         { return s1 += (int) number; }
string& operator<< (string& s1, const long long number)    { return s1 << string (number); }
string& operator<< (string& s1, const float number)        { return s1 += string (number); }
string& operator<< (string& s1, const double number)       { return s1 += string (number); }

string& operator<< (string& string1, const newline&)
{
    return string1 += newline::getdefault();
}

//==============================================================================
int string::indexofchar (const beast_wchar character) const noexcept
{
    return text.indexof (character);
}

int string::indexofchar (const int startindex, const beast_wchar character) const noexcept
{
    charpointertype t (text);

    for (int i = 0; ! t.isempty(); ++i)
    {
        if (i >= startindex)
        {
            if (t.getandadvance() == character)
                return i;
        }
        else
        {
            ++t;
        }
    }

    return -1;
}

int string::lastindexofchar (const beast_wchar character) const noexcept
{
    charpointertype t (text);
    int last = -1;

    for (int i = 0; ! t.isempty(); ++i)
        if (t.getandadvance() == character)
            last = i;

    return last;
}

int string::indexofanyof (const string& characterstolookfor, const int startindex, const bool ignorecase) const noexcept
{
    charpointertype t (text);

    for (int i = 0; ! t.isempty(); ++i)
    {
        if (i >= startindex)
        {
            if (characterstolookfor.text.indexof (t.getandadvance(), ignorecase) >= 0)
                return i;
        }
        else
        {
            ++t;
        }
    }

    return -1;
}

int string::indexof (const string& other) const noexcept
{
    return other.isempty() ? 0 : text.indexof (other.text);
}

int string::indexofignorecase (const string& other) const noexcept
{
    return other.isempty() ? 0 : characterfunctions::indexofignorecase (text, other.text);
}

int string::indexof (const int startindex, const string& other) const noexcept
{
    if (other.isempty())
        return -1;

    charpointertype t (text);

    for (int i = startindex; --i >= 0;)
    {
        if (t.isempty())
            return -1;

        ++t;
    }

    int found = t.indexof (other.text);
    if (found >= 0)
        found += startindex;
    return found;
}

int string::indexofignorecase (const int startindex, const string& other) const noexcept
{
    if (other.isempty())
        return -1;

    charpointertype t (text);

    for (int i = startindex; --i >= 0;)
    {
        if (t.isempty())
            return -1;

        ++t;
    }

    int found = characterfunctions::indexofignorecase (t, other.text);
    if (found >= 0)
        found += startindex;
    return found;
}

int string::lastindexof (const string& other) const noexcept
{
    if (other.isnotempty())
    {
        const int len = other.length();
        int i = length() - len;

        if (i >= 0)
        {
            charpointertype n (text + i);

            while (i >= 0)
            {
                if (n.compareupto (other.text, len) == 0)
                    return i;

                --n;
                --i;
            }
        }
    }

    return -1;
}

int string::lastindexofignorecase (const string& other) const noexcept
{
    if (other.isnotempty())
    {
        const int len = other.length();
        int i = length() - len;

        if (i >= 0)
        {
            charpointertype n (text + i);

            while (i >= 0)
            {
                if (n.compareignorecaseupto (other.text, len) == 0)
                    return i;

                --n;
                --i;
            }
        }
    }

    return -1;
}

int string::lastindexofanyof (const string& characterstolookfor, const bool ignorecase) const noexcept
{
    charpointertype t (text);
    int last = -1;

    for (int i = 0; ! t.isempty(); ++i)
        if (characterstolookfor.text.indexof (t.getandadvance(), ignorecase) >= 0)
            last = i;

    return last;
}

bool string::contains (const string& other) const noexcept
{
    return indexof (other) >= 0;
}

bool string::containschar (const beast_wchar character) const noexcept
{
    return text.indexof (character) >= 0;
}

bool string::containsignorecase (const string& t) const noexcept
{
    return indexofignorecase (t) >= 0;
}

int string::indexofwholeword (const string& word) const noexcept
{
    if (word.isnotempty())
    {
        charpointertype t (text);
        const int wordlen = word.length();
        const int end = (int) t.length() - wordlen;

        for (int i = 0; i <= end; ++i)
        {
            if (t.compareupto (word.text, wordlen) == 0
                  && (i == 0 || ! (t - 1).isletterordigit())
                  && ! (t + wordlen).isletterordigit())
                return i;

            ++t;
        }
    }

    return -1;
}

int string::indexofwholewordignorecase (const string& word) const noexcept
{
    if (word.isnotempty())
    {
        charpointertype t (text);
        const int wordlen = word.length();
        const int end = (int) t.length() - wordlen;

        for (int i = 0; i <= end; ++i)
        {
            if (t.compareignorecaseupto (word.text, wordlen) == 0
                  && (i == 0 || ! (t - 1).isletterordigit())
                  && ! (t + wordlen).isletterordigit())
                return i;

            ++t;
        }
    }

    return -1;
}

bool string::containswholeword (const string& wordtolookfor) const noexcept
{
    return indexofwholeword (wordtolookfor) >= 0;
}

bool string::containswholewordignorecase (const string& wordtolookfor) const noexcept
{
    return indexofwholewordignorecase (wordtolookfor) >= 0;
}

//==============================================================================
template <typename charpointer>
struct wildcardmatcher
{
    static bool matches (charpointer wildcard, charpointer test, const bool ignorecase) noexcept
    {
        for (;;)
        {
            const beast_wchar wc = wildcard.getandadvance();

            if (wc == '*')
                return wildcard.isempty() || matchesanywhere (wildcard, test, ignorecase);

            if (! charactermatches (wc, test.getandadvance(), ignorecase))
                return false;

            if (wc == 0)
                return true;
        }
    }

    static bool charactermatches (const beast_wchar wc, const beast_wchar tc, const bool ignorecase) noexcept
    {
        return (wc == tc) || (wc == '?' && tc != 0)
                || (ignorecase && characterfunctions::tolowercase (wc) == characterfunctions::tolowercase (tc));
    }

    static bool matchesanywhere (const charpointer wildcard, charpointer test, const bool ignorecase) noexcept
    {
        for (; ! test.isempty(); ++test)
            if (matches (wildcard, test, ignorecase))
                return true;

        return false;
    }
};

bool string::matcheswildcard (const string& wildcard, const bool ignorecase) const noexcept
{
    return wildcardmatcher<charpointertype>::matches (wildcard.text, text, ignorecase);
}

//==============================================================================
string string::repeatedstring (const string& stringtorepeat, int numberoftimestorepeat)
{
    if (numberoftimestorepeat <= 0)
        return empty;

    string result (preallocationbytes (stringtorepeat.getbyteoffsetofend() * (size_t) numberoftimestorepeat));
    charpointertype n (result.text);

    while (--numberoftimestorepeat >= 0)
        n.writeall (stringtorepeat.text);

    return result;
}

string string::paddedleft (const beast_wchar padcharacter, int minimumlength) const
{
    bassert (padcharacter != 0);

    int extrachars = minimumlength;
    charpointertype end (text);

    while (! end.isempty())
    {
        --extrachars;
        ++end;
    }

    if (extrachars <= 0 || padcharacter == 0)
        return *this;

    const size_t currentbytesize = (size_t) (((char*) end.getaddress()) - (char*) text.getaddress());
    string result (preallocationbytes (currentbytesize + (size_t) extrachars * charpointertype::getbytesrequiredfor (padcharacter)));
    charpointertype n (result.text);

    while (--extrachars >= 0)
        n.write (padcharacter);

    n.writeall (text);
    return result;
}

string string::paddedright (const beast_wchar padcharacter, int minimumlength) const
{
    bassert (padcharacter != 0);

    int extrachars = minimumlength;
    charpointertype end (text);

    while (! end.isempty())
    {
        --extrachars;
        ++end;
    }

    if (extrachars <= 0 || padcharacter == 0)
        return *this;

    const size_t currentbytesize = (size_t) (((char*) end.getaddress()) - (char*) text.getaddress());
    string result (preallocationbytes (currentbytesize + (size_t) extrachars * charpointertype::getbytesrequiredfor (padcharacter)));
    charpointertype n (result.text);

    n.writeall (text);

    while (--extrachars >= 0)
        n.write (padcharacter);

    n.writenull();
    return result;
}

//==============================================================================
string string::replacesection (int index, int numcharstoreplace, const string& stringtoinsert) const
{
    if (index < 0)
    {
        // a negative index to replace from?
        bassertfalse;
        index = 0;
    }

    if (numcharstoreplace < 0)
    {
        // replacing a negative number of characters?
        numcharstoreplace = 0;
        bassertfalse;
    }

    int i = 0;
    charpointertype insertpoint (text);

    while (i < index)
    {
        if (insertpoint.isempty())
        {
            // replacing beyond the end of the string?
            bassertfalse;
            return *this + stringtoinsert;
        }

        ++insertpoint;
        ++i;
    }

    charpointertype startofremainder (insertpoint);

    i = 0;
    while (i < numcharstoreplace && ! startofremainder.isempty())
    {
        ++startofremainder;
        ++i;
    }

    if (insertpoint == text && startofremainder.isempty())
        return stringtoinsert;

    const size_t initialbytes = (size_t) (((char*) insertpoint.getaddress()) - (char*) text.getaddress());
    const size_t newstringbytes = stringtoinsert.getbyteoffsetofend();
    const size_t remainderbytes = (size_t) (((char*) startofremainder.findterminatingnull().getaddress()) - (char*) startofremainder.getaddress());

    const size_t newtotalbytes = initialbytes + newstringbytes + remainderbytes;
    if (newtotalbytes <= 0)
        return string::empty;

    string result (preallocationbytes ((size_t) newtotalbytes));

    char* dest = (char*) result.text.getaddress();
    memcpy (dest, text.getaddress(), initialbytes);
    dest += initialbytes;
    memcpy (dest, stringtoinsert.text.getaddress(), newstringbytes);
    dest += newstringbytes;
    memcpy (dest, startofremainder.getaddress(), remainderbytes);
    dest += remainderbytes;
    charpointertype ((charpointertype::chartype*) dest).writenull();

    return result;
}

string string::replace (const string& stringtoreplace, const string& stringtoinsert, const bool ignorecase) const
{
    const int stringtoreplacelen = stringtoreplace.length();
    const int stringtoinsertlen = stringtoinsert.length();

    int i = 0;
    string result (*this);

    while ((i = (ignorecase ? result.indexofignorecase (i, stringtoreplace)
                            : result.indexof (i, stringtoreplace))) >= 0)
    {
        result = result.replacesection (i, stringtoreplacelen, stringtoinsert);
        i += stringtoinsertlen;
    }

    return result;
}

class stringcreationhelper
{
public:
    stringcreationhelper (const size_t initialbytes)
        : source (nullptr), dest (nullptr), allocatedbytes (initialbytes), byteswritten (0)
    {
        result.preallocatebytes (allocatedbytes);
        dest = result.getcharpointer();
    }

    stringcreationhelper (const string::charpointertype s)
        : source (s), dest (nullptr), allocatedbytes (stringholder::getallocatednumbytes (s)), byteswritten (0)
    {
        result.preallocatebytes (allocatedbytes);
        dest = result.getcharpointer();
    }

    void write (beast_wchar c)
    {
        byteswritten += string::charpointertype::getbytesrequiredfor (c);

        if (byteswritten > allocatedbytes)
        {
            allocatedbytes += std::max ((size_t) 8, allocatedbytes / 16);
            const size_t destoffset = (size_t) (((char*) dest.getaddress()) - (char*) result.getcharpointer().getaddress());
            result.preallocatebytes (allocatedbytes);
            dest = addbytestopointer (result.getcharpointer().getaddress(), (int) destoffset);
        }

        dest.write (c);
    }

    string result;
    string::charpointertype source;

private:
    string::charpointertype dest;
    size_t allocatedbytes, byteswritten;
};

string string::replacecharacter (const beast_wchar chartoreplace, const beast_wchar chartoinsert) const
{
    if (! containschar (chartoreplace))
        return *this;

    stringcreationhelper builder (text);

    for (;;)
    {
        beast_wchar c = builder.source.getandadvance();

        if (c == chartoreplace)
            c = chartoinsert;

        builder.write (c);

        if (c == 0)
            break;
    }

    return builder.result;
}

string string::replacecharacters (const string& characterstoreplace, const string& characterstoinsertinstead) const
{
    stringcreationhelper builder (text);

    for (;;)
    {
        beast_wchar c = builder.source.getandadvance();

        const int index = characterstoreplace.indexofchar (c);
        if (index >= 0)
            c = characterstoinsertinstead [index];

        builder.write (c);

        if (c == 0)
            break;
    }

    return builder.result;
}

//==============================================================================
bool string::startswith (const string& other) const noexcept
{
    return text.compareupto (other.text, other.length()) == 0;
}

bool string::startswithignorecase (const string& other) const noexcept
{
    return text.compareignorecaseupto (other.text, other.length()) == 0;
}

bool string::startswithchar (const beast_wchar character) const noexcept
{
    bassert (character != 0); // strings can't contain a null character!

    return *text == character;
}

bool string::endswithchar (const beast_wchar character) const noexcept
{
    bassert (character != 0); // strings can't contain a null character!

    if (text.isempty())
        return false;

    charpointertype t (text.findterminatingnull());
    return *--t == character;
}

bool string::endswith (const string& other) const noexcept
{
    charpointertype end (text.findterminatingnull());
    charpointertype otherend (other.text.findterminatingnull());

    while (end > text && otherend > other.text)
    {
        --end;
        --otherend;

        if (*end != *otherend)
            return false;
    }

    return otherend == other.text;
}

bool string::endswithignorecase (const string& other) const noexcept
{
    charpointertype end (text.findterminatingnull());
    charpointertype otherend (other.text.findterminatingnull());

    while (end > text && otherend > other.text)
    {
        --end;
        --otherend;

        if (end.tolowercase() != otherend.tolowercase())
            return false;
    }

    return otherend == other.text;
}

//==============================================================================
string string::touppercase() const
{
    stringcreationhelper builder (text);

    for (;;)
    {
        const beast_wchar c = builder.source.touppercase();
        ++(builder.source);
        builder.write (c);

        if (c == 0)
            break;
    }

    return builder.result;
}

string string::tolowercase() const
{
    stringcreationhelper builder (text);

    for (;;)
    {
        const beast_wchar c = builder.source.tolowercase();
        ++(builder.source);
        builder.write (c);

        if (c == 0)
            break;
    }

    return builder.result;
}

//==============================================================================
beast_wchar string::getlastcharacter() const noexcept
{
    return isempty() ? beast_wchar() : text [length() - 1];
}

string string::substring (int start, const int end) const
{
    if (start < 0)
        start = 0;

    if (end <= start)
        return empty;

    int i = 0;
    charpointertype t1 (text);

    while (i < start)
    {
        if (t1.isempty())
            return empty;

        ++i;
        ++t1;
    }

    charpointertype t2 (t1);
    while (i < end)
    {
        if (t2.isempty())
        {
            if (start == 0)
                return *this;

            break;
        }

        ++i;
        ++t2;
    }

    return string (t1, t2);
}

string string::substring (int start) const
{
    if (start <= 0)
        return *this;

    charpointertype t (text);

    while (--start >= 0)
    {
        if (t.isempty())
            return empty;

        ++t;
    }

    return string (t);
}

string string::droplastcharacters (const int numbertodrop) const
{
    return string (text, (size_t) std::max (0, length() - numbertodrop));
}

string string::getlastcharacters (const int numcharacters) const
{
    return string (text + std::max (0, length() - std::max (0, numcharacters)));
}

string string::fromfirstoccurrenceof (const string& sub,
                                      const bool includesubstring,
                                      const bool ignorecase) const
{
    const int i = ignorecase ? indexofignorecase (sub)
                             : indexof (sub);
    if (i < 0)
        return empty;

    return substring (includesubstring ? i : i + sub.length());
}

string string::fromlastoccurrenceof (const string& sub,
                                     const bool includesubstring,
                                     const bool ignorecase) const
{
    const int i = ignorecase ? lastindexofignorecase (sub)
                             : lastindexof (sub);
    if (i < 0)
        return *this;

    return substring (includesubstring ? i : i + sub.length());
}

string string::uptofirstoccurrenceof (const string& sub,
                                      const bool includesubstring,
                                      const bool ignorecase) const
{
    const int i = ignorecase ? indexofignorecase (sub)
                             : indexof (sub);
    if (i < 0)
        return *this;

    return substring (0, includesubstring ? i + sub.length() : i);
}

string string::uptolastoccurrenceof (const string& sub,
                                     const bool includesubstring,
                                     const bool ignorecase) const
{
    const int i = ignorecase ? lastindexofignorecase (sub)
                             : lastindexof (sub);
    if (i < 0)
        return *this;

    return substring (0, includesubstring ? i + sub.length() : i);
}

bool string::isquotedstring() const
{
    const string trimmed (trimstart());

    return trimmed[0] == '"'
        || trimmed[0] == '\'';
}

string string::unquoted() const
{
    const int len = length();

    if (len == 0)
        return empty;

    const beast_wchar lastchar = text [len - 1];
    const int dropatstart = (*text == '"' || *text == '\'') ? 1 : 0;
    const int dropatend = (lastchar == '"' || lastchar == '\'') ? 1 : 0;

    return substring (dropatstart, len - dropatend);
}

string string::quoted (const beast_wchar quotecharacter) const
{
    if (isempty())
        return chartostring (quotecharacter) + quotecharacter;

    string t (*this);

    if (! t.startswithchar (quotecharacter))
        t = chartostring (quotecharacter) + t;

    if (! t.endswithchar (quotecharacter))
        t += quotecharacter;

    return t;
}

//==============================================================================
static string::charpointertype findtrimmedend (const string::charpointertype start,
                                               string::charpointertype end)
{
    while (end > start)
    {
        if (! (--end).iswhitespace())
        {
            ++end;
            break;
        }
    }

    return end;
}

string string::trim() const
{
    if (isnotempty())
    {
        charpointertype start (text.findendofwhitespace());

        const charpointertype end (start.findterminatingnull());
        charpointertype trimmedend (findtrimmedend (start, end));

        if (trimmedend <= start)
            return empty;

        if (text < start || trimmedend < end)
            return string (start, trimmedend);
    }

    return *this;
}

string string::trimstart() const
{
    if (isnotempty())
    {
        const charpointertype t (text.findendofwhitespace());

        if (t != text)
            return string (t);
    }

    return *this;
}

string string::trimend() const
{
    if (isnotempty())
    {
        const charpointertype end (text.findterminatingnull());
        charpointertype trimmedend (findtrimmedend (text, end));

        if (trimmedend < end)
            return string (text, trimmedend);
    }

    return *this;
}

string string::trimcharactersatstart (const string& characterstotrim) const
{
    charpointertype t (text);

    while (characterstotrim.containschar (*t))
        ++t;

    return t == text ? *this : string (t);
}

string string::trimcharactersatend (const string& characterstotrim) const
{
    if (isnotempty())
    {
        const charpointertype end (text.findterminatingnull());
        charpointertype trimmedend (end);

        while (trimmedend > text)
        {
            if (! characterstotrim.containschar (*--trimmedend))
            {
                ++trimmedend;
                break;
            }
        }

        if (trimmedend < end)
            return string (text, trimmedend);
    }

    return *this;
}

//==============================================================================
string string::retaincharacters (const string& characterstoretain) const
{
    if (isempty())
        return empty;

    stringcreationhelper builder (text);

    for (;;)
    {
        beast_wchar c = builder.source.getandadvance();

        if (characterstoretain.containschar (c))
            builder.write (c);

        if (c == 0)
            break;
    }

    builder.write (0);
    return builder.result;
}

string string::removecharacters (const string& characterstoremove) const
{
    if (isempty())
        return empty;

    stringcreationhelper builder (text);

    for (;;)
    {
        beast_wchar c = builder.source.getandadvance();

        if (! characterstoremove.containschar (c))
            builder.write (c);

        if (c == 0)
            break;
    }

    return builder.result;
}

string string::initialsectioncontainingonly (const string& permittedcharacters) const
{
    charpointertype t (text);

    while (! t.isempty())
    {
        if (! permittedcharacters.containschar (*t))
            return string (text, t);

        ++t;
    }

    return *this;
}

string string::initialsectionnotcontaining (const string& characterstostopat) const
{
    charpointertype t (text);

    while (! t.isempty())
    {
        if (characterstostopat.containschar (*t))
            return string (text, t);

        ++t;
    }

    return *this;
}

bool string::containsonly (const string& chars) const noexcept
{
    charpointertype t (text);

    while (! t.isempty())
        if (! chars.containschar (t.getandadvance()))
            return false;

    return true;
}

bool string::containsanyof (const string& chars) const noexcept
{
    charpointertype t (text);

    while (! t.isempty())
        if (chars.containschar (t.getandadvance()))
            return true;

    return false;
}

bool string::containsnonwhitespacechars() const noexcept
{
    charpointertype t (text);

    while (! t.isempty())
    {
        if (! t.iswhitespace())
            return true;

        ++t;
    }

    return false;
}

// note! the format parameter here must not be a reference, otherwise ms's va_start macro fails to work (but still compiles).
string string::formatted (const string pf, ... )
{
    size_t buffersize = 256;

    for (;;)
    {
        va_list args;
        va_start (args, pf);

       #if beast_windows
        heapblock <wchar_t> temp (buffersize);
        const int num = (int) _vsnwprintf (temp.getdata(), buffersize - 1, pf.towidecharpointer(), args);
       #elif beast_android
        heapblock <char> temp (buffersize);
        const int num = (int) vsnprintf (temp.getdata(), buffersize - 1, pf.toutf8(), args);
       #else
        heapblock <wchar_t> temp (buffersize);
        const int num = (int) vswprintf (temp.getdata(), buffersize - 1, pf.towidecharpointer(), args);
       #endif

        va_end (args);

        if (num > 0)
            return string (temp);

        buffersize += 256;

        if (num == 0 || buffersize > 65536) // the upper limit is a sanity check to avoid situations where vprintf repeatedly
            break;                          // returns -1 because of an error rather than because it needs more space.
    }

    return empty;
}

//==============================================================================
int string::getintvalue() const noexcept
{
    return text.getintvalue32();
}

int string::gettrailingintvalue() const noexcept
{
    int n = 0;
    int mult = 1;
    charpointertype t (text.findterminatingnull());

    while (--t >= text)
    {
        if (! t.isdigit())
        {
            if (*t == '-')
                n = -n;

            break;
        }

        n += mult * (*t - '0');
        mult *= 10;
    }

    return n;
}

std::int64_t string::getlargeintvalue() const noexcept
{
    return text.getintvalue64();
}

float string::getfloatvalue() const noexcept
{
    return (float) getdoublevalue();
}

double string::getdoublevalue() const noexcept
{
    return text.getdoublevalue();
}

static const char hexdigits[] = "0123456789abcdef";

template <typename type>
struct hexconverter
{
    static string hextostring (type v)
    {
        char buffer[32];
        char* const end = buffer + 32;
        char* t = end;
        *--t = 0;

        do
        {
            *--t = hexdigits [(int) (v & 15)];
            v >>= 4;

        } while (v != 0);

        return string (t, (size_t) (end - t) - 1);
    }

    static type stringtohex (string::charpointertype t) noexcept
    {
        type result = 0;

        while (! t.isempty())
        {
            const int hexvalue = characterfunctions::gethexdigitvalue (t.getandadvance());

            if (hexvalue >= 0)
                result = (result << 4) | hexvalue;
        }

        return result;
    }
};

string string::tohexstring (const int number)
{
    return hexconverter <unsigned int>::hextostring ((unsigned int) number);
}

string string::tohexstring (const std::int64_t number)
{
    return hexconverter <std::uint64_t>::hextostring ((std::uint64_t) number);
}

string string::tohexstring (const short number)
{
    return tohexstring ((int) (unsigned short) number);
}

string string::tohexstring (const void* const d, const int size, const int groupsize)
{
    if (size <= 0)
        return empty;

    int numchars = (size * 2) + 2;
    if (groupsize > 0)
        numchars += size / groupsize;

    string s (preallocationbytes (sizeof (charpointertype::chartype) * (size_t) numchars));

    const unsigned char* data = static_cast <const unsigned char*> (d);
    charpointertype dest (s.text);

    for (int i = 0; i < size; ++i)
    {
        const unsigned char nextbyte = *data++;
        dest.write ((beast_wchar) hexdigits [nextbyte >> 4]);
        dest.write ((beast_wchar) hexdigits [nextbyte & 0xf]);

        if (groupsize > 0 && (i % groupsize) == (groupsize - 1) && i < (size - 1))
            dest.write ((beast_wchar) ' ');
    }

    dest.writenull();
    return s;
}

int   string::gethexvalue32() const noexcept    { return hexconverter<int>  ::stringtohex (text); }
std::int64_t string::gethexvalue64() const noexcept    { return hexconverter<std::int64_t>::stringtohex (text); }

//==============================================================================
string string::createstringfromdata (const void* const unknowndata, const int size)
{
    const std::uint8_t* const data = static_cast<const std::uint8_t*> (unknowndata);

    if (size <= 0 || data == nullptr)
        return empty;

    if (size == 1)
        return chartostring ((beast_wchar) data[0]);

    if (charpointer_utf16::isbyteordermarkbigendian (data)
         || charpointer_utf16::isbyteordermarklittleendian (data))
    {
        const int numchars = size / 2 - 1;

        stringcreationhelper builder ((size_t) numchars);

        const std::uint16_t* const src = (const std::uint16_t*) (data + 2);

        if (charpointer_utf16::isbyteordermarkbigendian (data))
        {
            for (int i = 0; i < numchars; ++i)
                builder.write ((beast_wchar) byteorder::swapiflittleendian (src[i]));
        }
        else
        {
            for (int i = 0; i < numchars; ++i)
                builder.write ((beast_wchar) byteorder::swapifbigendian (src[i]));
        }

        builder.write (0);
        return builder.result;
    }

    const std::uint8_t* start = data;

    if (size >= 3 && charpointer_utf8::isbyteordermark (data))
        start += 3;

    return string (charpointer_utf8 ((const char*) start),
                   charpointer_utf8 ((const char*) (data + size)));
}

//==============================================================================
static const beast_wchar emptychar = 0;

template <class charpointertype_src, class charpointertype_dest>
struct stringencodingconverter
{
    static charpointertype_dest convert (const string& s)
    {
        string& source = const_cast <string&> (s);

        typedef typename charpointertype_dest::chartype destchar;

        if (source.isempty())
            return charpointertype_dest (reinterpret_cast <const destchar*> (&emptychar));

        charpointertype_src text (source.getcharpointer());
        const size_t extrabytesneeded = charpointertype_dest::getbytesrequiredfor (text);
        const size_t endoffset = (text.sizeinbytes() + 3) & ~3u; // the new string must be word-aligned or many windows
                                                                // functions will fail to read it correctly!
        source.preallocatebytes (endoffset + extrabytesneeded);
        text = source.getcharpointer();

        void* const newspace = addbytestopointer (text.getaddress(), (int) endoffset);
        const charpointertype_dest extraspace (static_cast <destchar*> (newspace));

       #if beast_debug // (this just avoids spurious warnings from valgrind about the uninitialised bytes at the end of the buffer..)
        const size_t bytestoclear = (size_t) std::min ((int) extrabytesneeded, 4);
        zeromem (addbytestopointer (newspace, extrabytesneeded - bytestoclear), bytestoclear);
       #endif

        charpointertype_dest (extraspace).writeall (text);
        return extraspace;
    }
};

template <>
struct stringencodingconverter <charpointer_utf8, charpointer_utf8>
{
    static charpointer_utf8 convert (const string& source) noexcept   { return charpointer_utf8 ((charpointer_utf8::chartype*) source.getcharpointer().getaddress()); }
};

template <>
struct stringencodingconverter <charpointer_utf16, charpointer_utf16>
{
    static charpointer_utf16 convert (const string& source) noexcept  { return charpointer_utf16 ((charpointer_utf16::chartype*) source.getcharpointer().getaddress()); }
};

template <>
struct stringencodingconverter <charpointer_utf32, charpointer_utf32>
{
    static charpointer_utf32 convert (const string& source) noexcept  { return charpointer_utf32 ((charpointer_utf32::chartype*) source.getcharpointer().getaddress()); }
};

charpointer_utf8  string::toutf8()  const { return stringencodingconverter <charpointertype, charpointer_utf8 >::convert (*this); }
charpointer_utf16 string::toutf16() const { return stringencodingconverter <charpointertype, charpointer_utf16>::convert (*this); }
charpointer_utf32 string::toutf32() const { return stringencodingconverter <charpointertype, charpointer_utf32>::convert (*this); }

const char* string::torawutf8() const
{
    return toutf8().getaddress();
}

const wchar_t* string::towidecharpointer() const
{
    return stringencodingconverter <charpointertype, charpointer_wchar_t>::convert (*this).getaddress();
}

std::string string::tostdstring() const
{
    return std::string (torawutf8());
}

//==============================================================================
template <class charpointertype_src, class charpointertype_dest>
struct stringcopier
{
    static size_t copytobuffer (const charpointertype_src source, typename charpointertype_dest::chartype* const buffer, const size_t maxbuffersizebytes)
    {
        bassert (((std::ptrdiff_t) maxbuffersizebytes) >= 0); // keep this value positive!

        if (buffer == nullptr)
            return charpointertype_dest::getbytesrequiredfor (source) + sizeof (typename charpointertype_dest::chartype);

        return charpointertype_dest (buffer).writewithdestbytelimit (source, maxbuffersizebytes);
    }
};

size_t string::copytoutf8 (charpointer_utf8::chartype* const buffer, size_t maxbuffersizebytes) const noexcept
{
    return stringcopier <charpointertype, charpointer_utf8>::copytobuffer (text, buffer, maxbuffersizebytes);
}

size_t string::copytoutf16 (charpointer_utf16::chartype* const buffer, size_t maxbuffersizebytes) const noexcept
{
    return stringcopier <charpointertype, charpointer_utf16>::copytobuffer (text, buffer, maxbuffersizebytes);
}

size_t string::copytoutf32 (charpointer_utf32::chartype* const buffer, size_t maxbuffersizebytes) const noexcept
{
    return stringcopier <charpointertype, charpointer_utf32>::copytobuffer (text, buffer, maxbuffersizebytes);
}

//==============================================================================
size_t string::getnumbytesasutf8() const noexcept
{
    return charpointer_utf8::getbytesrequiredfor (text);
}

string string::fromutf8 (const char* const buffer, int buffersizebytes)
{
    if (buffer != nullptr)
    {
        if (buffersizebytes < 0) return string (charpointer_utf8 (buffer));
        if (buffersizebytes > 0) return string (charpointer_utf8 (buffer),
                                                charpointer_utf8 (buffer + buffersizebytes));
    }

    return string::empty;
}

#if beast_msvc
 #pragma warning (pop)
#endif

}
