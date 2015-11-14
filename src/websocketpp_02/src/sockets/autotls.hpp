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

#ifndef websocketpp_socket_autotls_hpp
#define websocketpp_socket_autotls_hpp

#include <ripple/websocket/autosocket/autosocket.h>
#include <beast/asio/placeholders.h>
#include <functional>

// note that autosocket.h must be included before this file

namespace websocketpp_02 {
namespace socket {

template <typename endpoint_type>
class autotls {
public:
    typedef autotls<endpoint_type> type;
    typedef autosocket autotls_socket;
    typedef boost::shared_ptr<autotls_socket> autotls_socket_ptr;

    // should be private friended
    boost::asio::io_service& get_io_service() {
        return m_io_service;
    }

    static void handle_shutdown(autotls_socket_ptr, const boost::system::error_code&) {
    }

    // should be private friended?
    autotls_socket::handshake_type get_handshake_type() {
        if (static_cast< endpoint_type* >(this)->is_server()) {
            return boost::asio::ssl::stream_base::server;
        } else {
            return boost::asio::ssl::stream_base::client;
        }
    }

    class handler_interface {
    public:
    	virtual ~handler_interface() = default;

        virtual void on_tcp_init() {};
        virtual boost::asio::ssl::context& get_ssl_context () = 0;
        virtual bool plain_only() = 0;
        virtual bool secure_only() = 0;
    };

    // connection specific details
    template <typename connection_type>
    class connection {
    public:
        // should these two be public or protected. if protected, how?
        autotls_socket::lowest_layer_type& get_raw_socket() {
            return m_socket_ptr->lowest_layer();
        }

        autotls_socket& get_socket() {
            return *m_socket_ptr;
        }

        bool is_secure() {
            return m_socket_ptr->issecure();
        }

        typename autosocket::lowest_layer_type&
        get_native_socket() {
            return m_socket_ptr->lowest_layer();
        }

    protected:
        connection(autotls<endpoint_type>& e)
         : m_endpoint(e)
         , m_connection(static_cast< connection_type& >(*this)) {}

        void init() {
            boost::asio::ssl::context& ssl_context (
                m_connection.get_handler()->get_ssl_context());

            m_socket_ptr = autotls_socket_ptr (new autotls_socket (
                m_endpoint.get_io_service(), ssl_context, m_connection.get_handler()->secure_only(),
                    m_connection.get_handler()->plain_only()));
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
                std::bind(
                    &connection<connection_type>::handle_init,
                    this,
                    callback,
                    beast::asio::placeholders::error
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
                std::bind(
		            &autotls<endpoint_type>::handle_shutdown,
                    m_socket_ptr,
                    beast::asio::placeholders::error
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
        autotls_socket_ptr                              m_socket_ptr;
        autotls<endpoint_type>&                         m_endpoint;
        connection_type&                                m_connection;
    };
protected:
    autotls (boost::asio::io_service& m) : m_io_service(m)
    {
    }

private:
    boost::asio::io_service&    m_io_service;
};

} // namespace socket
} // namespace websocketpp_02

#endif // websocketpp_socket_autotls_hpp
