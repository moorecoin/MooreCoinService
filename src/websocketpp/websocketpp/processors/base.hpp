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

#ifndef websocketpp_processor_base_hpp
#define websocketpp_processor_base_hpp

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/system_error.hpp>

#include <websocketpp/close.hpp>
#include <websocketpp/utilities.hpp>
#include <websocketpp/uri.hpp>

#include <map>
#include <string>

namespace websocketpp {
namespace processor {

/// constants related to processing websocket connections
namespace constants {

static char const upgrade_token[] = "websocket";
static char const connection_token[] = "upgrade";
static char const handshake_guid[] = "258eafa5-e914-47da-95ca-c5ab0dc85b11";

} // namespace constants


/// processor class related error codes
namespace error_cat {
enum value {
    bad_request = 0, // error was the result of improperly formatted user input
    internal_error = 1, // error was a logic error internal to websocket++
    protocol_violation = 2,
    message_too_big = 3,
    payload_violation = 4 // error was due to receiving invalid payload data
};
} // namespace error_cat

/// error code category and codes used by all processor types
namespace error {
enum processor_errors {
    /// catch-all error for processor policy errors that don't fit in other
    /// categories
    general = 1,

    /// error was the result of improperly formatted user input
    bad_request,

    /// processor encountered a protocol violation in an incoming message
    protocol_violation,

    /// processor encountered a message that was too large
    message_too_big,

    /// processor encountered invalid payload data.
    invalid_payload,

    /// the processor method was called with invalid arguments
    invalid_arguments,

    /// opcode was invalid for requested operation
    invalid_opcode,

    /// control frame too large
    control_too_big,

    /// illegal use of reserved bit
    invalid_rsv_bit,

    /// fragmented control message
    fragmented_control,

    /// continuation without message
    invalid_continuation,

    /// clients may not send unmasked frames
    masking_required,

    /// servers may not send masked frames
    masking_forbidden,

    /// payload length not minimally encoded
    non_minimal_encoding,

    /// not supported on 32 bit systems
    requires_64bit,

    /// invalid utf-8 encoding
    invalid_utf8,

    /// operation required not implemented functionality
    not_implemented,

    /// invalid http method
    invalid_http_method,

    /// invalid http version
    invalid_http_version,

    /// invalid http status
    invalid_http_status,

    /// missing required header
    missing_required_header,

    /// embedded sha-1 library error
    sha1_library,

    /// no support for this feature in this protocol version.
    no_protocol_support,

    /// reserved close code used
    reserved_close_code,

    /// invalid close code used
    invalid_close_code,

    /// using a reason requires a close code
    reason_requires_code,

    /// error parsing subprotocols
    subprotocol_parse_error,

    /// error parsing extensions
    extension_parse_error,

    /// extension related operation was ignored because extensions are disabled
    extensions_disabled
};

/// category for processor errors
class processor_category : public lib::error_category {
public:
    processor_category() {}

    char const * name() const _websocketpp_noexcept_token_ {
        return "websocketpp.processor";
    }

    std::string message(int value) const {
        switch(value) {
            case error::general:
                return "generic processor error";
            case error::bad_request:
                return "invalid user input";
            case error::protocol_violation:
                return "generic protocol violation";
            case error::message_too_big:
                return "a message was too large";
            case error::invalid_payload:
                return "a payload contained invalid data";
            case error::invalid_arguments:
                return "invalid function arguments";
            case error::invalid_opcode:
                return "invalid opcode";
            case error::control_too_big:
                return "control messages are limited to fewer than 125 characters";
            case error::invalid_rsv_bit:
                return "invalid use of reserved bits";
            case error::fragmented_control:
                return "control messages cannot be fragmented";
            case error::invalid_continuation:
                return "invalid message continuation";
            case error::masking_required:
                return "clients may not send unmasked frames";
            case error::masking_forbidden:
                return "servers may not send masked frames";
            case error::non_minimal_encoding:
                return "payload length was not minimally encoded";
            case error::requires_64bit:
                return "64 bit frames are not supported on 32 bit systems";
            case error::invalid_utf8:
                return "invalid utf8 encoding";
            case error::not_implemented:
                return "operation required not implemented functionality";
            case error::invalid_http_method:
                return "invalid http method.";
            case error::invalid_http_version:
                return "invalid http version.";
            case error::invalid_http_status:
                return "invalid http status.";
            case error::missing_required_header:
                return "a required http header is missing";
            case error::sha1_library:
                return "sha-1 library error";
            case error::no_protocol_support:
                return "the websocket protocol version in use does not support this feature";
            case error::reserved_close_code:
                return "reserved close code used";
            case error::invalid_close_code:
                return "invalid close code used";
            case error::reason_requires_code:
                return "using a close reason requires a valid close code";
            case error::subprotocol_parse_error:
                return "error parsing subprotocol header";
            case error::extension_parse_error:
                return "error parsing extension header";
            case error::extensions_disabled:
                return "extensions are disabled";
            default:
                return "unknown";
        }
    }
};

/// get a reference to a static copy of the processor error category
inline lib::error_category const & get_processor_category() {
    static processor_category instance;
    return instance;
}

/// create an error code with the given value and the processor category
inline lib::error_code make_error_code(error::processor_errors e) {
    return lib::error_code(static_cast<int>(e), get_processor_category());
}

/// converts a processor error_code into a websocket close code
/**
 * looks up the appropriate websocket close code that should be sent after an
 * error of this sort occurred.
 *
 * if the error is not in the processor category close::status::blank is
 * returned.
 *
 * if the error isn't normally associated with reasons to close a connection
 * (such as errors intended to be used internally or delivered to client
 * applications, ex: invalid arguments) then
 * close::status::internal_endpoint_error is returned.
 */
inline close::status::value to_ws(lib::error_code ec) {
    if (ec.category() != get_processor_category()) {
        return close::status::blank;
    }

    switch (ec.value()) {
        case error::protocol_violation:
        case error::control_too_big:
        case error::invalid_opcode:
        case error::invalid_rsv_bit:
        case error::fragmented_control:
        case error::invalid_continuation:
        case error::masking_required:
        case error::masking_forbidden:
        case error::reserved_close_code:
        case error::invalid_close_code:
            return close::status::protocol_error;
        case error::invalid_payload:
        case error::invalid_utf8:
            return close::status::invalid_payload;
        case error::message_too_big:
            return close::status::message_too_big;
        default:
            return close::status::internal_endpoint_error;
    }
}

} // namespace error
} // namespace processor
} // namespace websocketpp

_websocketpp_error_code_enum_ns_start_
template<> struct is_error_code_enum<websocketpp::processor::error::processor_errors>
{
    static bool const value = true;
};
_websocketpp_error_code_enum_ns_end_

#endif //websocketpp_processor_base_hpp
