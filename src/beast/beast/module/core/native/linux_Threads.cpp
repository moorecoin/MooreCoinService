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

namespace beast
{
//==============================================================================
bool beast_isrunningunderdebugger()
{
    static char testresult = 0;

    if (testresult == 0)
    {
        testresult = (char) ptrace (pt_trace_me, 0, 0, 0);

        if (testresult >= 0)
        {
            ptrace (pt_detach, 0, (caddr_t) 1, 0);
            testresult = 1;
        }
    }

    return testresult < 0;
}

bool process::isrunningunderdebugger()
{
    return beast_isrunningunderdebugger();
}

} // beast
