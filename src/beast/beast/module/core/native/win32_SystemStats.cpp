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

void outputdebugstring (std::string const& text)
{
    outputdebugstringa ((text + "\n").c_str ());
}

//==============================================================================
std::uint32_t beast_millisecondssincestartup() noexcept
{
    return (std::uint32_t) timegettime();
}

//==============================================================================
class hirescounterhandler
{
public:
    hirescounterhandler()
        : hiresticksoffset (0)
    {
        const mmresult res = timebeginperiod (1);
        (void) res;
        bassert (res == timerr_noerror);

        large_integer f;
        queryperformancefrequency (&f);
        hirestickspersecond = f.quadpart;
        hiresticksscalefactor = 1000.0 / hirestickspersecond;
    }

    inline std::int64_t gethighresolutionticks() noexcept
    {
        large_integer ticks;
        queryperformancecounter (&ticks);

        const std::int64_t maincounterashiresticks = (beast_millisecondssincestartup() * hirestickspersecond) / 1000;
        const std::int64_t newoffset = maincounterashiresticks - ticks.quadpart;

        std::int64_t offsetdrift = newoffset - hiresticksoffset;

        // fix for a very obscure pci hardware bug that can make the counter
        // sometimes jump forwards by a few seconds..
        if (offsetdrift < 0)
            offsetdrift = -offsetdrift;

        if (offsetdrift > (hirestickspersecond >> 1))
            hiresticksoffset = newoffset;

        return ticks.quadpart + hiresticksoffset;
    }

    inline double getmillisecondcounterhires() noexcept
    {
        return gethighresolutionticks() * hiresticksscalefactor;
    }

    std::int64_t hirestickspersecond, hiresticksoffset;
    double hiresticksscalefactor;
};

static hirescounterhandler hirescounterhandler;

std::int64_t  time::gethighresolutiontickspersecond() noexcept  { return hirescounterhandler.hirestickspersecond; }
std::int64_t  time::gethighresolutionticks() noexcept           { return hirescounterhandler.gethighresolutionticks(); }
double time::getmillisecondcounterhires() noexcept       { return hirescounterhandler.getmillisecondcounterhires(); }

//==============================================================================
std::string systemstats::getcomputername()
{
    char text [max_computername_length + 2] = { 0 };
    dword len = max_computername_length + 1;
    if (!getcomputernamea (text, &len))
        text[0] = 0;
    return text;
}

} // beast
