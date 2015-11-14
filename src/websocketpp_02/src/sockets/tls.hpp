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

#error use auto tls only

#ifndef websocketpp_socket_tls_hpp
#define websocketpp_socket_tls_hpp

#include "../common.hpp"
#include "socket_base.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

#include <iostream>

namespace websocketpp_02 {
namespace socket {

template <typename endpoint_type>
class tls {
public:
    typedef tls<endpoint_type> type;
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> tls_socket;
    typedef boost::shared_ptr<tls_socket> tls_socket_ptr;

    // should be private friended
    boost::asio::io_service& get_io_service() {
        return m_io_service;
    }

    static void handle_shutdown(tls_socket_ptr, const boost::system::error_code&) {
    }

    // should be private friended?
    tls_socket::handshake_type get_handshake_type() {
        if (static_cast< endpoint_type* >(this)->is_server()) {
            return boost::asio::ssl::stream_base::server;
        } else {
            return boost::asio::ssl::stream_base::client;
        }
    }

    bool is_secure() {
        return true;
    }

    class handler_interface {
    public:
    	virtual ~handler_interface() {}

        virtual void on_tcp_init() {};
        virtual boost::asio::ssl::context& on_tls_init() = 0;
    };

    // connection specific details
    template <typename connection_type>
    class connection {
    public:
        // should these two be public or protected. if protected, how?
        tls_socket::lowest_layer_type& get_raw_socket() {
            return m_socket_ptr->lowest_layer();
        }

        tls_socket& get_socket() {
            return *m_socket_ptr;
        }

        bool is_secure() {
            return true;
        }
    protected:
        connection(tls<endpoint_type>& e)
         : m_endpoint(e)
         , m_connection(static_cast< connection_type& >(*this)) {}

        void init()
        {
            boost::asio::ssl::context& ssl_context (
                m_connection.get_handler()->get_ssl_context ());

            m_socket_ptr = tls_socket_ptr (new tls_socket (
                m_endpoint.get_io_service(), ssl_context));
        }

        void async_init(boost::function<void(const boost::system::error_code&)> callback)
        {
            m_connection.get_handler()->on_tcp_init();

            // wait for tls handshake
            // todo: configurable value
            m_connection.register_timeout(5000,
                                          fail::status::timeout_tls,
                                          "timeout on tls handshake");

            m_socket_ptr->async_handshake(
                m_endpoint.get_handshake_type(),
                boost::bind(
                    &connection<connection_type>::handle_init,
                    this,
                    callback,
                    boost::asio::placeholders::error
                )
            );
        }

        void handle_init(socket_init_callback callback,const boost::system::error_code& error) {
            m_connection.cancel_timeout();
            callback(error);
        }

        // note, this function for some reason shouldn't/doesn't need to be
        // called for plain http connections. not sure why.
        bool shutdown() {
            boost::system::error_code ignored_ec;

            m_socket_ptr->async_shutdown( // don't block on connection shutdown djs
                boost::bind(
		            &tls<endpoint_type>::handle_shutdown,
                    m_socket_ptr,
                    boost::asio::placeholders::error
				)
			);

            if (ignored_ec) {
                return false;
            } else {
                return true;
            }
        }
    private:
        boost::shared_ptr<boost::asio::ssl::context>    m_context_ptr;
        tls_socket_ptr                                  m_socket_ptr;
        tls<endpoint_type>&                             m_endpoint;
        connection_type&                                m_connection;
    };
protected:
    tls (boost::asio::io_service& m) : m_io_service(m) {}
private:
    boost::asio::io_service&    m_io_service;
    tls_socket::handshake_type  m_handshake_type;
};

} // namespace socket
} // namespace websocketpp_02

#endif // websocketpp_socket_tls_hpp
