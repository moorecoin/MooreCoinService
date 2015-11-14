//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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

#ifndef beast_iosformat_h_included
#define beast_iosformat_h_included

#include <ostream>
#include <sstream>
#include <string>

namespace beast {

// a collection of handy stream manipulators and
// functions to produce nice looking log output.

/** left justifies a field at the specified width. */
struct leftw
{
    explicit leftw (int width_)
        : width (width_)
        { }
    int const width;
    template <class chart, class traits>
    friend std::basic_ios <chart, traits>& operator<< (
        std::basic_ios <chart, traits>& ios, leftw const& p)
    {
        ios.setf (std::ios_base::left, std::ios_base::adjustfield);
        ios.width (p.width);
        return ios;
    }
};

/** produce a section heading and fill the rest of the line with dashes. */
template <class chart, class traits, class allocator>
std::basic_string <chart, traits, allocator> heading (
    std::basic_string <chart, traits, allocator> title,
        int width = 80, chart fill = chart ('-'))
{
    title.reserve (width);
    title.push_back (chart (' '));
    title.resize (width, fill);
    return title;
}

/** produce a dashed line separator, with a specified or default size. */
struct divider
{
    typedef char chart;
    explicit divider (int width_ = 80, chart fill_ = chart ('-'))
        : width (width_)
        , fill (fill_)
        { }
    int const width;
    chart const fill;
    template <class chart, class traits>
    friend std::basic_ostream <chart, traits>& operator<< (
        std::basic_ostream <chart, traits>& os, divider const& d)
    {
        os << std::basic_string <chart, traits> (d.width, d.fill);
        return os;
    }
};

/** creates a padded field with an optiona fill character. */
struct fpad
{
    explicit fpad (int width_, int pad_ = 0, char fill_ = ' ')
        : width (width_ + pad_)
        , fill (fill_)
        { }
    int const width;
    char const fill;
    template <class chart, class traits>
    friend std::basic_ostream <chart, traits>& operator<< (
        std::basic_ostream <chart, traits>& os, fpad const& f)
    {
        os << std::basic_string <chart, traits> (f.width, f.fill);
        return os;
    }
};

//------------------------------------------------------------------------------

namespace detail {

template <typename t>
std::string to_string (t const& t)
{
    std::stringstream ss;
    ss << t;
    return ss.str();
}

}

/** justifies a field at the specified width. */
/** @{ */
template <class chart,
          class traits = std::char_traits <chart>,
          class allocator = std::allocator <chart>>
class field_t
{
public:
    typedef std::basic_string <chart, traits, allocator> string_t;
    field_t (string_t const& text_, int width_, int pad_, bool right_)
        : text (text_)
        , width (width_)
        , pad (pad_)
        , right (right_)
        { }
    string_t const text;
    int const width;
    int const pad;
    bool const right;
    template <class chart2, class traits2>
    friend std::basic_ostream <chart2, traits2>& operator<< (
        std::basic_ostream <chart2, traits2>& os,
            field_t <chart, traits, allocator> const& f)
    {
        std::size_t const length (f.text.length());
        if (f.right)
        {
            if (length < f.width)
                os << std::basic_string <chart2, traits2> (
                    f.width - length, chart2 (' '));
            os << f.text;
        }
        else
        {
            os << f.text;
            if (length < f.width)
                os << std::basic_string <chart2, traits2> (
                    f.width - length, chart2 (' '));
        }
        if (f.pad != 0)
            os << string_t (f.pad, chart (' '));
        return os;
    }
};

template <class chart, class traits, class allocator>
field_t <chart, traits, allocator> field (
    std::basic_string <chart, traits, allocator> const& text,
        int width = 8, int pad = 0, bool right = false)
{
    return field_t <chart, traits, allocator> (
        text, width, pad, right);
}

template <class chart>
field_t <chart> field (
    chart const* text, int width = 8, int pad = 0, bool right = false)
{
    return field_t <chart, std::char_traits <chart>,
        std::allocator <chart>> (std::basic_string <chart,
            std::char_traits <chart>, std::allocator <chart>> (text),
                width, pad, right);
}

template <typename t>
field_t <char> field (
    t const& t, int width = 8, int pad = 0, bool right = false)
{
    std::string const text (detail::to_string (t));
    return field (text, width, pad, right);
}

template <class chart, class traits, class allocator>
field_t <chart, traits, allocator> rfield (
    std::basic_string <chart, traits, allocator> const& text,
        int width = 8, int pad = 0)
{
    return field_t <chart, traits, allocator> (
        text, width, pad, true);
}

template <class chart>
field_t <chart> rfield (
    chart const* text, int width = 8, int pad = 0)
{
    return field_t <chart, std::char_traits <chart>,
        std::allocator <chart>> (std::basic_string <chart,
            std::char_traits <chart>, std::allocator <chart>> (text),
                width, pad, true);
}

template <typename t>
field_t <char> rfield (
    t const& t, int width = 8, int pad = 0)
{
    std::string const text (detail::to_string (t));
    return field (text, width, pad, true);
}
/** @} */

}

#endif
