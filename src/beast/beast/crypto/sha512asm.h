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

#ifndef _aps_sha512asm_h
#define _aps_sha512asm_h

#include <stdint.h>

#define	sha512asm_hash_size		64
#define	sha512asm_block_size		128l

/* hash size in 64-bit words */
#define sha512asm_hash_words 8

typedef struct _sha512asm_context {
  uint64_t totallength[2], blocks;
  uint64_t hash[sha512asm_hash_words];
  uint32_t bufferlength;
  union {
    uint64_t words[sha512asm_block_size/8];
    uint8_t bytes[sha512asm_block_size];
  } buffer;
} sha512asm_context;

#ifdef __cplusplus
extern "c" {
#endif

void init_sha512asm_avx2();
void init_sha512asm_avx();
void init_sha512asm_sse4();

void sha512asm_init (sha512asm_context *sc);
void sha512asm_update (sha512asm_context *sc, const void *data, size_t len);
void sha512asm_final (sha512asm_context *sc, uint8_t hash[sha512asm_hash_size]);

unsigned char *sha512asm(const unsigned char *d, size_t n,unsigned char *md);
    
/*
 * intel's optimized sha512 core routines. these routines are described in an
 * intel white-paper:
 * "fast sha-512 implementations on intel architecture processors"
 * note: works on amd bulldozer and later as well.
 */
extern void sha512_sse4(const void *input_data, void *digest, uint64_t num_blks);
extern void sha512_avx(const void *input_data, void *digest, uint64_t num_blks);
extern void sha512_rorx(const void *input_data, void *digest, uint64_t num_blks);

#ifdef __cplusplus
}
#endif

#endif /* !_aps_sha512asm_h */
