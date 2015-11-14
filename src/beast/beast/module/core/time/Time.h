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

#ifndef beast_time_h_included
#define beast_time_h_included

#include <beast/chrono/relativetime.h>

namespace beast {

//==============================================================================
/**
    holds an absolute date and time.

    internally, the time is stored at millisecond precision.

    @see relativetime
*/
class time
{
public:
    //==============================================================================
    /** creates a time object.

        this default constructor creates a time of 1st january 1970, (which is
        represented internally as 0ms).

        to create a time object representing the current time, use getcurrenttime().

        @see getcurrenttime
    */
    time() noexcept;

    /** creates a time based on a number of milliseconds.

        the internal millisecond count is set to 0 (1st january 1970). to create a
        time object set to the current time, use getcurrenttime().

        @param millisecondssinceepoch   the number of milliseconds since the unix
                                        'epoch' (midnight jan 1st 1970).
        @see getcurrenttime, currenttimemillis
    */
    explicit time (std::int64_t millisecondssinceepoch) noexcept;

    /** creates a time from a set of date components.

        the timezone is assumed to be whatever the system is using as its locale.

        @param year             the year, in 4-digit format, e.g. 2004
        @param month            the month, in the range 0 to 11
        @param day              the day of the month, in the range 1 to 31
        @param hours            hours in 24-hour clock format, 0 to 23
        @param minutes          minutes 0 to 59
        @param seconds          seconds 0 to 59
        @param milliseconds     milliseconds 0 to 999
        @param uselocaltime     if true, encode using the current machine's local time; if
                                false, it will always work in gmt.
    */
    time (int year,
          int month,
          int day,
          int hours,
          int minutes,
          int seconds = 0,
          int milliseconds = 0,
          bool uselocaltime = true) noexcept;

    /** creates a copy of another time object. */
    time (const time& other) noexcept;

    /** destructor. */
    ~time() noexcept;

    /** copies this time from another one. */
    time& operator= (const time& other) noexcept;

    //==============================================================================
    /** returns a time object that is set to the current system time.

        @see currenttimemillis
    */
    static time getcurrenttime() noexcept;

    /** returns `true` if this object represents "no time", or null.
        internally we check for milliseconds since epoch equal to zero.
    */
    /** @{ */
    bool isnull () const noexcept
    {
        return millissinceepoch == 0;
    }

    bool isnotnull () const noexcept
    {
        return millissinceepoch != 0;
    }
    /** @} */

    /** returns the time as a number of milliseconds.

        @returns    the number of milliseconds this time object represents, since
                    midnight jan 1st 1970.
        @see getmilliseconds
    */
    std::int64_t tomilliseconds() const noexcept                           { return millissinceepoch; }

    /** returns the year.

        a 4-digit format is used, e.g. 2004.
    */
    int getyear() const noexcept;

    /** returns the number of the month.

        the value returned is in the range 0 to 11.
        @see getmonthname
    */
    int getmonth() const noexcept;

    /** returns the name of the month.

        @param threeletterversion   if true, it'll be a 3-letter abbreviation, e.g. "jan"; if false
                                    it'll return the long form, e.g. "january"
        @see getmonth
    */
    string getmonthname (bool threeletterversion) const;

    /** returns the day of the month.
        the value returned is in the range 1 to 31.
    */
    int getdayofmonth() const noexcept;

    /** returns the number of the day of the week.
        the value returned is in the range 0 to 6 (0 = sunday, 1 = monday, etc).
    */
    int getdayofweek() const noexcept;

    /** returns the number of the day of the year.
        the value returned is in the range 0 to 365.
    */
    int getdayofyear() const noexcept;

    /** returns the name of the weekday.

        @param threeletterversion   if true, it'll return a 3-letter abbreviation, e.g. "tue"; if
                                    false, it'll return the full version, e.g. "tuesday".
    */
    string getweekdayname (bool threeletterversion) const;

    /** returns the number of hours since midnight.

        this is in 24-hour clock format, in the range 0 to 23.

        @see gethoursinampmformat, isafternoon
    */
    int gethours() const noexcept;

    /** returns true if the time is in the afternoon.

        so it returns true for "pm", false for "am".

        @see gethoursinampmformat, gethours
    */
    bool isafternoon() const noexcept;

    /** returns the hours in 12-hour clock format.

        this will return a value 1 to 12 - use isafternoon() to find out
        whether this is in the afternoon or morning.

        @see gethours, isafternoon
    */
    int gethoursinampmformat() const noexcept;

    /** returns the number of minutes, 0 to 59. */
    int getminutes() const noexcept;

    /** returns the number of seconds, 0 to 59. */
    int getseconds() const noexcept;

    /** returns the number of milliseconds, 0 to 999.

        unlike tomilliseconds(), this just returns the position within the
        current second rather than the total number since the epoch.

        @see tomilliseconds
    */
    int getmilliseconds() const noexcept;

    /** returns true if the local timezone uses a daylight saving correction. */
    bool isdaylightsavingtime() const noexcept;

    /** returns a 3-character string to indicate the local timezone. */
    string gettimezone() const noexcept;

    //==============================================================================
    /** quick way of getting a string version of a date and time.

        for a more powerful way of formatting the date and time, see the formatted() method.

        @param includedate      whether to include the date in the string
        @param includetime      whether to include the time in the string
        @param includeseconds   if the time is being included, this provides an option not to include
                                the seconds in it
        @param use24hourclock   if the time is being included, sets whether to use am/pm or 24
                                hour notation.
        @see formatted
    */
    string tostring (bool includedate,
                     bool includetime,
                     bool includeseconds = true,
                     bool use24hourclock = false) const noexcept;

    /** converts this date/time to a string with a user-defined format.

        this uses the c strftime() function to format this time as a string. to save you
        looking it up, these are the escape codes that strftime uses (other codes might
        work on some platforms and not others, but these are the common ones):

        %a  is replaced by the locale's abbreviated weekday name.
        %a  is replaced by the locale's full weekday name.
        %b  is replaced by the locale's abbreviated month name.
        %b  is replaced by the locale's full month name.
        %c  is replaced by the locale's appropriate date and time representation.
        %d  is replaced by the day of the month as a decimal number [01,31].
        %h  is replaced by the hour (24-hour clock) as a decimal number [00,23].
        %i  is replaced by the hour (12-hour clock) as a decimal number [01,12].
        %j  is replaced by the day of the year as a decimal number [001,366].
        %m  is replaced by the month as a decimal number [01,12].
        %m  is replaced by the minute as a decimal number [00,59].
        %p  is replaced by the locale's equivalent of either a.m. or p.m.
        %s  is replaced by the second as a decimal number [00,61].
        %u  is replaced by the week number of the year (sunday as the first day of the week) as a decimal number [00,53].
        %w  is replaced by the weekday as a decimal number [0,6], with 0 representing sunday.
        %w  is replaced by the week number of the year (monday as the first day of the week) as a decimal number [00,53]. all days in a new year preceding the first monday are considered to be in week 0.
        %x  is replaced by the locale's appropriate date representation.
        %x  is replaced by the locale's appropriate time representation.
        %y  is replaced by the year without century as a decimal number [00,99].
        %y  is replaced by the year with century as a decimal number.
        %z  is replaced by the timezone name or abbreviation, or by no bytes if no timezone information exists.
        %%  is replaced by %.

        @see tostring
    */
    string formatted (const string& format) const;

    //==============================================================================
    /** adds a relativetime to this time. */
    time& operator+= (relativetime delta);
    /** subtracts a relativetime from this time. */
    time& operator-= (relativetime delta);

    //==============================================================================
    /** returns the name of a day of the week.

        @param daynumber            the day, 0 to 6 (0 = sunday, 1 = monday, etc)
        @param threeletterversion   if true, it'll return a 3-letter abbreviation, e.g. "tue"; if
                                    false, it'll return the full version, e.g. "tuesday".
    */
    static string getweekdayname (int daynumber,
                                  bool threeletterversion);

    /** returns the name of one of the months.

        @param monthnumber  the month, 0 to 11
        @param threeletterversion   if true, it'll be a 3-letter abbreviation, e.g. "jan"; if false
                                    it'll return the long form, e.g. "january"
    */
    static string getmonthname (int monthnumber,
                                bool threeletterversion);

    //==============================================================================
    // static methods for getting system timers directly..

    /** returns the current system time.

        returns the number of milliseconds since midnight jan 1st 1970.

        should be accurate to within a few millisecs, depending on platform,
        hardware, etc.
    */
    static std::int64_t currenttimemillis() noexcept;

    /** returns the number of millisecs since a fixed event (usually system startup).

        this returns a monotonically increasing value which it unaffected by changes to the
        system clock. it should be accurate to within a few millisecs, depending on platform,
        hardware, etc.

        being a 32-bit return value, it will of course wrap back to 0 after 2^32 seconds of
        uptime, so be careful to take that into account. if you need a 64-bit time, you can
        use currenttimemillis() instead.

        @see getapproximatemillisecondcounter
    */
    static std::uint32_t getmillisecondcounter() noexcept;

    /** returns the number of millisecs since a fixed event (usually system startup).

        this has the same function as getmillisecondcounter(), but returns a more accurate
        value, using a higher-resolution timer if one is available.

        @see getmillisecondcounter
    */
    static double getmillisecondcounterhires() noexcept;

    /** less-accurate but faster version of getmillisecondcounter().

        this will return the last value that getmillisecondcounter() returned, so doesn't
        need to make a system call, but is less accurate - it shouldn't be more than
        100ms away from the correct time, though, so is still accurate enough for a
        lot of purposes.

        @see getmillisecondcounter
    */
    static std::uint32_t getapproximatemillisecondcounter() noexcept;

    //==============================================================================
    // high-resolution timers..

    /** returns the current high-resolution counter's tick-count.

        this is a similar idea to getmillisecondcounter(), but with a higher
        resolution.

        @see gethighresolutiontickspersecond, highresolutiontickstoseconds,
             secondstohighresolutionticks
    */
    static std::int64_t gethighresolutionticks() noexcept;

    /** returns the resolution of the high-resolution counter in ticks per second.

        @see gethighresolutionticks, highresolutiontickstoseconds,
             secondstohighresolutionticks
    */
    static std::int64_t gethighresolutiontickspersecond() noexcept;

    /** converts a number of high-resolution ticks into seconds.

        @see gethighresolutionticks, gethighresolutiontickspersecond,
             secondstohighresolutionticks
    */
    static double highresolutiontickstoseconds (std::int64_t ticks) noexcept;

    /** converts a number seconds into high-resolution ticks.

        @see gethighresolutionticks, gethighresolutiontickspersecond,
             highresolutiontickstoseconds
    */
    static std::int64_t secondstohighresolutionticks (double seconds) noexcept;


private:
    //==============================================================================
    std::int64_t millissinceepoch;
};

//==============================================================================
/** adds a relativetime to a time. */
time operator+ (time time, relativetime delta);
/** adds a relativetime to a time. */
time operator+ (relativetime delta, time time);

/** subtracts a relativetime from a time. */
time operator- (time time, relativetime delta);
/** returns the relative time difference between two times. */
const relativetime operator- (time time1, time time2);

/** compares two time objects. */
bool operator== (time time1, time time2);
/** compares two time objects. */
bool operator!= (time time1, time time2);
/** compares two time objects. */
bool operator<  (time time1, time time2);
/** compares two time objects. */
bool operator<= (time time1, time time2);
/** compares two time objects. */
bool operator>  (time time1, time time2);
/** compares two time objects. */
bool operator>= (time time1, time time2);

} // beast

#endif   // beast_time_h_included
