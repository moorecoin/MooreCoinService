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

#include "data.hpp"

#include "../processors/processor.hpp"
#include "../processors/hybi_header.hpp"

#ifdef max
	#undef max
#endif // #ifdef max

using websocketpp_02::message::data;
using websocketpp_02::processor::hybi_util::circshift_prepared_key;

data::data(data::pool_ptr p, size_t s) : m_prepared(false),m_index(s),m_ref_count(0),m_pool(p),m_live(false) {
    m_payload.reserve(payload_size_init);
}

websocketpp_02::frame::opcode::value data::get_opcode() const {
    return m_opcode;
}

const std::string& data::get_payload() const {
    return m_payload;
}
const std::string& data::get_header() const {
    return m_header;
}

// input must be a buffer with size divisible by the machine word_size and at
// least ceil(size/word_size)*word_size bytes long.
void data::process_payload(char *input, size_t size) {
    //std::cout << "data message processing: " << size << " bytes" << std::endl;

    const size_t new_size = m_payload.size() + size;

    if (new_size > payload_size_max) {
        throw processor::exception("message too big",processor::error::message_too_big);
    }

    if (new_size > m_payload.capacity()) {
        m_payload.reserve(std::max(new_size, 2*m_payload.capacity()));
    }

    if (m_masked) {
        //std::cout << "message is masked" << std::endl;

        //std::cout << "before: " << zsutil::to_hex(input, size) << std::endl;

        // this retrieves ceiling of size / word size
        size_t n = (size + sizeof(size_t) - 1) / sizeof(size_t);

        // reinterpret the input as an array of word sized integers
        size_t* data = reinterpret_cast<size_t*>(input);

        // unmask working buffer
        for (size_t i = 0; i < n; i++) {
            data[i] ^= m_prepared_key;
        }

        //std::cout << "after: " << zsutil::to_hex(input, size) << std::endl;

        // circshift working key
        //std::cout << "circshift by : " << size%4 << " bytes " << zsutil::to_hex(reinterpret_cast<char*>(&m_prepared_key),sizeof(size_t));
        m_prepared_key = circshift_prepared_key(m_prepared_key, size%4);
        //std::cout << " to " << zsutil::to_hex(reinterpret_cast<char*>(&m_prepared_key),sizeof(size_t)) << std::endl;
    }

    if (m_opcode == frame::opcode::text) {
        if (!m_validator.decode(input, input+size)) {
            throw processor::exception("invalid utf8 data",
                                       processor::error::payload_violation);
        }
    }

    // copy working buffer into
    //std::cout << "before: " << m_payload.size() << std::endl;

    m_payload.append(input, size);

    //std::cout << "after: " << m_payload.size() << std::endl;
}

void data::reset(websocketpp_02::frame::opcode::value opcode) {
    m_opcode = opcode;
    m_masked = false;
    m_payload.clear();
    m_validator.reset();
    m_prepared = false;
}

void data::complete() {
    if (m_opcode == frame::opcode::text) {
        if (!m_validator.complete()) {
            throw processor::exception("invalid utf8 data",
                                       processor::error::payload_violation);
        }
    }
}

void data::validate_payload() {
    if (m_opcode == frame::opcode::text) {
        if (!m_validator.decode(m_payload.begin(), m_payload.end())) {
            throw exception("invalid utf8 data",error::payload_violation);
        }

        if (!m_validator.complete()) {
            throw exception("invalid utf8 data",error::payload_violation);
        }
    }
}

void data::set_masking_key(int32_t key) {
    m_masking_key.i = key;
    m_prepared_key = processor::hybi_util::prepare_masking_key(m_masking_key);
    m_masked = true;
}

void data::set_prepared(bool b) {
    m_prepared = b;
}

bool data::get_prepared() const {
    return m_prepared;
}

// this could be further optimized using methods that write directly into the
// m_payload buffer
void data::set_payload(const std::string& payload) {
    m_payload.reserve(payload.size());
    m_payload = payload;
}
void data::append_payload(const std::string& payload) {
    m_payload.reserve(m_payload.size()+payload.size());
    m_payload.append(payload);
}
void data::mask() {
    if (m_masked && m_payload.size() > 0) {
        // by default websocket++ performs block masking/unmasking in a mannor that makes
        // some assumptions about the nature of the machine and stl library used. in
        // particular the assumption is either a 32 or 64 bit word size and an stl with
        // std::string::data returning a contiguous char array.
        //
        // this method improves performance by 3-8x depending on the ratio of small to
        // large messages and the availability of a 64 bit processor.
        //
        // to disable this optimization (for use with alternative stl implementations or
        // processors) define websocketpp_strict_masking when compiling the library. this
        // will force the library to perform masking in single byte chunks.
        //#define websocketpp_strict_masking

        #ifdef websocketpp_strict_masking
            size_t len = m_payload.size();
            for (size_t i = 0; i < len; i++) {
                m_payload[i] ^= m_masking_key.c[i%4];
            }
        #else
            // this should trigger a write to the string in case the stl
            // implimentation is copy-on-write and hasn't been written to yet.
            // performing the masking will always require a copy of the string
            // in this case to hold the masked version.
            m_payload[0] = m_payload[0];

            size_t size = m_payload.size()/sizeof(size_t);
            size_t key = m_masking_key.i;
			size_t wordsize = sizeof(size_t);
            if (wordsize == 8) {
                key <<= 32;
                key |= (static_cast<size_t>(m_masking_key.i) & 0x00000000ffffffffll);
            }
            size_t* data = reinterpret_cast<size_t*>(const_cast<char*>(m_payload.data()));
            for (size_t i = 0; i < size; i++) {
                data[i] ^= key;
            }
            for (size_t i = size*sizeof(size_t); i < m_payload.size(); i++) {
                m_payload[i] ^= m_masking_key.c[i%4];
            }
        #endif
    }
}

void data::set_header(const std::string& header) {
    m_header = header;
}





//
void data::set_live() {
    m_live = true;
}
size_t data::get_index() const {
    return m_index;
}
