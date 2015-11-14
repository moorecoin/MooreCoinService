/*
lz4 hc - high compression mode of lz4
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
- lz4 homepage : http://fastcompression.blogspot.com/p/lz4.html
- lz4 source repository : http://code.google.com/p/lz4/
*/



/**************************************
   tuning parameter
**************************************/
static const int lz4hc_compressionlevel_default = 8;


/**************************************
   includes
**************************************/
#include "lz4hc.h"


/**************************************
   local compiler options
**************************************/
#if defined(__gnuc__)
#  pragma gcc diagnostic ignored "-wunused-function"
#endif

#if defined (__clang__)
#  pragma clang diagnostic ignored "-wunused-function"
#endif


/**************************************
   common lz4 definition
**************************************/
#define lz4_commondefs_only
#include "lz4.c"


/**************************************
  local constants
**************************************/
#define dictionary_logsize 16
#define maxd (1<<dictionary_logsize)
#define maxd_mask ((u32)(maxd - 1))

#define hash_log (dictionary_logsize-1)
#define hashtablesize (1 << hash_log)
#define hash_mask (hashtablesize - 1)

#define optimal_ml (int)((ml_mask-1)+minmatch)

static const int g_maxcompressionlevel = 16;


/**************************************
   local types
**************************************/
typedef struct
{
    u32 hashtable[hashtablesize];
    u16   chaintable[maxd];
    const byte* end;        /* next block here to continue on current prefix */
    const byte* base;       /* all index relative to this position */
    const byte* dictbase;   /* alternate base for extdict */
    const byte* inputbuffer;/* deprecated */
    u32   dictlimit;        /* below that point, need extdict */
    u32   lowlimit;         /* below that point, no more dict */
    u32   nexttoupdate;
    u32   compressionlevel;
} lz4hc_data_structure;


/**************************************
   local macros
**************************************/
#define hash_function(i)       (((i) * 2654435761u) >> ((minmatch*8)-hash_log))
#define deltanext(p)           chaintable[(size_t)(p) & maxd_mask]
#define getnext(p)             ((p) - (size_t)deltanext(p))

static u32 lz4hc_hashptr(const void* ptr) { return hash_function(lz4_read32(ptr)); }



/**************************************
   hc compression
**************************************/
static void lz4hc_init (lz4hc_data_structure* hc4, const byte* start)
{
    mem_init((void*)hc4->hashtable, 0, sizeof(hc4->hashtable));
    mem_init(hc4->chaintable, 0xff, sizeof(hc4->chaintable));
    hc4->nexttoupdate = 64 kb;
    hc4->base = start - 64 kb;
    hc4->inputbuffer = start;
    hc4->end = start;
    hc4->dictbase = start - 64 kb;
    hc4->dictlimit = 64 kb;
    hc4->lowlimit = 64 kb;
}


/* update chains up to ip (excluded) */
force_inline void lz4hc_insert (lz4hc_data_structure* hc4, const byte* ip)
{
    u16* chaintable = hc4->chaintable;
    u32* hashtable  = hc4->hashtable;
    const byte* const base = hc4->base;
    const u32 target = (u32)(ip - base);
    u32 idx = hc4->nexttoupdate;

    while(idx < target)
    {
        u32 h = lz4hc_hashptr(base+idx);
        size_t delta = idx - hashtable[h];
        if (delta>max_distance) delta = max_distance;
        chaintable[idx & 0xffff] = (u16)delta;
        hashtable[h] = idx;
        idx++;
    }

    hc4->nexttoupdate = target;
}


force_inline int lz4hc_insertandfindbestmatch (lz4hc_data_structure* hc4,   /* index table will be updated */
                                               const byte* ip, const byte* const ilimit,
                                               const byte** matchpos,
                                               const int maxnbattempts)
{
    u16* const chaintable = hc4->chaintable;
    u32* const hashtable = hc4->hashtable;
    const byte* const base = hc4->base;
    const byte* const dictbase = hc4->dictbase;
    const u32 dictlimit = hc4->dictlimit;
    const u32 lowlimit = (hc4->lowlimit + 64 kb > (u32)(ip-base)) ? hc4->lowlimit : (u32)(ip - base) - (64 kb - 1);
    u32 matchindex;
    const byte* match;
    int nbattempts=maxnbattempts;
    size_t ml=0;

    /* hc4 match finder */
    lz4hc_insert(hc4, ip);
    matchindex = hashtable[lz4hc_hashptr(ip)];

    while ((matchindex>=lowlimit) && (nbattempts))
    {
        nbattempts--;
        if (matchindex >= dictlimit)
        {
            match = base + matchindex;
            if (*(match+ml) == *(ip+ml)
                && (lz4_read32(match) == lz4_read32(ip)))
            {
                size_t mlt = lz4_count(ip+minmatch, match+minmatch, ilimit) + minmatch;
                if (mlt > ml) { ml = mlt; *matchpos = match; }
            }
        }
        else
        {
            match = dictbase + matchindex;
            if (lz4_read32(match) == lz4_read32(ip))
            {
                size_t mlt;
                const byte* vlimit = ip + (dictlimit - matchindex);
                if (vlimit > ilimit) vlimit = ilimit;
                mlt = lz4_count(ip+minmatch, match+minmatch, vlimit) + minmatch;
                if ((ip+mlt == vlimit) && (vlimit < ilimit))
                    mlt += lz4_count(ip+mlt, base+dictlimit, ilimit);
                if (mlt > ml) { ml = mlt; *matchpos = base + matchindex; }   /* virtual matchpos */
            }
        }
        matchindex -= chaintable[matchindex & 0xffff];
    }

    return (int)ml;
}


force_inline int lz4hc_insertandgetwidermatch (
    lz4hc_data_structure* hc4,
    const byte* ip,
    const byte* ilowlimit,
    const byte* ihighlimit,
    int longest,
    const byte** matchpos,
    const byte** startpos,
    const int maxnbattempts)
{
    u16* const chaintable = hc4->chaintable;
    u32* const hashtable = hc4->hashtable;
    const byte* const base = hc4->base;
    const u32 dictlimit = hc4->dictlimit;
    const u32 lowlimit = (hc4->lowlimit + 64 kb > (u32)(ip-base)) ? hc4->lowlimit : (u32)(ip - base) - (64 kb - 1);
    const byte* const dictbase = hc4->dictbase;
    const byte* match;
    u32   matchindex;
    int nbattempts = maxnbattempts;
    int delta = (int)(ip-ilowlimit);


    /* first match */
    lz4hc_insert(hc4, ip);
    matchindex = hashtable[lz4hc_hashptr(ip)];

    while ((matchindex>=lowlimit) && (nbattempts))
    {
        nbattempts--;
        if (matchindex >= dictlimit)
        {
            match = base + matchindex;
            if (*(ilowlimit + longest) == *(match - delta + longest))
                if (lz4_read32(match) == lz4_read32(ip))
                {
                    const byte* startt = ip;
                    const byte* tmpmatch = match;
                    const byte* const matchend = ip + minmatch + lz4_count(ip+minmatch, match+minmatch, ihighlimit);

                    while ((startt>ilowlimit) && (tmpmatch > ilowlimit) && (startt[-1] == tmpmatch[-1])) {startt--; tmpmatch--;}

                    if ((matchend-startt) > longest)
                    {
                        longest = (int)(matchend-startt);
                        *matchpos = tmpmatch;
                        *startpos = startt;
                    }
                }
        }
        else
        {
            match = dictbase + matchindex;
            if (lz4_read32(match) == lz4_read32(ip))
            {
                size_t mlt;
                int back=0;
                const byte* vlimit = ip + (dictlimit - matchindex);
                if (vlimit > ihighlimit) vlimit = ihighlimit;
                mlt = lz4_count(ip+minmatch, match+minmatch, vlimit) + minmatch;
                if ((ip+mlt == vlimit) && (vlimit < ihighlimit))
                    mlt += lz4_count(ip+mlt, base+dictlimit, ihighlimit);
                while ((ip+back > ilowlimit) && (matchindex+back > lowlimit) && (ip[back-1] == match[back-1])) back--;
                mlt -= back;
                if ((int)mlt > longest) { longest = (int)mlt; *matchpos = base + matchindex + back; *startpos = ip+back; }
            }
        }
        matchindex -= chaintable[matchindex & 0xffff];
    }

    return longest;
}


typedef enum { nolimit = 0, limitedoutput = 1 } limitedoutput_directive;

#define lz4hc_debug 0
#if lz4hc_debug
static unsigned debug = 0;
#endif

force_inline int lz4hc_encodesequence (
    const byte** ip,
    byte** op,
    const byte** anchor,
    int matchlength,
    const byte* const match,
    limitedoutput_directive limitedoutputbuffer,
    byte* oend)
{
    int length;
    byte* token;

#if lz4hc_debug
    if (debug) printf("literal : %u  --  match : %u  --  offset : %u\n", (u32)(*ip - *anchor), (u32)matchlength, (u32)(*ip-match));
#endif

    /* encode literal length */
    length = (int)(*ip - *anchor);
    token = (*op)++;
    if ((limitedoutputbuffer) && ((*op + (length>>8) + length + (2 + 1 + lastliterals)) > oend)) return 1;   /* check output limit */
    if (length>=(int)run_mask) { int len; *token=(run_mask<<ml_bits); len = length-run_mask; for(; len > 254 ; len-=255) *(*op)++ = 255;  *(*op)++ = (byte)len; }
    else *token = (byte)(length<<ml_bits);

    /* copy literals */
    lz4_wildcopy(*op, *anchor, (*op) + length);
    *op += length;

    /* encode offset */
    lz4_writele16(*op, (u16)(*ip-match)); *op += 2;

    /* encode matchlength */
    length = (int)(matchlength-minmatch);
    if ((limitedoutputbuffer) && (*op + (length>>8) + (1 + lastliterals) > oend)) return 1;   /* check output limit */
    if (length>=(int)ml_mask) { *token+=ml_mask; length-=ml_mask; for(; length > 509 ; length-=510) { *(*op)++ = 255; *(*op)++ = 255; } if (length > 254) { length-=255; *(*op)++ = 255; } *(*op)++ = (byte)length; }
    else *token += (byte)(length);

    /* prepare next loop */
    *ip += matchlength;
    *anchor = *ip;

    return 0;
}


static int lz4hc_compress_generic (
    void* ctxvoid,
    const char* source,
    char* dest,
    int inputsize,
    int maxoutputsize,
    int compressionlevel,
    limitedoutput_directive limit
    )
{
    lz4hc_data_structure* ctx = (lz4hc_data_structure*) ctxvoid;
    const byte* ip = (const byte*) source;
    const byte* anchor = ip;
    const byte* const iend = ip + inputsize;
    const byte* const mflimit = iend - mflimit;
    const byte* const matchlimit = (iend - lastliterals);

    byte* op = (byte*) dest;
    byte* const oend = op + maxoutputsize;

    unsigned maxnbattempts;
    int   ml, ml2, ml3, ml0;
    const byte* ref=null;
    const byte* start2=null;
    const byte* ref2=null;
    const byte* start3=null;
    const byte* ref3=null;
    const byte* start0;
    const byte* ref0;


    /* init */
    if (compressionlevel > g_maxcompressionlevel) compressionlevel = g_maxcompressionlevel;
    if (compressionlevel < 1) compressionlevel = lz4hc_compressionlevel_default;
    maxnbattempts = 1 << (compressionlevel-1);
    ctx->end += inputsize;

    ip++;

    /* main loop */
    while (ip < mflimit)
    {
        ml = lz4hc_insertandfindbestmatch (ctx, ip, matchlimit, (&ref), maxnbattempts);
        if (!ml) { ip++; continue; }

        /* saved, in case we would skip too much */
        start0 = ip;
        ref0 = ref;
        ml0 = ml;

_search2:
        if (ip+ml < mflimit)
            ml2 = lz4hc_insertandgetwidermatch(ctx, ip + ml - 2, ip + 1, matchlimit, ml, &ref2, &start2, maxnbattempts);
        else ml2 = ml;

        if (ml2 == ml)  /* no better match */
        {
            if (lz4hc_encodesequence(&ip, &op, &anchor, ml, ref, limit, oend)) return 0;
            continue;
        }

        if (start0 < ip)
        {
            if (start2 < ip + ml0)   /* empirical */
            {
                ip = start0;
                ref = ref0;
                ml = ml0;
            }
        }

        /* here, start0==ip */
        if ((start2 - ip) < 3)   /* first match too small : removed */
        {
            ml = ml2;
            ip = start2;
            ref =ref2;
            goto _search2;
        }

_search3:
        /*
        * currently we have :
        * ml2 > ml1, and
        * ip1+3 <= ip2 (usually < ip1+ml1)
        */
        if ((start2 - ip) < optimal_ml)
        {
            int correction;
            int new_ml = ml;
            if (new_ml > optimal_ml) new_ml = optimal_ml;
            if (ip+new_ml > start2 + ml2 - minmatch) new_ml = (int)(start2 - ip) + ml2 - minmatch;
            correction = new_ml - (int)(start2 - ip);
            if (correction > 0)
            {
                start2 += correction;
                ref2 += correction;
                ml2 -= correction;
            }
        }
        /* now, we have start2 = ip+new_ml, with new_ml = min(ml, optimal_ml=18) */

        if (start2 + ml2 < mflimit)
            ml3 = lz4hc_insertandgetwidermatch(ctx, start2 + ml2 - 3, start2, matchlimit, ml2, &ref3, &start3, maxnbattempts);
        else ml3 = ml2;

        if (ml3 == ml2) /* no better match : 2 sequences to encode */
        {
            /* ip & ref are known; now for ml */
            if (start2 < ip+ml)  ml = (int)(start2 - ip);
            /* now, encode 2 sequences */
            if (lz4hc_encodesequence(&ip, &op, &anchor, ml, ref, limit, oend)) return 0;
            ip = start2;
            if (lz4hc_encodesequence(&ip, &op, &anchor, ml2, ref2, limit, oend)) return 0;
            continue;
        }

        if (start3 < ip+ml+3) /* not enough space for match 2 : remove it */
        {
            if (start3 >= (ip+ml)) /* can write seq1 immediately ==> seq2 is removed, so seq3 becomes seq1 */
            {
                if (start2 < ip+ml)
                {
                    int correction = (int)(ip+ml - start2);
                    start2 += correction;
                    ref2 += correction;
                    ml2 -= correction;
                    if (ml2 < minmatch)
                    {
                        start2 = start3;
                        ref2 = ref3;
                        ml2 = ml3;
                    }
                }

                if (lz4hc_encodesequence(&ip, &op, &anchor, ml, ref, limit, oend)) return 0;
                ip  = start3;
                ref = ref3;
                ml  = ml3;

                start0 = start2;
                ref0 = ref2;
                ml0 = ml2;
                goto _search2;
            }

            start2 = start3;
            ref2 = ref3;
            ml2 = ml3;
            goto _search3;
        }

        /*
        * ok, now we have 3 ascending matches; let's write at least the first one
        * ip & ref are known; now for ml
        */
        if (start2 < ip+ml)
        {
            if ((start2 - ip) < (int)ml_mask)
            {
                int correction;
                if (ml > optimal_ml) ml = optimal_ml;
                if (ip + ml > start2 + ml2 - minmatch) ml = (int)(start2 - ip) + ml2 - minmatch;
                correction = ml - (int)(start2 - ip);
                if (correction > 0)
                {
                    start2 += correction;
                    ref2 += correction;
                    ml2 -= correction;
                }
            }
            else
            {
                ml = (int)(start2 - ip);
            }
        }
        if (lz4hc_encodesequence(&ip, &op, &anchor, ml, ref, limit, oend)) return 0;

        ip = start2;
        ref = ref2;
        ml = ml2;

        start2 = start3;
        ref2 = ref3;
        ml2 = ml3;

        goto _search3;
    }

    /* encode last literals */
    {
        int lastrun = (int)(iend - anchor);
        if ((limit) && (((char*)op - dest) + lastrun + 1 + ((lastrun+255-run_mask)/255) > (u32)maxoutputsize)) return 0;  /* check output limit */
        if (lastrun>=(int)run_mask) { *op++=(run_mask<<ml_bits); lastrun-=run_mask; for(; lastrun > 254 ; lastrun-=255) *op++ = 255; *op++ = (byte) lastrun; }
        else *op++ = (byte)(lastrun<<ml_bits);
        memcpy(op, anchor, iend - anchor);
        op += iend-anchor;
    }

    /* end */
    return (int) (((char*)op)-dest);
}


int lz4_compresshc2(const char* source, char* dest, int inputsize, int compressionlevel)
{
    lz4hc_data_structure ctx;
    lz4hc_init(&ctx, (const byte*)source);
    return lz4hc_compress_generic (&ctx, source, dest, inputsize, 0, compressionlevel, nolimit);
}

int lz4_compresshc(const char* source, char* dest, int inputsize) { return lz4_compresshc2(source, dest, inputsize, 0); }

int lz4_compresshc2_limitedoutput(const char* source, char* dest, int inputsize, int maxoutputsize, int compressionlevel)
{
    lz4hc_data_structure ctx;
    lz4hc_init(&ctx, (const byte*)source);
    return lz4hc_compress_generic (&ctx, source, dest, inputsize, maxoutputsize, compressionlevel, limitedoutput);
}

int lz4_compresshc_limitedoutput(const char* source, char* dest, int inputsize, int maxoutputsize)
{
    return lz4_compresshc2_limitedoutput(source, dest, inputsize, maxoutputsize, 0);
}


/*****************************
 * using external allocation
 * ***************************/
int lz4_sizeofstatehc(void) { return sizeof(lz4hc_data_structure); }


int lz4_compresshc2_withstatehc (void* state, const char* source, char* dest, int inputsize, int compressionlevel)
{
    if (((size_t)(state)&(sizeof(void*)-1)) != 0) return 0;   /* error : state is not aligned for pointers (32 or 64 bits) */
    lz4hc_init ((lz4hc_data_structure*)state, (const byte*)source);
    return lz4hc_compress_generic (state, source, dest, inputsize, 0, compressionlevel, nolimit);
}

int lz4_compresshc_withstatehc (void* state, const char* source, char* dest, int inputsize)
{ return lz4_compresshc2_withstatehc (state, source, dest, inputsize, 0); }


int lz4_compresshc2_limitedoutput_withstatehc (void* state, const char* source, char* dest, int inputsize, int maxoutputsize, int compressionlevel)
{
    if (((size_t)(state)&(sizeof(void*)-1)) != 0) return 0;   /* error : state is not aligned for pointers (32 or 64 bits) */
    lz4hc_init ((lz4hc_data_structure*)state, (const byte*)source);
    return lz4hc_compress_generic (state, source, dest, inputsize, maxoutputsize, compressionlevel, limitedoutput);
}

int lz4_compresshc_limitedoutput_withstatehc (void* state, const char* source, char* dest, int inputsize, int maxoutputsize)
{ return lz4_compresshc2_limitedoutput_withstatehc (state, source, dest, inputsize, maxoutputsize, 0); }



/**************************************
 * streaming functions
 * ************************************/
/* allocation */
lz4_streamhc_t* lz4_createstreamhc(void) { return (lz4_streamhc_t*)malloc(sizeof(lz4_streamhc_t)); }
int lz4_freestreamhc (lz4_streamhc_t* lz4_streamhcptr) { free(lz4_streamhcptr); return 0; }


/* initialization */
void lz4_resetstreamhc (lz4_streamhc_t* lz4_streamhcptr, int compressionlevel)
{
    lz4_static_assert(sizeof(lz4hc_data_structure) <= lz4_streamhcsize);   /* if compilation fails here, lz4_streamhcsize must be increased */
    ((lz4hc_data_structure*)lz4_streamhcptr)->base = null;
    ((lz4hc_data_structure*)lz4_streamhcptr)->compressionlevel = (unsigned)compressionlevel;
}

int lz4_loaddicthc (lz4_streamhc_t* lz4_streamhcptr, const char* dictionary, int dictsize)
{
    lz4hc_data_structure* ctxptr = (lz4hc_data_structure*) lz4_streamhcptr;
    if (dictsize > 64 kb)
    {
        dictionary += dictsize - 64 kb;
        dictsize = 64 kb;
    }
    lz4hc_init (ctxptr, (const byte*)dictionary);
    if (dictsize >= 4) lz4hc_insert (ctxptr, (const byte*)dictionary +(dictsize-3));
    ctxptr->end = (const byte*)dictionary + dictsize;
    return dictsize;
}


/* compression */

static void lz4hc_setexternaldict(lz4hc_data_structure* ctxptr, const byte* newblock)
{
    if (ctxptr->end >= ctxptr->base + 4)
        lz4hc_insert (ctxptr, ctxptr->end-3);   /* referencing remaining dictionary content */
    /* only one memory segment for extdict, so any previous extdict is lost at this stage */
    ctxptr->lowlimit  = ctxptr->dictlimit;
    ctxptr->dictlimit = (u32)(ctxptr->end - ctxptr->base);
    ctxptr->dictbase  = ctxptr->base;
    ctxptr->base = newblock - ctxptr->dictlimit;
    ctxptr->end  = newblock;
    ctxptr->nexttoupdate = ctxptr->dictlimit;   /* match referencing will resume from there */
}

static int lz4_compresshc_continue_generic (lz4hc_data_structure* ctxptr,
                                            const char* source, char* dest,
                                            int inputsize, int maxoutputsize, limitedoutput_directive limit)
{
    /* auto-init if forgotten */
    if (ctxptr->base == null)
        lz4hc_init (ctxptr, (const byte*) source);

    /* check overflow */
    if ((size_t)(ctxptr->end - ctxptr->base) > 2 gb)
    {
        size_t dictsize = (size_t)(ctxptr->end - ctxptr->base) - ctxptr->dictlimit;
        if (dictsize > 64 kb) dictsize = 64 kb;

        lz4_loaddicthc((lz4_streamhc_t*)ctxptr, (const char*)(ctxptr->end) - dictsize, (int)dictsize);
    }

    /* check if blocks follow each other */
    if ((const byte*)source != ctxptr->end) lz4hc_setexternaldict(ctxptr, (const byte*)source);

    /* check overlapping input/dictionary space */
    {
        const byte* sourceend = (const byte*) source + inputsize;
        const byte* dictbegin = ctxptr->dictbase + ctxptr->lowlimit;
        const byte* dictend   = ctxptr->dictbase + ctxptr->dictlimit;
        if ((sourceend > dictbegin) && ((byte*)source < dictend))
        {
            if (sourceend > dictend) sourceend = dictend;
            ctxptr->lowlimit = (u32)(sourceend - ctxptr->dictbase);
            if (ctxptr->dictlimit - ctxptr->lowlimit < 4) ctxptr->lowlimit = ctxptr->dictlimit;
        }
    }

    return lz4hc_compress_generic (ctxptr, source, dest, inputsize, maxoutputsize, ctxptr->compressionlevel, limit);
}

int lz4_compresshc_continue (lz4_streamhc_t* lz4_streamhcptr, const char* source, char* dest, int inputsize)
{
    return lz4_compresshc_continue_generic ((lz4hc_data_structure*)lz4_streamhcptr, source, dest, inputsize, 0, nolimit);
}

int lz4_compresshc_limitedoutput_continue (lz4_streamhc_t* lz4_streamhcptr, const char* source, char* dest, int inputsize, int maxoutputsize)
{
    return lz4_compresshc_continue_generic ((lz4hc_data_structure*)lz4_streamhcptr, source, dest, inputsize, maxoutputsize, limitedoutput);
}


/* dictionary saving */

int lz4_savedicthc (lz4_streamhc_t* lz4_streamhcptr, char* safebuffer, int dictsize)
{
    lz4hc_data_structure* streamptr = (lz4hc_data_structure*)lz4_streamhcptr;
    int prefixsize = (int)(streamptr->end - (streamptr->base + streamptr->dictlimit));
    if (dictsize > 64 kb) dictsize = 64 kb;
    if (dictsize < 4) dictsize = 0;
    if (dictsize > prefixsize) dictsize = prefixsize;
    memcpy(safebuffer, streamptr->end - dictsize, dictsize);
    {
        u32 endindex = (u32)(streamptr->end - streamptr->base);
        streamptr->end = (const byte*)safebuffer + dictsize;
        streamptr->base = streamptr->end - endindex;
        streamptr->dictlimit = endindex - dictsize;
        streamptr->lowlimit = endindex - dictsize;
        if (streamptr->nexttoupdate < streamptr->dictlimit) streamptr->nexttoupdate = streamptr->dictlimit;
    }
    return dictsize;
}


/***********************************
 * deprecated functions
 ***********************************/
int lz4_sizeofstreamstatehc(void) { return lz4_streamhcsize; }

int lz4_resetstreamstatehc(void* state, const char* inputbuffer)
{
    if ((((size_t)state) & (sizeof(void*)-1)) != 0) return 1;   /* error : pointer is not aligned for pointer (32 or 64 bits) */
    lz4hc_init((lz4hc_data_structure*)state, (const byte*)inputbuffer);
    return 0;
}

void* lz4_createhc (const char* inputbuffer)
{
    void* hc4 = allocator(1, sizeof(lz4hc_data_structure));
    lz4hc_init ((lz4hc_data_structure*)hc4, (const byte*)inputbuffer);
    return hc4;
}

int lz4_freehc (void* lz4hc_data)
{
    freemem(lz4hc_data);
    return (0);
}

/*
int lz4_compresshc_continue (void* lz4hc_data, const char* source, char* dest, int inputsize)
{
return lz4hc_compress_generic (lz4hc_data, source, dest, inputsize, 0, 0, nolimit);
}
int lz4_compresshc_limitedoutput_continue (void* lz4hc_data, const char* source, char* dest, int inputsize, int maxoutputsize)
{
return lz4hc_compress_generic (lz4hc_data, source, dest, inputsize, maxoutputsize, 0, limitedoutput);
}
*/

int lz4_compresshc2_continue (void* lz4hc_data, const char* source, char* dest, int inputsize, int compressionlevel)
{
    return lz4hc_compress_generic (lz4hc_data, source, dest, inputsize, 0, compressionlevel, nolimit);
}

int lz4_compresshc2_limitedoutput_continue (void* lz4hc_data, const char* source, char* dest, int inputsize, int maxoutputsize, int compressionlevel)
{
    return lz4hc_compress_generic (lz4hc_data, source, dest, inputsize, maxoutputsize, compressionlevel, limitedoutput);
}

char* lz4_slideinputbufferhc(void* lz4hc_data)
{
    lz4hc_data_structure* hc4 = (lz4hc_data_structure*)lz4hc_data;
    int dictsize = lz4_savedicthc((lz4_streamhc_t*)lz4hc_data, (char*)(hc4->inputbuffer), 64 kb);
    return (char*)(hc4->inputbuffer + dictsize);
}
