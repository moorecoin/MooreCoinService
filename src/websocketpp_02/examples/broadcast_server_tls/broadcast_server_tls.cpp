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


#include "../../src/sockets/tls.hpp"
#include "../../src/websocketpp.hpp"

#include "broadcast_server_handler.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

#include <cstring>
#include <set>

#include <sys/resource.h>

//typedef websocketpp::endpoint<websocketpp::role::server,websocketpp::socket::plain> plain_endpoint_type;
//typedef plain_endpoint_type::handler_ptr plain_handler_ptr;

//typedef websocketpp::endpoint<websocketpp::role::server,websocketpp::socket::ssl> tls_endpoint_type;
//typedef tls_endpoint_type::handler_ptr tls_handler_ptr;

using websocketpp::server;
using websocketpp::server_tls;

int main(int argc, char* argv[]) {
    unsigned short port = 9002;
    bool tls = false;
    
    // 12288 is max os x limit without changing kernal settings
    const rlim_t ideal_size = 10000;
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
    }
    
    if (argc == 2) {
        // todo: input validation?
        port = atoi(argv[1]);
    }
    
    if (argc == 3) {
        // todo: input validation?
        port = atoi(argv[1]);
        tls = !strcmp(argv[2],"-tls");
    }
    
    try {
        if (tls) {
            server_tls::handler_ptr handler(new websocketpp::broadcast::server_handler<server_tls>());
            server_tls endpoint(handler);
            
            endpoint.alog().unset_level(websocketpp::log::alevel::all);
            endpoint.elog().set_level(websocketpp::log::elevel::all);
            
            std::cout << "starting secure websocket broadcast server on port " << port << std::endl;
            endpoint.listen(port);
        } else {
            server::handler_ptr handler(new websocketpp::broadcast::server_handler<server>());
            server endpoint(handler);
            
            endpoint.alog().unset_level(websocketpp::log::alevel::all);
            endpoint.elog().set_level(websocketpp::log::elevel::all);
            
            //endpoint.alog().set_level(websocketpp::log::alevel::devel);
            
            std::cout << "starting websocket broadcast server on port " << port << std::endl;
            endpoint.listen(port);
        }
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
        
    }
    
    return 0;
}
