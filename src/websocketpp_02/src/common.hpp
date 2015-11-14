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

#ifndef websocket_constants_hpp
#define websocket_constants_hpp

#ifndef __stdc_limit_macros
  #define __stdc_limit_macros
#endif
#include <stdint.h>

// size_max appears to be a compiler thing not an os header thing.
// make sure it is defined.
#ifndef size_max
    #define size_max ((size_t)(-1))
#endif

#ifdef _msc_ver
    #ifndef _websocketpp_cpp11_friend_
        #define _websocketpp_cpp11_friend_
    #endif
#endif

#include <exception>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

// defaults
namespace websocketpp_02 {
    static const std::string user_agent = "websocket++/0.2.1dev";

    typedef std::vector<unsigned char> binary_string;
    typedef boost::shared_ptr<binary_string> binary_string_ptr;

    typedef std::string utf8_string;
    typedef boost::shared_ptr<utf8_string> utf8_string_ptr;

    const uint64_t default_max_message_size = 0xffffff; // ~16mb

    const size_t default_read_threshold = 1; // 512 would be a more sane value for this
    const bool default_silent_close = false; // true

    const size_t max_thread_pool_size = 64;

    const uint16_t default_port = 80;
    const uint16_t default_secure_port = 443;

    inline uint16_t default_port(bool secure) {
        return (secure ? default_secure_port : default_port);
    }

    namespace session {
        namespace state {
            enum value {
                connecting = 0,
                open = 1,
                closing = 2,
                closed = 3
            };
        }
    }

    namespace close {
    namespace status {
        enum value {
            invalid_end = 999,
            normal = 1000,
            going_away = 1001,
            protocol_error = 1002,
            unsupported_data = 1003,
            rsv_adhoc_1 = 1004,
            no_status = 1005,
            abnormal_close = 1006,
            invalid_payload = 1007,
            policy_violation = 1008,
            message_too_big = 1009,
            extension_require = 1010,
            internal_endpoint_error = 1011,
            rsv_adhoc_2 = 1012,
            rsv_adhoc_3 = 1013,
            rsv_adhoc_4 = 1014,
            tls_handshake = 1015,
            rsv_start = 1016,
            rsv_end = 2999,
            invalid_start = 5000
        };

        inline bool reserved(value s) {
            return ((s >= rsv_start && s <= rsv_end) || s == rsv_adhoc_1
                    || s == rsv_adhoc_2 || s == rsv_adhoc_3 || s == rsv_adhoc_4);
        }

        // codes invalid on the wire
        inline bool invalid(value s) {
            return ((s <= invalid_end || s >= invalid_start) ||
                    s == no_status ||
                    s == abnormal_close ||
                    s == tls_handshake);
        }

        // todo functions for application ranges?
    } // namespace status
    } // namespace close

    namespace fail {
    namespace status {
        enum value {
            good = 0,           // no failure yet!
            system = 1,         // system call returned error, check that code
            websocket = 2,      // websocket close codes contain error
            unknown = 3,        // no failure information is avaliable
            timeout_tls = 4,    // tls handshake timed out
            timeout_ws = 5      // ws handshake timed out
        };
    } // namespace status
    } // namespace fail

    namespace frame {
        // opcodes are 4 bits
        // see spec section 5.2
        namespace opcode {
            enum value {
                continuation = 0x0,
                text = 0x1,
                binary = 0x2,
                rsv3 = 0x3,
                rsv4 = 0x4,
                rsv5 = 0x5,
                rsv6 = 0x6,
                rsv7 = 0x7,
                close = 0x8,
                ping = 0x9,
                pong = 0xa,
                control_rsvb = 0xb,
                control_rsvc = 0xc,
                control_rsvd = 0xd,
                control_rsve = 0xe,
                control_rsvf = 0xf
            };

            inline bool reserved(value v) {
                return (v >= rsv3 && v <= rsv7) ||
                (v >= control_rsvb && v <= control_rsvf);
            }

            inline bool invalid(value v) {
                return (v > 0xf || v < 0);
            }

            inline bool is_control(value v) {
                return v >= 0x8;
            }
        }

        namespace limits {
            static const uint8_t payload_size_basic = 125;
            static const uint16_t payload_size_extended = 0xffff; // 2^16, 65535
            static const uint64_t payload_size_jumbo = 0x7fffffffffffffffll;//2^63
        }
    } // namespace frame

    // exception class for errors that should be propogated back to the user.
    namespace error {
        enum value {
            generic = 0,
            // send attempted when endpoint write queue was full
            send_queue_full = 1,
            payload_violation = 2,
            endpoint_unsecure = 3,
            endpoint_unavailable = 4,
            invalid_uri = 5,
            no_outgoing_messages = 6,
            invalid_state = 7
        };
    }

    class exception : public std::exception {
    public:
        exception(const std::string& msg,
                  error::value code = error::generic)
        : m_msg(msg),m_code(code) {}
        ~exception() throw() {}

        virtual const char* what() const throw() {
            return m_msg.c_str();
        }

        error::value code() const throw() {
            return m_code;
        }

        std::string m_msg;
        error::value m_code;
    };
}

#endif // websocket_constants_hpp
