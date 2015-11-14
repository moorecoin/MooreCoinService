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

#ifndef websocketpp_common_network_hpp
#define websocketpp_common_network_hpp

// for ntohs and htons
#if defined(_win32)
    #include <winsock2.h>
#else
    //#include <arpa/inet.h>
    #include <netinet/in.h>
#endif

namespace websocketpp {
namespace lib {
namespace net {

inline bool is_little_endian() {
    short int val = 0x1;
    char *ptr = (char*)&val;
    return (ptr[0] == 1);
}

#define typ_init 0
#define typ_smle 1
#define typ_bige 2

/// convert 64 bit value to network byte order
/**
 * this method is prefixed to avoid conflicts with operating system level
 * macros for this functionality.
 *
 * todo: figure out if it would be beneficial to use operating system level
 * macros for this.
 *
 * @param src the integer in host byte order
 * @return src converted to network byte order
 */
inline uint64_t _htonll(uint64_t src) {
    static int typ = typ_init;
    unsigned char c;
    union {
        uint64_t ull;
        unsigned char c[8];
    } x;
    if (typ == typ_init) {
        x.ull = 0x01;
        typ = (x.c[7] == 0x01ull) ? typ_bige : typ_smle;
    }
    if (typ == typ_bige)
        return src;
    x.ull = src;
    c = x.c[0]; x.c[0] = x.c[7]; x.c[7] = c;
    c = x.c[1]; x.c[1] = x.c[6]; x.c[6] = c;
    c = x.c[2]; x.c[2] = x.c[5]; x.c[5] = c;
    c = x.c[3]; x.c[3] = x.c[4]; x.c[4] = c;
    return x.ull;
}

/// convert 64 bit value to host byte order
/**
 * this method is prefixed to avoid conflicts with operating system level
 * macros for this functionality.
 *
 * todo: figure out if it would be beneficial to use operating system level
 * macros for this.
 *
 * @param src the integer in network byte order
 * @return src converted to host byte order
 */
inline uint64_t _ntohll(uint64_t src) {
    return _htonll(src);
}

} // net
} // lib
} // websocketpp

#endif // websocketpp_common_network_hpp
