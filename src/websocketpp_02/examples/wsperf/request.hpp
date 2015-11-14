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

#ifndef wsperf_request_hpp
#define wsperf_request_hpp

#include "case.hpp"
#include "generic.hpp"
#include "wscmd.hpp"

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

using websocketpp::client;
using websocketpp::server;

namespace wsperf {

enum request_type {
    perf_test = 0,
    end_worker = 1
};

class writer {
public:
	virtual ~writer() {}
	
    virtual void write(std::string msg) = 0;
};

typedef boost::shared_ptr<writer> writer_ptr;

template <typename endpoint_type>
class ws_writer : public writer {
public:
    ws_writer(typename endpoint_type::handler::connection_ptr con) 
     : m_con(con) {}
    
    void write(std::string msg) {
        m_con->send(msg);
    }
private:
    typename endpoint_type::handler::connection_ptr m_con;
};

// a request encapsulates all of the information necesssary to perform a request
// the coordinator will fill in this information from the websocket connection 
// and add it to the processing queue. sleeping in this example is a placeholder
// for any long serial task.
struct request {
    writer_ptr  writer;
    
    request_type type;
    std::string req;                // the raw request
    std::string token;              // parsed test token. return in all results    
    
    /// run a test and return json result
    void process(unsigned int id);
    
    // simple json generation
    std::string prepare_response(std::string type,std::string data);
    std::string prepare_response_object(std::string type,std::string data);
};

// the coordinator is a simple wrapper around an stl queue. add_request inserts
// a new request. get_request returns the next available request and blocks
// (using condition variables) in the case that the queue is empty.
class request_coordinator {
public:
    void add_request(const request& r) {
        {
            boost::unique_lock<boost::mutex> lock(m_lock);
            m_requests.push(r);
        }
        m_cond.notify_one();
    }
    
    void get_request(request& value) {
        boost::unique_lock<boost::mutex> lock(m_lock);
        
        while (m_requests.empty()) {
            m_cond.wait(lock);
        }
        
        value = m_requests.front();
        m_requests.pop();
    }
    
    void reset() {
        boost::unique_lock<boost::mutex> lock(m_lock);
        
        while (!m_requests.empty()) {
            m_requests.pop();
        }
    }
private:
    std::queue<request>         m_requests;
    boost::mutex                m_lock;
    boost::condition_variable   m_cond;
};

/// handler that reads requests off the wire and dispatches them to a request queue
template <typename endpoint_type>
class concurrent_handler : public endpoint_type::handler {
public:
    typedef typename endpoint_type::handler::connection_ptr connection_ptr;
    typedef typename endpoint_type::handler::message_ptr message_ptr;
    
    concurrent_handler(request_coordinator& c,
                       std::string ident, 
                       std::string ua,
                       unsigned int num_workers)
     : m_coordinator(c), 
       m_ident(ident), 
       m_ua(ua),
       m_num_workers(num_workers),
       m_blocking(num_workers == 0) {}
    
    void on_open(connection_ptr con) {
        std::stringstream o;
        
        o << "{"
          << "\"type\":\"test_welcome\","
          << "\"version\":\"" << m_ua << "\","
          << "\"ident\":\"" << m_ident << "\","
          << "\"num_workers\":" << m_num_workers 
          << "}";
        
        con->send(o.str());
    }
    
    void on_message(connection_ptr con, message_ptr msg) {
        request r;
        r.type = perf_test;
        r.writer = writer_ptr(new ws_writer<endpoint_type>(con));
        r.req = msg->get_payload();
        
        if (m_blocking) {
            r.process(0);
        } else {
            m_coordinator.add_request(r);
        }
    }
    
    void on_fail(connection_ptr con) {
        std::cout << "a command connection failed." << std::endl;
    }
    
    void on_close(connection_ptr con) {
        std::cout << "a command connection closed." << std::endl;
    }
private:
    request_coordinator&    m_coordinator;
    std::string             m_ident;
    std::string             m_ua;
    unsigned int            m_num_workers;
    bool                    m_blocking;
};

// process_requests is the body function for a processing thread. it loops 
// forever reading requests, processing them serially, then reading another 
// request. a request with type end_worker will stop the processing loop.
void process_requests(request_coordinator* coordinator, unsigned int id);

} // namespace wsperf

#endif // wsperf_request_hpp
