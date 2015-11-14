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

#ifndef websocket_control_message_hpp
#define websocket_control_message_hpp

#include <iostream>
#include <boost/shared_ptr.hpp>

#include "../processors/processor.hpp"
#include "../websocket_frame.hpp"
#include "../utf8_validator/utf8_validator.hpp"
#include "../processors/hybi_util.hpp"

using websocketpp_02::processor::hybi_util::circshift_prepared_key;

namespace websocketpp_02 {
namespace message {

class control {
public:
    control() {
        m_payload.reserve(payload_size_init);
    }

    frame::opcode::value get_opcode() const {
        return m_opcode;
    };

    const std::string& get_payload() const {
        return m_payload;
    }

    void process_payload(char *input,uint64_t size) {
        const size_t new_size = static_cast<size_t>(m_payload.size() + size);

        if (new_size > payload_size_max) {
            throw processor::exception("message payload was too large.",processor::error::message_too_big);
        }

        if (m_masked) {
            // this retrieves ceiling of size / word size
            size_t n = static_cast<size_t>((size + sizeof(size_t) - 1) / sizeof(size_t));

            // reinterpret the input as an array of word sized integers
            size_t* data = reinterpret_cast<size_t*>(input);

            // unmask working buffer
            for (size_t i = 0; i < n; i++) {
                data[i] ^= m_prepared_key;
            }

            // circshift working key
            m_prepared_key = circshift_prepared_key(m_prepared_key, size%4);
        }

        // copy working buffer into
        m_payload.append(input, static_cast<size_t>(size));
    }

    void complete() {
        if (m_opcode == frame::opcode::close) {
            if (m_payload.size() == 1) {
                throw processor::exception("single byte close code",processor::error::protocol_violation);
            } else if (m_payload.size() >= 2) {
                close::status::value code = close::status::value(get_raw_close_code());

                if (close::status::invalid(code)) {
                    throw processor::exception("close code is not allowed on the wire.",processor::error::protocol_violation);
                } else if (close::status::reserved(code)) {
                    throw processor::exception("close code is reserved.",processor::error::protocol_violation);
                }

            }
            if (m_payload.size() > 2) {
                if (!m_validator.decode(m_payload.begin()+2,m_payload.end())) {
                    throw processor::exception("invalid utf8",processor::error::payload_violation);
                }
                if (!m_validator.complete()) {
                    throw processor::exception("invalid utf8",processor::error::payload_violation);
                }
            }
        }
    }

    void reset(frame::opcode::value opcode, uint32_t masking_key) {
        m_opcode = opcode;
        set_masking_key(masking_key);
        m_payload.resize(0);
        m_validator.reset();
    }

    close::status::value get_close_code() const {
        if (m_payload.size() == 0) {
            return close::status::no_status;
        } else {
            return close::status::value(get_raw_close_code());
        }
    }

    std::string get_close_reason() const {
        if (m_payload.size() > 2) {
            return m_payload.substr(2);
        } else {
            return std::string();
        }
    }

    void set_masking_key(int32_t key) {
        m_masking_key.i = key;
        m_prepared_key = processor::hybi_util::prepare_masking_key(m_masking_key);
        m_masked = true;
    }
private:
    uint16_t get_raw_close_code() const {
        if (m_payload.size() <= 1) {
            throw processor::exception("get_raw_close_code called with invalid size",processor::error::fatal_error);
        }

        union {uint16_t i;char c[2];} val;

        val.c[0] = m_payload[0];
        val.c[1] = m_payload[1];

        return ntohs(val.i);
    }

    static const uint64_t payload_size_init = 128; // 128b
    static const uint64_t payload_size_max = 128; // 128b

    typedef websocketpp_02::processor::hybi_util::masking_key_type masking_key_type;

    union masking_key {
        int32_t i;
        char    c[4];
    };

    // message state
    frame::opcode::value        m_opcode;

    // utf8 validation state
    utf8_validator::validator   m_validator;

    // masking state
    masking_key_type            m_masking_key;
    bool                        m_masked;
    size_t                      m_prepared_key;

    // message payload
    std::string                 m_payload;
};

typedef boost::shared_ptr<control> control_ptr;

} // namespace message
} // namespace websocketpp_02

#endif // websocket_control_message_hpp
