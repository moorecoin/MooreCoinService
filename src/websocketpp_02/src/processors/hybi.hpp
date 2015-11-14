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

#ifndef websocket_processor_hybi_hpp
#define websocket_processor_hybi_hpp

#include "processor.hpp"
#include "hybi_header.hpp"

#include "../base64/base64.h"
#include "../sha1/sha1.h"

#include <boost/algorithm/string.hpp>

#ifdef ignore
	#undef ignore
#endif // #ifdef ignore

#ifdef min
	#undef min
#endif // #ifdef min

namespace websocketpp_02 {
namespace processor {

namespace hybi_state {
    enum value {
        read_header = 0,
        read_payload = 1,
        ready = 2,
        ignore = 3
    };
}

/*

client case
end user asks connection for next message
- connection returns the next avaliable message or throws if none are ready
- connection resets the message and fills in a new masking key
end user calls set payload with const ref string to final payload
- set opcode... argument to set payload?
- set payload checks utf8 if copies from source, masks, and stores in payload.


prepare (hybi):
- writes header (writes opcode, payload length, masking key, fin bit, mask bit)


server case
end user asks connection for next message
- connection returns the next avaliable message or throws if none are ready
- connection resets the message and sets masking to off.
end user calls set payload with const ref string to final payload
- std::copy msg to payload

prepare
- writes header (writes opcode, payload length, fin bit, mask bit)


int reference_count
std::list< std::pair< std::string,std::string > >


*/

/*class hybi_message {
public:
    hybi_message(frame::opcode::value opcode) : m_processed(false) {

    }

    void reset() {

    }
private:
    bool        m_processed;
    std::string m_header;
    std::string m_payload;
};*/

// connection must provide:
// int32_t get_rng();
// message::data_ptr get_data_message();
// message::control_ptr get_control_message();
// bool is_secure();

template <class connection_type>
class hybi : public processor_base {
public:
    hybi(connection_type &connection)
     : m_connection(connection),
       m_write_frame(connection)
    {
        reset();
    }

    void validate_handshake(const http::parser::request& request) const {
        std::stringstream err;
        std::string h;

        if (request.method() != "get") {
            err << "websocket handshake has invalid method: "
            << request.method();

            throw(http::exception(err.str(),http::status_code::bad_request));
        }

        // todo: allow versions greater than 1.1
        if (request.version() != "http/1.1") {
            err << "websocket handshake has invalid http version: "
            << request.method();

            throw(http::exception(err.str(),http::status_code::bad_request));
        }

        // verify the presence of required headers
        if (request.header("host") == "") {
            throw(http::exception("required host header is missing",http::status_code::bad_request));
        }

        h = request.header("upgrade");
        if (h == "") {
            throw(http::exception("required upgrade header is missing",http::status_code::bad_request));
        } else if (!boost::ifind_first(h,"websocket")) {
            err << "upgrade header \"" << h << "\", does not contain required token \"websocket\"";
            throw(http::exception(err.str(),http::status_code::bad_request));
        }

        h = request.header("connection");
        if (h == "") {
            throw(http::exception("required connection header is missing",http::status_code::bad_request));
        } else if (!boost::ifind_first(h,"upgrade")) {
            err << "connection header, \"" << h
            << "\", does not contain required token \"upgrade\"";
            throw(http::exception(err.str(),http::status_code::bad_request));
        }

        if (request.header("sec-websocket-key") == "") {
            throw(http::exception("required sec-websocket-key header is missing",http::status_code::bad_request));
        }

        h = request.header("sec-websocket-version");
        if (h == "") {
            throw(http::exception("required sec-websocket-version header is missing",http::status_code::bad_request));
        } else {
            int version = atoi(h.c_str());

            if (version != 7 && version != 8 && version != 13) {
                err << "this processor doesn't support websocket protocol version "
                << version;
                throw(http::exception(err.str(),http::status_code::bad_request));
            }
        }
    }

    std::string get_origin(const http::parser::request& request) const {
        std::string h = request.header("sec-websocket-version");
        int version = atoi(h.c_str());

        if (version == 13) {
            return request.header("origin");
        } else if (version == 7 || version == 8) {
            return request.header("sec-websocket-origin");
        } else {
            throw(http::exception("could not determine origin header. check sec-websocket-version header",http::status_code::bad_request));
        }
    }

    uri_ptr get_uri(const http::parser::request& request) const {
        std::string h = request.header("host");

        size_t last_colon = h.rfind(":");
        size_t last_sbrace = h.rfind("]");

        // no : = hostname with no port
        // last : before ] = ipv6 literal with no port
        // : with no ] = hostname with port
        // : after ] = ipv6 literal with port
        if (last_colon == std::string::npos ||
            (last_sbrace != std::string::npos && last_sbrace > last_colon))
        {
            return uri_ptr(new uri(m_connection.is_secure(),h,request.uri()));
        } else {
            return uri_ptr(new uri(m_connection.is_secure(),
                                   h.substr(0,last_colon),
                                   h.substr(last_colon+1),
                                   request.uri()));
        }

        // todo: check if get_uri is a full uri
    }

    void handshake_response(const http::parser::request& request,
                            http::parser::response& response)
    {
        std::string server_key = request.header("sec-websocket-key");
        server_key += "258eafa5-e914-47da-95ca-c5ab0dc85b11";

        sha1        sha;
        uint32_t    message_digest[5];

        sha.reset();
        sha << server_key.c_str();

        if (sha.result(message_digest)){
            // convert sha1 hash bytes to network byte order because this sha1
            //  library works on ints rather than bytes
            for (int i = 0; i < 5; i++) {
                message_digest[i] = htonl(message_digest[i]);
            }

            server_key = base64_encode(
                                       reinterpret_cast<const unsigned char*>
                                       (message_digest),20
                                       );

            // set handshake accept headers
            response.replace_header("sec-websocket-accept",server_key);
            response.add_header("upgrade","websocket");
            response.add_header("connection","upgrade");
        } else {
            //m_endpoint->elog().at(log::elevel::rerror)
            //<< "error computing handshake sha1 hash" << log::endl;
            // todo: make sure this error path works
            response.set_status(http::status_code::internal_server_error);
        }
    }

    void consume(std::istream& s) {
        while (s.good() && m_state != hybi_state::ready) {
            try {
                switch (m_state) {
                    case hybi_state::read_header:
                        process_header(s);
                        break;
                    case hybi_state::read_payload:
                        process_payload(s);
                        break;
                    case hybi_state::ready:
                        // shouldn't be here..
                        break;
                    case hybi_state::ignore:
                        s.ignore(m_payload_left);
                        m_payload_left -= static_cast<size_t>(s.gcount());

                        if (m_payload_left == 0) {
                            reset();
                        }

                        break;
                    default:
                        break;
                }
            } catch (const processor::exception& e) {
                if (e.code() != processor::error::out_of_messages) {
                    // the out of messages exception acts as an inturrupt rather
                    // than an error. in that case we don't want to reset
                    // processor state. in all other cases we are aborting
                    // processing of the message in flight and want to reset the
                    // processor for a new message.
                    if (m_header.ready()) {
                        m_header.reset();
                        ignore();
                    }
                }

                throw e;
            }
        }
    }

    // sends the processor an inturrupt signal instructing it to ignore the next
    // num bytes and then reset itself. this is used to flush a bad frame out of
    // the read buffer.
    void ignore() {
        m_state = hybi_state::ignore;
    }

    void process_header(std::istream& s) {
        m_header.consume(s);

        if (m_header.ready()) {
            // get a free message from the read queue for the type of the
            // current message
            if (m_header.is_control()) {
                process_control_header();
            } else {
                process_data_header();
            }
        }
    }

    void process_control_header() {
        m_control_message = m_connection.get_control_message();

        if (!m_control_message) {
            throw processor::exception("out of control messages",
                                       processor::error::out_of_messages);
        }

        m_control_message->reset(m_header.get_opcode(),m_header.get_masking_key());

        m_payload_left = static_cast<size_t>(m_header.get_payload_size());

        if (m_payload_left == 0) {
            process_frame();
        } else {
            m_state = hybi_state::read_payload;
        }
    }

    void process_data_header() {
        if (!m_data_message) {
            // this is a new message. no continuation frames allowed.
            if (m_header.get_opcode() == frame::opcode::continuation) {
                throw processor::exception("received continuation frame without an outstanding message.",processor::error::protocol_violation);
            }

            m_data_message = m_connection.get_data_message();

            if (!m_data_message) {
                throw processor::exception("out of data messages",
                                           processor::error::out_of_messages);
            }

            m_data_message->reset(m_header.get_opcode());
        } else {
            // a message has already been started. continuation frames only!
            if (m_header.get_opcode() != frame::opcode::continuation) {
                throw processor::exception("received new message before the completion of the existing one.",processor::error::protocol_violation);
            }
        }

        m_payload_left = static_cast<size_t>(m_header.get_payload_size());

        if (m_payload_left == 0) {
            process_frame();
        } else {
            // each frame has a new masking key
            m_data_message->set_masking_key(m_header.get_masking_key());
            m_state = hybi_state::read_payload;
        }
    }

    void process_payload(std::istream& input) {
        //std::cout << "payload left 1: " << m_payload_left << std::endl;
        size_t num;

        // read bytes into processor buffer. read the lesser of the buffer size
        // and the number of bytes left in the payload.

        input.read(m_payload_buffer, std::min(m_payload_left, payload_buffer_size));
        num = static_cast<size_t>(input.gcount());

        if (input.bad()) {
            throw processor::exception("istream readsome error",
                                       processor::error::fatal_error);
        }

        if (num == 0) {
            return;
        }

        m_payload_left -= num;

        // tell the appropriate message to process the bytes.
        if (m_header.is_control()) {
            m_control_message->process_payload(m_payload_buffer,num);
        } else {
            //m_connection.alog().at(log::alevel::devel) << "process_payload. size:  " << m_payload_left << log::endl;
            m_data_message->process_payload(m_payload_buffer,num);
        }

        if (m_payload_left == 0) {
            process_frame();
        }

    }

    void process_frame() {
        if (m_header.get_fin()) {
            if (m_header.is_control()) {
                m_control_message->complete();
            } else {
                m_data_message->complete();
            }
            m_state = hybi_state::ready;
        } else {
            reset();
        }
    }

    bool ready() const {
        return m_state == hybi_state::ready;
    }

    bool is_control() const {
        return m_header.is_control();
    }

    // note this can only be called once
    message::data_ptr get_data_message() {
        message::data_ptr p = m_data_message;
        m_data_message.reset();
        return p;
    }

    // note this can only be called once
    message::control_ptr get_control_message() {
        message::control_ptr p = m_control_message;
        m_control_message.reset();
        return p;
    }

    void reset() {
        m_state = hybi_state::read_header;
        m_header.reset();
    }

    uint64_t get_bytes_needed() const {
        switch (m_state) {
            case hybi_state::read_header:
                return m_header.get_bytes_needed();
            case hybi_state::read_payload:
            case hybi_state::ignore:
                return m_payload_left;
            case hybi_state::ready:
                return 0;
            default:
                throw "shouldn't be here";
        }
    }

    // todo: replace all this to remove all lingering dependencies on
    // websocket_frame
    binary_string_ptr prepare_frame(frame::opcode::value opcode,
                                    bool mask,
                                    const utf8_string& payload) {
        /*if (opcode != frame::opcode::text) {
         // todo: hybi_legacy doesn't allow non-text frames.
         throw;
         }*/

        // todo: utf8 validation on payload.



        binary_string_ptr response(new binary_string(0));


        m_write_frame.reset();
        m_write_frame.set_opcode(opcode);
        m_write_frame.set_masked(mask);

        m_write_frame.set_fin(true);
        m_write_frame.set_payload(payload);


        m_write_frame.process_payload();

        // todo
        response->resize(m_write_frame.get_header_len()+m_write_frame.get_payload().size());

        // copy header
        std::copy(m_write_frame.get_header(),m_write_frame.get_header()+m_write_frame.get_header_len(),response->begin());

        // copy payload
        std::copy(m_write_frame.get_payload().begin(),m_write_frame.get_payload().end(),response->begin()+m_write_frame.get_header_len());


        return response;
    }

    binary_string_ptr prepare_frame(frame::opcode::value opcode,
                                    bool mask,
                                    const binary_string& payload) {
        /*if (opcode != frame::opcode::text) {
         // todo: hybi_legacy doesn't allow non-text frames.
         throw;
         }*/

        // todo: utf8 validation on payload.

        binary_string_ptr response(new binary_string(0));

        m_write_frame.reset();
        m_write_frame.set_opcode(opcode);
        m_write_frame.set_masked(mask);
        m_write_frame.set_fin(true);
        m_write_frame.set_payload(payload);

        m_write_frame.process_payload();

        // todo
        response->resize(m_write_frame.get_header_len()+m_write_frame.get_payload().size());

        // copy header
        std::copy(m_write_frame.get_header(),m_write_frame.get_header()+m_write_frame.get_header_len(),response->begin());

        // copy payload
        std::copy(m_write_frame.get_payload().begin(),m_write_frame.get_payload().end(),response->begin()+m_write_frame.get_header_len());

        return response;
    }

    /*binary_string_ptr prepare_close_frame(close::status::value code,
                                          bool mask,
                                          const std::string& reason) {
        binary_string_ptr response(new binary_string(0));

        m_write_frame.reset();
        m_write_frame.set_opcode(frame::opcode::close);
        m_write_frame.set_masked(mask);
        m_write_frame.set_fin(true);
        m_write_frame.set_status(code,reason);

        m_write_frame.process_payload();

        // todo
        response->resize(m_write_frame.get_header_len()+m_write_frame.get_payload().size());

        // copy header
        std::copy(m_write_frame.get_header(),m_write_frame.get_header()+m_write_frame.get_header_len(),response->begin());

        // copy payload
        std::copy(m_write_frame.get_payload().begin(),m_write_frame.get_payload().end(),response->begin()+m_write_frame.get_header_len());

        return response;
    }*/

    // new prepare frame stuff
    void prepare_frame(message::data_ptr msg) {
        assert(msg);
        if (msg->get_prepared()) {
            return;
        }

        msg->validate_payload();

        bool masked = !m_connection.is_server();
        int32_t key = m_connection.rand();

        m_write_header.reset();
        m_write_header.set_fin(true);
        m_write_header.set_opcode(msg->get_opcode());
        m_write_header.set_masked(masked,key);
        m_write_header.set_payload_size(msg->get_payload().size());
        m_write_header.complete();

        msg->set_header(m_write_header.get_header_bytes());

        if (masked) {
            msg->set_masking_key(key);
            msg->mask();
        }

        msg->set_prepared(true);
    }

    void prepare_close_frame(message::data_ptr msg,
                             close::status::value code,
                             const std::string& reason)
    {
        assert(msg);
        if (msg->get_prepared()) {
            return;
        }

        // set close payload
        if (code != close::status::no_status) {
            const uint16_t payload = htons(static_cast<u_short>(code));

            msg->set_payload(std::string(reinterpret_cast<const char*>(&payload), 2));
            msg->append_payload(reason);
        }

        // prepare rest of frame
        prepare_frame(msg);
    }
private:
    // must be divisible by 8 (some things are hardcoded for 4 and 8 byte word
    // sizes
    static const size_t     payload_buffer_size = 512;

    connection_type&        m_connection;
    int                     m_state;

    message::data_ptr       m_data_message;
    message::control_ptr    m_control_message;
    hybi_header             m_header;
    hybi_header             m_write_header;
    size_t                  m_payload_left;

    char                    m_payload_buffer[payload_buffer_size];

    frame::parser<connection_type>  m_write_frame; // todo: refactor this out
};

template <class connection_type>
const size_t hybi<connection_type>::payload_buffer_size;

} // namespace processor
} // namespace websocketpp_02

#endif // websocket_processor_hybi_hpp
