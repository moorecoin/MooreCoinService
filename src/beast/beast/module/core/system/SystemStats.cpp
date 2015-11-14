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

#include <cstdlib>
#include <iterator>
#include <memory>

// some basic tests, to keep an eye on things and make sure these types work ok
// on all platforms.

static_assert (sizeof (std::intptr_t) == sizeof (void*), "std::intptr_t must be the same size as void*");

static_assert (sizeof (std::int8_t) == 1,   "std::int8_t must be exactly 1 byte!");
static_assert (sizeof (std::int16_t) == 2,  "std::int16_t must be exactly 2 bytes!");
static_assert (sizeof (std::int32_t) == 4,  "std::int32_t must be exactly 4 bytes!");
static_assert (sizeof (std::int64_t) == 8,  "std::int64_t must be exactly 8 bytes!");

static_assert (sizeof (std::uint8_t) == 1,  "std::uint8_t must be exactly 1 byte!");
static_assert (sizeof (std::uint16_t) == 2, "std::uint16_t must be exactly 2 bytes!");
static_assert (sizeof (std::uint32_t) == 4, "std::uint32_t must be exactly 4 bytes!");
static_assert (sizeof (std::uint64_t) == 8, "std::uint64_t must be exactly 8 bytes!");

namespace beast
{

std::string
systemstats::getbeastversion()
{
    return "beast v" + std::to_string (beast_major_version) +
                 "." + std::to_string (beast_minor_version) +
                 "." + std::to_string (beast_buildnumber);
}

//==============================================================================
#if beast_linux
namespace linuxstatshelpers
{
    string getcpuinfo (const char* const key)
    {
        stringarray lines;
        file cpuinfo ("/proc/cpuinfo");
        
        if (cpuinfo.existsasfile())
        {
            fileinputstream in (cpuinfo);
            
            if (in.openedok())
                lines.addlines (in.readentirestreamasstring());
        }
        
        for (int i = lines.size(); --i >= 0;) // (nb - it's important that this runs in reverse order)
            if (lines[i].startswithignorecase (key))
                return lines[i].fromfirstoccurrenceof (":", false, false).trim();
        
        return string::empty;
    }
}
#endif // beast_linux

struct cpuinformation
{
    cpuinformation() noexcept
        : hasmmx (false), hassse (false),
          hassse2 (false), hassse3 (false), has3dnow (false),
          hassse4 (false), hasavx (false), hasavx2 (false)
    {
#if beast_linux
        const string flags (linuxstatshelpers::getcpuinfo ("flags"));
        hasmmx = flags.contains ("mmx");
        hassse = flags.contains ("sse");
        hassse2 = flags.contains ("sse2");
        hassse3 = flags.contains ("sse3");
        has3dnow = flags.contains ("3dnow");
        hassse4 = flags.contains ("sse4_1") || flags.contains ("sse4_2");
        hasavx = flags.contains ("avx");
        hasavx2 = flags.contains ("avx2");
#endif // beast_linux
    }

    bool hasmmx, hassse, hassse2, hassse3, has3dnow, hassse4, hasavx, hasavx2;
};

static const cpuinformation& getcpuinformation() noexcept
{
    static cpuinformation info;
    return info;
}

bool systemstats::hasmmx() noexcept { return getcpuinformation().hasmmx; }
bool systemstats::hassse() noexcept { return getcpuinformation().hassse; }
bool systemstats::hassse2() noexcept { return getcpuinformation().hassse2; }
bool systemstats::hassse3() noexcept { return getcpuinformation().hassse3; }
bool systemstats::has3dnow() noexcept { return getcpuinformation().has3dnow; }
bool systemstats::hassse4() noexcept { return getcpuinformation().hassse4; }
bool systemstats::hasavx() noexcept { return getcpuinformation().hasavx; }
bool systemstats::hasavx2() noexcept { return getcpuinformation().hasavx2; }

//==============================================================================
std::vector <std::string>
systemstats::getstackbacktrace()
{
    std::vector <std::string> result;

#if beast_android || beast_mingw || beast_bsd
    bassertfalse; // sorry, not implemented yet!

#elif beast_windows
    handle process = getcurrentprocess();
    syminitialize (process, nullptr, true);

    void* stack[128];
    int frames = (int) capturestackbacktrace (0,
        std::distance(std::begin(stack), std::end(stack)),
        stack, nullptr);

    heapblock<symbol_info> symbol;
    symbol.calloc (sizeof (symbol_info) + 256, 1);
    symbol->maxnamelen = 255;
    symbol->sizeofstruct = sizeof (symbol_info);

    for (int i = 0; i < frames; ++i)
    {
        dword64 displacement = 0;

        if (symfromaddr (process, (dword64) stack[i], &displacement, symbol))
        {
            std::string frame;

            frame.append (std::to_string (i) + ": ");

            imagehlp_module64 moduleinfo;
            zerostruct (moduleinfo);
            moduleinfo.sizeofstruct = sizeof (moduleinfo);

            if (::symgetmoduleinfo64 (process, symbol->modbase, &moduleinfo))
            {
                frame.append (moduleinfo.modulename);
                frame.append (": ");
            }

            frame.append (symbol->name);

            if (displacement)
            {
                frame.append ("+");
                frame.append (std::to_string (displacement));
            }

            result.push_back (frame);
        }
    }

#else
    void* stack[128];
    int frames = backtrace (stack,
        std::distance(std::begin(stack), std::end(stack)));

    std::unique_ptr<char*[], void(*)(void*)> frame (
        backtrace_symbols (stack, frames), std::free);

    for (int i = 0; i < frames; ++i)
        result.push_back (frame[i]);
#endif

    return result;
}

//==============================================================================
static systemstats::crashhandlerfunction globalcrashhandler = nullptr;

#if beast_windows
static long winapi handlecrash (lpexception_pointers)
{
    globalcrashhandler();
    return exception_execute_handler;
}
#else
static void handlecrash (int)
{
    globalcrashhandler();
    kill (getpid(), sigkill);
}

int beast_siginterrupt (int sig, int flag);
#endif

void systemstats::setapplicationcrashhandler (crashhandlerfunction handler)
{
    bassert (handler != nullptr); // this must be a valid function.
    globalcrashhandler = handler;

   #if beast_windows
    setunhandledexceptionfilter (handlecrash);
   #else
    const int signals[] = { sigfpe, sigill, sigsegv, sigbus, sigabrt, sigsys };

    for (int i = 0; i < numelementsinarray (signals); ++i)
    {
        ::signal (signals[i], handlecrash);
        beast_siginterrupt (signals[i], 1);
    }
   #endif
}

} // beast
