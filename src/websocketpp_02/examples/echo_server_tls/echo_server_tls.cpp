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

#include "../../src/sockets/tls.hpp"
#include "../../src/websocketpp.hpp"

#include <cstring>

using websocketpp::server;
using websocketpp::server_tls;

template <typename endpoint_type>
class echo_server_handler : public endpoint_type::handler {
public:
    typedef echo_server_handler<endpoint_type> type;
    typedef typename endpoint_type::handler::connection_ptr connection_ptr;
    typedef typename endpoint_type::handler::message_ptr message_ptr;
    
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
    
    void on_message(connection_ptr con,message_ptr msg) {
        con->send(msg->get_payload(),msg->get_opcode());
    }
    
    void http(connection_ptr con) {
        con->set_body("<!doctype html><html><head><title>websocket++ tls certificate test</title></head><body><h1>websocket++ tls certificate test</h1><p>this is an http(s) page served by a websocket++ server for the purposes of confirming that certificates are working since browsers normally silently ignore certificate issues.</p></body></html>");
    }
};

int main(int argc, char* argv[]) {
    unsigned short port = 9002;
    bool tls = false;
        
    if (argc >= 2) {
        port = atoi(argv[1]);
        
        if (port == 0) {
            std::cout << "unable to parse port input " << argv[1] << std::endl;
            return 1;
        }
    }
    
    if (argc == 3) {
        tls = !strcmp(argv[2],"-tls");
    }
    
    try {
        if (tls) {
            server_tls::handler::ptr handler(new echo_server_handler<server_tls>());
            server_tls endpoint(handler);
            
            endpoint.alog().unset_level(websocketpp::log::alevel::all);
            endpoint.elog().unset_level(websocketpp::log::elevel::all);
            
            std::cout << "starting secure websocket echo server on port " 
                      << port << std::endl;
            endpoint.listen(port);
        } else {
            server::handler::ptr handler(new echo_server_handler<server>());
            server endpoint(handler);
            
            endpoint.alog().unset_level(websocketpp::log::alevel::all);
            endpoint.elog().unset_level(websocketpp::log::elevel::all);
            
            std::cout << "starting websocket echo server on port " 
                      << port << std::endl;
            endpoint.listen(port);
        }
        
        
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    
    return 0;
}
