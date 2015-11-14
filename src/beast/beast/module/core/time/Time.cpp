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

#include <algorithm>
#include <thread>

namespace beast
{

namespace timehelpers
{
    static struct tm millistolocal (const std::int64_t millis) noexcept
    {
        struct tm result;
        const std::int64_t seconds = millis / 1000;

        if (seconds < 86400ll || seconds >= 2145916800ll)
        {
            // use extended maths for dates beyond 1970 to 2037..
            const int timezoneadjustment = 31536000 - (int) (time (1971, 0, 1, 0, 0).tomilliseconds() / 1000);
            const std::int64_t jdm = seconds + timezoneadjustment + 210866803200ll;

            const int days = (int) (jdm / 86400ll);
            const int a = 32044 + days;
            const int b = (4 * a + 3) / 146097;
            const int c = a - (b * 146097) / 4;
            const int d = (4 * c + 3) / 1461;
            const int e = c - (d * 1461) / 4;
            const int m = (5 * e + 2) / 153;

            result.tm_mday  = e - (153 * m + 2) / 5 + 1;
            result.tm_mon   = m + 2 - 12 * (m / 10);
            result.tm_year  = b * 100 + d - 6700 + (m / 10);
            result.tm_wday  = (days + 1) % 7;
            result.tm_yday  = -1;

            int t = (int) (jdm % 86400ll);
            result.tm_hour  = t / 3600;
            t %= 3600;
            result.tm_min   = t / 60;
            result.tm_sec   = t % 60;
            result.tm_isdst = -1;
        }
        else
        {
            time_t now = static_cast <time_t> (seconds);

          #if beast_windows
           #ifdef _inc_time_inl
            if (now >= 0 && now <= 0x793406fff)
                localtime_s (&result, &now);
            else
                zerostruct (result);
           #else
            result = *localtime (&now);
           #endif
          #else

            localtime_r (&now, &result); // more thread-safe
          #endif
        }

        return result;
    }

    static int extendedmodulo (const std::int64_t value, const int modulo) noexcept
    {
        return (int) (value >= 0 ? (value % modulo)
                                 : (value - ((value / modulo) + 1) * modulo));
    }

    static inline string formatstring (const string& format, const struct tm* const tm)
    {
       #if beast_android
        typedef charpointer_utf8  stringtype;
       #elif beast_windows
        typedef charpointer_utf16 stringtype;
       #else
        typedef charpointer_utf32 stringtype;
       #endif

        for (size_t buffersize = 256; ; buffersize += 256)
        {
            heapblock<stringtype::chartype> buffer (buffersize);

           #if beast_android
            const size_t numchars = strftime (buffer, buffersize - 1, format.toutf8(), tm);
           #elif beast_windows
            const size_t numchars = wcsftime (buffer, buffersize - 1, format.towidecharpointer(), tm);
           #else
            const size_t numchars = wcsftime (buffer, buffersize - 1, format.toutf32(), tm);
           #endif

            if (numchars > 0)
                return string (stringtype (buffer),
                               stringtype (buffer) + (int) numchars);
        }
    }

    static std::uint32_t lastmscountervalue = 0;
}

//==============================================================================
time::time() noexcept
    : millissinceepoch (0)
{
}

time::time (const time& other) noexcept
    : millissinceepoch (other.millissinceepoch)
{
}

time::time (const std::int64_t ms) noexcept
    : millissinceepoch (ms)
{
}

time::time (const int year,
            const int month,
            const int day,
            const int hours,
            const int minutes,
            const int seconds,
            const int milliseconds,
            const bool uselocaltime) noexcept
{
    bassert (year > 100); // year must be a 4-digit version

    if (year < 1971 || year >= 2038 || ! uselocaltime)
    {
        // use extended maths for dates beyond 1970 to 2037..
        const int timezoneadjustment = uselocaltime ? (31536000 - (int) (time (1971, 0, 1, 0, 0).tomilliseconds() / 1000))
                                                    : 0;
        const int a = (13 - month) / 12;
        const int y = year + 4800 - a;
        const int jd = day + (153 * (month + 12 * a - 2) + 2) / 5
                           + (y * 365) + (y /  4) - (y / 100) + (y / 400)
                           - 32045;

        const std::int64_t s = ((std::int64_t) jd) * 86400ll - 210866803200ll;

        millissinceepoch = 1000 * (s + (hours * 3600 + minutes * 60 + seconds - timezoneadjustment))
                             + milliseconds;
    }
    else
    {
        struct tm t;
        t.tm_year   = year - 1900;
        t.tm_mon    = month;
        t.tm_mday   = day;
        t.tm_hour   = hours;
        t.tm_min    = minutes;
        t.tm_sec    = seconds;
        t.tm_isdst  = -1;

        millissinceepoch = 1000 * (std::int64_t) mktime (&t);

        if (millissinceepoch < 0)
            millissinceepoch = 0;
        else
            millissinceepoch += milliseconds;
    }
}

time::~time() noexcept
{
}

time& time::operator= (const time& other) noexcept
{
    millissinceepoch = other.millissinceepoch;
    return *this;
}

//==============================================================================
std::int64_t time::currenttimemillis() noexcept
{
  #if beast_windows
    struct _timeb t;
   #ifdef _inc_time_inl
    _ftime_s (&t);
   #else
    _ftime (&t);
   #endif
    return ((std::int64_t) t.time) * 1000 + t.millitm;
  #else
    struct timeval tv;
    gettimeofday (&tv, nullptr);
    return ((std::int64_t) tv.tv_sec) * 1000 + tv.tv_usec / 1000;
  #endif
}

time time::getcurrenttime() noexcept
{
    return time (currenttimemillis());
}

//==============================================================================
std::uint32_t beast_millisecondssincestartup() noexcept;

std::uint32_t time::getmillisecondcounter() noexcept
{
    const std::uint32_t now = beast_millisecondssincestartup();

    if (now < timehelpers::lastmscountervalue)
    {
        // in multi-threaded apps this might be called concurrently, so
        // make sure that our last counter value only increases and doesn't
        // go backwards..
        if (now < timehelpers::lastmscountervalue - 1000)
            timehelpers::lastmscountervalue = now;
    }
    else
    {
        timehelpers::lastmscountervalue = now;
    }

    return now;
}

std::uint32_t time::getapproximatemillisecondcounter() noexcept
{
    if (timehelpers::lastmscountervalue == 0)
        getmillisecondcounter();

    return timehelpers::lastmscountervalue;
}

//==============================================================================
double time::highresolutiontickstoseconds (const std::int64_t ticks) noexcept
{
    return ticks / (double) gethighresolutiontickspersecond();
}

std::int64_t time::secondstohighresolutionticks (const double seconds) noexcept
{
    return (std::int64_t) (seconds * (double) gethighresolutiontickspersecond());
}

//==============================================================================
string time::tostring (const bool includedate,
                       const bool includetime,
                       const bool includeseconds,
                       const bool use24hourclock) const noexcept
{
    string result;

    if (includedate)
    {
        result << getdayofmonth() << ' '
               << getmonthname (true) << ' '
               << getyear();

        if (includetime)
            result << ' ';
    }

    if (includetime)
    {
        const int mins = getminutes();

        result << (use24hourclock ? gethours() : gethoursinampmformat())
               << (mins < 10 ? ":0" : ":") << mins;

        if (includeseconds)
        {
            const int secs = getseconds();
            result << (secs < 10 ? ":0" : ":") << secs;
        }

        if (! use24hourclock)
            result << (isafternoon() ? "pm" : "am");
    }

    return result.trimend();
}

string time::formatted (const string& format) const
{
    struct tm t (timehelpers::millistolocal (millissinceepoch));
    return timehelpers::formatstring (format, &t);
}

//==============================================================================
int time::getyear() const noexcept          { return timehelpers::millistolocal (millissinceepoch).tm_year + 1900; }
int time::getmonth() const noexcept         { return timehelpers::millistolocal (millissinceepoch).tm_mon; }
int time::getdayofyear() const noexcept     { return timehelpers::millistolocal (millissinceepoch).tm_yday; }
int time::getdayofmonth() const noexcept    { return timehelpers::millistolocal (millissinceepoch).tm_mday; }
int time::getdayofweek() const noexcept     { return timehelpers::millistolocal (millissinceepoch).tm_wday; }
int time::gethours() const noexcept         { return timehelpers::millistolocal (millissinceepoch).tm_hour; }
int time::getminutes() const noexcept       { return timehelpers::millistolocal (millissinceepoch).tm_min; }
int time::getseconds() const noexcept       { return timehelpers::extendedmodulo (millissinceepoch / 1000, 60); }
int time::getmilliseconds() const noexcept  { return timehelpers::extendedmodulo (millissinceepoch, 1000); }

int time::gethoursinampmformat() const noexcept
{
    const int hours = gethours();

    if (hours == 0)  return 12;
    if (hours <= 12) return hours;

    return hours - 12;
}

bool time::isafternoon() const noexcept
{
    return gethours() >= 12;
}

bool time::isdaylightsavingtime() const noexcept
{
    return timehelpers::millistolocal (millissinceepoch).tm_isdst != 0;
}

string time::gettimezone() const noexcept
{
    string zone[2];

  #if beast_windows
    _tzset();

   #ifdef _inc_time_inl
    for (int i = 0; i < 2; ++i)
    {
        char name[128] = { 0 };
        size_t length;
        _get_tzname (&length, name, 127, i);
        zone[i] = name;
    }
   #else
    const char** const zoneptr = (const char**) _tzname;
    zone[0] = zoneptr[0];
    zone[1] = zoneptr[1];
   #endif
  #else
    tzset();
    const char** const zoneptr = (const char**) tzname;
    zone[0] = zoneptr[0];
    zone[1] = zoneptr[1];
  #endif

    if (isdaylightsavingtime())
    {
        zone[0] = zone[1];

        if (zone[0].length() > 3
             && zone[0].containsignorecase ("daylight")
             && zone[0].contains ("gmt"))
            zone[0] = "bst";
    }

    return zone[0].substring (0, 3);
}

string time::getmonthname (const bool threeletterversion) const
{
    return getmonthname (getmonth(), threeletterversion);
}

string time::getweekdayname (const bool threeletterversion) const
{
    return getweekdayname (getdayofweek(), threeletterversion);
}

string time::getmonthname (int monthnumber, const bool threeletterversion)
{
    const char* const shortmonthnames[] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };
    const char* const longmonthnames[]  = { "january", "february", "march", "april", "may", "june", "july", "august", "september", "october", "november", "december" };

    monthnumber %= 12;

    return threeletterversion ? shortmonthnames [monthnumber]
                                     : longmonthnames [monthnumber];
}

string time::getweekdayname (int day, const bool threeletterversion)
{
    const char* const shortdaynames[] = { "sun", "mon", "tue", "wed", "thu", "fri", "sat" };
    const char* const longdaynames[]  = { "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday" };

    day %= 7;

    return threeletterversion ? shortdaynames [day]
                                     : longdaynames [day];
}

//==============================================================================
time& time::operator+= (relativetime delta)           { millissinceepoch += delta.inmilliseconds(); return *this; }
time& time::operator-= (relativetime delta)           { millissinceepoch -= delta.inmilliseconds(); return *this; }

time operator+ (time time, relativetime delta)        { time t (time); return t += delta; }
time operator- (time time, relativetime delta)        { time t (time); return t -= delta; }
time operator+ (relativetime delta, time time)        { time t (time); return t += delta; }
const relativetime operator- (time time1, time time2) { return relativetime::milliseconds (time1.tomilliseconds() - time2.tomilliseconds()); }

bool operator== (time time1, time time2)      { return time1.tomilliseconds() == time2.tomilliseconds(); }
bool operator!= (time time1, time time2)      { return time1.tomilliseconds() != time2.tomilliseconds(); }
bool operator<  (time time1, time time2)      { return time1.tomilliseconds() <  time2.tomilliseconds(); }
bool operator>  (time time1, time time2)      { return time1.tomilliseconds() >  time2.tomilliseconds(); }
bool operator<= (time time1, time time2)      { return time1.tomilliseconds() <= time2.tomilliseconds(); }
bool operator>= (time time1, time time2)      { return time1.tomilliseconds() >= time2.tomilliseconds(); }

} // beast
