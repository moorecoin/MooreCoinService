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

#include <boost/algorithm/string/replace.hpp>

using namespace websocketchat;
//using chat_server_handler::connection_ptr;

void chat_server_handler::validate(connection_ptr con) {
    std::stringstream err;
    
    // we only know about the chat resource
    if (con->get_resource() != "/chat") {
        err << "request for unknown resource " << con->get_resource();
        throw(websocketpp::http::exception(err.str(),websocketpp::http::status_code::not_found));
    }
    
    // require specific origin example
    if (con->get_origin() != "http://zaphoyd.com") {
        err << "request from unrecognized origin: " << con->get_origin();
        throw(websocketpp::http::exception(err.str(),websocketpp::http::status_code::forbidden));
    }
}


void chat_server_handler::on_open(connection_ptr con) {
    std::cout << "client " << con << " joined the lobby." << std::endl;
    m_connections.insert(std::pair<connection_ptr,std::string>(con,get_con_id(con)));

    // send user list and signon message to all clients
    send_to_all(serialize_state());
    con->send(encode_message("server","welcome, use the /alias command to set a name, /help for a list of other commands."));
    send_to_all(encode_message("server",m_connections[con]+" has joined the chat."));
}

void chat_server_handler::on_close(connection_ptr con) {
    std::map<connection_ptr,std::string>::iterator it = m_connections.find(con);
    
    if (it == m_connections.end()) {
        // this client has already disconnected, we can ignore this.
        // this happens during certain types of disconnect where there is a
        // deliberate "soft" disconnection preceeding the "hard" socket read
        // fail or disconnect ack message.
        return;
    }
    
    std::cout << "client " << con << " left the lobby." << std::endl;
    
    const std::string alias = it->second;
    m_connections.erase(it);

    // send user list and signoff message to all clients
    send_to_all(serialize_state());
    send_to_all(encode_message("server",alias+" has left the chat."));
}

void chat_server_handler::on_message(connection_ptr con, message_ptr msg) {
    if (msg->get_opcode() != websocketpp::frame::opcode::text) {
        return;
    }
    
    std::cout << "message from client " << con << ": " << msg->get_payload() << std::endl;
    
    
    
    // check for special command messages
    if (msg->get_payload() == "/help") {
        // print command list
        con->send(encode_message("server","avaliable commands:<br />&nbsp;&nbsp;&nbsp;&nbsp;/help - show this help<br />&nbsp;&nbsp;&nbsp;&nbsp;/alias foo - set alias to foo",false));
        return;
    }
    
    if (msg->get_payload().substr(0,7) == "/alias ") {
        std::string response;
        std::string alias;
        
        if (msg->get_payload().size() == 7) {
            response = "you must enter an alias.";
            con->send(encode_message("server",response));
            return;
        } else {
            alias = msg->get_payload().substr(7);
        }
        
        response = m_connections[con] + " is now known as "+alias;

        // store alias pre-escaped so we don't have to do this replacing every time this
        // user sends a message
        
        // escape json characters
        boost::algorithm::replace_all(alias,"\\","\\\\");
        boost::algorithm::replace_all(alias,"\"","\\\"");
        
        // escape html characters
        boost::algorithm::replace_all(alias,"&","&amp;");
        boost::algorithm::replace_all(alias,"<","&lt;");
        boost::algorithm::replace_all(alias,">","&gt;");
        
        m_connections[con] = alias;
        
        // set alias
        send_to_all(serialize_state());
        send_to_all(encode_message("server",response));
        return;
    }
    
    // catch other slash commands
    if ((msg->get_payload())[0] == '/') {
        con->send(encode_message("server","unrecognized command"));
        return;
    }
    
    // create json message to send based on msg
    send_to_all(encode_message(m_connections[con],msg->get_payload()));
}

// {"type":"participants","value":[<participant>,...]}
std::string chat_server_handler::serialize_state() {
    std::stringstream s;
    
    s << "{\"type\":\"participants\",\"value\":[";
    
    std::map<connection_ptr,std::string>::iterator it;
    
    for (it = m_connections.begin(); it != m_connections.end(); it++) {
        s << "\"" << (*it).second << "\"";
        if (++it != m_connections.end()) {
            s << ",";
        }
        it--;
    }
    
    s << "]}";
    
    return s.str();
}

// {"type":"msg","sender":"<sender>","value":"<msg>" }
std::string chat_server_handler::encode_message(std::string sender,std::string msg,bool escape) {
    std::stringstream s;
    
    // escape json characters
    boost::algorithm::replace_all(msg,"\\","\\\\");
    boost::algorithm::replace_all(msg,"\"","\\\"");
    
    // escape html characters
    if (escape) {
        boost::algorithm::replace_all(msg,"&","&amp;");
        boost::algorithm::replace_all(msg,"<","&lt;");
        boost::algorithm::replace_all(msg,">","&gt;");
    }
    
    s << "{\"type\":\"msg\",\"sender\":\"" << sender 
      << "\",\"value\":\"" << msg << "\"}";
    
    return s.str();
}

std::string chat_server_handler::get_con_id(connection_ptr con) {
    std::stringstream endpoint;
    //endpoint << con->get_endpoint();
    endpoint << con;
    return endpoint.str();
}

void chat_server_handler::send_to_all(std::string data) {
    std::map<connection_ptr,std::string>::iterator it;
    for (it = m_connections.begin(); it != m_connections.end(); it++) {
        (*it).first->send(data);
    }
}
