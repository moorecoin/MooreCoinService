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

#include "chat.hpp"

#include "../../src/websocketpp.hpp"
#include <boost/asio.hpp>

#include <iostream>

using namespace websocketchat;
using websocketpp::server;

int main(int argc, char* argv[]) {
    short port = 9003;
    
    if (argc == 2) {
        // todo: input validation?
        port = atoi(argv[1]);
    }
    
    try {
        // create an instance of our handler
        server::handler::ptr handler(new chat_server_handler());
        
        // create a server that listens on port `port` and uses our handler
        server endpoint(handler);
        
        endpoint.alog().set_level(websocketpp::log::alevel::connect);
        endpoint.alog().set_level(websocketpp::log::alevel::disconnect);
        
        endpoint.elog().set_level(websocketpp::log::elevel::rerror);
        endpoint.elog().set_level(websocketpp::log::elevel::fatal);
        
        // setup server settings
        // chat server should only be receiving small text messages, reduce max
        // message size limit slightly to save memory, improve performance, and 
        // guard against dos attacks.
        //server->set_max_message_size(0xffff); // 64kib
        
        std::cout << "starting chat server on port " << port << std::endl;
        
        endpoint.listen(port);
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    
    return 0;
}
