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

#include "hybi_header.hpp"

#include <cstring>

using websocketpp_02::processor::hybi_header;

hybi_header::hybi_header() {
    reset();
}
void hybi_header::reset() {
    memset(m_header, 0x00, max_header_length);
    m_state = state_basic_header;
    m_bytes_needed = basic_header_length;
}

// writing interface (parse a byte stream)
void hybi_header::consume(std::istream& input) {
    switch (m_state) {
        case state_basic_header:
            input.read(&m_header[basic_header_length-m_bytes_needed],
                       m_bytes_needed);

            m_bytes_needed -= input.gcount();

            if (m_bytes_needed == 0) {
                process_basic_header();

                validate_basic_header();

                if (m_bytes_needed > 0) {
                    m_state = state_extended_header;
                } else {
                    process_extended_header();
                    m_state = state_ready;
                }
            }
            break;
        case state_extended_header:
            input.read(&m_header[get_header_len()-m_bytes_needed],
                       m_bytes_needed);

            m_bytes_needed -= input.gcount();

            if (m_bytes_needed == 0) {
                process_extended_header();
                m_state = state_ready;
            }
            break;
        default:
            break;
    }
    //std::cout << "header so far: " << zsutil::to_hex(std::string(m_header,max_header_length)) << std::endl;
}
uint64_t hybi_header::get_bytes_needed() const {
    return m_bytes_needed;
}
bool hybi_header::ready() const {
    return m_state == state_ready;
}

// writing interface (set fields directly)
void hybi_header::set_fin(bool fin) {
    set_header_bit(bpb0_fin,0,fin);
}
void hybi_header::set_rsv1(bool b) {
    set_header_bit(bpb0_rsv1,0,b);
}
void hybi_header::set_rsv2(bool b) {
    set_header_bit(bpb0_rsv2,0,b);
}
void hybi_header::set_rsv3(bool b) {
    set_header_bit(bpb0_rsv3,0,b);
}
void hybi_header::set_opcode(websocketpp_02::frame::opcode::value op) {
    m_header[0] &= (0xff ^ bpb0_opcode); // clear op bits
    m_header[0] |= op; // set op bits
}
void hybi_header::set_masked(bool masked,int32_t key) {
    if (masked) {
        m_header[1] |= bpb1_mask;
        set_masking_key(key);
    } else {
        m_header[1] &= (0xff ^ bpb1_mask);
        clear_masking_key();
    }
}
void hybi_header::set_payload_size(uint64_t size) {
    if (size <= frame::limits::payload_size_basic) {
        m_header[1] |= size;
        m_payload_size = size;
    } else if (size <= frame::limits::payload_size_extended) {
        if (get_masked()) {
            // shift mask bytes to the correct position given the new size
            unsigned int mask_offset = get_header_len()-4;
            m_header[1] |= basic_payload_16bit_code;
            memcpy(&m_header[get_header_len()-4], &m_header[mask_offset], 4);
        } else {
            m_header[1] |= basic_payload_16bit_code;
        }
        m_payload_size = size;
        *(reinterpret_cast<uint16_t*>(&m_header[basic_header_length])) = htons(static_cast<uint16_t>(size));

       /* uint16_t net_size = htons(static_cast<uint16_t>(size));
		//memcpy(&m_header[basic_header_length], &net_size, sizeof(uint16_t));
		std::copy(
			reinterpret_cast<char*>(&net_size),
			reinterpret_cast<char*>(&net_size)+sizeof(uint16_t),
			&m_header[basic_header_length]
		);*/
    } else if (size <= frame::limits::payload_size_jumbo) {
        if (get_masked()) {
            // shift mask bytes to the correct position given the new size
            unsigned int mask_offset = get_header_len()-4;
            m_header[1] |= basic_payload_64bit_code;
            memcpy(&m_header[get_header_len()-4], &m_header[mask_offset], 4);
        } else {
            m_header[1] |= basic_payload_64bit_code;
        }
        m_payload_size = size;
        *(reinterpret_cast<uint64_t*>(&m_header[basic_header_length])) = zsutil::htonll(size);
    } else {
        throw processor::exception("set_payload_size called with value that was too large (>2^63)",processor::error::message_too_big);
    }

}
void hybi_header::complete() {
    validate_basic_header();
    m_state = state_ready;
}

// reading interface (get string of bytes)
std::string hybi_header::get_header_bytes() const {
    return std::string(m_header,get_header_len());
}

// reading interface (get fields directly)
bool hybi_header::get_fin() const {
    return ((m_header[0] & bpb0_fin) == bpb0_fin);
}
bool hybi_header::get_rsv1() const {
    return ((m_header[0] & bpb0_rsv1) == bpb0_rsv1);
}
bool hybi_header::get_rsv2() const {
    return ((m_header[0] & bpb0_rsv2) == bpb0_rsv2);
}
bool hybi_header::get_rsv3() const {
    return ((m_header[0] & bpb0_rsv3) == bpb0_rsv3);
}
websocketpp_02::frame::opcode::value hybi_header::get_opcode() const {
    return frame::opcode::value(m_header[0] & bpb0_opcode);
}
bool hybi_header::get_masked() const {
    return ((m_header[1] & bpb1_mask) == bpb1_mask);
}
int32_t hybi_header::get_masking_key() const {
    if (!get_masked()) {
        return 0;
    }
    return *reinterpret_cast<const int32_t*>(&m_header[get_header_len()-4]);
}
uint64_t hybi_header::get_payload_size() const {
    return m_payload_size;
}

bool hybi_header::is_control() const {
    return (frame::opcode::is_control(get_opcode()));
}

// private
unsigned int hybi_header::get_header_len() const {
    unsigned int temp = 2;

    if (get_masked()) {
        temp += 4;
    }

    if (get_basic_size() == 126) {
        temp += 2;
    } else if (get_basic_size() == 127) {
        temp += 8;
    }

    return temp;
}

uint8_t hybi_header::get_basic_size() const {
    return m_header[1] & bpb1_payload;
}

void hybi_header::validate_basic_header() const {
    // check for control frame size
    if (is_control() && get_basic_size() > frame::limits::payload_size_basic) {
        throw processor::exception("control frame is too large",processor::error::protocol_violation);
    }

    // check for reserved bits
    if (get_rsv1() || get_rsv2() || get_rsv3()) {
        throw processor::exception("reserved bit used",processor::error::protocol_violation);
    }

    // check for reserved opcodes
    if (frame::opcode::reserved(get_opcode())) {
        throw processor::exception("reserved opcode used",processor::error::protocol_violation);
    }

    // check for invalid opcodes
    if (frame::opcode::invalid(get_opcode())) {
        throw processor::exception("invalid opcode used",processor::error::protocol_violation);
    }

    // check for fragmented control message
    if (is_control() && !get_fin()) {
        throw processor::exception("fragmented control message",processor::error::protocol_violation);
    }
}

void hybi_header::process_basic_header() {
    m_bytes_needed = get_header_len() - basic_header_length;
}
void hybi_header::process_extended_header() {
    uint8_t s = get_basic_size();

    if (s <= frame::limits::payload_size_basic) {
        m_payload_size = s;
    } else if (s == basic_payload_16bit_code) {
        // reinterpret the second two bytes as a 16 bit integer in network
        // byte order. convert to host byte order and store locally.
        m_payload_size = ntohs(*(
            reinterpret_cast<uint16_t*>(&m_header[basic_header_length])
        ));

        if (m_payload_size < s) {
            std::stringstream err;
            err << "payload length not minimally encoded. using 16 bit form for payload size: " << m_payload_size;
            throw processor::exception(err.str(),processor::error::protocol_violation);
        }

    } else if (s == basic_payload_64bit_code) {
        // reinterpret the second eight bytes as a 64 bit integer in
        // network byte order. convert to host byte order and store.
        m_payload_size = zsutil::ntohll(*(
            reinterpret_cast<uint64_t*>(&m_header[basic_header_length])
        ));

        if (m_payload_size <= frame::limits::payload_size_extended) {
            throw processor::exception("payload length not minimally encoded",
                                       processor::error::protocol_violation);
        }

    } else {
        // todo: shouldn't be here how to handle?
        throw processor::exception("invalid get_basic_size in process_extended_header");
    }
}

void hybi_header::set_header_bit(uint8_t bit,int byte,bool value) {
    if (value) {
        m_header[byte] |= bit;
    } else {
        m_header[byte] &= (0xff ^ bit);
    }
}

void hybi_header::set_masking_key(int32_t key) {
    *(reinterpret_cast<int32_t *>(&m_header[get_header_len()-4])) = key;
}
void hybi_header::clear_masking_key() {
    // this is a no-op as clearing the mask bit also changes the get_header_len
    // method to not include these byte ranges. whenever the masking bit is re-
    // set a new key is generated anyways.
}
