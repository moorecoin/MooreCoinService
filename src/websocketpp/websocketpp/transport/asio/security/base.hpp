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

#ifndef websocketpp_transport_asio_socket_base_hpp
#define websocketpp_transport_asio_socket_base_hpp

#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/common/system_error.hpp>
#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/connection_hdl.hpp>

#include <boost/asio.hpp>

#include <string>

// interface that sockets/security policies must implement

/*
 * endpoint interface
 *
 * bool is_secure() const;
 * @return whether or not the endpoint creates secure connections
 *
 * lib::error_code init(socket_con_ptr scon);
 * called by the transport after a new connection is created to initialize
 * the socket component of the connection.
 * @param scon pointer to the socket component of the connection
 * @return error code (empty on success)
 */


// connection
// todo
// pre_init(init_handler);
// post_init(init_handler);

namespace websocketpp {
namespace transport {
namespace asio {
namespace socket {

/**
 * the transport::asio::socket::* classes are a set of security/socket related
 * policies and support code for the asio transport types.
 */

/// errors related to asio transport sockets
namespace error {
    enum value {
        /// catch-all error for security policy errors that don't fit in other
        /// categories
        security = 1,

        /// catch-all error for socket component errors that don't fit in other
        /// categories
        socket,

        /// a function was called in a state that it was illegal to do so.
        invalid_state,

        /// the application was prompted to provide a tls context and it was
        /// empty or otherwise invalid
        invalid_tls_context,

        /// tls handshake timeout
        tls_handshake_timeout,

        /// pass_through from underlying library
        pass_through,

        /// required tls_init handler not present
        missing_tls_init_handler,

        /// tls handshake failed
        tls_handshake_failed,
    };
} // namespace error

/// error category related to asio transport socket policies
class socket_category : public lib::error_category {
public:
    const char *name() const _websocketpp_noexcept_token_ {
        return "websocketpp.transport.asio.socket";
    }

    std::string message(int value) const {
        switch(value) {
            case error::security:
                return "security policy error";
            case error::socket:
                return "socket component error";
            case error::invalid_state:
                return "invalid state";
            case error::invalid_tls_context:
                return "invalid or empty tls context supplied";
            case error::tls_handshake_timeout:
                return "tls handshake timed out";
            case error::pass_through:
                return "pass through from socket policy";
            case error::missing_tls_init_handler:
                return "required tls_init handler not present.";
            case error::tls_handshake_failed:
                return "tls handshake failed";
            default:
                return "unknown";
        }
    }
};

inline const lib::error_category& get_socket_category() {
    static socket_category instance;
    return instance;
}

inline lib::error_code make_error_code(error::value e) {
    return lib::error_code(static_cast<int>(e), get_socket_category());
}

/// type of asio transport socket policy initialization handlers
typedef lib::function<void(const lib::error_code&)> init_handler;

} // namespace socket
} // namespace asio
} // namespace transport
} // namespace websocketpp

#endif // websocketpp_transport_asio_socket_base_hpp
