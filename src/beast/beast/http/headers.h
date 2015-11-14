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

#ifndef beast_http_headers_h_included
#define beast_http_headers_h_included

#include <beast/utility/ci_char_traits.h>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <algorithm>
#include <cctype>
#include <map>
#include <ostream>
#include <string>
#include <utility>

namespace beast {
namespace http {

/** holds a collection of http headers. */
class headers
{
public:
    using value_type = std::pair<std::string, std::string>;

private:
    struct element
        : boost::intrusive::set_base_hook <
            boost::intrusive::link_mode <
                boost::intrusive::normal_link>>
        , boost::intrusive::list_base_hook <
            boost::intrusive::link_mode <
                boost::intrusive::normal_link>>
    {
        template <class = void>
        element (std::string const& f, std::string const& v);

        value_type data;
    };

    struct less : private beast::ci_less
    {
        template <class string>
        bool
        operator() (string const& lhs, element const& rhs) const;

        template <class string>
        bool
        operator() (element const& lhs, string const& rhs) const;
    };

    struct transform
        : public std::unary_function <element, value_type>
    {
        value_type const&
        operator() (element const& e) const
        {
            return e.data;
        }
    };

    typedef boost::intrusive::make_list <element,
        boost::intrusive::constant_time_size <false>
            >::type list_t;

    typedef boost::intrusive::make_set <element,
        boost::intrusive::constant_time_size <true>
            >::type set_t;

    list_t list_;
    set_t set_;

public:
    typedef boost::transform_iterator <transform,
        list_t::const_iterator> iterator;
    typedef iterator const_iterator;

    ~headers()
    {
        clear();
    }

    headers() = default;

    headers (headers&& other);
    headers& operator= (headers&& other);

    headers (headers const& other);
    headers& operator= (headers const& other);

    /** returns an iterator to headers in order of appearance. */
    /** @{ */
    iterator
    begin() const;

    iterator
    end() const;

    iterator
    cbegin() const;

    iterator
    cend() const;
    /** @} */

    /** returns an iterator to the case-insensitive matching header. */
    template <class = void>
    iterator
    find (std::string const& field) const;

    /** returns the value for a case-insensitive matching header, or "" */
    template <class = void>
    std::string const&
    operator[] (std::string const& field) const;

    /** clear the contents of the headers. */
    template <class = void>
    void
    clear() noexcept;

    /** remove a field.
        @return the number of fields removed.
    */
    template <class = void>
    std::size_t
    erase (std::string const& field);

    /** append a field value.
        if a field value already exists the new value will be
        extended as per rfc2616 section 4.2.
    */
    // vfalco todo consider allowing rvalue references for std::move
    template <class = void>
    void
    append (std::string const& field, std::string const& value);
};

template <class = void>
std::string
to_string (headers const& h);

// hack!
template <class = void>
std::map <std::string, std::string>
build_map (headers const& h);

//------------------------------------------------------------------------------

template <class>
headers::element::element (
    std::string const& f, std::string const& v)
{
    data.first = f;
    data.second = v;
}

template <class string>
bool
headers::less::operator() (
    string const& lhs, element const& rhs) const
{
    return beast::ci_less::operator() (lhs, rhs.data.first);
}

template <class string>
bool
headers::less::operator() (
    element const& lhs, string const& rhs) const
{
    return beast::ci_less::operator() (lhs.data.first, rhs);
}

//------------------------------------------------------------------------------

inline
headers::headers (headers&& other)
    : list_ (std::move (other.list_))
    , set_ (std::move (other.set_))
{
    other.list_.clear();
    other.set_.clear();
}

inline
headers&
headers::operator= (headers&& other)
{
    list_ = std::move(other.list_);
    set_ = std::move(other.set_);
    other.list_.clear();
    other.set_.clear();
    return *this;
}

inline
headers::headers (headers const& other)
{
    for (auto const& e : other.list_)
        append (e.data.first, e.data.second);
}

inline
headers&
headers::operator= (headers const& other)
{
    clear();
    for (auto const& e : other.list_)
        append (e.data.first, e.data.second);
    return *this;
}

inline
headers::iterator
headers::begin() const
{
    return {list_.cbegin(), transform{}};
}

inline
headers::iterator
headers::end() const
{
    return {list_.cend(), transform{}};
}

inline
headers::iterator
headers::cbegin() const
{
    return {list_.cbegin(), transform{}};
}

inline
headers::iterator
headers::cend() const
{
    return {list_.cend(), transform{}};
}

template <class>
headers::iterator
headers::find (std::string const& field) const
{
    auto const iter (set_.find (field, less{}));
    if (iter == set_.end())
        return {list_.end(), transform{}};
    return {list_.iterator_to (*iter), transform{}};
}

template <class>
std::string const&
headers::operator[] (std::string const& field) const
{
    static std::string none;
    auto const found (find (field));
    if (found == end())
        return none;
    return found->second;
}

template <class>
void
headers::clear() noexcept
{
    for (auto iter (list_.begin()); iter != list_.end();)
        delete &(*iter++);
}

template <class>
std::size_t
headers::erase (std::string const& field)
{
    auto const iter = set_.find(field, less{});
    if (iter == set_.end())
        return 0;
    element& e = *iter;
    set_.erase(set_.iterator_to(e));
    list_.erase(list_.iterator_to(e));
    delete &e;
    return 1;
}

template <class>
void
headers::append (std::string const& field,
    std::string const& value)
{
    set_t::insert_commit_data d;
    auto const result (set_.insert_check (field, less{}, d));
    if (result.second)
    {
        element* const p = new element (field, value);
        list_.push_back (*p);
        set_.insert_commit (*p, d);
        return;
    }
    // if field already exists, append comma
    // separated value as per rfc2616 section 4.2
    auto& cur (result.first->data.second);
    cur.reserve (cur.size() + 1 + value.size());
    cur.append (1, ',');
    cur.append (value);
}

//------------------------------------------------------------------------------

template <class streambuf>
void
write (streambuf& stream, std::string const& s)
{
    stream.commit (boost::asio::buffer_copy (
        stream.prepare (s.size()), boost::asio::buffer(s)));
}

template <class streambuf>
void
write (streambuf& stream, char const* s)
{
    auto const len (::strlen(s));
    stream.commit (boost::asio::buffer_copy (
        stream.prepare (len), boost::asio::buffer (s, len)));
}

template <class streambuf>
void
write (streambuf& stream, headers const& h)
{
    for (auto const& _ : h)
    {
        write (stream, _.first);
        write (stream, ": ");
        write (stream, _.second);
        write (stream, "\r\n");
    }
}

template <class>
std::string
to_string (headers const& h)
{
    std::string s;
    std::size_t n (0);
    for (auto const& e : h)
        n += e.first.size() + 2 + e.second.size() + 2;
    s.reserve (n);
    for (auto const& e : h)
    {
        s.append (e.first);
        s.append (": ");
        s.append (e.second);
        s.append ("\r\n");
    }
    return s;
}

inline
std::ostream&
operator<< (std::ostream& s, headers const& h)
{
    s << to_string(h);
    return s;
}

template <class>
std::map <std::string, std::string>
build_map (headers const& h)
{
    std::map <std::string, std::string> c;
    for (auto const& e : h)
    {
        auto key (e.first);
        // todo replace with safe c++14 version
        std::transform (key.begin(), key.end(), key.begin(), ::tolower);
        c [key] = e.second;
    }
    return c;
}

} // http
} // beast

#endif