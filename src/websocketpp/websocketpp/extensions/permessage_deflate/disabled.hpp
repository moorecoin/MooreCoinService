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

#ifndef websocketpp_extension_permessage_deflate_disabled_hpp
#define websocketpp_extension_permessage_deflate_disabled_hpp

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/system_error.hpp>

#include <websocketpp/http/constants.hpp>
#include <websocketpp/extensions/extension.hpp>

#include <map>
#include <string>
#include <utility>

namespace websocketpp {
namespace extensions {
namespace permessage_deflate {

/// stub class for use when disabling permessage_deflate extension
/**
 * this class is a stub that implements the permessage_deflate interface
 * with minimal dependencies. it is used to disable permessage_deflate
 * functionality at compile time without loading any unnecessary code.
 */
template <typename config>
class disabled {
    typedef std::pair<lib::error_code,std::string> err_str_pair;

public:
    /// negotiate extension
    /**
     * the disabled extension always fails the negotiation with a disabled 
     * error.
     *
     * @param offer attribute from client's offer
     * @return status code and value to return to remote endpoint
     */
    err_str_pair negotiate(http::attribute_list const &) {
        return make_pair(make_error_code(error::disabled),std::string());
    }

    /// returns true if the extension is capable of providing
    /// permessage_deflate functionality
    bool is_implemented() const {
        return false;
    }

    /// returns true if permessage_deflate functionality is active for this
    /// connection
    bool is_enabled() const {
        return false;
    }

    /// compress bytes
    /**
     * @param [in] in string to compress
     * @param [out] out string to append compressed bytes to
     * @return error or status code
     */
    lib::error_code compress(std::string const &, std::string &) {
        return make_error_code(error::disabled);
    }

    /// decompress bytes
    /**
     * @param buf byte buffer to decompress
     * @param len length of buf
     * @param out string to append decompressed bytes to
     * @return error or status code
     */
    lib::error_code decompress(uint8_t const *, size_t, std::string &) {
        return make_error_code(error::disabled);
    }
};

} // namespace permessage_deflate
} // namespace extensions
} // namespace websocketpp

#endif // websocketpp_extension_permessage_deflate_disabled_hpp
