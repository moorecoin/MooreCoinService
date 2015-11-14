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

#include <beast/utility/debug.h>
#include <beast/unit_test/suite.h>
#include <beast/module/core/system/systemstats.h>

namespace beast {

namespace debug {

void breakpoint ()
{
#if beast_debug
    if (beast_isrunningunderdebugger ())
        beast_breakdebugger;

#else
    bassertfalse;

#endif
}

//------------------------------------------------------------------------------

#if beast_msvc && defined (_debug)

#if beast_check_memory_leaks
struct debugflagsinitialiser
{
    debugflagsinitialiser()
    {
        // activate leak checks on exit in the msvc debug crt (c runtime)
        //
        _crtsetdbgflag (_crtdbg_alloc_mem_df | _crtdbg_leak_check_df);
    }
};

static debugflagsinitialiser debugflagsinitialiser;
#endif

void setalwayscheckheap (bool balwayscheck)
{
    int flags = _crtsetdbgflag (_crtdbg_report_flag);

    if (balwayscheck) flags |= _crtdbg_check_always_df; // on
    else flags &= ~_crtdbg_check_always_df; // off

    _crtsetdbgflag (flags);
}

void setheapdelayedfree (bool bdelayedfree)
{
    int flags = _crtsetdbgflag (_crtdbg_report_flag);

    if (bdelayedfree) flags |= _crtdbg_delay_free_mem_df; // on
    else flags &= ~_crtdbg_delay_free_mem_df; // off

    _crtsetdbgflag (flags);
}

void setheapreportleaks (bool breportleaks)
{
    int flags = _crtsetdbgflag (_crtdbg_report_flag);

    if (breportleaks) flags |= _crtdbg_leak_check_df; // on
    else flags &= ~_crtdbg_leak_check_df; // off

    _crtsetdbgflag (flags);
}

void reportleaks ()
{
    _crtdumpmemoryleaks ();
}

void checkheap ()
{
    _crtcheckmemory ();
}

//------------------------------------------------------------------------------

#else

void setalwayscheckheap (bool)
{
}

void setheapdelayedfree (bool)
{
}

void setheapreportleaks (bool)
{
}

void reportleaks ()
{
}

void checkheap ()
{
}

#endif

//------------------------------------------------------------------------------

string getsourcelocation (char const* filename, int linenumber,
                          int numberofparents)
{
    return getfilenamefrompath (filename, numberofparents) +
        "(" + string::fromnumber (linenumber) + ")";
}

string getfilenamefrompath (const char* sourcefilename, int numberofparents)
{
    string fullpath (sourcefilename);

#if beast_windows
    // convert everything to forward slash
    fullpath = fullpath.replacecharacter ('\\', '/');
#endif

    string path;

    int choppoint = fullpath.lastindexofchar ('/');
    path = fullpath.substring (choppoint + 1);

    while (choppoint >= 0 && numberofparents > 0)
    {
        --numberofparents;
        fullpath = fullpath.substring (0, choppoint);
        choppoint = fullpath.lastindexofchar ('/');
        path = fullpath.substring (choppoint + 1) + '/' + path;
    }

    return path;
}

}

//------------------------------------------------------------------------------

// a simple unit test to determine the diagnostic settings in a build.
//
class debug_test : public unit_test::suite
{
public:
    static int envdebug ()
    {
    #ifdef _debug
        return 1;
    #else
        return 0;
    #endif
    }

    static int beastdebug ()
    {
    #ifdef beast_debug
        return beast_debug;
    #else
        return 0;
    #endif
    }

    static int beastforcedebug ()
    {
    #ifdef beast_force_debug
        return beast_force_debug;
    #else
        return 0;
    #endif
    }

    void run ()
    {
        log <<
            "_debug                           = " <<
            string::fromnumber (envdebug ());
        
        log <<
            "beast_debug                      = " <<
            string::fromnumber (beastdebug ());

        log <<
            "beast_force_debug                = " <<
            string::fromnumber (beastforcedebug ());

        log <<
            "sizeof(std::size_t)              = " <<
            string::fromnumber (sizeof(std::size_t));

        bassertfalse;

        fail ();
    }
};

beast_define_testsuite_manual(debug,utility,beast);

}
