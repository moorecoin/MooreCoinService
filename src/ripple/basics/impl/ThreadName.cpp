//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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

#include <beastconfig.h>

#if _msc_ver

#include <windows.h>

namespace ripple {

// vfalco todo use beast::thread::setcurrentthreadname() or something similar.

void setcallingthreadname (char const* threadname)
{
    struct threadinfo
    {
        dword dwtype;
        lpcstr szname;
        dword dwthreadid;
        dword dwflags;
    };

    threadinfo info;

    info.dwtype = 0x1000;
    info.szname = threadname;
    info.dwthreadid = getcurrentthreadid ();
    info.dwflags = 0;

    __try
    {
        // this is a visualstudio specific exception
        raiseexception (0x406d1388, 0, sizeof (info) / sizeof (ulong_ptr), (ulong_ptr*) &info);
    }
    __except (exception_continue_execution)
    {
    }
}

} // ripple

#else

namespace ripple {

#ifdef pr_set_name
#define have_name_thread
extern void setcallingthreadname (const char* n)
{
    static std::string pname;

    if (pname.empty ())
    {
        std::ifstream cline ("/proc/self/cmdline", std::ios::in);
        cline >> pname;

        if (pname.empty ())
            pname = "rippled";
        else
        {
            size_t zero = pname.find_first_of ('\0');

            if ((zero != std::string::npos) && (zero != 0))
                pname = pname.substr (0, zero);

            size_t slash = pname.find_last_of ('/');

            if (slash != std::string::npos)
                pname = pname.substr (slash + 1);
        }

        pname += " ";
    }

    // vfalco todo use beast::thread::setcurrentthreadname here
    //
    prctl (pr_set_name, (pname + n).c_str (), 0, 0, 0);
}
#endif

#ifndef have_name_thread
extern void setcallingthreadname (const char*)
{
}
#endif

} // ripple

#endif
