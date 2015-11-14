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

#ifndef beast_http_parser_h_included
#define beast_http_parser_h_included

#include <beast/http/message.h>
#include <beast/http/body.h>
#include <functional>
#include <string>
#include <utility>

namespace beast {
namespace http {

/** parser for http messages.
    the result is stored in a message object.
*/
class parser : public beast::http::basic_parser
{
private:
    std::reference_wrapper <message> message_;
    std::function<void(void const*, std::size_t)> write_body_;

public:
    /** construct a parser for http request or response.
        the headers plus request or status line are stored in message.
        the content-body, if any, is passed as a series of calls to
        the write_body function. transfer encodings are applied before
        any data is passed to the write_body function.
    */
    parser (std::function<void(void const*, std::size_t)> write_body,
            message& m, bool request)
        : beast::http::basic_parser (request)
        , message_(m)
        , write_body_(std::move(write_body)) 
    {
        message_.get().request(request);
    }

    parser (message& m, body& b, bool request)
        : beast::http::basic_parser (request)
        , message_(m)
    {
        write_body_ = [&b](void const* data, std::size_t size)
            {
                b.write(data, size);
            };

        message_.get().request(request);
    }

#if defined(_msc_ver) && _msc_ver <= 1800
    parser& operator= (parser&& other);

#else
    parser& operator= (parser&& other) = default;

#endif

private:
    template <class = void>
    void
    do_start ();

    template <class = void>
    bool
    do_request (method_t method, std::string const& url,
        int major, int minor, bool keep_alive, bool upgrade);

    template <class = void>
    bool
    do_response (int status, std::string const& text,
        int major, int minor, bool keep_alive, bool upgrade);

    template <class = void>
    void
    do_field (std::string const& field, std::string const& value);

    template <class = void>
    void
    do_body (void const* data, std::size_t bytes);

    template <class = void>
    void
    do_complete();

    void
    on_start () override
    {
        do_start();
    }

    bool
    on_request (method_t method, std::string const& url,
        int major, int minor, bool keep_alive, bool upgrade) override
    {
        return do_request (method, url, major, minor, keep_alive, upgrade);
    }

    bool
    on_response (int status, std::string const& text,
        int major, int minor, bool keep_alive, bool upgrade) override
    {
        return do_response (status, text, major, minor, keep_alive, upgrade);
    }

    void
    on_field (std::string const& field, std::string const& value) override
    {
        do_field (field, value);
    }

    void
    on_body (void const* data, std::size_t bytes) override
    {
        do_body (data, bytes);
    }

    void
    on_complete() override
    {
        do_complete();
    }
};

//------------------------------------------------------------------------------

#if defined(_msc_ver) && _msc_ver <= 1800
inline
parser&
parser::operator= (parser&& other)
{
    basic_parser::operator= (std::move(other));
    message_ = std::move (other.message_);
    return *this;
}
#endif

template <class>
void
parser::do_start()
{
}

template <class>
bool
parser::do_request (method_t method, std::string const& url,
    int major, int minor, bool keep_alive, bool upgrade)
{
    message_.get().method (method);
    message_.get().url (url);
    message_.get().version (major, minor);
    message_.get().keep_alive (keep_alive);
    message_.get().upgrade (upgrade);
    return true;
}

template <class>
bool
parser::do_response (int status, std::string const& text,
    int major, int minor, bool keep_alive, bool upgrade)
{
    message_.get().status (status);
    message_.get().reason (text);
    message_.get().version (major, minor);
    message_.get().keep_alive (keep_alive);
    message_.get().upgrade (upgrade);
    return true;
}

template <class>
void
parser::do_field (std::string const& field, std::string const& value)
{
    message_.get().headers.append (field, value);
}

template <class>
void
parser::do_body (void const* data, std::size_t bytes)
{
    write_body_(data, bytes);
}

template <class>
void
parser::do_complete()
{
}

} // http
} // beast

#endif