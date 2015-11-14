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

#ifndef websocketpp_uri_hpp
#define websocketpp_uri_hpp

#include "common.hpp"

#include <exception>

namespace websocketpp_02 {

// websocket uri only (not http/etc)

class uri_exception : public std::exception {
public:
    uri_exception(const std::string& msg) : m_msg(msg) {}
    ~uri_exception() throw() {}

    virtual const char* what() const throw() {
        return m_msg.c_str();
    }
private:
    std::string m_msg;
};

// todo: figure out why this fixes horrible linking errors.
static const uint16_t uri_default_port = 80;
static const uint16_t uri_default_secure_port = 443;

class uri {
public:
    explicit uri(const std::string& uri);
    uri(bool secure, const std::string& host, uint16_t port, const std::string& resource);
    uri(bool secure, const std::string& host, const std::string& resource);
    uri(bool secure, const std::string& host, const std::string& port, const std::string& resource);

    bool get_secure() const;
    std::string get_host() const;
    std::string get_host_port() const;
    uint16_t get_port() const;
    std::string get_port_str() const;
    std::string get_resource() const;
    std::string str() const;

    // get query?
    // get fragment

    // hi <3

    // get the string representation of this uri

    //std::string base() const; // is this still needed?

    // setter methods set some or all (in the case of parse) based on the input.
    // these functions throw a uri_exception on failure.
    /*void set_uri(const std::string& uri);

    void set_secure(bool secure);
    void set_host(const std::string& host);
    void set_port(uint16_t port);
    void set_port(const std::string& port);
    void set_resource(const std::string& resource);*/
private:
    uint16_t get_port_from_string(const std::string& port) const;

    bool        m_secure;
    std::string m_host;
    uint16_t    m_port;
    std::string m_resource;
};

typedef boost::shared_ptr<uri> uri_ptr;

} // namespace websocketpp_02

#endif // websocketpp_uri_hpp
