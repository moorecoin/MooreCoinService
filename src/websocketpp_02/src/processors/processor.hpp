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

#ifndef websocket_processor_hpp
#define websocket_processor_hpp

#include <exception>
#include <string>

namespace websocketpp_02 {
namespace processor {

namespace error {
    enum value {
        fatal_error = 0,            // force session end
        soft_error = 1,             // should log and ignore
        protocol_violation = 2,     // must end session
        payload_violation = 3,      // should end session
        internal_endpoint_error = 4,// cleanly end session
        message_too_big = 5,        // ???
        out_of_messages = 6         // read queue is empty, wait
    };
}

class exception : public std::exception {
public:
    exception(const std::string& msg,
              error::value code = error::fatal_error)
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

} // namespace processor
} // namespace websocketpp_02

#include "../http/parser.hpp"
#include "../uri.hpp"
#include "../websocket_frame.hpp" // todo: clean up

#include "../messages/data.hpp"
#include "../messages/control.hpp"

#include <boost/shared_ptr.hpp>

#include <iostream>

namespace websocketpp_02 {
namespace processor {

class processor_base : boost::noncopyable {
public:
    virtual ~processor_base() {}
    // validate client handshake
    // validate server handshake

    // given a list of http headers determine if the values are sufficient
    // to start a websocket session. if so begin constructing a response, if not throw a handshake
    // exception.
    // validate handshake request
    virtual void validate_handshake(const http::parser::request& headers) const = 0;

    virtual void handshake_response(const http::parser::request& request,http::parser::response& response) = 0;

    // extracts client origin from a handshake request
    virtual std::string get_origin(const http::parser::request& request) const = 0;
    // extracts client uri from a handshake request
    virtual uri_ptr get_uri(const http::parser::request& request) const = 0;

    // consume bytes, throw on exception
    virtual void consume(std::istream& s) = 0;

    // is there a message ready to be dispatched?
    virtual bool ready() const = 0;
    virtual bool is_control() const = 0;
    virtual message::data_ptr get_data_message() = 0;
    virtual message::control_ptr get_control_message() = 0;
    virtual void reset() = 0;

    virtual uint64_t get_bytes_needed() const = 0;

    // get information about the message that is ready
    //virtual frame::opcode::value get_opcode() const = 0;

    //virtual utf8_string_ptr get_utf8_payload() const = 0;
    //virtual binary_string_ptr get_binary_payload() const = 0;
    //virtual close::status::value get_close_code() const = 0;
    //virtual utf8_string get_close_reason() const = 0;

    // todo: prepare a frame
    //virtual binary_string_ptr prepare_frame(frame::opcode::value opcode,
    //                                      bool mask,
    //                                      const utf8_string& payload)  = 0;
    //virtual binary_string_ptr prepare_frame(frame::opcode::value opcode,
    //                                      bool mask,
    //                                      const binary_string& payload)  = 0;
    //
    //virtual binary_string_ptr prepare_close_frame(close::status::value code,
    //                                            bool mask,
    //                                            const std::string& reason) = 0;

    virtual void prepare_frame(message::data_ptr msg) = 0;
    virtual void prepare_close_frame(message::data_ptr msg,
                                     close::status::value code,
                                     const std::string& reason) = 0;

};

typedef boost::shared_ptr<processor_base> ptr;

} // namespace processor
} // namespace websocketpp_02

#endif // websocket_processor_hpp
