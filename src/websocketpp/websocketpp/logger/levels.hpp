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

#ifndef websocketpp_logger_levels_hpp
#define websocketpp_logger_levels_hpp

#include <websocketpp/common/stdint.hpp>

namespace websocketpp {
namespace log {

/// type of a channel package
typedef uint32_t level;

/// package of values for hinting at the nature of a given logger.
/**
 * used by the library to signal to the logging class a hint that it can use to
 * set itself up. for example, the `access` hint indicates that it is an access
 * log that might be suitable for being printed to an access log file or to cout
 * whereas `error` might be suitable for an error log file or cerr. 
 */
struct channel_type_hint {
    /// type of a channel type hint value
    typedef uint32_t value;
    
    /// no information
    static value const none = 0;
    /// access log
    static value const access = 1;
    /// error log
    static value const error = 2;
};

/// package of log levels for logging errors
struct elevel {
    /// special aggregate value representing "no levels"
    static level const none = 0x0;
    /// low level debugging information (warning: very chatty)
    static level const devel = 0x1;
    /// information about unusual system states or other minor internal library
    /// problems, less chatty than devel.
    static level const library = 0x2;
    /// information about minor configuration problems or additional information
    /// about other warnings.
    static level const info = 0x4;
    /// information about important problems not severe enough to terminate
    /// connections.
    static level const warn = 0x8;
    /// recoverable error. recovery may mean cleanly closing the connection with
    /// an appropriate error code to the remote endpoint.
    static level const rerror = 0x10;
    /// unrecoverable error. this error will trigger immediate unclean
    /// termination of the connection or endpoint.
    static level const fatal = 0x20;
    /// special aggregate value representing "all levels"
    static level const all = 0xffffffff;

    /// get the textual name of a channel given a channel id
    /**
     * the id must be that of a single channel. passing an aggregate channel
     * package results in undefined behavior.
     *
     * @param channel the channel id to look up.
     *
     * @return the name of the specified channel.
     */
    static char const * channel_name(level channel) {
        switch(channel) {
            case devel:
                return "devel";
            case library:
                return "library";
            case info:
                return "info";
            case warn:
                return "warning";
            case rerror:
                return "error";
            case fatal:
                return "fatal";
            default:
                return "unknown";
        }
    }
};

/// package of log levels for logging access events
struct alevel {
    /// special aggregate value representing "no levels"
    static level const none = 0x0;
    /// information about new connections
    /**
     * one line for each new connection that includes a host of information
     * including: the remote address, websocket version, requested resource,
     * http code, remote user agent
     */
    static level const connect = 0x1;
    /// one line for each closed connection. includes closing codes and reasons.
    static level const disconnect = 0x2;
    /// one line per control frame
    static level const control = 0x4;
    /// one line per frame, includes the full frame header
    static level const frame_header = 0x8;
    /// one line per frame, includes the full message payload (warning: chatty)
    static level const frame_payload = 0x10;
    /// reserved
    static level const message_header = 0x20;
    /// reserved
    static level const message_payload = 0x40;
    /// reserved
    static level const endpoint = 0x80;
    /// extra information about opening handshakes
    static level const debug_handshake = 0x100;
    /// extra information about closing handshakes
    static level const debug_close = 0x200;
    /// development messages (warning: very chatty)
    static level const devel = 0x400;
    /// special channel for application specific logs. not used by the library.
    static level const app = 0x800;
    /// special aggregate value representing "all levels"
    static level const all = 0xffffffff;

    /// get the textual name of a channel given a channel id
    /**
     * get the textual name of a channel given a channel id. the id must be that
     * of a single channel. passing an aggregate channel package results in
     * undefined behavior.
     *
     * @param channel the channelid to look up.
     *
     * @return the name of the specified channel.
     */
    static char const * channel_name(level channel) {
        switch(channel) {
            case connect:
                return "connect";
            case disconnect:
                return "disconnect";
            case control:
                return "control";
            case frame_header:
                return "frame_header";
            case frame_payload:
                return "frame_payload";
            case message_header:
                return "message_header";
            case message_payload:
                return "message_payload";
            case endpoint:
                return "endpoint";
            case debug_handshake:
                return "debug_handshake";
            case debug_close:
                return "debug_close";
            case devel:
                return "devel";
            case app:
                return "application";
            default:
                return "unknown";
        }
    }
};

} // logger
} // websocketpp

#endif //websocketpp_logger_levels_hpp
