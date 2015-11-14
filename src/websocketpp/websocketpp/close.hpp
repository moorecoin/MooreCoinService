
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

#ifndef websocketpp_close_hpp
#define websocketpp_close_hpp

/** \file
 * a package of types and methods for manipulating websocket close codes.
 */

#include <websocketpp/error.hpp>
#include <websocketpp/common/network.hpp>
#include <websocketpp/common/stdint.hpp>
#include <websocketpp/utf8_validator.hpp>

#include <string>

namespace websocketpp {
/// a package of types and methods for manipulating websocket close codes.
namespace close {
/// a package of types and methods for manipulating websocket close status'
namespace status {
    /// the type of a close code value.
    typedef uint16_t value;

    /// a blank value for internal use.
    static value const blank = 0;

    /// close the connection without a websocket close handshake.
    /**
     * this special value requests that the websocket connection be closed
     * without performing the websocket closing handshake. this does not comply
     * with rfc6455, but should be safe to do if necessary. this could be useful
     * for clients that need to disconnect quickly and cannot afford the
     * complete handshake.
     */
    static value const omit_handshake = 1;

    /// close the connection with a forced tcp drop.
    /**
     * this special value requests that the websocket connection be closed by
     * forcibly dropping the tcp connection. this will leave the other side of
     * the connection with a broken connection and some expensive timeouts. this
     * should not be done except in extreme cases or in cases of malicious
     * remote endpoints.
     */
    static value const force_tcp_drop = 2;

    /// normal closure, meaning that the purpose for which the connection was
    /// established has been fulfilled.
    static value const normal = 1000;

    /// the endpoint was "going away", such as a server going down or a browser
    /// navigating away from a page.
    static value const going_away = 1001;

    /// a protocol error occurred.
    static value const protocol_error = 1002;

    /// the connection was terminated because an endpoint received a type of
    /// data it cannot accept.
    /**
     * (e.g., an endpoint that understands only text data may send this if it
     * receives a binary message).
     */
    static value const unsupported_data = 1003;

    /// a dummy value to indicate that no status code was received.
    /**
     * this value is illegal on the wire.
     */
    static value const no_status = 1005;

    /// a dummy value to indicate that the connection was closed abnormally.
    /**
     * in such a case there was no close frame to extract a value from. this
     * value is illegal on the wire.
     */
    static value const abnormal_close = 1006;

    /// an endpoint received message data inconsistent with its type.
    /**
     * for example: invalid utf8 bytes in a text message.
     */
    static value const invalid_payload = 1007;

    /// an endpoint received a message that violated its policy.
    /**
     * this is a generic status code that can be returned when there is no other
     * more suitable status code (e.g., 1003 or 1009) or if there is a need to
     * hide specific details about the policy.
     */
    static value const policy_violation = 1008;

    /// an endpoint received a message too large to process.
    static value const message_too_big = 1009;

    /// a client expected the server to accept a required extension request
    /**
     * the list of extensions that are needed should appear in the /reason/ part
     * of the close frame. note that this status code is not used by the server,
     * because it can fail the websocket handshake instead.
     */
    static value const extension_required = 1010;

    /// an endpoint encountered an unexpected condition that prevented it from
    /// fulfilling the request.
    static value const internal_endpoint_error = 1011;

    /// indicates that the service is restarted. a client may reconnect and if
    /// if it chooses to do so, should reconnect using a randomized delay of
    /// 5-30s
    static value const service_restart = 1012;

    /// indicates that the service is experiencing overload. a client should
    /// only connect to a different ip (when there are multiple for the target)
    /// or reconnect to the same ip upon user action.
    static value const try_again_later = 1013;

    /// an endpoint failed to perform a tls handshake
    /**
     * designated for use in applications expecting a status code to indicate
     * that the connection was closed due to a failure to perform a tls
     * handshake (e.g., the server certificate can't be verified). this value is
     * illegal on the wire.
     */
    static value const tls_handshake = 1015;

    /// first value in range reserved for future protocol use
    static value const rsv_start = 1016;
    /// last value in range reserved for future protocol use
    static value const rsv_end = 2999;

    /// test whether a close code is in a reserved range
    /**
     * @param [in] code the code to test
     * @return whether or not code is reserved
     */
    inline bool reserved(value code) {
        return ((code >= rsv_start && code <= rsv_end) ||
                code == 1004 || code == 1014);
    }

    /// first value in range that is always invalid on the wire
    static value const invalid_low = 999;
    /// last value in range that is always invalid on the wire
    static value const invalid_high = 5000;

    /// test whether a close code is invalid on the wire
    /**
     * @param [in] code the code to test
     * @return whether or not code is invalid on the wire
     */
    inline bool invalid(value code) {
        return (code <= invalid_low || code >= invalid_high ||
                code == no_status || code == abnormal_close ||
                code == tls_handshake);
    }

    /// determine if the code represents an unrecoverable error
    /**
     * there is a class of errors for which once they are discovered normal
     * websocket functionality can no longer occur. this function determines
     * if a given code is one of these values. this information is used to
     * determine if the system has the capability of waiting for a close
     * acknowledgement or if it should drop the tcp connection immediately
     * after sending its close frame.
     *
     * @param [in] code the value to test.
     * @return true if the code represents an unrecoverable error
     */
    inline bool terminal(value code) {
        return (code == protocol_error || code == invalid_payload ||
                code == policy_violation || code == message_too_big ||
                 code == internal_endpoint_error);
    }
    
    /// return a human readable interpretation of a websocket close code
    /**
     * see https://tools.ietf.org/html/rfc6455#section-7.4 for more details.
     *
     * @since 0.3.0
     *
     * @param [in] code the code to look up.
     * @return a human readable interpretation of the code.
     */
    inline std::string get_string(value code) {
        switch (code) {
            case normal:
                return "normal close";
            case going_away:
                return "going away";
            case protocol_error:
                return "protocol error";
            case unsupported_data:
                return "unsupported data";
            case no_status:
                return "no status set";
            case abnormal_close:
                return "abnormal close";
            case invalid_payload:
                return "invalid payload";
            case policy_violation:
                return "policy violoation";
            case message_too_big:
                return "message too big";
            case extension_required:
                return "extension required";
            case internal_endpoint_error:
                return "internal endpoint error";
            case tls_handshake:
                return "tls handshake failure";
            default:
                return "unknown";
        }
    }
} // namespace status

/// type used to convert close statuses between integer and wire representations
union code_converter {
    uint16_t i;
    char c[2];
};

/// extract a close code value from a close payload
/**
 * if there is no close value (ie string is empty) status::no_status is
 * returned. if a code couldn't be extracted (usually do to a short or
 * otherwise mangled payload) status::protocol_error is returned and the ec
 * value is flagged as an error. note that this case is different than the case
 * where protocol error is received over the wire.
 *
 * if the value is in an invalid or reserved range ec is set accordingly.
 *
 * @param [in] payload close frame payload value received over the wire.
 * @param [out] ec set to indicate what error occurred, if any.
 * @return the extracted value
 */
inline status::value extract_code(std::string const & payload, lib::error_code
    & ec)
{
    ec = lib::error_code();

    if (payload.size() == 0) {
        return status::no_status;
    } else if (payload.size() == 1) {
        ec = make_error_code(error::bad_close_code);
        return status::protocol_error;
    }

    code_converter val;

    val.c[0] = payload[0];
    val.c[1] = payload[1];

    status::value code(ntohs(val.i));

    if (status::invalid(code)) {
        ec = make_error_code(error::invalid_close_code);
    }

    if (status::reserved(code)) {
        ec = make_error_code(error::reserved_close_code);
    }

    return code;
}

/// extract the reason string from a close payload
/**
 * the string should be a valid utf8 message. error::invalid_utf8 will be set if
 * the function extracts a reason that is not valid utf8.
 *
 * @param [in] payload the payload string to extract a reason from.
 * @param [out] ec set to indicate what error occurred, if any.
 * @return the reason string.
 */
inline std::string extract_reason(std::string const & payload, lib::error_code
    & ec)
{
    std::string reason = "";
    ec = lib::error_code();

    if (payload.size() > 2) {
        reason.append(payload.begin()+2,payload.end());
    }

    if (!websocketpp::utf8_validator::validate(reason)) {
        ec = make_error_code(error::invalid_utf8);
    }

    return reason;
}

} // namespace close
} // namespace websocketpp

#endif // websocketpp_close_hpp
