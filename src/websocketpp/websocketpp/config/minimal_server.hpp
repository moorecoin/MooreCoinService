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

#ifndef websocketpp_config_minimal_hpp
#define websocketpp_config_minimal_hpp

// non-policy common stuff
#include <websocketpp/common/platforms.hpp>
#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/stdint.hpp>

// concurrency
#include <websocketpp/concurrency/none.hpp>

// transport
#include <websocketpp/transport/stub/endpoint.hpp>

// http
#include <websocketpp/http/request.hpp>
#include <websocketpp/http/response.hpp>

// messages
#include <websocketpp/message_buffer/message.hpp>
#include <websocketpp/message_buffer/alloc.hpp>

// loggers
#include <websocketpp/logger/stub.hpp>

// rng
#include <websocketpp/random/none.hpp>

// user stub base classes
#include <websocketpp/endpoint_base.hpp>
#include <websocketpp/connection_base.hpp>

// extensions
#include <websocketpp/extensions/permessage_deflate/disabled.hpp>

namespace websocketpp {
namespace config {

/// server config with minimal dependencies
/**
 * this config strips out as many dependencies as possible. it is suitable for
 * use as a base class for custom configs that want to implement or choose their
 * own policies for components that even the core config includes.
 *
 * note: this config stubs out enough that it cannot be used directly. you must
 * supply at least a transport policy for a config based on `minimal_server` to
 * do anything useful.
 *
 * present dependency list for minimal_server config:
 *
 * c++98 stl:
 * <algorithm>
 * <map>
 * <sstream>
 * <string>
 * <vector>
 *
 * c++11 stl or boost
 * <memory>
 * <functional>
 * <system_error>
 *
 * operating system:
 * <stdint.h> or <boost/cstdint.hpp>
 * <netinet/in.h> or <winsock2.h> (for ntohl.. could potentially bundle this)
 *
 * @since 0.4.0-dev
 */
struct minimal_server {
    typedef minimal_server type;

    // concurrency policy
    typedef websocketpp::concurrency::none concurrency_type;

    // http parser policies
    typedef http::parser::request request_type;
    typedef http::parser::response response_type;

    // message policies
    typedef message_buffer::message<message_buffer::alloc::con_msg_manager>
        message_type;
    typedef message_buffer::alloc::con_msg_manager<message_type>
        con_msg_manager_type;
    typedef message_buffer::alloc::endpoint_msg_manager<con_msg_manager_type>
        endpoint_msg_manager_type;

    /// logging policies
    typedef websocketpp::log::stub<concurrency_type,
        websocketpp::log::elevel> elog_type;
    typedef websocketpp::log::stub<concurrency_type,
        websocketpp::log::alevel> alog_type;

    /// rng policies
    typedef websocketpp::random::none::int_generator<uint32_t> rng_type;

    /// controls compile time enabling/disabling of thread syncronization
    /// code disabling can provide a minor performance improvement to single
    /// threaded applications
    static bool const enable_multithreading = true;

    struct transport_config {
        typedef type::concurrency_type concurrency_type;
        typedef type::elog_type elog_type;
        typedef type::alog_type alog_type;
        typedef type::request_type request_type;
        typedef type::response_type response_type;

        /// controls compile time enabling/disabling of thread syncronization
        /// code disabling can provide a minor performance improvement to single
        /// threaded applications
        static bool const enable_multithreading = true;

        /// default timer values (in ms)

        /// length of time to wait for socket pre-initialization
        /**
         * exactly what this includes depends on the socket policy in use
         */
        static const long timeout_socket_pre_init = 5000;

        /// length of time to wait before a proxy handshake is aborted
        static const long timeout_proxy = 5000;

        /// length of time to wait for socket post-initialization
        /**
         * exactly what this includes depends on the socket policy in use.
         * often this means the tls handshake
         */
        static const long timeout_socket_post_init = 5000;

        /// length of time to wait for dns resolution
        static const long timeout_dns_resolve = 5000;

        /// length of time to wait for tcp connect
        static const long timeout_connect = 5000;

        /// length of time to wait for socket shutdown
        static const long timeout_socket_shutdown = 5000;
    };

    /// transport endpoint component
    typedef websocketpp::transport::stub::endpoint<transport_config>
        transport_type;

    /// user overridable endpoint base class
    typedef websocketpp::endpoint_base endpoint_base;
    /// user overridable connection base class
    typedef websocketpp::connection_base connection_base;

    /// default timer values (in ms)

    /// length of time before an opening handshake is aborted
    static const long timeout_open_handshake = 5000;
    /// length of time before a closing handshake is aborted
    static const long timeout_close_handshake = 5000;
    /// length of time to wait for a pong after a ping
    static const long timeout_pong = 5000;

    /// websocket protocol version to use as a client
    /**
     * what version of the websocket protocol to use for outgoing client
     * connections. setting this to a value other than 13 (rfc6455) is not
     * recommended.
     */
    static const int client_version = 13; // rfc6455

    /// default static error logging channels
    /**
     * which error logging channels to enable at compile time. channels not
     * enabled here will be unable to be selected by programs using the library.
     * this option gives an optimizing compiler the ability to remove entirely
     * code to test whether or not to print out log messages on a certain
     * channel
     *
     * default is all except for development/debug level errors
     */
    static const websocketpp::log::level elog_level =
        websocketpp::log::elevel::none;

    /// default static access logging channels
    /**
     * which access logging channels to enable at compile time. channels not
     * enabled here will be unable to be selected by programs using the library.
     * this option gives an optimizing compiler the ability to remove entirely
     * code to test whether or not to print out log messages on a certain
     * channel
     *
     * default is all except for development/debug level access messages
     */
    static const websocketpp::log::level alog_level =
        websocketpp::log::alevel::none;

    ///
    static const size_t connection_read_buffer_size = 16384;

    /// drop connections immediately on protocol error.
    /**
     * drop connections on protocol error rather than sending a close frame.
     * off by default. this may result in legit messages near the error being
     * dropped as well. it may free up resources otherwise spent dealing with
     * misbehaving clients.
     */
    static const bool drop_on_protocol_error = false;

    /// suppresses the return of detailed connection close information
    /**
     * silence close suppresses the return of detailed connection close
     * information during the closing handshake. this information is useful
     * for debugging and presenting useful errors to end users but may be
     * undesirable for security reasons in some production environments.
     * close reasons could be used by an attacker to confirm that the endpoint
     * is out of resources or be used to identify the websocket implementation
     * in use.
     *
     * note: this will suppress *all* close codes, including those explicitly
     * sent by local applications.
     */
    static const bool silent_close = false;

    /// default maximum message size
    /**
     * default value for the processor's maximum message size. maximum message size
     * determines the point at which the library will fail a connection with the 
     * message_too_big protocol error.
     *
     * the default is 32mb
     *
     * @since 0.4.0-alpha1
     */
    static const size_t max_message_size = 32000000;

    /// global flag for enabling/disabling extensions
    static const bool enable_extensions = true;

    /// extension specific settings:

    /// permessage_compress extension
    struct permessage_deflate_config {
        typedef core::request_type request_type;

        /// if the remote endpoint requests that we reset the compression
        /// context after each message should we honor the request?
        static const bool allow_disabling_context_takeover = true;

        /// if the remote endpoint requests that we reduce the size of the
        /// lz77 sliding window size this is the lowest value that will be
        /// allowed. values range from 8 to 15. a value of 8 means we will
        /// allow any possible window size. a value of 15 means do not allow
        /// negotiation of the window size (ie require the default).
        static const uint8_t minimum_outgoing_window_bits = 8;
    };

    typedef websocketpp::extensions::permessage_deflate::disabled
        <permessage_deflate_config> permessage_deflate_type;

    /// autonegotiate permessage-deflate
    /**
     * automatically enables the permessage-deflate extension.
     *
     * for clients this results in a permessage-deflate extension request being
     * sent with every request rather than requiring it to be requested manually
     *
     * for servers this results in accepting the first set of extension settings
     * requested by the client that we understand being used. the alternative is
     * requiring the extension to be manually negotiated in `validate`. with
     * auto-negotiate on, you may still override the auto-negotiate manually if
     * needed.
     */
    //static const bool autonegotiate_compression = false;
};

} // namespace config
} // namespace websocketpp

#endif // websocketpp_config_minimal_hpp
