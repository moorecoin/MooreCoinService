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

#include <beast/http/basic_url.h>

#include <beast/http/impl/joyent_parser.h>

namespace beast {
namespace http {
namespace detail {

void
basic_url_base::parse_impl (string_ref s, boost::system::error_code& ec)
{
    joyent::http_parser_url p;

    value_type const* const data (s.data());
    
    int const error (joyent::http_parser_parse_url (
        data, s.size(), false, &p));

    if (error)
    {
        ec = boost::system::error_code (
            boost::system::errc::invalid_argument,
            boost::system::generic_category());
        return;
    }

    if ((p.field_set & (1<<joyent::uf_schema)) != 0)
    {
        m_scheme = string_ref (
            data + p.field_data [joyent::uf_schema].off,
                p.field_data [joyent::uf_schema].len);
    }
    else
    {
        m_scheme = string_ref {};
    }

    if ((p.field_set & (1<<joyent::uf_host)) != 0)
    {
        m_host = string_ref (
            data + p.field_data [joyent::uf_host].off,
                p.field_data [joyent::uf_host].len);
    }
    else
    {
        m_host = string_ref {};
    }

    if ((p.field_set & (1<<joyent::uf_port)) != 0)
    {
        m_port = p.port;
        m_port_string = string_ref (
            data + p.field_data [joyent::uf_port].off,
                p.field_data [joyent::uf_port].len);
    }
    else
    {
        m_port = 0;
        m_port_string = string_ref {};
    }

    if ((p.field_set & (1<<joyent::uf_path)) != 0)
    {
        m_path = string_ref (
            data + p.field_data [joyent::uf_path].off,
                p.field_data [joyent::uf_path].len);
    }
    else
    {
        m_path = string_ref {};
    }

    if ((p.field_set & (1<<joyent::uf_query)) != 0)
    {
        m_query = string_ref (
            data + p.field_data [joyent::uf_query].off,
                p.field_data [joyent::uf_query].len);
    }
    else
    {
        m_query = string_ref {};
    }

    if ((p.field_set & (1<<joyent::uf_fragment)) != 0)
    {
        m_fragment = string_ref (
            data + p.field_data [joyent::uf_fragment].off,
                p.field_data [joyent::uf_fragment].len);
    }
    else
    {
        m_fragment = string_ref {};
    }

    if ((p.field_set & (1<<joyent::uf_userinfo)) != 0)
    {
        m_userinfo = string_ref (
            data + p.field_data [joyent::uf_userinfo].off,
                p.field_data [joyent::uf_userinfo].len);
    }
    else
    {
        m_userinfo = string_ref {};
    }
}

} // detail
} // http
} // beast
