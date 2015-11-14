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

#include <beast/unit_test.h>
#include <beast/streams/debug_ostream.h>

#ifdef _msc_ver
# ifndef win32_lean_and_mean // vc_extralean
#  define win32_lean_and_mean
#include <windows.h>
#  undef win32_lean_and_mean
# else
#include <windows.h>
# endif
#endif

#include <cstdlib>

// simple main used to produce stand
// alone executables that run unit tests.
int main()
{
    using namespace beast::unit_test;

#ifdef _msc_ver
    {
        int flags = _crtsetdbgflag (_crtdbg_report_flag);
        flags |= _crtdbg_leak_check_df;
        _crtsetdbgflag (flags);
    }
#endif

    {
        beast::debug_ostream s;
        reporter r (s);
        bool failed (r.run_each (global_suites()));
        if (failed)
            return exit_failure;
        return exit_success;
    }
}
