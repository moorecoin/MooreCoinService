/*
 * this file is a part of pcompress, a chunked parallel multi-
 * algorithm lossless compression and decompression program.
 *
 * copyright (c) 2012-2013 moinak ghosh. all rights reserved.
 * use is subject to license terms.
 *
 * this program is free software; you can redistribute it and/or
 * modify it under the terms of the gnu lesser general public
 * license as published by the free software foundation; either
 * version 3 of the license, or (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose. see the gnu
 * lesser general public license for more details.
 *
 * you should have received a copy of the gnu lesser general public
 * license along with this program.
 * if not, see <http://www.gnu.org/licenses/>.
 *
 * moinakg@belenix.org, http://moinakg.wordpress.com/
 *      
 */

/*-
 * copyright (c) 2001-2003 allan saddi <allan@saddi.com>
 * copyright (c) 2012 moinak ghosh moinakg <at1> gm0il <dot> com
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
 *
 * this software is provided by the author and contributors ``as is'' and
 * any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose
 * are disclaimed.  in no event shall the author or contributors be liable
 * for any direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute goods
 * or services; loss of use, data, or profits; or business interruption)
 * however caused and on any theory of liability, whether in contract, strict
 * liability, or tort (including negligence or otherwise) arising in any way
 * out of the use of this software, even if advised of the possibility of
 * such damage.
 */

/*
 * define words_bigendian if compiling on a big-endian architecture.
 */
 
#ifdef have_config_h
#include <config.h>
#endif /* have_config_h */

#if have_inttypes_h
# include <inttypes.h>
#else
# if have_stdint_h
#  include <stdint.h>
# endif
#endif

#include <pthread.h>
#include <string.h>
#include "sha512asm.h"

//little endian only

#if defined(__mingw32__) || defined(__mingw64__)

static __inline unsigned short
bswap_16 (unsigned short __x)
{
  return (__x >> 8) | (__x << 8);
}

static __inline unsigned int
bswap_32 (unsigned int __x)
{
  return (bswap_16 (__x & 0xffff) << 16) | (bswap_16 (__x >> 16));
}

static __inline unsigned long long
bswap_64 (unsigned long long __x)
{
  return (((unsigned long long) bswap_32 (__x & 0xffffffffull)) << 32) | (bswap_32 (__x >> 32));
}

#define byteswap(x) bswap_32(x)
#define byteswap64(x) bswap_64(x)

#elif defined(__apple__)

#include <libkern/osbyteorder.h>

#define byteswap(x) osswapbigtohostint32(x)
#define byteswap64(x) osswapbigtohostint64(x)

#else

#include <endian.h> //glibc

#define byteswap(x) be32toh(x)
#define byteswap64(x) be64toh(x)

#endif /* defined(__mingw32__) || defined(__mingw64__) */

typedef void (*update_func_ptr)(const void *input_data, void *digest, uint64_t num_blks);

static const uint8_t padding[sha512asm_block_size] = {
  0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint64_t iv512[sha512asm_hash_words] = {
  0x6a09e667f3bcc908ll,
  0xbb67ae8584caa73bll,
  0x3c6ef372fe94f82bll,
  0xa54ff53a5f1d36f1ll,
  0x510e527fade682d1ll,
  0x9b05688c2b3e6c1fll,
  0x1f83d9abfb41bd6bll,
  0x5be0cd19137e2179ll
};

static const uint64_t iv256[sha512asm_hash_words] = {
  0x22312194fc2bf72cll,
  0x9f555fa3c84c64c2ll,
  0x2393b86b6f53b151ll,
  0x963877195940eabdll,
  0x96283ee2a88effe3ll,
  0xbe5e1e2553863992ll,
  0x2b0199fc2c85b8aall,
  0x0eb72ddc81c52ca2ll
};

static update_func_ptr sha512_update_func;

void
init_sha512asm_avx2 ()
{
	sha512_update_func = sha512_rorx;
}

void
init_sha512asm_avx ()
{
	sha512_update_func = sha512_avx;
}

void
init_sha512asm_sse4 ()
{
	sha512_update_func = sha512_sse4;
}

static void
_init (sha512asm_context *sc, const uint64_t iv[sha512asm_hash_words])
{
	int i;

	sc->totallength[0] = 0ll;
	sc->totallength[1] = 0ll;
	for (i = 0; i < sha512asm_hash_words; i++)
		sc->hash[i] = iv[i];
	sc->bufferlength = 0l;
}

void
sha512asm_init (sha512asm_context *sc)
{
	_init (sc, iv512);
}

void
sha512asm_update (sha512asm_context *sc, const void *vdata, size_t len)
{
	const uint8_t *data = (const uint8_t *)vdata;
	uint32_t bufferbytesleft;
	size_t bytestocopy;
	int rem;
	uint64_t carrycheck;

	if (sc->bufferlength) {
		do {
			bufferbytesleft = sha512asm_block_size - sc->bufferlength;
			bytestocopy = bufferbytesleft;
			if (bytestocopy > len)
				bytestocopy = len;

			memcpy (&sc->buffer.bytes[sc->bufferlength], data, bytestocopy);
			carrycheck = sc->totallength[1];
			sc->totallength[1] += bytestocopy * 8l;
			if (sc->totallength[1] < carrycheck)
				sc->totallength[0]++;

			sc->bufferlength += bytestocopy;
			data += bytestocopy;
			len -= bytestocopy;

			if (sc->bufferlength == sha512asm_block_size) {
				sc->blocks = 1;
				sha512_update_func(sc->buffer.words, sc->hash, sc->blocks);
				sc->bufferlength = 0l;
			} else {
				return;
			}
		} while (len > 0 && len <= sha512asm_block_size);
		if (!len) return;
	}
	sc->blocks = len >> 7;
	rem = len - (sc->blocks << 7);
	len = sc->blocks << 7;
	carrycheck = sc->totallength[1];
	sc->totallength[1] += rem * 8l;
	if (sc->totallength[1] < carrycheck)
		sc->totallength[0]++;

	if (len) {
		carrycheck = sc->totallength[1];
		sc->totallength[1] += len * 8l;
		if (sc->totallength[1] < carrycheck)
			sc->totallength[0]++;
		sha512_update_func((uint32_t *)data, sc->hash, sc->blocks);
	}
	if (rem) {
		memcpy (&sc->buffer.bytes[0], data + len, rem);
		sc->bufferlength = rem;
	}
}

static void
_final (sha512asm_context *sc, uint8_t *hash, int hashwords, int halfword)
{
	uint32_t bytestopad;
	uint64_t lengthpad[2];
	int i;
	
	bytestopad = 240l - sc->bufferlength;
	if (bytestopad > sha512asm_block_size)
		bytestopad -= sha512asm_block_size;
	
	lengthpad[0] = byteswap64(sc->totallength[0]);
	lengthpad[1] = byteswap64(sc->totallength[1]);
	
	sha512asm_update (sc, padding, bytestopad);
	sha512asm_update (sc, lengthpad, 16l);
	
	if (hash) {
		for (i = 0; i < hashwords; i++) {
			*((uint64_t *) hash) = byteswap64(sc->hash[i]);
			hash += 8;
		}
		if (halfword) {
			hash[0] = (uint8_t) (sc->hash[i] >> 56);
			hash[1] = (uint8_t) (sc->hash[i] >> 48);
			hash[2] = (uint8_t) (sc->hash[i] >> 40);
			hash[3] = (uint8_t) (sc->hash[i] >> 32);
		}
	}
}

void
sha512asm_final (sha512asm_context *sc, uint8_t hash[sha512asm_hash_size])
{
	_final (sc, hash, sha512asm_hash_words, 0);
}

unsigned char *sha512asm(const unsigned char *d, size_t n,unsigned char *md)
{
    sha512asm_context sc;
    sha512asm_init(&sc);
    sha512asm_update (&sc, d, n);
    sha512asm_final (&sc, md);
    return md;
}