// copyright (c) 2008-2009 bjoern hoehrmann <bjoern@hoehrmann.de>
// see http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#ifndef utf8_validator_hpp
#define utf8_validator_hpp

#include <stdint.h>

namespace utf8_validator {

static const unsigned int utf8_accept = 0;
static const unsigned int utf8_reject = 1;

static const uint8_t utf8d[] = {
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

uint32_t inline
decode(uint32_t* state, uint32_t* codep, uint8_t byte) {
  uint32_t type = utf8d[byte];

  *codep = (*state != utf8_accept) ?
    (byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);

  *state = utf8d[256 + *state*16 + type];
  return *state;
}
    
class validator {
public:
    validator() : m_state(utf8_accept),m_codepoint(0) {}
    
    bool consume (uint32_t byte) {
        if (utf8_validator::decode(&m_state,&m_codepoint,static_cast<uint8_t>(byte)) == utf8_reject) {
            return false;
        }
        return true;
    }
    
    template <typename iterator_type>
    bool decode (iterator_type b, iterator_type e) {
        for (iterator_type i = b; i != e; i++) {
            if (utf8_validator::decode(&m_state,&m_codepoint,*i) == utf8_reject) {
                return false;
            }
        }
        return true;
    }
    
    bool complete() {
        return m_state == utf8_accept;
    }
    
    void reset() {
        m_state = utf8_accept;
        m_codepoint = 0;
    }
private:
    uint32_t    m_state;
    uint32_t    m_codepoint;
};

// convenience function that creates a validator, validates a complete string 
// and returns the result.
// todo: should this be inline?
inline bool validate(const std::string& s) {
    validator v;
    if (!v.decode(s.begin(),s.end())) {
        return false;
    }
    return v.complete();
}
    
} // namespace utf8_validator

#endif // utf8_validator_hpp
