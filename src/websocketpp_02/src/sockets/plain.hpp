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

#ifndef websocketpp_socket_plain_hpp
#define websocketpp_socket_plain_hpp

#include "socket_base.hpp"

#include <boost/asio.hpp>
#include <boost/function.hpp>

#include <iostream>

#ifdef _msc_ver
// disable "warning c4355: 'this' : used in base member initializer list".
#   pragma warning(push)
#   pragma warning(disable:4355)
#endif

namespace websocketpp_02 {
namespace socket {

template <typename endpoint_type>
class plain : boost::noncopyable {
public:
    boost::asio::io_service& get_io_service() {
        return m_io_service;
    }

    bool is_secure() {
        return false;
    }

    // hooks that this policy adds to handlers of connections that use it
    class handler_interface {
    public:
    	virtual ~handler_interface() {}

        virtual void on_tcp_init() {};
    };

    // connection specific details
    template <typename connection_type>
    class connection {
    public:
        // should these two be public or protected. if protected, how?
        boost::asio::ip::tcp::socket& get_raw_socket() {
            return m_socket;
        }

        boost::asio::ip::tcp::socket& get_socket() {
            return m_socket;
        }

        bool is_secure() {
            return false;
        }
    protected:
        connection(plain<endpoint_type>& e)
         : m_socket(e.get_io_service())
         , m_connection(static_cast< connection_type& >(*this)) {}

        void init() {

        }

        void async_init(socket_init_callback callback) {
            m_connection.get_handler()->on_tcp_init();

            // todo: make configuration option for no_delay
            m_socket.set_option(boost::asio::ip::tcp::no_delay(true));

            // todo: should this use post()?
            callback(boost::system::error_code());
        }

        bool shutdown() {
            boost::system::error_code ignored_ec;
            m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both,ignored_ec);

            if (ignored_ec) {
                return false;
            } else {
                return true;
            }
        }
    private:
        boost::asio::ip::tcp::socket m_socket;
        connection_type&             m_connection;
    };
protected:
    plain (boost::asio::io_service& m) : m_io_service(m) {}
private:
    boost::asio::io_service& m_io_service;
};


} // namespace socket
} // namespace websocketpp_02

#ifdef _msc_ver
#   pragma warning(pop)
#endif

#endif // websocketpp_socket_plain_hpp
