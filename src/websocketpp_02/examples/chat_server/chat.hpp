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

#ifndef chat_hpp
#define chat_hpp

// com.zaphoyd.websocketpp.chat protocol
// 
// client messages:
// alias [utf8 text, 16 characters max]
// msg [utf8 text]
// 
// server messages:
// {"type":"msg","sender":"<sender>","value":"<msg>" }
// {"type":"participants","value":[<participant>,...]}

#include "../../src/websocketpp.hpp"

#include <map>
#include <string>
#include <vector>

using websocketpp::server;

namespace websocketchat {

class chat_server_handler : public server::handler {
public:
    void validate(connection_ptr con); 
    
    // add new connection to the lobby
    void on_open(connection_ptr con);
        
    // someone disconnected from the lobby, remove them
    void on_close(connection_ptr con);
    
    void on_message(connection_ptr con, message_ptr msg);
private:
    std::string serialize_state();
    std::string encode_message(std::string sender,std::string msg,bool escape = true);
    std::string get_con_id(connection_ptr con);
    
    void send_to_all(std::string data);
    
    // list of outstanding connections
    std::map<connection_ptr,std::string> m_connections;
};

typedef boost::shared_ptr<chat_server_handler> chat_server_handler_ptr;

}
#endif // chat_hpp
