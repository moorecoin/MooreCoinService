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

#ifndef beast_httpparserimpl_h_included
#define beast_httpparserimpl_h_included

#include <beast/http/impl/joyent_parser.h>
#include <boost/asio/buffer.hpp>

namespace beast {

class httpparserimpl
{
public:
    enum
    {
        stringreservation = 256
    };

    explicit httpparserimpl (enum joyent::http_parser_type type)
        : m_finished (false)
        , m_was_value (false)
        , m_headerscomplete (false)
    {
        m_settings.on_message_begin     = &httpparserimpl::on_message_begin;
        m_settings.on_url               = &httpparserimpl::on_url;
        m_settings.on_status            = &httpparserimpl::on_status;
        m_settings.on_header_field      = &httpparserimpl::on_header_field;
        m_settings.on_header_value      = &httpparserimpl::on_header_value;
        m_settings.on_headers_complete  = &httpparserimpl::on_headers_complete;
        m_settings.on_body              = &httpparserimpl::on_body;
        m_settings.on_message_complete  = &httpparserimpl::on_message_complete;

        m_field.reserve (stringreservation);
        m_value.reserve (stringreservation);

        joyent::http_parser_init (&m_parser, type);
        m_parser.data = this;
    }

    ~httpparserimpl ()
    {
    }

    unsigned char error () const
    {
        return m_parser.http_errno;
    }

    string message () const
    {
        return string (joyent::http_errno_name (static_cast <
            enum joyent::http_errno> (m_parser.http_errno)));
    }

    std::size_t process (void const* buf, std::size_t bytes)
    {
        return joyent::http_parser_execute (&m_parser,
            &m_settings, static_cast <char const*> (buf), bytes);
    }

    void process_eof ()
    {
        joyent::http_parser_execute (&m_parser, &m_settings, nullptr, 0);
    }

    bool finished () const
    {
        return m_finished;
    }

    httpversion version () const
    {
        return httpversion (
            m_parser.http_major, m_parser.http_minor);
    }

    // only for httpresponse!
    unsigned short status_code () const
    {
        return m_parser.status_code;
    }

    // only for httprequest!
    unsigned char method () const
    {
        return m_parser.method;
    }

    unsigned char http_errno () const
    {
        return m_parser.http_errno;
    }

    string http_errno_message () const
    {
        return string (joyent::http_errno_name (
            static_cast <enum joyent::http_errno> (
                m_parser.http_errno)));
    }

    bool upgrade () const
    {
        return m_parser.upgrade != 0;
    }

    stringpairarray& fields ()
    {
        return m_fields;
    }

    bool headers_complete () const
    {
        return m_headerscomplete;
    }

    dynamicbuffer& body ()
    {
        return m_body;
    }

private:
    void addfieldvalue ()
    {
        if (m_field.size () > 0 && m_value.size () > 0)
            m_fields.set (m_field, m_value);
        m_field.resize (0);
        m_value.resize (0);
    }

    int onmessagebegin ()
    {
        int ec (0);
        return ec;
    }

    int onurl (char const*, std::size_t)
    {
        int ec (0);
        // this is for http request
        return ec;
    }

    int onstatus ()
    {
        int ec (0);
        return ec;
    }

    int onheaderfield (char const* at, std::size_t length)
    {
        int ec (0);
        if (m_was_value)
        {
            addfieldvalue ();
            m_was_value = false;
        }
        m_field.append (at, length);
        return ec;
    }

    int onheadervalue (char const* at, std::size_t length)
    {
        int ec (0);
        m_value.append (at, length);
        m_was_value = true;
        return ec;
    }

    int onheaderscomplete ()
    {
        m_headerscomplete = true;
        int ec (0);
        addfieldvalue ();
        return ec;
    }

    int onbody (char const* at, std::size_t length)
    {
        m_body.commit (boost::asio::buffer_copy (
            m_body.prepare <boost::asio::mutable_buffer> (length),
                boost::asio::buffer (at, length)));
        return 0;
    }

    int onmessagecomplete ()
    {
        int ec (0);
        m_finished = true;
        return ec;
    }

private:
    static int on_message_begin (joyent::http_parser* parser)
    {
        return static_cast <httpparserimpl*> (parser->data)->
            onmessagebegin ();
    }

    static int on_url (joyent::http_parser* parser, const char *at, size_t length)
    {
        return static_cast <httpparserimpl*> (parser->data)->
            onurl (at, length);
    }

    static int on_status (joyent::http_parser* parser,
        char const* /*at*/, size_t /*length*/)
    {
        return static_cast <httpparserimpl*> (parser->data)->
            onstatus ();
    }

    static int on_header_field (joyent::http_parser* parser,
        const char *at, size_t length)
    {
        return static_cast <httpparserimpl*> (parser->data)->
            onheaderfield (at, length);
    }

    static int on_header_value (joyent::http_parser* parser,
        const char *at, size_t length)
    {
        return static_cast <httpparserimpl*> (parser->data)->
            onheadervalue (at, length);
    }

    static int on_headers_complete (joyent::http_parser* parser)
    {
        return static_cast <httpparserimpl*> (parser->data)->
            onheaderscomplete ();
    }

    static int on_body (joyent::http_parser* parser,
        const char *at, size_t length)
    {
        return static_cast <httpparserimpl*> (parser->data)->
            onbody (at, length);
    }

    static int on_message_complete (joyent::http_parser* parser)
    {
        return static_cast <httpparserimpl*> (parser->data)->
            onmessagecomplete ();
    }

private:
    bool m_finished;
    joyent::http_parser_settings m_settings;
    joyent::http_parser m_parser;
    stringpairarray m_fields;
    bool m_was_value;
    std::string m_field;
    std::string m_value;
    bool m_headerscomplete;
    dynamicbuffer m_body;
};

}

#endif
