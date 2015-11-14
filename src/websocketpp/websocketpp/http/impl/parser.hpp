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

#ifndef http_parser_impl_hpp
#define http_parser_impl_hpp

#include <algorithm>
#include <sstream>
#include <string>

namespace websocketpp {
namespace http {
namespace parser {

inline void parser::set_version(std::string const & version) {
    m_version = version;
}

inline std::string const & parser::get_header(std::string const & key) const {
    header_list::const_iterator h = m_headers.find(key);

    if (h == m_headers.end()) {
        return empty_header;
    } else {
        return h->second;
    }
}

inline bool parser::get_header_as_plist(std::string const & key,
    parameter_list & out) const
{
    header_list::const_iterator it = m_headers.find(key);

    if (it == m_headers.end() || it->second.size() == 0) {
        return false;
    }

    return this->parse_parameter_list(it->second,out);
}

inline void parser::append_header(std::string const & key, std::string const &
    val)
{
    if (std::find_if(key.begin(),key.end(),is_not_token_char) != key.end()) {
        throw exception("invalid header name",status_code::bad_request);
    }

    if (this->get_header(key) == "") {
        m_headers[key] = val;
    } else {
        m_headers[key] += ", " + val;
    }
}

inline void parser::replace_header(std::string const & key, std::string const &
    val)
{
    m_headers[key] = val;
}

inline void parser::remove_header(std::string const & key) {
    m_headers.erase(key);
}

inline void parser::set_body(std::string const & value) {
    if (value.size() == 0) {
        remove_header("content-length");
        m_body = "";
        return;
    }

    std::stringstream len;
    len << value.size();
    replace_header("content-length", len.str());
    m_body = value;
}

inline bool parser::parse_parameter_list(std::string const & in,
    parameter_list & out) const
{
    if (in.size() == 0) {
        return false;
    }

    std::string::const_iterator it;
    it = extract_parameters(in.begin(),in.end(),out);
    return (it == in.begin());
}

inline bool parser::parse_headers(std::istream & s) {
    std::string header;
    std::string::size_type end;

    // get headers
    while (std::getline(s, header) && header != "\r") {
        if (header[header.size()-1] != '\r') {
            continue; // ignore malformed header lines?
        } else {
            header.erase(header.end()-1);
        }

        end = header.find(header_separator,0);

        if (end != std::string::npos) {
            append_header(header.substr(0,end),header.substr(end+2));
        }
    }

    return true;
}

inline void parser::process_header(std::string::iterator begin,
    std::string::iterator end)
{
    std::string::iterator cursor = std::search(
        begin,
        end,
        header_separator,
        header_separator + sizeof(header_separator) - 1
    );

    if (cursor == end) {
        throw exception("invalid header line",status_code::bad_request);
    }

    append_header(strip_lws(std::string(begin,cursor)),
                  strip_lws(std::string(cursor+sizeof(header_separator)-1,end)));
}

inline std::string parser::raw_headers() const {
    std::stringstream raw;

    header_list::const_iterator it;
    for (it = m_headers.begin(); it != m_headers.end(); it++) {
        raw << it->first << ": " << it->second << "\r\n";
    }

    return raw.str();
}



} // namespace parser
} // namespace http
} // namespace websocketpp

#endif // http_parser_impl_hpp
