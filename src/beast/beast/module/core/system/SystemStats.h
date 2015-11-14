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

#ifndef beast_systemstats_h_included
#define beast_systemstats_h_included

namespace beast
{

//==============================================================================
/**
    contains methods for finding out about the current hardware and os configuration.
*/
namespace systemstats
{
    //==============================================================================
    /** returns the current version of beast,
        see also the beast_version, beast_major_version and beast_minor_version macros.
    */
    std::string getbeastversion();

    //==============================================================================
    /** returns the host-name of the computer. */
    std::string getcomputername();

    //==============================================================================
    bool hasmmx() noexcept; /**< returns true if intel mmx instructions are available. */
    bool hassse() noexcept; /**< returns true if intel sse instructions are available. */
    bool hassse2() noexcept; /**< returns true if intel sse2 instructions are available. */
    bool hassse3() noexcept; /**< returns true if intel sse2 instructions are available. */
    bool has3dnow() noexcept; /**< returns true if amd 3dnow instructions are available. */
    bool hassse4() noexcept; /**< returns true if intel sse4 instructions are available. */
    bool hasavx() noexcept; /**< returns true if intel avx instructions are available. */
    bool hasavx2() noexcept; /**< returns true if intel avx2 instructions are available. */

    //==============================================================================
    /** returns a backtrace of the current call-stack.
        the usefulness of the result will depend on the level of debug symbols
        that are available in the executable.
    */
    std::vector <std::string>
    getstackbacktrace();

    /** a void() function type, used by setapplicationcrashhandler(). */
    typedef void (*crashhandlerfunction)();

    /** sets up a global callback function that will be called if the application
        executes some kind of illegal instruction.

        you may want to call getstackbacktrace() in your handler function, to find out
        where the problem happened and log it, etc.
    */
    void setapplicationcrashhandler (crashhandlerfunction);
};

} // beast

#endif   // beast_systemstats_h_included
