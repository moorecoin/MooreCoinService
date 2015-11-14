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

#ifndef zs_logger_hpp
#define zs_logger_hpp

#include <iostream>
#include <sstream>

#include <boost/date_time/posix_time/posix_time.hpp>

namespace websocketpp_02 {
namespace log {

namespace alevel {
    typedef uint16_t value;

    static const value off = 0x0;

    // a single line on connect with connecting ip, websocket version,
    // request resource, user agent, and the response code.
    static const value connect = 0x1;
    // a single line on disconnect with wasclean status and local and remote
    // close codes and reasons.
    static const value disconnect = 0x2;
    // a single line on incoming and outgoing control messages.
    static const value control = 0x4;
    // a single line on incoming and outgoing frames with full frame headers
    static const value frame_header = 0x10;
    // adds payloads to frame logs. note these can be long!
    static const value frame_payload = 0x20;
    // a single line on incoming and outgoing messages with metadata about type,
    // length, etc
    static const value message_header = 0x40;
    // adds payloads to message logs. note these can be long!
    static const value message_payload = 0x80;

    // notices about internal endpoint operations
    static const value endpoint = 0x100;

    // debug values
    static const value debug_handshake = 0x8000;
    static const value debug_close = 0x4000;
    static const value devel = 0x2000;

    static const value all = 0xffff;
}

namespace elevel {
    typedef uint32_t value; // make these two values different types djs

    static const value off = 0x0;

    static const value devel = 0x1;     // debugging
    static const value library = 0x2;   // library usage exceptions
    static const value info = 0x4;      //
    static const value warn = 0x8;      //
    static const value rerror = 0x10;    // recoverable error
    static const value fatal = 0x20;    // unrecoverable error

    static const value all = 0xffff;
}

extern void websocketlog(alevel::value, const std::string&);
extern void websocketlog(elevel::value, const std::string&);

template <typename level_type>
class logger {
public:
    template <typename t>
    logger<level_type>& operator<<(t a) {
            m_oss << a; // for now, make this unconditional djs
        return *this;
    }

    logger<level_type>& operator<<(logger<level_type>& (*f)(logger<level_type>&)) {
        return f(*this);
    }

    bool test_level(level_type l) {
        return (m_level & l) != 0;
    }

    void set_level(level_type l) {
        m_level |= l;
    }

    void set_levels(level_type l1, level_type l2) {
        level_type i = l1;

        while (i <= l2) {
            set_level(i);
            i *= 2;
        }
    }

    void unset_level(level_type l) {
        m_level &= ~l;
    }

    void set_prefix(const std::string& prefix) {
        if (prefix == "") {
            m_prefix = prefix;
        } else {
            m_prefix = prefix + " ";
        }
    }

    logger<level_type>& print() {
            websocketlog(m_write_level, m_oss.str()); // hand to our logger djs
            m_oss.str("");
#if 0
        if (test_level(m_write_level)) {
            std::cout << m_prefix <<
                boost::posix_time::to_iso_extended_string(
                    boost::posix_time::second_clock::local_time()
                ) << " [" << m_write_level << "] " << m_oss.str() << std::endl;
            m_oss.str("");
        }
#endif
       return *this;
    }

    logger<level_type>& at(level_type l) {
        m_write_level = l;
        return *this;
    }
private:
    std::ostringstream m_oss;
    level_type m_write_level;
    level_type m_level;
    std::string m_prefix;
};

template <typename level_type>
logger<level_type>& endl(logger<level_type>& out)
{
    return out.print();
}

}
}

#endif // zs_logger_hpp
