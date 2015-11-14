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

#ifndef websocketpp_broadcast_handler_hpp
#define websocketpp_broadcast_handler_hpp

#include "wscmd.hpp"

#include "../../src/sockets/tls.hpp"
#include "../../src/websocketpp.hpp"

#include "../../src/md5/md5.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

#include <map>
#include <set>

namespace websocketpp {
namespace broadcast {

/// this structure is used to keep track of message statistics
struct msg {
    int         id;
    size_t      sent;
    size_t      acked;
    size_t      size;
    uint64_t    time;
    
    std::string hash;
    boost::posix_time::ptime    time_sent;
};

typedef std::map<std::string,struct msg> msg_map;

template <typename endpoint_type>
class handler : public endpoint_type::handler {
public:
    typedef handler<endpoint_type> type;
    typedef boost::shared_ptr<type> ptr;
    typedef typename endpoint_type::handler_ptr handler_ptr;
    typedef typename endpoint_type::connection_ptr connection_ptr;
    
    handler() : m_nextid(0) {}
    
    void on_open(connection_ptr connection) {
        m_connections.insert(connection);
    }
    
    // this dummy tls init function will cause all tls connections to fail.
    // tls handling for broadcast::handler is usually done by a lobby handler.
    // if you want to use the broadcast handler alone with tls then return the
    // appropriately filled in context here.
    boost::shared_ptr<boost::asio::ssl::context> on_tls_init() {
        return boost::shared_ptr<boost::asio::ssl::context>();
    }
    
    void on_load(connection_ptr connection, handler_ptr old_handler) {
        this->on_open(connection);
        m_lobby = old_handler;
    }
    
    void on_close(connection_ptr connection) {
        m_connections.erase(connection);
    }
    
    void on_message(connection_ptr connection,message::data_ptr msg) {
        wscmd::cmd command = wscmd::parse(msg->get_payload());
        
        std::cout << "msg: " << msg->get_payload() << std::endl;
        
        if (command.command == "ack") {
            handle_ack(connection,command);
        } else {
            broadcast_message(msg);
        }
    }
    
    void command_error(connection_ptr connection,const std::string msg) {
        connection->send("{\"type\":\"error\",\"value\":\""+msg+"\"}");
    }
    
    // ack:e3458d0aceff8b70a3e5c0afec632881=38;e3458d0aceff8b70a3e5c0afec632881=42;
    void handle_ack(connection_ptr connection,const wscmd::cmd& command) {
        wscmd::arg_list::const_iterator arg_it;
        size_t count;
        
        for (arg_it = command.args.begin(); arg_it != command.args.end(); arg_it++) {
            if (m_msgs.find(arg_it->first) == m_msgs.end()) {
                std::cout << "ack for message we didn't send" << std::endl;
                continue;
            }
            
            count = atol(arg_it->second.c_str());
            if (count == 0) {
                continue;
            }
            
            struct msg& m(m_msgs[arg_it->first]);
            
            m.acked += count;
            
            if (m.acked == m.sent) {
                m.time = get_ms(m.time_sent);
            }
        }
    }
    
    // close: - close this connection
    // close:all; close all connections
    void close_connection(connection_ptr connection) {
        if (connection){
            connection->close(close::status::normal);
        } else {
            typename std::set<connection_ptr>::iterator it;
            
            for (it = m_connections.begin(); it != m_connections.end(); it++) {
                
                (*it)->close(close::status::normal);
            }
        }
    }
    
    void broadcast_message(message::data_ptr msg) {
        std::string hash = md5_hash_hex(msg->get_payload());
        struct msg& new_msg(m_msgs[hash]);
        
        new_msg.id = m_nextid++;
        new_msg.hash = hash;
        new_msg.size = msg->get_payload().size();
        new_msg.time_sent = boost::posix_time::microsec_clock::local_time();
        new_msg.time = 0;
        
        typename std::set<connection_ptr>::iterator it;
        
        // broadcast to clients        
        for (it = m_connections.begin(); it != m_connections.end(); it++) {
            //(*it)->send(msg->get_payload(),(msg->get_opcode() == frame::opcode::binary));
            for (int i = 0; i < 10; i++) {
                (*it)->send(msg);
            }
            
        }
        new_msg.sent = m_connections.size()*10;
        new_msg.acked = 0;
    }
    
    long get_ms(boost::posix_time::ptime s) const {
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
        boost::posix_time::time_period period(s,now);
        return period.length().total_milliseconds();
    }
    
    // hooks for admin console
    size_t get_connection_count() const {
        return m_connections.size();
    }
    
    const msg_map& get_message_stats() const {
        return m_msgs;
    }
    
    void clear_message_stats() {
        m_msgs.empty();
    }
private:
    handler_ptr     m_lobby;
    
    int             m_nextid;
    msg_map         m_msgs;
    
    std::set<connection_ptr>    m_connections;
};

} // namespace broadcast
} // namespace websocketpp

#endif // websocketpp_broadcast_handler_hpp
