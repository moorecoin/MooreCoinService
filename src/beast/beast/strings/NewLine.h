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

#ifndef beast_strings_newline_h_included
#define beast_strings_newline_h_included

#include <beast/config.h>

#include <beast/strings/string.h>

namespace beast {

//==============================================================================
/** this class is used for represent a new-line character sequence.

    to write a new-line to a stream, you can use the predefined 'newline' variable, e.g.
    @code
    myoutputstream << "hello world" << newline << newline;
    @endcode

    the exact character sequence that will be used for the new-line can be set and
    retrieved with outputstream::setnewlinestring() and outputstream::getnewlinestring().
*/
class newline
{
public:
    /** returns the default new-line sequence that the library uses.
        @see outputstream::setnewlinestring()
    */
    static const char* getdefault() noexcept        { return "\r\n"; }

    /** returns the default new-line sequence that the library uses.
        @see getdefault()
    */
    operator string() const                         { return getdefault(); }
};

//==============================================================================
/** a predefined object representing a new-line, which can be written to a string or stream.

    to write a new-line to a stream, you can use the predefined 'newline' variable like this:
    @code
    myoutputstream << "hello world" << newline << newline;
    @endcode
*/
extern newline newline;

//==============================================================================
/** writes a new-line sequence to a string.
    you can use the predefined object 'newline' to invoke this, e.g.
    @code
    mystring << "hello world" << newline << newline;
    @endcode
*/
string& operator<< (string& string1, const newline&);

}

#endif

