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

#ifndef beast_http_rfc2616_h_included
#define beast_http_rfc2616_h_included

#include <boost/regex.hpp>
#include <algorithm>
#include <string>
#include <iterator>
#include <utility>
#include <vector>

namespace beast {

/** routines for performing rfc2616 compliance.
    rfc2616:
        hypertext transfer protocol -- http/1.1
        http://www.w3.org/protocols/rfc2616/rfc2616
*/
namespace rfc2616 {

/** returns `true` if `c` is linear white space.
    this excludes the crlf sequence allowed for line continuations.
*/
template <class chart>
bool
is_lws (chart c)
{
    return c == ' ' || c == '\t';
}

/** returns `true` if `c` is any whitespace character. */
template <class chart>
bool
is_white (chart c)
{
    switch (c)
    {
    case ' ':  case '\f': case '\n':
    case '\r': case '\t': case '\v':
        return true;
    };
    return false;
}

/** returns `true` if `c` is a control character. */
template <class chart>
bool
is_ctl (chart c)
{
    return c <= 31 || c >= 127;
}

/** returns `true` if `c` is a separator. */
template <class chart>
bool
is_sep (chart c)
{
    switch (c)
    {
    case '(': case ')': case '<': case '>':  case '@':
    case ',': case ';': case ':': case '\\': case '"':
    case '{': case '}': case ' ': case '\t':
        return true;
    };
    return false;
}

template <class fwditer>
fwditer
trim_left (fwditer first, fwditer last)
{
    return std::find_if_not (first, last,
        &is_white <typename fwditer::value_type>);
}

template <class fwditer>
fwditer
trim_right (fwditer first, fwditer last)
{
    if (first == last)
        return last;
    do
    {
        --last;
        if (! is_white (*last))
            return ++last;
    }
    while (last != first);
    return first;
}

template <class chart, class traits, class allocator>
void
trim_right_in_place (std::basic_string <
    chart, traits, allocator>& s)
{
    s.resize (std::distance (s.begin(),
        trim_right (s.begin(), s.end())));
}

template <class fwditer>
std::pair <fwditer, fwditer>
trim (fwditer first, fwditer last)
{
    first = trim_left (first, last);
    last = trim_right (first, last);
    return std::make_pair (first, last);
}

template <class string>
string
trim (string const& s)
{
    using std::begin;
    using std::end;
    auto first = begin(s);
    auto last = end(s);
    std::tie (first, last) = trim (first, last);
    return { first, last };
}

template <class string>
string
trim_right (string const& s)
{
    using std::begin;
    using std::end;
    auto first (begin(s));
    auto last (end(s));
    last = trim_right (first, last);
    return { first, last };
}

inline
std::string
trim (std::string const& s)
{
    return trim <std::string> (s);
}

/** parse a character sequence of values separated by commas.
    double quotes and escape sequences will be converted.  excess white
    space, commas, double quotes, and empty elements are not copied.
    format:
       #(token|quoted-string)
    reference:
        http://www.w3.org/protocols/rfc2616/rfc2616-sec2.html#sec2
*/
template <class fwdit,
    class result = std::vector<
        std::basic_string<typename fwdit::value_type>>,
            class char>
result
split(fwdit first, fwdit last, char delim)
{
    result result;
    using string = typename result::value_type;
    fwdit iter = first;
    string e;
    while (iter != last)
    {
        if (*iter == '"')
        {
            // quoted-string
            ++iter;
            while (iter != last)
            {
                if (*iter == '"')
                {
                    ++iter;
                    break;
                }

                if (*iter == '\\')
                {
                    // quoted-pair
                    ++iter;
                    if (iter != last)
                        e.append (1, *iter++);
                }
                else
                {
                    // qdtext
                    e.append (1, *iter++);
                }
            }
            if (! e.empty())
            {
                result.emplace_back(std::move(e));
                e.clear();
            }
        }
        else if (*iter == delim)
        {
            e = trim_right (e);
            if (! e.empty())
            {
                result.emplace_back(std::move(e));
                e.clear();
            }
            ++iter;
        }
        else if (is_lws (*iter))
        {
            ++iter;
        }
        else
        {
            e.append (1, *iter++);
        }
    }

    if (! e.empty())
    {
        e = trim_right (e);
        if (! e.empty())
            result.emplace_back(std::move(e));
    }
    return result;
}

template <class fwdit,
    class result = std::vector<
        std::basic_string<typename fwdit::value_type>>>
result
split_commas(fwdit first, fwdit last)
{
    return split(first, last, ',');
}

template <class result = std::vector<std::string>>
result
split_commas(std::string const& s)
{
    return split_commas(s.begin(), s.end());
}

} // rfc2616

} // beast

#endif

