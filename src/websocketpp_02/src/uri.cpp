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

#include "uri.hpp"

#include <sstream>
#include <iostream>

#include <boost/regex.hpp>

using websocketpp_02::uri;

uri::uri(const std::string& uri) {
    // temporary testing non-regex implimentation.
    /*enum state {
        begin = 0,
        host_begin = 1,
        read_ipv6_literal = 2,
        read_hostname = 3,
        begin_port = 4,
        read_port = 5,
        read_resource = 6,
    };

    state the_state = begin;
    std::string temp;

    for (std::string::const_iterator it = uri.begin(); it != uri.end(); it++) {
        switch (the_state) {
            case begin:
                // we are looking for a ws:// or wss://
                if (temp.size() < 6) {
                    temp.append(1,*it);
                } else {
                    throw websocketpp_02::uri_exception("scheme is too long");
                }

                if (temp == "ws://") {
                    m_secure = false;
                    the_state = host_begin;
                    temp.clear();
                } else if (temp == "wss://") {
                    m_secure = true;
                    the_state = host_begin;
                    temp.clear();
                }
                break;
            case host_begin:
                // if character is [ go into ipv6 literal state
                // otherwise go into read hostname state
                if (*it == '[') {
                    the_state = read_ipv6_literal;
                } else {
                    *it--;
                    the_state = read_hostname;
                }
                break;
            case read_ipv6_literal:
                // read 0-9a-fa-f:. until ] or 45 characters have been read
                if (*it == ']') {
                    the_state = begin_port;
                    break;
                }

                if (m_host.size() > 45) {
                    throw websocketpp_02::uri_exception("ipv6 literal is too long");
                }

                if (*it == '.' ||
                    *it == ':' ||
                    (*it >= '0' && *it <= '9') ||
                    (*it >= 'a' && *it <= 'f') ||
                    (*it >= 'a' && *it <= 'f'))
                {
                    m_host.append(1,*it);
                }
                break;
            case read_hostname:
                //
                if (*it == ':') {
                    it--;
                    the_state = begin_port;
                    break;
                }

                if (*it == '/') {
                    it--;
                    the_state = begin_port;
                    break;
                }

                // todo: max url length?

                // todo: check valid characters?
                if (1) {
                    m_host.append(1,*it);
                }
                break;
            case begin_port:
                if (*it == ':') {
                    the_state = read_port;
                } else if (*it == '/') {
                    m_port = get_port_from_string("");
                     *it--;
                    the_state = read_resource;
                } else {
                    throw websocketpp_02::uri_exception("error parsing websocket uri");
                }
                break;
            case read_port:
                if (*it >= '0' && *it <= '9') {
                    if (temp.size() < 5) {
                        temp.append(1,*it);
                    } else {
                        throw websocketpp_02::uri_exception("port is too long");
                    }
                } else if (*it == '/') {
                    m_port = get_port_from_string(temp);
                    temp.clear();
                     *it--;
                    the_state = read_resource;

                } else {
                    throw websocketpp_02::uri_exception("error parsing websocket uri");
                }
                break;
            case read_resource:
                // max length check?

                if (*it == '#') {
                    throw websocketpp_02::uri_exception("websocket uris cannot have fragments");
                } else {
                    m_resource.append(1,*it);
                }
                break;
        }
    }

    switch (the_state) {
        case read_port:
            m_port = get_port_from_string(temp);
            break;
        default:
            break;
    }

    if (m_resource == "") {
        m_resource = "/";
    }*/


    boost::cmatch matches;
    const boost::regex expression("(ws|wss)://([^/:\\[]+|\\[[0-9a-fa-f:.]+\\])(:\\d{1,5})?(/[^#]*)?");

    // todo: should this split resource into path/query?

    if (boost::regex_match(uri.c_str(), matches, expression)) {
        m_secure = (matches[1] == "wss");
        m_host = matches[2];

        // strip brackets from ipv6 literal uris
        if (m_host[0] == '[') {
            m_host = m_host.substr(1,m_host.size()-2);
        }

        std::string port(matches[3]);

        if (port != "") {
            // strip off the :
            // this could probably be done with a better regex.
            port = port.substr(1);
        }

        m_port = get_port_from_string(port);

        m_resource = matches[4];

        if (m_resource == "") {
            m_resource = "/";
        }

        return;
    }

    throw websocketpp_02::uri_exception("error parsing websocket uri");

}

uri::uri(bool secure, const std::string& host, uint16_t port, const std::string& resource)
 : m_secure(secure),
   m_host(host),
   m_port(port),
   m_resource(resource == "" ? "/" : resource) {}

uri::uri(bool secure, const std::string& host, const std::string& resource)
: m_secure(secure),
  m_host(host),
  m_port(m_secure ? uri_default_secure_port : uri_default_port),
  m_resource(resource == "" ? "/" : resource) {}

uri::uri(bool secure,
         const std::string& host,
         const std::string& port,
         const std::string& resource)
 : m_secure(secure),
   m_host(host),
   m_port(get_port_from_string(port)),
   m_resource(resource == "" ? "/" : resource) {}

/* slightly cleaner c++11 delegated constructor method

uri::uri(bool secure,
         const std::string& host,
         uint16_t port,
         const std::string& resource)
 : m_secure(secure),
   m_host(host),
   m_port(port),
   m_resource(resource == "" ? "/" : resource)
{
    if (m_port > 65535) {
        throw websocketpp_02::uri_exception("port must be less than 65535");
    }
}

uri::uri(bool secure,
         const std::string& host,
         const std::string& port,
         const std::string& resource)
 : uri(secure, host, get_port_from_string(port), resource) {}

*/

bool uri::get_secure() const {
    return m_secure;
}

std::string uri::get_host() const {
    return m_host;
}

std::string uri::get_host_port() const {
    if (m_port == (m_secure ? uri_default_secure_port : uri_default_port)) {
         return m_host;
    } else {
        std::stringstream p;
        p << m_host << ":" << m_port;
        return p.str();
    }

}

uint16_t uri::get_port() const {
    return m_port;
}

std::string uri::get_port_str() const {
    std::stringstream p;
    p << m_port;
    return p.str();
}

std::string uri::get_resource() const {
    return m_resource;
}

std::string uri::str() const {
    std::stringstream s;

    s << "ws" << (m_secure ? "s" : "") << "://" << m_host;

    if (m_port != (m_secure ? uri_default_secure_port : uri_default_port)) {
        s << ":" << m_port;
    }

    s << m_resource;
    return s.str();
}

uint16_t uri::get_port_from_string(const std::string& port) const {
    if (port == "") {
        return (m_secure ? uri_default_secure_port : uri_default_port);
    }

    unsigned int t_port = atoi(port.c_str());

    if (t_port > 65535) {
        throw websocketpp_02::uri_exception("port must be less than 65535");
    }

    if (t_port == 0) {
        throw websocketpp_02::uri_exception("error parsing port string: "+port);
    }

    return static_cast<uint16_t>(t_port);
}
