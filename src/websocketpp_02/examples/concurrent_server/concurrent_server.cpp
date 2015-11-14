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

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include <cstring>
#include <sstream>

using websocketpp::server;

// a request encapsulates all of the information necesssary to perform a request
// the coordinator will fill in this information from the websocket connection 
// and add it to the processing queue. sleeping in this example is a placeholder
// for any long serial task.
struct request {
    server::handler::connection_ptr con;
    int                             value;
    
    void process() {
        std::stringstream response;
        response << "sleeping for " << value << " milliseconds!";
        con->send(response.str());
        
        boost::this_thread::sleep(boost::posix_time::milliseconds(value));
        
        response.str("");
        response << "done sleeping for " << value << " milliseconds!";
        con->send(response.str());
    }
};

// the coordinator is a simple wrapper around an stl queue. add_request inserts
// a new request. get_request returns the next available request and blocks
// (using condition variables) in the case that the queue is empty.
class request_coordinator {
public:
    void add_request(const request& r) {
        boost::unique_lock<boost::mutex> lock(m_lock);
        m_requests.push(r);
        lock.unlock();
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
private:
    std::queue<request>         m_requests;
    boost::mutex                m_lock;
    boost::condition_variable   m_cond;
};

// the websocket++ handler in this case reads numbers from connections and packs
// connection pointer + number into a request struct and passes it off to the
// coordinator.
class concurrent_server_handler : public server::handler {
public:
    concurrent_server_handler(request_coordinator& c) : m_coordinator(c) {}
    
    void on_message(connection_ptr con,message_ptr msg) {
        int value = atoi(msg->get_payload().c_str());
        
        if (value == 0) {
            con->send("invalid sleep value.");
        } else {
            request r;
            r.con = con;
            r.value = value;
            m_coordinator.add_request(r);
        }
    }
private:
    request_coordinator& m_coordinator;
};

class server_handler : public server::handler {
public:    
    void on_message(connection_ptr con,message_ptr msg) {
        int value = atoi(msg->get_payload().c_str());
        
        if (value == 0) {
            con->send("invalid sleep value.");
        } else {
            request r;
            r.con = con;
            r.value = value;
            r.process();
        }
    }
};

// process_requests is the body function for a processing thread. it loops 
// forever reading requests, processing them serially, then reading another 
// request. 
void process_requests(request_coordinator* coordinator);

void process_requests(request_coordinator* coordinator) {
    request r;
    
    while (1) {
        coordinator->get_request(r);
        
        r.process();
    }
}

// usage: <port> <thread_pool_threads> <worker_threads>
// 
// port = port to listen on
// thread_pool_threads = number of threads in the pool running io_service.run()
// worker_threads = number of threads in the sleep work pool.
//
// thread_pool_threads determines the number of threads that process i/o handlers. this 
// must be at least one. handlers and callbacks for individual connections are always 
// serially executed within that connection. an i/o thread pool will not improve 
// performance in cases where number of connections < number of threads in pool.
// 
// worker_threads=0 standard non-threaded websocket++ mode. handlers will block
//                  i/o operations within their own connection.
// worker_threads=1 a single work thread processes requests serially separate from the i/o 
//                  thread(s). this allows new connections and requests to be made while the
//                  processing thread is busy, but does allow long jobs to 
//                  monopolize the processor increasing request latency.
// worker_threads>1 multiple work threads will work on the single queue of
//                  requests provided by the i/o thread(s). this enables out of order
//                  completion of requests. the number of threads can be tuned 
//                  based on hardware concurrency available and expected load and
//                  job length.
int main(int argc, char* argv[]) {
    unsigned short port = 9002;
    size_t worker_threads = 2;
    size_t pool_threads = 2;
    
    try {
        if (argc >= 2) {
            std::stringstream buffer(argv[1]);
            buffer >> port;
        }
        
        if (argc >= 3) {
            std::stringstream buffer(argv[2]);
            buffer >> pool_threads;
        }
        
        if (argc >= 4) {
            std::stringstream buffer(argv[3]);
            buffer >> worker_threads;
        }
           
        request_coordinator rc;
        
        server::handler::ptr h;
        if (worker_threads == 0) {
            h = server::handler::ptr(new server_handler());
        } else {
            h = server::handler::ptr(new concurrent_server_handler(rc));
        }
        
        server echo_endpoint(h);
        
        echo_endpoint.alog().unset_level(websocketpp::log::alevel::all);
        echo_endpoint.elog().unset_level(websocketpp::log::elevel::all);
        
        echo_endpoint.elog().set_level(websocketpp::log::elevel::rerror);
        echo_endpoint.elog().set_level(websocketpp::log::elevel::fatal);
        
        std::list<boost::shared_ptr<boost::thread> > threads;
        if (worker_threads > 0) {
            for (size_t i = 0; i < worker_threads; i++) {
                threads.push_back(
                    boost::shared_ptr<boost::thread>(
                        new boost::thread(boost::bind(&process_requests, &rc))
                    )
                );
            }
        }
        
        std::cout << "starting websocket sleep server on port " << port 
                  << " with thread pool size " << pool_threads << " and " 
                  << worker_threads << " worker threads." << std::endl;
        echo_endpoint.listen(port,pool_threads);
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    
    return 0;
}
