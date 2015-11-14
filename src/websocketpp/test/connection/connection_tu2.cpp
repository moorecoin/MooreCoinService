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

#include "connection_tu2.hpp"

void echo_func(server* s, websocketpp::connection_hdl hdl, message_ptr msg) {
    s->send(hdl, msg->get_payload(), msg->get_opcode());
}

std::string run_server_test(std::string input) {
    server test_server;
    return run_server_test(test_server,input);
}

std::string run_server_test(server & s, std::string input) {
    server::connection_ptr con;
    std::stringstream output;

    s.clear_access_channels(websocketpp::log::alevel::all);
    s.clear_error_channels(websocketpp::log::elevel::all);

    s.register_ostream(&output);

    con = s.get_connection();
    con->start();

    std::stringstream channel;

    channel << input;
    channel >> *con;

    return output.str();
}
