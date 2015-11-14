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

#ifndef beast_result_h_included
#define beast_result_h_included

namespace beast
{

/** represents the 'success' or 'failure' of an operation, and holds an associated
    error message to describe the error when there's a failure.

    e.g.
    @code
    result myoperation()
    {
        if (dosomekindoffoobar())
            return result::ok();
        else
            return result::fail ("foobar didn't work!");
    }

    const result result (myoperation());

    if (result.wasok())
    {
        ...it's all good...
    }
    else
    {
        warnuseraboutfailure ("the foobar operation failed! error message was: "
                                + result.geterrormessage());
    }
    @endcode
*/
class  result
{
public:
    //==============================================================================
    /** creates and returns a 'successful' result. */
    static result ok() noexcept { return result(); }

    /** creates a 'failure' result.
        if you pass a blank error message in here, a default "unknown error" message
        will be used instead.
    */
    static result fail (const string& errormessage) noexcept;

    //==============================================================================
    /** returns true if this result indicates a success. */
    bool wasok() const noexcept;

    /** returns true if this result indicates a failure.
        you can use geterrormessage() to retrieve the error message associated
        with the failure.
    */
    bool failed() const noexcept;

    /** returns true if this result indicates a success.
        this is equivalent to calling wasok().
    */
    operator bool() const noexcept;

    /** returns true if this result indicates a failure.
        this is equivalent to calling failed().
    */
    bool operator!() const noexcept;

    /** returns the error message that was set when this result was created.
        for a successful result, this will be an empty string;
    */
    const string& geterrormessage() const noexcept;

    //==============================================================================
    result (const result&);
    result& operator= (const result&);

   #if beast_compiler_supports_move_semantics
    result (result&&) noexcept;
    result& operator= (result&&) noexcept;
   #endif

    bool operator== (const result& other) const noexcept;
    bool operator!= (const result& other) const noexcept;

private:
    string errormessage;

    // the default constructor is not for public use!
    // instead, use result::ok() or result::fail()
    result() noexcept;
    explicit result (const string&) noexcept;

    // these casts are private to prevent people trying to use the result object in numeric contexts
    operator int() const;
    operator void*() const;
};

} // beast

#endif

