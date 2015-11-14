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

#ifndef md5_included
#  define md5_included

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

typedef unsigned char md5_byte_t; /* 8-bit byte */
typedef unsigned int md5_word_t; /* 32-bit word */

/* define the state of the md5 algorithm. */
typedef struct md5_state_s {
    md5_word_t count[2];	/* message length in bits, lsw first */
    md5_word_t abcd[4];		/* digest buffer */
    md5_byte_t buf[64];		/* accumulate block */
} md5_state_t;

#ifdef __cplusplus
extern "c" 
{
#endif

/* initialize the algorithm. */
void md5_init(md5_state_t *pms);

/* append a string to the message. */
void md5_append(md5_state_t *pms, const md5_byte_t *data, size_t nbytes);

/* finish the message and return the digest. */
void md5_finish(md5_state_t *pms, md5_byte_t digest[16]);

#ifdef __cplusplus
}  /* end extern "c" */
#endif

#endif /* md5_included */
