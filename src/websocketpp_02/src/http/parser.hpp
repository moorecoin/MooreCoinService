/*
 * copyright (c) 2011, peter thorson. all rights reserved.
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

#ifndef http_parser_hpp
#define http_parser_hpp

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "constants.hpp"

namespace websocketpp_02 {
namespace http {
namespace parser {

    namespace state {
        enum value {
            method,
            resource,
            version,
            headers
        };
    }

typedef std::map<std::string,std::string> header_list;

// vfalco note can't we just use
//
//        std::transform (str.begin (), str.end (), str.begin (), ::tolower);
//
//        ?
//
static std::string tolower(const std::string& in)
{
	std::string out = in;
	for (std::size_t i = 0; i < out.size(); ++i)
		if (isupper(out[i]))
			out[i] = ::tolower(out[i]);
	return out;
}

class parser {
public:
    // consumes bytes from the stream and returns true if enough bytes have
    // been read
    bool consume (std::istream&) {
        throw "not implemented";
    }

    void set_version(const std::string& version) {
        // todo: validation?
        m_version = version;
    }

    const std::string& version() const {
        return m_version;
    }

    std::string header(const std::string& key) const {
        header_list::const_iterator h = m_headers.find(key);
        if (h != m_headers.end()) {
            return h->second;
        }

        h = m_headers.find(tolower(key));

        if (h == m_headers.end()) {
            return "";
        } else {
            return h->second;
        }
    }

    // multiple calls to add header will result in values aggregating.
    // use replace_header if you do not want this behavior.
    void add_header(const std::string &key, const std::string &val) {
        // todo: prevent use of reserved headers?
        if (this->header(key) == "") {
            m_headers[key] = val;
        } else {
            m_headers[key] += ", " + val;
        }
    }

    void replace_header(const std::string &key, const std::string &val) {
        m_headers[key] = val;
    }

    void remove_header(const std::string &key) {
        m_headers.erase(key);
    }
protected:
    bool parse_headers(std::istream& s) {
        std::string header;
        std::string::size_type end;

        // get headers
        while (std::getline(s, header) && header != "\r") {
            if (header[header.size()-1] != '\r') {
                continue; // ignore malformed header lines?
            } else {
                header.erase(header.end()-1);
            }

            end = header.find(": ",0);

            if (end != std::string::npos) {
                add_header(header.substr(0,end),header.substr(end+2));
            }
        }

        return true;
    }

    std::string raw_headers() {
        std::stringstream raw;

        header_list::iterator it;
        for (it = m_headers.begin(); it != m_headers.end(); it++) {
            raw << it->first << ": " << it->second << "\r\n";
        }

        return raw.str();
    }

private:
    std::string m_version;
    header_list m_headers;
};

class request : public parser {
public:
    // parse a complete header (ie \r\n\r\n must be in the input stream)
    bool parse_complete(std::istream& s) {
        std::string request;

        // get status line
        std::getline(s, request);

        if (request[request.size()-1] == '\r') {
            request.erase(request.end()-1);

            std::stringstream ss(request);
            std::string val;

            ss >> val;
            set_method(val);

            ss >> val;
            set_uri(val);

            ss >> val;
            set_version(val);
        } else {
            set_method(request);
            return false;
        }

        return parse_headers(s);
    }

    // todo: validation. make sure all required fields have been set?
    std::string raw() {
        std::stringstream raw;

        raw << m_method << " " << m_uri << " " << version() << "\r\n";
        raw << raw_headers() << "\r\n";

        return raw.str();
    }

    void set_method(const std::string& method) {
        // todo: validation?
        m_method = method;
    }

    const std::string& method() const {
        return m_method;
    }

    void set_uri(const std::string& uri) {
        // todo: validation?
        m_uri = uri;
    }

    const std::string& uri() const {
        return m_uri;
    }

private:
    std::string m_method;
    std::string m_uri;
};

class response : public parser {
public:
    // parse a complete header (ie \r\n\r\n must be in the input stream)
    bool parse_complete(std::istream& s) {
        std::string response;

        // get status line
        std::getline(s, response);

        if (response[response.size()-1] == '\r') {
            response.erase(response.end()-1);

            std::stringstream   ss(response);
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

    // todo: validation. make sure all required fields have been set?
    std::string raw() {
        std::stringstream raw;

        raw << version() << " " << m_status_code << " " << m_status_msg << "\r\n";
        raw << raw_headers() << "\r\n";

        raw << m_body;

        return raw.str();
    }

    void set_status(status_code::value code) {
        // todo: validation?
        m_status_code = code;
        m_status_msg = get_string(code);
    }

    void set_status(status_code::value code, const std::string& msg) {
        // todo: validation?
        m_status_code = code;
        m_status_msg = msg;
    }

    void set_body(const std::string& value) {
        if (value.size() == 0) {
            remove_header("content-length");
            m_body = "";
            return;
        }

        std::stringstream foo;
        foo << value.size();
        replace_header("content-length", foo.str());
        m_body = value;
    }

    status_code::value get_status_code() const {
        return m_status_code;
    }

    const std::string& get_status_msg() const {
        return m_status_msg;
    }
private:
    status_code::value  m_status_code;
    std::string         m_status_msg;
    std::string         m_body;
};

}
}
}

#endif // http_parser_hpp
