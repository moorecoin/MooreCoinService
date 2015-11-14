/*
xxhash - fast hash algorithm
copyright (c) 2012-2015, yann collet

bsd 2-clause license (http://www.opensource.org/licenses/bsd-license.php)

redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.

this software is provided by the copyright holders and contributors
"as is" and any express or implied warranties, including, but not
limited to, the implied warranties of merchantability and fitness for
a particular purpose are disclaimed. in no event shall the copyright
owner or contributors be liable for any direct, indirect, incidental,
special, exemplary, or consequential damages (including, but not
limited to, procurement of substitute goods or services; loss of use,
data, or profits; or business interruption) however caused and on any
theory of liability, whether in contract, strict liability, or tort
(including negligence or otherwise) arising in any way out of the use
of this software, even if advised of the possibility of such damage.

you can contact the author at :
- xxhash source repository : http://code.google.com/p/xxhash/
- xxhash source mirror : https://github.com/cyan4973/xxhash
- public discussion board : https://groups.google.com/forum/#!forum/lz4c
*/


/**************************************
*  tuning parameters
***************************************/
/* unaligned memory access is automatically enabled for "common" cpu, such as x86.
 * for others cpu, the compiler will be more cautious, and insert extra code to ensure aligned access is respected.
 * if you know your target cpu supports unaligned memory access, you want to force this option manually to improve performance.
 * you can also enable this parameter if you know your input data will always be aligned (boundaries of 4, for u32).
 */
#if defined(__arm_feature_unaligned) || defined(__i386) || defined(_m_ix86) || defined(__x86_64__) || defined(_m_x64)
#  define xxh_use_unaligned_access 1
#endif

/* xxh_accept_null_input_pointer :
 * if the input pointer is a null pointer, xxhash default behavior is to trigger a memory access error, since it is a bad pointer.
 * when this option is enabled, xxhash output for null input pointers will be the same as a null-length input.
 * by default, this option is disabled. to enable it, uncomment below define :
 */
/* #define xxh_accept_null_input_pointer 1 */

/* xxh_force_native_format :
 * by default, xxhash library provides endian-independant hash values, based on little-endian convention.
 * results are therefore identical for little-endian and big-endian cpu.
 * this comes at a performance cost for big-endian cpu, since some swapping is required to emulate little-endian format.
 * should endian-independance be of no importance for your application, you may set the #define below to 1.
 * it will improve speed for big-endian cpu.
 * this option has no impact on little_endian cpu.
 */
#define xxh_force_native_format 0


/**************************************
*  compiler specific options
***************************************/
#ifdef _msc_ver    /* visual studio */
#  pragma warning(disable : 4127)      /* disable: c4127: conditional expression is constant */
#  define force_inline static __forceinline
#else
#  if defined (__stdc_version__) && __stdc_version__ >= 199901l   /* c99 */
#    ifdef __gnuc__
#      define force_inline static inline __attribute__((always_inline))
#    else
#      define force_inline static inline
#    endif
#  else
#    define force_inline static
#  endif /* __stdc_version__ */
#endif


/**************************************
*  includes & memory related functions
***************************************/
#include "xxhash.h"
/* modify the local functions below should you wish to use some other memory routines */
/* for malloc(), free() */
#include <stdlib.h>
static void* xxh_malloc(size_t s) { return malloc(s); }
static void  xxh_free  (void* p)  { free(p); }
/* for memcpy() */
#include <string.h>
static void* xxh_memcpy(void* dest, const void* src, size_t size)
{
    return memcpy(dest,src,size);
}


/**************************************
*  basic types
***************************************/
#if defined (__stdc_version__) && __stdc_version__ >= 199901l   /* c99 */
# include <stdint.h>
typedef uint8_t  byte;
typedef uint16_t u16;
typedef uint32_t u32;
typedef  int32_t s32;
typedef uint64_t u64;
#else
typedef unsigned char      byte;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef   signed int       s32;
typedef unsigned long long u64;
#endif

#if defined(__gnuc__)  && !defined(xxh_use_unaligned_access)
#  define _packed __attribute__ ((packed))
#else
#  define _packed
#endif

#if !defined(xxh_use_unaligned_access) && !defined(__gnuc__)
#  ifdef __ibmc__
#    pragma pack(1)
#  else
#    pragma pack(push, 1)
#  endif
#endif

typedef struct _u32_s
{
    u32 v;
} _packed u32_s;
typedef struct _u64_s
{
    u64 v;
} _packed u64_s;

#if !defined(xxh_use_unaligned_access) && !defined(__gnuc__)
#  pragma pack(pop)
#endif

#define a32(x) (((u32_s *)(x))->v)
#define a64(x) (((u64_s *)(x))->v)


/*****************************************
*  compiler-specific functions and macros
******************************************/
#define gcc_version (__gnuc__ * 100 + __gnuc_minor__)

/* note : although _rotl exists for mingw (gcc under windows), performance seems poor */
#if defined(_msc_ver)
#  define xxh_rotl32(x,r) _rotl(x,r)
#  define xxh_rotl64(x,r) _rotl64(x,r)
#else
#  define xxh_rotl32(x,r) ((x << r) | (x >> (32 - r)))
#  define xxh_rotl64(x,r) ((x << r) | (x >> (64 - r)))
#endif

#if defined(_msc_ver)     /* visual studio */
#  define xxh_swap32 _byteswap_ulong
#  define xxh_swap64 _byteswap_uint64
#elif gcc_version >= 403
#  define xxh_swap32 __builtin_bswap32
#  define xxh_swap64 __builtin_bswap64
#else
static u32 xxh_swap32 (u32 x)
{
    return  ((x << 24) & 0xff000000 ) |
            ((x <<  8) & 0x00ff0000 ) |
            ((x >>  8) & 0x0000ff00 ) |
            ((x >> 24) & 0x000000ff );
}
static u64 xxh_swap64 (u64 x)
{
    return  ((x << 56) & 0xff00000000000000ull) |
            ((x << 40) & 0x00ff000000000000ull) |
            ((x << 24) & 0x0000ff0000000000ull) |
            ((x << 8)  & 0x000000ff00000000ull) |
            ((x >> 8)  & 0x00000000ff000000ull) |
            ((x >> 24) & 0x0000000000ff0000ull) |
            ((x >> 40) & 0x000000000000ff00ull) |
            ((x >> 56) & 0x00000000000000ffull);
}
#endif


/**************************************
*  constants
***************************************/
#define prime32_1   2654435761u
#define prime32_2   2246822519u
#define prime32_3   3266489917u
#define prime32_4    668265263u
#define prime32_5    374761393u

#define prime64_1 11400714785074694791ull
#define prime64_2 14029467366897019727ull
#define prime64_3  1609587929392839161ull
#define prime64_4  9650029242287828579ull
#define prime64_5  2870177450012600261ull


/***************************************
*  architecture macros
****************************************/
typedef enum { xxh_bigendian=0, xxh_littleendian=1 } xxh_endianess;
#ifndef xxh_cpu_little_endian   /* xxh_cpu_little_endian can be defined externally, for example using a compiler switch */
static const int one = 1;
#   define xxh_cpu_little_endian   (*(char*)(&one))
#endif


/**************************************
*  macros
***************************************/
#define xxh_static_assert(c)   { enum { xxh_static_assert = 1/(!!(c)) }; }    /* use only *after* variable declarations */


/****************************
*  memory reads
*****************************/
typedef enum { xxh_aligned, xxh_unaligned } xxh_alignment;

force_inline u32 xxh_readle32_align(const void* ptr, xxh_endianess endian, xxh_alignment align)
{
    if (align==xxh_unaligned)
        return endian==xxh_littleendian ? a32(ptr) : xxh_swap32(a32(ptr));
    else
        return endian==xxh_littleendian ? *(u32*)ptr : xxh_swap32(*(u32*)ptr);
}

force_inline u32 xxh_readle32(const void* ptr, xxh_endianess endian)
{
    return xxh_readle32_align(ptr, endian, xxh_unaligned);
}

force_inline u64 xxh_readle64_align(const void* ptr, xxh_endianess endian, xxh_alignment align)
{
    if (align==xxh_unaligned)
        return endian==xxh_littleendian ? a64(ptr) : xxh_swap64(a64(ptr));
    else
        return endian==xxh_littleendian ? *(u64*)ptr : xxh_swap64(*(u64*)ptr);
}

force_inline u64 xxh_readle64(const void* ptr, xxh_endianess endian)
{
    return xxh_readle64_align(ptr, endian, xxh_unaligned);
}


/****************************
*  simple hash functions
*****************************/
force_inline u32 xxh32_endian_align(const void* input, size_t len, u32 seed, xxh_endianess endian, xxh_alignment align)
{
    const byte* p = (const byte*)input;
    const byte* bend = p + len;
    u32 h32;
#define xxh_get32bits(p) xxh_readle32_align(p, endian, align)

#ifdef xxh_accept_null_input_pointer
    if (p==null)
    {
        len=0;
        bend=p=(const byte*)(size_t)16;
    }
#endif

    if (len>=16)
    {
        const byte* const limit = bend - 16;
        u32 v1 = seed + prime32_1 + prime32_2;
        u32 v2 = seed + prime32_2;
        u32 v3 = seed + 0;
        u32 v4 = seed - prime32_1;

        do
        {
            v1 += xxh_get32bits(p) * prime32_2;
            v1 = xxh_rotl32(v1, 13);
            v1 *= prime32_1;
            p+=4;
            v2 += xxh_get32bits(p) * prime32_2;
            v2 = xxh_rotl32(v2, 13);
            v2 *= prime32_1;
            p+=4;
            v3 += xxh_get32bits(p) * prime32_2;
            v3 = xxh_rotl32(v3, 13);
            v3 *= prime32_1;
            p+=4;
            v4 += xxh_get32bits(p) * prime32_2;
            v4 = xxh_rotl32(v4, 13);
            v4 *= prime32_1;
            p+=4;
        }
        while (p<=limit);

        h32 = xxh_rotl32(v1, 1) + xxh_rotl32(v2, 7) + xxh_rotl32(v3, 12) + xxh_rotl32(v4, 18);
    }
    else
    {
        h32  = seed + prime32_5;
    }

    h32 += (u32) len;

    while (p+4<=bend)
    {
        h32 += xxh_get32bits(p) * prime32_3;
        h32  = xxh_rotl32(h32, 17) * prime32_4 ;
        p+=4;
    }

    while (p<bend)
    {
        h32 += (*p) * prime32_5;
        h32 = xxh_rotl32(h32, 11) * prime32_1 ;
        p++;
    }

    h32 ^= h32 >> 15;
    h32 *= prime32_2;
    h32 ^= h32 >> 13;
    h32 *= prime32_3;
    h32 ^= h32 >> 16;

    return h32;
}


unsigned int xxh32 (const void* input, size_t len, unsigned seed)
{
#if 0
    /* simple version, good for code maintenance, but unfortunately slow for small inputs */
    xxh32_state_t state;
    xxh32_reset(&state, seed);
    xxh32_update(&state, input, len);
    return xxh32_digest(&state);
#else
    xxh_endianess endian_detected = (xxh_endianess)xxh_cpu_little_endian;

#  if !defined(xxh_use_unaligned_access)
    if ((((size_t)input) & 3) == 0)   /* input is aligned, let's leverage the speed advantage */
    {
        if ((endian_detected==xxh_littleendian) || xxh_force_native_format)
            return xxh32_endian_align(input, len, seed, xxh_littleendian, xxh_aligned);
        else
            return xxh32_endian_align(input, len, seed, xxh_bigendian, xxh_aligned);
    }
#  endif

    if ((endian_detected==xxh_littleendian) || xxh_force_native_format)
        return xxh32_endian_align(input, len, seed, xxh_littleendian, xxh_unaligned);
    else
        return xxh32_endian_align(input, len, seed, xxh_bigendian, xxh_unaligned);
#endif
}

force_inline u64 xxh64_endian_align(const void* input, size_t len, u64 seed, xxh_endianess endian, xxh_alignment align)
{
    const byte* p = (const byte*)input;
    const byte* bend = p + len;
    u64 h64;
#define xxh_get64bits(p) xxh_readle64_align(p, endian, align)

#ifdef xxh_accept_null_input_pointer
    if (p==null)
    {
        len=0;
        bend=p=(const byte*)(size_t)32;
    }
#endif

    if (len>=32)
    {
        const byte* const limit = bend - 32;
        u64 v1 = seed + prime64_1 + prime64_2;
        u64 v2 = seed + prime64_2;
        u64 v3 = seed + 0;
        u64 v4 = seed - prime64_1;

        do
        {
            v1 += xxh_get64bits(p) * prime64_2;
            p+=8;
            v1 = xxh_rotl64(v1, 31);
            v1 *= prime64_1;
            v2 += xxh_get64bits(p) * prime64_2;
            p+=8;
            v2 = xxh_rotl64(v2, 31);
            v2 *= prime64_1;
            v3 += xxh_get64bits(p) * prime64_2;
            p+=8;
            v3 = xxh_rotl64(v3, 31);
            v3 *= prime64_1;
            v4 += xxh_get64bits(p) * prime64_2;
            p+=8;
            v4 = xxh_rotl64(v4, 31);
            v4 *= prime64_1;
        }
        while (p<=limit);

        h64 = xxh_rotl64(v1, 1) + xxh_rotl64(v2, 7) + xxh_rotl64(v3, 12) + xxh_rotl64(v4, 18);

        v1 *= prime64_2;
        v1 = xxh_rotl64(v1, 31);
        v1 *= prime64_1;
        h64 ^= v1;
        h64 = h64 * prime64_1 + prime64_4;

        v2 *= prime64_2;
        v2 = xxh_rotl64(v2, 31);
        v2 *= prime64_1;
        h64 ^= v2;
        h64 = h64 * prime64_1 + prime64_4;

        v3 *= prime64_2;
        v3 = xxh_rotl64(v3, 31);
        v3 *= prime64_1;
        h64 ^= v3;
        h64 = h64 * prime64_1 + prime64_4;

        v4 *= prime64_2;
        v4 = xxh_rotl64(v4, 31);
        v4 *= prime64_1;
        h64 ^= v4;
        h64 = h64 * prime64_1 + prime64_4;
    }
    else
    {
        h64  = seed + prime64_5;
    }

    h64 += (u64) len;

    while (p+8<=bend)
    {
        u64 k1 = xxh_get64bits(p);
        k1 *= prime64_2;
        k1 = xxh_rotl64(k1,31);
        k1 *= prime64_1;
        h64 ^= k1;
        h64 = xxh_rotl64(h64,27) * prime64_1 + prime64_4;
        p+=8;
    }

    if (p+4<=bend)
    {
        h64 ^= (u64)(xxh_get32bits(p)) * prime64_1;
        h64 = xxh_rotl64(h64, 23) * prime64_2 + prime64_3;
        p+=4;
    }

    while (p<bend)
    {
        h64 ^= (*p) * prime64_5;
        h64 = xxh_rotl64(h64, 11) * prime64_1;
        p++;
    }

    h64 ^= h64 >> 33;
    h64 *= prime64_2;
    h64 ^= h64 >> 29;
    h64 *= prime64_3;
    h64 ^= h64 >> 32;

    return h64;
}


unsigned long long xxh64 (const void* input, size_t len, unsigned long long seed)
{
#if 0
    /* simple version, good for code maintenance, but unfortunately slow for small inputs */
    xxh64_state_t state;
    xxh64_reset(&state, seed);
    xxh64_update(&state, input, len);
    return xxh64_digest(&state);
#else
    xxh_endianess endian_detected = (xxh_endianess)xxh_cpu_little_endian;

#  if !defined(xxh_use_unaligned_access)
    if ((((size_t)input) & 7)==0)   /* input is aligned, let's leverage the speed advantage */
    {
        if ((endian_detected==xxh_littleendian) || xxh_force_native_format)
            return xxh64_endian_align(input, len, seed, xxh_littleendian, xxh_aligned);
        else
            return xxh64_endian_align(input, len, seed, xxh_bigendian, xxh_aligned);
    }
#  endif

    if ((endian_detected==xxh_littleendian) || xxh_force_native_format)
        return xxh64_endian_align(input, len, seed, xxh_littleendian, xxh_unaligned);
    else
        return xxh64_endian_align(input, len, seed, xxh_bigendian, xxh_unaligned);
#endif
}

/****************************************************
 *  advanced hash functions
****************************************************/

/*** allocation ***/
typedef struct
{
    u64 total_len;
    u32 seed;
    u32 v1;
    u32 v2;
    u32 v3;
    u32 v4;
    u32 mem32[4];   /* defined as u32 for alignment */
    u32 memsize;
} xxh_istate32_t;

typedef struct
{
    u64 total_len;
    u64 seed;
    u64 v1;
    u64 v2;
    u64 v3;
    u64 v4;
    u64 mem64[4];   /* defined as u64 for alignment */
    u32 memsize;
} xxh_istate64_t;


xxh32_state_t* xxh32_createstate(void)
{
    xxh_static_assert(sizeof(xxh32_state_t) >= sizeof(xxh_istate32_t));   /* a compilation error here means xxh32_state_t is not large enough */
    return (xxh32_state_t*)xxh_malloc(sizeof(xxh32_state_t));
}
xxh_errorcode xxh32_freestate(xxh32_state_t* stateptr)
{
    xxh_free(stateptr);
    return xxh_ok;
}

xxh64_state_t* xxh64_createstate(void)
{
    xxh_static_assert(sizeof(xxh64_state_t) >= sizeof(xxh_istate64_t));   /* a compilation error here means xxh64_state_t is not large enough */
    return (xxh64_state_t*)xxh_malloc(sizeof(xxh64_state_t));
}
xxh_errorcode xxh64_freestate(xxh64_state_t* stateptr)
{
    xxh_free(stateptr);
    return xxh_ok;
}


/*** hash feed ***/

xxh_errorcode xxh32_reset(xxh32_state_t* state_in, u32 seed)
{
    xxh_istate32_t* state = (xxh_istate32_t*) state_in;
    state->seed = seed;
    state->v1 = seed + prime32_1 + prime32_2;
    state->v2 = seed + prime32_2;
    state->v3 = seed + 0;
    state->v4 = seed - prime32_1;
    state->total_len = 0;
    state->memsize = 0;
    return xxh_ok;
}

xxh_errorcode xxh64_reset(xxh64_state_t* state_in, unsigned long long seed)
{
    xxh_istate64_t* state = (xxh_istate64_t*) state_in;
    state->seed = seed;
    state->v1 = seed + prime64_1 + prime64_2;
    state->v2 = seed + prime64_2;
    state->v3 = seed + 0;
    state->v4 = seed - prime64_1;
    state->total_len = 0;
    state->memsize = 0;
    return xxh_ok;
}


force_inline xxh_errorcode xxh32_update_endian (xxh32_state_t* state_in, const void* input, size_t len, xxh_endianess endian)
{
    xxh_istate32_t* state = (xxh_istate32_t *) state_in;
    const byte* p = (const byte*)input;
    const byte* const bend = p + len;

#ifdef xxh_accept_null_input_pointer
    if (input==null) return xxh_error;
#endif

    state->total_len += len;

    if (state->memsize + len < 16)   /* fill in tmp buffer */
    {
        xxh_memcpy((byte*)(state->mem32) + state->memsize, input, len);
        state->memsize += (u32)len;
        return xxh_ok;
    }

    if (state->memsize)   /* some data left from previous update */
    {
        xxh_memcpy((byte*)(state->mem32) + state->memsize, input, 16-state->memsize);
        {
            const u32* p32 = state->mem32;
            state->v1 += xxh_readle32(p32, endian) * prime32_2;
            state->v1 = xxh_rotl32(state->v1, 13);
            state->v1 *= prime32_1;
            p32++;
            state->v2 += xxh_readle32(p32, endian) * prime32_2;
            state->v2 = xxh_rotl32(state->v2, 13);
            state->v2 *= prime32_1;
            p32++;
            state->v3 += xxh_readle32(p32, endian) * prime32_2;
            state->v3 = xxh_rotl32(state->v3, 13);
            state->v3 *= prime32_1;
            p32++;
            state->v4 += xxh_readle32(p32, endian) * prime32_2;
            state->v4 = xxh_rotl32(state->v4, 13);
            state->v4 *= prime32_1;
            p32++;
        }
        p += 16-state->memsize;
        state->memsize = 0;
    }

    if (p <= bend-16)
    {
        const byte* const limit = bend - 16;
        u32 v1 = state->v1;
        u32 v2 = state->v2;
        u32 v3 = state->v3;
        u32 v4 = state->v4;

        do
        {
            v1 += xxh_readle32(p, endian) * prime32_2;
            v1 = xxh_rotl32(v1, 13);
            v1 *= prime32_1;
            p+=4;
            v2 += xxh_readle32(p, endian) * prime32_2;
            v2 = xxh_rotl32(v2, 13);
            v2 *= prime32_1;
            p+=4;
            v3 += xxh_readle32(p, endian) * prime32_2;
            v3 = xxh_rotl32(v3, 13);
            v3 *= prime32_1;
            p+=4;
            v4 += xxh_readle32(p, endian) * prime32_2;
            v4 = xxh_rotl32(v4, 13);
            v4 *= prime32_1;
            p+=4;
        }
        while (p<=limit);

        state->v1 = v1;
        state->v2 = v2;
        state->v3 = v3;
        state->v4 = v4;
    }

    if (p < bend)
    {
        xxh_memcpy(state->mem32, p, bend-p);
        state->memsize = (int)(bend-p);
    }

    return xxh_ok;
}

xxh_errorcode xxh32_update (xxh32_state_t* state_in, const void* input, size_t len)
{
    xxh_endianess endian_detected = (xxh_endianess)xxh_cpu_little_endian;

    if ((endian_detected==xxh_littleendian) || xxh_force_native_format)
        return xxh32_update_endian(state_in, input, len, xxh_littleendian);
    else
        return xxh32_update_endian(state_in, input, len, xxh_bigendian);
}



force_inline u32 xxh32_digest_endian (const xxh32_state_t* state_in, xxh_endianess endian)
{
    xxh_istate32_t* state = (xxh_istate32_t*) state_in;
    const byte * p = (const byte*)state->mem32;
    byte* bend = (byte*)(state->mem32) + state->memsize;
    u32 h32;

    if (state->total_len >= 16)
    {
        h32 = xxh_rotl32(state->v1, 1) + xxh_rotl32(state->v2, 7) + xxh_rotl32(state->v3, 12) + xxh_rotl32(state->v4, 18);
    }
    else
    {
        h32  = state->seed + prime32_5;
    }

    h32 += (u32) state->total_len;

    while (p+4<=bend)
    {
        h32 += xxh_readle32(p, endian) * prime32_3;
        h32  = xxh_rotl32(h32, 17) * prime32_4;
        p+=4;
    }

    while (p<bend)
    {
        h32 += (*p) * prime32_5;
        h32 = xxh_rotl32(h32, 11) * prime32_1;
        p++;
    }

    h32 ^= h32 >> 15;
    h32 *= prime32_2;
    h32 ^= h32 >> 13;
    h32 *= prime32_3;
    h32 ^= h32 >> 16;

    return h32;
}


u32 xxh32_digest (const xxh32_state_t* state_in)
{
    xxh_endianess endian_detected = (xxh_endianess)xxh_cpu_little_endian;

    if ((endian_detected==xxh_littleendian) || xxh_force_native_format)
        return xxh32_digest_endian(state_in, xxh_littleendian);
    else
        return xxh32_digest_endian(state_in, xxh_bigendian);
}


force_inline xxh_errorcode xxh64_update_endian (xxh64_state_t* state_in, const void* input, size_t len, xxh_endianess endian)
{
    xxh_istate64_t * state = (xxh_istate64_t *) state_in;
    const byte* p = (const byte*)input;
    const byte* const bend = p + len;

#ifdef xxh_accept_null_input_pointer
    if (input==null) return xxh_error;
#endif

    state->total_len += len;

    if (state->memsize + len < 32)   /* fill in tmp buffer */
    {
        xxh_memcpy(((byte*)state->mem64) + state->memsize, input, len);
        state->memsize += (u32)len;
        return xxh_ok;
    }

    if (state->memsize)   /* some data left from previous update */
    {
        xxh_memcpy(((byte*)state->mem64) + state->memsize, input, 32-state->memsize);
        {
            const u64* p64 = state->mem64;
            state->v1 += xxh_readle64(p64, endian) * prime64_2;
            state->v1 = xxh_rotl64(state->v1, 31);
            state->v1 *= prime64_1;
            p64++;
            state->v2 += xxh_readle64(p64, endian) * prime64_2;
            state->v2 = xxh_rotl64(state->v2, 31);
            state->v2 *= prime64_1;
            p64++;
            state->v3 += xxh_readle64(p64, endian) * prime64_2;
            state->v3 = xxh_rotl64(state->v3, 31);
            state->v3 *= prime64_1;
            p64++;
            state->v4 += xxh_readle64(p64, endian) * prime64_2;
            state->v4 = xxh_rotl64(state->v4, 31);
            state->v4 *= prime64_1;
            p64++;
        }
        p += 32-state->memsize;
        state->memsize = 0;
    }

    if (p+32 <= bend)
    {
        const byte* const limit = bend - 32;
        u64 v1 = state->v1;
        u64 v2 = state->v2;
        u64 v3 = state->v3;
        u64 v4 = state->v4;

        do
        {
            v1 += xxh_readle64(p, endian) * prime64_2;
            v1 = xxh_rotl64(v1, 31);
            v1 *= prime64_1;
            p+=8;
            v2 += xxh_readle64(p, endian) * prime64_2;
            v2 = xxh_rotl64(v2, 31);
            v2 *= prime64_1;
            p+=8;
            v3 += xxh_readle64(p, endian) * prime64_2;
            v3 = xxh_rotl64(v3, 31);
            v3 *= prime64_1;
            p+=8;
            v4 += xxh_readle64(p, endian) * prime64_2;
            v4 = xxh_rotl64(v4, 31);
            v4 *= prime64_1;
            p+=8;
        }
        while (p<=limit);

        state->v1 = v1;
        state->v2 = v2;
        state->v3 = v3;
        state->v4 = v4;
    }

    if (p < bend)
    {
        xxh_memcpy(state->mem64, p, bend-p);
        state->memsize = (int)(bend-p);
    }

    return xxh_ok;
}

xxh_errorcode xxh64_update (xxh64_state_t* state_in, const void* input, size_t len)
{
    xxh_endianess endian_detected = (xxh_endianess)xxh_cpu_little_endian;

    if ((endian_detected==xxh_littleendian) || xxh_force_native_format)
        return xxh64_update_endian(state_in, input, len, xxh_littleendian);
    else
        return xxh64_update_endian(state_in, input, len, xxh_bigendian);
}



force_inline u64 xxh64_digest_endian (const xxh64_state_t* state_in, xxh_endianess endian)
{
    xxh_istate64_t * state = (xxh_istate64_t *) state_in;
    const byte * p = (const byte*)state->mem64;
    byte* bend = (byte*)state->mem64 + state->memsize;
    u64 h64;

    if (state->total_len >= 32)
    {
        u64 v1 = state->v1;
        u64 v2 = state->v2;
        u64 v3 = state->v3;
        u64 v4 = state->v4;

        h64 = xxh_rotl64(v1, 1) + xxh_rotl64(v2, 7) + xxh_rotl64(v3, 12) + xxh_rotl64(v4, 18);

        v1 *= prime64_2;
        v1 = xxh_rotl64(v1, 31);
        v1 *= prime64_1;
        h64 ^= v1;
        h64 = h64*prime64_1 + prime64_4;

        v2 *= prime64_2;
        v2 = xxh_rotl64(v2, 31);
        v2 *= prime64_1;
        h64 ^= v2;
        h64 = h64*prime64_1 + prime64_4;

        v3 *= prime64_2;
        v3 = xxh_rotl64(v3, 31);
        v3 *= prime64_1;
        h64 ^= v3;
        h64 = h64*prime64_1 + prime64_4;

        v4 *= prime64_2;
        v4 = xxh_rotl64(v4, 31);
        v4 *= prime64_1;
        h64 ^= v4;
        h64 = h64*prime64_1 + prime64_4;
    }
    else
    {
        h64  = state->seed + prime64_5;
    }

    h64 += (u64) state->total_len;

    while (p+8<=bend)
    {
        u64 k1 = xxh_readle64(p, endian);
        k1 *= prime64_2;
        k1 = xxh_rotl64(k1,31);
        k1 *= prime64_1;
        h64 ^= k1;
        h64 = xxh_rotl64(h64,27) * prime64_1 + prime64_4;
        p+=8;
    }

    if (p+4<=bend)
    {
        h64 ^= (u64)(xxh_readle32(p, endian)) * prime64_1;
        h64 = xxh_rotl64(h64, 23) * prime64_2 + prime64_3;
        p+=4;
    }

    while (p<bend)
    {
        h64 ^= (*p) * prime64_5;
        h64 = xxh_rotl64(h64, 11) * prime64_1;
        p++;
    }

    h64 ^= h64 >> 33;
    h64 *= prime64_2;
    h64 ^= h64 >> 29;
    h64 *= prime64_3;
    h64 ^= h64 >> 32;

    return h64;
}


unsigned long long xxh64_digest (const xxh64_state_t* state_in)
{
    xxh_endianess endian_detected = (xxh_endianess)xxh_cpu_little_endian;

    if ((endian_detected==xxh_littleendian) || xxh_force_native_format)
        return xxh64_digest_endian(state_in, xxh_littleendian);
    else
        return xxh64_digest_endian(state_in, xxh_bigendian);
}


