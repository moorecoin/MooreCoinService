/*
 * copyright (c) 2014, peter thorson. all rights reserved.
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

#ifndef websocketpp_logger_basic_hpp
#define websocketpp_logger_basic_hpp

/* need a way to print a message to the log
 *
 * - timestamps
 * - channels
 * - thread safe
 * - output to stdout or file
 * - selective output channels, both compile time and runtime
 * - named channels
 * - ability to test whether a log message will be printed at compile time
 *
 */

#include <ctime>
#include <iostream>
#include <iomanip>

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/stdint.hpp>
#include <websocketpp/common/time.hpp>
#include <websocketpp/logger/levels.hpp>

namespace websocketpp {
namespace log {

/// basic logger that outputs to an ostream
template <typename concurrency, typename names>
class basic {
public:
    basic<concurrency,names>(channel_type_hint::value h = 
        channel_type_hint::access)
      : m_static_channels(0xffffffff)
      , m_dynamic_channels(0)
      , m_out(h == channel_type_hint::error ? &std::cerr : &std::cout) {}
      
    basic<concurrency,names>(std::ostream * out)
      : m_static_channels(0xffffffff)
      , m_dynamic_channels(0)
      , m_out(out) {}

    basic<concurrency,names>(level c, channel_type_hint::value h = 
        channel_type_hint::access)
      : m_static_channels(c)
      , m_dynamic_channels(0)
      , m_out(h == channel_type_hint::error ? &std::cerr : &std::cout) {}
    
    basic<concurrency,names>(level c, std::ostream * out)
      : m_static_channels(c)
      , m_dynamic_channels(0)
      , m_out(out) {}

    void set_ostream(std::ostream * out = &std::cout) {
        m_out = out;
    }

    void set_channels(level channels) {
        if (channels == names::none) {
            clear_channels(names::all);
            return;
        }

        scoped_lock_type lock(m_lock);
        m_dynamic_channels |= (channels & m_static_channels);
    }

    void clear_channels(level channels) {
        scoped_lock_type lock(m_lock);
        m_dynamic_channels &= ~channels;
    }

    void write(level channel, std::string const & msg) {
        scoped_lock_type lock(m_lock);
        if (!this->dynamic_test(channel)) { return; }
        *m_out << "[" << timestamp << "] "
                  << "[" << names::channel_name(channel) << "] "
                  << msg << "\n";
        m_out->flush();
    }

    void write(level channel, char const * msg) {
        scoped_lock_type lock(m_lock);
        if (!this->dynamic_test(channel)) { return; }
        *m_out << "[" << timestamp << "] "
                  << "[" << names::channel_name(channel) << "] "
                  << msg << "\n";
        m_out->flush();
    }

    _websocketpp_constexpr_token_ bool static_test(level channel) const {
        return ((channel & m_static_channels) != 0);
    }

    bool dynamic_test(level channel) {
        return ((channel & m_dynamic_channels) != 0);
    }
private:
    typedef typename concurrency::scoped_lock_type scoped_lock_type;
    typedef typename concurrency::mutex_type mutex_type;

    // the timestamp does not include the time zone, because on windows with the
    // default registry settings, the time zone would be written out in full,
    // which would be obnoxiously verbose.
    //
    // todo: find a workaround for this or make this format user settable
    static std::ostream & timestamp(std::ostream & os) {
        std::time_t t = std::time(null);
        std::tm lt = lib::localtime(t);
        #ifdef _websocketpp_puttime_
            return os << std::put_time(&lt,"%y-%m-%d %h:%m:%s");
        #else // falls back to strftime, which requires a temporary copy of the string.
            char buffer[20];
            size_t result = std::strftime(buffer,sizeof(buffer),"%y-%m-%d %h:%m:%s",&lt);
            return os << (result == 0 ? "unknown" : buffer);
        #endif
    }

    mutex_type m_lock;

    level const m_static_channels;
    level m_dynamic_channels;
    std::ostream * m_out;
};

} // log
} // websocketpp

#endif // websocketpp_logger_basic_hpp
