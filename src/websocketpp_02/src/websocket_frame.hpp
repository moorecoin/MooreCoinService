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

#ifndef websocket_frame_hpp
#define websocket_frame_hpp

#include "common.hpp"



#include "network_utilities.hpp"
#include "processors/processor.hpp"
#include "utf8_validator/utf8_validator.hpp"

#include <boost/utility.hpp>

#if beast_win32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <vector>
#include <cstring>
#include <iostream>
#include <algorithm>

namespace websocketpp_02 {
namespace frame {

/* policies to abstract out

 - random number generation
 - utf8 validation

rng
 int32_t gen()


class boost_random {
public:
    boost_random()
    int32_t gen();
private:
 boost::random::random_device m_rng;
 boost::random::variate_generator<boost::random::random_device&,boost::random::uniform_int_distribution<> > m_gen;
 }

 */

template <class rng_policy>
class parser : boost::noncopyable {
public:
    // basic payload byte flags
    static const uint8_t bpb0_opcode = 0x0f;
    static const uint8_t bpb0_rsv3 = 0x10;
    static const uint8_t bpb0_rsv2 = 0x20;
    static const uint8_t bpb0_rsv1 = 0x40;
    static const uint8_t bpb0_fin = 0x80;
    static const uint8_t bpb1_payload = 0x7f;
    static const uint8_t bpb1_mask = 0x80;

    static const uint8_t basic_payload_16bit_code = 0x7e; // 126
    static const uint8_t basic_payload_64bit_code = 0x7f; // 127

    static const unsigned int basic_header_length = 2;
    static const unsigned int max_header_length = 14;
    static const uint8_t extended_header_length = 12;
    static const uint64_t max_payload_size = 100000000; // 100mb

    // create an empty frame for writing into
    parser(rng_policy& rng) : m_rng(rng) {
        reset();
    }

    bool ready() const {
        return (m_state == state_ready);
    }
    uint64_t get_bytes_needed() const {
        return m_bytes_needed;
    }
    void reset() {
        m_state = state_basic_header;
        m_bytes_needed = basic_header_length;
        m_degraded = false;
        m_payload.clear();
        std::fill(m_header,m_header+max_header_length,0);
    }

    // method invariant: one of the following must always be true even in the case
    // of exceptions.
    // - m_bytes_needed > 0
    // - m-state = state_ready
    void consume(std::istream &s) {
        try {
            switch (m_state) {
                case state_basic_header:
                    s.read(&m_header[basic_header_length-m_bytes_needed],m_bytes_needed);

                    m_bytes_needed -= s.gcount();

                    if (m_bytes_needed == 0) {
                        process_basic_header();

                        validate_basic_header();

                        if (m_bytes_needed > 0) {
                            m_state = state_extended_header;
                        } else {
                            process_extended_header();

                            if (m_bytes_needed == 0) {
                                m_state = state_ready;
                                process_payload();

                            } else {
                                m_state = state_payload;
                            }
                        }
                    }
                    break;
                case state_extended_header:
                    s.read(&m_header[get_header_len()-m_bytes_needed],m_bytes_needed);

                    m_bytes_needed -= s.gcount();

                    if (m_bytes_needed == 0) {
                        process_extended_header();
                        if (m_bytes_needed == 0) {
                            m_state = state_ready;
                            process_payload();
                        } else {
                            m_state = state_payload;
                        }
                    }
                    break;
                case state_payload:
                    s.read(reinterpret_cast<char *>(&m_payload[m_payload.size()-m_bytes_needed]),
                           m_bytes_needed);

                    m_bytes_needed -= s.gcount();

                    if (m_bytes_needed == 0) {
                        m_state = state_ready;
                        process_payload();
                    }
                    break;
                case state_recovery:
                    // recovery state discards all bytes that are not the first byte
                    // of a close frame.
                    do {
                        s.read(reinterpret_cast<char *>(&m_header[0]),1);

                        //std::cout << std::hex << int(static_cast<unsigned char>(m_header[0])) << " ";

                        if (int(static_cast<unsigned char>(m_header[0])) == 0x88) {
                            //(bpb0_fin && connection_close)
                            m_bytes_needed--;
                            m_state = state_basic_header;
                            break;
                        }
                    } while (s.gcount() > 0);

                    //std::cout << std::endl;

                    break;
                default:
                    break;
            }

            /*if (s.gcount() == 0) {
             throw frame_error("consume read zero bytes",ferr_fatal_session_error);
             }*/
        } catch (const processor::exception& e) {
            // after this point all non-close frames must be considered garbage,
            // including the current one. reset it and put the reading frame into
            // a recovery state.
            if (m_degraded == true) {
                throw processor::exception("an error occurred while trying to gracefully recover from a less serious frame error.",processor::error::fatal_error);
            } else {
                reset();
                m_state = state_recovery;
                m_degraded = true;

                throw e;
            }
        }
    }

    // get pointers to underlying buffers
    char* get_header() {
        return m_header;
    }
    char* get_extended_header() {
        return m_header+basic_header_length;
    }
    unsigned int get_header_len() const {
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

    char* get_masking_key() {
        /*if (m_state != state_ready) {
            std::cout << "throw" << std::endl;
            throw processor::exception("attempted to get masking_key before reading full header");
        }*/
        return &m_header[get_header_len()-4];
    }

    // get and set header bits
    bool get_fin() const {
        return ((m_header[0] & bpb0_fin) == bpb0_fin);
    }
    void set_fin(bool fin) {
        if (fin) {
            m_header[0] |= bpb0_fin;
        } else {
            m_header[0] &= (0xff ^ bpb0_fin);
        }
    }

    bool get_rsv1() const {
        return ((m_header[0] & bpb0_rsv1) == bpb0_rsv1);
    }
    void set_rsv1(bool b) {
        if (b) {
            m_header[0] |= bpb0_rsv1;
        } else {
            m_header[0] &= (0xff ^ bpb0_rsv1);
        }
    }

    bool get_rsv2() const {
        return ((m_header[0] & bpb0_rsv2) == bpb0_rsv2);
    }
    void set_rsv2(bool b) {
        if (b) {
            m_header[0] |= bpb0_rsv2;
        } else {
            m_header[0] &= (0xff ^ bpb0_rsv2);
        }
    }

    bool get_rsv3() const {
        return ((m_header[0] & bpb0_rsv3) == bpb0_rsv3);
    }
    void set_rsv3(bool b) {
        if (b) {
            m_header[0] |= bpb0_rsv3;
        } else {
            m_header[0] &= (0xff ^ bpb0_rsv3);
        }
    }

    opcode::value get_opcode() const {
        return frame::opcode::value(m_header[0] & bpb0_opcode);
    }
    void set_opcode(opcode::value op) {
        if (opcode::reserved(op)) {
            throw processor::exception("reserved opcode",processor::error::protocol_violation);
        }

        if (opcode::invalid(op)) {
            throw processor::exception("invalid opcode",processor::error::protocol_violation);
        }

        if (is_control() && get_basic_size() > limits::payload_size_basic) {
            throw processor::exception("control frames can't have large payloads",processor::error::protocol_violation);
        }

        m_header[0] &= (0xff ^ bpb0_opcode); // clear op bits
        m_header[0] |= op; // set op bits
    }

    bool get_masked() const {
        return ((m_header[1] & bpb1_mask) == bpb1_mask);
    }
    void set_masked(bool masked) {
        if (masked) {
            m_header[1] |= bpb1_mask;
            generate_masking_key();
        } else {
            m_header[1] &= (0xff ^ bpb1_mask);
            clear_masking_key();
        }
    }

    uint8_t get_basic_size() const {
        return m_header[1] & bpb1_payload;
    }
    size_t get_payload_size() const {
        if (m_state != state_ready && m_state != state_payload) {
            // todo: how to handle errors like this?
            throw "attempted to get payload size before reading full header";
        }

        return m_payload.size();
    }

    close::status::value get_close_status() const {
        if (get_payload_size() == 0) {
            return close::status::no_status;
        } else if (get_payload_size() >= 2) {
            char val[2] = { m_payload[0], m_payload[1] };
            uint16_t code;

            std::copy(val,val+sizeof(code),&code);
            code = ntohs(code);

            return close::status::value(code);
        } else {
            return close::status::protocol_error;
        }
    }
    std::string get_close_msg() const {
        if (get_payload_size() > 2) {
            uint32_t state = utf8_validator::utf8_accept;
            uint32_t codep = 0;
            validate_utf8(&state,&codep,2);
            if (state != utf8_validator::utf8_accept) {
                throw processor::exception("invalid utf-8 data",processor::error::payload_violation);
            }
            return std::string(m_payload.begin()+2,m_payload.end());
        } else {
            return std::string();
        }
    }

    std::vector<unsigned char> &get_payload() {
        return m_payload;
    }

    void set_payload(const std::vector<unsigned char>& source) {
        set_payload_helper(source.size());

        std::copy(source.begin(),source.end(),m_payload.begin());
    }
    void set_payload(const std::string& source) {
        set_payload_helper(source.size());

        std::copy(source.begin(),source.end(),m_payload.begin());
    }
    void set_payload_helper(size_t s) {
        if (s > max_payload_size) {
            throw processor::exception("requested payload is over implementation defined limit",processor::error::message_too_big);
        }

        // limits imposed by the websocket spec
        if (is_control() && s > limits::payload_size_basic) {
            throw processor::exception("control frames can't have large payloads",processor::error::protocol_violation);
        }

        bool masked = get_masked();

        if (s <= limits::payload_size_basic) {
            m_header[1] = s;
        } else if (s <= limits::payload_size_extended) {
            m_header[1] = basic_payload_16bit_code;

            // this reinterprets the second pair of bytes in m_header as a
            // 16 bit int and writes the payload size there as an integer
            // in network byte order
            *reinterpret_cast<uint16_t*>(&m_header[basic_header_length]) = htons(s);
        } else if (s <= limits::payload_size_jumbo) {
            m_header[1] = basic_payload_64bit_code;
            *reinterpret_cast<uint64_t*>(&m_header[basic_header_length]) = zsutil::htonll(s);
        } else {
            throw processor::exception("payload size limit is 63 bits",processor::error::protocol_violation);
        }

        if (masked) {
            m_header[1] |= bpb1_mask;
        }

        m_payload.resize(s);
    }

    void set_status(close::status::value status,const std::string message = "") {
        // check for valid statuses
        if (close::status::invalid(status)) {
            std::stringstream err;
            err << "status code " << status << " is invalid";
            throw processor::exception(err.str());
        }

        if (close::status::reserved(status)) {
            std::stringstream err;
            err << "status code " << status << " is reserved";
            throw processor::exception(err.str());
        }

        m_payload.resize(2+message.size());

        char val[2];

        *reinterpret_cast<uint16_t*>(&val[0]) = htons(status);

        bool masked = get_masked();

        m_header[1] = message.size()+2;

        if (masked) {
            m_header[1] |= bpb1_mask;
        }

        m_payload[0] = val[0];
        m_payload[1] = val[1];

        std::copy(message.begin(),message.end(),m_payload.begin()+2);
    }

    bool is_control() const {
        return (opcode::is_control(get_opcode()));
    }

    std::string print_frame() const {
        std::stringstream f;

        unsigned int len = get_header_len();

        f << "frame: ";
        // print header
        for (unsigned int i = 0; i < len; i++) {
            f << std::hex << (unsigned short)m_header[i] << " ";
        }
        // print message
        if (m_payload.size() > 50) {
            f << "[payload of " << m_payload.size() << " bytes]";
        } else {
            std::vector<unsigned char>::const_iterator it;
            for (it = m_payload.begin(); it != m_payload.end(); it++) {
                f << *it;
            }
        }
        return f.str();
    }

    // reads basic header, sets and returns m_header_bits_needed
    void process_basic_header() {
        m_bytes_needed = get_header_len() - basic_header_length;
    }
    void process_extended_header() {
        uint8_t s = get_basic_size();
        uint64_t payload_size;
        int mask_index = basic_header_length;

        if (s <= limits::payload_size_basic) {
            payload_size = s;
        } else if (s == basic_payload_16bit_code) {
            // reinterpret the second two bytes as a 16 bit integer in network
            // byte order. convert to host byte order and store locally.
            payload_size = ntohs(*(
                reinterpret_cast<uint16_t*>(&m_header[basic_header_length])
            ));

            if (payload_size < s) {
                std::stringstream err;
                err << "payload length not minimally encoded. using 16 bit form for payload size: " << payload_size;
                m_bytes_needed = payload_size;
                throw processor::exception(err.str(),processor::error::protocol_violation);
            }

            mask_index += 2;
        } else if (s == basic_payload_64bit_code) {
            // reinterpret the second eight bytes as a 64 bit integer in
            // network byte order. convert to host byte order and store.
            payload_size = zsutil::ntohll(*(
                reinterpret_cast<uint64_t*>(&m_header[basic_header_length])
            ));

            if (payload_size <= limits::payload_size_extended) {
                m_bytes_needed = payload_size;
                throw processor::exception("payload length not minimally encoded",
                                processor::error::protocol_violation);
            }

            mask_index += 8;
        } else {
            // todo: shouldn't be here how to handle?
            throw processor::exception("invalid get_basic_size in process_extended_header");
        }

        if (get_masked() == 0) {
            clear_masking_key();
        } else {
            // todo: this should be removed entirely once it is confirmed to not
            // be used by anything.
            // std::copy(m_header[mask_index],m_header[mask_index+4],m_masking_key);
            /*m_masking_key[0] = m_header[mask_index+0];
            m_masking_key[1] = m_header[mask_index+1];
            m_masking_key[2] = m_header[mask_index+2];
            m_masking_key[3] = m_header[mask_index+3];*/
        }

        if (payload_size > max_payload_size) {
            // todo: frame/message size limits
            // todo: find a way to throw a server error without coupling frame
            //       with server
            // throw websocketpp_02::server_error("got frame with payload greater than maximum frame buffer size.");
            throw "got frame with payload greater than maximum frame buffer size.";
        }
        m_payload.resize(payload_size);
        m_bytes_needed = payload_size;
    }

    void process_payload() {
        if (get_masked()) {
            char *masking_key = get_masking_key();

            for (uint64_t i = 0; i < m_payload.size(); i++) {
                m_payload[i] = (m_payload[i] ^ masking_key[i%4]);
            }
        }
    }

    // experiment with more efficient masking code.
    void process_payload2() {
        // unmask payload one byte at a time

        //uint64_t key = (*((uint32_t*)m_masking_key;)) << 32;
        //key += *((uint32_t*)m_masking_key);

        // might need to switch byte order
        /*uint32_t key = *((uint32_t*)m_masking_key);

        // 4

        uint64_t i = 0;
        uint64_t s = (m_payload.size() / 4);

        std::cout << "s: " << s << std::endl;

        // chunks of 4
        for (i = 0; i < s; i+=4) {
            ((uint32_t*)(&m_payload[0]))[i] = (((uint32_t*)(&m_payload[0]))[i] ^ key);
        }

        // finish the last few
        for (i = s; i < m_payload.size(); i++) {
            m_payload[i] = (m_payload[i] ^ m_masking_key[i%4]);
        }*/
    }

    void validate_utf8(uint32_t* state,uint32_t* codep,size_t offset = 0) const {
        for (size_t i = offset; i < m_payload.size(); i++) {
            using utf8_validator::decode;

            if (decode(state,codep,m_payload[i]) == utf8_validator::utf8_reject) {
                throw processor::exception("invalid utf-8 data",processor::error::payload_violation);
            }
        }
    }
    void validate_basic_header() const {
        // check for control frame size
        if (is_control() && get_basic_size() > limits::payload_size_basic) {
            throw processor::exception("control frame is too large",processor::error::protocol_violation);
        }

        // check for reserved bits
        if (get_rsv1() || get_rsv2() || get_rsv3()) {
            throw processor::exception("reserved bit used",processor::error::protocol_violation);
        }

        // check for reserved opcodes
        if (opcode::reserved(get_opcode())) {
            throw processor::exception("reserved opcode used",processor::error::protocol_violation);
        }

        // check for fragmented control message
        if (is_control() && !get_fin()) {
            throw processor::exception("fragmented control message",processor::error::protocol_violation);
        }
    }

    void generate_masking_key() {
        *(reinterpret_cast<int32_t *>(&m_header[get_header_len()-4])) = m_rng.rand();
    }
    void clear_masking_key() {
        // this is a no-op as clearing the mask bit also changes the get_header_len
        // method to not include these byte ranges. whenever the masking bit is re-
        // set a new key is generated anyways.
    }

private:
    static const uint8_t state_basic_header = 1;
    static const uint8_t state_extended_header = 2;
    static const uint8_t state_payload = 3;
    static const uint8_t state_ready = 4;
    static const uint8_t state_recovery = 5;

    uint8_t     m_state;
    uint64_t    m_bytes_needed;
    bool        m_degraded;

    char m_header[max_header_length];
    std::vector<unsigned char> m_payload;

    rng_policy& m_rng;
};

}
}

#endif // websocket_frame_hpp
