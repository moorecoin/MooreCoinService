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

#ifndef http_parser_request_hpp
#define http_parser_request_hpp

#include <websocketpp/common/memory.hpp>
#include <websocketpp/http/parser.hpp>

namespace websocketpp {
namespace http {
namespace parser {

/// stores, parses, and manipulates http requests
/**
 * http::request provides the following functionality for working with http
 * requests.
 *
 * - initialize request via manually setting each element
 * - initialize request via reading raw bytes and parsing
 * - once initialized, access individual parsed elements
 * - once initialized, read entire request as raw bytes
 */
class request : public parser {
public:
    typedef request type;
    typedef lib::shared_ptr<type> ptr;

    request()
      : m_buf(lib::make_shared<std::string>())
      , m_ready(false) {}

    /// deprecated parse a complete header (\r\n\r\n must be in the istream)
    bool parse_complete(std::istream& s);

    /// process bytes in the input buffer
    /**
     * process up to len bytes from input buffer buf. returns the number of
     * bytes processed. bytes left unprocessed means bytes left over after the
     * final header delimiters.
     *
     * consume is a streaming processor. it may be called multiple times on one
     * request and the full headers need not be available before processing can
     * begin. if the end of the request was reached during this call to consume
     * the ready flag will be set. further calls to consume once ready will be
     * ignored.
     *
     * consume will throw an http::exception in the case of an error. typical
     * error reasons include malformed requests, incomplete requests, and max
     * header size being reached.
     *
     * @param buf pointer to byte buffer
     * @param len size of byte buffer
     * @return number of bytes processed.
     */
    size_t consume(const char *buf, size_t len);

    /// returns whether or not the request is ready for reading.
    bool ready() const {
        return m_ready;
    }

    /// returns the full raw request
    std::string raw();

    /// set the http method. must be a valid http token
    void set_method(const std::string& method);

    /// return the request method
    const std::string& get_method() const {
        return m_method;
    }

    /// set the http uri. must be a valid http uri
    void set_uri(const std::string& uri);

    /// return the requested uri
    const std::string& get_uri() const {
        return m_uri;
    }

private:
    /// helper function for message::consume. process request line
    void process(std::string::iterator begin, std::string::iterator end);

    lib::shared_ptr<std::string>    m_buf;
    std::string                     m_method;
    std::string                     m_uri;
    bool                            m_ready;
};

} // namespace parser
} // namespace http
} // namespace websocketpp

#include <websocketpp/http/impl/request.hpp>

#endif // http_parser_request_hpp
