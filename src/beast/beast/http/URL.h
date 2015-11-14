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

#ifndef beast_http_url_h_included
#define beast_http_url_h_included

#include <ios>
#include <utility>

namespace beast {

/** a url.
    the accompanying robust parser is hardened against all forms of attack.
*/
class url
{
public:
    /** construct a url from it's components. */
    url (
        std::string schema_,
        std::string host_,
        std::uint16_t port_,
        std::string port_string_,
        std::string path_,
        std::string query_ = "",
        std::string fragment_ = "",
        std::string userinfo_ = "");

    /** construct an empty url. */
    explicit url () = default;

    /** copy construct a url. */
    url (url const& other) = default;

    /** copy assign a url. */
    url& operator= (url const& other) = default;

    /** move construct a url. */
#ifdef _msc_ver
    url (url&& other)
        : m_scheme(std::move(other.m_scheme))
        , m_host(std::move(other.m_host))
        , m_port(other.m_port)
        , m_port_string(std::move(other.m_port_string))
        , m_path(std::move(other.m_path))
        , m_query(std::move(other.m_query))
        , m_fragment(std::move(other.m_fragment))
        , m_userinfo(std::move(other.m_userinfo))
    {
    }

#else
    url (url&& other) = default;
#endif

    /** returns `true` if this is an empty url. */
    bool
    empty () const;

    /** returns the scheme of the url.
        if no scheme was specified, the string will be empty.
    */
    std::string const&
    scheme () const;

    /** returns the host of the url.
        if no host was specified, the string will be empty.
    */
    std::string const&
    host () const;

    /** returns the port number as an integer.
        if no port was specified, the value will be zero.
    */
    std::uint16_t
    port () const;

    /** returns the port number as a string.
        if no port was specified, the string will be empty.
    */
    std::string const&
    port_string () const;

    /** returns the path of the url.
        if no path was specified, the string will be empty.
    */
    std::string const&
    path () const;

    /** returns the query parameters portion of the url.
        if no query parameters were present, the string will be empty.
    */
    std::string const&
    query () const;

    /** returns the url fragment, if any. */
    std::string const&
    fragment () const;

    /** returns the user information, if any. */
    std::string const&
    userinfo () const;

private:
    std::string m_scheme;
    std::string m_host;
    std::uint16_t m_port = 0;
    std::string m_port_string;
    std::string m_path;
    std::string m_query;
    std::string m_fragment;
    std::string m_userinfo;
};

/** attempt to parse a string into a url */
std::pair<bool, url>
parse_url(std::string const&);

/** retrieve the full url as a single string. */
std::string
to_string(url const& url);

/** output stream conversion. */
std::ostream&
operator<< (std::ostream& os, url const& url);

/** url comparisons. */
/** @{ */
inline bool
operator== (url const& lhs, url const& rhs)
{
    return to_string (lhs) == to_string (rhs);
}

inline bool
operator!= (url const& lhs, url const& rhs)
{
    return to_string (lhs) != to_string (rhs);
}

inline bool
operator<  (url const& lhs, url const& rhs)
{
    return to_string (lhs) < to_string (rhs);
}

inline bool operator>  (url const& lhs, url const& rhs)
{
    return to_string (rhs) < to_string (lhs);
}

inline bool
operator<= (url const& lhs, url const& rhs)
{
    return ! (to_string (rhs) < to_string (lhs));
}

inline bool
operator>= (url const& lhs, url const& rhs)
{
    return ! (to_string (lhs) < to_string (rhs));
}
/** @} */

/** boost::hash support */
template <class hasher>
inline
void
hash_append (hasher& h, url const& url)
{
    using beast::hash_append;
    hash_append (h, to_string (url));
}

}

//------------------------------------------------------------------------------

namespace std {

template <>
struct hash <beast::url>
{
    std::size_t operator() (beast::url const& url) const
    {
        return std::hash<std::string>{} (to_string (url));
    }
};

}

//------------------------------------------------------------------------------

#endif
