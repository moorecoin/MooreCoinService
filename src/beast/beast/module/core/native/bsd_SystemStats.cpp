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

void outputdebugstring (std::string const& text)
{
    std::cerr << text << std::endl;
}

//==============================================================================
std::string systemstats::getcomputername()
{
    char name [256] = { 0 };
    if (gethostname (name, sizeof (name) - 1) == 0)
        return name;

    return std::string{};
}

//==============================================================================
std::uint32_t beast_millisecondssincestartup() noexcept
{
    timespec t;
    clock_gettime (clock_monotonic, &t);

    return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

std::int64_t time::gethighresolutionticks() noexcept
{
    timespec t;
    clock_gettime (clock_monotonic, &t);

    return (t.tv_sec * (std::int64_t) 1000000) + (t.tv_nsec / 1000);
}

std::int64_t time::gethighresolutiontickspersecond() noexcept
{
    return 1000000;  // (microseconds)
}

double time::getmillisecondcounterhires() noexcept
{
    return gethighresolutionticks() * 0.001;
}

} // beast
