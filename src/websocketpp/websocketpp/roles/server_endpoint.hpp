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

#ifndef websocketpp_server_endpoint_hpp
#define websocketpp_server_endpoint_hpp

#include <websocketpp/endpoint.hpp>
#include <websocketpp/logger/levels.hpp>

#include <iostream>

namespace websocketpp {


/// server endpoint role based on the given config
/**
 *
 */
template <typename config>
class server : public endpoint<connection<config>,config> {
public:
    /// type of this endpoint
    typedef server<config> type;

    /// type of the endpoint concurrency component
    typedef typename config::concurrency_type concurrency_type;
    /// type of the endpoint transport component
    typedef typename config::transport_type transport_type;

    /// type of the connections this server will create
    typedef connection<config> connection_type;
    /// type of a shared pointer to the connections this server will create
    typedef typename connection_type::ptr connection_ptr;

    /// type of the connection transport component
    typedef typename transport_type::transport_con_type transport_con_type;
    /// type of a shared pointer to the connection transport component
    typedef typename transport_con_type::ptr transport_con_ptr;

    /// type of the endpoint component of this server
    typedef endpoint<connection_type,config> endpoint_type;

    explicit server() : endpoint_type(true)
    {
        endpoint_type::m_alog.write(log::alevel::devel, "server constructor");
    }

    /// create and initialize a new connection
    /**
     * the connection will be initialized and ready to begin. call its start()
     * method to begin the processing loop.
     *
     * note: the connection must either be started or terminated using
     * connection::terminate in order to avoid memory leaks.
     *
     * @return a pointer to the new connection.
     */
    connection_ptr get_connection() {
        return endpoint_type::create_connection();
    }

    /// starts the server's async connection acceptance loop (exception free)
    /**
     * initiates the server connection acceptance loop. must be called after
     * listen. this method will have no effect until the underlying io_service
     * starts running. it may be called after the io_service is already running.
     *
     * refer to documentation for the transport policy you are using for
     * instructions on how to stop this acceptance loop.
     * 
     * @param [out] ec a status code indicating an error, if any.
     */
    void start_accept(lib::error_code & ec) {
        if (!transport_type::is_listening()) {
            ec = error::make_error_code(error::async_accept_not_listening);
            return;
        }
        
        ec = lib::error_code();
        connection_ptr con = get_connection();
        
        transport_type::async_accept(
            lib::static_pointer_cast<transport_con_type>(con),
            lib::bind(&type::handle_accept,this,con,lib::placeholders::_1),
            ec
        );
        
        if (ec && con) {
            // if the connection was constructed but the accept failed,
            // terminate the connection to prevent memory leaks
            con->terminate(lib::error_code());
        }
    }

    /// starts the server's async connection acceptance loop
    /**
     * initiates the server connection acceptance loop. must be called after
     * listen. this method will have no effect until the underlying io_service
     * starts running. it may be called after the io_service is already running.
     *
     * refer to documentation for the transport policy you are using for
     * instructions on how to stop this acceptance loop.
     */
    void start_accept() {
        lib::error_code ec;
        start_accept(ec);
        if (ec) {
            throw exception(ec);
        }
    }

    /// handler callback for start_accept
    void handle_accept(connection_ptr con, lib::error_code const & ec) {
        if (ec) {
            con->terminate(ec);

            if (ec == error::operation_canceled) {
                endpoint_type::m_elog.write(log::elevel::info,
                    "handle_accept error: "+ec.message());
            } else {
                endpoint_type::m_elog.write(log::elevel::rerror,
                    "handle_accept error: "+ec.message());
            }
        } else {
            con->start();
        }

        lib::error_code start_ec;
        start_accept(start_ec);
        if (start_ec == error::async_accept_not_listening) {
            endpoint_type::m_elog.write(log::elevel::info,
                "stopping acceptance of new connections because the underlying transport is no longer listening.");
        } else if (start_ec) {
            endpoint_type::m_elog.write(log::elevel::rerror,
                "restarting async_accept loop failed: "+ec.message());
        }
    }
};

} // namespace websocketpp

#endif //websocketpp_server_endpoint_hpp
