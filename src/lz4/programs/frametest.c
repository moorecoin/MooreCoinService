/*
    frametest - test tool for lz4frame
    copyright (c) yann collet 2014
    gpl v2 license

    this program is free software; you can redistribute it and/or modify
    it under the terms of the gnu general public license as published by
    the free software foundation; either version 2 of the license, or
    (at your option) any later version.

    this program is distributed in the hope that it will be useful,
    but without any warranty; without even the implied warranty of
    merchantability or fitness for a particular purpose.  see the
    gnu general public license for more details.

    you should have received a copy of the gnu general public license along
    with this program; if not, write to the free software foundation, inc.,
    51 franklin street, fifth floor, boston, ma 02110-1301 usa.

    you can contact the author at :
    - lz4 source repository : http://code.google.com/p/lz4/
    - lz4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/**************************************
  compiler specific
**************************************/
#define _crt_secure_no_warnings   // fgets
#ifdef _msc_ver    /* visual studio */
#  pragma warning(disable : 4127)        /* disable: c4127: conditional expression is constant */
#  pragma warning(disable : 4146)        /* disable: c4146: minus unsigned expression */
#endif
#define gcc_version (__gnuc__ * 100 + __gnuc_minor__)
#ifdef __gnuc__
#  pragma gcc diagnostic ignored "-wmissing-braces"   /* gcc bug 53119 : doesn't accept { 0 } as initializer (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119) */
#  pragma gcc diagnostic ignored "-wmissing-field-initializers"   /* gcc bug 53119 : doesn't accept { 0 } as initializer (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119) */
#endif


/**************************************
  includes
**************************************/
#include <stdlib.h>     // free
#include <stdio.h>      // fgets, sscanf
#include <sys/timeb.h>  // timeb
#include <string.h>     // strcmp
#include "lz4frame_static.h"
#include "xxhash.h"     // xxh64


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
 constants
**************************************/
#ifndef lz4_version
#  define lz4_version ""
#endif

#define kb *(1u<<10)
#define mb *(1u<<20)
#define gb *(1u<<30)

static const u32 nbtestsdefault = 256 kb;
#define compressible_noise_length (2 mb)
#define fuz_compressibility_default 50
static const u32 prime1 = 2654435761u;
static const u32 prime2 = 2246822519u;



/**************************************
  macros
**************************************/
#define display(...)          fprintf(stderr, __va_args__)
#define displaylevel(l, ...)  if (displaylevel>=l) { display(__va_args__); }
#define displayupdate(l, ...) if (displaylevel>=l) { \
            if ((fuz_getmillispan(g_time) > refreshrate) || (displaylevel>=4)) \
            { g_time = fuz_getmillistart(); display(__va_args__); \
            if (displaylevel>=4) fflush(stdout); } }
static const u32 refreshrate = 150;
static u32 g_time = 0;


/*****************************************
  local parameters
*****************************************/
static u32 no_prompt = 0;
static char* programname;
static u32 displaylevel = 2;
static u32 pause = 0;


/*********************************************************
  fuzzer functions
*********************************************************/
static u32 fuz_getmillistart(void)
{
    struct timeb tb;
    u32 ncount;
    ftime( &tb );
    ncount = (u32) (((tb.time & 0xfffff) * 1000) +  tb.millitm);
    return ncount;
}


static u32 fuz_getmillispan(u32 ntimestart)
{
    u32 ncurrent = fuz_getmillistart();
    u32 nspan = ncurrent - ntimestart;
    if (ntimestart > ncurrent)
        nspan += 0x100000 * 1000;
    return nspan;
}


#  define fuz_rotl32(x,r) ((x << r) | (x >> (32 - r)))
unsigned int fuz_rand(unsigned int* src)
{
    u32 rand32 = *src;
    rand32 *= prime1;
    rand32 += prime2;
    rand32  = fuz_rotl32(rand32, 13);
    *src = rand32;
    return rand32 >> 5;
}


#define fuz_rand15bits  (fuz_rand(seed) & 0x7fff)
#define fuz_randlength  ( (fuz_rand(seed) & 3) ? (fuz_rand(seed) % 15) : (fuz_rand(seed) % 510) + 15)
static void fuz_fillcompressiblenoisebuffer(void* buffer, unsigned buffersize, double proba, u32* seed)
{
    byte* bbuffer = (byte*)buffer;
    unsigned pos = 0;
    u32 p32 = (u32)(32768 * proba);

    // first byte
    bbuffer[pos++] = (byte)(fuz_rand(seed));

    while (pos < buffersize)
    {
        // select : literal (noise) or copy (within 64k)
        if (fuz_rand15bits < p32)
        {
            // copy (within 64k)
            unsigned match, end;
            unsigned length = fuz_randlength + 4;
            unsigned offset = fuz_rand15bits + 1;
            if (offset > pos) offset = pos;
            if (pos + length > buffersize) length = buffersize - pos;
            match = pos - offset;
            end = pos + length;
            while (pos < end) bbuffer[pos++] = bbuffer[match++];
        }
        else
        {
            // literal (noise)
            unsigned end;
            unsigned length = fuz_randlength;
            if (pos + length > buffersize) length = buffersize - pos;
            end = pos + length;
            while (pos < end) bbuffer[pos++] = (byte)(fuz_rand(seed) >> 5);
        }
    }
}


static unsigned fuz_highbit(u32 v32)
{
    unsigned nbbits = 0;
    if (v32==0) return 0;
    while (v32)
    {
        v32 >>= 1;
        nbbits ++;
    }
    return nbbits;
}


int basictests(u32 seed, double compressibility)
{
    int testresult = 0;
    void* cnbuffer;
    void* compressedbuffer;
    void* decodedbuffer;
    u32 randstate = seed;
    size_t csize, testsize;
    lz4f_preferences_t prefs = { 0 };
    lz4f_decompressioncontext_t dctx;
    u64 crcorig;

    // create compressible test buffer
    cnbuffer = malloc(compressible_noise_length);
    compressedbuffer = malloc(lz4f_compressframebound(compressible_noise_length, null));
    decodedbuffer = malloc(compressible_noise_length);
    fuz_fillcompressiblenoisebuffer(cnbuffer, compressible_noise_length, compressibility, &randstate);
    crcorig = xxh64(cnbuffer, compressible_noise_length, 1);

    // trivial tests : one-step frame
    testsize = compressible_noise_length;
    displaylevel(3, "using null preferences : \n");
    csize = lz4f_compressframe(compressedbuffer, lz4f_compressframebound(testsize, null), cnbuffer, testsize, null);
    if (lz4f_iserror(csize)) goto _output_error;
    displaylevel(3, "compressed %i bytes into a %i bytes frame \n", (int)testsize, (int)csize);

    displaylevel(3, "decompression test : \n");
    {
        size_t decodedbuffersize = compressible_noise_length;
        size_t compressedbuffersize = csize;
        byte* op = (byte*)decodedbuffer;
        byte* const oend = (byte*)decodedbuffer + compressible_noise_length;
        byte* ip = (byte*)compressedbuffer;
        byte* const iend = (byte*)compressedbuffer + csize;
        u64 crcdest;

        lz4f_errorcode_t errorcode = lz4f_createdecompressioncontext(&dctx, lz4f_version);
        if (lz4f_iserror(errorcode)) goto _output_error;

        displaylevel(3, "single block : \n");
        errorcode = lz4f_decompress(dctx, decodedbuffer, &decodedbuffersize, compressedbuffer, &compressedbuffersize, null);
        crcdest = xxh64(decodedbuffer, compressible_noise_length, 1);
        if (crcdest != crcorig) goto _output_error;
        displaylevel(3, "regenerated %i bytes \n", (int)decodedbuffersize);

        displaylevel(3, "byte after byte : \n");
        while (ip < iend)
        {
            size_t osize = oend-op;
            size_t isize = 1;
            //display("%7i \n", (int)(ip-(byte*)compressedbuffer));
            errorcode = lz4f_decompress(dctx, op, &osize, ip, &isize, null);
            if (lz4f_iserror(errorcode)) goto _output_error;
            op += osize;
            ip += isize;
        }
        crcdest = xxh64(decodedbuffer, compressible_noise_length, 1);
        if (crcdest != crcorig) goto _output_error;
        displaylevel(3, "regenerated %i bytes \n", (int)decodedbuffersize);

        errorcode = lz4f_freedecompressioncontext(dctx);
        if (lz4f_iserror(errorcode)) goto _output_error;
    }

    displaylevel(3, "using 64 kb block : \n");
    prefs.frameinfo.blocksizeid = max64kb;
    prefs.frameinfo.contentchecksumflag = contentchecksumenabled;
    csize = lz4f_compressframe(compressedbuffer, lz4f_compressframebound(testsize, &prefs), cnbuffer, testsize, &prefs);
    if (lz4f_iserror(csize)) goto _output_error;
    displaylevel(3, "compressed %i bytes into a %i bytes frame \n", (int)testsize, (int)csize);

    displaylevel(3, "without checksum : \n");
    prefs.frameinfo.contentchecksumflag = nocontentchecksum;
    csize = lz4f_compressframe(compressedbuffer, lz4f_compressframebound(testsize, &prefs), cnbuffer, testsize, &prefs);
    if (lz4f_iserror(csize)) goto _output_error;
    displaylevel(3, "compressed %i bytes into a %i bytes frame \n", (int)testsize, (int)csize);

    displaylevel(3, "using 256 kb block : \n");
    prefs.frameinfo.blocksizeid = max256kb;
    prefs.frameinfo.contentchecksumflag = contentchecksumenabled;
    csize = lz4f_compressframe(compressedbuffer, lz4f_compressframebound(testsize, &prefs), cnbuffer, testsize, &prefs);
    if (lz4f_iserror(csize)) goto _output_error;
    displaylevel(3, "compressed %i bytes into a %i bytes frame \n", (int)testsize, (int)csize);

    displaylevel(3, "decompression test : \n");
    {
        size_t decodedbuffersize = compressible_noise_length;
        unsigned maxbits = fuz_highbit((u32)decodedbuffersize);
        byte* op = (byte*)decodedbuffer;
        byte* const oend = (byte*)decodedbuffer + compressible_noise_length;
        byte* ip = (byte*)compressedbuffer;
        byte* const iend = (byte*)compressedbuffer + csize;
        u64 crcdest;

        lz4f_errorcode_t errorcode = lz4f_createdecompressioncontext(&dctx, lz4f_version);
        if (lz4f_iserror(errorcode)) goto _output_error;

        displaylevel(3, "random segment sizes : \n");
        while (ip < iend)
        {
            unsigned nbbits = fuz_rand(&randstate) % maxbits;
            size_t isize = (fuz_rand(&randstate) & ((1<<nbbits)-1)) + 1;
            size_t osize = oend-op;
            if (isize > (size_t)(iend-ip)) isize = iend-ip;
            //display("%7i : + %6i\n", (int)(ip-(byte*)compressedbuffer), (int)isize);
            errorcode = lz4f_decompress(dctx, op, &osize, ip, &isize, null);
            if (lz4f_iserror(errorcode)) goto _output_error;
            op += osize;
            ip += isize;
        }
        crcdest = xxh64(decodedbuffer, compressible_noise_length, 1);
        if (crcdest != crcorig) goto _output_error;
        displaylevel(3, "regenerated %i bytes \n", (int)decodedbuffersize);

        errorcode = lz4f_freedecompressioncontext(dctx);
        if (lz4f_iserror(errorcode)) goto _output_error;
    }

    displaylevel(3, "without checksum : \n");
    prefs.frameinfo.contentchecksumflag = nocontentchecksum;
    csize = lz4f_compressframe(compressedbuffer, lz4f_compressframebound(testsize, &prefs), cnbuffer, testsize, &prefs);
    if (lz4f_iserror(csize)) goto _output_error;
    displaylevel(3, "compressed %i bytes into a %i bytes frame \n", (int)testsize, (int)csize);

    displaylevel(3, "using 1 mb block : \n");
    prefs.frameinfo.blocksizeid = max1mb;
    prefs.frameinfo.contentchecksumflag = contentchecksumenabled;
    csize = lz4f_compressframe(compressedbuffer, lz4f_compressframebound(testsize, &prefs), cnbuffer, testsize, &prefs);
    if (lz4f_iserror(csize)) goto _output_error;
    displaylevel(3, "compressed %i bytes into a %i bytes frame \n", (int)testsize, (int)csize);

    displaylevel(3, "without checksum : \n");
    prefs.frameinfo.contentchecksumflag = nocontentchecksum;
    csize = lz4f_compressframe(compressedbuffer, lz4f_compressframebound(testsize, &prefs), cnbuffer, testsize, &prefs);
    if (lz4f_iserror(csize)) goto _output_error;
    displaylevel(3, "compressed %i bytes into a %i bytes frame \n", (int)testsize, (int)csize);

    displaylevel(3, "using 4 mb block : \n");
    prefs.frameinfo.blocksizeid = max4mb;
    prefs.frameinfo.contentchecksumflag = contentchecksumenabled;
    csize = lz4f_compressframe(compressedbuffer, lz4f_compressframebound(testsize, &prefs), cnbuffer, testsize, &prefs);
    if (lz4f_iserror(csize)) goto _output_error;
    displaylevel(3, "compressed %i bytes into a %i bytes frame \n", (int)testsize, (int)csize);

    displaylevel(3, "without checksum : \n");
    prefs.frameinfo.contentchecksumflag = nocontentchecksum;
    csize = lz4f_compressframe(compressedbuffer, lz4f_compressframebound(testsize, &prefs), cnbuffer, testsize, &prefs);
    if (lz4f_iserror(csize)) goto _output_error;
    displaylevel(3, "compressed %i bytes into a %i bytes frame \n", (int)testsize, (int)csize);

    display("basic tests completed \n");
_end:
    free(cnbuffer);
    free(compressedbuffer);
    free(decodedbuffer);
    return testresult;

_output_error:
    testresult = 1;
    display("error detected ! \n");
    goto _end;
}


static void locatebuffdiff(const void* buff1, const void* buff2, size_t size, unsigned noncontiguous)
{
    int p=0;
    byte* b1=(byte*)buff1;
    byte* b2=(byte*)buff2;
    if (noncontiguous)
    {
        display("non-contiguous output test (%i bytes)\n", (int)size);
        return;
    }
    while (b1[p]==b2[p]) p++;
    display("error at pos %i/%i : %02x != %02x \n", p, (int)size, b1[p], b2[p]);
}


static const u32 srcdatalength = 9 mb;  /* needs to be > 2x4mb to test large blocks */

int fuzzertests(u32 seed, unsigned nbtests, unsigned starttest, double compressibility)
{
    unsigned testresult = 0;
    unsigned testnb = 0;
    void* srcbuffer = null;
    void* compressedbuffer = null;
    void* decodedbuffer = null;
    u32 corerand = seed;
    lz4f_decompressioncontext_t dctx = null;
    lz4f_compressioncontext_t cctx = null;
    size_t result;
    xxh64_state_t xxh64;
#   define check(cond, ...) if (cond) { display("error => "); display(__va_args__); \
                            display(" (seed %u, test nb %u)  \n", seed, testnb); goto _output_error; }

    // create buffers
    result = lz4f_createdecompressioncontext(&dctx, lz4f_version);
    check(lz4f_iserror(result), "allocation failed (error %i)", (int)result);
    result = lz4f_createcompressioncontext(&cctx, lz4f_version);
    check(lz4f_iserror(result), "allocation failed (error %i)", (int)result);
    srcbuffer = malloc(srcdatalength);
    check(srcbuffer==null, "srcbuffer allocation failed");
    compressedbuffer = malloc(lz4f_compressframebound(srcdatalength, null));
    check(compressedbuffer==null, "compressedbuffer allocation failed");
    decodedbuffer = malloc(srcdatalength);
    check(decodedbuffer==null, "decodedbuffer allocation failed");
    fuz_fillcompressiblenoisebuffer(srcbuffer, srcdatalength, compressibility, &corerand);

    // jump to requested testnb
    for (testnb =0; testnb < starttest; testnb++) (void)fuz_rand(&corerand);   // sync randomizer

    // main fuzzer loop
    for ( ; testnb < nbtests; testnb++)
    {
        u32 randstate = corerand ^ prime1;
        unsigned bsid   = 4 + (fuz_rand(&randstate) & 3);
        unsigned bmid   = fuz_rand(&randstate) & 1;
        unsigned ccflag = fuz_rand(&randstate) & 1;
        unsigned autoflush = (fuz_rand(&randstate) & 7) == 2;
        lz4f_preferences_t prefs = { 0 };
        lz4f_compressoptions_t coptions = { 0 };
        lz4f_decompressoptions_t doptions = { 0 };
        unsigned nbbits = (fuz_rand(&randstate) % (fuz_highbit(srcdatalength-1) - 1)) + 1;
        size_t srcsize = (fuz_rand(&randstate) & ((1<<nbbits)-1)) + 1;
        size_t srcstart = fuz_rand(&randstate) % (srcdatalength - srcsize);
        size_t csize;
        u64 crcorig, crcdecoded;
        lz4f_preferences_t* prefsptr = &prefs;

        (void)fuz_rand(&corerand);   // update rand seed
        prefs.frameinfo.blockmode = (blockmode_t)bmid;
        prefs.frameinfo.blocksizeid = (blocksizeid_t)bsid;
        prefs.frameinfo.contentchecksumflag = (contentchecksum_t)ccflag;
        prefs.autoflush = autoflush;
        prefs.compressionlevel = fuz_rand(&randstate) % 5;
        if ((fuz_rand(&randstate)&0xf) == 1) prefsptr = null;

        displayupdate(2, "\r%5u   ", testnb);
        crcorig = xxh64((byte*)srcbuffer+srcstart, (u32)srcsize, 1);

        if ((fuz_rand(&randstate)&0xf) == 2)
        {
            csize = lz4f_compressframe(compressedbuffer, lz4f_compressframebound(srcsize, prefsptr), (char*)srcbuffer + srcstart, srcsize, prefsptr);
            check(lz4f_iserror(csize), "lz4f_compressframe failed : error %i (%s)", (int)csize, lz4f_geterrorname(csize));
        }
        else
        {
            const byte* ip = (const byte*)srcbuffer + srcstart;
            const byte* const iend = ip + srcsize;
            byte* op = (byte*)compressedbuffer;
            byte* const oend = op + lz4f_compressframebound(srcdatalength, null);
            unsigned maxbits = fuz_highbit((u32)srcsize);
            result = lz4f_compressbegin(cctx, op, oend-op, prefsptr);
            check(lz4f_iserror(result), "compression header failed (error %i)", (int)result);
            op += result;
            while (ip < iend)
            {
                unsigned nbbitsseg = fuz_rand(&randstate) % maxbits;
                size_t isize = (fuz_rand(&randstate) & ((1<<nbbitsseg)-1)) + 1;
                size_t osize = lz4f_compressbound(isize, prefsptr);
                unsigned forceflush = ((fuz_rand(&randstate) & 3) == 1);
                if (isize > (size_t)(iend-ip)) isize = iend-ip;
                coptions.stablesrc = ((fuz_rand(&randstate) & 3) == 1);

                result = lz4f_compressupdate(cctx, op, osize, ip, isize, &coptions);
                check(lz4f_iserror(result), "compression failed (error %i)", (int)result);
                op += result;
                ip += isize;

                if (forceflush)
                {
                    result = lz4f_flush(cctx, op, oend-op, &coptions);
                    check(lz4f_iserror(result), "compression failed (error %i)", (int)result);
                    op += result;
                }
            }
            result = lz4f_compressend(cctx, op, oend-op, &coptions);
            check(lz4f_iserror(result), "compression completion failed (error %i)", (int)result);
            op += result;
            csize = op-(byte*)compressedbuffer;
        }

        {
            const byte* ip = (const byte*)compressedbuffer;
            const byte* const iend = ip + csize;
            byte* op = (byte*)decodedbuffer;
            byte* const oend = op + srcdatalength;
            unsigned maxbits = fuz_highbit((u32)csize);
            unsigned noncontiguousdst = (fuz_rand(&randstate) & 3) == 1;
            noncontiguousdst += fuz_rand(&randstate) & noncontiguousdst;   /* 0=>0; 1=>1,2 */
            xxh64_reset(&xxh64, 1);
            while (ip < iend)
            {
                unsigned nbbitsi = (fuz_rand(&randstate) % (maxbits-1)) + 1;
                unsigned nbbitso = (fuz_rand(&randstate) % (maxbits)) + 1;
                size_t isize = (fuz_rand(&randstate) & ((1<<nbbitsi)-1)) + 1;
                size_t osize = (fuz_rand(&randstate) & ((1<<nbbitso)-1)) + 2;
                if (isize > (size_t)(iend-ip)) isize = iend-ip;
                if (osize > (size_t)(oend-op)) osize = oend-op;
                doptions.stabledst = fuz_rand(&randstate) & 1;
                if (noncontiguousdst==2) doptions.stabledst = 0;
                //if (ip == compressedbuffer+62073)                    display("osize : %i : pos %i \n", (int)osize, (int)(op-(byte*)decodedbuffer));
                result = lz4f_decompress(dctx, op, &osize, ip, &isize, &doptions);
                //if (op+osize >= (byte*)decodedbuffer+94727)                    display("isize : %i : pos %i \n", (int)isize, (int)(ip-(byte*)compressedbuffer));
                //if ((int)result<0)                    display("isize : %i : pos %i \n", (int)isize, (int)(ip-(byte*)compressedbuffer));
                if (result == (size_t)-error_checksum_invalid) locatebuffdiff((byte*)srcbuffer+srcstart, decodedbuffer, srcsize, noncontiguousdst);
                check(lz4f_iserror(result), "decompression failed (error %i:%s)", (int)result, lz4f_geterrorname((lz4f_errorcode_t)result));
                xxh64_update(&xxh64, op, (u32)osize);
                op += osize;
                ip += isize;
                op += noncontiguousdst;
                if (noncontiguousdst==2) op = decodedbuffer;   // overwritten destination
            }
            check(result != 0, "frame decompression failed (error %i)", (int)result);
            crcdecoded = xxh64_digest(&xxh64);
            if (crcdecoded != crcorig) locatebuffdiff((byte*)srcbuffer+srcstart, decodedbuffer, srcsize, noncontiguousdst);
            check(crcdecoded != crcorig, "decompression corruption");
        }
    }

    displaylevel(2, "\rall tests completed   \n");

_end:
    lz4f_freedecompressioncontext(dctx);
    lz4f_freecompressioncontext(cctx);
    free(srcbuffer);
    free(compressedbuffer);
    free(decodedbuffer);

    if (pause)
    {
        display("press enter to finish \n");
        getchar();
    }
    return testresult;

_output_error:
    testresult = 1;
    goto _end;
}


int fuz_usage(void)
{
    display( "usage :\n");
    display( "      %s [args]\n", programname);
    display( "\n");
    display( "arguments :\n");
    display( " -i#    : nb of tests (default:%u) \n", nbtestsdefault);
    display( " -s#    : select seed (default:prompt user)\n");
    display( " -t#    : select starting test number (default:0)\n");
    display( " -p#    : select compressibility in %% (default:%i%%)\n", fuz_compressibility_default);
    display( " -v     : verbose\n");
    display( " -h     : display help and exit\n");
    return 0;
}


int main(int argc, char** argv)
{
    u32 seed=0;
    int seedset=0;
    int argnb;
    int nbtests = nbtestsdefault;
    int testnb = 0;
    int proba = fuz_compressibility_default;
    int result=0;

    // check command line
    programname = argv[0];
    for(argnb=1; argnb<argc; argnb++)
    {
        char* argument = argv[argnb];

        if(!argument) continue;   // protection if argument empty

        // decode command (note : aggregated commands are allowed)
        if (argument[0]=='-')
        {
            if (!strcmp(argument, "--no-prompt"))
            {
                no_prompt=1;
                seedset=1;
                displaylevel=1;
                continue;
            }
            argument++;

            while (*argument!=0)
            {
                switch(*argument)
                {
                case 'h':
                    return fuz_usage();
                case 'v':
                    argument++;
                    displaylevel=4;
                    break;
                case 'q':
                    argument++;
                    displaylevel--;
                    break;
                case 'p': /* pause at the end */
                    argument++;
                    pause = 1;
                    break;

                case 'i':
                    argument++;
                    nbtests=0;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        nbtests *= 10;
                        nbtests += *argument - '0';
                        argument++;
                    }
                    break;
                case 's':
                    argument++;
                    seed=0;
                    seedset=1;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        seed *= 10;
                        seed += *argument - '0';
                        argument++;
                    }
                    break;
                case 't':
                    argument++;
                    testnb=0;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        testnb *= 10;
                        testnb += *argument - '0';
                        argument++;
                    }
                    break;
                case 'p':   /* compressibility % */
                    argument++;
                    proba=0;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        proba *= 10;
                        proba += *argument - '0';
                        argument++;
                    }
                    if (proba<0) proba=0;
                    if (proba>100) proba=100;
                    break;
                default:
                    ;
                    return fuz_usage();
                }
            }
        }
    }

    // get seed
    printf("starting lz4frame tester (%i-bits, %s)\n", (int)(sizeof(size_t)*8), lz4_version);

    if (!seedset) seed = fuz_getmillistart() % 10000;
    printf("seed = %u\n", seed);
    if (proba!=fuz_compressibility_default) printf("compressibility : %i%%\n", proba);

    if (nbtests<=0) nbtests=1;

    if (testnb==0) result = basictests(seed, ((double)proba) / 100);
    if (result) return 1;
    return fuzzertests(seed, nbtests, testnb, ((double)proba) / 100);
}
