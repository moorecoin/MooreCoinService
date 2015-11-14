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

#include "chat_client_handler.hpp"

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <iostream>

using boost::asio::ip::tcp;
using websocketpp::client;

using namespace websocketchat;

int main(int argc, char* argv[]) {
    std::string uri;
    
    if (argc != 2) {
        std::cout << "usage: `chat_client ws_uri`" << std::endl;
    } else {
        uri = argv[1];
    }
        
    try {
        chat_client_handler_ptr handler(new chat_client_handler());
        client endpoint(handler);
        client::connection_ptr con;
        
        endpoint.alog().unset_level(websocketpp::log::alevel::all);
        endpoint.elog().unset_level(websocketpp::log::elevel::all);
        
        endpoint.elog().set_level(websocketpp::log::elevel::rerror);
        endpoint.elog().set_level(websocketpp::log::elevel::fatal);
        
        con = endpoint.get_connection(uri);
        
        con->add_request_header("user-agent","websocket++/0.2.0 websocket++chat/0.2.0");
        con->add_subprotocol("com.zaphoyd.websocketpp.chat");
        
        con->set_origin("http://zaphoyd.com");

        endpoint.connect(con);
        
        boost::thread t(boost::bind(&client::run, &endpoint, false));
        
        char line[512];
        while (std::cin.getline(line, 512)) {
            handler->send(line);
        }
        
        t.join();
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    
    return 0;
}
