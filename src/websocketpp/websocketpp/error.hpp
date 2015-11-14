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

#ifndef websocketpp_error_hpp
#define websocketpp_error_hpp

#include <string>

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/system_error.hpp>

namespace websocketpp {

/// combination error code / string type for returning two values
typedef std::pair<lib::error_code,std::string> err_str_pair;

/// library level error codes
namespace error {
enum value {
    /// catch-all library error
    general = 1,

    /// send attempted when endpoint write queue was full
    send_queue_full,

    /// attempted an operation using a payload that was improperly formatted
    /// ex: invalid utf8 encoding on a text message.
    payload_violation,

    /// attempted to open a secure connection with an insecure endpoint
    endpoint_not_secure,

    /// attempted an operation that required an endpoint that is no longer
    /// available. this is usually because the endpoint went out of scope
    /// before a connection that it created.
    endpoint_unavailable,

    /// an invalid uri was supplied
    invalid_uri,

    /// the endpoint is out of outgoing message buffers
    no_outgoing_buffers,

    /// the endpoint is out of incoming message buffers
    no_incoming_buffers,

    /// the connection was in the wrong state for this operation
    invalid_state,

    /// unable to parse close code
    bad_close_code,

    /// close code is in a reserved range
    reserved_close_code,

    /// close code is invalid
    invalid_close_code,

    /// invalid utf-8
    invalid_utf8,

    /// invalid subprotocol
    invalid_subprotocol,

    /// an operation was attempted on a connection that did not exist or was
    /// already deleted.
    bad_connection,

    /// unit testing utility error code
    test,

    /// connection creation attempted failed
    con_creation_failed,

    /// selected subprotocol was not requested by the client
    unrequested_subprotocol,

    /// attempted to use a client specific feature on a server endpoint
    client_only,

    /// attempted to use a server specific feature on a client endpoint
    server_only,

    /// http connection ended
    http_connection_ended,

    /// websocket opening handshake timed out
    open_handshake_timeout,

    /// websocket close handshake timed out
    close_handshake_timeout,

    /// invalid port in uri
    invalid_port,

    /// an async accept operation failed because the underlying transport has been
    /// requested to not listen for new connections anymore.
    async_accept_not_listening,

    /// the requested operation was canceled
    operation_canceled
}; // enum value


class category : public lib::error_category {
public:
    category() {}

    char const * name() const _websocketpp_noexcept_token_ {
        return "websocketpp";
    }

    std::string message(int value) const {
        switch(value) {
            case error::general:
                return "generic error";
            case error::send_queue_full:
                return "send queue full";
            case error::payload_violation:
                return "payload violation";
            case error::endpoint_not_secure:
                return "endpoint not secure";
            case error::endpoint_unavailable:
                return "endpoint not available";
            case error::invalid_uri:
                return "invalid uri";
            case error::no_outgoing_buffers:
                return "no outgoing message buffers";
            case error::no_incoming_buffers:
                return "no incoming message buffers";
            case error::invalid_state:
                return "invalid state";
            case error::bad_close_code:
                return "unable to extract close code";
            case error::invalid_close_code:
                return "extracted close code is in an invalid range";
            case error::reserved_close_code:
                return "extracted close code is in a reserved range";
            case error::invalid_utf8:
                return "invalid utf-8";
            case error::invalid_subprotocol:
                return "invalid subprotocol";
            case error::bad_connection:
                return "bad connection";
            case error::test:
                return "test error";
            case error::con_creation_failed:
                return "connection creation attempt failed";
            case error::unrequested_subprotocol:
                return "selected subprotocol was not requested by the client";
            case error::client_only:
                return "feature not available on server endpoints";
            case error::server_only:
                return "feature not available on client endpoints";
            case error::http_connection_ended:
                return "http connection ended";
            case error::open_handshake_timeout:
                return "the opening handshake timed out";
            case error::close_handshake_timeout:
                return "the closing handshake timed out";
            case error::invalid_port:
                return "invalid uri port";
            case error::async_accept_not_listening:
                return "async accept not listening";
            case error::operation_canceled:
                return "operation canceled";
            default:
                return "unknown";
        }
    }
};

inline const lib::error_category& get_category() {
    static category instance;
    return instance;
}

inline lib::error_code make_error_code(error::value e) {
    return lib::error_code(static_cast<int>(e), get_category());
}

} // namespace error
} // namespace websocketpp

_websocketpp_error_code_enum_ns_start_
template<> struct is_error_code_enum<websocketpp::error::value>
{
    static bool const value = true;
};
_websocketpp_error_code_enum_ns_end_

namespace websocketpp {

class exception : public std::exception {
public:
    exception(std::string const & msg, lib::error_code ec = make_error_code(error::general))
      : m_msg(msg), m_code(ec)
    {}

    explicit exception(lib::error_code ec)
      : m_code(ec)
    {}

    ~exception() throw() {}

    virtual char const * what() const throw() {
        if (m_msg.empty()) {
            return m_code.message().c_str();
        } else {
            return m_msg.c_str();
        }
    }

    lib::error_code code() const throw() {
        return m_code;
    }

    std::string m_msg;
    lib::error_code m_code;
};

} // namespace websocketpp

#endif // websocketpp_error_hpp
