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

#ifndef beast_strings_stringcharpointertype_h_included
#define beast_strings_stringcharpointertype_h_included

#include <beast/config.h>
#include <beast/strings/charpointer_utf8.h>
#include <beast/strings/charpointer_utf16.h>
#include <beast/strings/charpointer_utf32.h>

namespace beast {

/** this is the character encoding type used internally to store the string.

    by setting the value of beast_string_utf_type to 8, 16, or 32, you can change the
    internal storage format of the string class. utf-8 uses the least space (if your strings
    contain few extended characters), but call operator[] involves iterating the string to find
    the required index. utf-32 provides instant random access to its characters, but uses 4 bytes
    per character to store them. utf-16 uses more space than utf-8 and is also slow to index,
    but is the native wchar_t format used in windows.

    it doesn't matter too much which format you pick, because the toutf8(), toutf16() and
    toutf32() methods let you access the string's content in any of the other formats.
*/
#if (beast_string_utf_type == 32)
typedef charpointer_utf32 stringcharpointertype;

#elif (beast_string_utf_type == 16)
typedef charpointer_utf16 stringcharpointertype;

#elif (beast_string_utf_type == 8)
typedef charpointer_utf8  stringcharpointertype;

#else
#error "you must set the value of beast_string_utf_type to be either 8, 16, or 32!"

#endif

}

#endif

