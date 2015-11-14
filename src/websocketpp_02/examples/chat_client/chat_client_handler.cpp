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

#include <boost/algorithm/string/replace.hpp>

using websocketchat::chat_client_handler;
using websocketpp::client;

void chat_client_handler::on_fail(connection_ptr con) {
    std::cout << "connection failed" << std::endl;
}

void chat_client_handler::on_open(connection_ptr con) {
    m_con = con;

    std::cout << "successfully connected" << std::endl;
}

void chat_client_handler::on_close(connection_ptr con) {
    m_con = connection_ptr();

    std::cout << "client was disconnected" << std::endl;
}

void chat_client_handler::on_message(connection_ptr con,message_ptr msg) {
    decode_server_msg(msg->get_payload());
}

// client api
// client api methods will be called from outside the io_service.run thread
//  they need to be careful to not touch unsyncronized member variables.

void chat_client_handler::send(const std::string &msg) {
    if (!m_con) {
        std::cerr << "error: no connected session" << std::endl;
        return;
    }
    
    if (msg == "/list") {
        std::cout << "list all participants" << std::endl;
    } else if (msg == "/close") {
        close();
    } else {
        m_con->send(msg);
    }
}

void chat_client_handler::close() {
    if (!m_con) {
        std::cerr << "error: no connected session" << std::endl;
        return;
    }
    m_con->close(websocketpp::close::status::going_away,"");
}

// end client api


// {"type":"participants","value":[<participant>,...]}
// {"type":"msg","sender":"<sender>","value":"<msg>" }
void chat_client_handler::decode_server_msg(const std::string &msg) {
    // for messages of type participants, erase and rebuild m_participants
    // for messages of type msg, print out message
    
    // note: the chat server was written with the intention of the client having a built in
    // json parser. to keep external dependencies low for this demonstration chat client i am
    // parsing the server messages by hand.
    
    std::string::size_type start = 9;
    std::string::size_type end;
    
    if (msg.substr(0,start) != "{\"type\":\"") {
        // ignore
        std::cout << "invalid message" << std::endl;
        return;
    }
    
    

    if (msg.substr(start,15) == "msg\",\"sender\":\"") {
        // parse message
        std::string sender;
        std::string message;
        
        start += 15;

        end = msg.find("\"",start);
        while (end != std::string::npos) {
            if (msg[end-1] == '\\') {
                sender += msg.substr(start,end-start-1) + "\"";
                start = end+1;
                end = msg.find("\"",start);
            } else {
                sender += msg.substr(start,end-start);
                start = end;
                break;
            }
        }
        
        if (msg.substr(start,11) != "\",\"value\":\"") {
            std::cout << "invalid message" << std::endl;
            return;
        }

        start += 11;

        end = msg.find("\"",start);
        while (end != std::string::npos) {
            if (msg[end-1] == '\\') {
                message += msg.substr(start,end-start-1) + "\"";
                start = end+1;
                end = msg.find("\"",start);
            } else {
                message += msg.substr(start,end-start);
                start = end;
                break;
            }
        }

        std::cout << "[" << sender << "] " << message << std::endl;
    } else if (msg.substr(start,23) == "participants\",\"value\":[") {
        // parse participants
        std::cout << "participants message" << std::endl;
    } else {
        // unknown message type
        std::cout << "unknown message" << std::endl;
    }
}
