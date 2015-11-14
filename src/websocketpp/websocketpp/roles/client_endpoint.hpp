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

#ifndef websocketpp_client_endpoint_hpp
#define websocketpp_client_endpoint_hpp

#include <websocketpp/endpoint.hpp>
#include <websocketpp/logger/levels.hpp>

#include <iostream>

namespace websocketpp {


/// client endpoint role based on the given config
/**
 *
 */
template <typename config>
class client : public endpoint<connection<config>,config> {
public:
    /// type of this endpoint
    typedef client<config> type;

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

    explicit client() : endpoint_type(false)
    {
        endpoint_type::m_alog.write(log::alevel::devel, "client constructor");
    }

    /// get a new connection
    /**
     * creates and returns a pointer to a new connection to the given uri
     * suitable for passing to connect(connection_ptr). this method allows
     * applying connection specific settings before performing the opening
     * handshake.
     *
     * @param [in] location uri to open the connection to as a uri_ptr
     * @param [out] ec an status code indicating failure reasons, if any
     *
     * @return a connection_ptr to the new connection
     */
    connection_ptr get_connection(uri_ptr location, lib::error_code & ec) {
        if (location->get_secure() && !transport_type::is_secure()) {
            ec = error::make_error_code(error::endpoint_not_secure);
            return connection_ptr();
        }

        connection_ptr con = endpoint_type::create_connection();

        if (!con) {
            ec = error::make_error_code(error::con_creation_failed);
            return con;
        }

        con->set_uri(location);

        ec = lib::error_code();
        return con;
    }

    /// get a new connection (string version)
    /**
     * creates and returns a pointer to a new connection to the given uri
     * suitable for passing to connect(connection_ptr). this overload allows
     * default construction of the uri_ptr from a standard string.
     *
     * @param [in] u uri to open the connection to as a string
     * @param [out] ec an status code indicating failure reasons, if any
     *
     * @return a connection_ptr to the new connection
     */
    connection_ptr get_connection(std::string const & u, lib::error_code & ec) {
        uri_ptr location = lib::make_shared<uri>(u);

        if (!location->get_valid()) {
            ec = error::make_error_code(error::invalid_uri);
            return connection_ptr();
        }

        return get_connection(location, ec);
    }

    /// begin the connection process for the given connection
    /**
     * initiates the opening connection handshake for connection con. exact
     * behavior depends on the underlying transport policy.
     *
     * @param con the connection to connect
     *
     * @return the pointer to the connection originally passed in.
     */
    connection_ptr connect(connection_ptr con) {
        // ask transport to perform a connection
        transport_type::async_connect(
            lib::static_pointer_cast<transport_con_type>(con),
            con->get_uri(),
            lib::bind(
                &type::handle_connect,
                this,
                con,
                lib::placeholders::_1
            )
        );

        return con;
    }
private:
    // handle_connect
    void handle_connect(connection_ptr con, lib::error_code const & ec) {
        if (ec) {
            con->terminate(ec);

            endpoint_type::m_elog.write(log::elevel::rerror,
                    "handle_connect error: "+ec.message());
        } else {
            endpoint_type::m_alog.write(log::alevel::connect,
                "successful connection");

            con->start();
        }
    }
};

} // namespace websocketpp

#endif //websocketpp_client_endpoint_hpp
