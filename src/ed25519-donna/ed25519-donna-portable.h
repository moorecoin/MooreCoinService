#include "ed25519-donna-portable-identify.h"

#define mul32x32_64(a,b) (((uint64_t)(a))*(b))

/* platform */
#if defined(compiler_msvc)
	#include <intrin.h>
	#if !defined(_debug)
		#undef mul32x32_64		
		#define mul32x32_64(a,b) __emulu(a,b)
	#endif
	#undef inline
	#define inline __forceinline
	#define donna_inline __forceinline
	#define donna_noinline __declspec(noinline)
	#define align(x) __declspec(align(x))
	#define rotl32(a,b) _rotl(a,b)
	#define rotr32(a,b) _rotr(a,b)
#else
	#include <sys/param.h>
	#define donna_inline inline __attribute__((always_inline))
	#define donna_noinline __attribute__((noinline))
	#define align(x) __attribute__((aligned(x)))
	#define rotl32(a,b) (((a) << (b)) | ((a) >> (32 - b)))
	#define rotr32(a,b) (((a) >> (b)) | ((a) << (32 - b)))
#endif

/* uint128_t */
#if defined(cpu_64bits) && !defined(ed25519_force_32bit)
	#if defined(compiler_clang) && (compiler_clang >= 30100)
		#define have_native_uint128
		typedef unsigned __int128 uint128_t;
	#elif defined(compiler_msvc)
		#define have_uint128
		typedef struct uint128_t {
			uint64_t lo, hi;
		} uint128_t;
		#define mul64x64_128(out,a,b) out.lo = _umul128(a,b,&out.hi);
		#define shr128_pair(out,hi,lo,shift) out = __shiftright128(lo, hi, shift);
		#define shl128_pair(out,hi,lo,shift) out = __shiftleft128(lo, hi, shift);
		#define shr128(out,in,shift) shr128_pair(out, in.hi, in.lo, shift)
		#define shl128(out,in,shift) shl128_pair(out, in.hi, in.lo, shift)
		#define add128(a,b) { uint64_t p = a.lo; a.lo += b.lo; a.hi += b.hi + (a.lo < p); }
		#define add128_64(a,b) { uint64_t p = a.lo; a.lo += b; a.hi += (a.lo < p); }
		#define lo128(a) (a.lo)
		#define hi128(a) (a.hi)
	#elif defined(compiler_gcc) && !defined(have_native_uint128)
		#if defined(__sizeof_int128__)
			#define have_native_uint128
			typedef unsigned __int128 uint128_t;
		#elif (compiler_gcc >= 40400)
			#define have_native_uint128
			typedef unsigned uint128_t __attribute__((mode(ti)));
		#elif defined(cpu_x86_64)
			#define have_uint128
			typedef struct uint128_t {
				uint64_t lo, hi;
			} uint128_t;
			#define mul64x64_128(out,a,b) __asm__ ("mulq %3" : "=a" (out.lo), "=d" (out.hi) : "a" (a), "rm" (b));
			#define shr128_pair(out,hi,lo,shift) __asm__ ("shrdq %2,%1,%0" : "+r" (lo) : "r" (hi), "j" (shift)); out = lo;
			#define shl128_pair(out,hi,lo,shift) __asm__ ("shldq %2,%1,%0" : "+r" (hi) : "r" (lo), "j" (shift)); out = hi;
			#define shr128(out,in,shift) shr128_pair(out,in.hi, in.lo, shift)
			#define shl128(out,in,shift) shl128_pair(out,in.hi, in.lo, shift)
			#define add128(a,b) __asm__ ("addq %4,%2; adcq %5,%3" : "=r" (a.hi), "=r" (a.lo) : "1" (a.lo), "0" (a.hi), "rm" (b.lo), "rm" (b.hi) : "cc");
			#define add128_64(a,b) __asm__ ("addq %4,%2; adcq $0,%3" : "=r" (a.hi), "=r" (a.lo) : "1" (a.lo), "0" (a.hi), "rm" (b) : "cc");
			#define lo128(a) (a.lo)
			#define hi128(a) (a.hi)
		#endif
	#endif

	#if defined(have_native_uint128)
		#define have_uint128
		#define mul64x64_128(out,a,b) out = (uint128_t)a * b;
		#define shr128_pair(out,hi,lo,shift) out = (uint64_t)((((uint128_t)hi << 64) | lo) >> (shift));
		#define shl128_pair(out,hi,lo,shift) out = (uint64_t)(((((uint128_t)hi << 64) | lo) << (shift)) >> 64);
		#define shr128(out,in,shift) out = (uint64_t)(in >> (shift));
		#define shl128(out,in,shift) out = (uint64_t)((in << shift) >> 64);
		#define add128(a,b) a += b;
		#define add128_64(a,b) a += (uint64_t)b;
		#define lo128(a) ((uint64_t)a)
		#define hi128(a) ((uint64_t)(a >> 64))
	#endif

	#if !defined(have_uint128)
		#error need a uint128_t implementation!
	#endif
#endif

/* endian */
#if !defined(ed25519_opensslrng)
static inline void u32to8_le(unsigned char *p, const uint32_t v) {
	p[0] = (unsigned char)(v      );
	p[1] = (unsigned char)(v >>  8);
	p[2] = (unsigned char)(v >> 16);
	p[3] = (unsigned char)(v >> 24);
}
#endif

#if !defined(have_uint128)
static inline uint32_t u8to32_le(const unsigned char *p) {
	return
	(((uint32_t)(p[0])      ) | 
	 ((uint32_t)(p[1]) <<  8) |
	 ((uint32_t)(p[2]) << 16) |
	 ((uint32_t)(p[3]) << 24));
}
#else
static inline uint64_t u8to64_le(const unsigned char *p) {
	return
	(((uint64_t)(p[0])      ) |
	 ((uint64_t)(p[1]) <<  8) |
	 ((uint64_t)(p[2]) << 16) |
	 ((uint64_t)(p[3]) << 24) |
	 ((uint64_t)(p[4]) << 32) |
	 ((uint64_t)(p[5]) << 40) |
	 ((uint64_t)(p[6]) << 48) |
	 ((uint64_t)(p[7]) << 56));
}

static inline void u64to8_le(unsigned char *p, const uint64_t v) {
	p[0] = (unsigned char)(v      );
	p[1] = (unsigned char)(v >>  8);
	p[2] = (unsigned char)(v >> 16);
	p[3] = (unsigned char)(v >> 24);
	p[4] = (unsigned char)(v >> 32);
	p[5] = (unsigned char)(v >> 40);
	p[6] = (unsigned char)(v >> 48);
	p[7] = (unsigned char)(v >> 56);
}
#endif

#include <stdlib.h>
#include <string.h>


