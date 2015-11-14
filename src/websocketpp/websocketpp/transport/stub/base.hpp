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

#ifndef websocketpp_transport_stub_base_hpp
#define websocketpp_transport_stub_base_hpp

#include <websocketpp/common/system_error.hpp>
#include <websocketpp/common/cpp11.hpp>

#include <string>

namespace websocketpp {
namespace transport {
/// stub transport policy that has no input or output.
namespace stub {

/// stub transport errors
namespace error {
enum value {
    /// catch-all error for transport policy errors that don't fit in other
    /// categories
    general = 1,

    /// not implimented
    not_implimented
};

/// iostream transport error category
class category : public lib::error_category {
    public:
    category() {}

    char const * name() const _websocketpp_noexcept_token_ {
        return "websocketpp.transport.stub";
    }

    std::string message(int value) const {
        switch(value) {
            case general:
                return "generic stub transport policy error";
            case not_implimented:
                return "feature not implimented";
            default:
                return "unknown";
        }
    }
};

/// get a reference to a static copy of the stub transport error category
inline lib::error_category const & get_category() {
    static category instance;
    return instance;
}

/// get an error code with the given value and the stub transport category
inline lib::error_code make_error_code(error::value e) {
    return lib::error_code(static_cast<int>(e), get_category());
}

} // namespace error
} // namespace stub
} // namespace transport
} // namespace websocketpp
_websocketpp_error_code_enum_ns_start_
template<> struct is_error_code_enum<websocketpp::transport::stub::error::value>
{
    static bool const value = true;
};
_websocketpp_error_code_enum_ns_end_

#endif // websocketpp_transport_stub_base_hpp
