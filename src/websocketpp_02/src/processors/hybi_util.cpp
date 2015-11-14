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

#include "hybi_util.hpp"

namespace websocketpp_02 {
namespace processor {
namespace hybi_util {

size_t prepare_masking_key(const masking_key_type& key) {
    size_t prepared_key = key.i;
	size_t wordsize = sizeof(size_t);
    if (wordsize == 8) {
        prepared_key <<= 32;
        prepared_key |= (static_cast<size_t>(key.i) & 0x00000000ffffffffll);
    }
    return prepared_key;
}

size_t circshift_prepared_key(size_t prepared_key, size_t offset) {
    size_t temp = prepared_key << (sizeof(size_t)-offset)*8;
    return (prepared_key >> offset*8) | temp;
}

void word_mask_exact(char* data,size_t length,const masking_key_type& key) {
    size_t prepared_key = prepare_masking_key(key);
    size_t n = length/sizeof(size_t);
    size_t* word_data = reinterpret_cast<size_t*>(data);

    for (size_t i = 0; i < n; i++) {
        word_data[i] ^= prepared_key;
    }

    for (size_t i = n*sizeof(size_t); i < length; i++) {
        data[i] ^= key.c[i%4];
    }
}

} // namespace hybi_util
} // namespace processor
} // namespace websocketpp_02
