/*
  md5.hpp is a reformulation of the md5.h and md5.c code from
  http://www.opensource.apple.com/source/cups/cups-59/cups/md5.c to allow it to
  function as a component of a header only library. this conversion was done by
  peter thorson (webmaster@zaphoyd.com) in 2012 for the websocket++ project. the
  changes are released under the same license as the original (listed below)
*/
/*
  copyright (c) 1999, 2002 aladdin enterprises.  all rights reserved.

  this software is provided 'as-is', without any express or implied
  warranty.  in no event will the authors be held liable for any damages
  arising from the use of this software.

  permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. the origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. if you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. this notice may not be removed or altered from any source distribution.

  l. peter deutsch
  ghost@aladdin.com

 */
/* $id: md5.h,v 1.4 2002/04/13 19:20:28 lpd exp $ */
/*
  independent implementation of md5 (rfc 1321).

  this code implements the md5 algorithm defined in rfc 1321, whose
  text is available at
    http://www.ietf.org/rfc/rfc1321.txt
  the code is derived from the text of the rfc, including the test suite
  (section a.5) but excluding the rest of appendix a.  it does not include
  any code or documentation that is identified in the rfc as being
  copyrighted.

  the original and principal author of md5.h is l. peter deutsch
  <ghost@aladdin.com>.  other authors are noted in the change history
  that follows (in reverse chronological order):

  2002-04-13 lpd removed support for non-ansi compilers; removed
    references to ghostscript; clarified derivation from rfc 1321;
    now handles byte order either statically or dynamically.
  1999-11-04 lpd edited comments slightly for automatic toc extraction.
  1999-10-18 lpd fixed typo in header comment (ansi2knr rather than md5);
    added conditionalization for c++ compilation from martin
    purschke <purschke@bnl.gov>.
  1999-05-03 lpd original version.
 */

#ifndef websocketpp_common_md5_hpp
#define websocketpp_common_md5_hpp

/*
 * this package supports both compile-time and run-time determination of cpu
 * byte order.  if arch_is_big_endian is defined as 0, the code will be
 * compiled to run only on little-endian cpus; if arch_is_big_endian is
 * defined as non-zero, the code will be compiled to run only on big-endian
 * cpus; if arch_is_big_endian is not defined, the code will be compiled to
 * run on either big- or little-endian cpus, but will run slightly less
 * efficiently on either one than if arch_is_big_endian is defined.
 */

#include <stddef.h>
#include <string>
#include <cstring>

namespace websocketpp {
/// provides md5 hashing functionality
namespace md5 {

typedef unsigned char md5_byte_t; /* 8-bit byte */
typedef unsigned int md5_word_t; /* 32-bit word */

/* define the state of the md5 algorithm. */
typedef struct md5_state_s {
    md5_word_t count[2];    /* message length in bits, lsw first */
    md5_word_t abcd[4];     /* digest buffer */
    md5_byte_t buf[64];     /* accumulate block */
} md5_state_t;

/* initialize the algorithm. */
inline void md5_init(md5_state_t *pms);

/* append a string to the message. */
inline void md5_append(md5_state_t *pms, md5_byte_t const * data, size_t nbytes);

/* finish the message and return the digest. */
inline void md5_finish(md5_state_t *pms, md5_byte_t digest[16]);

#undef zsw_md5_byte_order   /* 1 = big-endian, -1 = little-endian, 0 = unknown */
#ifdef arch_is_big_endian
#  define zsw_md5_byte_order (arch_is_big_endian ? 1 : -1)
#else
#  define zsw_md5_byte_order 0
#endif

#define zsw_md5_t_mask ((md5_word_t)~0)
#define zsw_md5_t1 /* 0xd76aa478 */ (zsw_md5_t_mask ^ 0x28955b87)
#define zsw_md5_t2 /* 0xe8c7b756 */ (zsw_md5_t_mask ^ 0x173848a9)
#define zsw_md5_t3    0x242070db
#define zsw_md5_t4 /* 0xc1bdceee */ (zsw_md5_t_mask ^ 0x3e423111)
#define zsw_md5_t5 /* 0xf57c0faf */ (zsw_md5_t_mask ^ 0x0a83f050)
#define zsw_md5_t6    0x4787c62a
#define zsw_md5_t7 /* 0xa8304613 */ (zsw_md5_t_mask ^ 0x57cfb9ec)
#define zsw_md5_t8 /* 0xfd469501 */ (zsw_md5_t_mask ^ 0x02b96afe)
#define zsw_md5_t9    0x698098d8
#define zsw_md5_t10 /* 0x8b44f7af */ (zsw_md5_t_mask ^ 0x74bb0850)
#define zsw_md5_t11 /* 0xffff5bb1 */ (zsw_md5_t_mask ^ 0x0000a44e)
#define zsw_md5_t12 /* 0x895cd7be */ (zsw_md5_t_mask ^ 0x76a32841)
#define zsw_md5_t13    0x6b901122
#define zsw_md5_t14 /* 0xfd987193 */ (zsw_md5_t_mask ^ 0x02678e6c)
#define zsw_md5_t15 /* 0xa679438e */ (zsw_md5_t_mask ^ 0x5986bc71)
#define zsw_md5_t16    0x49b40821
#define zsw_md5_t17 /* 0xf61e2562 */ (zsw_md5_t_mask ^ 0x09e1da9d)
#define zsw_md5_t18 /* 0xc040b340 */ (zsw_md5_t_mask ^ 0x3fbf4cbf)
#define zsw_md5_t19    0x265e5a51
#define zsw_md5_t20 /* 0xe9b6c7aa */ (zsw_md5_t_mask ^ 0x16493855)
#define zsw_md5_t21 /* 0xd62f105d */ (zsw_md5_t_mask ^ 0x29d0efa2)
#define zsw_md5_t22    0x02441453
#define zsw_md5_t23 /* 0xd8a1e681 */ (zsw_md5_t_mask ^ 0x275e197e)
#define zsw_md5_t24 /* 0xe7d3fbc8 */ (zsw_md5_t_mask ^ 0x182c0437)
#define zsw_md5_t25    0x21e1cde6
#define zsw_md5_t26 /* 0xc33707d6 */ (zsw_md5_t_mask ^ 0x3cc8f829)
#define zsw_md5_t27 /* 0xf4d50d87 */ (zsw_md5_t_mask ^ 0x0b2af278)
#define zsw_md5_t28    0x455a14ed
#define zsw_md5_t29 /* 0xa9e3e905 */ (zsw_md5_t_mask ^ 0x561c16fa)
#define zsw_md5_t30 /* 0xfcefa3f8 */ (zsw_md5_t_mask ^ 0x03105c07)
#define zsw_md5_t31    0x676f02d9
#define zsw_md5_t32 /* 0x8d2a4c8a */ (zsw_md5_t_mask ^ 0x72d5b375)
#define zsw_md5_t33 /* 0xfffa3942 */ (zsw_md5_t_mask ^ 0x0005c6bd)
#define zsw_md5_t34 /* 0x8771f681 */ (zsw_md5_t_mask ^ 0x788e097e)
#define zsw_md5_t35    0x6d9d6122
#define zsw_md5_t36 /* 0xfde5380c */ (zsw_md5_t_mask ^ 0x021ac7f3)
#define zsw_md5_t37 /* 0xa4beea44 */ (zsw_md5_t_mask ^ 0x5b4115bb)
#define zsw_md5_t38    0x4bdecfa9
#define zsw_md5_t39 /* 0xf6bb4b60 */ (zsw_md5_t_mask ^ 0x0944b49f)
#define zsw_md5_t40 /* 0xbebfbc70 */ (zsw_md5_t_mask ^ 0x4140438f)
#define zsw_md5_t41    0x289b7ec6
#define zsw_md5_t42 /* 0xeaa127fa */ (zsw_md5_t_mask ^ 0x155ed805)
#define zsw_md5_t43 /* 0xd4ef3085 */ (zsw_md5_t_mask ^ 0x2b10cf7a)
#define zsw_md5_t44    0x04881d05
#define zsw_md5_t45 /* 0xd9d4d039 */ (zsw_md5_t_mask ^ 0x262b2fc6)
#define zsw_md5_t46 /* 0xe6db99e5 */ (zsw_md5_t_mask ^ 0x1924661a)
#define zsw_md5_t47    0x1fa27cf8
#define zsw_md5_t48 /* 0xc4ac5665 */ (zsw_md5_t_mask ^ 0x3b53a99a)
#define zsw_md5_t49 /* 0xf4292244 */ (zsw_md5_t_mask ^ 0x0bd6ddbb)
#define zsw_md5_t50    0x432aff97
#define zsw_md5_t51 /* 0xab9423a7 */ (zsw_md5_t_mask ^ 0x546bdc58)
#define zsw_md5_t52 /* 0xfc93a039 */ (zsw_md5_t_mask ^ 0x036c5fc6)
#define zsw_md5_t53    0x655b59c3
#define zsw_md5_t54 /* 0x8f0ccc92 */ (zsw_md5_t_mask ^ 0x70f3336d)
#define zsw_md5_t55 /* 0xffeff47d */ (zsw_md5_t_mask ^ 0x00100b82)
#define zsw_md5_t56 /* 0x85845dd1 */ (zsw_md5_t_mask ^ 0x7a7ba22e)
#define zsw_md5_t57    0x6fa87e4f
#define zsw_md5_t58 /* 0xfe2ce6e0 */ (zsw_md5_t_mask ^ 0x01d3191f)
#define zsw_md5_t59 /* 0xa3014314 */ (zsw_md5_t_mask ^ 0x5cfebceb)
#define zsw_md5_t60    0x4e0811a1
#define zsw_md5_t61 /* 0xf7537e82 */ (zsw_md5_t_mask ^ 0x08ac817d)
#define zsw_md5_t62 /* 0xbd3af235 */ (zsw_md5_t_mask ^ 0x42c50dca)
#define zsw_md5_t63    0x2ad7d2bb
#define zsw_md5_t64 /* 0xeb86d391 */ (zsw_md5_t_mask ^ 0x14792c6e)

static void md5_process(md5_state_t *pms, md5_byte_t const * data /*[64]*/) {
    md5_word_t
    a = pms->abcd[0], b = pms->abcd[1],
    c = pms->abcd[2], d = pms->abcd[3];
    md5_word_t t;
#if zsw_md5_byte_order > 0
    /* define storage only for big-endian cpus. */
    md5_word_t x[16];
#else
    /* define storage for little-endian or both types of cpus. */
    md5_word_t xbuf[16];
    md5_word_t const * x;
#endif

    {
#if zsw_md5_byte_order == 0
    /*
     * determine dynamically whether this is a big-endian or
     * little-endian machine, since we can use a more efficient
     * algorithm on the latter.
     */
    static int const w = 1;

    if (*((md5_byte_t const *)&w)) /* dynamic little-endian */
#endif
#if zsw_md5_byte_order <= 0     /* little-endian */
    {
        /*
         * on little-endian machines, we can process properly aligned
         * data without copying it.
         */
        if (!((data - (md5_byte_t const *)0) & 3)) {
        /* data are properly aligned */
        x = (md5_word_t const *)data;
        } else {
        /* not aligned */
        std::memcpy(xbuf, data, 64);
        x = xbuf;
        }
    }
#endif
#if zsw_md5_byte_order == 0
    else            /* dynamic big-endian */
#endif
#if zsw_md5_byte_order >= 0     /* big-endian */
    {
        /*
         * on big-endian machines, we must arrange the bytes in the
         * right order.
         */
        const md5_byte_t *xp = data;
        int i;

#  if zsw_md5_byte_order == 0
        x = xbuf;       /* (dynamic only) */
#  else
#    define xbuf x      /* (static only) */
#  endif
        for (i = 0; i < 16; ++i, xp += 4)
        xbuf[i] = xp[0] + (xp[1] << 8) + (xp[2] << 16) + (xp[3] << 24);
    }
#endif
    }

#define zsw_md5_rotate_left(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

    /* round 1. */
    /* let [abcd k s i] denote the operation
       a = b + ((a + f(b,c,d) + x[k] + t[i]) <<< s). */
#define zsw_md5_f(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define set(a, b, c, d, k, s, ti)\
  t = a + zsw_md5_f(b,c,d) + x[k] + ti;\
  a = zsw_md5_rotate_left(t, s) + b
    /* do the following 16 operations. */
    set(a, b, c, d,  0,  7,  zsw_md5_t1);
    set(d, a, b, c,  1, 12,  zsw_md5_t2);
    set(c, d, a, b,  2, 17,  zsw_md5_t3);
    set(b, c, d, a,  3, 22,  zsw_md5_t4);
    set(a, b, c, d,  4,  7,  zsw_md5_t5);
    set(d, a, b, c,  5, 12,  zsw_md5_t6);
    set(c, d, a, b,  6, 17,  zsw_md5_t7);
    set(b, c, d, a,  7, 22,  zsw_md5_t8);
    set(a, b, c, d,  8,  7,  zsw_md5_t9);
    set(d, a, b, c,  9, 12, zsw_md5_t10);
    set(c, d, a, b, 10, 17, zsw_md5_t11);
    set(b, c, d, a, 11, 22, zsw_md5_t12);
    set(a, b, c, d, 12,  7, zsw_md5_t13);
    set(d, a, b, c, 13, 12, zsw_md5_t14);
    set(c, d, a, b, 14, 17, zsw_md5_t15);
    set(b, c, d, a, 15, 22, zsw_md5_t16);
#undef set

     /* round 2. */
     /* let [abcd k s i] denote the operation
          a = b + ((a + g(b,c,d) + x[k] + t[i]) <<< s). */
#define zsw_md5_g(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define set(a, b, c, d, k, s, ti)\
  t = a + zsw_md5_g(b,c,d) + x[k] + ti;\
  a = zsw_md5_rotate_left(t, s) + b
     /* do the following 16 operations. */
    set(a, b, c, d,  1,  5, zsw_md5_t17);
    set(d, a, b, c,  6,  9, zsw_md5_t18);
    set(c, d, a, b, 11, 14, zsw_md5_t19);
    set(b, c, d, a,  0, 20, zsw_md5_t20);
    set(a, b, c, d,  5,  5, zsw_md5_t21);
    set(d, a, b, c, 10,  9, zsw_md5_t22);
    set(c, d, a, b, 15, 14, zsw_md5_t23);
    set(b, c, d, a,  4, 20, zsw_md5_t24);
    set(a, b, c, d,  9,  5, zsw_md5_t25);
    set(d, a, b, c, 14,  9, zsw_md5_t26);
    set(c, d, a, b,  3, 14, zsw_md5_t27);
    set(b, c, d, a,  8, 20, zsw_md5_t28);
    set(a, b, c, d, 13,  5, zsw_md5_t29);
    set(d, a, b, c,  2,  9, zsw_md5_t30);
    set(c, d, a, b,  7, 14, zsw_md5_t31);
    set(b, c, d, a, 12, 20, zsw_md5_t32);
#undef set

     /* round 3. */
     /* let [abcd k s t] denote the operation
          a = b + ((a + h(b,c,d) + x[k] + t[i]) <<< s). */
#define zsw_md5_h(x, y, z) ((x) ^ (y) ^ (z))
#define set(a, b, c, d, k, s, ti)\
  t = a + zsw_md5_h(b,c,d) + x[k] + ti;\
  a = zsw_md5_rotate_left(t, s) + b
     /* do the following 16 operations. */
    set(a, b, c, d,  5,  4, zsw_md5_t33);
    set(d, a, b, c,  8, 11, zsw_md5_t34);
    set(c, d, a, b, 11, 16, zsw_md5_t35);
    set(b, c, d, a, 14, 23, zsw_md5_t36);
    set(a, b, c, d,  1,  4, zsw_md5_t37);
    set(d, a, b, c,  4, 11, zsw_md5_t38);
    set(c, d, a, b,  7, 16, zsw_md5_t39);
    set(b, c, d, a, 10, 23, zsw_md5_t40);
    set(a, b, c, d, 13,  4, zsw_md5_t41);
    set(d, a, b, c,  0, 11, zsw_md5_t42);
    set(c, d, a, b,  3, 16, zsw_md5_t43);
    set(b, c, d, a,  6, 23, zsw_md5_t44);
    set(a, b, c, d,  9,  4, zsw_md5_t45);
    set(d, a, b, c, 12, 11, zsw_md5_t46);
    set(c, d, a, b, 15, 16, zsw_md5_t47);
    set(b, c, d, a,  2, 23, zsw_md5_t48);
#undef set

     /* round 4. */
     /* let [abcd k s t] denote the operation
          a = b + ((a + i(b,c,d) + x[k] + t[i]) <<< s). */
#define zsw_md5_i(x, y, z) ((y) ^ ((x) | ~(z)))
#define set(a, b, c, d, k, s, ti)\
  t = a + zsw_md5_i(b,c,d) + x[k] + ti;\
  a = zsw_md5_rotate_left(t, s) + b
     /* do the following 16 operations. */
    set(a, b, c, d,  0,  6, zsw_md5_t49);
    set(d, a, b, c,  7, 10, zsw_md5_t50);
    set(c, d, a, b, 14, 15, zsw_md5_t51);
    set(b, c, d, a,  5, 21, zsw_md5_t52);
    set(a, b, c, d, 12,  6, zsw_md5_t53);
    set(d, a, b, c,  3, 10, zsw_md5_t54);
    set(c, d, a, b, 10, 15, zsw_md5_t55);
    set(b, c, d, a,  1, 21, zsw_md5_t56);
    set(a, b, c, d,  8,  6, zsw_md5_t57);
    set(d, a, b, c, 15, 10, zsw_md5_t58);
    set(c, d, a, b,  6, 15, zsw_md5_t59);
    set(b, c, d, a, 13, 21, zsw_md5_t60);
    set(a, b, c, d,  4,  6, zsw_md5_t61);
    set(d, a, b, c, 11, 10, zsw_md5_t62);
    set(c, d, a, b,  2, 15, zsw_md5_t63);
    set(b, c, d, a,  9, 21, zsw_md5_t64);
#undef set

     /* then perform the following additions. (that is increment each
        of the four registers by the value it had before this block
        was started.) */
    pms->abcd[0] += a;
    pms->abcd[1] += b;
    pms->abcd[2] += c;
    pms->abcd[3] += d;
}

void md5_init(md5_state_t *pms) {
    pms->count[0] = pms->count[1] = 0;
    pms->abcd[0] = 0x67452301;
    pms->abcd[1] = /*0xefcdab89*/ zsw_md5_t_mask ^ 0x10325476;
    pms->abcd[2] = /*0x98badcfe*/ zsw_md5_t_mask ^ 0x67452301;
    pms->abcd[3] = 0x10325476;
}

void md5_append(md5_state_t *pms, md5_byte_t const * data, size_t nbytes) {
    md5_byte_t const * p = data;
    size_t left = nbytes;
    int offset = (pms->count[0] >> 3) & 63;
    md5_word_t nbits = (md5_word_t)(nbytes << 3);

    if (nbytes <= 0)
    return;

    /* update the message length. */
    pms->count[1] += nbytes >> 29;
    pms->count[0] += nbits;
    if (pms->count[0] < nbits)
    pms->count[1]++;

    /* process an initial partial block. */
    if (offset) {
    int copy = (offset + nbytes > 64 ? 64 - offset : static_cast<int>(nbytes));

    std::memcpy(pms->buf + offset, p, copy);
    if (offset + copy < 64)
        return;
    p += copy;
    left -= copy;
    md5_process(pms, pms->buf);
    }

    /* process full blocks. */
    for (; left >= 64; p += 64, left -= 64)
    md5_process(pms, p);

    /* process a final partial block. */
    if (left)
    std::memcpy(pms->buf, p, left);
}

void md5_finish(md5_state_t *pms, md5_byte_t digest[16]) {
    static md5_byte_t const pad[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    md5_byte_t data[8];
    int i;

    /* save the length before padding. */
    for (i = 0; i < 8; ++i)
    data[i] = (md5_byte_t)(pms->count[i >> 2] >> ((i & 3) << 3));
    /* pad to 56 bytes mod 64. */
    md5_append(pms, pad, ((55 - (pms->count[0] >> 3)) & 63) + 1);
    /* append the length. */
    md5_append(pms, data, 8);
    for (i = 0; i < 16; ++i)
    digest[i] = (md5_byte_t)(pms->abcd[i >> 2] >> ((i & 3) << 3));
}

// some convenience c++ functions
inline std::string md5_hash_string(std::string const & s) {
    char digest[16];

    md5_state_t state;

    md5_init(&state);
    md5_append(&state, (md5_byte_t const *)s.c_str(), s.size());
    md5_finish(&state, (md5_byte_t *)digest);

    std::string ret;
    ret.resize(16);
    std::copy(digest,digest+16,ret.begin());

    return ret;
}

const char hexval[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

inline std::string md5_hash_hex(std::string const & input) {
    std::string hash = md5_hash_string(input);
    std::string hex;

    for (size_t i = 0; i < hash.size(); i++) {
        hex.push_back(hexval[((hash[i] >> 4) & 0xf)]);
        hex.push_back(hexval[(hash[i]) & 0x0f]);
    }

    return hex;
}

} // md5
} // websocketpp

#endif // websocketpp_common_md5_hpp
