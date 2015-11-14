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

#ifndef websocketpp_broadcast_server_handler_hpp
#define websocketpp_broadcast_server_handler_hpp

#include "../../src/sockets/tls.hpp"
#include "../../src/websocketpp.hpp"

#include "broadcast_handler.hpp"
#include "broadcast_admin_handler.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

namespace websocketpp {
namespace broadcast {
    
template <typename endpoint_type>
class server_handler : public endpoint_type::handler {
public:
    typedef server_handler<endpoint_type> type;
    typedef boost::shared_ptr<type> ptr;
    typedef typename endpoint_type::handler_ptr handler_ptr;
    typedef typename admin_handler<endpoint_type>::ptr admin_handler_ptr;
    typedef typename handler<endpoint_type>::ptr broadcast_handler_ptr;
    typedef typename endpoint_type::connection_ptr connection_ptr;
    
    server_handler();
    
    std::string get_password() const {
        return "test";
    }
    
    boost::shared_ptr<boost::asio::ssl::context> on_tls_init() {
        // create a tls context, init, and return.
        boost::shared_ptr<boost::asio::ssl::context> context(new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1));
        try {
            context->set_options(boost::asio::ssl::context::default_workarounds |
                                 boost::asio::ssl::context::no_sslv2 |
                                 boost::asio::ssl::context::no_sslv3 |
                                 boost::asio::ssl::context::single_dh_use);
            context->set_password_callback(boost::bind(&type::get_password, this));
            context->use_certificate_chain_file("../../src/ssl/server.pem");
            context->use_private_key_file("../../src/ssl/server.pem", boost::asio::ssl::context::pem);
            context->use_tmp_dh_file("../../src/ssl/dh512.pem");
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
        return context;
    }
    
    void validate(connection_ptr connection) {}
    
    void on_open(connection_ptr connection) {
        if (connection->get_resource() == "/admin") {
            connection->set_handler(m_admin_handler);
        } else {
            connection->set_handler(m_broadcast_handler);
        }
    }
    
    void on_unload(connection_ptr connection, handler_ptr new_handler) {
        
    }
    
    void on_close(connection_ptr connection) {}
    
    void on_message(connection_ptr connection,websocketpp::message::data_ptr msg) {}
    
    void http(connection_ptr connection);
    
    void on_fail(connection_ptr connection) {
        std::cout << "connection failed" << std::endl;
    }
    
    // utility
    
    handler_ptr get_broadcast_handler() {
        return m_broadcast_handler;
    }
    
private:
    admin_handler_ptr       m_admin_handler;
    broadcast_handler_ptr   m_broadcast_handler;
};

} // namespace broadcast
} // namespace websocketpp




namespace websocketpp {
namespace broadcast {

template <class endpoint>
server_handler<endpoint>::server_handler() 
 : m_admin_handler(new admin_handler<endpoint>()),
   m_broadcast_handler(new handler<endpoint>())
{
    m_admin_handler->track(m_broadcast_handler);
}

template <class endpoint>
void server_handler<endpoint>::http(connection_ptr connection) {
    std::stringstream foo;
    
    foo << "<html><body><p>" 
        << m_broadcast_handler->get_connection_count()
        << " current connections.</p></body></html>";
    
    connection->set_body(foo.str());
}

} // namespace broadcast
} // namespace websocketpp

#endif // websocketpp_broadcast_server_handler_hpp
