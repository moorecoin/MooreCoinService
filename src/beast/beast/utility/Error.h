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

#ifndef beast_utility_error_h_included
#define beast_utility_error_h_included

#include <beast/config.h>

#include <beast/strings/string.h>

#include <stdexcept>

namespace beast {

/** a concise error report.

    this lightweight but flexible class records lets you record the file and
    line where a recoverable error occurred, along with some optional human
    readable text.

    a recoverable error can be passed along and turned into a non recoverable
    error by throwing the object: it's derivation from std::exception is
    fully compliant with the c++ exception interface.

    @ingroup beast_core
*/
class error
    : public std::exception
{
public:
    /** numeric code.

        this enumeration is useful when the caller needs to take different
        actions depending on the failure. for example, trying again later if
        a file is locked.
    */
    enum code
    {
        success,        //!< "the operation was successful"

        general,        //!< "a general error occurred"

        canceled,       //!< "the operation was canceled"
        exception,      //!< "an exception was thrown"
        unexpected,     //!< "an unexpected result was encountered"
        platform,       //!< "a system exception was signaled"

        nomemory,       //!< "there was not enough memory"
        nomoredata,     //!< "the end of data was reached"
        invaliddata,    //!< "the data is corrupt or invalid"
        bufferspace,    //!< "the buffer is too small"
        badparameter,   //!< "one or more parameters were invalid"
        assertfailed,   //!< "an assertion failed"

        fileinuse,      //!< "the file is in use"
        fileexists,     //!< "the file exists"
        filenoperm,     //!< "permission was denied" (file attributes conflict)
        fileioerror,    //!< "an i/o or device error occurred"
        filenospace,    //!< "there is no space left on the device"
        filenotfound,   //!< "the file was not found"
        filenameinvalid //!< "the file name was illegal or malformed"
    };

    error ();
    error (error const& other);
    error& operator= (error const& other);

    virtual ~error () noexcept;

    code code () const;
    bool failed () const;

    explicit operator bool () const
    {
        return code () != success;
    }

    string const getreasontext () const;
    string const getsourcefilename () const;
    int getlinenumber () const;

    error& fail (char const* sourcefilename,
                 int linenumber,
                 string const reasontext,
                 code errorcode = general);

    error& fail (char const* sourcefilename,
                 int linenumber,
                 code errorcode = general);

    // a function that is capable of recovering from an error (for
    // example, by performing a different action) can reset the
    // object so it can be passed up.
    void reset ();

    // call this when reporting the error to clear the "checked" flag
    void willbereported () const;

    // for std::exception. this lets you throw an error that should
    // terminate the application. the what() message will be less
    // descriptive so ideally you should catch the error object instead.
    char const* what () const noexcept;

    static string const getreasontextforcode (code code);

private:
    code m_code;
    string m_reasontext;
    string m_sourcefilename;
    int m_linenumber;
    mutable bool m_needstobechecked;
    mutable string m_what; // created on demand
    mutable char const* m_szwhat;
};

}

#endif
