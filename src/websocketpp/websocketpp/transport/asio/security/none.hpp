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

#ifndef websocketpp_transport_security_none_hpp
#define websocketpp_transport_security_none_hpp

#include <websocketpp/common/memory.hpp>
#include <websocketpp/transport/asio/security/base.hpp>

#include <boost/asio.hpp>

#include <string>

namespace websocketpp {
namespace transport {
namespace asio {
/// a socket policy for the asio transport that implements a plain, unencrypted
/// socket
namespace basic_socket {

/// the signature of the socket init handler for this socket policy
typedef lib::function<void(connection_hdl,boost::asio::ip::tcp::socket&)>
    socket_init_handler;

/// basic boost asio connection socket component
/**
 * transport::asio::basic_socket::connection implements a connection socket
 * component using boost asio ip::tcp::socket.
 */
class connection : public lib::enable_shared_from_this<connection> {
public:
    /// type of this connection socket component
    typedef connection type;
    /// type of a shared pointer to this connection socket component
    typedef lib::shared_ptr<type> ptr;

    /// type of a pointer to the asio io_service being used
    typedef boost::asio::io_service* io_service_ptr;
    /// type of a pointer to the asio io_service strand being used
    typedef lib::shared_ptr<boost::asio::io_service::strand> strand_ptr;
    /// type of the asio socket being used
    typedef boost::asio::ip::tcp::socket socket_type;
    /// type of a shared pointer to the socket being used.
    typedef lib::shared_ptr<socket_type> socket_ptr;

    explicit connection() : m_state(uninitialized) {
        //std::cout << "transport::asio::basic_socket::connection constructor"
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
        return false;
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

    /// retrieve a pointer to the underlying socket
    /**
     * this is used internally. it can also be used to set socket options, etc
     */
    boost::asio::ip::tcp::socket& get_socket() {
        return *m_socket;
    }

    /// retrieve a pointer to the underlying socket
    /**
     * this is used internally.
     */
    boost::asio::ip::tcp::socket& get_next_layer() {
        return *m_socket;
    }

    /// retrieve a pointer to the underlying socket
    /**
     * this is used internally. it can also be used to set socket options, etc
     */
    boost::asio::ip::tcp::socket& get_raw_socket() {
        return *m_socket;
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
        boost::asio::ip::tcp::endpoint ep = m_socket->remote_endpoint(bec);

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
     * @param strand a shared pointer to the connection's asio strand
     * @param is_server whether or not the endpoint is a server or not.
     */
    lib::error_code init_asio (io_service_ptr service, strand_ptr, bool)
    {
        if (m_state != uninitialized) {
            return socket::make_error_code(socket::error::invalid_state);
        }

        m_socket = lib::make_shared<boost::asio::ip::tcp::socket>(
            lib::ref(*service));

        m_state = ready;

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
        if (m_state != ready) {
            callback(socket::make_error_code(socket::error::invalid_state));
            return;
        }

        if (m_socket_init_handler) {
            m_socket_init_handler(m_hdl,*m_socket);
        }

        m_state = reading;

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
        callback(lib::error_code());
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

    /// cancel all async operations on this socket
    void cancel_socket() {
        m_socket->cancel();
    }

    void async_shutdown(socket_shutdown_handler h) {
        boost::system::error_code ec;
        m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both,ec);
        h(ec);
    }

    lib::error_code get_ec() const {
        return lib::error_code();
    }

    /// translate any security policy specific information about an error code
    /**
     * translate_ec takes a boost error code and attempts to convert its value
     * to an appropriate websocketpp error code. the plain socket policy does
     * not presently provide any additional information so all errors will be
     * reported as the generic transport pass_through error.
     *
     * @since 0.3.0
     *
     * @param ec the error code to translate_ec
     * @return the translated error code
     */
    lib::error_code translate_ec(boost::system::error_code) {
        // we don't know any more information about this error so pass through
        return make_error_code(transport::error::pass_through);
    }
private:
    enum state {
        uninitialized = 0,
        ready = 1,
        reading = 2
    };

    socket_ptr          m_socket;
    state               m_state;

    connection_hdl      m_hdl;
    socket_init_handler m_socket_init_handler;
};

/// basic asio endpoint socket component
/**
 * transport::asio::basic_socket::endpoint implements an endpoint socket
 * component that uses boost asio's ip::tcp::socket.
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
        return false;
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
        return lib::error_code();
    }
private:
    socket_init_handler m_socket_init_handler;
};

} // namespace basic_socket
} // namespace asio
} // namespace transport
} // namespace websocketpp

#endif // websocketpp_transport_security_none_hpp
