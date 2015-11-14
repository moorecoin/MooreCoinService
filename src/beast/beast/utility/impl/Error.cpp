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

#include <beast/utility/error.h>
#include <beast/utility/debug.h>

#include <ostream>

// vfalco todo localizable strings
#ifndef trans
#define trans(s) (s)
#define undef_trans
#endif

namespace beast {

error::error ()
    : m_code (success)
    , m_linenumber (0)
    , m_needstobechecked (true)
    , m_szwhat (0)
{
}

error::error (error const& other)
    : m_code (other.m_code)
    , m_reasontext (other.m_reasontext)
    , m_sourcefilename (other.m_sourcefilename)
    , m_linenumber (other.m_linenumber)
    , m_needstobechecked (true)
    , m_szwhat (0)
{
    other.m_needstobechecked = false;
}

error::~error () noexcept
{
    /* if this goes off it means an error object was created but never tested */
    bassert (!m_needstobechecked);
}

error& error::operator= (error const& other)
{
    m_code = other.m_code;
    m_reasontext = other.m_reasontext;
    m_sourcefilename = other.m_sourcefilename;
    m_linenumber = other.m_linenumber;
    m_needstobechecked = true;
    m_what = string::empty;
    m_szwhat = 0;

    other.m_needstobechecked = false;

    return *this;
}

error::code error::code () const
{
    m_needstobechecked = false;
    return m_code;
}

bool error::failed () const
{
    return code () != success;
}

string const error::getreasontext () const
{
    return m_reasontext;
}

string const error::getsourcefilename () const
{
    return m_sourcefilename;
}

int error::getlinenumber () const
{
    return m_linenumber;
}

error& error::fail (char const* sourcefilename,
                    int linenumber,
                    string const reasontext,
                    code errorcode)
{
    bassert (m_code == success);
    bassert (errorcode != success);

    m_code = errorcode;
    m_reasontext = reasontext;
    m_sourcefilename = debug::getfilenamefrompath (sourcefilename);
    m_linenumber = linenumber;
    m_needstobechecked = true;

    return *this;
}

error& error::fail (char const* sourcefilename,
                    int linenumber,
                    code errorcode)
{
    return fail (sourcefilename,
                 linenumber,
                 getreasontextforcode (errorcode),
                 errorcode);
}

void error::reset ()
{
    m_code = success;
    m_reasontext = string::empty;
    m_sourcefilename = string::empty;
    m_linenumber = 0;
    m_needstobechecked = true;
    m_what = string::empty;
    m_szwhat = 0;
}

void error::willbereported () const
{
    m_needstobechecked = false;
}

char const* error::what () const noexcept
{
    if (! m_szwhat)
    {
        // the application could not be initialized because sqlite was denied access permission
        // the application unexpectedly quit because the exception 'sqlite was denied access permission at file ' was thrown
        m_what <<
               m_reasontext << " " <<
               trans ("at file") << " '" <<
               m_sourcefilename << "' " <<
               trans ("line") << " " <<
               string (m_linenumber) << " " <<
               trans ("with code") << " = " <<
               string (m_code);

        m_szwhat = (const char*)m_what.toutf8 ();
    }

    return m_szwhat;
}

string const error::getreasontextforcode (code code)
{
    string s;

    switch (code)
    {
    case success:
        s = trans ("the operation was successful");
        break;

    case general:
        s = trans ("a general error occurred");
        break;

    case canceled:
        s = trans ("the operation was canceled");
        break;

    case exception:
        s = trans ("an exception was thrown");
        break;

    case unexpected:
        s = trans ("an unexpected result was encountered");
        break;

    case platform:
        s = trans ("a system exception was signaled");
        break;

    case nomemory:
        s = trans ("there was not enough memory");
        break;

    case nomoredata:
        s = trans ("the end of data was reached");
        break;

    case invaliddata:
        s = trans ("the data is corrupt or invalid");
        break;

    case bufferspace:
        s = trans ("the buffer is too small");
        break;

    case badparameter:
        s = trans ("one or more parameters were invalid");
        break;

    case assertfailed:
        s = trans ("an assertion failed");
        break;

    case fileinuse:
        s = trans ("the file is in use");
        break;

    case fileexists:
        s = trans ("the file exists");
        break;

    case filenoperm:
        s = trans ("permission was denied");
        break;

    case fileioerror:
        s = trans ("an i/o or device error occurred");
        break;

    case filenospace:
        s = trans ("there is no space left on the device");
        break;

    case filenotfound:
        s = trans ("the file was not found");
        break;

    case filenameinvalid:
        s = trans ("the file name was illegal or malformed");
        break;

    default:
        s = trans ("an unknown error code was received");
        break;
    }

    return s;
}

#ifdef undef_trans
#undef trans
#undef undef_trans
#endif

}

