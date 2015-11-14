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

#ifndef chat_client_handler_hpp
#define chat_client_handler_hpp

// com.zaphoyd.websocketpp.chat protocol
// 
// client messages:
// alias [utf8 text, 16 characters max]
// msg [utf8 text]
// 
// server messages:
// {"type":"msg","sender":"<sender>","value":"<msg>" }
// {"type":"participants","value":[<participant>,...]}

#include <boost/shared_ptr.hpp>

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <map>
#include <string>
#include <queue>

using websocketpp::client;

namespace websocketchat {

class chat_client_handler : public client::handler {
public:
    chat_client_handler() {}
    virtual ~chat_client_handler() {}
    
    void on_fail(connection_ptr con);
    
    // connection to chat room complete
    void on_open(connection_ptr con);

    // connection to chat room closed
    void on_close(connection_ptr con);
    
    // got a new message from server
    void on_message(connection_ptr con, message_ptr msg);
    
    // client api
    void send(const std::string &msg);
    void close();

private:
    void decode_server_msg(const std::string &msg);
    
    // list of other chat participants
    std::set<std::string> m_participants;
    std::queue<std::string> m_msg_queue;
    connection_ptr m_con;
};

typedef boost::shared_ptr<chat_client_handler> chat_client_handler_ptr;

}
#endif // chat_client_handler_hpp
