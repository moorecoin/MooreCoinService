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

#ifndef websocketpp_transport_security_tls_hpp
#define websocketpp_transport_security_tls_hpp

#include <websocketpp/transport/asio/security/base.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/common/memory.hpp>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <string>

namespace websocketpp {
namespace transport {
namespace asio {
/// a socket policy for the asio transport that implements a tls encrypted
/// socket by wrapping with an asio::ssl::stream
namespace tls_socket {

/// the signature of the socket_init_handler for this socket policy
typedef lib::function<void(connection_hdl,boost::asio::ssl::stream<
    boost::asio::ip::tcp::socket>&)> socket_init_handler;
/// the signature of the tls_init_handler for this socket policy
typedef lib::function<lib::shared_ptr<boost::asio::ssl::context>(connection_hdl)>
    tls_init_handler;

/// tls enabled boost asio connection socket component
/**
 * transport::asio::tls_socket::connection implements a secure connection socket
 * component that uses boost asio's ssl::stream to wrap an ip::tcp::socket.
 */
class connection : public lib::enable_shared_from_this<connection> {
public:
    /// type of this connection socket component
    typedef connection type;
    /// type of a shared pointer to this connection socket component
    typedef lib::shared_ptr<type> ptr;

    /// type of the asio socket being used
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_type;
    /// type of a shared pointer to the asio socket being used
    typedef lib::shared_ptr<socket_type> socket_ptr;
    /// type of a pointer to the asio io_service being used
    typedef boost::asio::io_service* io_service_ptr;
    /// type of a pointer to the asio io_service strand being used
    typedef lib::shared_ptr<boost::asio::io_service::strand> strand_ptr;
    /// type of a shared pointer to the asio tls context being used
    typedef lib::shared_ptr<boost::asio::ssl::context> context_ptr;

    typedef boost::system::error_code boost_error;

    explicit connection() {
        //std::cout << "transport::asio::tls_socket::connection constructor"
        //          << std::endl;
    }

    /// get a shared pointer to this component
    ptr get_shared() {
        return shared_from_this();
    }

    /// check whether or not this connection is secure
    /**
     * @return whether or not this connection is secure
     */
    bool is_secure() const {
        return true;
    }

    /// retrieve a pointer to the underlying socket
    /**
     * this is used internally. it can also be used to set socket options, etc
     */
    socket_type::lowest_layer_type& get_raw_socket() {
        return m_socket->lowest_layer();
    }

    /// retrieve a pointer to the layer below the ssl stream
    /**
     * this is used internally.
     */
    socket_type::next_layer_type& get_next_layer() {
        return m_socket->next_layer();
    }

    /// retrieve a pointer to the wrapped socket
    /**
     * this is used internally.
     */
    socket_type& get_socket() {
        return *m_socket;
    }

    /// set the socket initialization handler
    /**
     * the socket initialization handler is called after the socket object is
     * created but before it is used. this gives the application a chance to
     * set any asio socket options it needs.
     *
     * @param h the new socket_init_handler
     */
    void set_socket_init_handler(socket_init_handler h) {
        m_socket_init_handler = h;
    }

    /// set tls init handler
    /**
     * the tls init handler is called when needed to request a tls context for
     * the library to use. a tls init handler must be set and it must return a
     * valid tls context in order for this endpoint to be able to initialize
     * tls connections
     *
     * @param h the new tls_init_handler
     */
    void set_tls_init_handler(tls_init_handler h) {
        m_tls_init_handler = h;
    }

    /// get the remote endpoint address
    /**
     * the iostream transport has no information about the ultimate remote
     * endpoint. it will return the string "iostream transport". to indicate
     * this.
     *
     * todo: allow user settable remote endpoint addresses if this seems useful
     *
     * @return a string identifying the address of the remote endpoint
     */
    std::string get_remote_endpoint(lib::error_code &ec) const {
        std::stringstream s;

        boost::system::error_code bec;
        boost::asio::ip::tcp::endpoint ep = m_socket->lowest_layer().remote_endpoint(bec);

        if (bec) {
            ec = error::make_error_code(error::pass_through);
            s << "error getting remote endpoint: " << bec
               << " (" << bec.message() << ")";
            return s.str();
        } else {
            ec = lib::error_code();
            s << ep;
            return s.str();
        }
    }
protected:
    /// perform one time initializations
    /**
     * init_asio is called once immediately after construction to initialize
     * boost::asio components to the io_service
     *
     * @param service a pointer to the endpoint's io_service
     * @param strand a pointer to the connection's strand
     * @param is_server whether or not the endpoint is a server or not.
     */
    lib::error_code init_asio (io_service_ptr service, strand_ptr strand,
        bool is_server)
    {
        if (!m_tls_init_handler) {
            return socket::make_error_code(socket::error::missing_tls_init_handler);
        }
        m_context = m_tls_init_handler(m_hdl);

        if (!m_context) {
            return socket::make_error_code(socket::error::invalid_tls_context);
        }
        m_socket = lib::make_shared<socket_type>(
            _websocketpp_ref(*service),lib::ref(*m_context));

        m_io_service = service;
        m_strand = strand;
        m_is_server = is_server;

        return lib::error_code();
    }

    /// pre-initialize security policy
    /**
     * called by the transport after a new connection is created to initialize
     * the socket component of the connection. this method is not allowed to
     * write any bytes to the wire. this initialization happens before any
     * proxies or other intermediate wrappers are negotiated.
     *
     * @param callback handler to call back with completion information
     */
    void pre_init(init_handler callback) {
        if (m_socket_init_handler) {
            m_socket_init_handler(m_hdl,get_socket());
        }

        callback(lib::error_code());
    }

    /// post-initialize security policy
    /**
     * called by the transport after all intermediate proxies have been
     * negotiated. this gives the security policy the chance to talk with the
     * real remote endpoint for a bit before the websocket handshake.
     *
     * @param callback handler to call back with completion information
     */
    void post_init(init_handler callback) {
        m_ec = socket::make_error_code(socket::error::tls_handshake_timeout);

        // tls handshake
        if (m_strand) {
            m_socket->async_handshake(
                get_handshake_type(),
                m_strand->wrap(lib::bind(
                    &type::handle_init, get_shared(),
                    callback,
                    lib::placeholders::_1
                ))
            );
        } else {
            m_socket->async_handshake(
                get_handshake_type(),
                lib::bind(
                    &type::handle_init, get_shared(),
                    callback,
                    lib::placeholders::_1
                )
            );
        }
    }

    /// sets the connection handle
    /**
     * the connection handle is passed to any handlers to identify the
     * connection
     *
     * @param hdl the new handle
     */
    void set_handle(connection_hdl hdl) {
        m_hdl = hdl;
    }

    void handle_init(init_handler callback,boost::system::error_code const & ec)
    {
        if (ec) {
            m_ec = socket::make_error_code(socket::error::tls_handshake_failed);
        } else {
            m_ec = lib::error_code();
        }

        callback(m_ec);
    }

    lib::error_code get_ec() const {
        return m_ec;
    }

    /// cancel all async operations on this socket
    void cancel_socket() {
        get_raw_socket().cancel();
    }

    void async_shutdown(socket_shutdown_handler callback) {
        m_socket->async_shutdown(callback);
    }

    /// translate any security policy specific information about an error code
    /**
     * translate_ec takes a boost error code and attempts to convert its value
     * to an appropriate websocketpp error code. any error that is determined to
     * be related to tls but does not have a more specific websocketpp error
     * code is returned under the catch all error "tls_error".
     *
     * non-tls related errors are returned as the transport generic pass_through
     * error.
     *
     * @since 0.3.0
     *
     * @param ec the error code to translate_ec
     * @return the translated error code
     */
    lib::error_code translate_ec(boost::system::error_code ec) {
        if (ec.category() == boost::asio::error::get_ssl_category()) {
            if (err_get_reason(ec.value()) == ssl_r_short_read) {
                return make_error_code(transport::error::tls_short_read);
            } else {
                // we know it is a tls related error, but otherwise don't know
                // more. pass through as tls generic.
                return make_error_code(transport::error::tls_error);
            }
        } else {
            // we don't know any more information about this error so pass
            // through
            return make_error_code(transport::error::pass_through);
        }
    }
private:
    socket_type::handshake_type get_handshake_type() {
        if (m_is_server) {
            return boost::asio::ssl::stream_base::server;
        } else {
            return boost::asio::ssl::stream_base::client;
        }
    }

    io_service_ptr      m_io_service;
    strand_ptr          m_strand;
    context_ptr         m_context;
    socket_ptr          m_socket;
    bool                m_is_server;

    lib::error_code     m_ec;

    connection_hdl      m_hdl;
    socket_init_handler m_socket_init_handler;
    tls_init_handler    m_tls_init_handler;
};

/// tls enabled boost asio endpoint socket component
/**
 * transport::asio::tls_socket::endpoint implements a secure endpoint socket
 * component that uses boost asio's ssl::stream to wrap an ip::tcp::socket.
 */
class endpoint {
public:
    /// the type of this endpoint socket component
    typedef endpoint type;

    /// the type of the corresponding connection socket component
    typedef connection socket_con_type;
    /// the type of a shared pointer to the corresponding connection socket
    /// component.
    typedef socket_con_type::ptr socket_con_ptr;

    explicit endpoint() {}

    /// checks whether the endpoint creates secure connections
    /**
     * @return whether or not the endpoint creates secure connections
     */
    bool is_secure() const {
        return true;
    }

    /// set socket init handler
    /**
     * the socket init handler is called after a connection's socket is created
     * but before it is used. this gives the end application an opportunity to
     * set asio socket specific parameters.
     *
     * @param h the new socket_init_handler
     */
    void set_socket_init_handler(socket_init_handler h) {
        m_socket_init_handler = h;
    }

    /// set tls init handler
    /**
     * the tls init handler is called when needed to request a tls context for
     * the library to use. a tls init handler must be set and it must return a
     * valid tls context in order for this endpoint to be able to initialize
     * tls connections
     *
     * @param h the new tls_init_handler
     */
    void set_tls_init_handler(tls_init_handler h) {
        m_tls_init_handler = h;
    }
protected:
    /// initialize a connection
    /**
     * called by the transport after a new connection is created to initialize
     * the socket component of the connection.
     *
     * @param scon pointer to the socket component of the connection
     *
     * @return error code (empty on success)
     */
    lib::error_code init(socket_con_ptr scon) {
        scon->set_socket_init_handler(m_socket_init_handler);
        scon->set_tls_init_handler(m_tls_init_handler);
        return lib::error_code();
    }

private:
    socket_init_handler m_socket_init_handler;
    tls_init_handler m_tls_init_handler;
};

} // namespace tls_socket
} // namespace asio
} // namespace transport
} // namespace websocketpp

#endif // websocketpp_transport_security_tls_hpp
