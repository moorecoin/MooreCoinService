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

#ifndef beast_utility_debug_ostream_h_included
#define beast_utility_debug_ostream_h_included

#include <beast/streams/abstract_ostream.h>

#include <iostream>

#ifdef _msc_ver
# ifndef win32_lean_and_mean // vc_extralean
#  define win32_lean_and_mean
#include <windows.h>
#  undef win32_lean_and_mean
# else
#include <windows.h>
# endif
# ifdef min
#  undef min
# endif
# ifdef max
#  undef max
# endif
#endif

namespace beast {

#ifdef _msc_ver
/** a basic_abstract_ostream that redirects output to an attached debugger. */
class debug_ostream
    : public abstract_ostream
{
private:
    bool m_debugger;

public:
    debug_ostream()
        : m_debugger (isdebuggerpresent() != false)
    {
        // note that the check for an attached debugger is made only
        // during construction time, for efficiency. a stream created before
        // the debugger is attached will not have output redirected.
    }

    void
    write (string_type const& s) override
    {
        if (m_debugger)
        {
            outputdebugstringa ((s + "\n").c_str());
            return;
        }
        
        std::cout << s << std::endl;
    }
};

#else
class debug_ostream
    : public abstract_ostream
{
public:
    void
    write (string_type const& s) override
    {
        std::cout << s << std::endl;
    }
};

#endif

}

#endif
