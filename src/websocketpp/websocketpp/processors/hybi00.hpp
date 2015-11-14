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

#ifndef websocketpp_processor_hybi00_hpp
#define websocketpp_processor_hybi00_hpp

#include <cstdlib>

#include <websocketpp/frame.hpp>
#include <websocketpp/utf8_validator.hpp>
#include <websocketpp/common/network.hpp>
#include <websocketpp/common/md5.hpp>
#include <websocketpp/common/platforms.hpp>

#include <websocketpp/processors/processor.hpp>

namespace websocketpp {
namespace processor {

/// processor for hybi draft version 00
/**
 * there are many differences between hybi 00 and hybi 13
 */
template <typename config>
class hybi00 : public processor<config> {
public:
    typedef processor<config> base;

    typedef typename config::request_type request_type;
    typedef typename config::response_type response_type;

    typedef typename config::message_type message_type;
    typedef typename message_type::ptr message_ptr;

    typedef typename config::con_msg_manager_type::ptr msg_manager_ptr;

    explicit hybi00(bool secure, bool p_is_server, msg_manager_ptr manager)
      : processor<config>(secure, p_is_server)
      , msg_hdr(0x00)
      , msg_ftr(0xff)
      , m_state(header)
      , m_msg_manager(manager) {}

    int get_version() const {
        return 0;
    }

    lib::error_code validate_handshake(request_type const & r) const {
        if (r.get_method() != "get") {
            return make_error_code(error::invalid_http_method);
        }

        if (r.get_version() != "http/1.1") {
            return make_error_code(error::invalid_http_version);
        }

        // required headers
        // host is required by http/1.1
        // connection is required by is_websocket_handshake
        // upgrade is required by is_websocket_handshake
        if (r.get_header("sec-websocket-key1") == "" ||
            r.get_header("sec-websocket-key2") == "" ||
            r.get_header("sec-websocket-key3") == "")
        {
            return make_error_code(error::missing_required_header);
        }

        return lib::error_code();
    }

    lib::error_code process_handshake(request_type const & req,
        std::string const & subprotocol, response_type & res) const
    {
        char key_final[16];

        // copy key1 into final key
        decode_client_key(req.get_header("sec-websocket-key1"), &key_final[0]);

        // copy key2 into final key
        decode_client_key(req.get_header("sec-websocket-key2"), &key_final[4]);

        // copy key3 into final key
        // key3 should be exactly 8 bytes. if it is more it will be truncated
        // if it is less the final key will almost certainly be wrong.
        // todo: decide if it is best to silently fail here or produce some sort
        //       of warning or exception.
        const std::string& key3 = req.get_header("sec-websocket-key3");
        std::copy(key3.c_str(),
                  key3.c_str()+(std::min)(static_cast<size_t>(8), key3.size()),
                  &key_final[8]);

        res.append_header(
            "sec-websocket-key3",
            md5::md5_hash_string(std::string(key_final,16))
        );

        res.append_header("upgrade","websocket");
        res.append_header("connection","upgrade");

        // echo back client's origin unless our local application set a
        // more restrictive one.
        if (res.get_header("sec-websocket-origin") == "") {
            res.append_header("sec-websocket-origin",req.get_header("origin"));
        }

        // echo back the client's request host unless our local application
        // set a different one.
        if (res.get_header("sec-websocket-location") == "") {
            uri_ptr uri = get_uri(req);
            res.append_header("sec-websocket-location",uri->str());
        }

        if (subprotocol != "") {
            res.replace_header("sec-websocket-protocol",subprotocol);
        }

        return lib::error_code();
    }

    /// fill in a set of request headers for a client connection request
    /**
     * the hybi 00 processor only implements incoming connections so this will
     * always return an error.
     *
     * @param [out] req  set of headers to fill in
     * @param [in] uri the uri being connected to
     * @param [in] subprotocols the list of subprotocols to request
     */
    lib::error_code client_handshake_request(request_type &, uri_ptr,
        std::vector<std::string> const &) const
    {
        return error::make_error_code(error::no_protocol_support);
    }

    /// validate the server's response to an outgoing handshake request
    /**
     * the hybi 00 processor only implements incoming connections so this will
     * always return an error.
     *
     * @param req the original request sent
     * @param res the reponse to generate
     * @return an error code, 0 on success, non-zero for other errors
     */
    lib::error_code validate_server_handshake_response(request_type const &,
        response_type &) const
    {
        return error::make_error_code(error::no_protocol_support);
    }

    std::string get_raw(response_type const & res) const {
        response_type temp = res;
        temp.remove_header("sec-websocket-key3");
        return temp.raw() + res.get_header("sec-websocket-key3");
    }

    std::string const & get_origin(request_type const & r) const {
        return r.get_header("origin");
    }

    /// extracts requested subprotocols from a handshake request
    /**
     * hybi00 doesn't support subprotocols so there never will be any requested
     *
     * @param [in] req the request to extract from
     * @param [out] subprotocol_list a reference to a vector of strings to store
     * the results in.
     */
    lib::error_code extract_subprotocols(request_type const &, 
        std::vector<std::string> &)
    {
        return lib::error_code();
    }

    uri_ptr get_uri(request_type const & request) const {
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
            return lib::make_shared<uri>(base::m_secure, h, request.get_uri());
        } else {
            return lib::make_shared<uri>(base::m_secure,
                                   h.substr(0,last_colon),
                                   h.substr(last_colon+1),
                                   request.get_uri());
        }

        // todo: check if get_uri is a full uri
    }

    /// get hybi00 handshake key3
    /**
     * @todo this doesn't appear to be used anymore. it might be able to be
     * removed
     */
    std::string get_key3() const {
        return "";
    }

    /// process new websocket connection bytes
    size_t consume(uint8_t * buf, size_t len, lib::error_code & ec) {
        // if in state header we are expecting a 0x00 byte, if we don't get one
        // it is a fatal error
        size_t p = 0; // bytes processed
        size_t l = 0;

        ec = lib::error_code();

        while (p < len) {
            if (m_state == header) {
                if (buf[p] == msg_hdr) {
                    p++;
                    m_msg_ptr = m_msg_manager->get_message(frame::opcode::text,1);

                    if (!m_msg_ptr) {
                        ec = make_error_code(websocketpp::error::no_incoming_buffers);
                        m_state = fatal_error;
                    } else {
                        m_state = payload;
                    }
                } else {
                    ec = make_error_code(error::protocol_violation);
                    m_state = fatal_error;
                }
            } else if (m_state == payload) {
                uint8_t *it = std::find(buf+p,buf+len,msg_ftr);

                // 0    1    2    3    4    5
                // 0x00 0x23 0x23 0x23 0xff 0xxx

                // copy payload bytes into message
                l = static_cast<size_t>(it-(buf+p));
                m_msg_ptr->append_payload(buf+p,l);
                p += l;

                if (it != buf+len) {
                    // message is done, copy it and the trailing
                    p++;
                    // todo: validation
                    m_state = ready;
                }
            } else {
                // todo
                break;
            }
        }
        // if we get one, we create a new message and move to application state

        // if in state application we are copying bytes into the output message
        // and validating them for utf8 until we hit a 0xff byte. once we hit
        // 0x00, the message is complete and is dispatched. then we go back to
        // header state.

        //ec = make_error_code(error::not_implemented);
        return p;
    }

    bool ready() const {
        return (m_state == ready);
    }

    bool get_error() const {
        return false;
    }

    message_ptr get_message() {
        message_ptr ret = m_msg_ptr;
        m_msg_ptr = message_ptr();
        m_state = header;
        return ret;
    }

    /// prepare a message for writing
    /**
     * performs validation, masking, compression, etc. will return an error if
     * there was an error, otherwise msg will be ready to be written
     */
    virtual lib::error_code prepare_data_frame(message_ptr in, message_ptr out)
    {
        if (!in || !out) {
            return make_error_code(error::invalid_arguments);
        }

        // todo: check if the message is prepared already

        // validate opcode
        if (in->get_opcode() != frame::opcode::text) {
            return make_error_code(error::invalid_opcode);
        }

        std::string& i = in->get_raw_payload();
        //std::string& o = out->get_raw_payload();

        // validate payload utf8
        if (!utf8_validator::validate(i)) {
            return make_error_code(error::invalid_payload);
        }

        // generate header
        out->set_header(std::string(reinterpret_cast<char const *>(&msg_hdr),1));

        // process payload
        out->set_payload(i);
        out->append_payload(std::string(reinterpret_cast<char const *>(&msg_ftr),1));

        // hybi00 doesn't support compression
        // hybi00 doesn't have masking

        out->set_prepared(true);

        return lib::error_code();
    }

    /// prepare a ping frame
    /**
     * hybi 00 doesn't support pings so this will always return an error
     *
     * @param in the string to use for the ping payload
     * @param out the message buffer to prepare the ping in.
     * @return status code, zero on success, non-zero on failure
     */
    lib::error_code prepare_ping(std::string const &, message_ptr) const
    {
        return lib::error_code(error::no_protocol_support);
    }

    /// prepare a pong frame
    /**
     * hybi 00 doesn't support pongs so this will always return an error
     *
     * @param in the string to use for the pong payload
     * @param out the message buffer to prepare the pong in.
     * @return status code, zero on success, non-zero on failure
     */
    lib::error_code prepare_pong(const std::string &, message_ptr) const
    {
        return lib::error_code(error::no_protocol_support);
    }

    /// prepare a close frame
    /**
     * hybi 00 doesn't support the close code or reason so these parameters are
     * ignored.
     *
     * @param code the close code to send
     * @param reason the reason string to send
     * @param out the message buffer to prepare the fame in
     * @return status code, zero on success, non-zero on failure
     */
    lib::error_code prepare_close(close::status::value, std::string const &, 
        message_ptr out) const
    {
        if (!out) {
            return lib::error_code(error::invalid_arguments);
        }

        std::string val;
        val.append(1,'\xff');
        val.append(1,'\x00');
        out->set_payload(val);
        out->set_prepared(true);

        return lib::error_code();
    }
private:
    void decode_client_key(std::string const & key, char * result) const {
        unsigned int spaces = 0;
        std::string digits = "";
        uint32_t num;

        // key2
        for (size_t i = 0; i < key.size(); i++) {
            if (key[i] == ' ') {
                spaces++;
            } else if (key[i] >= '0' && key[i] <= '9') {
                digits += key[i];
            }
        }

        num = static_cast<uint32_t>(strtoul(digits.c_str(), null, 10));
        if (spaces > 0 && num > 0) {
            num = htonl(num/spaces);
            std::copy(reinterpret_cast<char*>(&num),
                      reinterpret_cast<char*>(&num)+4,
                      result);
        } else {
            std::fill(result,result+4,0);
        }
    }

    enum state {
        header = 0,
        payload = 1,
        ready = 2,
        fatal_error = 3
    };

    uint8_t const msg_hdr;
    uint8_t const msg_ftr;

    state m_state;

    msg_manager_ptr m_msg_manager;
    message_ptr m_msg_ptr;
    utf8_validator::validator m_validator;
};

} // namespace processor
} // namespace websocketpp

#endif //websocketpp_processor_hybi00_hpp
