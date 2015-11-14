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

#ifndef http_parser_request_impl_hpp
#define http_parser_request_impl_hpp

#include <algorithm>
#include <sstream>

#include <websocketpp/http/parser.hpp>

namespace websocketpp {
namespace http {
namespace parser {

inline bool request::parse_complete(std::istream& s) {
    std::string req;

    // get status line
    std::getline(s, req);

    if (req[req.size()-1] == '\r') {
        req.erase(req.end()-1);

        std::stringstream ss(req);
        std::string val;

        ss >> val;
        set_method(val);

        ss >> val;
        set_uri(val);

        ss >> val;
        set_version(val);
    } else {
        return false;
    }

    return parse_headers(s);
}

inline size_t request::consume(const char *buf, size_t len) {
    if (m_ready) {return 0;}

    if (m_buf->size() + len > max_header_size) {
        // exceeded max header size
        throw exception("maximum header size exceeded.",
                        status_code::request_header_fields_too_large);
    }

    // copy new header bytes into buffer
    m_buf->append(buf,len);

    // search for delimiter in buf. if found read until then. if not read all
    std::string::iterator begin = m_buf->begin();
    std::string::iterator end;

    for (;;) {
        // search for line delimiter
        end = std::search(
            begin,
            m_buf->end(),
            header_delimiter,
            header_delimiter+sizeof(header_delimiter)-1
        );

        //std::cout << "mark5: " << end-begin << std::endl;
        //std::cout << "mark6: " << sizeof(header_delimiter) << std::endl;

        if (end == m_buf->end()) {
            // we are out of bytes. discard the processed bytes and copy the
            // remaining unprecessed bytes to the beginning of the buffer
            std::copy(begin,end,m_buf->begin());
            m_buf->resize(static_cast<std::string::size_type>(end-begin));

            return len;
        }

        //the range [begin,end) now represents a line to be processed.
        if (end-begin == 0) {
            // we got a blank line
            if (m_method.empty() || get_header("host") == "") {
                throw exception("incomplete request",status_code::bad_request);
            }
            m_ready = true;

            size_t bytes_processed = (
                len - static_cast<std::string::size_type>(m_buf->end()-end)
                    + sizeof(header_delimiter) - 1
            );

            // frees memory used temporarily during request parsing
            m_buf.reset();

            // return number of bytes processed (starting bytes - bytes left)
            return bytes_processed;
        } else {
            if (m_method.empty()) {
                this->process(begin,end);
            } else {
                this->process_header(begin,end);
            }
        }

        begin = end+(sizeof(header_delimiter)-1);
    }
}

inline std::string request::raw() {
    // todo: validation. make sure all required fields have been set?
    std::stringstream ret;

    ret << m_method << " " << m_uri << " " << get_version() << "\r\n";
    ret << raw_headers() << "\r\n" << m_body;

    return ret.str();
}

inline void request::set_method(const std::string& method) {
    if (std::find_if(method.begin(),method.end(),is_not_token_char) != method.end()) {
        throw exception("invalid method token.",status_code::bad_request);
    }

    m_method = method;
}

/// set http body
/**
 * sets the body of the http object and fills in the appropriate content length
 * header
 *
 * @param value the value to set the body to.
 */
inline void request::set_uri(const std::string& uri) {
    // todo: validation?
    m_uri = uri;
}

inline void request::process(std::string::iterator begin, std::string::iterator
    end)
{
    std::string::iterator cursor_start = begin;
    std::string::iterator cursor_end = std::find(begin,end,' ');

    if (cursor_end == end) {
        throw exception("invalid request line1",status_code::bad_request);
    }

    set_method(std::string(cursor_start,cursor_end));

    cursor_start = cursor_end+1;
    cursor_end = std::find(cursor_start,end,' ');

    if (cursor_end == end) {
        throw exception("invalid request line2",status_code::bad_request);
    }

    set_uri(std::string(cursor_start,cursor_end));
    set_version(std::string(cursor_end+1,end));
}

} // namespace parser
} // namespace http
} // namespace websocketpp

#endif // http_parser_request_impl_hpp
