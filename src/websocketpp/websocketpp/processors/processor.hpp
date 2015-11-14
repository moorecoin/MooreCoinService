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

#ifndef websocketpp_processor_hpp
#define websocketpp_processor_hpp

#include <websocketpp/processors/base.hpp>
#include <websocketpp/common/system_error.hpp>

#include <websocketpp/close.hpp>
#include <websocketpp/utilities.hpp>
#include <websocketpp/uri.hpp>

#include <map>
#include <string>

namespace websocketpp {
/// processors encapsulate the protocol rules specific to each websocket version
/**
 * the processors namespace includes a number of free functions that operate on
 * various websocket related data structures and perform processing that is not
 * related to specific versions of the protocol.
 *
 * it also includes the abstract interface for the protocol specific processing
 * engines. these engines wrap all of the logic necessary for parsing and
 * validating websocket handshakes and messages of specific protocol version
 * and set of allowed extensions.
 *
 * an instance of a processor represents the state of a single websocket
 * connection of the associated version. one processor instance is needed per
 * logical websocket connection.
 */
namespace processor {

/// determine whether or not a generic http request is a websocket handshake
/**
 * @param r the http request to read.
 *
 * @return true if the request is a websocket handshake, false otherwise
 */
template <typename request_type>
bool is_websocket_handshake(request_type& r) {
    using utility::ci_find_substr;

    std::string const & upgrade_header = r.get_header("upgrade");

    if (ci_find_substr(upgrade_header, constants::upgrade_token,
        sizeof(constants::upgrade_token)-1) == upgrade_header.end())
    {
        return false;
    }

    std::string const & con_header = r.get_header("connection");

    if (ci_find_substr(con_header, constants::connection_token,
        sizeof(constants::connection_token)-1) == con_header.end())
    {
        return false;
    }

    return true;
}

/// extract the version from a websocket handshake request
/**
 * a blank version header indicates a spec before versions were introduced.
 * the only such versions in shipping products are hixie draft 75 and hixie
 * draft 76. draft 75 is present in chrome 4-5 and safari 5.0.0, draft 76 (also
 * known as hybi 00 is present in chrome 6-13 and safari 5.0.1+. as
 * differentiating between these two sets of browsers is very difficult and
 * safari 5.0.1+ accounts for the vast majority of cases in the wild this
 * function assumes that all handshakes without a valid version header are
 * hybi 00.
 *
 * @param r the websocket handshake request to read.
 *
 * @return the websocket handshake version or -1 if there was an extraction
 * error.
 */
template <typename request_type>
int get_websocket_version(request_type& r) {
    if (r.get_header("sec-websocket-version") == "") {
        return 0;
    }

    int version;
    std::istringstream ss(r.get_header("sec-websocket-version"));

    if ((ss >> version).fail()) {
        return -1;
    }

    return version;
}

/// extract a uri ptr from the host header of the request
/**
 * @param request the request to extract the host header from.
 *
 * @param scheme the scheme under which this request was received (ws, wss,
 * http, https, etc)
 *
 * @return a uri_pointer that encodes the value of the host header.
 */
template <typename request_type>
uri_ptr get_uri_from_host(request_type & request, std::string scheme) {
    std::string h = request.get_header("host");

    size_t last_colon = h.rfind(":");
    size_t last_sbrace = h.rfind("]");

    // no : = hostname with no port
    // last : before ] = ipv6 literal with no port
    // : with no ] = hostname with port
    // : after ] = ipv6 literal with port
    if (last_colon == std::string::npos ||
        (last_sbrace != std::string::npos && last_sbrace > last_colon))
    {
        return lib::make_shared<uri>(scheme, h, request.get_uri());
    } else {
        return lib::make_shared<uri>(scheme,
                               h.substr(0,last_colon),
                               h.substr(last_colon+1),
                               request.get_uri());
    }
}

/// websocket protocol processor abstract base class
template <typename config>
class processor {
public:
    typedef processor<config> type;
    typedef typename config::request_type request_type;
    typedef typename config::response_type response_type;
    typedef typename config::message_type::ptr message_ptr;
    typedef std::pair<lib::error_code,std::string> err_str_pair;

    explicit processor(bool secure, bool p_is_server)
      : m_secure(secure)
      , m_server(p_is_server)
      , m_max_message_size(config::max_message_size)
    {}

    virtual ~processor() {}

    /// get the protocol version of this processor
    virtual int get_version() const = 0;

    /// get maximum message size
    /**
     * get maximum message size. maximum message size determines the point at which the
     * processor will fail a connection with the message_too_big protocol error.
     *
     * the default is retrieved from the max_message_size value from the template config
     *
     * @since 0.3.0
     */
    size_t get_max_message_size() const {
        return m_max_message_size;
    }
    
    /// set maximum message size
    /**
     * set maximum message size. maximum message size determines the point at which the
     * processor will fail a connection with the message_too_big protocol error.
     *
     * the default is retrieved from the max_message_size value from the template config
     *
     * @since 0.3.0
     *
     * @param new_value the value to set as the maximum message size.
     */
    void set_max_message_size(size_t new_value) {
        m_max_message_size = new_value;
    }

    /// returns whether or not the permessage_compress extension is implemented
    /**
     * compile time flag that indicates whether this processor has implemented
     * the permessage_compress extension. by default this is false.
     */
    virtual bool has_permessage_compress() const {
        return false;
    }

    /// initializes extensions based on the sec-websocket-extensions header
    /**
     * reads the sec-websocket-extensions header and determines if any of the
     * requested extensions are supported by this processor. if they are their
     * settings data is initialized.
     *
     * @param request the request headers to look at.
     */
    virtual err_str_pair negotiate_extensions(request_type const &) {
        return err_str_pair();
    }

    /// validate a websocket handshake request for this version
    /**
     * @param request the websocket handshake request to validate.
     * is_websocket_handshake(request) must be true and
     * get_websocket_version(request) must equal this->get_version().
     *
     * @return a status code, 0 on success, non-zero for specific sorts of
     * failure
     */
    virtual lib::error_code validate_handshake(request_type const & request) const = 0;

    /// calculate the appropriate response for this websocket request
    /**
     * @param req the request to process
     *
     * @param subprotocol the subprotocol in use
     *
     * @param res the response to store the processed response in
     *
     * @return an error code, 0 on success, non-zero for other errors
     */
    virtual lib::error_code process_handshake(request_type const & req,
        std::string const & subprotocol, response_type& res) const = 0;

    /// fill in an http request for an outgoing connection handshake
    /**
     * @param req the request to process.
     *
     * @return an error code, 0 on success, non-zero for other errors
     */
    virtual lib::error_code client_handshake_request(request_type & req,
        uri_ptr uri, std::vector<std::string> const & subprotocols) const = 0;

    /// validate the server's response to an outgoing handshake request
    /**
     * @param req the original request sent
     * @param res the reponse to generate
     * @return an error code, 0 on success, non-zero for other errors
     */
    virtual lib::error_code validate_server_handshake_response(request_type
        const & req, response_type & res) const = 0;

    /// given a completed response, get the raw bytes to put on the wire
    virtual std::string get_raw(response_type const & request) const = 0;

    /// return the value of the header containing the cors origin.
    virtual std::string const & get_origin(request_type const & request) const = 0;

    /// extracts requested subprotocols from a handshake request
    /**
     * extracts a list of all subprotocols that the client has requested in the
     * given opening handshake request.
     *
     * @param [in] req the request to extract from
     * @param [out] subprotocol_list a reference to a vector of strings to store
     * the results in.
     */
    virtual lib::error_code extract_subprotocols(const request_type & req,
        std::vector<std::string> & subprotocol_list) = 0;

    /// extracts client uri from a handshake request
    virtual uri_ptr get_uri(request_type const & request) const = 0;

    /// process new websocket connection bytes
    /**
     * websocket connections are a continous stream of bytes that must be
     * interpreted by a protocol processor into discrete frames.
     *
     * @param buf buffer from which bytes should be read.
     * @param len length of buffer
     * @param ec reference to an error code to return any errors in
     * @return number of bytes processed
     */
    virtual size_t consume(uint8_t *buf, size_t len, lib::error_code & ec) = 0;

    /// checks if there is a message ready
    /**
     * checks if the most recent consume operation processed enough bytes to
     * complete a new websocket message. the message can be retrieved by calling
     * get_message() which will reset the internal state to not-ready and allow
     * consume to read more bytes.
     *
     * @return whether or not a message is ready.
     */
    virtual bool ready() const = 0;

    /// retrieves the most recently processed message
    /**
     * retrieves a shared pointer to the recently completed message if there is
     * one. if ready() returns true then there is a message available.
     * retrieving the message with get_message will reset the state of ready.
     * as such, each new message may be retrieved only once. calling get_message
     * when there is no message available will result in a null pointer being
     * returned.
     *
     * @return a pointer to the most recently processed message or a null shared
     *         pointer.
     */
    virtual message_ptr get_message() = 0;

    /// tests whether the processor is in a fatal error state
    virtual bool get_error() const = 0;

    /// retrieves the number of bytes presently needed by the processor
    /// this value may be used as a hint to the transport layer as to how many
    /// bytes to wait for before running consume again.
    virtual size_t get_bytes_needed() const {
        return 1;
    }

    /// prepare a data message for writing
    /**
     * performs validation, masking, compression, etc. will return an error if
     * there was an error, otherwise msg will be ready to be written
     */
    virtual lib::error_code prepare_data_frame(message_ptr in, message_ptr out) = 0;

    /// prepare a ping frame
    /**
     * ping preparation is entirely state free. there is no payload validation
     * other than length. payload need not be utf-8.
     *
     * @param in the string to use for the ping payload
     * @param out the message buffer to prepare the ping in.
     * @return status code, zero on success, non-zero on failure
     */
    virtual lib::error_code prepare_ping(std::string const & in, message_ptr out) const 
        = 0;

    /// prepare a pong frame
    /**
     * pong preparation is entirely state free. there is no payload validation
     * other than length. payload need not be utf-8.
     *
     * @param in the string to use for the pong payload
     * @param out the message buffer to prepare the pong in.
     * @return status code, zero on success, non-zero on failure
     */
    virtual lib::error_code prepare_pong(std::string const & in, message_ptr out) const 
        = 0;

    /// prepare a close frame
    /**
     * close preparation is entirely state free. the code and reason are both
     * subject to validation. reason must be valid utf-8. code must be a valid
     * un-reserved websocket close code. use close::status::no_status to
     * indicate no code. if no code is supplied a reason may not be specified.
     *
     * @param code the close code to send
     * @param reason the reason string to send
     * @param out the message buffer to prepare the fame in
     * @return status code, zero on success, non-zero on failure
     */
    virtual lib::error_code prepare_close(close::status::value code,
        std::string const & reason, message_ptr out) const = 0;
protected:
    bool const m_secure;
    bool const m_server;
    size_t m_max_message_size;
};

} // namespace processor
} // namespace websocketpp

#endif //websocketpp_processor_hpp
