/*
 * copyright (c) 2014, peter thorson. all rights reserved.
 *
 * redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * neither the name of the websocket++ project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * this software is provided by the copyright holders and contributors "as is"
 * and any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose
 * are disclaimed. in no event shall peter thorson be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential damages
 * (including, but not limited to, procurement of substitute goods or services;
 * loss of use, data, or profits; or business interruption) however caused and
 * on any theory of liability, whether in contract, strict liability, or tort
 * (including negligence or otherwise) arising in any way out of the use of this
 * software, even if advised of the possibility of such damage.
 *
 */

#ifndef http_parser_response_hpp
#define http_parser_response_hpp

#include <websocketpp/http/parser.hpp>

namespace websocketpp {
namespace http {
namespace parser {

/// stores, parses, and manipulates http responses
/**
 * http::response provides the following functionality for working with http
 * responses.
 *
 * - initialize response via manually setting each element
 * - initialize response via reading raw bytes and parsing
 * - once initialized, access individual parsed elements
 * - once initialized, read entire response as raw bytes
 *
 * http::response checks for header completeness separately from the full
 * response. once the header is complete, the content-length header is read to
 * determine when to stop reading body bytes. if no content-length is present
 * ready() will never return true. it is the responsibility of the caller to
 * consume to determine when the response is complete (ie when the connection
 * terminates, or some other metric).
 */
class response : public parser {
public:
    typedef response type;
    typedef lib::shared_ptr<type> ptr;

    response()
      : m_read(0)
      , m_buf(lib::make_shared<std::string>())
      , m_status_code(status_code::uninitialized)
      , m_state(response_line) {}

    /// process bytes in the input buffer
    /**
     * process up to len bytes from input buffer buf. returns the number of
     * bytes processed. bytes left unprocessed means bytes left over after the
     * final header delimiters.
     *
     * consume is a streaming processor. it may be called multiple times on one
     * response and the full headers need not be available before processing can
     * begin. if the end of the response was reached during this call to consume
     * the ready flag will be set. further calls to consume once ready will be
     * ignored.
     *
     * consume will throw an http::exception in the case of an error. typical
     * error reasons include malformed responses, incomplete responses, and max
     * header size being reached.
     *
     * @param buf pointer to byte buffer
     * @param len size of byte buffer
     * @return number of bytes processed.
     */
    size_t consume(const char *buf, size_t len);

    size_t consume(std::istream & s);

    /// returns true if the response is ready.
    /**
     * @note will never return true if the content length header is not present
     */
    bool ready() const {
        return m_state == done;
    }

    /// returns true if the response headers are fully parsed.
    bool headers_ready() const {
        return (m_state == body || m_state == done);
    }

    /// deprecated parse a complete response from a pre-delimited istream
    bool parse_complete(std::istream& s);

    /// returns the full raw response
    std::string raw() const;

    /// set response status code and message
    /**
     * sets the response status code to `code` and looks up the corresponding
     * message for standard codes. non-standard codes will be entered as unknown
     * use set_status(status_code::value,std::string) overload to set both
     * values explicitly.
     *
     * @param code code to set
     * @param msg message to set
     */
    void set_status(status_code::value code);

    /// set response status code and message
    /**
     * sets the response status code and message to independent custom values.
     * use set_status(status_code::value) to set the code and have the standard
     * message be automatically set.
     *
     * @param code code to set
     * @param msg message to set
     */
    void set_status(status_code::value code, const std::string& msg);

    /// return the response status code
    status_code::value get_status_code() const {
        return m_status_code;
    }

    /// return the response status message
    const std::string& get_status_msg() const {
        return m_status_msg;
    }
private:
    /// helper function for consume. process response line
    void process(std::string::iterator begin, std::string::iterator end);

    /// helper function for processing body bytes
    size_t process_body(const char *buf, size_t len);

    enum state {
        response_line = 0,
        headers = 1,
        body = 2,
        done = 3
    };

    std::string                     m_status_msg;
    size_t                          m_read;
    lib::shared_ptr<std::string>    m_buf;
    status_code::value              m_status_code;
    state                           m_state;

};

} // namespace parser
} // namespace http
} // namespace websocketpp

#include <websocketpp/http/impl/response.hpp>

#endif // http_parser_response_hpp
