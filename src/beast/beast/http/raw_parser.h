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

#ifndef beast_http_raw_parser_h_included
#define beast_http_raw_parser_h_included

#include <beast/utility/empty_base_optimization.h>

#include <boost/system/error_code.hpp> // change to <system_error> soon

#include <array>
#include <cstdint>
#include <memory>

namespace beast {

namespace joyent {
struct http_parser;
};

namespace http {

/** raw http message parser.
    this is implemented using a zero-allocation state machine. the caller
    is responsible for all buffer management.
*/
class raw_parser
{
private:
    typedef boost::system::error_code error_code;

public:
    enum message_type
    {
        request,
        response
    };

    struct callback
    {
        /** called when the first byte of an http request is received. */
        virtual
        error_code
        on_request ();

        /** called when the first byte of an http response is received. */
        virtual
        error_code
        on_response ();

        /** called repeatedly to provide parts of the url.
            this is only for requests.
        */
        virtual
        error_code
        on_url (
            void const* in, std::size_t bytes);

        /** called when the status is received.
            this is only for responses.
        */
        virtual
        error_code
        on_status (int status_code,
            void const* in, std::size_t bytes);

        /** called repeatedly to provide parts of a field. */
        virtual
        error_code
        on_header_field (
            void const* in, std::size_t bytes);

        /** called repeatedly to provide parts of a value. */
        virtual
        error_code
        on_header_value (
            void const* in, std::size_t bytes);

        /** called when there are no more bytes of headers remaining. */
        virtual
        error_code
        on_headers_done (
            bool keep_alive);

        /** called repeatedly to provide parts of the body. */
        virtual
        error_code
        on_body (bool is_final,
            void const* in, std::size_t bytes);

        /** called when there are no more bytes of body remaining. */
        virtual
        error_code on_message_complete (
            bool keep_alive);
    };

    explicit raw_parser (callback& cb);

    ~raw_parser();

    /** prepare to parse a new message.
        the previous state information, if any, is discarded.
    */
    void
    reset (message_type type);

    /** processs message data.
        the return value includes the error code if any,
        and the number of bytes consumed in the input sequence.
    */
    std::pair <error_code, std::size_t>
    process_data (void const* in, std::size_t bytes);

    /** notify the parser the end of the data is reached.
        normally this will be called in response to the remote
        end closing down its half of the connection.
    */
    error_code
    process_eof ();

private:
    int do_message_start ();
    int do_url (char const* in, std::size_t bytes);
    int do_status (char const* in, std::size_t bytes);
    int do_header_field (char const* in, std::size_t bytes);
    int do_header_value (char const* in, std::size_t bytes);
    int do_headers_done ();
    int do_body (char const* in, std::size_t bytes);
    int do_message_complete ();

    static int on_message_start (joyent::http_parser*);
    static int on_url (joyent::http_parser*, char const*, std::size_t);
    static int on_status (joyent::http_parser*, char const*, std::size_t);
    static int on_header_field (joyent::http_parser*, char const*, std::size_t);
    static int on_header_value (joyent::http_parser*, char const*, std::size_t);
    static int on_headers_done (joyent::http_parser*);
    static int on_body (joyent::http_parser*, char const*, std::size_t);
    static int on_message_complete (joyent::http_parser*);

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

    std::reference_wrapper <callback> m_cb;
    error_code m_ec;
    char m_state [sizeof(state_t)];
    char m_hooks [sizeof(hooks_t)];
};

}
}

#endif
