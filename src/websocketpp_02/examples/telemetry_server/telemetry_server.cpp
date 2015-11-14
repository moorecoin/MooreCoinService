/*
 * copyright (c) 2012, peter thorson. all rights reserved.
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
#include <set>

#include <boost/functional.hpp>
#include <boost/bind.hpp>

using websocketpp::server;

/// thread body. counts up indefinitely, one increment per second. after each
/// it calls the handler back asking it to broadcast the new value. the handler
/// callback returns whether or not the handler would like the telemetry thread
/// to stop. if callback returns true the telemetry loop ands end the thread 
/// exits.
void generate_telemetry(boost::function<bool(const std::string&)> callback) {
    size_t value = 0;
    
    for (;;) {        
        // do some work
        value++;
        
        // broadcast state
        std::stringstream m;
        m << value;
        
        if (callback(m.str())) {
            break;
        }
        
        // wait
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
    }
}

class telemetry_server_handler : public server::handler {
public:
    typedef telemetry_server_handler type;
    typedef boost::shared_ptr<type> ptr;
    
    telemetry_server_handler() : m_done(false),m_value(0) {
        boost::function<bool(const std::string&)> callback = boost::bind(&type::on_tick,this,_1);
        
        // start a thread that will generate telemetry independently and call
        // this handler back when it has new data to send.
        m_telemetry_thread.reset(new boost::thread(
            boost::bind(
                &generate_telemetry,
                callback
            )
        ));
    }
    
    // if the handler is going away set done to true and wait for the thread
    // to exit.
    ~telemetry_server_handler() {
        {
            boost::lock_guard<boost::mutex> guard(m_mutex);
            m_done = true;
        }
        m_telemetry_thread->join();
    }
    
    /// function that we pass to the telemetry thread to broadcast the new 
    /// state. it returns the global "are we done" value so we can control when
    /// the thread stops running.
    bool on_tick(const std::string& msg) {
        boost::lock_guard<boost::mutex> guard(m_mutex);
        
        std::set<connection_ptr>::iterator it;
        
        for (it = m_connections.begin(); it != m_connections.end(); it++) {
            (*it)->send(msg);
        }
        
        return m_done;
    }
    
    // register a new client
    void on_open(connection_ptr con) {
        boost::lock_guard<boost::mutex> guard(m_mutex);
        m_connections.insert(con);
    }
    
    // remove an exiting client
    void on_close(connection_ptr con) {
        boost::lock_guard<boost::mutex> guard(m_mutex);
        m_connections.erase(con);
    }
private:
    bool                                m_done;
    size_t                              m_value;
    std::set<connection_ptr>            m_connections;
    
    boost::mutex                        m_mutex;    // guards m_connections
    boost::shared_ptr<boost::thread>    m_telemetry_thread;
};

int main(int argc, char* argv[]) {
    unsigned short port = 9007;
        
    if (argc == 2) {
        port = atoi(argv[1]);
        
        if (port == 0) {
            std::cout << "unable to parse port input " << argv[1] << std::endl;
            return 1;
        }
    }
    
    try {       
        server::handler::ptr handler(new telemetry_server_handler());
        server endpoint(handler);
                
        endpoint.alog().unset_level(websocketpp::log::alevel::all);
        endpoint.elog().unset_level(websocketpp::log::elevel::all);
        
        endpoint.alog().set_level(websocketpp::log::alevel::connect);
        endpoint.alog().set_level(websocketpp::log::alevel::disconnect);
        
        endpoint.elog().set_level(websocketpp::log::elevel::rerror);
        endpoint.elog().set_level(websocketpp::log::elevel::fatal);
        
        std::cout << "starting websocket telemetry server on port " << port << std::endl;
        endpoint.listen(port);
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    
    return 0;
}
