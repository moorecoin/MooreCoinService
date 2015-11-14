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

#ifndef beast_http_basic_parser_h_included
#define beast_http_basic_parser_h_included

#include <beast/http/method.h>
#include <boost/asio/buffer.hpp>
#include <boost/system/error_code.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <beast/utility/noexcept.h>

namespace beast {

namespace joyent {
struct http_parser;
};

namespace http {

class basic_parser
{
private:
    // these structures must exactly match the
    // declarations in joyent http_parser.h include
    //
    struct state_t
    {
      unsigned int type  : 2;
      unsigned int flags : 6;
      unsigned int state : 8;
      unsigned int header_state : 8;
      unsigned int index : 8;
      std::uint32_t nread;
      std::uint64_t content_length;
      unsigned short http_major;
      unsigned short http_minor;
      unsigned int status_code : 16;
      unsigned int method : 8;
      unsigned int http_errno : 7;
      unsigned int upgrade : 1;
      void *data;
    };

    typedef int (*data_cb_t) (
        state_t*, const char *at, size_t length);
    typedef int (*cb_t) (state_t*);

    struct hooks_t
    {
      cb_t      on_message_begin;
      data_cb_t on_url;
      data_cb_t on_status;
      data_cb_t on_header_field;
      data_cb_t on_header_value;
      cb_t      on_headers_complete;
      data_cb_t on_body;
      cb_t      on_message_complete;
    };

    char state_ [sizeof(state_t)];
    char hooks_ [sizeof(hooks_t)];

    bool complete_ = false;
    std::string url_;
    std::string status_;
    std::string field_;
    std::string value_;

public:
    typedef boost::system::error_code error_code;

    virtual
    ~basic_parser() = default;

    /** construct the parser.
        if `request` is `true` this sets up the parser to
        process an http request.
    */
    explicit
    basic_parser (bool request) noexcept;

    basic_parser&
    operator= (basic_parser&& other);

    /** returns `true` if parsing is complete.
        this is only defined when no errors have been returned.
    */
    bool
    complete() const noexcept
    {
        return complete_;
    }

    /** write data to the parser.
        @param data a buffer containing the data to write
        @param bytes the size of the buffer pointed to by data.
        @return a pair with bool success, and the number of bytes consumed.
    */
    std::pair <error_code, std::size_t>
    write (void const* data, std::size_t bytes);

    /** write a set of buffer data to the parser.
        the return value includes the error code if any,
        and the number of bytes consumed in the input sequence.
        @param buffers the buffers to write. these must meet the
                       requirements of constbuffersequence.
        @return a pair with bool success, and the number of bytes consumed.
    */
    template <class constbuffersequence>
    std::pair <error_code, std::size_t>
    write (constbuffersequence const& buffers);

    /** called to indicate the end of file.
        http needs to know where the end of the stream is. for example,
        sometimes servers send responses without content-length and
        expect the client to consume input (for the body) until eof.
        callbacks and errors will still be processed as usual.
        @note this is typically called when a socket read returns eof.
        @return `true` if the message is complete.
    */
    error_code
    write_eof();

protected:
    /** called once when a new message begins. */
    virtual
    void
    on_start() = 0;

    /** called for each header field. */
    virtual
    void
    on_field (std::string const& field, std::string const& value) = 0;

    /** called for requests when all the headers have been received.
        this will precede any content body.
        when keep_alive is false:
            * server roles respond with a "connection: close" header.
            * client roles close the connection.
        when upgrade is true, no content-body is expected, and the
        return value is ignored.

        @param method the http method specified in the request line
        @param major the http major version number
        @param minor the http minor version number
        @param url the url specified in the request line
        @param keep_alive `false` if this is the last message.
        @param upgrade `true` if the upgrade header is specified
        @return `true` if upgrade is false and a content body is expected.
    */
    virtual
    bool
    on_request (method_t method, std::string const& url,
        int major, int minor, bool keep_alive, bool upgrade) = 0;

    /** called for responses when all the headers have been received.
        this will precede any content body.
        when keep_alive is `false`:
            * client roles close the connection.
            * server roles respond with a "connection: close" header.
        when upgrade is true, no content-body is expected, and the
        return value is ignored.

        @param status the numerical http status code in the response line
        @param text the status text in the response line
        @param major the http major version number
        @param minor the http minor version number
        @param keep_alive `false` if this is the last message.
        @param upgrade `true` if the upgrade header is specified
        @return `true` if upgrade is false and a content body is expected.
    */
    virtual
    bool
    on_response (int status, std::string const& text,
        int major, int minor, bool keep_alive, bool upgrade) = 0;

    /** called zero or more times for the content body.
        any transfer encoding is already decoded in the
        memory pointed to by data.

        @param data a memory block containing the next decoded
                    chunk of the content body.
        @param bytes the number of bytes pointed to by data.
    */
    virtual
    void
    on_body (void const* data, std::size_t bytes) = 0;

    /** called once when the message is complete. */
    virtual
    void
    on_complete() = 0;

private:
    void check_header();

    int do_message_start ();
    int do_url (char const* in, std::size_t bytes);
    int do_status (char const* in, std::size_t bytes);
    int do_header_field (char const* in, std::size_t bytes);
    int do_header_value (char const* in, std::size_t bytes);
    int do_headers_complete ();
    int do_body (char const* in, std::size_t bytes);
    int do_message_complete ();

    static int cb_message_start (joyent::http_parser*);
    static int cb_url (joyent::http_parser*, char const*, std::size_t);
    static int cb_status (joyent::http_parser*, char const*, std::size_t);
    static int cb_header_field (joyent::http_parser*, char const*, std::size_t);
    static int cb_header_value (joyent::http_parser*, char const*, std::size_t);
    static int cb_headers_complete (joyent::http_parser*);
    static int cb_body (joyent::http_parser*, char const*, std::size_t);
    static int cb_message_complete (joyent::http_parser*);
};

template <class constbuffersequence>
auto
basic_parser::write (constbuffersequence const& buffers) ->
    std::pair <error_code, std::size_t>
{
    std::pair <error_code, std::size_t> result ({}, 0);
    for (auto const& buffer : buffers)
    {
        std::size_t bytes_consumed;
        std::tie (result.first, bytes_consumed) =
            write (boost::asio::buffer_cast <void const*> (buffer),
                boost::asio::buffer_size (buffer));
        if (result.first)
            break;
        result.second += bytes_consumed;
        if (complete())
            break;
    }
    return result;
}

} // http
} // beast

#endif
