/*
	public domain by andrew m. <liquidsun@gmail.com>
	modified from the amd64-51-30k implementation by
		daniel j. bernstein
		niels duif
		tanja lange
		peter schwabe
		bo-yin yang
*/


#include "ed25519-donna-portable.h"

#if defined(ed25519_sse2)
#else
	#if defined(have_uint128) && !defined(ed25519_force_32bit)
		#define ed25519_64bit
	#else
		#define ed25519_32bit
	#endif
#endif

#if !defined(ed25519_no_inline_asm)
	/* detect extra features first so un-needed functions can be disabled throughout */
	#if defined(ed25519_sse2)
		#if defined(compiler_gcc) && defined(cpu_x86)
			#define ed25519_gcc_32bit_sse_choose
		#elif defined(compiler_gcc) && defined(cpu_x86_64)
			#define ed25519_gcc_64bit_sse_choose
		#endif
	#else
		#if defined(cpu_x86_64)
			#if defined(compiler_gcc) 
				#if defined(ed25519_64bit)
					#define ed25519_gcc_64bit_x86_choose
				#else
					#define ed25519_gcc_64bit_32bit_choose
				#endif
			#endif
		#endif
	#endif
#endif

#if defined(ed25519_sse2)
	#include "curve25519-donna-sse2.h"
#elif defined(ed25519_64bit)
	#include "curve25519-donna-64bit.h"
#else
	#include "curve25519-donna-32bit.h"
#endif

#include "curve25519-donna-helpers.h"

/* separate uint128 check for 64 bit sse2 */
#if defined(have_uint128) && !defined(ed25519_force_32bit)
	#include "modm-donna-64bit.h"
#else
	#include "modm-donna-32bit.h"
#endif

typedef unsigned char hash_512bits[64];

/*
	timing safe memory compare
*/
static int
ed25519_verify(const unsigned char *x, const unsigned char *y, size_t len) {
	size_t differentbits = 0;
	while (len--)
		differentbits |= (*x++ ^ *y++);
	return (int) (1 & ((differentbits - 1) >> 8));
}


/*
 * arithmetic on the twisted edwards curve -x^2 + y^2 = 1 + dx^2y^2
 * with d = -(121665/121666) = 37095705934669439343138083508754565189542113879843219016388785533085940283555
 * base point: (15112221349535400772501151409588531511454012693041857206046113283949847762202,46316835694926478169428394003475163141307993866256225615783033603165251855960);
 */

typedef struct ge25519_t {
	bignum25519 x, y, z, t;
} ge25519;

typedef struct ge25519_p1p1_t {
	bignum25519 x, y, z, t;
} ge25519_p1p1;

typedef struct ge25519_niels_t {
	bignum25519 ysubx, xaddy, t2d;
} ge25519_niels;

typedef struct ge25519_pniels_t {
	bignum25519 ysubx, xaddy, z, t2d;
} ge25519_pniels;

#include "ed25519-donna-basepoint-table.h"

#if defined(ed25519_64bit)
	#include "ed25519-donna-64bit-tables.h"
	#include "ed25519-donna-64bit-x86.h"
#else
	#include "ed25519-donna-32bit-tables.h"
	#include "ed25519-donna-64bit-x86-32bit.h"
#endif


#if defined(ed25519_sse2)
	#include "ed25519-donna-32bit-sse2.h"
	#include "ed25519-donna-64bit-sse2.h"
	#include "ed25519-donna-impl-sse2.h"
#else
	#include "ed25519-donna-impl-base.h"
#endif

