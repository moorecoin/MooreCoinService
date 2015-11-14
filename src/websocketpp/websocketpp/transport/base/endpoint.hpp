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

#ifndef websocketpp_transport_base_hpp
#define websocketpp_transport_base_hpp

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/common/system_error.hpp>
#include <websocketpp/uri.hpp>

#include <iostream>

namespace websocketpp {
/// transport policies provide network connectivity and timers
/**
 * ### endpoint interface
 *
 * transport endpoint components needs to provide:
 *
 * **init**\n
 * `lib::error_code init(transport_con_ptr tcon)`\n
 * init is called by an endpoint once for each newly created connection.
 * it's purpose is to give the transport policy the chance to perform any
 * transport specific initialization that couldn't be done via the default
 * constructor.
 *
 * **is_secure**\n
 * `bool is_secure() const`\n
 * test whether the transport component of this endpoint is capable of secure
 * connections.
 *
 * **async_connect**\n
 * `void async_connect(transport_con_ptr tcon, uri_ptr location,
 *  connect_handler handler)`\n
 * initiate a connection to `location` using the given connection `tcon`. `tcon`
 * is a pointer to the transport connection component of the connection. when
 * complete, `handler` should be called with the the connection's
 * `connection_hdl` and any error that occurred.
 *
 * **init_logging**
 * `void init_logging(alog_type * a, elog_type * e)`\n
 * called once after construction to provide pointers to the endpoint's access
 * and error loggers. these may be stored and used to log messages or ignored.
 */
namespace transport {

/// the type and signature of the callback passed to the accept method
typedef lib::function<void(lib::error_code const &)> accept_handler;

/// the type and signature of the callback passed to the connect method
typedef lib::function<void(lib::error_code const &)> connect_handler;

} // namespace transport
} // namespace websocketpp

#endif // websocketpp_transport_base_hpp
