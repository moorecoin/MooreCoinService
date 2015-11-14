/*
 * file:	sha2.h
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
 * $id: sha2.h,v 1.1 2001/11/08 00:02:01 adg exp adg $
 */

#ifndef __sha2_h__
#define __sha2_h__

//#ifdef __cplusplus
//extern "c" {
//#endif

/*
 * import u_intxx_t size_t type definitions from system headers.  you
 * may need to change this, or define these things yourself in this
 * file.
 */
#include <sys/types.h>


/*** sha-256/384/512 various length definitions ***********************/
#define sha256_digest_string_length	(sha256::digestlength * 2 + 1)
#define sha384_block_length   128
#define sha384_digest_length   48
#define sha384_digest_string_length	(sha384_digest_length * 2 + 1)
#define sha512_block_length	 128
#define sha512_digest_length  64
#define sha512_digest_string_length	(sha512_digest_length * 2 + 1)

/*** sha-256/384/512 context structures *******************************/
typedef struct _sha512_ctx {
	std::uint64_t	state[8];
	std::uint64_t	bitcount[2];
	std::uint8_t	buffer[sha512_block_length];
} sha512_ctx;

typedef sha512_ctx sha384_ctx;


/*** sha-256/384/512 function prototypes ******************************/
#ifndef noproto

void sha256_init(sha256::detail::context *);
void sha256_update(sha256::detail::context*, const std::uint8_t*, size_t);
void sha256_final(std::uint8_t[sha256::digestlength], sha256::detail::context*);
char* sha256_end(sha256::detail::context*, char[sha256_digest_string_length]);
char* sha256_data(const std::uint8_t*, size_t, char[sha256_digest_string_length]);

void sha384_init(sha384_ctx*);
void sha384_update(sha384_ctx*, const std::uint8_t*, size_t);
void sha384_final(std::uint8_t[sha384_digest_length], sha384_ctx*);
char* sha384_end(sha384_ctx*, char[sha384_digest_string_length]);
char* sha384_data(const std::uint8_t*, size_t, char[sha384_digest_string_length]);

void sha512_init(sha512_ctx*);
void sha512_update(sha512_ctx*, const std::uint8_t*, size_t);
void sha512_final(std::uint8_t[sha512_digest_length], sha512_ctx*);
char* sha512_end(sha512_ctx*, char[sha512_digest_string_length]);
char* sha512_data(const std::uint8_t*, size_t, char[sha512_digest_string_length]);

#else /* noproto */

void sha256_init();
void sha256_update();
void sha256_final();
char* sha256_end();
char* sha256_data();

void sha384_init();
void sha384_update();
void sha384_final();
char* sha384_end();
char* sha384_data();

void sha512_init();
void sha512_update();
void sha512_final();
char* sha512_end();
char* sha512_data();

#endif /* noproto */

//#ifdef	__cplusplus
//}
//#endif /* __cplusplus */

#endif /* __sha2_h__ */

