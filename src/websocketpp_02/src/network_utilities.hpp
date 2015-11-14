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

#ifndef network_utilities_hpp
#define network_utilities_hpp

#include <stdint.h>
#include <string>

// http://www.viva64.com/en/k/0018/
// todo: impliment stuff from here: 
// http://stackoverflow.com/questions/809902/64-bit-ntohl-in-c

namespace zsutil {

#define typ_init 0 
#define typ_smle 1 
#define typ_bige 2 

#undef htonll
#undef ntohll

uint64_t htonll(uint64_t src);
uint64_t ntohll(uint64_t src);

std::string lookup_ws_close_status_string(uint16_t code);

std::string to_hex(const std::string& input);
std::string to_hex(const char* input,size_t length);

} // namespace zsutil

#endif // network_utilities_hpp
