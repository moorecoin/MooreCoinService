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

#ifndef websocket_processor_hybi_header_hpp
#define websocket_processor_hybi_header_hpp

#include "processor.hpp"

namespace websocketpp_02 {
namespace processor {

/// describes a processor for reading and writing websocket frame headers
/**
 * the hybi_header class provides a processor capable of reading and writing
 * websocket frame headers. it has two writing modes and two reading modes.
 *
 * writing method 1: call consume() until ready()
 * writing method 2: call set_* methods followed by complete()
 *
 * writing methods are valid only when ready() returns false. use reset() to
 * reset the header for writing again. mixing writing methods between calls to
 * reset() may behave unpredictably.
 *
 * reading method 1: call get_header_bytes() to return a string of bytes
 * reading method 2: call get_* methods to read individual values
 *
 * reading methods are valid only when ready() is true.
 *
 * @par thread safety
 * @e distinct @e objects: safe.@n
 * @e shared @e objects: unsafe
 */
class hybi_header {
public:
    /// construct a header processor and initialize for writing
    hybi_header();
    /// reset a header processor for writing
    void reset();

    // writing interface (parse a byte stream)
    // valid only if ready() returns false
    // consume will throw a processor::exception in the case that the bytes it
    // read do not form a valid websocket frame header.
    void consume(std::istream& input);
    uint64_t get_bytes_needed() const;
    bool ready() const;

    // writing interface (set fields directly)
    // valid only if ready() returns false
    // set_* may allow invalid values. call complete() once values are set to
    // check for header validity.
    void set_fin(bool fin);
    void set_rsv1(bool b);
    void set_rsv2(bool b);
    void set_rsv3(bool b);
    void set_opcode(websocketpp_02::frame::opcode::value op);
    void set_masked(bool masked,int32_t key);
    void set_payload_size(uint64_t size);
    // complete will throw a processor::exception in the case that the
    // combination of values set do not form a valid websocket frame header.
    void complete();

    // reading interface (get string of bytes)
    // valid only if ready() returns true
    std::string get_header_bytes() const;

    // reading interface (get fields directly)
    // valid only if ready() returns true
    bool get_fin() const;
    bool get_rsv1() const;
    bool get_rsv2() const;
    bool get_rsv3() const;
    frame::opcode::value get_opcode() const;
    bool get_masked() const;
    // will return zero in the case where get_masked() is false. note:
    // a masking key of zero is slightly different than no mask at all.
    int32_t get_masking_key() const;
    uint64_t get_payload_size() const;

    bool is_control() const;
private:
    // general helper functions
    unsigned int get_header_len() const;
    uint8_t get_basic_size() const;
    void validate_basic_header() const;

    // helper functions for writing
    void process_basic_header();
    void process_extended_header();
    void set_header_bit(uint8_t bit,int byte,bool value);
    void set_masking_key(int32_t key);
    void clear_masking_key();

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

    static const uint8_t state_basic_header = 1;
    static const uint8_t state_extended_header = 2;
    static const uint8_t state_ready = 3;
    static const uint8_t state_write = 4;

    uint8_t     m_state;
    std::streamsize m_bytes_needed;
    uint64_t    m_payload_size;
    char m_header[max_header_length];
};

} // namespace processor
} // namespace websocketpp_02

#endif // websocket_processor_hybi_header_hpp
