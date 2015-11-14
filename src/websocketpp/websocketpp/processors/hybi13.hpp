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

#ifndef websocketpp_processor_hybi13_hpp
#define websocketpp_processor_hybi13_hpp

#include <cassert>

#include <websocketpp/frame.hpp>
#include <websocketpp/utf8_validator.hpp>
#include <websocketpp/common/network.hpp>
#include <websocketpp/common/platforms.hpp>
#include <websocketpp/http/constants.hpp>

#include <websocketpp/processors/processor.hpp>

#include <websocketpp/sha1/sha1.hpp>
#include <websocketpp/base64/base64.hpp>

#include <string>
#include <vector>
#include <utility>

namespace websocketpp {
namespace processor {

/// processor for hybi version 13 (rfc6455)
template <typename config>
class hybi13 : public processor<config> {
public:
    typedef processor<config> base;

    typedef typename config::request_type request_type;
    typedef typename config::response_type response_type;

    typedef typename config::message_type message_type;
    typedef typename message_type::ptr message_ptr;

    typedef typename config::con_msg_manager_type msg_manager_type;
    typedef typename msg_manager_type::ptr msg_manager_ptr;
    typedef typename config::rng_type rng_type;

    typedef typename config::permessage_deflate_type permessage_deflate_type;

    typedef std::pair<lib::error_code,std::string> err_str_pair;

    explicit hybi13(bool secure, bool p_is_server, msg_manager_ptr manager, rng_type& rng)
      : processor<config>(secure, p_is_server)
      , m_msg_manager(manager)
      , m_rng(rng)
    {
        reset_headers();
    }

    int get_version() const {
        return 13;
    }

    bool has_permessage_deflate() const {
        return m_permessage_deflate.is_implemented();
    }

    err_str_pair negotiate_extensions(request_type const & req) {
        err_str_pair ret;

        // respect blanket disabling of all extensions and don't even parse
        // the extension header
        if (!config::enable_extensions) {
            ret.first = make_error_code(error::extensions_disabled);
            return ret;
        }

        http::parameter_list p;

        bool error = req.get_header_as_plist("sec-websocket-extensions",p);

        if (error) {
            ret.first = make_error_code(error::extension_parse_error);
            return ret;
        }

        // if there are no extensions parsed then we are done!
        if (p.size() == 0) {
            return ret;
        }

        http::parameter_list::const_iterator it;

        if (m_permessage_deflate.is_implemented()) {
            err_str_pair neg_ret;
            for (it = p.begin(); it != p.end(); ++it) {
                // look through each extension, if the key is permessage-deflate
                if (it->first == "permessage-deflate") {
                    neg_ret = m_permessage_deflate.negotiate(it->second);

                    if (neg_ret.first) {
                        // figure out if this is an error that should halt all
                        // extension negotiations or simply cause negotiation of
                        // this specific extension to fail.
                        //std::cout << "permessage-compress negotiation failed: "
                        //          << neg_ret.first.message() << std::endl;
                    } else {
                        // note: this list will need commas if websocket++ ever
                        // supports more than one extension
                        ret.second += neg_ret.second;
                        continue;
                    }
                }
            }
        }

        return ret;
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
        if (r.get_header("sec-websocket-key") == "") {
            return make_error_code(error::missing_required_header);
        }

        return lib::error_code();
    }

    /* todo: the 'subprotocol' parameter may need to be expanded into a more
     * generic struct if other user input parameters to the processed handshake
     * are found.
     */
    lib::error_code process_handshake(request_type const & request, const
        std::string & subprotocol, response_type& response) const
    {
        std::string server_key = request.get_header("sec-websocket-key");

        lib::error_code ec = process_handshake_key(server_key);

        if (ec) {
            return ec;
        }

        response.replace_header("sec-websocket-accept",server_key);
        response.append_header("upgrade",constants::upgrade_token);
        response.append_header("connection",constants::connection_token);

        if (!subprotocol.empty()) {
            response.replace_header("sec-websocket-protocol",subprotocol);
        }

        return lib::error_code();
    }

    /// fill in a set of request headers for a client connection request
    /**
     * @param [out] req  set of headers to fill in
     * @param [in] uri the uri being connected to
     * @param [in] subprotocols the list of subprotocols to request
     */
    lib::error_code client_handshake_request(request_type & req, uri_ptr
        uri, std::vector<std::string> const & subprotocols) const
    {
        req.set_method("get");
        req.set_uri(uri->get_resource());
        req.set_version("http/1.1");

        req.append_header("upgrade","websocket");
        req.append_header("connection","upgrade");
        req.replace_header("sec-websocket-version","13");
        req.replace_header("host",uri->get_host_port());

        if (!subprotocols.empty()) {
            std::ostringstream result;
            std::vector<std::string>::const_iterator it = subprotocols.begin();
            result << *it++;
            while (it != subprotocols.end()) {
                result << ", " << *it++;
            }

            req.replace_header("sec-websocket-protocol",result.str());
        }

        // generate handshake key
        frame::uint32_converter conv;
        unsigned char raw_key[16];

        for (int i = 0; i < 4; i++) {
            conv.i = m_rng();
            std::copy(conv.c,conv.c+4,&raw_key[i*4]);
        }

        req.replace_header("sec-websocket-key",base64_encode(raw_key, 16));

        return lib::error_code();
    }

    /// validate the server's response to an outgoing handshake request
    /**
     * @param req the original request sent
     * @param res the reponse to generate
     * @return an error code, 0 on success, non-zero for other errors
     */
    lib::error_code validate_server_handshake_response(request_type const & req,
        response_type& res) const
    {
        // a valid response has an http 101 switching protocols code
        if (res.get_status_code() != http::status_code::switching_protocols) {
            return error::make_error_code(error::invalid_http_status);
        }

        // and the upgrade token in an upgrade header
        std::string const & upgrade_header = res.get_header("upgrade");
        if (utility::ci_find_substr(upgrade_header, constants::upgrade_token,
            sizeof(constants::upgrade_token)-1) == upgrade_header.end())
        {
            return error::make_error_code(error::missing_required_header);
        }

        // and the websocket token in the connection header
        std::string const & con_header = res.get_header("connection");
        if (utility::ci_find_substr(con_header, constants::connection_token,
            sizeof(constants::connection_token)-1) == con_header.end())
        {
            return error::make_error_code(error::missing_required_header);
        }

        // and has a valid sec-websocket-accept value
        std::string key = req.get_header("sec-websocket-key");
        lib::error_code ec = process_handshake_key(key);

        if (ec || key != res.get_header("sec-websocket-accept")) {
            return error::make_error_code(error::missing_required_header);
        }

        return lib::error_code();
    }

    std::string get_raw(response_type const & res) const {
        return res.raw();
    }

    std::string const & get_origin(request_type const & r) const {
        return r.get_header("origin");
    }

    lib::error_code extract_subprotocols(request_type const & req,
        std::vector<std::string> & subprotocol_list)
    {
        if (!req.get_header("sec-websocket-protocol").empty()) {
            http::parameter_list p;

             if (!req.get_header_as_plist("sec-websocket-protocol",p)) {
                 http::parameter_list::const_iterator it;

                 for (it = p.begin(); it != p.end(); ++it) {
                     subprotocol_list.push_back(it->first);
                 }
             } else {
                 return error::make_error_code(error::subprotocol_parse_error);
             }
        }
        return lib::error_code();
    }

    uri_ptr get_uri(request_type const & request) const {
        return get_uri_from_host(request,(base::m_secure ? "wss" : "ws"));
    }

    /// process new websocket connection bytes
    /**
     *
     * hybi 13 data streams represent a series of variable length frames. each
     * frame is made up of a series of fixed length fields. the lengths of later
     * fields are contained in earlier fields. the first field length is fixed
     * by the spec.
     *
     * this processor represents a state machine that keeps track of what field
     * is presently being read and how many more bytes are needed to complete it
     *
     *
     *
     *
     * read two header bytes
     *   extract full frame length.
     *   read extra header bytes
     * validate frame header (including extension validate)
     * read extension data into extension message state object
     * read payload data into payload
     *
     * @param buf input buffer
     *
     * @param len length of input buffer
     *
     * @return number of bytes processed or zero on error
     */
    size_t consume(uint8_t * buf, size_t len, lib::error_code & ec) {
        size_t p = 0;

        ec = lib::error_code();

        //std::cout << "consume: " << utility::to_hex(buf,len) << std::endl;

        // loop while we don't have a message ready and we still have bytes
        // left to process.
        while (m_state != ready && m_state != fatal_error &&
               (p < len || m_bytes_needed == 0))
        {
            if (m_state == header_basic) {
                p += this->copy_basic_header_bytes(buf+p,len-p);

                if (m_bytes_needed > 0) {
                    continue;
                }

                ec = this->validate_incoming_basic_header(
                    m_basic_header, base::m_server, !m_data_msg.msg_ptr
                );
                if (ec) {break;}

                // extract full header size and adjust consume state accordingly
                m_state = header_extended;
                m_cursor = 0;
                m_bytes_needed = frame::get_header_len(m_basic_header) -
                    frame::basic_header_length;
            } else if (m_state == header_extended) {
                p += this->copy_extended_header_bytes(buf+p,len-p);

                if (m_bytes_needed > 0) {
                    continue;
                }

                ec = validate_incoming_extended_header(m_basic_header,m_extended_header);
                if (ec){break;}

                m_state = application;
                m_bytes_needed = static_cast<size_t>(get_payload_size(m_basic_header,m_extended_header));

                // check if this frame is the start of a new message and set up
                // the appropriate message metadata.
                frame::opcode::value op = frame::get_opcode(m_basic_header);

                // todo: get_message failure conditions

                if (frame::opcode::is_control(op)) {
                    m_control_msg = msg_metadata(
                        m_msg_manager->get_message(op,m_bytes_needed),
                        frame::get_masking_key(m_basic_header,m_extended_header)
                    );

                    m_current_msg = &m_control_msg;
                } else {
                    if (!m_data_msg.msg_ptr) {
                        if (m_bytes_needed > base::m_max_message_size) {
                            ec = make_error_code(error::message_too_big);
                            break;
                        }
                        
                        m_data_msg = msg_metadata(
                            m_msg_manager->get_message(op,m_bytes_needed),
                            frame::get_masking_key(m_basic_header,m_extended_header)
                        );
                    } else {
                        // fetch the underlying payload buffer from the data message we
                        // are writing into.
                        std::string & out = m_data_msg.msg_ptr->get_raw_payload();
                        
                        if (out.size() + m_bytes_needed > base::m_max_message_size) {
                            ec = make_error_code(error::message_too_big);
                            break;
                        }
                        
                        // each frame starts a new masking key. all other state
                        // remains between frames.
                        m_data_msg.prepared_key = prepare_masking_key(
                            frame::get_masking_key(
                                m_basic_header,
                                m_extended_header
                            )
                        );
                        
                        out.reserve(out.size() + m_bytes_needed);
                    }
                    m_current_msg = &m_data_msg;
                }
            } else if (m_state == extension) {
                m_state = application;
            } else if (m_state == application) {
                size_t bytes_to_process = (std::min)(m_bytes_needed,len-p);

                if (bytes_to_process > 0) {
                    p += this->process_payload_bytes(buf+p,bytes_to_process,ec);

                    if (ec) {break;}
                }

                if (m_bytes_needed > 0) {
                    continue;
                }

                // if this was the last frame in the message set the ready flag.
                // otherwise, reset processor state to read additional frames.
                if (frame::get_fin(m_basic_header)) {
                    // ensure that text messages end on a valid utf8 code point
                    if (frame::get_opcode(m_basic_header) == frame::opcode::text) {
                        if (!m_current_msg->validator.complete()) {
                            ec = make_error_code(error::invalid_utf8);
                            break;
                        }
                    }

                    m_state = ready;
                } else {
                    this->reset_headers();
                }
            } else {
                // shouldn't be here
                ec = make_error_code(error::general);
                return 0;
            }
        }

        return p;
    }

    void reset_headers() {
        m_state = header_basic;
        m_bytes_needed = frame::basic_header_length;

        m_basic_header.b0 = 0x00;
        m_basic_header.b1 = 0x00;

        std::fill_n(
            m_extended_header.bytes,
            frame::max_extended_header_length,
            0x00
        );
    }

    /// test whether or not the processor has a message ready
    bool ready() const {
        return (m_state == ready);
    }

    message_ptr get_message() {
        if (!ready()) {
            return message_ptr();
        }
        message_ptr ret = m_current_msg->msg_ptr;
        m_current_msg->msg_ptr.reset();

        if (frame::opcode::is_control(ret->get_opcode())) {
            m_control_msg.msg_ptr.reset();
        } else {
            m_data_msg.msg_ptr.reset();
        }

        this->reset_headers();

        return ret;
    }

    /// test whether or not the processor is in a fatal error state.
    bool get_error() const {
        return m_state == fatal_error;
    }

    size_t get_bytes_needed() const {
        return m_bytes_needed;
    }

    /// prepare a user data message for writing
    /**
     * performs validation, masking, compression, etc. will return an error if
     * there was an error, otherwise msg will be ready to be written
     *
     * by default websocket++ performs block masking/unmasking in a manner that
     * makes assumptions about the nature of the machine and stl library used.
     * in particular the assumption is either a 32 or 64 bit word size and an
     * stl with std::string::data returning a contiguous char array.
     *
     * this method improves masking performance by 3-8x depending on the ratio
     * of small to large messages and the availability of a 64 bit processor.
     *
     * to disable this optimization (for use with alternative stl
     * implementations or processors) define websocketpp_strict_masking when
     * compiling the library. this will force the library to perform masking in
     * single byte chunks.
     *
     * todo: tests
     *
     * @param in an unprepared message to prepare
     * @param out a message to be overwritten with the prepared message
     * @return error code
     */
    virtual lib::error_code prepare_data_frame(message_ptr in, message_ptr out)
    {
        if (!in || !out) {
            return make_error_code(error::invalid_arguments);
        }

        frame::opcode::value op = in->get_opcode();

        // validate opcode: only regular data frames
        if (frame::opcode::is_control(op)) {
            return make_error_code(error::invalid_opcode);
        }

        std::string& i = in->get_raw_payload();
        std::string& o = out->get_raw_payload();

        // validate payload utf8
        if (op == frame::opcode::text && !utf8_validator::validate(i)) {
            return make_error_code(error::invalid_payload);
        }

        frame::masking_key_type key;
        bool masked = !base::m_server;
        bool compressed = m_permessage_deflate.is_enabled()
                          && in->get_compressed();
        bool fin = in->get_fin();

        // generate header
        frame::basic_header h(op,i.size(),fin,masked,compressed);

        if (masked) {
            // generate masking key.
            key.i = m_rng();

            frame::extended_header e(i.size(),key.i);
            out->set_header(frame::prepare_header(h,e));
        } else {
            frame::extended_header e(i.size());
            out->set_header(frame::prepare_header(h,e));
        }

        // prepare payload
        if (compressed) {
            // compress and store in o after header.
            m_permessage_deflate.compress(i,o);

            // mask in place if necessary
            if (masked) {
                this->masked_copy(o,o,key);
            }
        } else {
            // no compression, just copy data into the output buffer
            o.resize(i.size());

            // if we are masked, have the masking function write to the output
            // buffer directly to avoid another copy. if not masked, copy
            // directly without masking.
            if (masked) {
                this->masked_copy(i,o,key);
            } else {
                std::copy(i.begin(),i.end(),o.begin());
            }
        }

        out->set_prepared(true);

        return lib::error_code();
    }

    /// get uri
    lib::error_code prepare_ping(std::string const & in, message_ptr out) const {
        return this->prepare_control(frame::opcode::ping,in,out);
    }

    lib::error_code prepare_pong(std::string const & in, message_ptr out) const {
        return this->prepare_control(frame::opcode::pong,in,out);
    }

    virtual lib::error_code prepare_close(close::status::value code,
        std::string const & reason, message_ptr out) const
    {
        if (close::status::reserved(code)) {
            return make_error_code(error::reserved_close_code);
        }

        if (close::status::invalid(code) && code != close::status::no_status) {
            return make_error_code(error::invalid_close_code);
        }

        if (code == close::status::no_status && reason.size() > 0) {
            return make_error_code(error::reason_requires_code);
        }

        if (reason.size() > frame:: limits::payload_size_basic-2) {
            return make_error_code(error::control_too_big);
        }

        std::string payload;

        if (code != close::status::no_status) {
            close::code_converter val;
            val.i = htons(code);

            payload.resize(reason.size()+2);

            payload[0] = val.c[0];
            payload[1] = val.c[1];

            std::copy(reason.begin(),reason.end(),payload.begin()+2);
        }

        return this->prepare_control(frame::opcode::close,payload,out);
    }
protected:
    /// convert a client handshake key into a server response key in place
    lib::error_code process_handshake_key(std::string & key) const {
        key.append(constants::handshake_guid);

        unsigned char message_digest[20];
        sha1::calc(key.c_str(),key.length(),message_digest);
        key = base64_encode(message_digest,20);

        return lib::error_code();
    }

    /// reads bytes from buf into m_basic_header
    size_t copy_basic_header_bytes(uint8_t const * buf, size_t len) {
        if (len == 0 || m_bytes_needed == 0) {
            return 0;
        }

        if (len > 1) {
            // have at least two bytes
            if (m_bytes_needed == 2) {
                m_basic_header.b0 = buf[0];
                m_basic_header.b1 = buf[1];
                m_bytes_needed -= 2;
                return 2;
            } else {
                m_basic_header.b1 = buf[0];
                m_bytes_needed--;
                return 1;
            }
        } else {
            // have exactly one byte
            if (m_bytes_needed == 2) {
                m_basic_header.b0 = buf[0];
                m_bytes_needed--;
                return 1;
            } else {
                m_basic_header.b1 = buf[0];
                m_bytes_needed--;
                return 1;
            }
        }
    }

    /// reads bytes from buf into m_extended_header
    size_t copy_extended_header_bytes(uint8_t const * buf, size_t len) {
        size_t bytes_to_read = (std::min)(m_bytes_needed,len);

        std::copy(buf,buf+bytes_to_read,m_extended_header.bytes+m_cursor);
        m_cursor += bytes_to_read;
        m_bytes_needed -= bytes_to_read;

        return bytes_to_read;
    }

    /// reads bytes from buf into message payload
    /**
     * this function performs unmasking and uncompression, validates the
     * decoded bytes, and writes them to the appropriate message buffer.
     *
     * this member function will use the input buffer as stratch space for its
     * work. the raw input bytes will not be preserved. this applies only to the
     * bytes actually needed. at most min(m_bytes_needed,len) will be processed.
     *
     * @param buf input/working buffer
     * @param len length of buf
     * @return number of bytes processed or zero in case of an error
     */
    size_t process_payload_bytes(uint8_t * buf, size_t len, lib::error_code& ec)
    {
        // unmask if masked
        if (frame::get_masked(m_basic_header)) {
            #ifdef websocketpp_strict_masking
                m_current_msg->prepared_key = frame::byte_mask_circ(
                    buf,
                    len,
                    m_current_msg->prepared_key
                );
            #else
                m_current_msg->prepared_key = frame::word_mask_circ(
                    buf,
                    len,
                    m_current_msg->prepared_key
                );
            #endif
        }

        std::string & out = m_current_msg->msg_ptr->get_raw_payload();
        size_t offset = out.size();

        // decompress message if needed.
        if (m_permessage_deflate.is_enabled()
            && frame::get_rsv1(m_basic_header))
        {
            // decompress current buffer into the message buffer
            m_permessage_deflate.decompress(buf,len,out);

            // get the length of the newly uncompressed output
            offset = out.size() - offset;
        } else {
            // no compression, straight copy
            out.append(reinterpret_cast<char *>(buf),len);
        }

        // validate unmasked, decompressed values
        if (m_current_msg->msg_ptr->get_opcode() == frame::opcode::text) {
            if (!m_current_msg->validator.decode(out.begin()+offset,out.end())) {
                ec = make_error_code(error::invalid_utf8);
                return 0;
            }
        }

        m_bytes_needed -= len;

        return len;
    }

    /// validate an incoming basic header
    /**
     * validates an incoming hybi13 basic header.
     *
     * @param h the basic header to validate
     * @param is_server whether or not the endpoint that received this frame
     * is a server.
     * @param new_msg whether or not this is the first frame of the message
     * @return 0 on success or a non-zero error code on failure
     */
    lib::error_code validate_incoming_basic_header(frame::basic_header const & h,
        bool is_server, bool new_msg) const
    {
        frame::opcode::value op = frame::get_opcode(h);

        // check control frame size limit
        if (frame::opcode::is_control(op) &&
            frame::get_basic_size(h) > frame::limits::payload_size_basic)
        {
            return make_error_code(error::control_too_big);
        }

        // check that rsv bits are clear
        // the only rsv bits allowed are rsv1 if the permessage_compress
        // extension is enabled for this connection and the message is not
        // a control message.
        //
        // todo: unit tests for this
        if (frame::get_rsv1(h) && (!m_permessage_deflate.is_enabled()
                || frame::opcode::is_control(op)))
        {
            return make_error_code(error::invalid_rsv_bit);
        }

        if (frame::get_rsv2(h) || frame::get_rsv3(h)) {
            return make_error_code(error::invalid_rsv_bit);
        }

        // check for reserved opcodes
        if (frame::opcode::reserved(op)) {
            return make_error_code(error::invalid_opcode);
        }

        // check for invalid opcodes
        // todo: unit tests for this?
        if (frame::opcode::invalid(op)) {
            return make_error_code(error::invalid_opcode);
        }

        // check for fragmented control message
        if (frame::opcode::is_control(op) && !frame::get_fin(h)) {
            return make_error_code(error::fragmented_control);
        }

        // check for continuation without an active message
        if (new_msg && op == frame::opcode::continuation) {
            return make_error_code(error::invalid_continuation);
        }

        // check for new data frame when expecting continuation
        if (!new_msg && !frame::opcode::is_control(op) &&
            op != frame::opcode::continuation)
        {
            return make_error_code(error::invalid_continuation);
        }

        // servers should reject any unmasked frames from clients.
        // clients should reject any masked frames from servers.
        if (is_server && !frame::get_masked(h)) {
            return make_error_code(error::masking_required);
        } else if (!is_server && frame::get_masked(h)) {
            return make_error_code(error::masking_forbidden);
        }

        return lib::error_code();
    }

    /// validate an incoming extended header
    /**
     * validates an incoming hybi13 full header.
     *
     * @todo unit test for the >32 bit frames on 32 bit systems case
     *
     * @param h the basic header to validate
     * @param e the extended header to validate
     * @return an error_code, non-zero values indicate why the validation
     * failed
     */
    lib::error_code validate_incoming_extended_header(frame::basic_header h,
        frame::extended_header e) const
    {
        uint8_t basic_size = frame::get_basic_size(h);
        uint64_t payload_size = frame::get_payload_size(h,e);

        // check for non-minimally encoded payloads
        if (basic_size == frame::payload_size_code_16bit &&
            payload_size <= frame::limits::payload_size_basic)
        {
            return make_error_code(error::non_minimal_encoding);
        }

        if (basic_size == frame::payload_size_code_64bit &&
            payload_size <= frame::limits::payload_size_extended)
        {
            return make_error_code(error::non_minimal_encoding);
        }

        // check for >32bit frames on 32 bit systems
        if (sizeof(size_t) == 4 && (payload_size >> 32)) {
            return make_error_code(error::requires_64bit);
        }

        return lib::error_code();
    }

    /// copy and mask/unmask in one operation
    /**
     * reads input from one string and writes unmasked output to another.
     *
     * @param [in] i the input string.
     * @param [out] o the output string.
     * @param [in] key the masking key to use for masking/unmasking
     */
    void masked_copy (std::string const & i, std::string & o,
        frame::masking_key_type key) const
    {
        #ifdef websocketpp_strict_masking
            frame::byte_mask(i.begin(),i.end(),o.begin(),key);
        #else
            websocketpp::frame::word_mask_exact(
                reinterpret_cast<uint8_t *>(const_cast<char *>(i.data())),
                reinterpret_cast<uint8_t *>(const_cast<char *>(o.data())),
                i.size(),
                key
            );
        #endif
    }

    /// generic prepare control frame with opcode and payload.
    /**
     * internal control frame building method. handles validation, masking, etc
     *
     * @param op the control opcode to use
     * @param payload the payload to use
     * @param out the message buffer to store the prepared frame in
     * @return status code, zero on success, non-zero on error
     */
    lib::error_code prepare_control(frame::opcode::value op,
        std::string const & payload, message_ptr out) const
    {
        if (!out) {
            return make_error_code(error::invalid_arguments);
        }

        if (!frame::opcode::is_control(op)) {
            return make_error_code(error::invalid_opcode);
        }

        if (payload.size() > frame::limits::payload_size_basic) {
            return make_error_code(error::control_too_big);
        }

        frame::masking_key_type key;
        bool masked = !base::m_server;

        frame::basic_header h(op,payload.size(),true,masked);

        std::string & o = out->get_raw_payload();
        o.resize(payload.size());

        if (masked) {
            // generate masking key.
            key.i = m_rng();

            frame::extended_header e(payload.size(),key.i);
            out->set_header(frame::prepare_header(h,e));
            this->masked_copy(payload,o,key);
        } else {
            frame::extended_header e(payload.size());
            out->set_header(frame::prepare_header(h,e));
            std::copy(payload.begin(),payload.end(),o.begin());
        }

        out->set_prepared(true);

        return lib::error_code();
    }

    enum state {
        header_basic = 0,
        header_extended = 1,
        extension = 2,
        application = 3,
        ready = 4,
        fatal_error = 5
    };

    /// this data structure holds data related to processing a message, such as
    /// the buffer it is being written to, its masking key, its utf8 validation
    /// state, and sometimes its compression state.
    struct msg_metadata {
        msg_metadata() {}
        msg_metadata(message_ptr m, size_t p) : msg_ptr(m),prepared_key(p) {}
        msg_metadata(message_ptr m, frame::masking_key_type p)
          : msg_ptr(m)
          , prepared_key(prepare_masking_key(p)) {}

        message_ptr msg_ptr;        // pointer to the message data buffer
        size_t      prepared_key;   // prepared masking key
        utf8_validator::validator validator; // utf8 validation state
    };

    // basic header of the frame being read
    frame::basic_header m_basic_header;

    // pointer to a manager that can create message buffers for us.
    msg_manager_ptr m_msg_manager;

    // number of bytes needed to complete the current operation
    size_t m_bytes_needed;

    // number of extended header bytes read
    size_t m_cursor;

    // metadata for the current data msg
    msg_metadata m_data_msg;
    // metadata for the current control msg
    msg_metadata m_control_msg;

    // pointer to the metadata associated with the frame being read
    msg_metadata * m_current_msg;

    // extended header of current frame
    frame::extended_header m_extended_header;

    rng_type & m_rng;

    // overall state of the processor
    state m_state;

    // extensions
    permessage_deflate_type m_permessage_deflate;
};

} // namespace processor
} // namespace websocketpp

#endif //websocketpp_processor_hybi13_hpp
