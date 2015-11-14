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

void* getuser32function (const char* functionname)
{
    hmodule module = getmodulehandlea ("user32.dll");
    bassert (module != 0);
    return (void*) getprocaddress (module, functionname);
}

//==============================================================================
#if ! beast_use_intrinsics
// in newer compilers, the inline versions of these are used (in beast_atomic.h), but in
// older ones we have to actually call the ops as win32 functions..
long beast_interlockedexchange (volatile long* a, long b) noexcept                { return interlockedexchange (a, b); }
long beast_interlockedincrement (volatile long* a) noexcept                       { return interlockedincrement (a); }
long beast_interlockeddecrement (volatile long* a) noexcept                       { return interlockeddecrement (a); }
long beast_interlockedexchangeadd (volatile long* a, long b) noexcept             { return interlockedexchangeadd (a, b); }
long beast_interlockedcompareexchange (volatile long* a, long b, long c) noexcept { return interlockedcompareexchange (a, b, c); }

__int64 beast_interlockedcompareexchange64 (volatile __int64* value, __int64 newvalue, __int64 valuetocompare) noexcept
{
    bassertfalse; // this operation isn't available in old ms compiler versions!

    __int64 oldvalue = *value;
    if (oldvalue == valuetocompare)
        *value = newvalue;

    return oldvalue;
}

#endif

//==============================================================================


criticalsection::criticalsection() noexcept
{
    // (just to check the ms haven't changed this structure and broken things...)
#if beast_vc7_or_earlier
    static_assert (sizeof (critical_section) <= 24,
        "the size of the critical_section structure is less not what we expected.");
#else
    static_assert (sizeof (critical_section) <= sizeof (criticalsection::section),
        "the size of the critical_section structure is less not what we expected.");
#endif

    initializecriticalsection ((critical_section*) section);
}

criticalsection::~criticalsection() noexcept { deletecriticalsection ((critical_section*) section); }
void criticalsection::enter() const noexcept { entercriticalsection ((critical_section*) section); }
bool criticalsection::tryenter() const noexcept { return tryentercriticalsection ((critical_section*) section) != false; }
void criticalsection::exit() const noexcept { leavecriticalsection ((critical_section*) section); }

//==============================================================================
bool beast_isrunningunderdebugger()
{
    return isdebuggerpresent() != false;
}

bool process::isrunningunderdebugger()
{
    return beast_isrunningunderdebugger();
}

//------------------------------------------------------------------------------
void process::terminate()
{
   #if beast_msvc && beast_check_memory_leaks
    _crtdumpmemoryleaks();
   #endif

    // bullet in the head in case there's a problem shutting down..
    exitprocess (0);
}

//==============================================================================

} // beast
