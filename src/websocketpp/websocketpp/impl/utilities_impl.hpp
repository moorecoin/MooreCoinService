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

#ifndef websocketpp_utilities_impl_hpp
#define websocketpp_utilities_impl_hpp

namespace websocketpp {
namespace utility {

inline std::string to_lower(std::string const & in) {
    std::string out = in;
    std::transform(out.begin(),out.end(),out.begin(),::tolower);
    return out;
}

inline std::string to_hex(const std::string& input) {
    std::string output;
    std::string hex = "0123456789abcdef";

    for (size_t i = 0; i < input.size(); i++) {
        output += hex[(input[i] & 0xf0) >> 4];
        output += hex[input[i] & 0x0f];
        output += " ";
    }

    return output;
}

inline std::string to_hex(const uint8_t* input,size_t length) {
    std::string output;
    std::string hex = "0123456789abcdef";

    for (size_t i = 0; i < length; i++) {
        output += hex[(input[i] & 0xf0) >> 4];
        output += hex[input[i] & 0x0f];
        output += " ";
    }

    return output;
}

inline std::string to_hex(const char* input,size_t length) {
    return to_hex(reinterpret_cast<const uint8_t*>(input),length);
}

inline std::string string_replace_all(std::string subject, const std::string&
    search, const std::string& replace)
{
    size_t pos = 0;
    while((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
    return subject;
}

} // namespace utility
} // namespace websocketpp

#endif // websocketpp_utilities_impl_hpp
