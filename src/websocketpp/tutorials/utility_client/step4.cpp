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
 */

// **note:** this file is a snapshot of the websocket++ utility client tutorial.
// additional related material can be found in the tutorials/utility_client
// directory of the websocket++ repository.

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <sstream>

typedef websocketpp::client<websocketpp::config::asio_client> client;

class connection_metadata {
public:
    typedef websocketpp::lib::shared_ptr<connection_metadata> ptr;

    connection_metadata(int id, websocketpp::connection_hdl hdl, std::string uri)
      : m_id(id)
      , m_hdl(hdl)
      , m_status("connecting")
      , m_uri(uri)
      , m_server("n/a")
    {}

    void on_open(client * c, websocketpp::connection_hdl hdl) {
        m_status = "open";

        client::connection_ptr con = c->get_con_from_hdl(hdl);
        m_server = con->get_response_header("server");
    }

    void on_fail(client * c, websocketpp::connection_hdl hdl) {
        m_status = "failed";

        client::connection_ptr con = c->get_con_from_hdl(hdl);
        m_server = con->get_response_header("server");
        m_error_reason = con->get_ec().message();
    }

    friend std::ostream & operator<< (std::ostream & out, connection_metadata const & data);
private:
    int m_id;
    websocketpp::connection_hdl m_hdl;
    std::string m_status;
    std::string m_uri;
    std::string m_server;
    std::string m_error_reason;
};

std::ostream & operator<< (std::ostream & out, connection_metadata const & data) {
    out << "> uri: " << data.m_uri << "\n"
        << "> status: " << data.m_status << "\n"
        << "> remote server: " << (data.m_server.empty() ? "none specified" : data.m_server) << "\n"
        << "> error/close reason: " << (data.m_error_reason.empty() ? "n/a" : data.m_error_reason);

    return out;
}

class websocket_endpoint {
public:
    websocket_endpoint () : m_next_id(0) {
        m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
        m_endpoint.clear_error_channels(websocketpp::log::elevel::all);

        m_endpoint.init_asio();
        m_endpoint.start_perpetual();

        m_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(&client::run, &m_endpoint);
    }

    int connect(std::string const & uri) {
        websocketpp::lib::error_code ec;

        client::connection_ptr con = m_endpoint.get_connection(uri, ec);

        if (ec) {
            std::cout << "> connect initialization error: " << ec.message() << std::endl;
            return -1;
        }

        int new_id = m_next_id++;
        connection_metadata::ptr metadata_ptr = websocketpp::lib::make_shared<connection_metadata>(new_id, con->get_handle(), uri);
        m_connection_list[new_id] = metadata_ptr;

        con->set_open_handler(websocketpp::lib::bind(
            &connection_metadata::on_open,
            metadata_ptr,
            &m_endpoint,
            websocketpp::lib::placeholders::_1
        ));
        con->set_fail_handler(websocketpp::lib::bind(
            &connection_metadata::on_fail,
            metadata_ptr,
            &m_endpoint,
            websocketpp::lib::placeholders::_1
        ));

        m_endpoint.connect(con);

        return new_id;
    }

    connection_metadata::ptr get_metadata(int id) const {
        con_list::const_iterator metadata_it = m_connection_list.find(id);
        if (metadata_it == m_connection_list.end()) {
            return connection_metadata::ptr();
        } else {
            return metadata_it->second;
        }
    }
private:
    typedef std::map<int,connection_metadata::ptr> con_list;

    client m_endpoint;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;

    con_list m_connection_list;
    int m_next_id;
};

int main() {
    bool done = false;
    std::string input;
    websocket_endpoint endpoint;

    while (!done) {
        std::cout << "enter command: ";
        std::getline(std::cin, input);

        if (input == "quit") {
            done = true;
        } else if (input == "help") {
            std::cout
                << "\ncommand list:\n"
                << "connect <ws uri>\n"
                << "show <connection id>\n"
                << "help: display this help text\n"
                << "quit: exit the program\n"
                << std::endl;
        } else if (input.substr(0,7) == "connect") {
            int id = endpoint.connect(input.substr(8));
            if (id != -1) {
                std::cout << "> created connection with id " << id << std::endl;
            }
        } else if (input.substr(0,4) == "show") {
            int id = atoi(input.substr(5).c_str());

            connection_metadata::ptr metadata = endpoint.get_metadata(id);
            if (metadata) {
                std::cout << *metadata << std::endl;
            } else {
                std::cout << "> unknown connection id " << id << std::endl;
            }
        } else {
            std::cout << "> unrecognized command" << std::endl;
        }
    }

    return 0;
}

/*

clang++ -std=c++11 -stdlib=libc++ -i/users/zaphoyd/software/websocketpp/ -i/users/zaphoyd/software/boost_1_55_0/ -d_websocketpp_cpp11_stl_ step4.cpp /users/zaphoyd/software/boost_1_55_0/stage/lib/libboost_system.a

clang++ -i/users/zaphoyd/software/websocketpp/ -i/users/zaphoyd/software/boost_1_55_0/ step4.cpp /users/zaphoyd/software/boost_1_55_0/stage/lib/libboost_system.a /users/zaphoyd/software/boost_1_55_0/stage/lib/libboost_thread.a /users/zaphoyd/software/boost_1_55_0/stage/lib/libboost_random.a

clang++ -std=c++11 -stdlib=libc++ -i/users/zaphoyd/documents/websocketpp/ -i/users/zaphoyd/documents/boost_1_53_0_libcpp/ -d_websocketpp_cpp11_stl_ step4.cpp /users/zaphoyd/documents/boost_1_53_0_libcpp/stage/lib/libboost_system.a

*/
