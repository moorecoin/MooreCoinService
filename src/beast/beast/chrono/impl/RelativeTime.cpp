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

#include <beast/chrono/relativetime.h>

// vfalco todo migrate the localizable strings interfaces for this file

#ifndef needs_trans
#define needs_trans(s) (s)
#endif

#ifndef trans
#define trans(s) (s)
#endif

namespace beast {

relativetime::relativetime (const relativetime::value_type secs) noexcept
    : numseconds (secs)
{
}

relativetime::relativetime (const relativetime& other) noexcept
    : numseconds (other.numseconds)
{
}

relativetime::~relativetime() noexcept {}

//==============================================================================

relativetime relativetime::milliseconds (const int milliseconds) noexcept
{
    return relativetime (milliseconds * 0.001);
}

relativetime relativetime::milliseconds (const std::int64_t milliseconds) noexcept
{
    return relativetime (milliseconds * 0.001);
}

relativetime relativetime::seconds (relativetime::value_type s) noexcept
{
    return relativetime (s);
}

relativetime relativetime::minutes (const relativetime::value_type numberofminutes) noexcept
{
    return relativetime (numberofminutes * 60.0);
}

relativetime relativetime::hours (const relativetime::value_type numberofhours) noexcept
{
    return relativetime (numberofhours * (60.0 * 60.0));
}

relativetime relativetime::days (const relativetime::value_type numberofdays) noexcept
{
    return relativetime (numberofdays  * (60.0 * 60.0 * 24.0));
}

relativetime relativetime::weeks (const relativetime::value_type numberofweeks) noexcept
{
    return relativetime (numberofweeks * (60.0 * 60.0 * 24.0 * 7.0));
}

//==============================================================================

std::int64_t relativetime::inmilliseconds() const noexcept
{
    return (std::int64_t) (numseconds * 1000.0);
}

relativetime::value_type relativetime::inminutes() const noexcept
{
    return numseconds / 60.0;
}

relativetime::value_type relativetime::inhours() const noexcept
{
    return numseconds / (60.0 * 60.0);
}

relativetime::value_type relativetime::indays() const noexcept
{
    return numseconds / (60.0 * 60.0 * 24.0);
}

relativetime::value_type relativetime::inweeks() const noexcept
{
    return numseconds / (60.0 * 60.0 * 24.0 * 7.0);
}

//==============================================================================

relativetime& relativetime::operator= (const relativetime& other) noexcept      { numseconds = other.numseconds; return *this; }

relativetime relativetime::operator+= (relativetime t) noexcept
{
    numseconds += t.numseconds; return *this;
}

relativetime relativetime::operator-= (relativetime t) noexcept
{
    numseconds -= t.numseconds; return *this;
}

relativetime relativetime::operator+= (const relativetime::value_type secs) noexcept
{
    numseconds += secs; return *this;
}

relativetime relativetime::operator-= (const relativetime::value_type secs) noexcept
{
    numseconds -= secs; return *this;
}

relativetime operator+ (relativetime t1, relativetime t2) noexcept
{
    return t1 += t2;
}

relativetime operator- (relativetime t1, relativetime t2) noexcept
{
    return t1 -= t2;
}

bool operator== (relativetime t1, relativetime t2) noexcept
{
    return t1.inseconds() == t2.inseconds();
}

bool operator!= (relativetime t1, relativetime t2) noexcept
{
    return t1.inseconds() != t2.inseconds();
}

bool operator>  (relativetime t1, relativetime t2) noexcept
{
    return t1.inseconds() >  t2.inseconds();
}

bool operator<  (relativetime t1, relativetime t2) noexcept
{
    return t1.inseconds() <  t2.inseconds();
}

bool operator>= (relativetime t1, relativetime t2) noexcept
{
    return t1.inseconds() >= t2.inseconds();
}

bool operator<= (relativetime t1, relativetime t2) noexcept
{
    return t1.inseconds() <= t2.inseconds();
}

//==============================================================================

static void translatetimefield (string& result, int n, const char* singular, const char* plural)
{
    result << trans (string((n == 1) ? singular : plural))
                .replace (n == 1 ? "1" : "2", string (n))
           << ' ';
}

string relativetime::getdescription (const string& returnvalueforzerotime) const
{
    if (numseconds < 0.001 && numseconds > -0.001)
        return returnvalueforzerotime;

    string result;
    result.preallocatebytes (32);

    if (numseconds < 0)
        result << '-';

    int fieldsshown = 0;
    int n = std::abs ((int) inweeks());
    if (n > 0)
    {
        translatetimefield (result, n, needs_trans("1 week"), needs_trans("2 weeks"));
        ++fieldsshown;
    }

    n = std::abs ((int) indays()) % 7;
    if (n > 0)
    {
        translatetimefield (result, n, needs_trans("1 day"), needs_trans("2 days"));
        ++fieldsshown;
    }

    if (fieldsshown < 2)
    {
        n = std::abs ((int) inhours()) % 24;
        if (n > 0)
        {
            translatetimefield (result, n, needs_trans("1 hour"), needs_trans("2 hours"));
            ++fieldsshown;
        }

        if (fieldsshown < 2)
        {
            n = std::abs ((int) inminutes()) % 60;
            if (n > 0)
            {
                translatetimefield (result, n, needs_trans("1 minute"), needs_trans("2 minutes"));
                ++fieldsshown;
            }

            if (fieldsshown < 2)
            {
                n = std::abs ((int) inseconds()) % 60;
                if (n > 0)
                {
                    translatetimefield (result, n, needs_trans("1 seconds"), needs_trans("2 seconds"));
                    ++fieldsshown;
                }

                if (fieldsshown == 0)
                {
                    n = std::abs ((int) inmilliseconds()) % 1000;
                    if (n > 0)
                        result << n << ' ' << trans ("ms");
                }
            }
        }
    }

    return result.trimend();
}

std::string relativetime::to_string () const
{
    return getdescription ().tostdstring();
}

}

#if beast_windows

#include <windows.h>

namespace beast {
namespace detail {

static double monotoniccurrenttimeinseconds()
{
    return gettickcount64() / 1000.0;
}
 
}
}

#elif beast_mac || beast_ios

#include <mach/mach_time.h>
#include <mach/mach.h>

namespace beast {
namespace detail {
    
static double monotoniccurrenttimeinseconds()
{
    struct staticinitializer
    {
        staticinitializer ()
        {
            double numerator;
            double denominator;

            mach_timebase_info_data_t timebase;
            (void) mach_timebase_info (&timebase);
            
            if (timebase.numer % 1000000 == 0)
            {
                numerator   = timebase.numer / 1000000.0;
                denominator = timebase.denom * 1000.0;
            }
            else
            {
                numerator   = timebase.numer;
                // vfalco note i don't understand this code
                //denominator = timebase.denom * (std::uint64_t) 1000000 * 1000.0;
                denominator = timebase.denom * 1000000000.0;
            }
            
            ratio = numerator / denominator;
        }

        double ratio;
    };
    
    static staticinitializer const data;

    return mach_absolute_time() * data.ratio;
}
}
}

#else
    
#include <time.h>

namespace beast {
namespace detail {

static double monotoniccurrenttimeinseconds()
{
    timespec t;
    clock_gettime (clock_monotonic, &t);
    return t.tv_sec + t.tv_nsec / 1000000000.0;
}

}
}
    
#endif

namespace beast {
namespace detail {

// records and returns the time from process startup
static double getstartuptime()
{
    struct staticinitializer
    {
        staticinitializer ()
        {
            when = detail::monotoniccurrenttimeinseconds();
        }
        
        double when;
    };

    static staticinitializer const data;

    return data.when;
}

// used to call getstartuptime as early as possible
struct startuptimestaticinitializer
{
    startuptimestaticinitializer ()
    {
        getstartuptime();
    }
};

static startuptimestaticinitializer startuptimestaticinitializer;
    
}

relativetime relativetime::fromstartup ()
{
    return relativetime (
        detail::monotoniccurrenttimeinseconds() - detail::getstartuptime());
}

}

