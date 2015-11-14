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

#include <beast/module/core/diagnostic/fatalerror.h>

#include <atomic>
#include <exception>
#include <iostream>
#include <mutex>

namespace beast {

//------------------------------------------------------------------------------
void
fatalerror (char const* message, char const* file, int line)
{
    static std::atomic <int> error_count (0);
    static std::recursive_mutex gate;

    // we only allow one thread to report a fatal error. other threads that
    // encounter fatal errors while we are reporting get blocked here.
    std::lock_guard<std::recursive_mutex> lock(gate);

    // if we encounter a recursive fatal error, then we want to terminate
    // unconditionally.
    if (error_count++ != 0)
        return std::terminate ();

    // we protect this entire block of code since writing to cerr might trigger
    // exceptions.
    try
    {
        std::cerr << "an error has occurred. the application will terminate.\n";

        if (message != nullptr && message [0] != 0)
            std::cerr << "message: " << message << '\n';

        if (file != nullptr && file [0] != 0)
            std::cerr << "   file: " << file << ":" << line << '\n';

        auto const backtrace = systemstats::getstackbacktrace ();

        if (!backtrace.empty ())
        {
            std::cerr << "  stack:" << std::endl;

            for (auto const& frame : backtrace)
                std::cerr << "    " << frame << '\n';
        }
    }
    catch (...)
    {
        // nothing we can do - just fall through and terminate
    }

    return std::terminate ();
}

} // beast
