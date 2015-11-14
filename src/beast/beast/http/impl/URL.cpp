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

#include <beast/http/url.h>

namespace beast {

url::url (
    std::string scheme_,
    std::string host_,
    std::uint16_t port_,
    std::string port_string_,
    std::string path_,
    std::string query_,
    std::string fragment_,
    std::string userinfo_)
        : m_scheme (scheme_)
        , m_host (host_)
        , m_port (port_)
        , m_port_string (port_string_)
        , m_path (path_)
        , m_query (query_)
        , m_fragment (fragment_)
        , m_userinfo (userinfo_)
{
}

//------------------------------------------------------------------------------

bool
url::empty () const
{
    return m_scheme.empty ();
}

std::string
const& url::scheme () const
{
    return m_scheme;
}

std::string
const& url::host () const
{
    return m_host;
}

std::string
const& url::port_string () const
{
    return m_port_string;
}

std::uint16_t
url::port () const
{
    return m_port;
}

std::string
const& url::path () const
{
    return m_path;
}

std::string
const& url::query () const
{
    return m_query;
}

std::string
const& url::fragment () const
{
    return m_fragment;
}

std::string
const& url::userinfo () const
{
    return m_userinfo;
}

//------------------------------------------------------------------------------
std::pair<bool, url>
parse_url(std::string const& url)
{
    std::size_t const buflen (url.size ());
    char const* const buf (url.c_str ());

    joyent::http_parser_url parser;

    if (joyent::http_parser_parse_url (buf, buflen, false, &parser) != 0)
        return std::make_pair (false, url{});

    std::string scheme;
    std::string host;
    std::uint16_t port (0);
    std::string port_string;
    std::string path;
    std::string query;
    std::string fragment;
    std::string userinfo;

    if ((parser.field_set & (1<<joyent::uf_schema)) != 0)
    {
        scheme = std::string (
            buf + parser.field_data [joyent::uf_schema].off,
                parser.field_data [joyent::uf_schema].len);
    }

    if ((parser.field_set & (1<<joyent::uf_host)) != 0)
    {
        host = std::string (
            buf + parser.field_data [joyent::uf_host].off,
                parser.field_data [joyent::uf_host].len);
    }

    if ((parser.field_set & (1<<joyent::uf_port)) != 0)
    {
        port = parser.port;
        port_string = std::string (
            buf + parser.field_data [joyent::uf_port].off,
                parser.field_data [joyent::uf_port].len);
    }

    if ((parser.field_set & (1<<joyent::uf_path)) != 0)
    {
        path = std::string (
            buf + parser.field_data [joyent::uf_path].off,
                parser.field_data [joyent::uf_path].len);
    }

    if ((parser.field_set & (1<<joyent::uf_query)) != 0)
    {
        query = std::string (
            buf + parser.field_data [joyent::uf_query].off,
                parser.field_data [joyent::uf_query].len);
    }

    if ((parser.field_set & (1<<joyent::uf_fragment)) != 0)
    {
        fragment = std::string (
            buf + parser.field_data [joyent::uf_fragment].off,
                parser.field_data [joyent::uf_fragment].len);
    }

    if ((parser.field_set & (1<<joyent::uf_userinfo)) != 0)
    {
        userinfo = std::string (
            buf + parser.field_data [joyent::uf_userinfo].off,
                parser.field_data [joyent::uf_userinfo].len);
    }

    return std::make_pair (true,
        url {scheme, host, port, port_string, path, query, fragment, userinfo});
}

std::string
to_string (url const& url)
{
    std::string s;

    if (!url.empty ())
    {
        // pre-allocate enough for components and inter-component separators
        s.reserve (
            url.scheme ().length () + url.userinfo ().length () +
            url.host ().length () + url.port_string ().length () +
            url.query ().length () + url.fragment ().length () + 16);

        s.append (url.scheme ());
        s.append ("://");
        
        if (!url.userinfo ().empty ())
        {
            s.append (url.userinfo ());
            s.append ("@");
        }

        s.append (url.host ());

        if (url.port ())
        {
            s.append (":");
            s.append (url.port_string ());
        }

        s.append (url.path ());

        if (!url.query ().empty ())
        {
            s.append ("?");
            s.append (url.query ());
        }

        if (!url.fragment ().empty ())
        {
            s.append ("#");
            s.append (url.fragment ());
        }
    }

    return s;
}

std::ostream&
operator<< (std::ostream &os, url const& url)
{
    os << to_string (url);
    return os;
}

}
