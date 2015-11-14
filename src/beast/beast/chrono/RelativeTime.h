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

#ifndef beast_chrono_relativetime_h_included
#define beast_chrono_relativetime_h_included

#include <beast/config.h>
#include <beast/strings/string.h>

#include <beast/utility/noexcept.h>
#include <string>
#include <sstream>

namespace beast {

//==============================================================================
/** a relative measure of time.

    the time is stored as a number of seconds, at double-precision floating
    point accuracy, and may be positive or negative.

    if you need an absolute time, (i.e. a date + time), see the time class.
*/
class  relativetime
{
public:
    //==============================================================================
    /** the underlying data type used by relativetime.

        if you need to get to the underlying time and manipulate it
        you can use this to declare a type that is guaranteed to
        work cleanly.
    */
    typedef double value_type;

    //==============================================================================
    /** creates a relativetime.

        @param seconds  the number of seconds, which may be +ve or -ve.
        @see milliseconds, minutes, hours, days, weeks
    */
    explicit relativetime (value_type seconds = 0.0) noexcept;

    /** copies another relative time. */
    relativetime (const relativetime& other) noexcept;

    /** copies another relative time. */
    relativetime& operator= (const relativetime& other) noexcept;

    /** destructor. */
    ~relativetime() noexcept;

    bool iszero() const
        { return numseconds == 0; }

    bool isnotzero() const
        { return numseconds != 0; }

    /** returns the amount of time since the process was started. */
    static relativetime fromstartup ();

    //==============================================================================
    /** creates a new relativetime object representing a number of milliseconds.
        @see seconds, minutes, hours, days, weeks
    */
    static relativetime milliseconds (int milliseconds) noexcept;

    /** creates a new relativetime object representing a number of milliseconds.
        @see seconds, minutes, hours, days, weeks
    */
    static relativetime milliseconds (std::int64_t milliseconds) noexcept;

    /** creates a new relativetime object representing a number of seconds.
        @see milliseconds, minutes, hours, days, weeks
    */
    static relativetime seconds (value_type seconds) noexcept;

    /** creates a new relativetime object representing a number of minutes.
        @see milliseconds, hours, days, weeks
    */
    static relativetime minutes (value_type numberofminutes) noexcept;

    /** creates a new relativetime object representing a number of hours.
        @see milliseconds, minutes, days, weeks
    */
    static relativetime hours (value_type numberofhours) noexcept;

    /** creates a new relativetime object representing a number of days.
        @see milliseconds, minutes, hours, weeks
    */
    static relativetime days (value_type numberofdays) noexcept;

    /** creates a new relativetime object representing a number of weeks.
        @see milliseconds, minutes, hours, days
    */
    static relativetime weeks (value_type numberofweeks) noexcept;

    //==============================================================================
    /** returns the number of milliseconds this time represents.
        @see milliseconds, inseconds, inminutes, inhours, indays, inweeks
    */
    std::int64_t inmilliseconds() const noexcept;

    /** returns the number of seconds this time represents.
        @see inmilliseconds, inminutes, inhours, indays, inweeks
    */
    value_type inseconds() const noexcept       { return numseconds; }

    /** returns the number of minutes this time represents.
        @see inmilliseconds, inseconds, inhours, indays, inweeks
    */
    value_type inminutes() const noexcept;

    /** returns the number of hours this time represents.
        @see inmilliseconds, inseconds, inminutes, indays, inweeks
    */
    value_type inhours() const noexcept;

    /** returns the number of days this time represents.
        @see inmilliseconds, inseconds, inminutes, inhours, inweeks
    */
    value_type indays() const noexcept;

    /** returns the number of weeks this time represents.
        @see inmilliseconds, inseconds, inminutes, inhours, indays
    */
    value_type inweeks() const noexcept;

    /** returns a readable textual description of the time.

        the exact format of the string returned will depend on
        the magnitude of the time - e.g.

        "1 min 4 secs", "1 hr 45 mins", "2 weeks 5 days", "140 ms"

        so that only the two most significant units are printed.

        the returnvalueforzerotime value is the result that is returned if the
        length is zero. depending on your application you might want to use this
        to return something more relevant like "empty" or "0 secs", etc.

        @see inmilliseconds, inseconds, inminutes, inhours, indays, inweeks
    */
    string getdescription (const string& returnvalueforzerotime = "0") const;
    std::string to_string () const;

    template <typename number>
    relativetime operator+ (number seconds) const noexcept
        { return relativetime (numseconds + seconds); }

    template <typename number>
    relativetime operator- (number seconds) const noexcept
        { return relativetime (numseconds - seconds); }

    /** adds another relativetime to this one. */
    relativetime operator+= (relativetime timetoadd) noexcept;

    /** subtracts another relativetime from this one. */
    relativetime operator-= (relativetime timetosubtract) noexcept;

    /** adds a number of seconds to this time. */
    relativetime operator+= (value_type secondstoadd) noexcept;

    /** subtracts a number of seconds from this time. */
    relativetime operator-= (value_type secondstosubtract) noexcept;

private:
    value_type numseconds;
};

//------------------------------------------------------------------------------

bool operator== (relativetime t1, relativetime t2) noexcept;
bool operator!= (relativetime t1, relativetime t2) noexcept;
bool operator>  (relativetime t1, relativetime t2) noexcept;
bool operator<  (relativetime t1, relativetime t2) noexcept;
bool operator>= (relativetime t1, relativetime t2) noexcept;
bool operator<= (relativetime t1, relativetime t2) noexcept;

//------------------------------------------------------------------------------

/** adds two relativetimes together. */
relativetime operator+ (relativetime t1, relativetime t2) noexcept;

/** subtracts two relativetimes. */
relativetime operator- (relativetime t1, relativetime t2) noexcept;

inline std::ostream& operator<< (std::ostream& os, relativetime const& diff)
{
    os << diff.to_string();
    return os;
}

}

#endif
