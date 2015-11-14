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

#ifndef websocketpp_transport_iostream_base_hpp
#define websocketpp_transport_iostream_base_hpp

#include <websocketpp/common/system_error.hpp>
#include <websocketpp/common/cpp11.hpp>

#include <string>

namespace websocketpp {
namespace transport {
/// transport policy that uses stl iostream for i/o and does not support timers
namespace iostream {

/// iostream transport errors
namespace error {
enum value {
    /// catch-all error for transport policy errors that don't fit in other
    /// categories
    general = 1,

    /// async_read_at_least call requested more bytes than buffer can store
    invalid_num_bytes,

    /// async_read called while another async_read was in progress
    double_read,

    /// an operation that requires an output stream was attempted before
    /// setting one.
    output_stream_required,

    /// stream error
    bad_stream
};

/// iostream transport error category
class category : public lib::error_category {
    public:
    category() {}

    char const * name() const _websocketpp_noexcept_token_ {
        return "websocketpp.transport.iostream";
    }

    std::string message(int value) const {
        switch(value) {
            case general:
                return "generic iostream transport policy error";
            case invalid_num_bytes:
                return "async_read_at_least call requested more bytes than buffer can store";
            case double_read:
                return "async read already in progress";
            case output_stream_required:
                return "an output stream to be set before async_write can be used";
            case bad_stream:
                return "a stream operation returned ios::bad";
            default:
                return "unknown";
        }
    }
};

/// get a reference to a static copy of the iostream transport error category
inline lib::error_category const & get_category() {
    static category instance;
    return instance;
}

/// get an error code with the given value and the iostream transport category
inline lib::error_code make_error_code(error::value e) {
    return lib::error_code(static_cast<int>(e), get_category());
}

} // namespace error
} // namespace iostream
} // namespace transport
} // namespace websocketpp
_websocketpp_error_code_enum_ns_start_
template<> struct is_error_code_enum<websocketpp::transport::iostream::error::value>
{
    static bool const value = true;
};
_websocketpp_error_code_enum_ns_end_

#endif // websocketpp_transport_iostream_base_hpp
