/*
  copyright (c) 1999, 2000, 2002 aladdin enterprises.  all rights reserved.

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
/* $id: md5.c,v 1.6 2002/04/13 19:20:28 lpd exp $ */
/*
  independent implementation of md5 (rfc 1321).

  this code implements the md5 algorithm defined in rfc 1321, whose
  text is available at
	http://www.ietf.org/rfc/rfc1321.txt
  the code is derived from the text of the rfc, including the test suite
  (section a.5) but excluding the rest of appendix a.  it does not include
  any code or documentation that is identified in the rfc as being
  copyrighted.

  the original and principal author of md5.c is l. peter deutsch
  <ghost@aladdin.com>.  other authors are noted in the change history
  that follows (in reverse chronological order):

  2002-04-13 lpd clarified derivation from rfc 1321; now handles byte order
	either statically or dynamically; added missing #include <string.h>
	in library.
  2002-03-11 lpd corrected argument list for main(), and added int return
	type, in test program and t value program.
  2002-02-21 lpd added missing #include <stdio.h> in test program.
  2000-07-03 lpd patched to eliminate warnings about "constant is
	unsigned in ansi c, signed in traditional"; made test program
	self-checking.
  1999-11-04 lpd edited comments slightly for automatic toc extraction.
  1999-10-18 lpd fixed typo in header comment (ansi2knr rather than md5).
  1999-05-03 lpd original version.
 */

#include "md5.h"
#include <string.h>

#undef byte_order	/* 1 = big-endian, -1 = little-endian, 0 = unknown */
#ifdef arch_is_big_endian
#  define byte_order (arch_is_big_endian ? 1 : -1)
#else
#  define byte_order 0
#endif

#define t_mask ((md5_word_t)~0)
#define t1 /* 0xd76aa478 */ (t_mask ^ 0x28955b87)
#define t2 /* 0xe8c7b756 */ (t_mask ^ 0x173848a9)
#define t3    0x242070db
#define t4 /* 0xc1bdceee */ (t_mask ^ 0x3e423111)
#define t5 /* 0xf57c0faf */ (t_mask ^ 0x0a83f050)
#define t6    0x4787c62a
#define t7 /* 0xa8304613 */ (t_mask ^ 0x57cfb9ec)
#define t8 /* 0xfd469501 */ (t_mask ^ 0x02b96afe)
#define t9    0x698098d8
#define t10 /* 0x8b44f7af */ (t_mask ^ 0x74bb0850)
#define t11 /* 0xffff5bb1 */ (t_mask ^ 0x0000a44e)
#define t12 /* 0x895cd7be */ (t_mask ^ 0x76a32841)
#define t13    0x6b901122
#define t14 /* 0xfd987193 */ (t_mask ^ 0x02678e6c)
#define t15 /* 0xa679438e */ (t_mask ^ 0x5986bc71)
#define t16    0x49b40821
#define t17 /* 0xf61e2562 */ (t_mask ^ 0x09e1da9d)
#define t18 /* 0xc040b340 */ (t_mask ^ 0x3fbf4cbf)
#define t19    0x265e5a51
#define t20 /* 0xe9b6c7aa */ (t_mask ^ 0x16493855)
#define t21 /* 0xd62f105d */ (t_mask ^ 0x29d0efa2)
#define t22    0x02441453
#define t23 /* 0xd8a1e681 */ (t_mask ^ 0x275e197e)
#define t24 /* 0xe7d3fbc8 */ (t_mask ^ 0x182c0437)
#define t25    0x21e1cde6
#define t26 /* 0xc33707d6 */ (t_mask ^ 0x3cc8f829)
#define t27 /* 0xf4d50d87 */ (t_mask ^ 0x0b2af278)
#define t28    0x455a14ed
#define t29 /* 0xa9e3e905 */ (t_mask ^ 0x561c16fa)
#define t30 /* 0xfcefa3f8 */ (t_mask ^ 0x03105c07)
#define t31    0x676f02d9
#define t32 /* 0x8d2a4c8a */ (t_mask ^ 0x72d5b375)
#define t33 /* 0xfffa3942 */ (t_mask ^ 0x0005c6bd)
#define t34 /* 0x8771f681 */ (t_mask ^ 0x788e097e)
#define t35    0x6d9d6122
#define t36 /* 0xfde5380c */ (t_mask ^ 0x021ac7f3)
#define t37 /* 0xa4beea44 */ (t_mask ^ 0x5b4115bb)
#define t38    0x4bdecfa9
#define t39 /* 0xf6bb4b60 */ (t_mask ^ 0x0944b49f)
#define t40 /* 0xbebfbc70 */ (t_mask ^ 0x4140438f)
#define t41    0x289b7ec6
#define t42 /* 0xeaa127fa */ (t_mask ^ 0x155ed805)
#define t43 /* 0xd4ef3085 */ (t_mask ^ 0x2b10cf7a)
#define t44    0x04881d05
#define t45 /* 0xd9d4d039 */ (t_mask ^ 0x262b2fc6)
#define t46 /* 0xe6db99e5 */ (t_mask ^ 0x1924661a)
#define t47    0x1fa27cf8
#define t48 /* 0xc4ac5665 */ (t_mask ^ 0x3b53a99a)
#define t49 /* 0xf4292244 */ (t_mask ^ 0x0bd6ddbb)
#define t50    0x432aff97
#define t51 /* 0xab9423a7 */ (t_mask ^ 0x546bdc58)
#define t52 /* 0xfc93a039 */ (t_mask ^ 0x036c5fc6)
#define t53    0x655b59c3
#define t54 /* 0x8f0ccc92 */ (t_mask ^ 0x70f3336d)
#define t55 /* 0xffeff47d */ (t_mask ^ 0x00100b82)
#define t56 /* 0x85845dd1 */ (t_mask ^ 0x7a7ba22e)
#define t57    0x6fa87e4f
#define t58 /* 0xfe2ce6e0 */ (t_mask ^ 0x01d3191f)
#define t59 /* 0xa3014314 */ (t_mask ^ 0x5cfebceb)
#define t60    0x4e0811a1
#define t61 /* 0xf7537e82 */ (t_mask ^ 0x08ac817d)
#define t62 /* 0xbd3af235 */ (t_mask ^ 0x42c50dca)
#define t63    0x2ad7d2bb
#define t64 /* 0xeb86d391 */ (t_mask ^ 0x14792c6e)


static void
md5_process(md5_state_t *pms, const md5_byte_t *data /*[64]*/)
{
    md5_word_t
	a = pms->abcd[0], b = pms->abcd[1],
	c = pms->abcd[2], d = pms->abcd[3];
    md5_word_t t;
#if byte_order > 0
    /* define storage only for big-endian cpus. */
    md5_word_t x[16];
#else
    /* define storage for little-endian or both types of cpus. */
    md5_word_t xbuf[16];
    const md5_word_t *x;
#endif

    {
#if byte_order == 0
	/*
	 * determine dynamically whether this is a big-endian or
	 * little-endian machine, since we can use a more efficient
	 * algorithm on the latter.
	 */
	static const int w = 1;

	if (*((const md5_byte_t *)&w)) /* dynamic little-endian */
#endif
#if byte_order <= 0		/* little-endian */
	{
	    /*
	     * on little-endian machines, we can process properly aligned
	     * data without copying it.
	     */
	    if (!((data - (const md5_byte_t *)0) & 3)) {
		/* data are properly aligned */
		x = (const md5_word_t *)data;
	    } else {
		/* not aligned */
		memcpy(xbuf, data, 64);
		x = xbuf;
	    }
	}
#endif
#if byte_order == 0
	else			/* dynamic big-endian */
#endif
#if byte_order >= 0		/* big-endian */
	{
	    /*
	     * on big-endian machines, we must arrange the bytes in the
	     * right order.
	     */
	    const md5_byte_t *xp = data;
	    int i;

#  if byte_order == 0
	    x = xbuf;		/* (dynamic only) */
#  else
#    define xbuf x		/* (static only) */
#  endif
	    for (i = 0; i < 16; ++i, xp += 4)
		xbuf[i] = xp[0] + (xp[1] << 8) + (xp[2] << 16) + (xp[3] << 24);
	}
#endif
    }

#define rotate_left(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

    /* round 1. */
    /* let [abcd k s i] denote the operation
       a = b + ((a + f(b,c,d) + x[k] + t[i]) <<< s). */
#define f(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define set(a, b, c, d, k, s, ti)\
  t = a + f(b,c,d) + x[k] + ti;\
  a = rotate_left(t, s) + b
    /* do the following 16 operations. */
    set(a, b, c, d,  0,  7,  t1);
    set(d, a, b, c,  1, 12,  t2);
    set(c, d, a, b,  2, 17,  t3);
    set(b, c, d, a,  3, 22,  t4);
    set(a, b, c, d,  4,  7,  t5);
    set(d, a, b, c,  5, 12,  t6);
    set(c, d, a, b,  6, 17,  t7);
    set(b, c, d, a,  7, 22,  t8);
    set(a, b, c, d,  8,  7,  t9);
    set(d, a, b, c,  9, 12, t10);
    set(c, d, a, b, 10, 17, t11);
    set(b, c, d, a, 11, 22, t12);
    set(a, b, c, d, 12,  7, t13);
    set(d, a, b, c, 13, 12, t14);
    set(c, d, a, b, 14, 17, t15);
    set(b, c, d, a, 15, 22, t16);
#undef set

     /* round 2. */
     /* let [abcd k s i] denote the operation
          a = b + ((a + g(b,c,d) + x[k] + t[i]) <<< s). */
#define g(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define set(a, b, c, d, k, s, ti)\
  t = a + g(b,c,d) + x[k] + ti;\
  a = rotate_left(t, s) + b
     /* do the following 16 operations. */
    set(a, b, c, d,  1,  5, t17);
    set(d, a, b, c,  6,  9, t18);
    set(c, d, a, b, 11, 14, t19);
    set(b, c, d, a,  0, 20, t20);
    set(a, b, c, d,  5,  5, t21);
    set(d, a, b, c, 10,  9, t22);
    set(c, d, a, b, 15, 14, t23);
    set(b, c, d, a,  4, 20, t24);
    set(a, b, c, d,  9,  5, t25);
    set(d, a, b, c, 14,  9, t26);
    set(c, d, a, b,  3, 14, t27);
    set(b, c, d, a,  8, 20, t28);
    set(a, b, c, d, 13,  5, t29);
    set(d, a, b, c,  2,  9, t30);
    set(c, d, a, b,  7, 14, t31);
    set(b, c, d, a, 12, 20, t32);
#undef set

     /* round 3. */
     /* let [abcd k s t] denote the operation
          a = b + ((a + h(b,c,d) + x[k] + t[i]) <<< s). */
#define h(x, y, z) ((x) ^ (y) ^ (z))
#define set(a, b, c, d, k, s, ti)\
  t = a + h(b,c,d) + x[k] + ti;\
  a = rotate_left(t, s) + b
     /* do the following 16 operations. */
    set(a, b, c, d,  5,  4, t33);
    set(d, a, b, c,  8, 11, t34);
    set(c, d, a, b, 11, 16, t35);
    set(b, c, d, a, 14, 23, t36);
    set(a, b, c, d,  1,  4, t37);
    set(d, a, b, c,  4, 11, t38);
    set(c, d, a, b,  7, 16, t39);
    set(b, c, d, a, 10, 23, t40);
    set(a, b, c, d, 13,  4, t41);
    set(d, a, b, c,  0, 11, t42);
    set(c, d, a, b,  3, 16, t43);
    set(b, c, d, a,  6, 23, t44);
    set(a, b, c, d,  9,  4, t45);
    set(d, a, b, c, 12, 11, t46);
    set(c, d, a, b, 15, 16, t47);
    set(b, c, d, a,  2, 23, t48);
#undef set

     /* round 4. */
     /* let [abcd k s t] denote the operation
          a = b + ((a + i(b,c,d) + x[k] + t[i]) <<< s). */
#define i(x, y, z) ((y) ^ ((x) | ~(z)))
#define set(a, b, c, d, k, s, ti)\
  t = a + i(b,c,d) + x[k] + ti;\
  a = rotate_left(t, s) + b
     /* do the following 16 operations. */
    set(a, b, c, d,  0,  6, t49);
    set(d, a, b, c,  7, 10, t50);
    set(c, d, a, b, 14, 15, t51);
    set(b, c, d, a,  5, 21, t52);
    set(a, b, c, d, 12,  6, t53);
    set(d, a, b, c,  3, 10, t54);
    set(c, d, a, b, 10, 15, t55);
    set(b, c, d, a,  1, 21, t56);
    set(a, b, c, d,  8,  6, t57);
    set(d, a, b, c, 15, 10, t58);
    set(c, d, a, b,  6, 15, t59);
    set(b, c, d, a, 13, 21, t60);
    set(a, b, c, d,  4,  6, t61);
    set(d, a, b, c, 11, 10, t62);
    set(c, d, a, b,  2, 15, t63);
    set(b, c, d, a,  9, 21, t64);
#undef set

     /* then perform the following additions. (that is increment each
        of the four registers by the value it had before this block
        was started.) */
    pms->abcd[0] += a;
    pms->abcd[1] += b;
    pms->abcd[2] += c;
    pms->abcd[3] += d;
}

void
md5_init(md5_state_t *pms)
{
    pms->count[0] = pms->count[1] = 0;
    pms->abcd[0] = 0x67452301;
    pms->abcd[1] = /*0xefcdab89*/ t_mask ^ 0x10325476;
    pms->abcd[2] = /*0x98badcfe*/ t_mask ^ 0x67452301;
    pms->abcd[3] = 0x10325476;
}

void
md5_append(md5_state_t *pms, const md5_byte_t *data, size_t nbytes)
{
    const md5_byte_t *p = data;
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
	int copy = (offset + nbytes > 64 ? 64 - offset : nbytes);

	memcpy(pms->buf + offset, p, copy);
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
	memcpy(pms->buf, p, left);
}

void
md5_finish(md5_state_t *pms, md5_byte_t digest[16])
{
    static const md5_byte_t pad[64] = {
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
