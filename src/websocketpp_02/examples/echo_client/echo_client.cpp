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

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <iostream>

using websocketpp::client;

class echo_client_handler : public client::handler {
public:
    void on_message(connection_ptr con, message_ptr msg) {        
        if (con->get_resource() == "/getcasecount") {
            std::cout << "detected " << msg->get_payload() << " test cases." << std::endl;
            m_case_count = atoi(msg->get_payload().c_str());
        } else {
            con->send(msg->get_payload(),msg->get_opcode());
        }
    }
    
    void on_fail(connection_ptr con) {
        std::cout << "connection failed" << std::endl;
    }
    
    int m_case_count;
};


int main(int argc, char* argv[]) {
    std::string uri = "ws://localhost:9001/";
    
    if (argc == 2) {
        uri = argv[1];
        
    } else if (argc > 2) {
        std::cout << "usage: `echo_client test_url`" << std::endl;
    }
    
    try {
        client::handler::ptr handler(new echo_client_handler());
        client::connection_ptr con;
        client endpoint(handler);
        
        endpoint.alog().unset_level(websocketpp::log::alevel::all);
        endpoint.elog().unset_level(websocketpp::log::elevel::all);
        
        con = endpoint.connect(uri+"getcasecount");
                
        endpoint.run();
        
        std::cout << "case count: " << boost::dynamic_pointer_cast<echo_client_handler>(handler)->m_case_count << std::endl;
        
        for (int i = 1; i <= boost::dynamic_pointer_cast<echo_client_handler>(handler)->m_case_count; i++) {
            endpoint.reset();
            
            std::stringstream url;
            
            url << uri << "runcase?case=" << i << "&agent=websocket++/0.2.0-dev";
                        
            con = endpoint.connect(url.str());
            
            endpoint.run();
        }
        
        std::cout << "done" << std::endl;
        
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    
    return 0;
}
