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

#ifndef wsperf_stress_handler_hpp
#define wsperf_stress_handler_hpp

#include "wscmd.hpp"

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <boost/chrono.hpp>
#include <boost/thread/mutex.hpp>

#include <exception>

using websocketpp::client;

namespace wsperf {

namespace con_lifetime {
    enum value {
        fixed = 0,
        random = 1,
        unlimited = 2
    };
}



namespace msg_mode {
    enum value {
        none = 0,
        fixed = 1,
        unlimited = 2
    };
}

struct msg_data {
    typedef boost::chrono::steady_clock::time_point time_point;
    
    size_t      msg_id;
    time_point  send_time;
    time_point  recv_time;
    //size_t      payload_len;
};

struct con_data {
    typedef boost::chrono::steady_clock::time_point time_point;
    
    con_data() {}
    
    con_data(size_t id,time_point init)
      : id(id)
      , init(init)
      , start(init)
      , tcp_established(init)
      , on_open(init)
      , on_fail(init)
      , close_sent(init)
      , on_close(init)
      , status("connecting")
    {
    }
    
    std::string print() const {
        std::stringstream o;
        
        o << "{";
        o << "\"id\":" << id;
        o << ",\"status\":\"" << status << "\"";
        o << ",\"start\":" << get_rel_microseconds(start);
        o << ",\"tcp\":" << get_rel_microseconds(tcp_established);
        o << ",\"open\":" << get_rel_microseconds(on_open);
        o << ",\"fail\":" << get_rel_microseconds(on_fail);
        o << ",\"close_sent\":" << get_rel_microseconds(close_sent);
        o << ",\"close\":" << get_rel_microseconds(on_close);
                
        o << ",\"messages\":[";
        std::string sep = "";
        std::vector<msg_data>::const_iterator it;
        for (it = messages.begin(); it != messages.end(); it++) {
            o << sep << "[" 
              << get_rel_microseconds((*it).send_time) << ","
              << get_rel_microseconds((*it).recv_time)
              << "]";
            sep = ",";
        }
        
        o << "]";
        
        o << "}";
        
        return o.str();
    }
    
    double get_rel_microseconds(time_point t) const {
        boost::chrono::nanoseconds dur = t - init;
        return static_cast<double> (dur.count()) / 1000.;
    }
    
    size_t   id;
    time_point init;
    time_point start;
    time_point tcp_established;
    time_point on_open;
    time_point on_fail;
    time_point close_sent;
    time_point on_close;
    std::string status;
    std::vector<msg_data> messages;
    //stress_handler::message_ptr msg;
};



class stress_handler : public client::handler {
public:
    typedef stress_handler type;
    typedef boost::chrono::steady_clock::time_point time_point;
    typedef std::map<connection_ptr,time_point> time_map;
    
    /// construct a stress test from a wscmd command
    explicit stress_handler(wscmd::cmd& cmd);
    
    void on_connect(connection_ptr con);
    
    void on_message(connection_ptr con,websocketpp::message::data_ptr msg);
    
    void on_handshake_init(connection_ptr con);
    void on_open(connection_ptr con);
    void on_close(connection_ptr con);
    void on_fail(connection_ptr con);
    
    void start(connection_ptr con);
    void close(connection_ptr con);
    void end();
        
    std::string get_data() const;
    virtual bool maintenance();
    
    void start_message_test();
protected:
    size_t m_current_connections;
    size_t m_max_connections;
    size_t m_total_connections;
    size_t m_failed_connections;
    
    size_t m_next_con_id;
    time_point  m_init;
    
    // connection related timestamps
    std::map<connection_ptr,con_data> m_con_data;
    mutable std::list<connection_ptr> m_dirty;
    
    // stats update timer
    size_t m_timeout;
    boost::shared_ptr<boost::asio::deadline_timer> m_timer;
    
    size_t              m_next_msg_id;
    boost::shared_ptr<std::string> m_msg;
    
    // test settings pulled from the command
    con_lifetime::value m_con_lifetime;
    size_t              m_con_duration;
    bool                m_con_sync;
    
    msg_mode::value     m_msg_mode;
    size_t              m_msg_count;
    size_t              m_msg_size;
    
    
    mutable boost::mutex m_lock;
};

typedef boost::shared_ptr<stress_handler> stress_handler_ptr;

} // namespace wsperf

#endif // wsperf_stress_handler_hpp
