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

#ifndef websocketpp_logger_stub_hpp
#define websocketpp_logger_stub_hpp

#include <string>

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/logger/levels.hpp>

namespace websocketpp {
namespace log {

/// stub logger that ignores all input
class stub {
public:
    /// construct the logger
    /**
     * @param hint a channel type specific hint for how to construct the logger
     */
    explicit stub(channel_type_hint::value) {}
    
    /// construct the logger
    /**
     * @param default_channels a set of channels to statically enable
     * @param hint a channel type specific hint for how to construct the logger
     */
    stub(level, channel_type_hint::value) {}
    _websocketpp_constexpr_token_ stub() {}

    /// dynamically enable the given list of channels
    /**
     * all operations on the stub logger are no-ops and all arguments are 
     * ignored
     *
     * @param channels the package of channels to enable
     */
    void set_channels(level) {}
    
    /// dynamically disable the given list of channels
    /**
     * all operations on the stub logger are no-ops and all arguments are 
     * ignored
     *
     * @param channels the package of channels to disable
     */
    void clear_channels(level) {}

    /// write a string message to the given channel
    /**
     * writing on the stub logger is a no-op and all arguments are ignored
     *
     * @param channel the package of channels to write to
     * @param msg the message to write
     */
    void write(level, std::string const &) {}
    
    /// write a cstring message to the given channel
    /**
     * writing on the stub logger is a no-op and all arguments are ignored
     *
     * @param channel the package of channels to write to
     * @param msg the message to write
     */
    void write(level, char const *) {}

    /// test whether a channel is statically enabled
    /**
     * the stub logger has no channels so all arguments are ignored and 
     * `static_test` always returns false.
     *
     * @param channel the package of channels to test
     */
    _websocketpp_constexpr_token_ bool static_test(level) const {
        return false;
    }
    
    /// test whether a channel is dynamically enabled
    /**
     * the stub logger has no channels so all arguments are ignored and 
     * `dynamic_test` always returns false.
     *
     * @param channel the package of channels to test
     */
    bool dynamic_test(level) {
        return false;
    }
};

} // log
} // websocketpp

#endif // websocketpp_logger_stub_hpp
