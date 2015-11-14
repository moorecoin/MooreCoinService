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

#ifndef echo_server_handler_hpp
#define echo_server_handler_hpp

#include "../../src/websocketpp.hpp"
#include "../../src/interfaces/session.hpp"
#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>

using websocketpp::session::server_ptr;
using websocketpp::session::server_handler;

namespace websocketecho {

class echo_server_handler : public server_handler {
public:
    // the echo server allows all domains is protocol free.
    void validate(server_ptr session) {}
    
    // an echo server is stateless. 
    // the handler has no need to keep track of connected clients.
    void on_fail(server_ptr session) {}
    void on_open(server_ptr session) {}
    void on_close(server_ptr session) {}
    
    // both text and binary messages are echoed back to the sending client.
        void on_message(server_ptr session,websocketpp::utf8_string_ptr msg) {
        std::cout << *msg << std::endl;
        session->send(*msg);
    }
    void on_message(server_ptr session,websocketpp::binary_string_ptr data) {
        session->send(*data);
    }
};

}

#endif // echo_server_handler_hpp
