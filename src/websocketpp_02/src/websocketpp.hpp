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

#ifndef websocketpp_hpp
#define websocketpp_hpp

#include "common.hpp"
#include "endpoint.hpp"

namespace websocketpp_02 {
#ifdef websocketpp_role_server_hpp
    #ifdef websocketpp_socket_plain_hpp
        typedef websocketpp_02::endpoint<websocketpp_02::role::server,
                                      websocketpp_02::socket::plain> server;
    #endif
    #ifdef websocketpp_socket_tls_hpp
        typedef websocketpp_02::endpoint<websocketpp_02::role::server,
                                      websocketpp_02::socket::tls> server_tls;
    #endif
    #ifdef websocketpp_socket_autotls_hpp
        typedef websocketpp_02::endpoint<websocketpp_02::role::server,
                                      websocketpp_02::socket::autotls> server_autotls;
    #endif
#endif


#ifdef websocketpp_role_client_hpp
    #ifdef websocketpp_socket_plain_hpp
        typedef websocketpp_02::endpoint<websocketpp_02::role::client,
                                      websocketpp_02::socket::plain> client;
    #endif
    #ifdef websocketpp_socket_tls_hpp
        typedef websocketpp_02::endpoint<websocketpp_02::role::client,
                                      websocketpp_02::socket::tls> client_tls;
    #endif
#endif
} // namespace websocketpp_02

#endif // websocketpp_hpp
