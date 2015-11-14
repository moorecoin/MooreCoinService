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

#include "../../src/websocketpp.hpp"

#include <cstring>

using websocketpp::server;

class echo_server_handler : public server::handler {
public:
    void on_message(connection_ptr con, message_ptr msg) {
        con->send(msg->get_payload(),msg->get_opcode());
    }
};

int main(int argc, char* argv[]) {
    unsigned short port = 9002;
        
    if (argc == 2) {
        port = atoi(argv[1]);
        
        if (port == 0) {
            std::cout << "unable to parse port input " << argv[1] << std::endl;
            return 1;
        }
    }
    
    try {       
        server::handler::ptr h(new echo_server_handler());
        server echo_endpoint(h);
        
        echo_endpoint.alog().unset_level(websocketpp::log::alevel::all);
        echo_endpoint.elog().unset_level(websocketpp::log::elevel::all);
        
        echo_endpoint.alog().set_level(websocketpp::log::alevel::connect);
        echo_endpoint.alog().set_level(websocketpp::log::alevel::disconnect);
        
        echo_endpoint.elog().set_level(websocketpp::log::elevel::rerror);
        echo_endpoint.elog().set_level(websocketpp::log::elevel::fatal);
        
        std::cout << "starting websocket echo server on port " << port << std::endl;
        echo_endpoint.listen(port);
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    
    return 0;
}
