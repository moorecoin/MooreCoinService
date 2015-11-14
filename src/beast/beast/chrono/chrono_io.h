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

//  chrono_io
//
//  (c) copyright howard hinnant
//  use, modification and distribution are subject to the boost software license,
//  version 1.0. (see accompanying file license_1_0.txt or copy at
//  http://www.boost.org/license_1_0.txt).

#ifndef beast_chrono_chrono_io_h_included
#define beast_chrono_chrono_io_h_included

#include <beast/config.h>

#include <beast/utility/noexcept.h>
#include <ctime>
#include <locale>
    
/*

    chrono_io synopsis

#include <chrono>
#include <ratio_io>

namespace std
{
namespace chrono
{

enum duration_style {prefix, symbol};
enum timezone {utc, local};

// facets

class durationpunct
    : public locale::facet
{
public:
    static locale::id id;

    explicit durationpunct(size_t refs = 0);
    explicit durationpunct(duration_style fmt, size_t refs = 0);
    
    bool is_symbol_name() const noexcept;
    bool is_prefix_name() const noexcept;
};

template <class chart>
class timepunct
    : public locale::facet
{
public:
    typedef basic_string<chart> string_type;

    static locale::id id;

    explicit timepunct(size_t refs = 0);
    timepunct(timezone tz, string_type fmt, size_t refs = 0);

    const string_type& fmt() const noexcept;
    std::chrono::timezone timezone() const noexcept;
};

// manipulators

class duration_fmt
{
public:
    explicit duration_fmt(duration_style f) noexcept;
    explicit operator duration_style() const noexcept;
};

unspecified time_fmt(timezone tz);
template<class chart>
    unspecified time_fmt(timezone tz, basic_string<chart> fmt);
template<class chart>
    unspecified time_fmt(timezone tz, const chart* fmt);

template<class chart, class traits>
std::basic_ostream<chart, traits>&
operator<<(std::basic_ostream<chart, traits>& os, duration_fmt d);

template<class chart, class traits>
std::basic_istream<chart, traits>&
operator>>(std::basic_istream<chart, traits>& is, duration_fmt d);

// duration i/o

template <class chart, class traits, class rep, class period>
    basic_ostream<chart, traits>&
    operator<<(basic_ostream<chart, traits>& os, const duration<rep, period>& d);

template <class chart, class traits, class rep, class period>
    basic_istream<chart, traits>&
    operator>>(basic_istream<chart, traits>& is, duration<rep, period>& d);

// system_clock i/o

template <class chart, class traits, class duration>
    basic_ostream<chart, traits>&
    operator<<(basic_ostream<chart, traits>& os,
               const time_point<system_clock, duration>& tp);

template <class chart, class traits, class duration>
    basic_istream<chart, traits>&
    operator>>(basic_istream<chart, traits>& is,
               time_point<system_clock, duration>& tp);

// steady_clock i/o

template <class chart, class traits, class duration>
    basic_ostream<chart, traits>&
    operator<<(basic_ostream<chart, traits>& os,
               const time_point<steady_clock, duration>& tp);

template <class chart, class traits, class duration>
    basic_istream<chart, traits>&
    operator>>(basic_istream<chart, traits>& is,
               time_point<steady_clock, duration>& tp);

// high_resolution_clock i/o

template <class chart, class traits, class duration>
    basic_ostream<chart, traits>&
    operator<<(basic_ostream<chart, traits>& os,
               const time_point<high_resolution_clock, duration>& tp);

template <class chart, class traits, class duration>
    basic_istream<chart, traits>&
    operator>>(basic_istream<chart, traits>& is,
               time_point<high_resolution_clock, duration>& tp);

}  // chrono
}  // std

*/

#include <chrono>
#include <beast/chrono/ratio_io.h>

//_libcpp_begin_namespace_std
namespace std {

namespace chrono
{

template <class to, class rep, class period>
to
round(const duration<rep, period>& d)
{
    to t0 = duration_cast<to>(d);
    to t1 = t0;
    ++t1;
    typedef typename common_type<to, duration<rep, period> >::type _d;
    _d diff0 = d - t0;
    _d diff1 = t1 - d;
    if (diff0 == diff1)
    {
        if (t0.count() & 1)
            return t1;
        return t0;
    }
    else if (diff0 < diff1)
        return t0;
    return t1;
}

enum duration_style {prefix, symbol};
enum timezone {utc, local};

class durationpunct
    : public locale::facet
{
private:
    duration_style __style_;
public:
    static locale::id id;

    explicit durationpunct(size_t refs = 0)
        : locale::facet(refs), __style_(prefix) {}

    explicit durationpunct(duration_style fmt, size_t refs = 0)
        : locale::facet(refs), __style_(fmt) {}

    bool is_symbol_name() const noexcept {return __style_ == symbol;}
    bool is_prefix_name() const noexcept {return __style_ == prefix;}
};

class duration_fmt
{
    duration_style form_;
public:
    explicit duration_fmt(duration_style f) noexcept : form_(f) {}
    // vfalco note disabled this for msvc
    /*explicit*/
        operator duration_style() const noexcept {return form_;}
};

template<class chart, class traits>
basic_ostream<chart, traits>&
operator <<(basic_ostream<chart, traits>& os, duration_fmt d)
{
    os.imbue(locale(os.getloc(), new durationpunct(static_cast<duration_style>(d))));
    return os;
}

template<class chart, class traits>
basic_istream<chart, traits>&
operator >>(basic_istream<chart, traits>& is, duration_fmt d)
{
    is.imbue(locale(is.getloc(), new durationpunct(static_cast<duration_style>(d))));
    return is;
}

template <class _chart, class _rep, class _period>
basic_string<_chart>
__get_unit(bool __is_long, const duration<_rep, _period>& d)
{
    if (__is_long)
    {
        _chart __p[] = {'s', 'e', 'c', 'o', 'n', 'd', 's', 0};
        basic_string<_chart> s = ratio_string<_period, _chart>::prefix() + __p;
        if (d.count() == 1 || d.count() == -1)
            s.pop_back();
        return s;
    }
    return ratio_string<_period, _chart>::symbol() + 's';
}

template <class _chart, class _rep>
basic_string<_chart>
__get_unit(bool __is_long, const duration<_rep, ratio<1> >& d)
{
    if (__is_long)
    {
        _chart __p[] = {'s', 'e', 'c', 'o', 'n', 'd', 's'};
        basic_string<_chart> s = basic_string<_chart>(__p, __p + sizeof(__p) / sizeof(_chart));
        if (d.count() == 1 || d.count() == -1)
            s.pop_back();
        return s;
    }
    return basic_string<_chart>(1, 's');
}

template <class _chart, class _rep>
basic_string<_chart>
__get_unit(bool __is_long, const duration<_rep, ratio<60> >& d)
{
    if (__is_long)
    {
        _chart __p[] = {'m', 'i', 'n', 'u', 't', 'e', 's'};
        basic_string<_chart> s = basic_string<_chart>(__p, __p + sizeof(__p) / sizeof(_chart));
        if (d.count() == 1 || d.count() == -1)
            s.pop_back();
        return s;
    }
    _chart __p[] = {'m', 'i', 'n'};
    return basic_string<_chart>(__p, __p + sizeof(__p) / sizeof(_chart));
}

template <class _chart, class _rep>
basic_string<_chart>
__get_unit(bool __is_long, const duration<_rep, ratio<3600> >& d)
{
    if (__is_long)
    {
        _chart __p[] = {'h', 'o', 'u', 'r', 's'};
        basic_string<_chart> s = basic_string<_chart>(__p, __p + sizeof(__p) / sizeof(_chart));
        if (d.count() == 1 || d.count() == -1)
            s.pop_back();
        return s;
    }
    return basic_string<_chart>(1, 'h');
}

template <class _chart, class _traits, class _rep, class _period>
basic_ostream<_chart, _traits>&
operator<<(basic_ostream<_chart, _traits>& __os, const duration<_rep, _period>& __d)
{
    typename basic_ostream<_chart, _traits>::sentry ok(__os);
    if (ok)
    {
        typedef durationpunct _f;
        typedef basic_string<_chart> string_type;
        bool failed = false;
        try
        {
            bool __is_long = true;
            locale __loc = __os.getloc();
            if (has_facet<_f>(__loc))
            {
                const _f& f = use_facet<_f>(__loc);
                __is_long = f.is_prefix_name();
            }
            string_type __unit = __get_unit<_chart>(__is_long, __d);
            __os << __d.count() << ' ' << __unit;
        }
        catch (...)
        {
            failed = true;
        }
        if (failed)
            __os.setstate(ios_base::failbit | ios_base::badbit);
    }
    return __os;
}

template <class _rep, bool = is_scalar<_rep>::value>
struct __duration_io_intermediate
{
    typedef _rep type;
};

template <class _rep>
struct __duration_io_intermediate<_rep, true>
{
    typedef typename conditional
    <
        is_floating_point<_rep>::value,
            long double,
            typename conditional
            <
                is_signed<_rep>::value,
                    long long,
                    unsigned long long
            >::type
    >::type type;
};

template <class t>
t
__gcd(t x, t y)
{
    while (y != 0)
    {
        t old_x = x;
        x = y;
        y = old_x % y;
    }
    return x;
}

template <>
long double
inline
__gcd(long double, long double)
{
    return 1;
}

template <class _chart, class _traits, class _rep, class _period>
basic_istream<_chart, _traits>&
operator>>(basic_istream<_chart, _traits>& __is, duration<_rep, _period>& __d)
{
    // these are unused and generate warnings
    //typedef basic_string<_chart> string_type;
    //typedef durationpunct _f;

    typedef typename __duration_io_intermediate<_rep>::type _ir;
    _ir __r;
    // read value into __r
    __is >> __r;
    if (__is.good())
    {
        // now determine unit
        typedef istreambuf_iterator<_chart, _traits> _i;
        _i __i(__is);
        _i __e;
        if (__i != __e && *__i == ' ')  // mandatory ' ' after value
        {
            ++__i;
            if (__i != __e)
            {
                locale __loc = __is.getloc();
                // unit is num / den (yet to be determined)
                unsigned long long num = 0;
                unsigned long long den = 0;
                ios_base::iostate __err = ios_base::goodbit;
                if (*__i == '[')
                {
                    // parse [n/d]s or [n/d]seconds format
                    ++__i;
                    _chart __x;
                    __is >> num >> __x >> den;
                    if (!__is.good() || __x != '/')
                    {
                        __is.setstate(__is.failbit);
                        return __is;
                    }
                    __i = _i(__is);
                    if (*__i != ']')
                    {
                        __is.setstate(__is.failbit);
                        return __is;
                    }
                    ++__i;
                    const basic_string<_chart> __units[] =
                    {
                        __get_unit<_chart>(true, seconds(2)),
                        __get_unit<_chart>(true, seconds(1)),
                        __get_unit<_chart>(false, seconds(1))
                    };
                    const basic_string<_chart>* __k = __scan_keyword(__i, __e,
                                  __units, __units + sizeof(__units)/sizeof(__units[0]),
                                  use_facet<ctype<_chart> >(__loc),
                                  __err);
                    switch ((__k - __units) / 3)
                    {
                    case 0:
                        break;
                    default:
                        __is.setstate(__err);
                        return __is;
                    }
                }
                else
                {
                    // parse si name, short or long
                    const basic_string<_chart> __units[] =
                    {
                        __get_unit<_chart>(true, duration<_rep, atto>(2)),
                        __get_unit<_chart>(true, duration<_rep, atto>(1)),
                        __get_unit<_chart>(false, duration<_rep, atto>(1)),
                        __get_unit<_chart>(true, duration<_rep, femto>(2)),
                        __get_unit<_chart>(true, duration<_rep, femto>(1)),
                        __get_unit<_chart>(false, duration<_rep, femto>(1)),
                        __get_unit<_chart>(true, duration<_rep, pico>(2)),
                        __get_unit<_chart>(true, duration<_rep, pico>(1)),
                        __get_unit<_chart>(false, duration<_rep, pico>(1)),
                        __get_unit<_chart>(true, duration<_rep, nano>(2)),
                        __get_unit<_chart>(true, duration<_rep, nano>(1)),
                        __get_unit<_chart>(false, duration<_rep, nano>(1)),
                        __get_unit<_chart>(true, duration<_rep, micro>(2)),
                        __get_unit<_chart>(true, duration<_rep, micro>(1)),
                        __get_unit<_chart>(false, duration<_rep, micro>(1)),
                        __get_unit<_chart>(true, duration<_rep, milli>(2)),
                        __get_unit<_chart>(true, duration<_rep, milli>(1)),
                        __get_unit<_chart>(false, duration<_rep, milli>(1)),
                        __get_unit<_chart>(true, duration<_rep, centi>(2)),
                        __get_unit<_chart>(true, duration<_rep, centi>(1)),
                        __get_unit<_chart>(false, duration<_rep, centi>(1)),
                        __get_unit<_chart>(true, duration<_rep, deci>(2)),
                        __get_unit<_chart>(true, duration<_rep, deci>(1)),
                        __get_unit<_chart>(false, duration<_rep, deci>(1)),
                        __get_unit<_chart>(true, duration<_rep, deca>(2)),
                        __get_unit<_chart>(true, duration<_rep, deca>(1)),
                        __get_unit<_chart>(false, duration<_rep, deca>(1)),
                        __get_unit<_chart>(true, duration<_rep, hecto>(2)),
                        __get_unit<_chart>(true, duration<_rep, hecto>(1)),
                        __get_unit<_chart>(false, duration<_rep, hecto>(1)),
                        __get_unit<_chart>(true, duration<_rep, kilo>(2)),
                        __get_unit<_chart>(true, duration<_rep, kilo>(1)),
                        __get_unit<_chart>(false, duration<_rep, kilo>(1)),
                        __get_unit<_chart>(true, duration<_rep, mega>(2)),
                        __get_unit<_chart>(true, duration<_rep, mega>(1)),
                        __get_unit<_chart>(false, duration<_rep, mega>(1)),
                        __get_unit<_chart>(true, duration<_rep, giga>(2)),
                        __get_unit<_chart>(true, duration<_rep, giga>(1)),
                        __get_unit<_chart>(false, duration<_rep, giga>(1)),
                        __get_unit<_chart>(true, duration<_rep, tera>(2)),
                        __get_unit<_chart>(true, duration<_rep, tera>(1)),
                        __get_unit<_chart>(false, duration<_rep, tera>(1)),
                        __get_unit<_chart>(true, duration<_rep, peta>(2)),
                        __get_unit<_chart>(true, duration<_rep, peta>(1)),
                        __get_unit<_chart>(false, duration<_rep, peta>(1)),
                        __get_unit<_chart>(true, duration<_rep, exa>(2)),
                        __get_unit<_chart>(true, duration<_rep, exa>(1)),
                        __get_unit<_chart>(false, duration<_rep, exa>(1)),
                        __get_unit<_chart>(true, duration<_rep, ratio<1> >(2)),
                        __get_unit<_chart>(true, duration<_rep, ratio<1> >(1)),
                        __get_unit<_chart>(false, duration<_rep, ratio<1> >(1)),
                        __get_unit<_chart>(true, duration<_rep, ratio<60> >(2)),
                        __get_unit<_chart>(true, duration<_rep, ratio<60> >(1)),
                        __get_unit<_chart>(false, duration<_rep, ratio<60> >(1)),
                        __get_unit<_chart>(true, duration<_rep, ratio<3600> >(2)),
                        __get_unit<_chart>(true, duration<_rep, ratio<3600> >(1)),
                        __get_unit<_chart>(false, duration<_rep, ratio<3600> >(1))
                    };
                    const basic_string<_chart>* __k = __scan_keyword(__i, __e,
                                  __units, __units + sizeof(__units)/sizeof(__units[0]),
                                  use_facet<ctype<_chart> >(__loc),
                                  __err);
                    switch (__k - __units)
                    {
                    case 0:
                    case 1:
                    case 2:
                        num = 1ull;
                        den = 1000000000000000000ull;
                        break;
                    case 3:
                    case 4:
                    case 5:
                        num = 1ull;
                        den = 1000000000000000ull;
                        break;
                    case 6:
                    case 7:
                    case 8:
                        num = 1ull;
                        den = 1000000000000ull;
                        break;
                    case 9:
                    case 10:
                    case 11:
                        num = 1ull;
                        den = 1000000000ull;
                        break;
                    case 12:
                    case 13:
                    case 14:
                        num = 1ull;
                        den = 1000000ull;
                        break;
                    case 15:
                    case 16:
                    case 17:
                        num = 1ull;
                        den = 1000ull;
                        break;
                    case 18:
                    case 19:
                    case 20:
                        num = 1ull;
                        den = 100ull;
                        break;
                    case 21:
                    case 22:
                    case 23:
                        num = 1ull;
                        den = 10ull;
                        break;
                    case 24:
                    case 25:
                    case 26:
                        num = 10ull;
                        den = 1ull;
                        break;
                    case 27:
                    case 28:
                    case 29:
                        num = 100ull;
                        den = 1ull;
                        break;
                    case 30:
                    case 31:
                    case 32:
                        num = 1000ull;
                        den = 1ull;
                        break;
                    case 33:
                    case 34:
                    case 35:
                        num = 1000000ull;
                        den = 1ull;
                        break;
                    case 36:
                    case 37:
                    case 38:
                        num = 1000000000ull;
                        den = 1ull;
                        break;
                    case 39:
                    case 40:
                    case 41:
                        num = 1000000000000ull;
                        den = 1ull;
                        break;
                    case 42:
                    case 43:
                    case 44:
                        num = 1000000000000000ull;
                        den = 1ull;
                        break;
                    case 45:
                    case 46:
                    case 47:
                        num = 1000000000000000000ull;
                        den = 1ull;
                        break;
                    case 48:
                    case 49:
                    case 50:
                        num = 1;
                        den = 1;
                        break;
                    case 51:
                    case 52:
                    case 53:
                        num = 60;
                        den = 1;
                        break;
                    case 54:
                    case 55:
                    case 56:
                        num = 3600;
                        den = 1;
                        break;
                    default:
                        __is.setstate(__err);
                        return __is;
                    }
                }
                // unit is num/den
                // __r should be multiplied by (num/den) / _period
                // reduce (num/den) / _period to lowest terms
                unsigned long long __gcd_n1_n2 = __gcd<unsigned long long>(num, _period::num);
                unsigned long long __gcd_d1_d2 = __gcd<unsigned long long>(den, _period::den);
                num /= __gcd_n1_n2;
                den /= __gcd_d1_d2;
                unsigned long long __n2 = _period::num / __gcd_n1_n2;
                unsigned long long __d2 = _period::den / __gcd_d1_d2;
                if (num > numeric_limits<unsigned long long>::max() / __d2 ||
                    den > numeric_limits<unsigned long long>::max() / __n2)
                {
                    // (num/den) / _period overflows
                    __is.setstate(__is.failbit);
                    return __is;
                }
                num *= __d2;
                den *= __n2;
                // num / den is now factor to multiply by __r
                typedef typename common_type<_ir, unsigned long long>::type _ct;
                if (is_integral<_ir>::value)
                {
                    // reduce __r * num / den
                    _ct __t = __gcd<_ct>(__r, den);
                    __r /= __t;
                    den /= __t;
                    if (den != 1)
                    {
                        // conversion to _period is integral and not exact
                        __is.setstate(__is.failbit);
                        return __is;
                    }
                }
                if (__r > duration_values<_ct>::max() / num)
                {
                    // conversion to _period overflowed
                    __is.setstate(__is.failbit);
                    return __is;
                }
                _ct __t = __r * num;
                __t /= den;
                if (duration_values<_rep>::max() < __t)
                {
                    // conversion to _period overflowed
                    __is.setstate(__is.failbit);
                    return __is;
                }
                // success!  store it.
                __r = _rep(__t);
                __d = duration<_rep, _period>(__r);
                __is.setstate(__err);
            }
            else
                __is.setstate(__is.failbit | __is.eofbit);
        }
        else
        {
            if (__i == __e)
                __is.setstate(__is.eofbit);
            __is.setstate(__is.failbit);
        }
    }
    else
        __is.setstate(__is.failbit);
    return __is;
}

template <class chart>
class timepunct
    : public locale::facet
{
public:
    typedef basic_string<chart> string_type;

private:
    string_type           fmt_;
    chrono::timezone tz_;

public:
    static locale::id id;

    explicit timepunct(size_t refs = 0)
        : locale::facet(refs), tz_(utc) {}
    timepunct(timezone tz, string_type fmt, size_t refs = 0)
        : locale::facet(refs), fmt_(std::move(fmt)), tz_(tz) {}

    const string_type& fmt() const noexcept {return fmt_;}
    chrono::timezone get_timezone() const noexcept {return tz_;}
};

template <class chart>
locale::id
timepunct<chart>::id;

template <class _chart, class _traits, class _duration>
basic_ostream<_chart, _traits>&
operator<<(basic_ostream<_chart, _traits>& __os,
           const time_point<steady_clock, _duration>& __tp)
{
    return __os << __tp.time_since_epoch() << " since boot";
}

template<class chart>
struct __time_manip
{
    basic_string<chart> fmt_;
    timezone tz_;

    __time_manip(timezone tz, basic_string<chart> fmt)
        : fmt_(std::move(fmt)),
          tz_(tz) {}
};

template<class chart, class traits>
basic_ostream<chart, traits>&
operator <<(basic_ostream<chart, traits>& os, __time_manip<chart> m)
{
    os.imbue(locale(os.getloc(), new timepunct<chart>(m.tz_, std::move(m.fmt_))));
    return os;
}

template<class chart, class traits>
basic_istream<chart, traits>&
operator >>(basic_istream<chart, traits>& is, __time_manip<chart> m)
{
    is.imbue(locale(is.getloc(), new timepunct<chart>(m.tz_, std::move(m.fmt_))));
    return is;
}

template<class chart>
inline
__time_manip<chart>
time_fmt(timezone tz, const chart* fmt)
{
    return __time_manip<chart>(tz, fmt);
}

template<class chart>
inline
__time_manip<chart>
time_fmt(timezone tz, basic_string<chart> fmt)
{
    return __time_manip<chart>(tz, std::move(fmt));
}

class __time_man
{
    timezone form_;
public:
    explicit __time_man(timezone f) : form_(f) {}
    // explicit
        operator timezone() const {return form_;}
};

template<class chart, class traits>
basic_ostream<chart, traits>&
operator <<(basic_ostream<chart, traits>& os, __time_man m)
{
    os.imbue(locale(os.getloc(), new timepunct<chart>(static_cast<timezone>(m), basic_string<chart>())));
    return os;
}

template<class chart, class traits>
basic_istream<chart, traits>&
operator >>(basic_istream<chart, traits>& is, __time_man m)
{
    is.imbue(locale(is.getloc(), new timepunct<chart>(static_cast<timezone>(m), basic_string<chart>())));
    return is;
}

inline
__time_man
time_fmt(timezone f)
{
    return __time_man(f);
}

} // chrono

}

#endif

