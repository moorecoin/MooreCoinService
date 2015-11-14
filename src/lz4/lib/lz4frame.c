/*
lz4 auto-framing library
copyright (c) 2011-2014, yann collet.
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
- lz4 source repository : http://code.google.com/p/lz4/
- lz4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/* lz4f is a stand-alone api to create lz4-compressed frames
* fully conformant to specification v1.4.1.
* all related operations, including memory management, are handled by the library.
* */


/**************************************
compiler options
**************************************/
#ifdef _msc_ver    /* visual studio */
#  pragma warning(disable : 4127)        /* disable: c4127: conditional expression is constant */
#endif

#define gcc_version (__gnuc__ * 100 + __gnuc_minor__)
#ifdef __gnuc__
#  pragma gcc diagnostic ignored "-wmissing-braces"   /* gcc bug 53119 : doesn't accept { 0 } as initializer (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119) */
#  pragma gcc diagnostic ignored "-wmissing-field-initializers"   /* gcc bug 53119 : doesn't accept { 0 } as initializer (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119) */
#endif


/**************************************
memory routines
**************************************/
#include <stdlib.h>   /* malloc, calloc, free */
#define allocator(s)   calloc(1,s)
#define freemem        free
#include <string.h>   /* memset, memcpy, memmove */
#define mem_init       memset


/**************************************
includes
**************************************/
#include "lz4frame_static.h"
#include "lz4.h"
#include "lz4hc.h"
#include "xxhash.h"


/**************************************
basic types
**************************************/
#if defined(__stdc_version__) && (__stdc_version__ >= 199901l)   /* c99 */
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
constants
**************************************/
#define kb *(1<<10)
#define mb *(1<<20)
#define gb *(1<<30)

#define _1bit  0x01
#define _2bits 0x03
#define _3bits 0x07
#define _4bits 0x0f
#define _8bits 0xff

#define lz4f_magicnumber 0x184d2204u
#define lz4f_blockuncompressed_flag 0x80000000u
#define lz4f_maxheaderframe_size 7
#define lz4f_blocksizeid_default max64kb

static const u32 minhclevel = 3;

/**************************************
structures and local types
**************************************/
typedef struct
{
    lz4f_preferences_t prefs;
    u32 version;
    u32 cstage;
    size_t maxblocksize;
    size_t maxbuffersize;
    byte*  tmpbuff;
    byte*  tmpin;
    size_t tmpinsize;
    xxh32_state_t xxh;
    void* lz4ctxptr;
    u32 lz4ctxlevel;     /* 0: unallocated;  1: lz4_stream_t;  3: lz4_streamhc_t */
} lz4f_cctx_internal_t;

typedef struct
{
    lz4f_frameinfo_t frameinfo;
    unsigned version;
    unsigned dstage;
    size_t maxblocksize;
    size_t maxbuffersize;
    const byte* srcexpect;
    byte*  tmpin;
    size_t tmpinsize;
    size_t tmpintarget;
    byte*  tmpoutbuffer;
    byte*  dict;
    size_t dictsize;
    byte*  tmpout;
    size_t tmpoutsize;
    size_t tmpoutstart;
    xxh32_state_t xxh;
    byte   header[8];
} lz4f_dctx_internal_t;


/**************************************
macros
**************************************/


/**************************************
error management
**************************************/
#define lz4f_generate_string(string) #string,
static const char* lz4f_errorstrings[] = { lz4f_list_errors(lz4f_generate_string) };


u32 lz4f_iserror(lz4f_errorcode_t code)
{
    return (code > (lz4f_errorcode_t)(-error_maxcode));
}

const char* lz4f_geterrorname(lz4f_errorcode_t code)
{
    static const char* codeerror = "unspecified error code";
    if (lz4f_iserror(code)) return lz4f_errorstrings[-(int)(code)];
    return codeerror;
}


/**************************************
private functions
**************************************/
static size_t lz4f_getblocksize(unsigned blocksizeid)
{
    static const size_t blocksizes[4] = { 64 kb, 256 kb, 1 mb, 4 mb };

    if (blocksizeid == 0) blocksizeid = lz4f_blocksizeid_default;
    blocksizeid -= 4;
    if (blocksizeid > 3) return (size_t)-error_maxblocksize_invalid;
    return blocksizes[blocksizeid];
}


/* unoptimized version; solves endianess & alignment issues */
static void lz4f_writele32 (byte* dstptr, u32 value32)
{
    dstptr[0] = (byte)value32;
    dstptr[1] = (byte)(value32 >> 8);
    dstptr[2] = (byte)(value32 >> 16);
    dstptr[3] = (byte)(value32 >> 24);
}

static u32 lz4f_readle32 (const byte* srcptr)
{
    u32 value32 = srcptr[0];
    value32 += (srcptr[1]<<8);
    value32 += (srcptr[2]<<16);
    value32 += (srcptr[3]<<24);
    return value32;
}


static byte lz4f_headerchecksum (const byte* header, size_t length)
{
    u32 xxh = xxh32(header, (u32)length, 0);
    return (byte)(xxh >> 8);
}


/**************************************
simple compression functions
**************************************/
size_t lz4f_compressframebound(size_t srcsize, const lz4f_preferences_t* preferencesptr)
{
    lz4f_preferences_t prefs = { 0 };
    size_t headersize;
    size_t streamsize;

    if (preferencesptr!=null) prefs = *preferencesptr;
    {
        blocksizeid_t proposedbsid = max64kb;
        size_t maxblocksize = 64 kb;
        while (prefs.frameinfo.blocksizeid > proposedbsid)
        {
            if (srcsize <= maxblocksize)
            {
                prefs.frameinfo.blocksizeid = proposedbsid;
                break;
            }
            proposedbsid++;
            maxblocksize <<= 2;
        }
    }
    prefs.autoflush = 1;

    headersize = 7;      /* basic header size (no option) including magic number */
    streamsize = lz4f_compressbound(srcsize, &prefs);

    return headersize + streamsize;
}


/* lz4f_compressframe()
* compress an entire srcbuffer into a valid lz4 frame, as defined by specification v1.4.1, in a single step.
* the most important rule is that dstbuffer must be large enough (dstmaxsize) to ensure compression completion even in worst case.
* you can get the minimum value of dstmaxsize by using lz4f_compressframebound()
* if this condition is not respected, lz4f_compressframe() will fail (result is an errorcode)
* the lz4f_preferences_t structure is optional : you can provide null as argument. all preferences will be set to default.
* the result of the function is the number of bytes written into dstbuffer.
* the function outputs an error code if it fails (can be tested using lz4f_iserror())
*/
size_t lz4f_compressframe(void* dstbuffer, size_t dstmaxsize, const void* srcbuffer, size_t srcsize, const lz4f_preferences_t* preferencesptr)
{
    lz4f_cctx_internal_t cctxi = { 0 };   /* works because no allocation */
    lz4f_preferences_t prefs = { 0 };
    lz4f_compressoptions_t options = { 0 };
    lz4f_errorcode_t errorcode;
    byte* const dststart = (byte*) dstbuffer;
    byte* dstptr = dststart;
    byte* const dstend = dststart + dstmaxsize;


    cctxi.version = lz4f_version;
    cctxi.maxbuffersize = 5 mb;   /* mess with real buffer size to prevent allocation; works because autoflush==1 & stablesrc==1 */

    if (preferencesptr!=null) prefs = *preferencesptr;
    {
        blocksizeid_t proposedbsid = max64kb;
        size_t maxblocksize = 64 kb;
        while (prefs.frameinfo.blocksizeid > proposedbsid)
        {
            if (srcsize <= maxblocksize)
            {
                prefs.frameinfo.blocksizeid = proposedbsid;
                break;
            }
            proposedbsid++;
            maxblocksize <<= 2;
        }
    }
    prefs.autoflush = 1;
    if (srcsize <= lz4f_getblocksize(prefs.frameinfo.blocksizeid))
        prefs.frameinfo.blockmode = blockindependent;   /* no need for linked blocks */

    options.stablesrc = 1;

    if (dstmaxsize < lz4f_compressframebound(srcsize, &prefs))
        return (size_t)-error_dstmaxsize_toosmall;

    errorcode = lz4f_compressbegin(&cctxi, dstbuffer, dstmaxsize, &prefs);  /* write header */
    if (lz4f_iserror(errorcode)) return errorcode;
    dstptr += errorcode;   /* header size */

    dstmaxsize -= errorcode;
    errorcode = lz4f_compressupdate(&cctxi, dstptr, dstmaxsize, srcbuffer, srcsize, &options);
    if (lz4f_iserror(errorcode)) return errorcode;
    dstptr += errorcode;

    errorcode = lz4f_compressend(&cctxi, dstptr, dstend-dstptr, &options);   /* flush last block, and generate suffix */
    if (lz4f_iserror(errorcode)) return errorcode;
    dstptr += errorcode;

    freemem(cctxi.lz4ctxptr);

    return (dstptr - dststart);
}


/***********************************
* advanced compression functions
* *********************************/

/* lz4f_createcompressioncontext() :
* the first thing to do is to create a compressioncontext object, which will be used in all compression operations.
* this is achieved using lz4f_createcompressioncontext(), which takes as argument a version and an lz4f_preferences_t structure.
* the version provided must be lz4f_version. it is intended to track potential version differences between different binaries.
* the function will provide a pointer to an allocated lz4f_compressioncontext_t object.
* if the result lz4f_errorcode_t is not ok_noerror, there was an error during context creation.
* object can release its memory using lz4f_freecompressioncontext();
*/
lz4f_errorcode_t lz4f_createcompressioncontext(lz4f_compressioncontext_t* lz4f_compressioncontextptr, unsigned version)
{
    lz4f_cctx_internal_t* cctxptr;

    cctxptr = (lz4f_cctx_internal_t*)allocator(sizeof(lz4f_cctx_internal_t));
    if (cctxptr==null) return (lz4f_errorcode_t)(-error_allocation_failed);

    cctxptr->version = version;
    cctxptr->cstage = 0;   /* next stage : write header */

    *lz4f_compressioncontextptr = (lz4f_compressioncontext_t)cctxptr;

    return ok_noerror;
}


lz4f_errorcode_t lz4f_freecompressioncontext(lz4f_compressioncontext_t lz4f_compressioncontext)
{
    lz4f_cctx_internal_t* cctxptr = (lz4f_cctx_internal_t*)lz4f_compressioncontext;

    freemem(cctxptr->lz4ctxptr);
    freemem(cctxptr->tmpbuff);
    freemem(lz4f_compressioncontext);

    return ok_noerror;
}


/* lz4f_compressbegin() :
* will write the frame header into dstbuffer.
* dstbuffer must be large enough to accommodate a header (dstmaxsize). maximum header size is lz4f_maxheaderframe_size bytes.
* the result of the function is the number of bytes written into dstbuffer for the header
* or an error code (can be tested using lz4f_iserror())
*/
size_t lz4f_compressbegin(lz4f_compressioncontext_t compressioncontext, void* dstbuffer, size_t dstmaxsize, const lz4f_preferences_t* preferencesptr)
{
    lz4f_preferences_t prefnull = { 0 };
    lz4f_cctx_internal_t* cctxptr = (lz4f_cctx_internal_t*)compressioncontext;
    byte* const dststart = (byte*)dstbuffer;
    byte* dstptr = dststart;
    byte* headerstart;
    size_t requiredbuffsize;

    if (dstmaxsize < lz4f_maxheaderframe_size) return (size_t)-error_dstmaxsize_toosmall;
    if (cctxptr->cstage != 0) return (size_t)-error_generic;
    if (preferencesptr == null) preferencesptr = &prefnull;
    cctxptr->prefs = *preferencesptr;

    /* ctx management */
    {
        u32 targetctxlevel = cctxptr->prefs.compressionlevel<minhclevel ? 1 : 2;
        if (cctxptr->lz4ctxlevel < targetctxlevel)
        {
            freemem(cctxptr->lz4ctxptr);
            if (cctxptr->prefs.compressionlevel<minhclevel)
                cctxptr->lz4ctxptr = (void*)lz4_createstream();
            else
                cctxptr->lz4ctxptr = (void*)lz4_createstreamhc();
            cctxptr->lz4ctxlevel = targetctxlevel;
        }
    }

    /* buffer management */
    if (cctxptr->prefs.frameinfo.blocksizeid == 0) cctxptr->prefs.frameinfo.blocksizeid = lz4f_blocksizeid_default;
    cctxptr->maxblocksize = lz4f_getblocksize(cctxptr->prefs.frameinfo.blocksizeid);

    requiredbuffsize = cctxptr->maxblocksize + ((cctxptr->prefs.frameinfo.blockmode == blocklinked) * 128 kb);
    if (preferencesptr->autoflush)
        requiredbuffsize = (cctxptr->prefs.frameinfo.blockmode == blocklinked) * 64 kb;   /* just needs dict */

    if (cctxptr->maxbuffersize < requiredbuffsize)
    {
        cctxptr->maxbuffersize = requiredbuffsize;
        freemem(cctxptr->tmpbuff);
        cctxptr->tmpbuff = (byte*)allocator(requiredbuffsize);
        if (cctxptr->tmpbuff == null) return (size_t)-error_allocation_failed;
    }
    cctxptr->tmpin = cctxptr->tmpbuff;
    cctxptr->tmpinsize = 0;
    xxh32_reset(&(cctxptr->xxh), 0);
    if (cctxptr->prefs.compressionlevel<minhclevel)
        lz4_resetstream((lz4_stream_t*)(cctxptr->lz4ctxptr));
    else
        lz4_resetstreamhc((lz4_streamhc_t*)(cctxptr->lz4ctxptr), cctxptr->prefs.compressionlevel);

    /* magic number */
    lz4f_writele32(dstptr, lz4f_magicnumber);
    dstptr += 4;
    headerstart = dstptr;

    /* flg byte */
    *dstptr++ = ((1 & _2bits) << 6)    /* version('01') */
        + ((cctxptr->prefs.frameinfo.blockmode & _1bit ) << 5)    /* block mode */
        + (char)((cctxptr->prefs.frameinfo.contentchecksumflag & _1bit ) << 2);   /* stream checksum */
    /* bd byte */
    *dstptr++ = (char)((cctxptr->prefs.frameinfo.blocksizeid & _3bits) << 4);
    /* crc byte */
    *dstptr++ = lz4f_headerchecksum(headerstart, 2);

    cctxptr->cstage = 1;   /* header written, wait for data block */

    return (dstptr - dststart);
}


/* lz4f_compressbound() : gives the size of dst buffer given a srcsize to handle worst case situations.
*                        the lz4f_frameinfo_t structure is optional :
*                        you can provide null as argument, all preferences will then be set to default.
* */
size_t lz4f_compressbound(size_t srcsize, const lz4f_preferences_t* preferencesptr)
{
    const lz4f_preferences_t prefsnull = { 0 };
    const lz4f_preferences_t* prefsptr = (preferencesptr==null) ? &prefsnull : preferencesptr;
    blocksizeid_t bid = prefsptr->frameinfo.blocksizeid;
    size_t blocksize = lz4f_getblocksize(bid);
    unsigned nbblocks = (unsigned)(srcsize / blocksize) + 1;
    size_t lastblocksize = prefsptr->autoflush ? srcsize % blocksize : blocksize;
    size_t blockinfo = 4;   /* default, without block crc option */
    size_t frameend = 4 + (prefsptr->frameinfo.contentchecksumflag*4);
    size_t result = (blockinfo * nbblocks) + (blocksize * (nbblocks-1)) + lastblocksize + frameend;

    return result;
}


typedef int (*compressfunc_t)(void* ctx, const char* src, char* dst, int srcsize, int dstsize, int level);

static size_t lz4f_compressblock(void* dst, const void* src, size_t srcsize, compressfunc_t compress, void* lz4ctx, int level)
{
    /* compress one block */
    byte* csizeptr = (byte*)dst;
    u32 csize;
    csize = (u32)compress(lz4ctx, (const char*)src, (char*)(csizeptr+4), (int)(srcsize), (int)(srcsize-1), level);
    lz4f_writele32(csizeptr, csize);
    if (csize == 0)   /* compression failed */
    {
        csize = (u32)srcsize;
        lz4f_writele32(csizeptr, csize + lz4f_blockuncompressed_flag);
        memcpy(csizeptr+4, src, srcsize);
    }
    return csize + 4;
}


static int lz4f_locallz4_compress_limitedoutput_withstate(void* ctx, const char* src, char* dst, int srcsize, int dstsize, int level)
{
    (void) level;
    return lz4_compress_limitedoutput_withstate(ctx, src, dst, srcsize, dstsize);
}

static int lz4f_locallz4_compress_limitedoutput_continue(void* ctx, const char* src, char* dst, int srcsize, int dstsize, int level)
{
    (void) level;
    return lz4_compress_limitedoutput_continue((lz4_stream_t*)ctx, src, dst, srcsize, dstsize);
}

static int lz4f_locallz4_compresshc_limitedoutput_continue(void* ctx, const char* src, char* dst, int srcsize, int dstsize, int level)
{
    (void) level;
    return lz4_compresshc_limitedoutput_continue((lz4_streamhc_t*)ctx, src, dst, srcsize, dstsize);
}

static compressfunc_t lz4f_selectcompression(blockmode_t blockmode, u32 level)
{
    if (level < minhclevel)
    {
        if (blockmode == blockindependent) return lz4f_locallz4_compress_limitedoutput_withstate;
        return lz4f_locallz4_compress_limitedoutput_continue;
    }
    if (blockmode == blockindependent) return lz4_compresshc2_limitedoutput_withstatehc;
    return lz4f_locallz4_compresshc_limitedoutput_continue;
}

static int lz4f_localsavedict(lz4f_cctx_internal_t* cctxptr)
{
    if (cctxptr->prefs.compressionlevel < minhclevel)
        return lz4_savedict ((lz4_stream_t*)(cctxptr->lz4ctxptr), (char*)(cctxptr->tmpbuff), 64 kb);
    return lz4_savedicthc ((lz4_streamhc_t*)(cctxptr->lz4ctxptr), (char*)(cctxptr->tmpbuff), 64 kb);
}

typedef enum { notdone, fromtmpbuffer, fromsrcbuffer } lz4f_lastblockstatus;

/* lz4f_compressupdate()
* lz4f_compressupdate() can be called repetitively to compress as much data as necessary.
* the most important rule is that dstbuffer must be large enough (dstmaxsize) to ensure compression completion even in worst case.
* if this condition is not respected, lz4f_compress() will fail (result is an errorcode)
* you can get the minimum value of dstmaxsize by using lz4f_compressbound()
* the lz4f_compressoptions_t structure is optional : you can provide null as argument.
* the result of the function is the number of bytes written into dstbuffer : it can be zero, meaning input data was just buffered.
* the function outputs an error code if it fails (can be tested using lz4f_iserror())
*/
size_t lz4f_compressupdate(lz4f_compressioncontext_t compressioncontext, void* dstbuffer, size_t dstmaxsize, const void* srcbuffer, size_t srcsize, const lz4f_compressoptions_t* compressoptionsptr)
{
    lz4f_compressoptions_t coptionsnull = { 0 };
    lz4f_cctx_internal_t* cctxptr = (lz4f_cctx_internal_t*)compressioncontext;
    size_t blocksize = cctxptr->maxblocksize;
    const byte* srcptr = (const byte*)srcbuffer;
    const byte* const srcend = srcptr + srcsize;
    byte* const dststart = (byte*)dstbuffer;
    byte* dstptr = dststart;
    lz4f_lastblockstatus lastblockcompressed = notdone;
    compressfunc_t compress;


    if (cctxptr->cstage != 1) return (size_t)-error_generic;
    if (dstmaxsize < lz4f_compressbound(srcsize, &(cctxptr->prefs))) return (size_t)-error_dstmaxsize_toosmall;
    if (compressoptionsptr == null) compressoptionsptr = &coptionsnull;

    /* select compression function */
    compress = lz4f_selectcompression(cctxptr->prefs.frameinfo.blockmode, cctxptr->prefs.compressionlevel);

    /* complete tmp buffer */
    if (cctxptr->tmpinsize > 0)   /* some data already within tmp buffer */
    {
        size_t sizetocopy = blocksize - cctxptr->tmpinsize;
        if (sizetocopy > srcsize)
        {
            /* add src to tmpin buffer */
            memcpy(cctxptr->tmpin + cctxptr->tmpinsize, srcbuffer, srcsize);
            srcptr = srcend;
            cctxptr->tmpinsize += srcsize;
            /* still needs some crc */
        }
        else
        {
            /* complete tmpin block and then compress it */
            lastblockcompressed = fromtmpbuffer;
            memcpy(cctxptr->tmpin + cctxptr->tmpinsize, srcbuffer, sizetocopy);
            srcptr += sizetocopy;

            dstptr += lz4f_compressblock(dstptr, cctxptr->tmpin, blocksize, compress, cctxptr->lz4ctxptr, cctxptr->prefs.compressionlevel);

            if (cctxptr->prefs.frameinfo.blockmode==blocklinked) cctxptr->tmpin += blocksize;
            cctxptr->tmpinsize = 0;
        }
    }

    while ((size_t)(srcend - srcptr) >= blocksize)
    {
        /* compress full block */
        lastblockcompressed = fromsrcbuffer;
        dstptr += lz4f_compressblock(dstptr, srcptr, blocksize, compress, cctxptr->lz4ctxptr, cctxptr->prefs.compressionlevel);
        srcptr += blocksize;
    }

    if ((cctxptr->prefs.autoflush) && (srcptr < srcend))
    {
        /* compress remaining input < blocksize */
        lastblockcompressed = fromsrcbuffer;
        dstptr += lz4f_compressblock(dstptr, srcptr, srcend - srcptr, compress, cctxptr->lz4ctxptr, cctxptr->prefs.compressionlevel);
        srcptr  = srcend;
    }

    /* preserve dictionary if necessary */
    if ((cctxptr->prefs.frameinfo.blockmode==blocklinked) && (lastblockcompressed==fromsrcbuffer))
    {
        if (compressoptionsptr->stablesrc)
        {
            cctxptr->tmpin = cctxptr->tmpbuff;
        }
        else
        {
            int realdictsize = lz4f_localsavedict(cctxptr);
            if (realdictsize==0) return (size_t)-error_generic;
            cctxptr->tmpin = cctxptr->tmpbuff + realdictsize;
        }
    }

    /* keep tmpin within limits */
    if ((cctxptr->tmpin + blocksize) > (cctxptr->tmpbuff + cctxptr->maxbuffersize)   /* necessarily blocklinked && lastblockcompressed==fromtmpbuffer */
        && !(cctxptr->prefs.autoflush))
    {
        lz4f_localsavedict(cctxptr);
        cctxptr->tmpin = cctxptr->tmpbuff + 64 kb;
    }

    /* some input data left, necessarily < blocksize */
    if (srcptr < srcend)
    {
        /* fill tmp buffer */
        size_t sizetocopy = srcend - srcptr;
        memcpy(cctxptr->tmpin, srcptr, sizetocopy);
        cctxptr->tmpinsize = sizetocopy;
    }

    if (cctxptr->prefs.frameinfo.contentchecksumflag == contentchecksumenabled)
        xxh32_update(&(cctxptr->xxh), srcbuffer, (unsigned)srcsize);

    return dstptr - dststart;
}


/* lz4f_flush()
* should you need to create compressed data immediately, without waiting for a block to be filled,
* you can call lz4_flush(), which will immediately compress any remaining data stored within compressioncontext.
* the result of the function is the number of bytes written into dstbuffer
* (it can be zero, this means there was no data left within compressioncontext)
* the function outputs an error code if it fails (can be tested using lz4f_iserror())
* the lz4f_compressoptions_t structure is optional : you can provide null as argument.
*/
size_t lz4f_flush(lz4f_compressioncontext_t compressioncontext, void* dstbuffer, size_t dstmaxsize, const lz4f_compressoptions_t* compressoptionsptr)
{
    lz4f_compressoptions_t coptionsnull = { 0 };
    lz4f_cctx_internal_t* cctxptr = (lz4f_cctx_internal_t*)compressioncontext;
    byte* const dststart = (byte*)dstbuffer;
    byte* dstptr = dststart;
    compressfunc_t compress;


    if (cctxptr->tmpinsize == 0) return 0;   /* nothing to flush */
    if (cctxptr->cstage != 1) return (size_t)-error_generic;
    if (dstmaxsize < (cctxptr->tmpinsize + 16)) return (size_t)-error_dstmaxsize_toosmall;
    if (compressoptionsptr == null) compressoptionsptr = &coptionsnull;
    (void)compressoptionsptr;   /* not yet useful */

    /* select compression function */
    compress = lz4f_selectcompression(cctxptr->prefs.frameinfo.blockmode, cctxptr->prefs.compressionlevel);

    /* compress tmp buffer */
    dstptr += lz4f_compressblock(dstptr, cctxptr->tmpin, cctxptr->tmpinsize, compress, cctxptr->lz4ctxptr, cctxptr->prefs.compressionlevel);
    if (cctxptr->prefs.frameinfo.blockmode==blocklinked) cctxptr->tmpin += cctxptr->tmpinsize;
    cctxptr->tmpinsize = 0;

    /* keep tmpin within limits */
    if ((cctxptr->tmpin + cctxptr->maxblocksize) > (cctxptr->tmpbuff + cctxptr->maxbuffersize))   /* necessarily blocklinked */
    {
        lz4f_localsavedict(cctxptr);
        cctxptr->tmpin = cctxptr->tmpbuff + 64 kb;
    }

    return dstptr - dststart;
}


/* lz4f_compressend()
* when you want to properly finish the compressed frame, just call lz4f_compressend().
* it will flush whatever data remained within compressioncontext (like lz4_flush())
* but also properly finalize the frame, with an endmark and a checksum.
* the result of the function is the number of bytes written into dstbuffer (necessarily >= 4 (endmark size))
* the function outputs an error code if it fails (can be tested using lz4f_iserror())
* the lz4f_compressoptions_t structure is optional : you can provide null as argument.
* compressioncontext can then be used again, starting with lz4f_compressbegin(). the preferences will remain the same.
*/
size_t lz4f_compressend(lz4f_compressioncontext_t compressioncontext, void* dstbuffer, size_t dstmaxsize, const lz4f_compressoptions_t* compressoptionsptr)
{
    lz4f_cctx_internal_t* cctxptr = (lz4f_cctx_internal_t*)compressioncontext;
    byte* const dststart = (byte*)dstbuffer;
    byte* dstptr = dststart;
    size_t errorcode;

    errorcode = lz4f_flush(compressioncontext, dstbuffer, dstmaxsize, compressoptionsptr);
    if (lz4f_iserror(errorcode)) return errorcode;
    dstptr += errorcode;

    lz4f_writele32(dstptr, 0);
    dstptr+=4;   /* endmark */

    if (cctxptr->prefs.frameinfo.contentchecksumflag == contentchecksumenabled)
    {
        u32 xxh = xxh32_digest(&(cctxptr->xxh));
        lz4f_writele32(dstptr, xxh);
        dstptr+=4;   /* content checksum */
    }

    cctxptr->cstage = 0;   /* state is now re-usable (with identical preferences) */

    return dstptr - dststart;
}


/***********************************
* decompression functions
* *********************************/

/* resource management */

/* lz4f_createdecompressioncontext() :
* the first thing to do is to create a decompressioncontext object, which will be used in all decompression operations.
* this is achieved using lz4f_createdecompressioncontext().
* the function will provide a pointer to a fully allocated and initialized lz4f_decompressioncontext object.
* if the result lz4f_errorcode_t is not zero, there was an error during context creation.
* object can release its memory using lz4f_freedecompressioncontext();
*/
lz4f_errorcode_t lz4f_createdecompressioncontext(lz4f_compressioncontext_t* lz4f_decompressioncontextptr, unsigned versionnumber)
{
    lz4f_dctx_internal_t* dctxptr;

    dctxptr = allocator(sizeof(lz4f_dctx_internal_t));
    if (dctxptr==null) return (lz4f_errorcode_t)-error_generic;

    dctxptr->version = versionnumber;
    *lz4f_decompressioncontextptr = (lz4f_compressioncontext_t)dctxptr;
    return ok_noerror;
}

lz4f_errorcode_t lz4f_freedecompressioncontext(lz4f_compressioncontext_t lz4f_decompressioncontext)
{
    lz4f_dctx_internal_t* dctxptr = (lz4f_dctx_internal_t*)lz4f_decompressioncontext;
    freemem(dctxptr->tmpin);
    freemem(dctxptr->tmpoutbuffer);
    freemem(dctxptr);
    return ok_noerror;
}


/* decompression */

static size_t lz4f_decodeheader(lz4f_dctx_internal_t* dctxptr, const byte* srcptr, size_t srcsize)
{
    byte flg, bd, hc;
    unsigned version, blockmode, blockchecksumflag, contentsizeflag, contentchecksumflag, dictflag, blocksizeid;
    size_t bufferneeded;

    /* need to decode header to get frameinfo */
    if (srcsize < 7) return (size_t)-error_generic;   /* minimal header size */

    /* control magic number */
    if (lz4f_readle32(srcptr) != lz4f_magicnumber) return (size_t)-error_generic;
    srcptr += 4;

    /* flags */
    flg = srcptr[0];
    version = (flg>>6)&_2bits;
    blockmode = (flg>>5) & _1bit;
    blockchecksumflag = (flg>>4) & _1bit;
    contentsizeflag = (flg>>3) & _1bit;
    contentchecksumflag = (flg>>2) & _1bit;
    dictflag = (flg>>0) & _1bit;
    bd = srcptr[1];
    blocksizeid = (bd>>4) & _3bits;

    /* check */
    hc = lz4f_headerchecksum(srcptr, 2);
    if (hc != srcptr[2]) return (size_t)-error_generic;   /* bad header checksum error */

    /* validate */
    if (version != 1) return (size_t)-error_generic;   /* version number, only supported value */
    if (blockchecksumflag != 0) return (size_t)-error_generic;   /* only supported value for the time being */
    if (contentsizeflag != 0) return (size_t)-error_generic;   /* only supported value for the time being */
    if (((flg>>1)&_1bit) != 0) return (size_t)-error_generic;   /* reserved bit */
    if (dictflag != 0) return (size_t)-error_generic;   /* only supported value for the time being */
    if (((bd>>7)&_1bit) != 0) return (size_t)-error_generic;   /* reserved bit */
    if (blocksizeid < 4) return (size_t)-error_generic;   /* only supported values for the time being */
    if (((bd>>0)&_4bits) != 0) return (size_t)-error_generic;   /* reserved bits */

    /* save */
    dctxptr->frameinfo.blockmode = blockmode;
    dctxptr->frameinfo.contentchecksumflag = contentchecksumflag;
    dctxptr->frameinfo.blocksizeid = blocksizeid;
    dctxptr->maxblocksize = lz4f_getblocksize(blocksizeid);

    /* init */
    if (contentchecksumflag) xxh32_reset(&(dctxptr->xxh), 0);

    /* alloc */
    bufferneeded = dctxptr->maxblocksize + ((dctxptr->frameinfo.blockmode==blocklinked) * 128 kb);
    if (bufferneeded > dctxptr->maxbuffersize)   /* tmp buffers too small */
    {
        freemem(dctxptr->tmpin);
        freemem(dctxptr->tmpoutbuffer);
        dctxptr->maxbuffersize = bufferneeded;
        dctxptr->tmpin = allocator(dctxptr->maxblocksize);
        if (dctxptr->tmpin == null) return (size_t)-error_generic;
        dctxptr->tmpoutbuffer= allocator(dctxptr->maxbuffersize);
        if (dctxptr->tmpoutbuffer== null) return (size_t)-error_generic;
    }
    dctxptr->tmpinsize = 0;
    dctxptr->tmpintarget = 0;
    dctxptr->dict = dctxptr->tmpoutbuffer;
    dctxptr->dictsize = 0;
    dctxptr->tmpout = dctxptr->tmpoutbuffer;
    dctxptr->tmpoutstart = 0;
    dctxptr->tmpoutsize = 0;

    return 7;
}


typedef enum { dstage_getheader=0, dstage_storeheader, dstage_decodeheader,
    dstage_getcblocksize, dstage_storecblocksize, dstage_decodecblocksize,
    dstage_copydirect,
    dstage_getcblock, dstage_storecblock, dstage_decodecblock,
    dstage_decodecblock_intodst, dstage_decodecblock_intotmp, dstage_flushout,
    dstage_getsuffix, dstage_storesuffix, dstage_checksuffix
} dstage_t;


/* lz4f_getframeinfo()
* this function decodes frame header information, such as blocksize.
* it is optional : you could start by calling directly lz4f_decompress() instead.
* the objective is to extract header information without starting decompression, typically for allocation purposes.
* lz4f_getframeinfo() can also be used *after* starting decompression, on a valid lz4f_decompressioncontext_t.
* the number of bytes read from srcbuffer will be provided within *srcsizeptr (necessarily <= original value).
* you are expected to resume decompression from where it stopped (srcbuffer + *srcsizeptr)
* the function result is an hint of the better srcsize to use for next call to lz4f_decompress,
* or an error code which can be tested using lz4f_iserror().
*/
lz4f_errorcode_t lz4f_getframeinfo(lz4f_decompressioncontext_t decompressioncontext, lz4f_frameinfo_t* frameinfoptr, const void* srcbuffer, size_t* srcsizeptr)
{
    lz4f_dctx_internal_t* dctxptr = (lz4f_dctx_internal_t*)decompressioncontext;

    if (dctxptr->dstage == dstage_getheader)
    {
        lz4f_errorcode_t errorcode = lz4f_decodeheader(dctxptr, srcbuffer, *srcsizeptr);
        if (lz4f_iserror(errorcode)) return errorcode;
        *srcsizeptr = errorcode;
        *frameinfoptr = dctxptr->frameinfo;
        dctxptr->srcexpect = null;
        dctxptr->dstage = dstage_getcblocksize;
        return 4;
    }

    /* frameinfo already decoded */
    *srcsizeptr = 0;
    *frameinfoptr = dctxptr->frameinfo;
    return 0;
}


static int lz4f_decompress_safe (const char* source, char* dest, int compressedsize, int maxdecompressedsize, const char* dictstart, int dictsize)
{
    (void)dictstart;
    (void)dictsize;
    return lz4_decompress_safe (source, dest, compressedsize, maxdecompressedsize);
}



static void lz4f_updatedict(lz4f_dctx_internal_t* dctxptr, const byte* dstptr, size_t dstsize, const byte* dstptr0, unsigned withintmp)
{
    if (dctxptr->dictsize==0)
        dctxptr->dict = (byte*)dstptr;   /* priority to dictionary continuity */

    if (dctxptr->dict + dctxptr->dictsize == dstptr)   /* dictionary continuity */
    {
        dctxptr->dictsize += dstsize;
        return;
    }

    if (dstptr - dstptr0 + dstsize >= 64 kb)   /* dstbuffer large enough to become dictionary */
    {
        dctxptr->dict = (byte*)dstptr0;
        dctxptr->dictsize = dstptr - dstptr0 + dstsize;
        return;
    }

    if ((withintmp) && (dctxptr->dict == dctxptr->tmpoutbuffer))
    {
        /* assumption : dctxptr->dict + dctxptr->dictsize == dctxptr->tmpout + dctxptr->tmpoutstart */
        dctxptr->dictsize += dstsize;
        return;
    }

    if (withintmp) /* copy relevant dict portion in front of tmpout within tmpoutbuffer */
    {
#if 0
        size_t saveddictsize = dctxptr->tmpout - dctxptr->tmpoutbuffer;
        memcpy(dctxptr->tmpoutbuffer, dctxptr->dict + dctxptr->dictsize - dctxptr->tmpoutstart- saveddictsize, saveddictsize);
        dctxptr->dict = dctxptr->tmpoutbuffer;
        dctxptr->dictsize = saveddictsize + dctxptr->tmpoutstart + dstsize;
        return;

#else

        size_t preservesize = dctxptr->tmpout - dctxptr->tmpoutbuffer;
        size_t copysize = 64 kb - dctxptr->tmpoutsize;
        byte* olddictend = dctxptr->dict + dctxptr->dictsize - dctxptr->tmpoutstart;
        if (dctxptr->tmpoutsize > 64 kb) copysize = 0;
        if (copysize > preservesize) copysize = preservesize;

        memcpy(dctxptr->tmpoutbuffer + preservesize - copysize, olddictend - copysize, copysize);

        dctxptr->dict = dctxptr->tmpoutbuffer;
        dctxptr->dictsize = preservesize + dctxptr->tmpoutstart + dstsize;
        return;
#endif
    }

    if (dctxptr->dict == dctxptr->tmpoutbuffer)     /* copy dst into tmp to complete dict */
    {
        if (dctxptr->dictsize + dstsize > dctxptr->maxbuffersize)   /* tmp buffer not large enough */
        {
            size_t preservesize = 64 kb - dstsize;   /* note : dstsize < 64 kb */
            memcpy(dctxptr->dict, dctxptr->dict + dctxptr->dictsize - preservesize, preservesize);
            dctxptr->dictsize = preservesize;
        }
        memcpy(dctxptr->dict + dctxptr->dictsize, dstptr, dstsize);
        dctxptr->dictsize += dstsize;
        return;
    }

    /* join dict & dest into tmp */
    {
        size_t preservesize = 64 kb - dstsize;   /* note : dstsize < 64 kb */
        if (preservesize > dctxptr->dictsize) preservesize = dctxptr->dictsize;
        memcpy(dctxptr->tmpoutbuffer, dctxptr->dict + dctxptr->dictsize - preservesize, preservesize);
        memcpy(dctxptr->tmpoutbuffer + preservesize, dstptr, dstsize);
        dctxptr->dict = dctxptr->tmpoutbuffer;
        dctxptr->dictsize = preservesize + dstsize;
    }
}



/* lz4f_decompress()
* call this function repetitively to regenerate data compressed within srcbuffer.
* the function will attempt to decode *srcsizeptr from srcbuffer, into dstbuffer of maximum size *dstsizeptr.
*
* the number of bytes regenerated into dstbuffer will be provided within *dstsizeptr (necessarily <= original value).
*
* the number of bytes effectively read from srcbuffer will be provided within *srcsizeptr (necessarily <= original value).
* if the number of bytes read is < number of bytes provided, then the decompression operation is not complete.
* you will have to call it again, continuing from where it stopped.
*
* the function result is an hint of the better srcsize to use for next call to lz4f_decompress.
* basically, it's the size of the current (or remaining) compressed block + header of next block.
* respecting the hint provides some boost to performance, since it allows less buffer shuffling.
* note that this is just a hint, you can always provide any srcsize you want.
* when a frame is fully decoded, the function result will be 0.
* if decompression failed, function result is an error code which can be tested using lz4f_iserror().
*/
size_t lz4f_decompress(lz4f_decompressioncontext_t decompressioncontext,
                       void* dstbuffer, size_t* dstsizeptr,
                       const void* srcbuffer, size_t* srcsizeptr,
                       const lz4f_decompressoptions_t* decompressoptionsptr)
{
    lz4f_dctx_internal_t* dctxptr = (lz4f_dctx_internal_t*)decompressioncontext;
    static const lz4f_decompressoptions_t optionsnull = { 0 };
    const byte* const srcstart = (const byte*)srcbuffer;
    const byte* const srcend = srcstart + *srcsizeptr;
    const byte* srcptr = srcstart;
    byte* const dststart = (byte*)dstbuffer;
    byte* const dstend = dststart + *dstsizeptr;
    byte* dstptr = dststart;
    const byte* selectedin=null;
    unsigned doanotherstage = 1;
    size_t nextsrcsizehint = 1;


    if (decompressoptionsptr==null) decompressoptionsptr = &optionsnull;
    *srcsizeptr = 0;
    *dstsizeptr = 0;

    /* expect to continue decoding src buffer where it left previously */
    if (dctxptr->srcexpect != null)
    {
        if (srcstart != dctxptr->srcexpect) return (size_t)-error_generic;
    }

    /* programmed as a state machine */

    while (doanotherstage)
    {

        switch(dctxptr->dstage)
        {

        case dstage_getheader:
            {
                if (srcend-srcptr >= 7)
                {
                    selectedin = srcptr;
                    srcptr += 7;
                    dctxptr->dstage = dstage_decodeheader;
                    break;
                }
                dctxptr->tmpinsize = 0;
                dctxptr->dstage = dstage_storeheader;
                break;
            }

        case dstage_storeheader:
            {
                size_t sizetocopy = 7 - dctxptr->tmpinsize;
                if (sizetocopy > (size_t)(srcend - srcptr)) sizetocopy =  srcend - srcptr;
                memcpy(dctxptr->header + dctxptr->tmpinsize, srcptr, sizetocopy);
                dctxptr->tmpinsize += sizetocopy;
                srcptr += sizetocopy;
                if (dctxptr->tmpinsize < 7)
                {
                    nextsrcsizehint = (7 - dctxptr->tmpinsize) + 4;
                    doanotherstage = 0;   /* no enough src, wait to get some more */
                    break;
                }
                selectedin = dctxptr->header;
                dctxptr->dstage = dstage_decodeheader;
                break;
            }

        case dstage_decodeheader:
            {
                lz4f_errorcode_t errorcode = lz4f_decodeheader(dctxptr, selectedin, 7);
                if (lz4f_iserror(errorcode)) return errorcode;
                dctxptr->dstage = dstage_getcblocksize;
                break;
            }

        case dstage_getcblocksize:
            {
                if ((srcend - srcptr) >= 4)
                {
                    selectedin = srcptr;
                    srcptr += 4;
                    dctxptr->dstage = dstage_decodecblocksize;
                    break;
                }
                /* not enough input to read cblocksize field */
                dctxptr->tmpinsize = 0;
                dctxptr->dstage = dstage_storecblocksize;
                break;
            }

        case dstage_storecblocksize:
            {
                size_t sizetocopy = 4 - dctxptr->tmpinsize;
                if (sizetocopy > (size_t)(srcend - srcptr)) sizetocopy = srcend - srcptr;
                memcpy(dctxptr->tmpin + dctxptr->tmpinsize, srcptr, sizetocopy);
                srcptr += sizetocopy;
                dctxptr->tmpinsize += sizetocopy;
                if (dctxptr->tmpinsize < 4) /* not enough input to get full cblocksize; wait for more */
                {
                    nextsrcsizehint = 4 - dctxptr->tmpinsize;
                    doanotherstage=0;
                    break;
                }
                selectedin = dctxptr->tmpin;
                dctxptr->dstage = dstage_decodecblocksize;
                break;
            }

        case dstage_decodecblocksize:
            {
                size_t nextcblocksize = lz4f_readle32(selectedin) & 0x7fffffffu;
                if (nextcblocksize==0)   /* frameend signal, no more cblock */
                {
                    dctxptr->dstage = dstage_getsuffix;
                    break;
                }
                if (nextcblocksize > dctxptr->maxblocksize) return (size_t)-error_generic;   /* invalid cblocksize */
                dctxptr->tmpintarget = nextcblocksize;
                if (lz4f_readle32(selectedin) & lz4f_blockuncompressed_flag)
                {
                    dctxptr->dstage = dstage_copydirect;
                    break;
                }
                dctxptr->dstage = dstage_getcblock;
                if (dstptr==dstend)
                {
                    nextsrcsizehint = nextcblocksize + 4;
                    doanotherstage = 0;
                }
                break;
            }

        case dstage_copydirect:   /* uncompressed block */
            {
                size_t sizetocopy = dctxptr->tmpintarget;
                if ((size_t)(srcend-srcptr) < sizetocopy) sizetocopy = srcend - srcptr;  /* not enough input to read full block */
                if ((size_t)(dstend-dstptr) < sizetocopy) sizetocopy = dstend - dstptr;
                memcpy(dstptr, srcptr, sizetocopy);
                if (dctxptr->frameinfo.contentchecksumflag) xxh32_update(&(dctxptr->xxh), srcptr, (u32)sizetocopy);

                /* dictionary management */
                if (dctxptr->frameinfo.blockmode==blocklinked)
                    lz4f_updatedict(dctxptr, dstptr, sizetocopy, dststart, 0);

                srcptr += sizetocopy;
                dstptr += sizetocopy;
                if (sizetocopy == dctxptr->tmpintarget)   /* all copied */
                {
                    dctxptr->dstage = dstage_getcblocksize;
                    break;
                }
                dctxptr->tmpintarget -= sizetocopy;   /* still need to copy more */
                nextsrcsizehint = dctxptr->tmpintarget + 4;
                doanotherstage = 0;
                break;
            }

        case dstage_getcblock:   /* entry from dstage_decodecblocksize */
            {
                if ((size_t)(srcend-srcptr) < dctxptr->tmpintarget)
                {
                    dctxptr->tmpinsize = 0;
                    dctxptr->dstage = dstage_storecblock;
                    break;
                }
                selectedin = srcptr;
                srcptr += dctxptr->tmpintarget;
                dctxptr->dstage = dstage_decodecblock;
                break;
            }

        case dstage_storecblock:
            {
                size_t sizetocopy = dctxptr->tmpintarget - dctxptr->tmpinsize;
                if (sizetocopy > (size_t)(srcend-srcptr)) sizetocopy = srcend-srcptr;
                memcpy(dctxptr->tmpin + dctxptr->tmpinsize, srcptr, sizetocopy);
                dctxptr->tmpinsize += sizetocopy;
                srcptr += sizetocopy;
                if (dctxptr->tmpinsize < dctxptr->tmpintarget)  /* need more input */
                {
                    nextsrcsizehint = (dctxptr->tmpintarget - dctxptr->tmpinsize) + 4;
                    doanotherstage=0;
                    break;
                }
                selectedin = dctxptr->tmpin;
                dctxptr->dstage = dstage_decodecblock;
                break;
            }

        case dstage_decodecblock:
            {
                if ((size_t)(dstend-dstptr) < dctxptr->maxblocksize)   /* not enough place into dst : decode into tmpout */
                    dctxptr->dstage = dstage_decodecblock_intotmp;
                else
                    dctxptr->dstage = dstage_decodecblock_intodst;
                break;
            }

        case dstage_decodecblock_intodst:
            {
                int (*decoder)(const char*, char*, int, int, const char*, int);
                int decodedsize;

                if (dctxptr->frameinfo.blockmode == blocklinked)
                    decoder = lz4_decompress_safe_usingdict;
                else
                    decoder = lz4f_decompress_safe;

                decodedsize = decoder((const char*)selectedin, (char*)dstptr, (int)dctxptr->tmpintarget, (int)dctxptr->maxblocksize, (const char*)dctxptr->dict, (int)dctxptr->dictsize);
                if (decodedsize < 0) return (size_t)-error_generic;   /* decompression failed */
                if (dctxptr->frameinfo.contentchecksumflag) xxh32_update(&(dctxptr->xxh), dstptr, decodedsize);

                /* dictionary management */
                if (dctxptr->frameinfo.blockmode==blocklinked)
                    lz4f_updatedict(dctxptr, dstptr, decodedsize, dststart, 0);

                dstptr += decodedsize;
                dctxptr->dstage = dstage_getcblocksize;
                break;
            }

        case dstage_decodecblock_intotmp:
            {
                /* not enough place into dst : decode into tmpout */
                int (*decoder)(const char*, char*, int, int, const char*, int);
                int decodedsize;

                if (dctxptr->frameinfo.blockmode == blocklinked)
                    decoder = lz4_decompress_safe_usingdict;
                else
                    decoder = lz4f_decompress_safe;

                /* ensure enough place for tmpout */
                if (dctxptr->frameinfo.blockmode == blocklinked)
                {
                    if (dctxptr->dict == dctxptr->tmpoutbuffer)
                    {
                        if (dctxptr->dictsize > 128 kb)
                        {
                            memcpy(dctxptr->dict, dctxptr->dict + dctxptr->dictsize - 64 kb, 64 kb);
                            dctxptr->dictsize = 64 kb;
                        }
                        dctxptr->tmpout = dctxptr->dict + dctxptr->dictsize;
                    }
                    else   /* dict not within tmp */
                    {
                        size_t reserveddictspace = dctxptr->dictsize;
                        if (reserveddictspace > 64 kb) reserveddictspace = 64 kb;
                        dctxptr->tmpout = dctxptr->tmpoutbuffer + reserveddictspace;
                    }
                }

                /* decode */
                decodedsize = decoder((const char*)selectedin, (char*)dctxptr->tmpout, (int)dctxptr->tmpintarget, (int)dctxptr->maxblocksize, (const char*)dctxptr->dict, (int)dctxptr->dictsize);
                if (decodedsize < 0) return (size_t)-error_decompressionfailed;   /* decompression failed */
                if (dctxptr->frameinfo.contentchecksumflag) xxh32_update(&(dctxptr->xxh), dctxptr->tmpout, decodedsize);
                dctxptr->tmpoutsize = decodedsize;
                dctxptr->tmpoutstart = 0;
                dctxptr->dstage = dstage_flushout;
                break;
            }

        case dstage_flushout:  /* flush decoded data from tmpout to dstbuffer */
            {
                size_t sizetocopy = dctxptr->tmpoutsize - dctxptr->tmpoutstart;
                if (sizetocopy > (size_t)(dstend-dstptr)) sizetocopy = dstend-dstptr;
                memcpy(dstptr, dctxptr->tmpout + dctxptr->tmpoutstart, sizetocopy);

                /* dictionary management */
                if (dctxptr->frameinfo.blockmode==blocklinked)
                    lz4f_updatedict(dctxptr, dstptr, sizetocopy, dststart, 1);

                dctxptr->tmpoutstart += sizetocopy;
                dstptr += sizetocopy;

                /* end of flush ? */
                if (dctxptr->tmpoutstart == dctxptr->tmpoutsize)
                {
                    dctxptr->dstage = dstage_getcblocksize;
                    break;
                }
                nextsrcsizehint = 4;
                doanotherstage = 0;   /* still some data to flush */
                break;
            }

        case dstage_getsuffix:
            {
                size_t suffixsize = dctxptr->frameinfo.contentchecksumflag * 4;
                if (suffixsize == 0)   /* frame completed */
                {
                    nextsrcsizehint = 0;
                    dctxptr->dstage = dstage_getheader;
                    doanotherstage = 0;
                    break;
                }
                if ((srcend - srcptr) >= 4)   /* crc present */
                {
                    selectedin = srcptr;
                    srcptr += 4;
                    dctxptr->dstage = dstage_checksuffix;
                    break;
                }
                dctxptr->tmpinsize = 0;
                dctxptr->dstage = dstage_storesuffix;
                break;
            }

        case dstage_storesuffix:
            {
                size_t sizetocopy = 4 - dctxptr->tmpinsize;
                if (sizetocopy > (size_t)(srcend - srcptr)) sizetocopy = srcend - srcptr;
                memcpy(dctxptr->tmpin + dctxptr->tmpinsize, srcptr, sizetocopy);
                srcptr += sizetocopy;
                dctxptr->tmpinsize += sizetocopy;
                if (dctxptr->tmpinsize < 4)  /* not enough input to read complete suffix */
                {
                    nextsrcsizehint = 4 - dctxptr->tmpinsize;
                    doanotherstage=0;
                    break;
                }
                selectedin = dctxptr->tmpin;
                dctxptr->dstage = dstage_checksuffix;
                break;
            }

        case dstage_checksuffix:
            {
                u32 readcrc = lz4f_readle32(selectedin);
                u32 resultcrc = xxh32_digest(&(dctxptr->xxh));
                if (readcrc != resultcrc) return (size_t)-error_checksum_invalid;
                nextsrcsizehint = 0;
                dctxptr->dstage = dstage_getheader;
                doanotherstage = 0;
                break;
            }
        }
    }

    /* preserve dictionary within tmp if necessary */
    if ( (dctxptr->frameinfo.blockmode==blocklinked)
        &&(dctxptr->dict != dctxptr->tmpoutbuffer)
        &&(!decompressoptionsptr->stabledst)
        &&((unsigned)(dctxptr->dstage-1) < (unsigned)(dstage_getsuffix-1))
        )
    {
        if (dctxptr->dstage == dstage_flushout)
        {
            size_t preservesize = dctxptr->tmpout - dctxptr->tmpoutbuffer;
            size_t copysize = 64 kb - dctxptr->tmpoutsize;
            byte* olddictend = dctxptr->dict + dctxptr->dictsize - dctxptr->tmpoutstart;
            if (dctxptr->tmpoutsize > 64 kb) copysize = 0;
            if (copysize > preservesize) copysize = preservesize;

            memcpy(dctxptr->tmpoutbuffer + preservesize - copysize, olddictend - copysize, copysize);

            dctxptr->dict = dctxptr->tmpoutbuffer;
            dctxptr->dictsize = preservesize + dctxptr->tmpoutstart;
        }
        else
        {
            size_t newdictsize = dctxptr->dictsize;
            byte* olddictend = dctxptr->dict + dctxptr->dictsize;
            if ((newdictsize) > 64 kb) newdictsize = 64 kb;

            memcpy(dctxptr->tmpoutbuffer, olddictend - newdictsize, newdictsize);

            dctxptr->dict = dctxptr->tmpoutbuffer;
            dctxptr->dictsize = newdictsize;
            dctxptr->tmpout = dctxptr->tmpoutbuffer + newdictsize;
        }
    }

    if (srcptr<srcend)   /* function must be called again with following source data */
        dctxptr->srcexpect = srcptr;
    else
        dctxptr->srcexpect = null;
    *srcsizeptr = (srcptr - srcstart);
    *dstsizeptr = (dstptr - dststart);
    return nextsrcsizehint;
}
