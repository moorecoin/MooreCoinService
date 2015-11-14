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

#include <beast/module/asio/httpparser.h>
#include <beast/module/asio/httpparserimpl.h>

namespace beast {

httpparser::httpparser (type type)
    : m_type (type)
    , m_impl (new httpparserimpl (
        (type == typeresponse) ? joyent::http_response : joyent::http_request))
{
}

httpparser::~httpparser ()
{
}

unsigned char httpparser::error () const
{
    return m_impl->http_errno ();
}

string httpparser::message () const
{
    return m_impl->http_errno_message ();
}

std::size_t httpparser::process (void const* buf, std::size_t bytes)
{
    std::size_t const bytes_used (m_impl->process (buf, bytes));

    if (m_impl->finished ())
    {
        if (m_type == typerequest)
        {
            m_request = new httprequest (
                m_impl->version (),
                m_impl->fields (),
                m_impl->body (),
                m_impl->method ());
        }
        else if (m_type == typeresponse)
        {
            m_response = new httpresponse (
                m_impl->version (),
                m_impl->fields (),
                m_impl->body (),
                m_impl->status_code ());
        }
        else
        {
            bassertfalse;
        }
    }

    return bytes_used;
}

void httpparser::process_eof ()
{
    m_impl->process_eof ();
}

bool httpparser::finished () const
{
    return m_impl->finished();
}

stringpairarray const& httpparser::fields () const
{
    return m_impl->fields();
}

bool httpparser::headerscomplete () const
{
    return m_impl->headers_complete();
}

sharedptr <httprequest> const& httpparser::request ()
{
    bassert (m_type == typerequest);

    return m_request;
}

sharedptr <httpresponse> const& httpparser::response ()
{
    bassert (m_type == typeresponse);

    return m_response;
}

}
