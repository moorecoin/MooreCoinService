/*
 * file:	sha2.c
 * author:	aaron d. gifford - http://www.aarongifford.com/
 * 
 * copyright (c) 2000-2001, aaron d. gifford
 * all rights reserved.
 *
 * redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * this software is provided by the author and contributor(s) ``as is'' and
 * any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose
 * are disclaimed.  in no event shall the author or contributor(s) be liable
 * for any direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute goods
 * or services; loss of use, data, or profits; or business interruption)
 * however caused and on any theory of liability, whether in contract, strict
 * liability, or tort (including negligence or otherwise) arising in any way
 * out of the use of this software, even if advised of the possibility of
 * such damage.
 *
 */

#include <string.h>	/* memcpy()/memset() or bcopy()/bzero() */
#include <assert.h>	/* assert() */
#include "sha2.h"

/*
 * assert note:
 * some sanity checking code is included using assert().  on my freebsd
 * system, this additional code can be removed by compiling with ndebug
 * defined.  check your own systems manpage on assert() to see how to
 * compile without the sanity checking code on your system.
 *
 * unrolled transform loop note:
 * you can define sha2_unroll_transform to use the unrolled transform
 * loop version for the hash transform rounds (defined using macros
 * later in this file).  either define on the command line, for example:
 *
 *   cc -dsha2_unroll_transform -o sha2 sha2.c sha2prog.c
 *
 * or define below:
 *
 *   #define sha2_unroll_transform
 *
 */


/*** sha-256/384/512 machine architecture definitions *****************/
/*
 * byte_order note:
 *
 * please make sure that your system defines byte_order.  if your
 * architecture is little-endian, make sure it also defines
 * little_endian and that the two (byte_order and little_endian) are
 * equivilent.
 *
 * if your system does not define the above, then you can do so by
 * hand like this:
 *
 *   #define little_endian 1234
 *   #define big_endian    4321
 *
 * and for little-endian machines, add:
 *
 *   #define byte_order little_endian 
 *
 * or for big-endian machines:
 *
 *   #define byte_order big_endian
 *
 * the freebsd machine this was written on defines byte_order
 * appropriately by including <sys/types.h> (which in turn includes
 * <machine/endian.h> where the appropriate definitions are actually
 * made).
 */
#if !defined(byte_order) || (byte_order != little_endian && byte_order != big_endian)
#error define byte_order to be equal to either little_endian or big_endian
#endif

/*
 * define the followingsha2_* types to types of the correct length on
 * the native archtecture.   most bsd systems and linux define u_intxx_t
 * types.  machines with very recent ansi c headers, can use the
 * uintxx_t definintions from inttypes.h by defining sha2_use_inttypes_h
 * during compile or in the sha.h header file.
 *
 * machines that support neither u_intxx_t nor inttypes.h's uintxx_t
 * will need to define these three typedefs below (and the appropriate
 * ones in sha.h too) by hand according to their system architecture.
 *
 * thank you, jun-ichiro itojun hagino, for suggesting using u_intxx_t
 * types and pointing out recent ansi c support for uintxx_t in inttypes.h.
 */

typedef std::uint8_t  sha2_byte;	/* exactly 1 byte */
typedef std::uint32_t sha2_word32;	/* exactly 4 bytes */
typedef std::uint64_t sha2_word64;	/* exactly 8 bytes */

/*** sha-256/384/512 various length definitions ***********************/
/* note: most of these are in sha2.h */
#define sha256_short_block_length	(sha256::blocklength - 8)
#define sha384_short_block_length	(sha384_block_length - 16)
#define sha512_short_block_length	(sha512_block_length - 16)

/*** endian reversal macros *******************************************/
#if byte_order == little_endian
#define reverse32(w,x)	{ \
	sha2_word32 tmp = (w); \
	tmp = (tmp >> 16) | (tmp << 16); \
	(x) = ((tmp & 0xff00ff00ul) >> 8) | ((tmp & 0x00ff00fful) << 8); \
}
#define reverse64(w,x)	{ \
	sha2_word64 tmp = (w); \
	tmp = (tmp >> 32) | (tmp << 32); \
	tmp = ((tmp & 0xff00ff00ff00ff00ull) >> 8) | \
	      ((tmp & 0x00ff00ff00ff00ffull) << 8); \
	(x) = ((tmp & 0xffff0000ffff0000ull) >> 16) | \
	      ((tmp & 0x0000ffff0000ffffull) << 16); \
}
#endif /* byte_order == little_endian */

/*
 * macro for incrementally adding the unsigned 64-bit integer n to the
 * unsigned 128-bit integer (represented using a two-element array of
 * 64-bit words):
 */
#define addinc128(w,n)	{ \
	(w)[0] += (sha2_word64)(n); \
	if ((w)[0] < (n)) { \
		(w)[1]++; \
	} \
}

/*
 * macros for copying blocks of memory and for zeroing out ranges
 * of memory.  using these macros makes it easy to switch from
 * using memset()/memcpy() and using bzero()/bcopy().
 *
 * please define either sha2_use_memset_memcpy or define
 * sha2_use_bzero_bcopy depending on which function set you
 * choose to use:
 */
#if !defined(sha2_use_memset_memcpy) && !defined(sha2_use_bzero_bcopy)
/* default to memset()/memcpy() if no option is specified */
#define	sha2_use_memset_memcpy	1
#endif
#if defined(sha2_use_memset_memcpy) && defined(sha2_use_bzero_bcopy)
/* abort with an error if both options are defined */
#error define either sha2_use_memset_memcpy or sha2_use_bzero_bcopy, not both!
#endif

#ifdef sha2_use_memset_memcpy
#define memset_bzero(p,l)	memset((p), 0, (l))
#define memcpy_bcopy(d,s,l)	memcpy((d), (s), (l))
#endif
#ifdef sha2_use_bzero_bcopy
#define memset_bzero(p,l)	bzero((p), (l))
#define memcpy_bcopy(d,s,l)	bcopy((s), (d), (l))
#endif


/*** the six logical functions ****************************************/
/*
 * bit shifting and rotation (used by the six sha-xyz logical functions:
 *
 *   note:  the naming of r and s appears backwards here (r is a shift and
 *   s is a rotation) because the sha-256/384/512 description document
 *   (see http://csrc.nist.gov/cryptval/shs/sha256-384-512.pdf) uses this
 *   same "backwards" definition.
 */
/* shift-right (used in sha-256, sha-384, and sha-512): */
#define r(b,x) 		((x) >> (b))

/* 32-bit rotate-right (used in sha-256): */
#define s32(b,x)	(((x) >> (b)) | ((x) << (32 - (b))))
/* 64-bit rotate-right (used in sha-384 and sha-512): */
#define s64(b,x)	(((x) >> (b)) | ((x) << (64 - (b))))

/* two of six logical functions used in sha-256, sha-384, and sha-512: */
#define ch(x,y,z)	(((x) & (y)) ^ ((~(x)) & (z)))
#define maj(x,y,z)	(((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/* four of six logical functions used in sha-256: */
#define sigma0_256(x)	(s32(2,  (x)) ^ s32(13, (x)) ^ s32(22, (x)))
#define sigma1_256(x)	(s32(6,  (x)) ^ s32(11, (x)) ^ s32(25, (x)))
#define sigma0_256(x)	(s32(7,  (x)) ^ s32(18, (x)) ^ r(3 ,   (x)))
#define sigma1_256(x)	(s32(17, (x)) ^ s32(19, (x)) ^ r(10,   (x)))

/* four of six logical functions used in sha-384 and sha-512: */
#define sigma0_512(x)	(s64(28, (x)) ^ s64(34, (x)) ^ s64(39, (x)))
#define sigma1_512(x)	(s64(14, (x)) ^ s64(18, (x)) ^ s64(41, (x)))
#define sigma0_512(x)	(s64( 1, (x)) ^ s64( 8, (x)) ^ r( 7,   (x)))
#define sigma1_512(x)	(s64(19, (x)) ^ s64(61, (x)) ^ r( 6,   (x)))

/*** internal function prototypes *************************************/
/* note: these should not be accessed directly from outside this
 * library -- they are intended for private internal visibility/use
 * only.
 */
void sha512_last(sha512_ctx*);
void sha256_transform(sha256::detail::context*, const sha2_word32*);
void sha512_transform(sha512_ctx*, const sha2_word64*);


/*** sha-xyz initial hash values and constants ************************/
/* hash constant words k for sha-256: */
const static sha2_word32 k256[64] = {
	0x428a2f98ul, 0x71374491ul, 0xb5c0fbcful, 0xe9b5dba5ul,
	0x3956c25bul, 0x59f111f1ul, 0x923f82a4ul, 0xab1c5ed5ul,
	0xd807aa98ul, 0x12835b01ul, 0x243185beul, 0x550c7dc3ul,
	0x72be5d74ul, 0x80deb1feul, 0x9bdc06a7ul, 0xc19bf174ul,
	0xe49b69c1ul, 0xefbe4786ul, 0x0fc19dc6ul, 0x240ca1ccul,
	0x2de92c6ful, 0x4a7484aaul, 0x5cb0a9dcul, 0x76f988daul,
	0x983e5152ul, 0xa831c66dul, 0xb00327c8ul, 0xbf597fc7ul,
	0xc6e00bf3ul, 0xd5a79147ul, 0x06ca6351ul, 0x14292967ul,
	0x27b70a85ul, 0x2e1b2138ul, 0x4d2c6dfcul, 0x53380d13ul,
	0x650a7354ul, 0x766a0abbul, 0x81c2c92eul, 0x92722c85ul,
	0xa2bfe8a1ul, 0xa81a664bul, 0xc24b8b70ul, 0xc76c51a3ul,
	0xd192e819ul, 0xd6990624ul, 0xf40e3585ul, 0x106aa070ul,
	0x19a4c116ul, 0x1e376c08ul, 0x2748774cul, 0x34b0bcb5ul,
	0x391c0cb3ul, 0x4ed8aa4aul, 0x5b9cca4ful, 0x682e6ff3ul,
	0x748f82eeul, 0x78a5636ful, 0x84c87814ul, 0x8cc70208ul,
	0x90befffaul, 0xa4506cebul, 0xbef9a3f7ul, 0xc67178f2ul
};

/* initial hash value h for sha-256: */
const static sha2_word32 sha256_initial_hash_value[8] = {
	0x6a09e667ul,
	0xbb67ae85ul,
	0x3c6ef372ul,
	0xa54ff53aul,
	0x510e527ful,
	0x9b05688cul,
	0x1f83d9abul,
	0x5be0cd19ul
};

/* hash constant words k for sha-384 and sha-512: */
const static sha2_word64 k512[80] = {
	0x428a2f98d728ae22ull, 0x7137449123ef65cdull,
	0xb5c0fbcfec4d3b2full, 0xe9b5dba58189dbbcull,
	0x3956c25bf348b538ull, 0x59f111f1b605d019ull,
	0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull,
	0xd807aa98a3030242ull, 0x12835b0145706fbeull,
	0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull,
	0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull,
	0x9bdc06a725c71235ull, 0xc19bf174cf692694ull,
	0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull,
	0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull,
	0x2de92c6f592b0275ull, 0x4a7484aa6ea6e483ull,
	0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull,
	0x983e5152ee66dfabull, 0xa831c66d2db43210ull,
	0xb00327c898fb213full, 0xbf597fc7beef0ee4ull,
	0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull,
	0x06ca6351e003826full, 0x142929670a0e6e70ull,
	0x27b70a8546d22ffcull, 0x2e1b21385c26c926ull,
	0x4d2c6dfc5ac42aedull, 0x53380d139d95b3dfull,
	0x650a73548baf63deull, 0x766a0abb3c77b2a8ull,
	0x81c2c92e47edaee6ull, 0x92722c851482353bull,
	0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull,
	0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull,
	0xd192e819d6ef5218ull, 0xd69906245565a910ull,
	0xf40e35855771202aull, 0x106aa07032bbd1b8ull,
	0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull,
	0x2748774cdf8eeb99ull, 0x34b0bcb5e19b48a8ull,
	0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull,
	0x5b9cca4f7763e373ull, 0x682e6ff3d6b2b8a3ull,
	0x748f82ee5defb2fcull, 0x78a5636f43172f60ull,
	0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
	0x90befffa23631e28ull, 0xa4506cebde82bde9ull,
	0xbef9a3f7b2c67915ull, 0xc67178f2e372532bull,
	0xca273eceea26619cull, 0xd186b8c721c0c207ull,
	0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull,
	0x06f067aa72176fbaull, 0x0a637dc5a2c898a6ull,
	0x113f9804bef90daeull, 0x1b710b35131c471bull,
	0x28db77f523047d84ull, 0x32caab7b40c72493ull,
	0x3c9ebe0a15c9bebcull, 0x431d67c49c100d4cull,
	0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull,
	0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull
};

/* initial hash value h for sha-384 */
const static sha2_word64 sha384_initial_hash_value[8] = {
	0xcbbb9d5dc1059ed8ull,
	0x629a292a367cd507ull,
	0x9159015a3070dd17ull,
	0x152fecd8f70e5939ull,
	0x67332667ffc00b31ull,
	0x8eb44a8768581511ull,
	0xdb0c2e0d64f98fa7ull,
	0x47b5481dbefa4fa4ull
};

/* initial hash value h for sha-512 */
const static sha2_word64 sha512_initial_hash_value[8] = {
	0x6a09e667f3bcc908ull,
	0xbb67ae8584caa73bull,
	0x3c6ef372fe94f82bull,
	0xa54ff53a5f1d36f1ull,
	0x510e527fade682d1ull,
	0x9b05688c2b3e6c1full,
	0x1f83d9abfb41bd6bull,
	0x5be0cd19137e2179ull
};

/*
 * constant used by sha256/384/512_end() functions for converting the
 * digest to a readable hexadecimal character string:
 */
static const char *sha2_hex_digits = "0123456789abcdef";


/*** sha-256: *********************************************************/
void sha256_init(sha256::detail::context* context) {
	if (context == (sha256::detail::context*)0) {
		return;
	}
	memcpy_bcopy(context->state, sha256_initial_hash_value, sha256::digestlength);
	memset_bzero(context->buffer, sha256::blocklength);
	context->bitcount = 0;
}

#ifdef sha2_unroll_transform

/* unrolled sha-256 round macros: */

#if byte_order == little_endian

#define round256_0_to_15(a,b,c,d,e,f,g,h)	\
	reverse32(*data++, w256[j]); \
	t1 = (h) + sigma1_256(e) + ch((e), (f), (g)) + \
             k256[j] + w256[j]; \
	(d) += t1; \
	(h) = t1 + sigma0_256(a) + maj((a), (b), (c)); \
	j++


#else /* byte_order == little_endian */

#define round256_0_to_15(a,b,c,d,e,f,g,h)	\
	t1 = (h) + sigma1_256(e) + ch((e), (f), (g)) + \
	     k256[j] + (w256[j] = *data++); \
	(d) += t1; \
	(h) = t1 + sigma0_256(a) + maj((a), (b), (c)); \
	j++

#endif /* byte_order == little_endian */

#define round256(a,b,c,d,e,f,g,h)	\
	s0 = w256[(j+1)&0x0f]; \
	s0 = sigma0_256(s0); \
	s1 = w256[(j+14)&0x0f]; \
	s1 = sigma1_256(s1); \
	t1 = (h) + sigma1_256(e) + ch((e), (f), (g)) + k256[j] + \
	     (w256[j&0x0f] += s1 + w256[(j+9)&0x0f] + s0); \
	(d) += t1; \
	(h) = t1 + sigma0_256(a) + maj((a), (b), (c)); \
	j++

void sha256_transform(sha256::detail::context* context, const sha2_word32* data) {
	sha2_word32	a, b, c, d, e, f, g, h, s0, s1;
	sha2_word32	t1, *w256;
	int		j;

	w256 = (sha2_word32*)context->buffer;

	/* initialize registers with the prev. intermediate value */
	a = context->state[0];
	b = context->state[1];
	c = context->state[2];
	d = context->state[3];
	e = context->state[4];
	f = context->state[5];
	g = context->state[6];
	h = context->state[7];

	j = 0;
	do {
		/* rounds 0 to 15 (unrolled): */
		round256_0_to_15(a,b,c,d,e,f,g,h);
		round256_0_to_15(h,a,b,c,d,e,f,g);
		round256_0_to_15(g,h,a,b,c,d,e,f);
		round256_0_to_15(f,g,h,a,b,c,d,e);
		round256_0_to_15(e,f,g,h,a,b,c,d);
		round256_0_to_15(d,e,f,g,h,a,b,c);
		round256_0_to_15(c,d,e,f,g,h,a,b);
		round256_0_to_15(b,c,d,e,f,g,h,a);
	} while (j < 16);

	/* now for the remaining rounds to 64: */
	do {
		round256(a,b,c,d,e,f,g,h);
		round256(h,a,b,c,d,e,f,g);
		round256(g,h,a,b,c,d,e,f);
		round256(f,g,h,a,b,c,d,e);
		round256(e,f,g,h,a,b,c,d);
		round256(d,e,f,g,h,a,b,c);
		round256(c,d,e,f,g,h,a,b);
		round256(b,c,d,e,f,g,h,a);
	} while (j < 64);

	/* compute the current intermediate hash value */
	context->state[0] += a;
	context->state[1] += b;
	context->state[2] += c;
	context->state[3] += d;
	context->state[4] += e;
	context->state[5] += f;
	context->state[6] += g;
	context->state[7] += h;

	/* clean up */
	a = b = c = d = e = f = g = h = t1 = 0;
}

#else /* sha2_unroll_transform */

void sha256_transform(sha256::detail::context* context, const sha2_word32* data) {
	sha2_word32	a, b, c, d, e, f, g, h, s0, s1;
	sha2_word32	t1, t2, *w256;
	int		j;

	w256 = (sha2_word32*)context->buffer;

	/* initialize registers with the prev. intermediate value */
	a = context->state[0];
	b = context->state[1];
	c = context->state[2];
	d = context->state[3];
	e = context->state[4];
	f = context->state[5];
	g = context->state[6];
	h = context->state[7];

	j = 0;
	do {
#if byte_order == little_endian
		/* copy data while converting to host byte order */
		reverse32(*data++,w256[j]);
		/* apply the sha-256 compression function to update a..h */
		t1 = h + sigma1_256(e) + ch(e, f, g) + k256[j] + w256[j];
#else /* byte_order == little_endian */
		/* apply the sha-256 compression function to update a..h with copy */
		t1 = h + sigma1_256(e) + ch(e, f, g) + k256[j] + (w256[j] = *data++);
#endif /* byte_order == little_endian */
		t2 = sigma0_256(a) + maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;

		j++;
	} while (j < 16);

	do {
		/* part of the message block expansion: */
		s0 = w256[(j+1)&0x0f];
		s0 = sigma0_256(s0);
		s1 = w256[(j+14)&0x0f];	
		s1 = sigma1_256(s1);

		/* apply the sha-256 compression function to update a..h */
		t1 = h + sigma1_256(e) + ch(e, f, g) + k256[j] + 
		     (w256[j&0x0f] += s1 + w256[(j+9)&0x0f] + s0);
		t2 = sigma0_256(a) + maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;

		j++;
	} while (j < 64);

	/* compute the current intermediate hash value */
	context->state[0] += a;
	context->state[1] += b;
	context->state[2] += c;
	context->state[3] += d;
	context->state[4] += e;
	context->state[5] += f;
	context->state[6] += g;
	context->state[7] += h;

	/* clean up */
	a = b = c = d = e = f = g = h = t1 = t2 = 0;
}

#endif /* sha2_unroll_transform */

void sha256_update(sha256::detail::context* context, const sha2_byte *data, size_t len) {
	unsigned int	freespace, usedspace;

	if (len == 0) {
		/* calling with no data is valid - we do nothing */
		return;
	}

	/* sanity check: */
	assert(context != (sha256::detail::context*)0 && data != (sha2_byte*)0);

	usedspace = (context->bitcount >> 3) % sha256::blocklength;
	if (usedspace > 0) {
		/* calculate how much free space is available in the buffer */
		freespace = sha256::blocklength - usedspace;

		if (len >= freespace) {
			/* fill the buffer completely and process it */
			memcpy_bcopy(&context->buffer[usedspace], data, freespace);
			context->bitcount += freespace << 3;
			len -= freespace;
			data += freespace;
			sha256_transform(context, (sha2_word32*)context->buffer);
		} else {
			/* the buffer is not yet full */
			memcpy_bcopy(&context->buffer[usedspace], data, len);
			context->bitcount += len << 3;
			/* clean up: */
			usedspace = freespace = 0;
			return;
		}
	}
	while (len >= sha256::blocklength) {
		/* process as many complete blocks as we can */
		sha256_transform(context, (sha2_word32*)data);
		context->bitcount += sha256::blocklength << 3;
		len -= sha256::blocklength;
		data += sha256::blocklength;
	}
	if (len > 0) {
		/* there's left-overs, so save 'em */
		memcpy_bcopy(context->buffer, data, len);
		context->bitcount += len << 3;
	}
	/* clean up: */
	usedspace = freespace = 0;
}

void sha256_final(sha2_byte digest[], sha256::detail::context* context) {
	sha2_word32	*d = (sha2_word32*)digest;
	unsigned int	usedspace;

	/* sanity check: */
	assert(context != (sha256::detail::context*)0);

	/* if no digest buffer is passed, we don't bother doing this: */
	if (digest != (sha2_byte*)0) {
		usedspace = (context->bitcount >> 3) % sha256::blocklength;
#if byte_order == little_endian
		/* convert from host byte order */
		reverse64(context->bitcount,context->bitcount);
#endif
		if (usedspace > 0) {
			/* begin padding with a 1 bit: */
			context->buffer[usedspace++] = 0x80;

			if (usedspace <= sha256_short_block_length) {
				/* set-up for the last transform: */
				memset_bzero(&context->buffer[usedspace], sha256_short_block_length - usedspace);
			} else {
				if (usedspace < sha256::blocklength) {
					memset_bzero(&context->buffer[usedspace], sha256::blocklength - usedspace);
				}
				/* do second-to-last transform: */
				sha256_transform(context, (sha2_word32*)context->buffer);

				/* and set-up for the last transform: */
				memset_bzero(context->buffer, sha256_short_block_length);
			}
		} else {
			/* set-up for the last transform: */
			memset_bzero(context->buffer, sha256_short_block_length);

			/* begin padding with a 1 bit: */
			*context->buffer = 0x80;
		}
		/* set the bit count: */
		*(sha2_word64*)&context->buffer[sha256_short_block_length] = context->bitcount;

		/* final transform: */
		sha256_transform(context, (sha2_word32*)context->buffer);

#if byte_order == little_endian
		{
			/* convert to host byte order */
			int	j;
			for (j = 0; j < 8; j++) {
				reverse32(context->state[j],context->state[j]);
				*d++ = context->state[j];
			}
		}
#else
		memcpy_bcopy(d, context->state, sha256::digestlength);
#endif
	}

	/* clean up state data: */
	memset_bzero(context, sizeof(sha256::detail::context));
	usedspace = 0;
}

char *sha256_end(sha256::detail::context* context, char buffer[]) {
	sha2_byte	digest[sha256::digestlength], *d = digest;
	int		i;

	/* sanity check: */
	assert(context != (sha256::detail::context*)0);

	if (buffer != (char*)0) {
		sha256_final(digest, context);

		for (i = 0; i < sha256::digestlength; i++) {
			*buffer++ = sha2_hex_digits[(*d & 0xf0) >> 4];
			*buffer++ = sha2_hex_digits[*d & 0x0f];
			d++;
		}
		*buffer = (char)0;
	} else {
		memset_bzero(context, sizeof(sha256::detail::context));
	}
	memset_bzero(digest, sha256::digestlength);
	return buffer;
}

char* sha256_data(const sha2_byte* data, size_t len, char digest[sha256_digest_string_length]) {
	sha256::detail::context	context;

	sha256_init(&context);
	sha256_update(&context, data, len);
	return sha256_end(&context, digest);
}


/*** sha-512: *********************************************************/
void sha512_init(sha512_ctx* context) {
	if (context == (sha512_ctx*)0) {
		return;
	}
	memcpy_bcopy(context->state, sha512_initial_hash_value, sha512_digest_length);
	memset_bzero(context->buffer, sha512_block_length);
	context->bitcount[0] = context->bitcount[1] =  0;
}

#ifdef sha2_unroll_transform

/* unrolled sha-512 round macros: */
#if byte_order == little_endian

#define round512_0_to_15(a,b,c,d,e,f,g,h)	\
	reverse64(*data++, w512[j]); \
	t1 = (h) + sigma1_512(e) + ch((e), (f), (g)) + \
             k512[j] + w512[j]; \
	(d) += t1, \
	(h) = t1 + sigma0_512(a) + maj((a), (b), (c)), \
	j++


#else /* byte_order == little_endian */

#define round512_0_to_15(a,b,c,d,e,f,g,h)	\
	t1 = (h) + sigma1_512(e) + ch((e), (f), (g)) + \
             k512[j] + (w512[j] = *data++); \
	(d) += t1; \
	(h) = t1 + sigma0_512(a) + maj((a), (b), (c)); \
	j++

#endif /* byte_order == little_endian */

#define round512(a,b,c,d,e,f,g,h)	\
	s0 = w512[(j+1)&0x0f]; \
	s0 = sigma0_512(s0); \
	s1 = w512[(j+14)&0x0f]; \
	s1 = sigma1_512(s1); \
	t1 = (h) + sigma1_512(e) + ch((e), (f), (g)) + k512[j] + \
             (w512[j&0x0f] += s1 + w512[(j+9)&0x0f] + s0); \
	(d) += t1; \
	(h) = t1 + sigma0_512(a) + maj((a), (b), (c)); \
	j++

void sha512_transform(sha512_ctx* context, const sha2_word64* data) {
	sha2_word64	a, b, c, d, e, f, g, h, s0, s1;
	sha2_word64	t1, *w512 = (sha2_word64*)context->buffer;
	int		j;

	/* initialize registers with the prev. intermediate value */
	a = context->state[0];
	b = context->state[1];
	c = context->state[2];
	d = context->state[3];
	e = context->state[4];
	f = context->state[5];
	g = context->state[6];
	h = context->state[7];

	j = 0;
	do {
		round512_0_to_15(a,b,c,d,e,f,g,h);
		round512_0_to_15(h,a,b,c,d,e,f,g);
		round512_0_to_15(g,h,a,b,c,d,e,f);
		round512_0_to_15(f,g,h,a,b,c,d,e);
		round512_0_to_15(e,f,g,h,a,b,c,d);
		round512_0_to_15(d,e,f,g,h,a,b,c);
		round512_0_to_15(c,d,e,f,g,h,a,b);
		round512_0_to_15(b,c,d,e,f,g,h,a);
	} while (j < 16);

	/* now for the remaining rounds up to 79: */
	do {
		round512(a,b,c,d,e,f,g,h);
		round512(h,a,b,c,d,e,f,g);
		round512(g,h,a,b,c,d,e,f);
		round512(f,g,h,a,b,c,d,e);
		round512(e,f,g,h,a,b,c,d);
		round512(d,e,f,g,h,a,b,c);
		round512(c,d,e,f,g,h,a,b);
		round512(b,c,d,e,f,g,h,a);
	} while (j < 80);

	/* compute the current intermediate hash value */
	context->state[0] += a;
	context->state[1] += b;
	context->state[2] += c;
	context->state[3] += d;
	context->state[4] += e;
	context->state[5] += f;
	context->state[6] += g;
	context->state[7] += h;

	/* clean up */
	a = b = c = d = e = f = g = h = t1 = 0;
}

#else /* sha2_unroll_transform */

void sha512_transform(sha512_ctx* context, const sha2_word64* data) {
	sha2_word64	a, b, c, d, e, f, g, h, s0, s1;
	sha2_word64	t1, t2, *w512 = (sha2_word64*)context->buffer;
	int		j;

	/* initialize registers with the prev. intermediate value */
	a = context->state[0];
	b = context->state[1];
	c = context->state[2];
	d = context->state[3];
	e = context->state[4];
	f = context->state[5];
	g = context->state[6];
	h = context->state[7];

	j = 0;
	do {
#if byte_order == little_endian
		/* convert to host byte order */
		reverse64(*data++, w512[j]);
		/* apply the sha-512 compression function to update a..h */
		t1 = h + sigma1_512(e) + ch(e, f, g) + k512[j] + w512[j];
#else /* byte_order == little_endian */
		/* apply the sha-512 compression function to update a..h with copy */
		t1 = h + sigma1_512(e) + ch(e, f, g) + k512[j] + (w512[j] = *data++);
#endif /* byte_order == little_endian */
		t2 = sigma0_512(a) + maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;

		j++;
	} while (j < 16);

	do {
		/* part of the message block expansion: */
		s0 = w512[(j+1)&0x0f];
		s0 = sigma0_512(s0);
		s1 = w512[(j+14)&0x0f];
		s1 =  sigma1_512(s1);

		/* apply the sha-512 compression function to update a..h */
		t1 = h + sigma1_512(e) + ch(e, f, g) + k512[j] +
		     (w512[j&0x0f] += s1 + w512[(j+9)&0x0f] + s0);
		t2 = sigma0_512(a) + maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;

		j++;
	} while (j < 80);

	/* compute the current intermediate hash value */
	context->state[0] += a;
	context->state[1] += b;
	context->state[2] += c;
	context->state[3] += d;
	context->state[4] += e;
	context->state[5] += f;
	context->state[6] += g;
	context->state[7] += h;

	/* clean up */
	a = b = c = d = e = f = g = h = t1 = t2 = 0;
}

#endif /* sha2_unroll_transform */

void sha512_update(sha512_ctx* context, const sha2_byte *data, size_t len) {
	unsigned int	freespace, usedspace;

	if (len == 0) {
		/* calling with no data is valid - we do nothing */
		return;
	}

	/* sanity check: */
	assert(context != (sha512_ctx*)0 && data != (sha2_byte*)0);

	usedspace = (context->bitcount[0] >> 3) % sha512_block_length;
	if (usedspace > 0) {
		/* calculate how much free space is available in the buffer */
		freespace = sha512_block_length - usedspace;

		if (len >= freespace) {
			/* fill the buffer completely and process it */
			memcpy_bcopy(&context->buffer[usedspace], data, freespace);
			addinc128(context->bitcount, freespace << 3);
			len -= freespace;
			data += freespace;
			sha512_transform(context, (sha2_word64*)context->buffer);
		} else {
			/* the buffer is not yet full */
			memcpy_bcopy(&context->buffer[usedspace], data, len);
			addinc128(context->bitcount, len << 3);
			/* clean up: */
			usedspace = freespace = 0;
			return;
		}
	}
	while (len >= sha512_block_length) {
		/* process as many complete blocks as we can */
		sha512_transform(context, (sha2_word64*)data);
		addinc128(context->bitcount, sha512_block_length << 3);
		len -= sha512_block_length;
		data += sha512_block_length;
	}
	if (len > 0) {
		/* there's left-overs, so save 'em */
		memcpy_bcopy(context->buffer, data, len);
		addinc128(context->bitcount, len << 3);
	}
	/* clean up: */
	usedspace = freespace = 0;
}

void sha512_last(sha512_ctx* context) {
	unsigned int	usedspace;

	usedspace = (context->bitcount[0] >> 3) % sha512_block_length;
#if byte_order == little_endian
	/* convert from host byte order */
	reverse64(context->bitcount[0],context->bitcount[0]);
	reverse64(context->bitcount[1],context->bitcount[1]);
#endif
	if (usedspace > 0) {
		/* begin padding with a 1 bit: */
		context->buffer[usedspace++] = 0x80;

		if (usedspace <= sha512_short_block_length) {
			/* set-up for the last transform: */
			memset_bzero(&context->buffer[usedspace], sha512_short_block_length - usedspace);
		} else {
			if (usedspace < sha512_block_length) {
				memset_bzero(&context->buffer[usedspace], sha512_block_length - usedspace);
			}
			/* do second-to-last transform: */
			sha512_transform(context, (sha2_word64*)context->buffer);

			/* and set-up for the last transform: */
			memset_bzero(context->buffer, sha512_block_length - 2);
		}
	} else {
		/* prepare for final transform: */
		memset_bzero(context->buffer, sha512_short_block_length);

		/* begin padding with a 1 bit: */
		*context->buffer = 0x80;
	}
	/* store the length of input data (in bits): */
	*(sha2_word64*)&context->buffer[sha512_short_block_length] = context->bitcount[1];
	*(sha2_word64*)&context->buffer[sha512_short_block_length+8] = context->bitcount[0];

	/* final transform: */
	sha512_transform(context, (sha2_word64*)context->buffer);
}

void sha512_final(sha2_byte digest[], sha512_ctx* context) {
	sha2_word64	*d = (sha2_word64*)digest;

	/* sanity check: */
	assert(context != (sha512_ctx*)0);

	/* if no digest buffer is passed, we don't bother doing this: */
	if (digest != (sha2_byte*)0) {
		sha512_last(context);

		/* save the hash data for output: */
#if byte_order == little_endian
		{
			/* convert to host byte order */
			int	j;
			for (j = 0; j < 8; j++) {
				reverse64(context->state[j],context->state[j]);
				*d++ = context->state[j];
			}
		}
#else
		memcpy_bcopy(d, context->state, sha512_digest_length);
#endif
	}

	/* zero out state data */
	memset_bzero(context, sizeof(sha512_ctx));
}

char *sha512_end(sha512_ctx* context, char buffer[]) {
	sha2_byte	digest[sha512_digest_length], *d = digest;
	int		i;

	/* sanity check: */
	assert(context != (sha512_ctx*)0);

	if (buffer != (char*)0) {
		sha512_final(digest, context);

		for (i = 0; i < sha512_digest_length; i++) {
			*buffer++ = sha2_hex_digits[(*d & 0xf0) >> 4];
			*buffer++ = sha2_hex_digits[*d & 0x0f];
			d++;
		}
		*buffer = (char)0;
	} else {
		memset_bzero(context, sizeof(sha512_ctx));
	}
	memset_bzero(digest, sha512_digest_length);
	return buffer;
}

char* sha512_data(const sha2_byte* data, size_t len, char digest[sha512_digest_string_length]) {
	sha512_ctx	context;

	sha512_init(&context);
	sha512_update(&context, data, len);
	return sha512_end(&context, digest);
}


/*** sha-384: *********************************************************/
void sha384_init(sha384_ctx* context) {
	if (context == (sha384_ctx*)0) {
		return;
	}
	memcpy_bcopy(context->state, sha384_initial_hash_value, sha512_digest_length);
	memset_bzero(context->buffer, sha384_block_length);
	context->bitcount[0] = context->bitcount[1] = 0;
}

void sha384_update(sha384_ctx* context, const sha2_byte* data, size_t len) {
	sha512_update((sha512_ctx*)context, data, len);
}

void sha384_final(sha2_byte digest[], sha384_ctx* context) {
	sha2_word64	*d = (sha2_word64*)digest;

	/* sanity check: */
	assert(context != (sha384_ctx*)0);

	/* if no digest buffer is passed, we don't bother doing this: */
	if (digest != (sha2_byte*)0) {
		sha512_last((sha512_ctx*)context);

		/* save the hash data for output: */
#if byte_order == little_endian
		{
			/* convert to host byte order */
			int	j;
			for (j = 0; j < 6; j++) {
				reverse64(context->state[j],context->state[j]);
				*d++ = context->state[j];
			}
		}
#else
		memcpy_bcopy(d, context->state, sha384_digest_length);
#endif
	}

	/* zero out state data */
	memset_bzero(context, sizeof(sha384_ctx));
}

char *sha384_end(sha384_ctx* context, char buffer[]) {
	sha2_byte	digest[sha384_digest_length], *d = digest;
	int		i;

	/* sanity check: */
	assert(context != (sha384_ctx*)0);

	if (buffer != (char*)0) {
		sha384_final(digest, context);

		for (i = 0; i < sha384_digest_length; i++) {
			*buffer++ = sha2_hex_digits[(*d & 0xf0) >> 4];
			*buffer++ = sha2_hex_digits[*d & 0x0f];
			d++;
		}
		*buffer = (char)0;
	} else {
		memset_bzero(context, sizeof(sha384_ctx));
	}
	memset_bzero(digest, sha384_digest_length);
	return buffer;
}

char* sha384_data(const sha2_byte* data, size_t len, char digest[sha384_digest_string_length]) {
	sha384_ctx	context;

	sha384_init(&context);
	sha384_update(&context, data, len);
	return sha384_end(&context, digest);
}

#undef r
#undef s32
#undef s64
#undef ch
#undef maj
#undef sigma0_256
#undef sigma1_256
#undef sigma0_256
#undef sigma1_256
#undef sigma0_512
#undef sigma1_512
#undef sigma0_512
#undef sigma1_512
