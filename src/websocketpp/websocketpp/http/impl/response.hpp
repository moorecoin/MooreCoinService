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

#ifndef http_parser_response_impl_hpp
#define http_parser_response_impl_hpp

#include <algorithm>
#include <sstream>

#include <websocketpp/http/parser.hpp>

namespace websocketpp {
namespace http {
namespace parser {

inline size_t response::consume(const char *buf, size_t len) {
    if (m_state == done) {return 0;}

    if (m_state == body) {
        return this->process_body(buf,len);
    }

    if (m_read + len > max_header_size) {
        // exceeded max header size
        throw exception("maximum header size exceeded.",
                        status_code::request_header_fields_too_large);
    }

    // copy new header bytes into buffer
    m_buf->append(buf,len);

    // search for delimiter in buf. if found read until then. if not read all
    std::string::iterator begin = m_buf->begin();
    std::string::iterator end = begin;


    for (;;) {
        // search for delimiter
        end = std::search(
            begin,
            m_buf->end(),
            header_delimiter,
            header_delimiter + sizeof(header_delimiter) - 1
        );

        if (end == m_buf->end()) {
            // we are out of bytes. discard the processed bytes and copy the
            // remaining unprecessed bytes to the beginning of the buffer
            std::copy(begin,end,m_buf->begin());
            m_buf->resize(static_cast<std::string::size_type>(end-begin));

            m_read +=len;

            return len;
        }

        //the range [begin,end) now represents a line to be processed.

        if (end-begin == 0) {
            // we got a blank line
            if (m_state == response_line) {
                throw exception("incomplete request",status_code::bad_request);
            }

            // todo: grab content-length
            std::string length = get_header("content-length");

            if (length == "") {
                // no content length found, read indefinitely
                m_read = 0;
            } else {
                std::istringstream ss(length);

                if ((ss >> m_read).fail()) {
                    throw exception("unable to parse content-length header",
                                    status_code::bad_request);
                }
            }

            m_state = body;

            // calc header bytes processed (starting bytes - bytes left)
            size_t read = (
                len - static_cast<std::string::size_type>(m_buf->end() - end)
                + sizeof(header_delimiter) - 1
            );

            // if there were bytes left process them as body bytes
            if (read < len) {
                read += this->process_body(buf+read,(len-read));
            }

            // frees memory used temporarily during header parsing
            m_buf.reset();

            return read;
        } else {
            if (m_state == response_line) {
                this->process(begin,end);
                m_state = headers;
            } else {
                this->process_header(begin,end);
            }
        }

        begin = end+(sizeof(header_delimiter) - 1);
    }
}

inline size_t response::consume(std::istream & s) {
    char buf[istream_buffer];
    size_t bytes_read;
    size_t bytes_processed;
    size_t total = 0;

    while (s.good()) {
        s.getline(buf,istream_buffer);
        bytes_read = static_cast<size_t>(s.gcount());

        if (s.fail() || s.eof()) {
            bytes_processed = this->consume(buf,bytes_read);
            total += bytes_processed;

            if (bytes_processed != bytes_read) {
                // problem
                break;
            }
        } else if (s.bad()) {
            // problem
            break;
        } else {
            // the delimiting newline was found. replace the trailing null with
            // the newline that was discarded, since our raw consume function
            // expects the newline to be be there.
            buf[bytes_read-1] = '\n';
            bytes_processed = this->consume(buf,bytes_read);
            total += bytes_processed;

            if (bytes_processed != bytes_read) {
                // problem
                break;
            }
        }
    }

    return total;
}

inline bool response::parse_complete(std::istream& s) {
    // parse a complete header (ie \r\n\r\n must be in the input stream)
    std::string line;

    // get status line
    std::getline(s, line);

    if (line[line.size()-1] == '\r') {
        line.erase(line.end()-1);

        std::stringstream   ss(line);
        std::string         str_val;
        int                 int_val;
        char                char_val[256];

        ss >> str_val;
        set_version(str_val);

        ss >> int_val;
        ss.getline(char_val,256);
        set_status(status_code::value(int_val),std::string(char_val));
    } else {
        return false;
    }

    return parse_headers(s);
}

inline std::string response::raw() const {
    // todo: validation. make sure all required fields have been set?

    std::stringstream ret;

    ret << get_version() << " " << m_status_code << " " << m_status_msg;
    ret << "\r\n" << raw_headers() << "\r\n";

    ret << m_body;

    return ret.str();
}

inline void response::set_status(status_code::value code) {
    // todo: validation?
    m_status_code = code;
    m_status_msg = get_string(code);
}

inline void response::set_status(status_code::value code, const std::string&
    msg)
{
    // todo: validation?
    m_status_code = code;
    m_status_msg = msg;
}

inline void response::process(std::string::iterator begin,
    std::string::iterator end)
{
    std::string::iterator cursor_start = begin;
    std::string::iterator cursor_end = std::find(begin,end,' ');

    if (cursor_end == end) {
        throw exception("invalid response line",status_code::bad_request);
    }

    set_version(std::string(cursor_start,cursor_end));

    cursor_start = cursor_end+1;
    cursor_end = std::find(cursor_start,end,' ');

    if (cursor_end == end) {
        throw exception("invalid request line",status_code::bad_request);
    }

    int code;

    std::istringstream ss(std::string(cursor_start,cursor_end));

    if ((ss >> code).fail()) {
        throw exception("unable to parse response code",status_code::bad_request);
    }

    set_status(status_code::value(code),std::string(cursor_end+1,end));
}

inline size_t response::process_body(const char *buf, size_t len) {
    // if no content length was set then we read forever and never set m_ready
    if (m_read == 0) {
        //m_body.append(buf,len);
        //return len;
        m_state = done;
        return 0;
    }

    // otherwise m_read is the number of bytes left.
    size_t to_read;

    if (len >= m_read) {
        // if we have more bytes than we need read, read only the amount needed
        // then set done state
        to_read = m_read;
        m_state = done;
    } else {
        // we need more bytes than are available, read them all
        to_read = len;
    }

    m_body.append(buf,to_read);
    m_read -= to_read;
    return to_read;
}

} // namespace parser
} // namespace http
} // namespace websocketpp

#endif // http_parser_response_impl_hpp
