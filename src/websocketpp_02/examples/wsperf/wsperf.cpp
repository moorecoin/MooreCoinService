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

#include "request.hpp"

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include <cstring>
#include <sstream>
#include <fstream>

// this default will only work on unix systems.
// windows systems should set this as a compile flag to an appropriate value
#ifndef wsperf_config
    #define wsperf_config "~/.wsperf"
#endif

static const std::string user_agent = "wsperf/0.2.0dev "+websocketpp::user_agent;

using websocketpp::client;
namespace po = boost::program_options;

int start_server(po::variables_map& vm);
int start_client(po::variables_map& vm);

int start_server(po::variables_map& vm) {
    unsigned short port = vm["port"].as<unsigned short>();
    unsigned int num_threads = vm["num_threads"].as<unsigned int>();
    std::string ident = vm["ident"].as<std::string>();
    
    bool silent = (vm.count("silent") && vm["silent"].as<int>() == 1);
    
    std::list< boost::shared_ptr<boost::thread> > threads;
    
    wsperf::request_coordinator rc;
    
    server::handler::ptr h;
    h = server::handler::ptr(
        new wsperf::concurrent_handler<server>(
            rc,
            ident,
            user_agent,
            num_threads
        )
    );
    
    if (!silent) {
        std::cout << "starting wsperf server on port " << port << " with " << num_threads << " processing threads." << std::endl;
    }
    
    // start worker threads
    for (unsigned int i = 0; i < num_threads; i++) {
        threads.push_back(boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&wsperf::process_requests, &rc, i))));
    }
    
    // start websocket++
    server endpoint(h);
    
    endpoint.alog().unset_level(websocketpp::log::alevel::all);
    endpoint.elog().unset_level(websocketpp::log::elevel::all);
    
    if (!silent) {
        endpoint.alog().set_level(websocketpp::log::alevel::connect);
        endpoint.alog().set_level(websocketpp::log::alevel::disconnect);
        
        endpoint.elog().set_level(websocketpp::log::elevel::rerror);
        endpoint.elog().set_level(websocketpp::log::elevel::fatal);
    }
    
    endpoint.listen(port);
    
    return 0;
}

int start_client(po::variables_map& vm) {
    if (!vm.count("uri")) {
        std::cerr << "client mode requires uri" << std::endl;
        return 1;
    }
    
    bool silent = (vm.count("silent") && vm["silent"].as<int>() == 1);
    
    unsigned int reconnect = vm["reconnect"].as<unsigned int>();
    
    std::string uri = vm["uri"].as<std::string>();
    unsigned int num_threads = vm["num_threads"].as<unsigned int>();
    std::string ident = vm["ident"].as<std::string>();
    
    // start wsperf
    std::list< boost::shared_ptr<boost::thread> > threads;
    std::list< boost::shared_ptr<boost::thread> >::iterator thread_it;
    
    wsperf::request_coordinator rc;
    
    client::handler::ptr h;
    h = client::handler::ptr(
        new wsperf::concurrent_handler<client>(
            rc,
            ident,
            user_agent,
            num_threads
        )
    );
    
    if (!silent) {
        std::cout << "starting wsperf client connecting to " << uri << " with " << num_threads << " processing threads." << std::endl;
    }
    
    for (unsigned int i = 0; i < num_threads; i++) {
        threads.push_back(boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&wsperf::process_requests, &rc, i))));
    }
    
    while(1) {
        client endpoint(h);
        
        endpoint.alog().unset_level(websocketpp::log::alevel::all);
        endpoint.elog().unset_level(websocketpp::log::elevel::all);
        
        if (!silent) {
            endpoint.alog().set_level(websocketpp::log::alevel::connect);
            endpoint.alog().set_level(websocketpp::log::alevel::disconnect);
            
            endpoint.elog().set_level(websocketpp::log::elevel::rerror);
            endpoint.elog().set_level(websocketpp::log::elevel::fatal);
        }
        
        client::connection_ptr con = endpoint.get_connection(uri);
        
        con->add_request_header("user-agent",user_agent);
        con->add_subprotocol("wsperf");
        
        endpoint.connect(con);
        
        // this will block until there is an error or the websocket closes
        endpoint.run();
        
        rc.reset();
        
        if (!reconnect) {
            break;
        } else {
            boost::this_thread::sleep(boost::posix_time::seconds(reconnect));
        }
    }
    
    // add a "stop work" request for each outstanding worker thread
    for (thread_it = threads.begin(); thread_it != threads.end(); ++thread_it) {
        wsperf::request r;
        r.type = wsperf::end_worker;
        rc.add_request(r);
    }
    
    // wait for worker threads to finish quitting.
    for (thread_it = threads.begin(); thread_it != threads.end(); ++thread_it) {
        (*thread_it)->join();
    }
        
    return 0;
}

int main(int argc, char* argv[]) {
    try {
        // 12288 is max os x limit without changing kernal settings
        /*const rlim_t ideal_size = 10000;
        rlim_t old_size;
        rlim_t old_max;
        
        struct rlimit rl;
        int result;
        
        result = getrlimit(rlimit_nofile, &rl);
        if (result == 0) {
            //std::cout << "system fd limits: " << rl.rlim_cur << " max: " << rl.rlim_max << std::endl;
            
            old_size = rl.rlim_cur;
            old_max = rl.rlim_max;
            
            if (rl.rlim_cur < ideal_size) {
                std::cout << "attempting to raise system file descriptor limit from " << rl.rlim_cur << " to " << ideal_size << std::endl;
                rl.rlim_cur = ideal_size;
                
                if (rl.rlim_max < ideal_size) {
                    rl.rlim_max = ideal_size;
                }
                
                result = setrlimit(rlimit_nofile, &rl);
                
                if (result == 0) {
                    std::cout << "success" << std::endl;
                } else if (result == eperm) {
                    std::cout << "failed. this server will be limited to " << old_size << " concurrent connections. error code: insufficient permissions. try running process as root. system max: " << old_max << std::endl;
                } else {
                    std::cout << "failed. this server will be limited to " << old_size << " concurrent connections. error code: " << errno << " system max: " << old_max << std::endl;
                }
            }
        }*/
        
        std::string config_file;
        
        // read and process command line options
        po::options_description generic("generic");
        generic.add_options()
            ("help", "produce this help message")
            ("version,v", po::value<int>()->implicit_value(1), "print version information")
            ("config", po::value<std::string>(&config_file)->default_value(wsperf_config),
                  "configuration file to use.")
        ;
        
        po::options_description config("configuration");
        config.add_options()
            ("server,s", po::value<int>()->implicit_value(1), "run in server mode")
            ("client,c", po::value<int>()->implicit_value(1), "run in client mode")
            ("port,p", po::value<unsigned short>()->default_value(9050), "port to listen on in server mode")
            ("uri,u", po::value<std::string>(), "uri to connect to in client mode")
            ("reconnect,r", po::value<unsigned int>()->default_value(0), "auto-reconnect delay (in seconds) after a connection ends or fails in client mode. zero indicates do not reconnect.")
            ("num_threads", po::value<unsigned int>()->default_value(2), "number of worker threads to use")
            ("silent", po::value<int>()->implicit_value(1), "silent mode. will not print errors to stdout")
            ("ident,i", po::value<std::string>()->default_value("unspecified"), "implimentation identification string reported by this agent.")
        ;
        
        po::options_description cmdline_options;
        cmdline_options.add(generic).add(config);
        
        po::options_description config_file_options;
        config_file_options.add(config);
        
        po::options_description visible("allowed options");
        visible.add(generic).add(config);
        
        
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
        po::notify(vm);
        
        std::ifstream ifs(config_file.c_str());
        if (ifs) {
            store(parse_config_file(ifs, config_file_options), vm);
            notify(vm);
        }
        
        if (vm.count("help")) {
            std::cout << cmdline_options << std::endl;
            return 1;
        }
        
        if (vm.count("version")) {
            std::cout << user_agent << std::endl;
            return 1;
        }
        
        if (vm.count("server") && vm["server"].as<int>() == 1) {
            return start_server(vm);
        } else if (vm.count("client") && vm["client"].as<int>() == 1) {
            return start_client(vm);
        } else {
            std::cerr << "you must choose either client or server mode. see wsperf --help for more information" << std::endl;
            return 1;
        }
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    
    return 0;
}
