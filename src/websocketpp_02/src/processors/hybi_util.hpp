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

#ifndef websocket_hybi_util_hpp
#define websocket_hybi_util_hpp

#include "../common.hpp"

namespace websocketpp_02 {
namespace processor {
namespace hybi_util {

// type used to store a masking key
union masking_key_type {
    int32_t i;
    char    c[4];
};

// extract a masking key into a value the size of a machine word. machine word
// size must be 4 or 8
size_t prepare_masking_key(const masking_key_type& key);

// circularly shifts the supplied prepared masking key by offset bytes
// prepared_key must be the output of prepare_masking_key with the associated
//    restrictions on the machine word size.
// offset must be 0, 1, 2, or 3
size_t circshift_prepared_key(size_t prepared_key, size_t offset);

// basic byte by byte mask
template <typename iter_type>
void byte_mask(iter_type b, iter_type e, const masking_key_type& key, size_t key_offset = 0) {
    size_t key_index = key_offset;
    for (iter_type i = b; i != e; i++) {
        *i ^= key.c[key_index++];
        key_index %= 4;
    }
}

// exactly masks the bytes from start to end using key `key`
void word_mask_exact(char* data,size_t length,const masking_key_type& key);

} // namespace hybi_util
} // namespace processor
} // namespace websocketpp_02

#endif // websocket_hybi_util_hpp
