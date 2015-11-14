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

#ifndef websocketpp_md5_wrapper_hpp
#define websocketpp_md5_wrapper_hpp

#include "md5.h"
#include <string>

namespace websocketpp_02 {

// could be compiled separately
inline std::string md5_hash_string(const std::string& s) {
	char digest[16];

	md5_state_t state;

	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)s.c_str(), s.size());
	md5_finish(&state, (md5_byte_t *)digest);

    std::string ret;
    ret.resize(16);
    std::copy(digest,digest+16,ret.begin());

	return ret;
}

const char hexval[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

inline std::string md5_hash_hex(const std::string& input) {
    std::string hash = md5_hash_string(input);
    std::string hex;

    for (size_t i = 0; i < hash.size(); i++) {
        hex.push_back(hexval[((hash[i] >> 4) & 0xf)]);
        hex.push_back(hexval[(hash[i]) & 0x0f]);
    }

    return hex;
}

} // websocketpp_02

#endif // websocketpp_md5_wrapper_hpp
