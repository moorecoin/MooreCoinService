/*
 * the following code is adapted from code originally written by bjoern
 * hoehrmann <bjoern@hoehrmann.de>. see
 * http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
 *
 * the original license:
 *
 * copyright (c) 2008-2009 bjoern hoehrmann <bjoern@hoehrmann.de>
 *
 * permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "software"), to deal
 * in the software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the software, and to permit persons to whom the software is
 * furnished to do so, subject to the following conditions:
 *
 * the above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the software.
 *
 * the software is provided "as is", without warranty of any kind, express or
 * implied, including but not limited to the warranties of merchantability,
 * fitness for a particular purpose and noninfringement. in no event shall the
 * authors or copyright holders be liable for any claim, damages or other
 * liability, whether in an action of contract, tort or otherwise, arising from,
 * out of or in connection with the software or the use or other dealings in the
 * software.
*/

#ifndef utf8_validator_hpp
#define utf8_validator_hpp

#include <websocketpp/common/stdint.hpp>

namespace websocketpp {
namespace utf8_validator {

/// state that represents a valid utf8 input sequence
static unsigned int const utf8_accept = 0;
/// state that represents an invalid utf8 input sequence
static unsigned int const utf8_reject = 1;

/// lookup table for the utf8 decode state machine
static uint8_t const utf8d[] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
  0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
  0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
  0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
  1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
  1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
  1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

/// decode the next byte of a utf8 sequence
/**
 * @param [out] state the decoder state to advance
 * @param [out] codep the codepoint to fill in
 * @param [in] byte the byte to input
 * @return the ending state of the decode operation
 */
inline uint32_t decode(uint32_t * state, uint32_t * codep, uint8_t byte) {
  uint32_t type = utf8d[byte];

  *codep = (*state != utf8_accept) ?
    (byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);

  *state = utf8d[256 + *state*16 + type];
  return *state;
}

/// provides streaming utf8 validation functionality
class validator {
public:
    /// construct and initialize the validator
    validator() : m_state(utf8_accept),m_codepoint(0) {}

    /// advance the state of the validator with the next input byte
    /**
     * @param byte the byte to advance the validation state with
     * @return whether or not the byte resulted in a validation error.
     */
    bool consume (uint8_t byte) {
        if (utf8_validator::decode(&m_state,&m_codepoint,byte) == utf8_reject) {
            return false;
        }
        return true;
    }

    /// advance validator state with input from an iterator pair
    /**
     * @param begin input iterator to the start of the input range
     * @param end input iterator to the end of the input range
     * @return whether or not decoding the bytes resulted in a validation error.
     */
    template <typename iterator_type>
    bool decode (iterator_type begin, iterator_type end) {
        for (iterator_type it = begin; it != end; ++it) {
            unsigned int result = utf8_validator::decode(
                &m_state,
                &m_codepoint,
                static_cast<uint8_t>(*it)
            );

            if (result == utf8_reject) {
                return false;
            }
        }
        return true;
    }

    /// return whether the input sequence ended on a valid utf8 codepoint
    /**
     * @return whether or not the input sequence ended on a valid codepoint.
     */
    bool complete() {
        return m_state == utf8_accept;
    }

    /// reset the validator to decode another message
    void reset() {
        m_state = utf8_accept;
        m_codepoint = 0;
    }
private:
    uint32_t    m_state;
    uint32_t    m_codepoint;
};

/// validate a utf8 string
/**
 * convenience function that creates a validator, validates a complete string
 * and returns the result.
 */
inline bool validate(std::string const & s) {
    validator v;
    if (!v.decode(s.begin(),s.end())) {
        return false;
    }
    return v.complete();
}

} // namespace utf8_validator
} // namespace websocketpp

#endif // utf8_validator_hpp
