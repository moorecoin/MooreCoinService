/*
 * copyright (c) 2014, peter thorson. all rights reserved.
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

#include <iostream>
#include <sstream>

// test environment:
// server, no tls, no locks, iostream based transport
#include <websocketpp/config/core.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::server<websocketpp::config::core> server;
/// note: the "server" config is being used for the client here because we don't
/// want to pull in the real rng. a better way to do this might be a custom
/// client config with the rng explicitly stubbed out.
typedef websocketpp::client<websocketpp::config::core> client;
typedef websocketpp::config::core::message_type::ptr message_ptr;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

void echo_func(server* s, websocketpp::connection_hdl hdl, message_ptr msg);
std::string run_server_test(std::string input);
std::string run_server_test(server & s, std::string input);
