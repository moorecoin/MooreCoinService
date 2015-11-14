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

#ifndef websocketpp_extension_hpp
#define websocketpp_extension_hpp

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/system_error.hpp>

#include <string>
#include <vector>

namespace websocketpp {

/**
 * some generic information about extensions
 *
 * each extension object has an implemented flag. it can be retrieved by calling
 * is_implemented(). this compile time flag indicates whether or not the object
 * in question actually implements the extension or if it is a placeholder stub
 *
 * each extension object also has an enabled flag. it can be retrieved by
 * calling is_enabled(). this runtime flag indicates whether or not the
 * extension has been negotiated for this connection.
 */
namespace extensions {

namespace error {
enum value {
    /// catch all
    general = 1,

    /// extension disabled
    disabled
};

class category : public lib::error_category {
public:
    category() {}

    const char *name() const _websocketpp_noexcept_token_ {
        return "websocketpp.extension";
    }

    std::string message(int value) const {
        switch(value) {
            case general:
                return "generic extension error";
            case disabled:
                return "use of methods from disabled extension";
            default:
                return "unknown permessage-compress error";
        }
    }
};

inline const lib::error_category& get_category() {
    static category instance;
    return instance;
}

inline lib::error_code make_error_code(error::value e) {
    return lib::error_code(static_cast<int>(e), get_category());
}

} // namespace error
} // namespace extensions
} // namespace websocketpp

_websocketpp_error_code_enum_ns_start_
template<> struct is_error_code_enum
    <websocketpp::extensions::error::value>
{
    static const bool value = true;
};
_websocketpp_error_code_enum_ns_end_

#endif // websocketpp_extension_hpp
