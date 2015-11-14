/*
   lz4 - fast lz compression algorithm
   copyright (c) 2011-2015, yann collet.
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
   - lz4 source repository : http://code.google.com/p/lz4
   - lz4 source mirror : https://github.com/cyan4973/lz4
   - lz4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/


/**************************************
   tuning parameters
**************************************/
/*
 * heapmode :
 * select how default compression functions will allocate memory for their hash table,
 * in memory stack (0:default, fastest), or in memory heap (1:requires malloc()).
 */
#define heapmode 0

/*
 * cpu_has_efficient_unaligned_memory_access :
 * by default, the source code expects the compiler to correctly optimize
 * 4-bytes and 8-bytes read on architectures able to handle it efficiently.
 * this is not always the case. in some circumstances (arm notably),
 * the compiler will issue cautious code even when target is able to correctly handle unaligned memory accesses.
 *
 * you can force the compiler to use unaligned memory access by uncommenting the line below.
 * one of the below scenarios will happen :
 * 1 - your target cpu correctly handle unaligned access, and was not well optimized by compiler (good case).
 *     you will witness large performance improvements (+50% and up).
 *     keep the line uncommented and send a word to upstream (https://groups.google.com/forum/#!forum/lz4c)
 *     the goal is to automatically detect such situations by adding your target cpu within an exception list.
 * 2 - your target cpu correctly handle unaligned access, and was already already optimized by compiler
 *     no change will be experienced.
 * 3 - your target cpu inefficiently handle unaligned access.
 *     you will experience a performance loss. comment back the line.
 * 4 - your target cpu does not handle unaligned access.
 *     program will crash.
 * if uncommenting results in better performance (case 1)
 * please report your configuration to upstream (https://groups.google.com/forum/#!forum/lz4c)
 * an automatic detection macro will be added to match your case within future versions of the library.
 */
/* #define cpu_has_efficient_unaligned_memory_access 1 */


/**************************************
   cpu feature detection
**************************************/
/*
 * automated efficient unaligned memory access detection
 * based on known hardware architectures
 * this list will be updated thanks to feedbacks
 */
#if defined(cpu_has_efficient_unaligned_memory_access) \
    || defined(__arm_feature_unaligned) \
    || defined(__i386__) || defined(__x86_64__) \
    || defined(_m_ix86) || defined(_m_x64) \
    || defined(__arm_arch_7__) || defined(__arm_arch_8__) \
    || (defined(_m_arm) && (_m_arm >= 7))
#  define lz4_unaligned_access 1
#else
#  define lz4_unaligned_access 0
#endif

/*
 * lz4_force_sw_bitcount
 * define this parameter if your target system or compiler does not support hardware bit count
 */
#if defined(_msc_ver) && defined(_win32_wce)   /* visual studio for windows ce does not support hardware bit count */
#  define lz4_force_sw_bitcount
#endif


/**************************************
   compiler options
**************************************/
#if defined(__stdc_version__) && (__stdc_version__ >= 199901l)   /* c99 */
/* "restrict" is a known keyword */
#else
#  define restrict /* disable restrict */
#endif

#ifdef _msc_ver    /* visual studio */
#  define force_inline static __forceinline
#  include <intrin.h>
#  pragma warning(disable : 4127)        /* disable: c4127: conditional expression is constant */
#  pragma warning(disable : 4293)        /* disable: c4293: too large shift (32-bits) */
#else
#  if defined(__stdc_version__) && (__stdc_version__ >= 199901l)   /* c99 */
#    ifdef __gnuc__
#      define force_inline static inline __attribute__((always_inline))
#    else
#      define force_inline static inline
#    endif
#  else
#    define force_inline static
#  endif   /* __stdc_version__ */
#endif  /* _msc_ver */

#define gcc_version (__gnuc__ * 100 + __gnuc_minor__)

#if (gcc_version >= 302) || (__intel_compiler >= 800) || defined(__clang__)
#  define expect(expr,value)    (__builtin_expect ((expr),(value)) )
#else
#  define expect(expr,value)    (expr)
#endif

#define likely(expr)     expect((expr) != 0, 1)
#define unlikely(expr)   expect((expr) != 0, 0)


/**************************************
   memory routines
**************************************/
#include <stdlib.h>   /* malloc, calloc, free */
#define allocator(n,s) calloc(n,s)
#define freemem        free
#include <string.h>   /* memset, memcpy */
#define mem_init       memset


/**************************************
   includes
**************************************/
#include "lz4.h"


/**************************************
   basic types
**************************************/
#if defined (__stdc_version__) && (__stdc_version__ >= 199901l)   /* c99 */
# include <stdint.h>
  typedef  uint8_t byte;
  typedef uint16_t u16;
  typedef uint32_t u32;
  typedef  int32_t s32;
  typedef uint64_t u64;
#else
  typedef unsigned char       byte;
  typedef unsigned short      u16;
  typedef unsigned int        u32;
  typedef   signed int        s32;
  typedef unsigned long long  u64;
#endif


/**************************************
   reading and writing into memory
**************************************/
#define stepsize sizeof(size_t)

static unsigned lz4_64bits(void) { return sizeof(void*)==8; }

static unsigned lz4_islittleendian(void)
{
    const union { u32 i; byte c[4]; } one = { 1 };   /* don't use static : performance detrimental  */
    return one.c[0];
}


static u16 lz4_readle16(const void* memptr)
{
    if ((lz4_unaligned_access) && (lz4_islittleendian()))
        return *(u16*)memptr;
    else
    {
        const byte* p = memptr;
        return (u16)((u16)p[0] + (p[1]<<8));
    }
}

static void lz4_writele16(void* memptr, u16 value)
{
    if ((lz4_unaligned_access) && (lz4_islittleendian()))
    {
        *(u16*)memptr = value;
        return;
    }
    else
    {
        byte* p = memptr;
        p[0] = (byte) value;
        p[1] = (byte)(value>>8);
    }
}


static u16 lz4_read16(const void* memptr)
{
    if (lz4_unaligned_access)
        return *(u16*)memptr;
    else
    {
        u16 val16;
        memcpy(&val16, memptr, 2);
        return val16;
    }
}

static u32 lz4_read32(const void* memptr)
{
    if (lz4_unaligned_access)
        return *(u32*)memptr;
    else
    {
        u32 val32;
        memcpy(&val32, memptr, 4);
        return val32;
    }
}

static u64 lz4_read64(const void* memptr)
{
    if (lz4_unaligned_access)
        return *(u64*)memptr;
    else
    {
        u64 val64;
        memcpy(&val64, memptr, 8);
        return val64;
    }
}

static size_t lz4_read_arch(const void* p)
{
    if (lz4_64bits())
        return (size_t)lz4_read64(p);
    else
        return (size_t)lz4_read32(p);
}


static void lz4_copy4(void* dstptr, const void* srcptr)
{
    if (lz4_unaligned_access)
    {
        *(u32*)dstptr = *(u32*)srcptr;
        return;
    }
    memcpy(dstptr, srcptr, 4);
}

static void lz4_copy8(void* dstptr, const void* srcptr)
{
#if gcc_version!=409  /* disabled on gcc 4.9, as it generates invalid opcode (crash) */
    if (lz4_unaligned_access)
    {
        if (lz4_64bits())
            *(u64*)dstptr = *(u64*)srcptr;
        else
            ((u32*)dstptr)[0] = ((u32*)srcptr)[0],
            ((u32*)dstptr)[1] = ((u32*)srcptr)[1];
        return;
    }
#endif
    memcpy(dstptr, srcptr, 8);
}

/* customized version of memcpy, which may overwrite up to 7 bytes beyond dstend */
static void lz4_wildcopy(void* dstptr, const void* srcptr, void* dstend)
{
    byte* d = dstptr;
    const byte* s = srcptr;
    byte* e = dstend;
    do { lz4_copy8(d,s); d+=8; s+=8; } while (d<e);
}


/**************************************
   common constants
**************************************/
#define minmatch 4

#define copylength 8
#define lastliterals 5
#define mflimit (copylength+minmatch)
static const int lz4_minlength = (mflimit+1);

#define kb *(1 <<10)
#define mb *(1 <<20)
#define gb *(1u<<30)

#define maxd_log 16
#define max_distance ((1 << maxd_log) - 1)

#define ml_bits  4
#define ml_mask  ((1u<<ml_bits)-1)
#define run_bits (8-ml_bits)
#define run_mask ((1u<<run_bits)-1)


/**************************************
   common utils
**************************************/
#define lz4_static_assert(c)    { enum { lz4_static_assert = 1/(int)(!!(c)) }; }   /* use only *after* variable declarations */


/********************************
   common functions
********************************/
static unsigned lz4_nbcommonbytes (register size_t val)
{
    if (lz4_islittleendian())
    {
        if (lz4_64bits())
        {
#       if defined(_msc_ver) && defined(_win64) && !defined(lz4_force_sw_bitcount)
            unsigned long r = 0;
            _bitscanforward64( &r, (u64)val );
            return (int)(r>>3);
#       elif defined(__gnuc__) && (gcc_version >= 304) && !defined(lz4_force_sw_bitcount)
            return (__builtin_ctzll((u64)val) >> 3);
#       else
            static const int debruijnbytepos[64] = { 0, 0, 0, 0, 0, 1, 1, 2, 0, 3, 1, 3, 1, 4, 2, 7, 0, 2, 3, 6, 1, 5, 3, 5, 1, 3, 4, 4, 2, 5, 6, 7, 7, 0, 1, 2, 3, 3, 4, 6, 2, 6, 5, 5, 3, 4, 5, 6, 7, 1, 2, 4, 6, 4, 4, 5, 7, 2, 6, 5, 7, 6, 7, 7 };
            return debruijnbytepos[((u64)((val & -(long long)val) * 0x0218a392cdabbd3full)) >> 58];
#       endif
        }
        else /* 32 bits */
        {
#       if defined(_msc_ver) && !defined(lz4_force_sw_bitcount)
            unsigned long r;
            _bitscanforward( &r, (u32)val );
            return (int)(r>>3);
#       elif defined(__gnuc__) && (gcc_version >= 304) && !defined(lz4_force_sw_bitcount)
            return (__builtin_ctz((u32)val) >> 3);
#       else
            static const int debruijnbytepos[32] = { 0, 0, 3, 0, 3, 1, 3, 0, 3, 2, 2, 1, 3, 2, 0, 1, 3, 3, 1, 2, 2, 2, 2, 0, 3, 1, 2, 0, 1, 0, 1, 1 };
            return debruijnbytepos[((u32)((val & -(s32)val) * 0x077cb531u)) >> 27];
#       endif
        }
    }
    else   /* big endian cpu */
    {
        if (lz4_64bits())
        {
#       if defined(_msc_ver) && defined(_win64) && !defined(lz4_force_sw_bitcount)
            unsigned long r = 0;
            _bitscanreverse64( &r, val );
            return (unsigned)(r>>3);
#       elif defined(__gnuc__) && (gcc_version >= 304) && !defined(lz4_force_sw_bitcount)
            return (__builtin_clzll(val) >> 3);
#       else
            unsigned r;
            if (!(val>>32)) { r=4; } else { r=0; val>>=32; }
            if (!(val>>16)) { r+=2; val>>=8; } else { val>>=24; }
            r += (!val);
            return r;
#       endif
        }
        else /* 32 bits */
        {
#       if defined(_msc_ver) && !defined(lz4_force_sw_bitcount)
            unsigned long r = 0;
            _bitscanreverse( &r, (unsigned long)val );
            return (unsigned)(r>>3);
#       elif defined(__gnuc__) && (gcc_version >= 304) && !defined(lz4_force_sw_bitcount)
            return (__builtin_clz(val) >> 3);
#       else
            unsigned r;
            if (!(val>>16)) { r=2; val>>=8; } else { r=0; val>>=24; }
            r += (!val);
            return r;
#       endif
        }
    }
}

static unsigned lz4_count(const byte* pin, const byte* pmatch, const byte* pinlimit)
{
    const byte* const pstart = pin;

    while (likely(pin<pinlimit-(stepsize-1)))
    {
        size_t diff = lz4_read_arch(pmatch) ^ lz4_read_arch(pin);
        if (!diff) { pin+=stepsize; pmatch+=stepsize; continue; }
        pin += lz4_nbcommonbytes(diff);
        return (unsigned)(pin - pstart);
    }

    if (lz4_64bits()) if ((pin<(pinlimit-3)) && (lz4_read32(pmatch) == lz4_read32(pin))) { pin+=4; pmatch+=4; }
    if ((pin<(pinlimit-1)) && (lz4_read16(pmatch) == lz4_read16(pin))) { pin+=2; pmatch+=2; }
    if ((pin<pinlimit) && (*pmatch == *pin)) pin++;
    return (unsigned)(pin - pstart);
}


#ifndef lz4_commondefs_only
/**************************************
   local constants
**************************************/
#define lz4_hashlog   (lz4_memory_usage-2)
#define hashtablesize (1 << lz4_memory_usage)
#define hash_size_u32 (1 << lz4_hashlog)       /* required as macro for static allocation */

static const int lz4_64klimit = ((64 kb) + (mflimit-1));
static const u32 lz4_skiptrigger = 6;  /* increase this value ==> compression run slower on incompressible data */


/**************************************
   local utils
**************************************/
int lz4_versionnumber (void) { return lz4_version_number; }
int lz4_compressbound(int isize)  { return lz4_compressbound(isize); }


/**************************************
   local structures and types
**************************************/
typedef struct {
    u32 hashtable[hash_size_u32];
    u32 currentoffset;
    u32 initcheck;
    const byte* dictionary;
    const byte* bufferstart;
    u32 dictsize;
} lz4_stream_t_internal;

typedef enum { notlimited = 0, limitedoutput = 1 } limitedoutput_directive;
typedef enum { byptr, byu32, byu16 } tabletype_t;

typedef enum { nodict = 0, withprefix64k, usingextdict } dict_directive;
typedef enum { nodictissue = 0, dictsmall } dictissue_directive;

typedef enum { endonoutputsize = 0, endoninputsize = 1 } endcondition_directive;
typedef enum { full = 0, partial = 1 } earlyend_directive;



/********************************
   compression functions
********************************/

static u32 lz4_hashsequence(u32 sequence, tabletype_t tabletype)
{
    if (tabletype == byu16)
        return (((sequence) * 2654435761u) >> ((minmatch*8)-(lz4_hashlog+1)));
    else
        return (((sequence) * 2654435761u) >> ((minmatch*8)-lz4_hashlog));
}

static u32 lz4_hashposition(const byte* p, tabletype_t tabletype) { return lz4_hashsequence(lz4_read32(p), tabletype); }

static void lz4_putpositiononhash(const byte* p, u32 h, void* tablebase, tabletype_t tabletype, const byte* srcbase)
{
    switch (tabletype)
    {
    case byptr: { const byte** hashtable = (const byte**)tablebase; hashtable[h] = p; return; }
    case byu32: { u32* hashtable = (u32*) tablebase; hashtable[h] = (u32)(p-srcbase); return; }
    case byu16: { u16* hashtable = (u16*) tablebase; hashtable[h] = (u16)(p-srcbase); return; }
    }
}

static void lz4_putposition(const byte* p, void* tablebase, tabletype_t tabletype, const byte* srcbase)
{
    u32 h = lz4_hashposition(p, tabletype);
    lz4_putpositiononhash(p, h, tablebase, tabletype, srcbase);
}

static const byte* lz4_getpositiononhash(u32 h, void* tablebase, tabletype_t tabletype, const byte* srcbase)
{
    if (tabletype == byptr) { const byte** hashtable = (const byte**) tablebase; return hashtable[h]; }
    if (tabletype == byu32) { u32* hashtable = (u32*) tablebase; return hashtable[h] + srcbase; }
    { u16* hashtable = (u16*) tablebase; return hashtable[h] + srcbase; }   /* default, to ensure a return */
}

static const byte* lz4_getposition(const byte* p, void* tablebase, tabletype_t tabletype, const byte* srcbase)
{
    u32 h = lz4_hashposition(p, tabletype);
    return lz4_getpositiononhash(h, tablebase, tabletype, srcbase);
}

static int lz4_compress_generic(
                 void* ctx,
                 const char* source,
                 char* dest,
                 int inputsize,
                 int maxoutputsize,
                 limitedoutput_directive outputlimited,
                 tabletype_t tabletype,
                 dict_directive dict,
                 dictissue_directive dictissue)
{
    lz4_stream_t_internal* const dictptr = (lz4_stream_t_internal*)ctx;

    const byte* ip = (const byte*) source;
    const byte* base;
    const byte* lowlimit;
    const byte* const lowreflimit = ip - dictptr->dictsize;
    const byte* const dictionary = dictptr->dictionary;
    const byte* const dictend = dictionary + dictptr->dictsize;
    const size_t dictdelta = dictend - (const byte*)source;
    const byte* anchor = (const byte*) source;
    const byte* const iend = ip + inputsize;
    const byte* const mflimit = iend - mflimit;
    const byte* const matchlimit = iend - lastliterals;

    byte* op = (byte*) dest;
    byte* const olimit = op + maxoutputsize;

    u32 forwardh;
    size_t refdelta=0;

    /* init conditions */
    if ((u32)inputsize > (u32)lz4_max_input_size) return 0;          /* unsupported input size, too large (or negative) */
    switch(dict)
    {
    case nodict:
    default:
        base = (const byte*)source;
        lowlimit = (const byte*)source;
        break;
    case withprefix64k:
        base = (const byte*)source - dictptr->currentoffset;
        lowlimit = (const byte*)source - dictptr->dictsize;
        break;
    case usingextdict:
        base = (const byte*)source - dictptr->currentoffset;
        lowlimit = (const byte*)source;
        break;
    }
    if ((tabletype == byu16) && (inputsize>=lz4_64klimit)) return 0;   /* size too large (not within 64k limit) */
    if (inputsize<lz4_minlength) goto _last_literals;                  /* input too small, no compression (all literals) */

    /* first byte */
    lz4_putposition(ip, ctx, tabletype, base);
    ip++; forwardh = lz4_hashposition(ip, tabletype);

    /* main loop */
    for ( ; ; )
    {
        const byte* match;
        byte* token;
        {
            const byte* forwardip = ip;
            unsigned step=1;
            unsigned searchmatchnb = (1u << lz4_skiptrigger);

            /* find a match */
            do {
                u32 h = forwardh;
                ip = forwardip;
                forwardip += step;
                step = searchmatchnb++ >> lz4_skiptrigger;

                if (unlikely(forwardip > mflimit)) goto _last_literals;

                match = lz4_getpositiononhash(h, ctx, tabletype, base);
                if (dict==usingextdict)
                {
                    if (match<(const byte*)source)
                    {
                        refdelta = dictdelta;
                        lowlimit = dictionary;
                    }
                    else
                    {
                        refdelta = 0;
                        lowlimit = (const byte*)source;
                    }
                }
                forwardh = lz4_hashposition(forwardip, tabletype);
                lz4_putpositiononhash(ip, h, ctx, tabletype, base);

            } while ( ((dictissue==dictsmall) ? (match < lowreflimit) : 0)
                || ((tabletype==byu16) ? 0 : (match + max_distance < ip))
                || (lz4_read32(match+refdelta) != lz4_read32(ip)) );
        }

        /* catch up */
        while ((ip>anchor) && (match+refdelta > lowlimit) && (unlikely(ip[-1]==match[refdelta-1]))) { ip--; match--; }

        {
            /* encode literal length */
            unsigned litlength = (unsigned)(ip - anchor);
            token = op++;
            if ((outputlimited) && (unlikely(op + litlength + (2 + 1 + lastliterals) + (litlength/255) > olimit)))
                return 0;   /* check output limit */
            if (litlength>=run_mask)
            {
                int len = (int)litlength-run_mask;
                *token=(run_mask<<ml_bits);
                for(; len >= 255 ; len-=255) *op++ = 255;
                *op++ = (byte)len;
            }
            else *token = (byte)(litlength<<ml_bits);

            /* copy literals */
            lz4_wildcopy(op, anchor, op+litlength);
            op+=litlength;
        }

_next_match:
        /* encode offset */
        lz4_writele16(op, (u16)(ip-match)); op+=2;

        /* encode matchlength */
        {
            unsigned matchlength;

            if ((dict==usingextdict) && (lowlimit==dictionary))
            {
                const byte* limit;
                match += refdelta;
                limit = ip + (dictend-match);
                if (limit > matchlimit) limit = matchlimit;
                matchlength = lz4_count(ip+minmatch, match+minmatch, limit);
                ip += minmatch + matchlength;
                if (ip==limit)
                {
                    unsigned more = lz4_count(ip, (const byte*)source, matchlimit);
                    matchlength += more;
                    ip += more;
                }
            }
            else
            {
                matchlength = lz4_count(ip+minmatch, match+minmatch, matchlimit);
                ip += minmatch + matchlength;
            }

            if ((outputlimited) && (unlikely(op + (1 + lastliterals) + (matchlength>>8) > olimit)))
                return 0;    /* check output limit */
            if (matchlength>=ml_mask)
            {
                *token += ml_mask;
                matchlength -= ml_mask;
                for (; matchlength >= 510 ; matchlength-=510) { *op++ = 255; *op++ = 255; }
                if (matchlength >= 255) { matchlength-=255; *op++ = 255; }
                *op++ = (byte)matchlength;
            }
            else *token += (byte)(matchlength);
        }

        anchor = ip;

        /* test end of chunk */
        if (ip > mflimit) break;

        /* fill table */
        lz4_putposition(ip-2, ctx, tabletype, base);

        /* test next position */
        match = lz4_getposition(ip, ctx, tabletype, base);
        if (dict==usingextdict)
        {
            if (match<(const byte*)source)
            {
                refdelta = dictdelta;
                lowlimit = dictionary;
            }
            else
            {
                refdelta = 0;
                lowlimit = (const byte*)source;
            }
        }
        lz4_putposition(ip, ctx, tabletype, base);
        if ( ((dictissue==dictsmall) ? (match>=lowreflimit) : 1)
            && (match+max_distance>=ip)
            && (lz4_read32(match+refdelta)==lz4_read32(ip)) )
        { token=op++; *token=0; goto _next_match; }

        /* prepare next loop */
        forwardh = lz4_hashposition(++ip, tabletype);
    }

_last_literals:
    /* encode last literals */
    {
        int lastrun = (int)(iend - anchor);
        if ((outputlimited) && (((char*)op - dest) + lastrun + 1 + ((lastrun+255-run_mask)/255) > (u32)maxoutputsize))
            return 0;   /* check output limit */
        if (lastrun>=(int)run_mask) { *op++=(run_mask<<ml_bits); lastrun-=run_mask; for(; lastrun >= 255 ; lastrun-=255) *op++ = 255; *op++ = (byte) lastrun; }
        else *op++ = (byte)(lastrun<<ml_bits);
        memcpy(op, anchor, iend - anchor);
        op += iend-anchor;
    }

    /* end */
    return (int) (((char*)op)-dest);
}


int lz4_compress(const char* source, char* dest, int inputsize)
{
#if (heapmode)
    void* ctx = allocator(lz4_streamsize_u64, 8);   /* aligned on 8-bytes boundaries */
#else
    u64 ctx[lz4_streamsize_u64] = {0};      /* ensure data is aligned on 8-bytes boundaries */
#endif
    int result;

    if (inputsize < lz4_64klimit)
        result = lz4_compress_generic((void*)ctx, source, dest, inputsize, 0, notlimited, byu16, nodict, nodictissue);
    else
        result = lz4_compress_generic((void*)ctx, source, dest, inputsize, 0, notlimited, lz4_64bits() ? byu32 : byptr, nodict, nodictissue);

#if (heapmode)
    freemem(ctx);
#endif
    return result;
}

int lz4_compress_limitedoutput(const char* source, char* dest, int inputsize, int maxoutputsize)
{
#if (heapmode)
    void* ctx = allocator(lz4_streamsize_u64, 8);   /* aligned on 8-bytes boundaries */
#else
    u64 ctx[lz4_streamsize_u64] = {0};      /* ensure data is aligned on 8-bytes boundaries */
#endif
    int result;

    if (inputsize < lz4_64klimit)
        result = lz4_compress_generic((void*)ctx, source, dest, inputsize, maxoutputsize, limitedoutput, byu16, nodict, nodictissue);
    else
        result = lz4_compress_generic((void*)ctx, source, dest, inputsize, maxoutputsize, limitedoutput, lz4_64bits() ? byu32 : byptr, nodict, nodictissue);

#if (heapmode)
    freemem(ctx);
#endif
    return result;
}


/*****************************************
   experimental : streaming functions
*****************************************/

/*
 * lz4_initstream
 * use this function once, to init a newly allocated lz4_stream_t structure
 * return : 1 if ok, 0 if error
 */
void lz4_resetstream (lz4_stream_t* lz4_stream)
{
    mem_init(lz4_stream, 0, sizeof(lz4_stream_t));
}

lz4_stream_t* lz4_createstream(void)
{
    lz4_stream_t* lz4s = (lz4_stream_t*)allocator(8, lz4_streamsize_u64);
    lz4_static_assert(lz4_streamsize >= sizeof(lz4_stream_t_internal));    /* a compilation error here means lz4_streamsize is not large enough */
    lz4_resetstream(lz4s);
    return lz4s;
}

int lz4_freestream (lz4_stream_t* lz4_stream)
{
    freemem(lz4_stream);
    return (0);
}


int lz4_loaddict (lz4_stream_t* lz4_dict, const char* dictionary, int dictsize)
{
    lz4_stream_t_internal* dict = (lz4_stream_t_internal*) lz4_dict;
    const byte* p = (const byte*)dictionary;
    const byte* const dictend = p + dictsize;
    const byte* base;

    if (dict->initcheck) lz4_resetstream(lz4_dict);                         /* uninitialized structure detected */

    if (dictsize < minmatch)
    {
        dict->dictionary = null;
        dict->dictsize = 0;
        return 0;
    }

    if (p <= dictend - 64 kb) p = dictend - 64 kb;
    base = p - dict->currentoffset;
    dict->dictionary = p;
    dict->dictsize = (u32)(dictend - p);
    dict->currentoffset += dict->dictsize;

    while (p <= dictend-minmatch)
    {
        lz4_putposition(p, dict, byu32, base);
        p+=3;
    }

    return dict->dictsize;
}


static void lz4_renormdictt(lz4_stream_t_internal* lz4_dict, const byte* src)
{
    if ((lz4_dict->currentoffset > 0x80000000) ||
        ((size_t)lz4_dict->currentoffset > (size_t)src))   /* address space overflow */
    {
        /* rescale hash table */
        u32 delta = lz4_dict->currentoffset - 64 kb;
        const byte* dictend = lz4_dict->dictionary + lz4_dict->dictsize;
        int i;
        for (i=0; i<hash_size_u32; i++)
        {
            if (lz4_dict->hashtable[i] < delta) lz4_dict->hashtable[i]=0;
            else lz4_dict->hashtable[i] -= delta;
        }
        lz4_dict->currentoffset = 64 kb;
        if (lz4_dict->dictsize > 64 kb) lz4_dict->dictsize = 64 kb;
        lz4_dict->dictionary = dictend - lz4_dict->dictsize;
    }
}


force_inline int lz4_compress_continue_generic (void* lz4_stream, const char* source, char* dest, int inputsize,
                                                int maxoutputsize, limitedoutput_directive limit)
{
    lz4_stream_t_internal* streamptr = (lz4_stream_t_internal*)lz4_stream;
    const byte* const dictend = streamptr->dictionary + streamptr->dictsize;

    const byte* smallest = (const byte*) source;
    if (streamptr->initcheck) return 0;   /* uninitialized structure detected */
    if ((streamptr->dictsize>0) && (smallest>dictend)) smallest = dictend;
    lz4_renormdictt(streamptr, smallest);

    /* check overlapping input/dictionary space */
    {
        const byte* sourceend = (const byte*) source + inputsize;
        if ((sourceend > streamptr->dictionary) && (sourceend < dictend))
        {
            streamptr->dictsize = (u32)(dictend - sourceend);
            if (streamptr->dictsize > 64 kb) streamptr->dictsize = 64 kb;
            if (streamptr->dictsize < 4) streamptr->dictsize = 0;
            streamptr->dictionary = dictend - streamptr->dictsize;
        }
    }

    /* prefix mode : source data follows dictionary */
    if (dictend == (const byte*)source)
    {
        int result;
        if ((streamptr->dictsize < 64 kb) && (streamptr->dictsize < streamptr->currentoffset))
            result = lz4_compress_generic(lz4_stream, source, dest, inputsize, maxoutputsize, limit, byu32, withprefix64k, dictsmall);
        else
            result = lz4_compress_generic(lz4_stream, source, dest, inputsize, maxoutputsize, limit, byu32, withprefix64k, nodictissue);
        streamptr->dictsize += (u32)inputsize;
        streamptr->currentoffset += (u32)inputsize;
        return result;
    }

    /* external dictionary mode */
    {
        int result;
        if ((streamptr->dictsize < 64 kb) && (streamptr->dictsize < streamptr->currentoffset))
            result = lz4_compress_generic(lz4_stream, source, dest, inputsize, maxoutputsize, limit, byu32, usingextdict, dictsmall);
        else
            result = lz4_compress_generic(lz4_stream, source, dest, inputsize, maxoutputsize, limit, byu32, usingextdict, nodictissue);
        streamptr->dictionary = (const byte*)source;
        streamptr->dictsize = (u32)inputsize;
        streamptr->currentoffset += (u32)inputsize;
        return result;
    }
}

int lz4_compress_continue (lz4_stream_t* lz4_stream, const char* source, char* dest, int inputsize)
{
    return lz4_compress_continue_generic(lz4_stream, source, dest, inputsize, 0, notlimited);
}

int lz4_compress_limitedoutput_continue (lz4_stream_t* lz4_stream, const char* source, char* dest, int inputsize, int maxoutputsize)
{
    return lz4_compress_continue_generic(lz4_stream, source, dest, inputsize, maxoutputsize, limitedoutput);
}


/* hidden debug function, to force separate dictionary mode */
int lz4_compress_forceextdict (lz4_stream_t* lz4_dict, const char* source, char* dest, int inputsize)
{
    lz4_stream_t_internal* streamptr = (lz4_stream_t_internal*)lz4_dict;
    int result;
    const byte* const dictend = streamptr->dictionary + streamptr->dictsize;

    const byte* smallest = dictend;
    if (smallest > (const byte*) source) smallest = (const byte*) source;
    lz4_renormdictt((lz4_stream_t_internal*)lz4_dict, smallest);

    result = lz4_compress_generic(lz4_dict, source, dest, inputsize, 0, notlimited, byu32, usingextdict, nodictissue);

    streamptr->dictionary = (const byte*)source;
    streamptr->dictsize = (u32)inputsize;
    streamptr->currentoffset += (u32)inputsize;

    return result;
}


int lz4_savedict (lz4_stream_t* lz4_dict, char* safebuffer, int dictsize)
{
    lz4_stream_t_internal* dict = (lz4_stream_t_internal*) lz4_dict;
    const byte* previousdictend = dict->dictionary + dict->dictsize;

    if ((u32)dictsize > 64 kb) dictsize = 64 kb;   /* useless to define a dictionary > 64 kb */
    if ((u32)dictsize > dict->dictsize) dictsize = dict->dictsize;

    memmove(safebuffer, previousdictend - dictsize, dictsize);

    dict->dictionary = (const byte*)safebuffer;
    dict->dictsize = (u32)dictsize;

    return dictsize;
}



/****************************
   decompression functions
****************************/
/*
 * this generic decompression function cover all use cases.
 * it shall be instantiated several times, using different sets of directives
 * note that it is essential this generic function is really inlined,
 * in order to remove useless branches during compilation optimization.
 */
force_inline int lz4_decompress_generic(
                 const char* const source,
                 char* const dest,
                 int inputsize,
                 int outputsize,         /* if endoninput==endoninputsize, this value is the max size of output buffer. */

                 int endoninput,         /* endonoutputsize, endoninputsize */
                 int partialdecoding,    /* full, partial */
                 int targetoutputsize,   /* only used if partialdecoding==partial */
                 int dict,               /* nodict, withprefix64k, usingextdict */
                 const byte* const lowprefix,  /* == dest if dict == nodict */
                 const byte* const dictstart,  /* only if dict==usingextdict */
                 const size_t dictsize         /* note : = 0 if nodict */
                 )
{
    /* local variables */
    const byte* restrict ip = (const byte*) source;
    const byte* const iend = ip + inputsize;

    byte* op = (byte*) dest;
    byte* const oend = op + outputsize;
    byte* cpy;
    byte* oexit = op + targetoutputsize;
    const byte* const lowlimit = lowprefix - dictsize;

    const byte* const dictend = (const byte*)dictstart + dictsize;
    const size_t dec32table[] = {4, 1, 2, 1, 4, 4, 4, 4};
    const size_t dec64table[] = {0, 0, 0, (size_t)-1, 0, 1, 2, 3};

    const int safedecode = (endoninput==endoninputsize);
    const int checkoffset = ((safedecode) && (dictsize < (int)(64 kb)));


    /* special cases */
    if ((partialdecoding) && (oexit> oend-mflimit)) oexit = oend-mflimit;                         /* targetoutputsize too high => decode everything */
    if ((endoninput) && (unlikely(outputsize==0))) return ((inputsize==1) && (*ip==0)) ? 0 : -1;  /* empty output buffer */
    if ((!endoninput) && (unlikely(outputsize==0))) return (*ip==0?1:-1);


    /* main loop */
    while (1)
    {
        unsigned token;
        size_t length;
        const byte* match;

        /* get literal length */
        token = *ip++;
        if ((length=(token>>ml_bits)) == run_mask)
        {
            unsigned s;
            do
            {
                s = *ip++;
                length += s;
            }
            while (likely((endoninput)?ip<iend-run_mask:1) && (s==255));
            if ((safedecode) && unlikely((size_t)(op+length)<(size_t)(op))) goto _output_error;   /* overflow detection */
            if ((safedecode) && unlikely((size_t)(ip+length)<(size_t)(ip))) goto _output_error;   /* overflow detection */
        }

        /* copy literals */
        cpy = op+length;
        if (((endoninput) && ((cpy>(partialdecoding?oexit:oend-mflimit)) || (ip+length>iend-(2+1+lastliterals))) )
            || ((!endoninput) && (cpy>oend-copylength)))
        {
            if (partialdecoding)
            {
                if (cpy > oend) goto _output_error;                           /* error : write attempt beyond end of output buffer */
                if ((endoninput) && (ip+length > iend)) goto _output_error;   /* error : read attempt beyond end of input buffer */
            }
            else
            {
                if ((!endoninput) && (cpy != oend)) goto _output_error;       /* error : block decoding must stop exactly there */
                if ((endoninput) && ((ip+length != iend) || (cpy > oend))) goto _output_error;   /* error : input must be consumed */
            }
            memcpy(op, ip, length);
            ip += length;
            op += length;
            break;     /* necessarily eof, due to parsing restrictions */
        }
        lz4_wildcopy(op, ip, cpy);
        ip += length; op = cpy;

        /* get offset */
        match = cpy - lz4_readle16(ip); ip+=2;
        if ((checkoffset) && (unlikely(match < lowlimit))) goto _output_error;   /* error : offset outside destination buffer */

        /* get matchlength */
        length = token & ml_mask;
        if (length == ml_mask)
        {
            unsigned s;
            do
            {
                if ((endoninput) && (ip > iend-lastliterals)) goto _output_error;
                s = *ip++;
                length += s;
            } while (s==255);
            if ((safedecode) && unlikely((size_t)(op+length)<(size_t)op)) goto _output_error;   /* overflow detection */
        }
        length += minmatch;

        /* check external dictionary */
        if ((dict==usingextdict) && (match < lowprefix))
        {
            if (unlikely(op+length > oend-lastliterals)) goto _output_error;   /* doesn't respect parsing restriction */

            if (length <= (size_t)(lowprefix-match))
            {
                /* match can be copied as a single segment from external dictionary */
                match = dictend - (lowprefix-match);
                memcpy(op, match, length);
                op += length;
            }
            else
            {
                /* match encompass external dictionary and current segment */
                size_t copysize = (size_t)(lowprefix-match);
                memcpy(op, dictend - copysize, copysize);
                op += copysize;
                copysize = length - copysize;
                if (copysize > (size_t)(op-lowprefix))   /* overlap within current segment */
                {
                    byte* const endofmatch = op + copysize;
                    const byte* copyfrom = lowprefix;
                    while (op < endofmatch) *op++ = *copyfrom++;
                }
                else
                {
                    memcpy(op, lowprefix, copysize);
                    op += copysize;
                }
            }
            continue;
        }

        /* copy repeated sequence */
        cpy = op + length;
        if (unlikely((op-match)<8))
        {
            const size_t dec64 = dec64table[op-match];
            op[0] = match[0];
            op[1] = match[1];
            op[2] = match[2];
            op[3] = match[3];
            match += dec32table[op-match];
            lz4_copy4(op+4, match);
            op += 8; match -= dec64;
        } else { lz4_copy8(op, match); op+=8; match+=8; }

        if (unlikely(cpy>oend-12))
        {
            if (cpy > oend-lastliterals) goto _output_error;    /* error : last lastliterals bytes must be literals */
            if (op < oend-8)
            {
                lz4_wildcopy(op, match, oend-8);
                match += (oend-8) - op;
                op = oend-8;
            }
            while (op<cpy) *op++ = *match++;
        }
        else
            lz4_wildcopy(op, match, cpy);
        op=cpy;   /* correction */
    }

    /* end of decoding */
    if (endoninput)
       return (int) (((char*)op)-dest);     /* nb of output bytes decoded */
    else
       return (int) (((char*)ip)-source);   /* nb of input bytes read */

    /* overflow error detected */
_output_error:
    return (int) (-(((char*)ip)-source))-1;
}


int lz4_decompress_safe(const char* source, char* dest, int compressedsize, int maxdecompressedsize)
{
    return lz4_decompress_generic(source, dest, compressedsize, maxdecompressedsize, endoninputsize, full, 0, nodict, (byte*)dest, null, 0);
}

int lz4_decompress_safe_partial(const char* source, char* dest, int compressedsize, int targetoutputsize, int maxdecompressedsize)
{
    return lz4_decompress_generic(source, dest, compressedsize, maxdecompressedsize, endoninputsize, partial, targetoutputsize, nodict, (byte*)dest, null, 0);
}

int lz4_decompress_fast(const char* source, char* dest, int originalsize)
{
    return lz4_decompress_generic(source, dest, 0, originalsize, endonoutputsize, full, 0, withprefix64k, (byte*)(dest - 64 kb), null, 64 kb);
}


/* streaming decompression functions */

typedef struct
{
    byte* externaldict;
    size_t extdictsize;
    byte* prefixend;
    size_t prefixsize;
} lz4_streamdecode_t_internal;

/*
 * if you prefer dynamic allocation methods,
 * lz4_createstreamdecode()
 * provides a pointer (void*) towards an initialized lz4_streamdecode_t structure.
 */
lz4_streamdecode_t* lz4_createstreamdecode(void)
{
    lz4_streamdecode_t* lz4s = (lz4_streamdecode_t*) allocator(sizeof(u64), lz4_streamdecodesize_u64);
    return lz4s;
}

int lz4_freestreamdecode (lz4_streamdecode_t* lz4_stream)
{
    freemem(lz4_stream);
    return 0;
}

/*
 * lz4_setstreamdecode
 * use this function to instruct where to find the dictionary
 * this function is not necessary if previous data is still available where it was decoded.
 * loading a size of 0 is allowed (same effect as no dictionary).
 * return : 1 if ok, 0 if error
 */
int lz4_setstreamdecode (lz4_streamdecode_t* lz4_streamdecode, const char* dictionary, int dictsize)
{
    lz4_streamdecode_t_internal* lz4sd = (lz4_streamdecode_t_internal*) lz4_streamdecode;
    lz4sd->prefixsize = (size_t) dictsize;
    lz4sd->prefixend = (byte*) dictionary + dictsize;
    lz4sd->externaldict = null;
    lz4sd->extdictsize  = 0;
    return 1;
}

/*
*_continue() :
    these decoding functions allow decompression of multiple blocks in "streaming" mode.
    previously decoded blocks must still be available at the memory position where they were decoded.
    if it's not possible, save the relevant part of decoded data into a safe buffer,
    and indicate where it stands using lz4_setstreamdecode()
*/
int lz4_decompress_safe_continue (lz4_streamdecode_t* lz4_streamdecode, const char* source, char* dest, int compressedsize, int maxoutputsize)
{
    lz4_streamdecode_t_internal* lz4sd = (lz4_streamdecode_t_internal*) lz4_streamdecode;
    int result;

    if (lz4sd->prefixend == (byte*)dest)
    {
        result = lz4_decompress_generic(source, dest, compressedsize, maxoutputsize,
                                        endoninputsize, full, 0,
                                        usingextdict, lz4sd->prefixend - lz4sd->prefixsize, lz4sd->externaldict, lz4sd->extdictsize);
        if (result <= 0) return result;
        lz4sd->prefixsize += result;
        lz4sd->prefixend  += result;
    }
    else
    {
        lz4sd->extdictsize = lz4sd->prefixsize;
        lz4sd->externaldict = lz4sd->prefixend - lz4sd->extdictsize;
        result = lz4_decompress_generic(source, dest, compressedsize, maxoutputsize,
                                        endoninputsize, full, 0,
                                        usingextdict, (byte*)dest, lz4sd->externaldict, lz4sd->extdictsize);
        if (result <= 0) return result;
        lz4sd->prefixsize = result;
        lz4sd->prefixend  = (byte*)dest + result;
    }

    return result;
}

int lz4_decompress_fast_continue (lz4_streamdecode_t* lz4_streamdecode, const char* source, char* dest, int originalsize)
{
    lz4_streamdecode_t_internal* lz4sd = (lz4_streamdecode_t_internal*) lz4_streamdecode;
    int result;

    if (lz4sd->prefixend == (byte*)dest)
    {
        result = lz4_decompress_generic(source, dest, 0, originalsize,
                                        endonoutputsize, full, 0,
                                        usingextdict, lz4sd->prefixend - lz4sd->prefixsize, lz4sd->externaldict, lz4sd->extdictsize);
        if (result <= 0) return result;
        lz4sd->prefixsize += originalsize;
        lz4sd->prefixend  += originalsize;
    }
    else
    {
        lz4sd->extdictsize = lz4sd->prefixsize;
        lz4sd->externaldict = (byte*)dest - lz4sd->extdictsize;
        result = lz4_decompress_generic(source, dest, 0, originalsize,
                                        endonoutputsize, full, 0,
                                        usingextdict, (byte*)dest, lz4sd->externaldict, lz4sd->extdictsize);
        if (result <= 0) return result;
        lz4sd->prefixsize = originalsize;
        lz4sd->prefixend  = (byte*)dest + originalsize;
    }

    return result;
}


/*
advanced decoding functions :
*_usingdict() :
    these decoding functions work the same as "_continue" ones,
    the dictionary must be explicitly provided within parameters
*/

force_inline int lz4_decompress_usingdict_generic(const char* source, char* dest, int compressedsize, int maxoutputsize, int safe, const char* dictstart, int dictsize)
{
    if (dictsize==0)
        return lz4_decompress_generic(source, dest, compressedsize, maxoutputsize, safe, full, 0, nodict, (byte*)dest, null, 0);
    if (dictstart+dictsize == dest)
    {
        if (dictsize >= (int)(64 kb - 1))
            return lz4_decompress_generic(source, dest, compressedsize, maxoutputsize, safe, full, 0, withprefix64k, (byte*)dest-64 kb, null, 0);
        return lz4_decompress_generic(source, dest, compressedsize, maxoutputsize, safe, full, 0, nodict, (byte*)dest-dictsize, null, 0);
    }
    return lz4_decompress_generic(source, dest, compressedsize, maxoutputsize, safe, full, 0, usingextdict, (byte*)dest, (byte*)dictstart, dictsize);
}

int lz4_decompress_safe_usingdict(const char* source, char* dest, int compressedsize, int maxoutputsize, const char* dictstart, int dictsize)
{
    return lz4_decompress_usingdict_generic(source, dest, compressedsize, maxoutputsize, 1, dictstart, dictsize);
}

int lz4_decompress_fast_usingdict(const char* source, char* dest, int originalsize, const char* dictstart, int dictsize)
{
    return lz4_decompress_usingdict_generic(source, dest, 0, originalsize, 0, dictstart, dictsize);
}

/* debug function */
int lz4_decompress_safe_forceextdict(const char* source, char* dest, int compressedsize, int maxoutputsize, const char* dictstart, int dictsize)
{
    return lz4_decompress_generic(source, dest, compressedsize, maxoutputsize, endoninputsize, full, 0, usingextdict, (byte*)dest, (byte*)dictstart, dictsize);
}


/***************************************************
    obsolete functions
***************************************************/
/*
these function names are deprecated and should no longer be used.
they are only provided here for compatibility with older user programs.
- lz4_uncompress is totally equivalent to lz4_decompress_fast
- lz4_uncompress_unknownoutputsize is totally equivalent to lz4_decompress_safe
*/
int lz4_uncompress (const char* source, char* dest, int outputsize) { return lz4_decompress_fast(source, dest, outputsize); }
int lz4_uncompress_unknownoutputsize (const char* source, char* dest, int isize, int maxoutputsize) { return lz4_decompress_safe(source, dest, isize, maxoutputsize); }


/* obsolete streaming functions */

int lz4_sizeofstreamstate() { return lz4_streamsize; }

static void lz4_init(lz4_stream_t_internal* lz4ds, const byte* base)
{
    mem_init(lz4ds, 0, lz4_streamsize);
    lz4ds->bufferstart = base;
}

int lz4_resetstreamstate(void* state, const char* inputbuffer)
{
    if ((((size_t)state) & 3) != 0) return 1;   /* error : pointer is not aligned on 4-bytes boundary */
    lz4_init((lz4_stream_t_internal*)state, (const byte*)inputbuffer);
    return 0;
}

void* lz4_create (const char* inputbuffer)
{
    void* lz4ds = allocator(8, lz4_streamsize_u64);
    lz4_init ((lz4_stream_t_internal*)lz4ds, (const byte*)inputbuffer);
    return lz4ds;
}

char* lz4_slideinputbuffer (void* lz4_data)
{
    lz4_stream_t_internal* ctx = (lz4_stream_t_internal*)lz4_data;
    int dictsize = lz4_savedict((lz4_stream_t*)ctx, (char*)ctx->bufferstart, 64 kb);
    return (char*)(ctx->bufferstart + dictsize);
}

/*  obsolete compresson functions using user-allocated state */

int lz4_sizeofstate() { return lz4_streamsize; }

int lz4_compress_withstate (void* state, const char* source, char* dest, int inputsize)
{
    if (((size_t)(state)&3) != 0) return 0;   /* error : state is not aligned on 4-bytes boundary */
    mem_init(state, 0, lz4_streamsize);

    if (inputsize < lz4_64klimit)
        return lz4_compress_generic(state, source, dest, inputsize, 0, notlimited, byu16, nodict, nodictissue);
    else
        return lz4_compress_generic(state, source, dest, inputsize, 0, notlimited, lz4_64bits() ? byu32 : byptr, nodict, nodictissue);
}

int lz4_compress_limitedoutput_withstate (void* state, const char* source, char* dest, int inputsize, int maxoutputsize)
{
    if (((size_t)(state)&3) != 0) return 0;   /* error : state is not aligned on 4-bytes boundary */
    mem_init(state, 0, lz4_streamsize);

    if (inputsize < lz4_64klimit)
        return lz4_compress_generic(state, source, dest, inputsize, maxoutputsize, limitedoutput, byu16, nodict, nodictissue);
    else
        return lz4_compress_generic(state, source, dest, inputsize, maxoutputsize, limitedoutput, lz4_64bits() ? byu32 : byptr, nodict, nodictissue);
}

/* obsolete streaming decompression functions */

int lz4_decompress_safe_withprefix64k(const char* source, char* dest, int compressedsize, int maxoutputsize)
{
    return lz4_decompress_generic(source, dest, compressedsize, maxoutputsize, endoninputsize, full, 0, withprefix64k, (byte*)dest - 64 kb, null, 64 kb);
}

int lz4_decompress_fast_withprefix64k(const char* source, char* dest, int originalsize)
{
    return lz4_decompress_generic(source, dest, 0, originalsize, endonoutputsize, full, 0, withprefix64k, (byte*)dest - 64 kb, null, 64 kb);
}

#endif   /* lz4_commondefs_only */

