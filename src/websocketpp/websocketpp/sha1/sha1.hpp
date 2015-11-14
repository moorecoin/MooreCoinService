/*
*****
sha1.hpp is a repackaging of the sha1.cpp and sha1.h files from the smallsha1
library (http://code.google.com/p/smallsha1/) into a single header suitable for
use as a header only library. this conversion was done by peter thorson
(webmaster@zaphoyd.com) in 2013. all modifications to the code are redistributed
under the same license as the original, which is listed below.
*****

 copyright (c) 2011, micael hildenborg
 all rights reserved.

 redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
    * redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * neither the name of micael hildenborg nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

 this software is provided by micael hildenborg ''as is'' and any
 express or implied warranties, including, but not limited to, the implied
 warranties of merchantability and fitness for a particular purpose are
 disclaimed. in no event shall micael hildenborg be liable for any
 direct, indirect, incidental, special, exemplary, or consequential damages
 (including, but not limited to, procurement of substitute goods or services;
 loss of use, data, or profits; or business interruption) however caused and
 on any theory of liability, whether in contract, strict liability, or tort
 (including negligence or otherwise) arising in any way out of the use of this
 software, even if advised of the possibility of such damage.
 */

#ifndef sha1_defined
#define sha1_defined

namespace websocketpp {
namespace sha1 {

namespace { // local

// rotate an integer value to left.
inline unsigned int rol(unsigned int value, unsigned int steps) {
    return ((value << steps) | (value >> (32 - steps)));
}

// sets the first 16 integers in the buffert to zero.
// used for clearing the w buffert.
inline void clearwbuffert(unsigned int * buffert)
{
    for (int pos = 16; --pos >= 0;)
    {
        buffert[pos] = 0;
    }
}

inline void innerhash(unsigned int * result, unsigned int * w)
{
    unsigned int a = result[0];
    unsigned int b = result[1];
    unsigned int c = result[2];
    unsigned int d = result[3];
    unsigned int e = result[4];

    int round = 0;

    #define sha1macro(func,val) \
    { \
        const unsigned int t = rol(a, 5) + (func) + e + val + w[round]; \
        e = d; \
        d = c; \
        c = rol(b, 30); \
        b = a; \
        a = t; \
    }

    while (round < 16)
    {
        sha1macro((b & c) | (~b & d), 0x5a827999)
        ++round;
    }
    while (round < 20)
    {
        w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
        sha1macro((b & c) | (~b & d), 0x5a827999)
        ++round;
    }
    while (round < 40)
    {
        w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
        sha1macro(b ^ c ^ d, 0x6ed9eba1)
        ++round;
    }
    while (round < 60)
    {
        w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
        sha1macro((b & c) | (b & d) | (c & d), 0x8f1bbcdc)
        ++round;
    }
    while (round < 80)
    {
        w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
        sha1macro(b ^ c ^ d, 0xca62c1d6)
        ++round;
    }

    #undef sha1macro

    result[0] += a;
    result[1] += b;
    result[2] += c;
    result[3] += d;
    result[4] += e;
}

} // namespace

/// calculate a sha1 hash
/**
 * @param src points to any kind of data to be hashed.
 * @param bytelength the number of bytes to hash from the src pointer.
 * @param hash should point to a buffer of at least 20 bytes of size for storing
 * the sha1 result in.
 */
inline void calc(void const * src, size_t bytelength, unsigned char * hash) {
    // init the result array.
    unsigned int result[5] = { 0x67452301, 0xefcdab89, 0x98badcfe,
                               0x10325476, 0xc3d2e1f0 };

    // cast the void src pointer to be the byte array we can work with.
    unsigned char const * sarray = (unsigned char const *) src;

    // the reusable round buffer
    unsigned int w[80];

    // loop through all complete 64byte blocks.

    size_t endcurrentblock;
    size_t currentblock = 0;

    if (bytelength >= 64) {
        size_t const endoffullblocks = bytelength - 64;

        while (currentblock <= endoffullblocks) {
            endcurrentblock = currentblock + 64;

            // init the round buffer with the 64 byte block data.
            for (int roundpos = 0; currentblock < endcurrentblock; currentblock += 4)
            {
                // this line will swap endian on big endian and keep endian on
                // little endian.
                w[roundpos++] = (unsigned int) sarray[currentblock + 3]
                        | (((unsigned int) sarray[currentblock + 2]) << 8)
                        | (((unsigned int) sarray[currentblock + 1]) << 16)
                        | (((unsigned int) sarray[currentblock]) << 24);
            }
            innerhash(result, w);
        }
    }

    // handle the last and not full 64 byte block if existing.
    endcurrentblock = bytelength - currentblock;
    clearwbuffert(w);
    size_t lastblockbytes = 0;
    for (;lastblockbytes < endcurrentblock; ++lastblockbytes) {
        w[lastblockbytes >> 2] |= (unsigned int) sarray[lastblockbytes + currentblock] << ((3 - (lastblockbytes & 3)) << 3);
    }

    w[lastblockbytes >> 2] |= 0x80 << ((3 - (lastblockbytes & 3)) << 3);
    if (endcurrentblock >= 56) {
        innerhash(result, w);
        clearwbuffert(w);
    }
    w[15] = bytelength << 3;
    innerhash(result, w);

    // store hash in result pointer, and make sure we get in in the correct
    // order on both endian models.
    for (int hashbyte = 20; --hashbyte >= 0;) {
        hash[hashbyte] = (result[hashbyte >> 2] >> (((3 - hashbyte) & 0x3) << 3)) & 0xff;
    }
}

} // namespace sha1
} // namespace websocketpp

#endif // sha1_defined
