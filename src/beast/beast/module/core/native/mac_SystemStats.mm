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

scopedautoreleasepool::scopedautoreleasepool()
{
    pool = [[nsautoreleasepool alloc] init];
}

scopedautoreleasepool::~scopedautoreleasepool()
{
    [((nsautoreleasepool*) pool) release];
}

//==============================================================================
void outputdebugstring (std::string const& text)
{
    // would prefer to use std::cerr here, but avoiding it for
    // the moment, due to clang jit linkage problems.
    fputs (text.c_str (), stderr);
    fputs ("\n", stderr);
    fflush (stderr);
}

//==============================================================================

#if beast_mac
struct rlimitinitialiser
{
    rlimitinitialiser()
    {
        rlimit lim;
        getrlimit (rlimit_nofile, &lim);
        lim.rlim_cur = lim.rlim_max = rlim_infinity;
        setrlimit (rlimit_nofile, &lim);
    }
};

static rlimitinitialiser rlimitinitialiser;
#endif

//==============================================================================
std::string systemstats::getcomputername()
{
    char name [256] = { 0 };
    if (gethostname (name, sizeof (name) - 1) == 0)
        return string (name).uptolastoccurrenceof (".local", false, true).tostdstring();

    return std::string{};
}

//==============================================================================
class hirescounterhandler
{
public:
    hirescounterhandler()
    {
        mach_timebase_info_data_t timebase;
        (void) mach_timebase_info (&timebase);

        if (timebase.numer % 1000000 == 0)
        {
            numerator   = timebase.numer / 1000000;
            denominator = timebase.denom;
        }
        else
        {
            numerator   = timebase.numer;
            denominator = timebase.denom * (std::uint64_t) 1000000;
        }

        highrestimerfrequency = (timebase.denom * (std::uint64_t) 1000000000) / timebase.numer;
        highrestimertomillisecratio = numerator / (double) denominator;
    }

    inline std::uint32_t millisecondssincestartup() const noexcept
    {
        return (std::uint32_t) ((mach_absolute_time() * numerator) / denominator);
    }

    inline double getmillisecondcounterhires() const noexcept
    {
        return mach_absolute_time() * highrestimertomillisecratio;
    }

    std::int64_t highrestimerfrequency;

private:
    std::uint64_t numerator, denominator;
    double highrestimertomillisecratio;
};

static hirescounterhandler& hirescounterhandler()
{
    static hirescounterhandler hirescounterhandler;
    return hirescounterhandler;
}

std::uint32_t beast_millisecondssincestartup() noexcept         { return hirescounterhandler().millisecondssincestartup(); }
double time::getmillisecondcounterhires() noexcept      { return hirescounterhandler().getmillisecondcounterhires(); }
std::int64_t  time::gethighresolutiontickspersecond() noexcept { return hirescounterhandler().highrestimerfrequency; }
std::int64_t  time::gethighresolutionticks() noexcept          { return (std::int64_t) mach_absolute_time(); }

} // beast
