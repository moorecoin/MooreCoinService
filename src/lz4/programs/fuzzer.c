/*
    fuzzer.c - fuzzer test tool for lz4
    copyright (c) yann collet 2012-2015

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
    - lz4 source repository : http://code.google.com/p/lz4
    - lz4 source mirror : https://github.com/cyan4973/lz4
    - lz4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/**************************************
* remove visual warning messages
**************************************/
#ifdef _msc_ver    /* visual studio */
#  define _crt_secure_no_warnings    /* fgets */
#  pragma warning(disable : 4127)    /* disable: c4127: conditional expression is constant */
#  pragma warning(disable : 4146)    /* disable: c4146: minus unsigned expression */
#  pragma warning(disable : 4310)    /* disable: c4310: constant char value > 127 */
#endif


/**************************************
* includes
**************************************/
#include <stdlib.h>
#include <stdio.h>      /* fgets, sscanf */
#include <sys/timeb.h>  /* timeb */
#include <string.h>     /* strcmp */
#include "lz4.h"
#include "lz4hc.h"
#include "xxhash.h"


/**************************************
* basic types
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
* constants
**************************************/
#ifndef lz4_version
#  define lz4_version ""
#endif

#define nb_attempts (1<<16)
#define compressible_noise_length (1 << 21)
#define fuz_max_block_size (1 << 17)
#define fuz_max_dict_size  (1 << 15)
#define fuz_compressibility_default 60
#define prime1   2654435761u
#define prime2   2246822519u
#define prime3   3266489917u

#define kb *(1u<<10)
#define mb *(1u<<20)
#define gb *(1u<<30)


/*****************************************
* macros
*****************************************/
#define display(...)         fprintf(stderr, __va_args__)
#define displaylevel(l, ...) if (g_displaylevel>=l) { display(__va_args__); }
static int g_displaylevel = 2;
static const u32 g_refreshrate = 250;
static u32 g_time = 0;


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

static u32 fuz_rotl32(u32 u32, u32 nbbits)
{
    return ((u32 << nbbits) | (u32 >> (32 - nbbits)));
}

static u32 fuz_rand(u32* src)
{
    u32 rand32 = *src;
    rand32 *= prime1;
    rand32 += prime2;
    rand32  = fuz_rotl32(rand32, 13);
    *src = rand32;
    return rand32 >> 3;
}


#define fuz_rand15bits  ((fuz_rand(seed) >> 3) & 32767)
#define fuz_randlength  ( ((fuz_rand(seed) >> 7) & 3) ? (fuz_rand(seed) % 15) : (fuz_rand(seed) % 510) + 15)
static void fuz_fillcompressiblenoisebuffer(void* buffer, size_t buffersize, double proba, u32* seed)
{
    byte* bbuffer = (byte*)buffer;
    size_t pos = 0;
    u32 p32 = (u32)(32768 * proba);

    // first byte
    bbuffer[pos++] = (byte)(fuz_rand(seed));

    while (pos < buffersize)
    {
        // select : literal (noise) or copy (within 64k)
        if (fuz_rand15bits < p32)
        {
            // copy (within 64k)
            size_t match, d;
            size_t length = fuz_randlength + 4;
            size_t offset = fuz_rand15bits + 1;
            if (offset > pos) offset = pos;
            d = pos + length;
            if (d > buffersize) d = buffersize;
            match = pos - offset;
            while (pos < d) bbuffer[pos++] = bbuffer[match++];
        }
        else
        {
            // literal (noise)
            size_t d;
            size_t length = fuz_randlength;
            d = pos + length;
            if (d > buffersize) d = buffersize;
            while (pos < d) bbuffer[pos++] = (byte)(fuz_rand(seed) >> 5);
        }
    }
}


#define max_nb_buff_i134 150
#define blocksize_i134   (32 mb)
static int fuz_addressoverflow(void)
{
    char* buffers[max_nb_buff_i134+1] = {0};
    int i, nbbuff=0;
    int highaddress = 0;

    printf("overflow tests : ");

    // only possible in 32-bits
    if (sizeof(void*)==8)
    {
        printf("64 bits mode : no overflow \n");
        fflush(stdout);
        return 0;
    }

    buffers[0] = (char*)malloc(blocksize_i134);
    buffers[1] = (char*)malloc(blocksize_i134);
    if ((!buffers[0]) || (!buffers[1]))
    {
        printf("not enough memory for tests \n");
        return 0;
    }
    for (nbbuff=2; nbbuff < max_nb_buff_i134; nbbuff++)
    {
        printf("%3i \b\b\b\b", nbbuff);
        buffers[nbbuff] = (char*)malloc(blocksize_i134);
        //printf("%08x ", (u32)(size_t)(buffers[nbbuff]));
        fflush(stdout);

        if (((size_t)buffers[nbbuff] > (size_t)0x80000000) && (!highaddress))
        {
            printf("high address detected : ");
            fflush(stdout);
            highaddress=1;
        }
        if (buffers[nbbuff]==null) goto _endoftests;

        {
            size_t sizetogenerateoverflow = (size_t)(- ((size_t)buffers[nbbuff-1]) + 512);
            int nbof255 = (int)((sizetogenerateoverflow / 255) + 1);
            char* input = buffers[nbbuff-1];
            char* output = buffers[nbbuff];
            int r;
            input[0] = (char)0xf0;   // literal length overflow
            input[1] = (char)0xff;
            input[2] = (char)0xff;
            input[3] = (char)0xff;
            for(i = 4; i <= nbof255+4; i++) input[i] = (char)0xff;
            r = lz4_decompress_safe(input, output, nbof255+64, blocksize_i134);
            if (r>0) goto _overflowerror;
            input[0] = (char)0x1f;   // match length overflow
            input[1] = (char)0x01;
            input[2] = (char)0x01;
            input[3] = (char)0x00;
            r = lz4_decompress_safe(input, output, nbof255+64, blocksize_i134);
            if (r>0) goto _overflowerror;

            output = buffers[nbbuff-2];   // reverse in/out pointer order
            input[0] = (char)0xf0;   // literal length overflow
            input[1] = (char)0xff;
            input[2] = (char)0xff;
            input[3] = (char)0xff;
            r = lz4_decompress_safe(input, output, nbof255+64, blocksize_i134);
            if (r>0) goto _overflowerror;
            input[0] = (char)0x1f;   // match length overflow
            input[1] = (char)0x01;
            input[2] = (char)0x01;
            input[3] = (char)0x00;
            r = lz4_decompress_safe(input, output, nbof255+64, blocksize_i134);
            if (r>0) goto _overflowerror;
        }
    }

    nbbuff++;
_endoftests:
    for (i=0 ; i<nbbuff; i++) free(buffers[i]);
    if (!highaddress) printf("high address not possible \n");
    else printf("all overflows correctly detected \n");
    return 0;

_overflowerror:
    printf("address space overflow error !! \n");
    exit(1);
}


static void fuz_displayupdate(unsigned testnb)
{
    if ((fuz_getmillispan(g_time) > g_refreshrate) || (g_displaylevel>=3))
    {
        g_time = fuz_getmillistart();
        display("\r%5u   ", testnb);
        if (g_displaylevel>=3) fflush(stdout);
    }
}


static int fuz_test(u32 seed, const u32 nbcycles, const u32 startcycle, const double compressibility)
{
    unsigned long long bytes = 0;
    unsigned long long cbytes = 0;
    unsigned long long hcbytes = 0;
    unsigned long long ccbytes = 0;
    void* cnbuffer;
    char* compressedbuffer;
    char* decodedbuffer;
#   define fuz_max   lz4_compressbound(len)
    int ret;
    unsigned cyclenb;
#   define fuz_checktest(cond, ...) if (cond) { printf("test %u : ", testnb); printf(__va_args__); \
    printf(" (seed %u, cycle %u) \n", seed, cyclenb); goto _output_error; }
#   define fuz_displaytest          { testnb++; g_displaylevel<3 ? 0 : printf("%2u\b\b", testnb); if (g_displaylevel==4) fflush(stdout); }
    void* statelz4   = malloc(lz4_sizeofstate());
    void* statelz4hc = malloc(lz4_sizeofstatehc());
    void* lz4continue;
    lz4_stream_t lz4dict;
    lz4_streamhc_t lz4dicthc;
    u32 crcorig, crccheck;
    u32 corerandstate = seed;
    u32 randstate = corerandstate ^ prime3;


    // init
    memset(&lz4dict, 0, sizeof(lz4dict));

    // create compressible test buffer
    cnbuffer = malloc(compressible_noise_length);
    fuz_fillcompressiblenoisebuffer(cnbuffer, compressible_noise_length, compressibility, &randstate);
    compressedbuffer = (char*)malloc(lz4_compressbound(fuz_max_block_size));
    decodedbuffer = (char*)malloc(fuz_max_dict_size + fuz_max_block_size);

    // move to startcycle
    for (cyclenb = 0; cyclenb < startcycle; cyclenb++)
    {
        (void)fuz_rand(&corerandstate);

        if (0)   // some problems related to dictionary re-use; in this case, enable this loop
        {
            int dictsize, blocksize, blockstart;
            char* dict;
            char* block;
            fuz_displayupdate(cyclenb);
            randstate = corerandstate ^ prime3;
            blocksize  = fuz_rand(&randstate) % fuz_max_block_size;
            blockstart = fuz_rand(&randstate) % (compressible_noise_length - blocksize);
            dictsize   = fuz_rand(&randstate) % fuz_max_dict_size;
            if (dictsize > blockstart) dictsize = blockstart;
            block = ((char*)cnbuffer) + blockstart;
            dict = block - dictsize;
            lz4_loaddict(&lz4dict, dict, dictsize);
            lz4_compress_continue(&lz4dict, block, compressedbuffer, blocksize);
            lz4_loaddict(&lz4dict, dict, dictsize);
            lz4_compress_continue(&lz4dict, block, compressedbuffer, blocksize);
            lz4_loaddict(&lz4dict, dict, dictsize);
            lz4_compress_continue(&lz4dict, block, compressedbuffer, blocksize);
        }
    }

    // test loop
    for (cyclenb = startcycle; cyclenb < nbcycles; cyclenb++)
    {
        u32 testnb = 0;
        char* dict;
        char* block;
        int dictsize, blocksize, blockstart, compressedsize, hccompressedsize;
        int blockcontinuecompressedsize;

        fuz_displayupdate(cyclenb);
        (void)fuz_rand(&corerandstate);
        randstate = corerandstate ^ prime3;

        // select block to test
        blocksize  = fuz_rand(&randstate) % fuz_max_block_size;
        blockstart = fuz_rand(&randstate) % (compressible_noise_length - blocksize);
        dictsize   = fuz_rand(&randstate) % fuz_max_dict_size;
        if (dictsize > blockstart) dictsize = blockstart;
        block = ((char*)cnbuffer) + blockstart;
        dict = block - dictsize;

        /* compression tests */

        // test compression hc
        fuz_displaytest;
        ret = lz4_compresshc(block, compressedbuffer, blocksize);
        fuz_checktest(ret==0, "lz4_compresshc() failed");
        hccompressedsize = ret;

        // test compression hc using external state
        fuz_displaytest;
        ret = lz4_compresshc_withstatehc(statelz4hc, block, compressedbuffer, blocksize);
        fuz_checktest(ret==0, "lz4_compresshc_withstatehc() failed");

        // test compression using external state
        fuz_displaytest;
        ret = lz4_compress_withstate(statelz4, block, compressedbuffer, blocksize);
        fuz_checktest(ret==0, "lz4_compress_withstate() failed");

        // test compression
        fuz_displaytest;
        ret = lz4_compress(block, compressedbuffer, blocksize);
        fuz_checktest(ret==0, "lz4_compress() failed");
        compressedsize = ret;

        /* decompression tests */

        crcorig = xxh32(block, blocksize, 0);

        // test decoding with output size being exactly what's necessary => must work
        fuz_displaytest;
        ret = lz4_decompress_fast(compressedbuffer, decodedbuffer, blocksize);
        fuz_checktest(ret<0, "lz4_decompress_fast failed despite correct space");
        fuz_checktest(ret!=compressedsize, "lz4_decompress_fast failed : did not fully read compressed data");
        crccheck = xxh32(decodedbuffer, blocksize, 0);
        fuz_checktest(crccheck!=crcorig, "lz4_decompress_fast corrupted decoded data");

        // test decoding with one byte missing => must fail
        fuz_displaytest;
        decodedbuffer[blocksize-1] = 0;
        ret = lz4_decompress_fast(compressedbuffer, decodedbuffer, blocksize-1);
        fuz_checktest(ret>=0, "lz4_decompress_fast should have failed, due to output size being too small");
        fuz_checktest(decodedbuffer[blocksize-1], "lz4_decompress_fast overrun specified output buffer");

        // test decoding with one byte too much => must fail
        fuz_displaytest;
        ret = lz4_decompress_fast(compressedbuffer, decodedbuffer, blocksize+1);
        fuz_checktest(ret>=0, "lz4_decompress_fast should have failed, due to output size being too large");

        // test decoding with output size exactly what's necessary => must work
        fuz_displaytest;
        decodedbuffer[blocksize] = 0;
        ret = lz4_decompress_safe(compressedbuffer, decodedbuffer, compressedsize, blocksize);
        fuz_checktest(ret<0, "lz4_decompress_safe failed despite sufficient space");
        fuz_checktest(ret!=blocksize, "lz4_decompress_safe did not regenerate original data");
        fuz_checktest(decodedbuffer[blocksize], "lz4_decompress_safe overrun specified output buffer size");
        crccheck = xxh32(decodedbuffer, blocksize, 0);
        fuz_checktest(crccheck!=crcorig, "lz4_decompress_safe corrupted decoded data");

        // test decoding with more than enough output size => must work
        fuz_displaytest;
        decodedbuffer[blocksize] = 0;
        decodedbuffer[blocksize+1] = 0;
        ret = lz4_decompress_safe(compressedbuffer, decodedbuffer, compressedsize, blocksize+1);
        fuz_checktest(ret<0, "lz4_decompress_safe failed despite amply sufficient space");
        fuz_checktest(ret!=blocksize, "lz4_decompress_safe did not regenerate original data");
        //fuz_checktest(decodedbuffer[blocksize], "lz4_decompress_safe wrote more than (unknown) target size");   // well, is that an issue ?
        fuz_checktest(decodedbuffer[blocksize+1], "lz4_decompress_safe overrun specified output buffer size");
        crccheck = xxh32(decodedbuffer, blocksize, 0);
        fuz_checktest(crccheck!=crcorig, "lz4_decompress_safe corrupted decoded data");

        // test decoding with output size being one byte too short => must fail
        fuz_displaytest;
        decodedbuffer[blocksize-1] = 0;
        ret = lz4_decompress_safe(compressedbuffer, decodedbuffer, compressedsize, blocksize-1);
        fuz_checktest(ret>=0, "lz4_decompress_safe should have failed, due to output size being one byte too short");
        fuz_checktest(decodedbuffer[blocksize-1], "lz4_decompress_safe overrun specified output buffer size");

        // test decoding with output size being 10 bytes too short => must fail
        fuz_displaytest;
        if (blocksize>10)
        {
            decodedbuffer[blocksize-10] = 0;
            ret = lz4_decompress_safe(compressedbuffer, decodedbuffer, compressedsize, blocksize-10);
            fuz_checktest(ret>=0, "lz4_decompress_safe should have failed, due to output size being 10 bytes too short");
            fuz_checktest(decodedbuffer[blocksize-10], "lz4_decompress_safe overrun specified output buffer size");
        }

        // test decoding with input size being one byte too short => must fail
        fuz_displaytest;
        ret = lz4_decompress_safe(compressedbuffer, decodedbuffer, compressedsize-1, blocksize);
        fuz_checktest(ret>=0, "lz4_decompress_safe should have failed, due to input size being one byte too short (blocksize=%i, ret=%i, compressedsize=%i)", blocksize, ret, compressedsize);

        // test decoding with input size being one byte too large => must fail
        fuz_displaytest;
        decodedbuffer[blocksize] = 0;
        ret = lz4_decompress_safe(compressedbuffer, decodedbuffer, compressedsize+1, blocksize);
        fuz_checktest(ret>=0, "lz4_decompress_safe should have failed, due to input size being too large");
        fuz_checktest(decodedbuffer[blocksize], "lz4_decompress_safe overrun specified output buffer size");

        // test partial decoding with target output size being max/2 => must work
        fuz_displaytest;
        ret = lz4_decompress_safe_partial(compressedbuffer, decodedbuffer, compressedsize, blocksize/2, blocksize);
        fuz_checktest(ret<0, "lz4_decompress_safe_partial failed despite sufficient space");

        // test partial decoding with target output size being just below max => must work
        fuz_displaytest;
        ret = lz4_decompress_safe_partial(compressedbuffer, decodedbuffer, compressedsize, blocksize-3, blocksize);
        fuz_checktest(ret<0, "lz4_decompress_safe_partial failed despite sufficient space");

        /* test compression with limited output size */

        /* test compression with output size being exactly what's necessary (should work) */
        fuz_displaytest;
        ret = lz4_compress_limitedoutput(block, compressedbuffer, blocksize, compressedsize);
        fuz_checktest(ret==0, "lz4_compress_limitedoutput() failed despite sufficient space");

        /* test compression with output size being exactly what's necessary and external state (should work) */
        fuz_displaytest;
        ret = lz4_compress_limitedoutput_withstate(statelz4, block, compressedbuffer, blocksize, compressedsize);
        fuz_checktest(ret==0, "lz4_compress_limitedoutput_withstate() failed despite sufficient space");

        /* test hc compression with output size being exactly what's necessary (should work) */
        fuz_displaytest;
        ret = lz4_compresshc_limitedoutput(block, compressedbuffer, blocksize, hccompressedsize);
        fuz_checktest(ret==0, "lz4_compresshc_limitedoutput() failed despite sufficient space");

        /* test hc compression with output size being exactly what's necessary (should work) */
        fuz_displaytest;
        ret = lz4_compresshc_limitedoutput_withstatehc(statelz4hc, block, compressedbuffer, blocksize, hccompressedsize);
        fuz_checktest(ret==0, "lz4_compresshc_limitedoutput_withstatehc() failed despite sufficient space");

        /* test compression with missing bytes into output buffer => must fail */
        fuz_displaytest;
        {
            int missingbytes = (fuz_rand(&randstate) % 0x3f) + 1;
            if (missingbytes >= compressedsize) missingbytes = compressedsize-1;
            missingbytes += !missingbytes;   /* avoid special case missingbytes==0 */
            compressedbuffer[compressedsize-missingbytes] = 0;
            ret = lz4_compress_limitedoutput(block, compressedbuffer, blocksize, compressedsize-missingbytes);
            fuz_checktest(ret, "lz4_compress_limitedoutput should have failed (output buffer too small by %i byte)", missingbytes);
            fuz_checktest(compressedbuffer[compressedsize-missingbytes], "lz4_compress_limitedoutput overran output buffer ! (%i missingbytes)", missingbytes)
        }

        /* test hc compression with missing bytes into output buffer => must fail */
        fuz_displaytest;
        {
            int missingbytes = (fuz_rand(&randstate) % 0x3f) + 1;
            if (missingbytes >= hccompressedsize) missingbytes = hccompressedsize-1;
            missingbytes += !missingbytes;   /* avoid special case missingbytes==0 */
            compressedbuffer[hccompressedsize-missingbytes] = 0;
            ret = lz4_compresshc_limitedoutput(block, compressedbuffer, blocksize, hccompressedsize-missingbytes);
            fuz_checktest(ret, "lz4_compresshc_limitedoutput should have failed (output buffer too small by %i byte)", missingbytes);
            fuz_checktest(compressedbuffer[hccompressedsize-missingbytes], "lz4_compresshc_limitedoutput overran output buffer ! (%i missingbytes)", missingbytes)
        }


        /********************/
        /* dictionary tests */
        /********************/

        /* compress using dictionary */
        fuz_displaytest;
        lz4continue = lz4_create (dict);
        lz4_compress_continue ((lz4_stream_t*)lz4continue, dict, compressedbuffer, dictsize);   // just to fill hash tables
        blockcontinuecompressedsize = lz4_compress_continue ((lz4_stream_t*)lz4continue, block, compressedbuffer, blocksize);
        fuz_checktest(blockcontinuecompressedsize==0, "lz4_compress_continue failed");
        free (lz4continue);

        /* decompress with dictionary as prefix */
        fuz_displaytest;
        memcpy(decodedbuffer, dict, dictsize);
        ret = lz4_decompress_fast_withprefix64k(compressedbuffer, decodedbuffer+dictsize, blocksize);
        fuz_checktest(ret!=blockcontinuecompressedsize, "lz4_decompress_fast_withprefix64k did not read all compressed block input");
        crccheck = xxh32(decodedbuffer+dictsize, blocksize, 0);
        if (crccheck!=crcorig)
        {
            int i=0;
            while (block[i]==decodedbuffer[i]) i++;
            printf("wrong byte at position %i/%i\n", i, blocksize);

        }
        fuz_checktest(crccheck!=crcorig, "lz4_decompress_fast_withprefix64k corrupted decoded data (dict %i)", dictsize);

        fuz_displaytest;
        ret = lz4_decompress_safe_withprefix64k(compressedbuffer, decodedbuffer+dictsize, blockcontinuecompressedsize, blocksize);
        fuz_checktest(ret!=blocksize, "lz4_decompress_safe_withprefix64k did not regenerate original data");
        crccheck = xxh32(decodedbuffer+dictsize, blocksize, 0);
        fuz_checktest(crccheck!=crcorig, "lz4_decompress_safe_withprefix64k corrupted decoded data");

        // compress using external dictionary
        fuz_displaytest;
        dict -= (fuz_rand(&randstate) & 0xf) + 1;   // separation, so it is an extdict
        if (dict < (char*)cnbuffer) dict = (char*)cnbuffer;
        lz4_loaddict(&lz4dict, dict, dictsize);
        blockcontinuecompressedsize = lz4_compress_continue(&lz4dict, block, compressedbuffer, blocksize);
        fuz_checktest(blockcontinuecompressedsize==0, "lz4_compress_continue failed");

        fuz_displaytest;
        lz4_loaddict(&lz4dict, dict, dictsize);
        ret = lz4_compress_limitedoutput_continue(&lz4dict, block, compressedbuffer, blocksize, blockcontinuecompressedsize-1);
        fuz_checktest(ret>0, "lz4_compress_limitedoutput_continue using extdict should fail : one missing byte for output buffer");

        fuz_displaytest;
        lz4_loaddict(&lz4dict, dict, dictsize);
        ret = lz4_compress_limitedoutput_continue(&lz4dict, block, compressedbuffer, blocksize, blockcontinuecompressedsize);
        fuz_checktest(ret!=blockcontinuecompressedsize, "lz4_compress_limitedoutput_compressed size is different (%i != %i)", ret, blockcontinuecompressedsize);
        fuz_checktest(ret<=0, "lz4_compress_limitedoutput_continue should work : enough size available within output buffer");

        // decompress with dictionary as external
        fuz_displaytest;
        decodedbuffer[blocksize] = 0;
        ret = lz4_decompress_fast_usingdict(compressedbuffer, decodedbuffer, blocksize, dict, dictsize);
        fuz_checktest(ret!=blockcontinuecompressedsize, "lz4_decompress_fast_usingdict did not read all compressed block input");
        fuz_checktest(decodedbuffer[blocksize], "lz4_decompress_fast_usingdict overrun specified output buffer size")
            crccheck = xxh32(decodedbuffer, blocksize, 0);
        if (crccheck!=crcorig)
        {
            int i=0;
            while (block[i]==decodedbuffer[i]) i++;
            printf("wrong byte at position %i/%i\n", i, blocksize);
        }
        fuz_checktest(crccheck!=crcorig, "lz4_decompress_fast_usingdict corrupted decoded data (dict %i)", dictsize);

        fuz_displaytest;
        decodedbuffer[blocksize] = 0;
        ret = lz4_decompress_safe_usingdict(compressedbuffer, decodedbuffer, blockcontinuecompressedsize, blocksize, dict, dictsize);
        fuz_checktest(ret!=blocksize, "lz4_decompress_safe_usingdict did not regenerate original data");
        fuz_checktest(decodedbuffer[blocksize], "lz4_decompress_safe_usingdict overrun specified output buffer size")
            crccheck = xxh32(decodedbuffer, blocksize, 0);
        fuz_checktest(crccheck!=crcorig, "lz4_decompress_safe_usingdict corrupted decoded data");

        fuz_displaytest;
        decodedbuffer[blocksize-1] = 0;
        ret = lz4_decompress_fast_usingdict(compressedbuffer, decodedbuffer, blocksize-1, dict, dictsize);
        fuz_checktest(ret>=0, "lz4_decompress_fast_withdict should have failed : wrong original size (-1 byte)");
        fuz_checktest(decodedbuffer[blocksize-1], "lz4_decompress_fast_usingdict overrun specified output buffer size");

        fuz_displaytest;
        decodedbuffer[blocksize-1] = 0;
        ret = lz4_decompress_safe_usingdict(compressedbuffer, decodedbuffer, blockcontinuecompressedsize, blocksize-1, dict, dictsize);
        fuz_checktest(ret>=0, "lz4_decompress_safe_usingdict should have failed : not enough output size (-1 byte)");
        fuz_checktest(decodedbuffer[blocksize-1], "lz4_decompress_safe_usingdict overrun specified output buffer size");

        fuz_displaytest;
        {
            u32 missingbytes = (fuz_rand(&randstate) & 0xf) + 2;
            if ((u32)blocksize > missingbytes)
            {
                decodedbuffer[blocksize-missingbytes] = 0;
                ret = lz4_decompress_safe_usingdict(compressedbuffer, decodedbuffer, blockcontinuecompressedsize, blocksize-missingbytes, dict, dictsize);
                fuz_checktest(ret>=0, "lz4_decompress_safe_usingdict should have failed : output buffer too small (-%u byte)", missingbytes);
                fuz_checktest(decodedbuffer[blocksize-missingbytes], "lz4_decompress_safe_usingdict overrun specified output buffer size (-%u byte) (blocksize=%i)", missingbytes, blocksize);
            }
        }

        // compress hc using external dictionary
        fuz_displaytest;
        dict -= (fuz_rand(&randstate) & 7);    // even bigger separation
        if (dict < (char*)cnbuffer) dict = (char*)cnbuffer;
        lz4_resetstreamhc (&lz4dicthc, fuz_rand(&randstate) & 0x7);
        lz4_loaddicthc(&lz4dicthc, dict, dictsize);
        blockcontinuecompressedsize = lz4_compresshc_continue(&lz4dicthc, block, compressedbuffer, blocksize);
        fuz_checktest(blockcontinuecompressedsize==0, "lz4_compresshc_continue failed");

        fuz_displaytest;
        lz4_loaddicthc(&lz4dicthc, dict, dictsize);
        ret = lz4_compresshc_limitedoutput_continue(&lz4dicthc, block, compressedbuffer, blocksize, blockcontinuecompressedsize-1);
        fuz_checktest(ret>0, "lz4_compresshc_limitedoutput_continue using extdict should fail : one missing byte for output buffer");

        fuz_displaytest;
        lz4_loaddicthc(&lz4dicthc, dict, dictsize);
        ret = lz4_compresshc_limitedoutput_continue(&lz4dicthc, block, compressedbuffer, blocksize, blockcontinuecompressedsize);
        fuz_checktest(ret!=blockcontinuecompressedsize, "lz4_compress_limitedoutput_compressed size is different (%i != %i)", ret, blockcontinuecompressedsize);
        fuz_checktest(ret<=0, "lz4_compress_limitedoutput_continue should work : enough size available within output buffer");

        fuz_displaytest;
        decodedbuffer[blocksize] = 0;
        ret = lz4_decompress_safe_usingdict(compressedbuffer, decodedbuffer, blockcontinuecompressedsize, blocksize, dict, dictsize);
        fuz_checktest(ret!=blocksize, "lz4_decompress_safe_usingdict did not regenerate original data");
        fuz_checktest(decodedbuffer[blocksize], "lz4_decompress_safe_usingdict overrun specified output buffer size")
            crccheck = xxh32(decodedbuffer, blocksize, 0);
        if (crccheck!=crcorig)
        {
            int i=0;
            while (block[i]==decodedbuffer[i]) i++;
            printf("wrong byte at position %i/%i\n", i, blocksize);
        }
        fuz_checktest(crccheck!=crcorig, "lz4_decompress_safe_usingdict corrupted decoded data");


        // ***** end of tests *** //
        // fill stats
        bytes += blocksize;
        cbytes += compressedsize;
        hcbytes += hccompressedsize;
        ccbytes += blockcontinuecompressedsize;
    }

    printf("\r%7u /%7u   - ", cyclenb, nbcycles);
    printf("all tests completed successfully \n");
    printf("compression ratio: %0.3f%%\n", (double)cbytes/bytes*100);
    printf("hc compression ratio: %0.3f%%\n", (double)hcbytes/bytes*100);
    printf("ratio with dict: %0.3f%%\n", (double)ccbytes/bytes*100);

    // unalloc
    {
        int result = 0;
_exit:
        free(cnbuffer);
        free(compressedbuffer);
        free(decodedbuffer);
        free(statelz4);
        free(statelz4hc);
        return result;

_output_error:
        result = 1;
        goto _exit;
    }
}


#define testinputsize (192 kb)
#define testcompressedsize (128 kb)
#define ringbuffersize (8 kb)

static void fuz_unittests(void)
{
    const unsigned testnb = 0;
    const unsigned seed   = 0;
    const unsigned cyclenb= 0;
    char testinput[testinputsize];
    char testcompressed[testcompressedsize];
    char testverify[testinputsize];
    char ringbuffer[ringbuffersize];
    u32 randstate = 1;

    // init
    fuz_fillcompressiblenoisebuffer(testinput, testinputsize, 0.50, &randstate);

    // 32-bits address space overflow test
    fuz_addressoverflow();

    // lz4 streaming tests
    {
        lz4_stream_t* stateptr;
        lz4_stream_t  streamingstate;
        u64 crcorig;
        u64 crcnew;
        int result;

        // allocation test
        stateptr = lz4_createstream();
        fuz_checktest(stateptr==null, "lz4_createstream() allocation failed");
        lz4_freestream(stateptr);

        // simple compression test
        crcorig = xxh64(testinput, testcompressedsize, 0);
        lz4_resetstream(&streamingstate);
        result = lz4_compress_limitedoutput_continue(&streamingstate, testinput, testcompressed, testcompressedsize, testcompressedsize-1);
        fuz_checktest(result==0, "lz4_compress_limitedoutput_continue() compression failed");

        result = lz4_decompress_safe(testcompressed, testverify, result, testcompressedsize);
        fuz_checktest(result!=(int)testcompressedsize, "lz4_decompress_safe() decompression failed");
        crcnew = xxh64(testverify, testcompressedsize, 0);
        fuz_checktest(crcorig!=crcnew, "lz4_decompress_safe() decompression corruption");

        // ring buffer test
        {
            xxh64_state_t xxhorig;
            xxh64_state_t xxhnew;
            lz4_streamdecode_t decodestate;
            const u32 maxmessagesizelog = 10;
            const u32 maxmessagesizemask = (1<<maxmessagesizelog) - 1;
            u32 messagesize = (fuz_rand(&randstate) & maxmessagesizemask) + 1;
            u32 inext = 0;
            u32 rnext = 0;
            u32 dnext = 0;
            const u32 dbuffersize = ringbuffersize + maxmessagesizemask;

            xxh64_reset(&xxhorig, 0);
            xxh64_reset(&xxhnew, 0);
            lz4_resetstream(&streamingstate);
            lz4_setstreamdecode(&decodestate, null, 0);

            while (inext + messagesize < testcompressedsize)
            {
                xxh64_update(&xxhorig, testinput + inext, messagesize);
                crcorig = xxh64_digest(&xxhorig);

                memcpy (ringbuffer + rnext, testinput + inext, messagesize);
                result = lz4_compress_limitedoutput_continue(&streamingstate, ringbuffer + rnext, testcompressed, messagesize, testcompressedsize-ringbuffersize);
                fuz_checktest(result==0, "lz4_compress_limitedoutput_continue() compression failed");

                result = lz4_decompress_safe_continue(&decodestate, testcompressed, testverify + dnext, result, messagesize);
                fuz_checktest(result!=(int)messagesize, "ringbuffer : lz4_decompress_safe() test failed");

                xxh64_update(&xxhnew, testverify + dnext, messagesize);
                crcnew = crcorig = xxh64_digest(&xxhnew);
                fuz_checktest(crcorig!=crcnew, "lz4_decompress_safe() decompression corruption");

                // prepare next message
                inext += messagesize;
                rnext += messagesize;
                dnext += messagesize;
                messagesize = (fuz_rand(&randstate) & maxmessagesizemask) + 1;
                if (rnext + messagesize > ringbuffersize) rnext = 0;
                if (dnext + messagesize > dbuffersize) dnext = 0;
            }
        }
    }

    // lz4 hc streaming tests
    {
        lz4_streamhc_t* sp;
        lz4_streamhc_t  shc;
        //xxh64_state_t xxh;
        u64 crcorig;
        u64 crcnew;
        int result;

        // allocation test
        sp = lz4_createstreamhc();
        fuz_checktest(sp==null, "lz4_createstreamhc() allocation failed");
        lz4_freestreamhc(sp);

        // simple compression test
        crcorig = xxh64(testinput, testcompressedsize, 0);
        lz4_resetstreamhc(&shc, 0);
        result = lz4_compresshc_limitedoutput_continue(&shc, testinput, testcompressed, testcompressedsize, testcompressedsize-1);
        fuz_checktest(result==0, "lz4_compresshc_limitedoutput_continue() compression failed");

        result = lz4_decompress_safe(testcompressed, testverify, result, testcompressedsize);
        fuz_checktest(result!=(int)testcompressedsize, "lz4_decompress_safe() decompression failed");
        crcnew = xxh64(testverify, testcompressedsize, 0);
        fuz_checktest(crcorig!=crcnew, "lz4_decompress_safe() decompression corruption");

        // simple dictionary compression test
        crcorig = xxh64(testinput + 64 kb, testcompressedsize, 0);
        lz4_resetstreamhc(&shc, 0);
        lz4_loaddicthc(&shc, testinput, 64 kb);
        result = lz4_compresshc_limitedoutput_continue(&shc, testinput + 64 kb, testcompressed, testcompressedsize, testcompressedsize-1);
        fuz_checktest(result==0, "lz4_compresshc_limitedoutput_continue() dictionary compression failed : result = %i", result);

        result = lz4_decompress_safe_usingdict(testcompressed, testverify, result, testcompressedsize, testinput, 64 kb);
        fuz_checktest(result!=(int)testcompressedsize, "lz4_decompress_safe() simple dictionary decompression test failed");
        crcnew = xxh64(testverify, testcompressedsize, 0);
        fuz_checktest(crcorig!=crcnew, "lz4_decompress_safe() simple dictionary decompression test : corruption");

        // multiple hc compression test with dictionary
        {
            int result1, result2;
            int segsize = testcompressedsize / 2;
            crcorig = xxh64(testinput + segsize, testcompressedsize, 0);
            lz4_resetstreamhc(&shc, 0);
            lz4_loaddicthc(&shc, testinput, segsize);
            result1 = lz4_compresshc_limitedoutput_continue(&shc, testinput + segsize, testcompressed, segsize, segsize -1);
            fuz_checktest(result1==0, "lz4_compresshc_limitedoutput_continue() dictionary compression failed : result = %i", result1);
            result2 = lz4_compresshc_limitedoutput_continue(&shc, testinput + 2*segsize, testcompressed+result1, segsize, segsize-1);
            fuz_checktest(result2==0, "lz4_compresshc_limitedoutput_continue() dictionary compression failed : result = %i", result2);

            result = lz4_decompress_safe_usingdict(testcompressed, testverify, result1, segsize, testinput, segsize);
            fuz_checktest(result!=segsize, "lz4_decompress_safe() dictionary decompression part 1 failed");
            result = lz4_decompress_safe_usingdict(testcompressed+result1, testverify+segsize, result2, segsize, testinput, 2*segsize);
            fuz_checktest(result!=segsize, "lz4_decompress_safe() dictionary decompression part 2 failed");
            crcnew = xxh64(testverify, testcompressedsize, 0);
            fuz_checktest(crcorig!=crcnew, "lz4_decompress_safe() dictionary decompression corruption");
        }

        // remote dictionary hc compression test
        crcorig = xxh64(testinput + 64 kb, testcompressedsize, 0);
        lz4_resetstreamhc(&shc, 0);
        lz4_loaddicthc(&shc, testinput, 32 kb);
        result = lz4_compresshc_limitedoutput_continue(&shc, testinput + 64 kb, testcompressed, testcompressedsize, testcompressedsize-1);
        fuz_checktest(result==0, "lz4_compresshc_limitedoutput_continue() remote dictionary failed : result = %i", result);

        result = lz4_decompress_safe_usingdict(testcompressed, testverify, result, testcompressedsize, testinput, 32 kb);
        fuz_checktest(result!=(int)testcompressedsize, "lz4_decompress_safe_usingdict() decompression failed following remote dictionary hc compression test");
        crcnew = xxh64(testverify, testcompressedsize, 0);
        fuz_checktest(crcorig!=crcnew, "lz4_decompress_safe_usingdict() decompression corruption");

        // multiple hc compression with ext. dictionary
        {
            xxh64_state_t crcorigstate;
            xxh64_state_t crcnewstate;
            const char* dict = testinput + 3;
            int dictsize = (fuz_rand(&randstate) & 8191);
            char* dst = testverify;

            size_t segstart = dictsize + 7;
            int segsize = (fuz_rand(&randstate) & 8191);
            int segnb = 1;

            lz4_resetstreamhc(&shc, 0);
            lz4_loaddicthc(&shc, dict, dictsize);

            xxh64_reset(&crcorigstate, 0);
            xxh64_reset(&crcnewstate, 0);

            while (segstart + segsize < testinputsize)
            {
                xxh64_update(&crcorigstate, testinput + segstart, segsize);
                crcorig = xxh64_digest(&crcorigstate);
                result = lz4_compresshc_limitedoutput_continue(&shc, testinput + segstart, testcompressed, segsize, lz4_compressbound(segsize));
                fuz_checktest(result==0, "lz4_compresshc_limitedoutput_continue() dictionary compression failed : result = %i", result);

                result = lz4_decompress_safe_usingdict(testcompressed, dst, result, segsize, dict, dictsize);
                fuz_checktest(result!=segsize, "lz4_decompress_safe_usingdict() dictionary decompression part %i failed", segnb);
                xxh64_update(&crcnewstate, dst, segsize);
                crcnew = xxh64_digest(&crcnewstate);
                if (crcorig!=crcnew)
                {
                    size_t c=0;
                    while (dst[c] == testinput[segstart+c]) c++;
                    display("bad decompression at %u / %u \n", (u32)c, (u32)segsize);
                }
                fuz_checktest(crcorig!=crcnew, "lz4_decompress_safe_usingdict() part %i corruption", segnb);

                dict = dst;
                //dict = testinput + segstart;
                dictsize = segsize;

                dst += segsize + 1;
                segnb ++;

                segstart += segsize + (fuz_rand(&randstate) & 0xf) + 1;
                segsize = (fuz_rand(&randstate) & 8191);
            }
        }

        // ring buffer test
        {
            xxh64_state_t xxhorig;
            xxh64_state_t xxhnew;
            lz4_streamdecode_t decodestate;
            const u32 maxmessagesizelog = 10;
            const u32 maxmessagesizemask = (1<<maxmessagesizelog) - 1;
            u32 messagesize = (fuz_rand(&randstate) & maxmessagesizemask) + 1;
            u32 inext = 0;
            u32 rnext = 0;
            u32 dnext = 0;
            const u32 dbuffersize = ringbuffersize + maxmessagesizemask;

            xxh64_reset(&xxhorig, 0);
            xxh64_reset(&xxhnew, 0);
            lz4_resetstreamhc(&shc, 0);
            lz4_setstreamdecode(&decodestate, null, 0);

            while (inext + messagesize < testcompressedsize)
            {
                xxh64_update(&xxhorig, testinput + inext, messagesize);
                crcorig = xxh64_digest(&xxhorig);

                memcpy (ringbuffer + rnext, testinput + inext, messagesize);
                result = lz4_compresshc_limitedoutput_continue(&shc, ringbuffer + rnext, testcompressed, messagesize, testcompressedsize-ringbuffersize);
                fuz_checktest(result==0, "lz4_compresshc_limitedoutput_continue() compression failed");

                result = lz4_decompress_safe_continue(&decodestate, testcompressed, testverify + dnext, result, messagesize);
                fuz_checktest(result!=(int)messagesize, "ringbuffer : lz4_decompress_safe() test failed");

                xxh64_update(&xxhnew, testverify + dnext, messagesize);
                crcnew = crcorig = xxh64_digest(&xxhnew);
                fuz_checktest(crcorig!=crcnew, "lz4_decompress_safe() decompression corruption");

                // prepare next message
                inext += messagesize;
                rnext += messagesize;
                dnext += messagesize;
                messagesize = (fuz_rand(&randstate) & maxmessagesizemask) + 1;
                if (rnext + messagesize > ringbuffersize) rnext = 0;
                if (dnext + messagesize > dbuffersize) dnext = 0;
            }
        }

        // small decoder-side ring buffer test
        {
            xxh64_state_t xxhorig;
            xxh64_state_t xxhnew;
            lz4_streamdecode_t decodestate;
            const u32 maxmessagesizelog = 10;
            const u32 maxmessagesizemask = (1<<maxmessagesizelog) - 1;
            u32 messagesize = (fuz_rand(&randstate) & maxmessagesizemask) + 1;
            u32 totalmessagesize = 0;
            u32 inext = 0;
            u32 dnext = 0;
            const u32 dbuffersize = 64 kb + maxmessagesizemask;

            xxh64_reset(&xxhorig, 0);
            xxh64_reset(&xxhnew, 0);
            lz4_resetstreamhc(&shc, 0);
            lz4_setstreamdecode(&decodestate, null, 0);

            while (totalmessagesize < 9 mb)
            {
                xxh64_update(&xxhorig, testinput + inext, messagesize);
                crcorig = xxh64_digest(&xxhorig);

                result = lz4_compresshc_limitedoutput_continue(&shc, testinput + inext, testcompressed, messagesize, testcompressedsize-ringbuffersize);
                fuz_checktest(result==0, "lz4_compresshc_limitedoutput_continue() compression failed");

                result = lz4_decompress_safe_continue(&decodestate, testcompressed, testverify + dnext, result, messagesize);
                fuz_checktest(result!=(int)messagesize, "ringbuffer : lz4_decompress_safe() test failed");

                xxh64_update(&xxhnew, testverify + dnext, messagesize);
                crcnew = crcorig = xxh64_digest(&xxhnew);
                fuz_checktest(crcorig!=crcnew, "lz4_decompress_safe() decompression corruption");

                // prepare next message
                dnext += messagesize;
                totalmessagesize += messagesize;
                messagesize = (fuz_rand(&randstate) & maxmessagesizemask) + 1;
                inext = (fuz_rand(&randstate) & 65535);
                if (dnext + messagesize > dbuffersize) dnext = 0;
            }
        }

        // long stream test ; warning : very long test !
        if (1)
        {
            xxh64_state_t crcorigstate;
            xxh64_state_t crcnewstate;
            const u64 totaltestsize = 6ull << 30;
            u64 totaltestdone = 0;
            size_t oldstart = 0;
            size_t oldsize  = 0;
            u32 segnb = 1;

            display("long hc streaming test (%u mb)\n", (u32)(totaltestsize >> 20));
            lz4_resetstreamhc(&shc, 0);

            xxh64_reset(&crcorigstate, 0);
            xxh64_reset(&crcnewstate, 0);

            while (totaltestdone < totaltestsize)
            {
                size_t testsize = (fuz_rand(&randstate) & 65535) + 1;
                size_t teststart = fuz_rand(&randstate) & 65535;

                fuz_displayupdate((u32)(totaltestdone >> 20));

                if (teststart == oldstart + oldsize)   // corner case not covered by this test (lz4_decompress_safe_usingdict() limitation)
                    teststart++;

                xxh64_update(&crcorigstate, testinput + teststart, testsize);
                crcorig = xxh64_digest(&crcorigstate);

                result = lz4_compresshc_limitedoutput_continue(&shc, testinput + teststart, testcompressed, (int)testsize, lz4_compressbound((int)testsize));
                fuz_checktest(result==0, "lz4_compresshc_limitedoutput_continue() dictionary compression failed : result = %i", result);

                result = lz4_decompress_safe_usingdict(testcompressed, testverify, result, (int)testsize, testinput + oldstart, (int)oldsize);
                fuz_checktest(result!=(int)testsize, "lz4_decompress_safe_usingdict() dictionary decompression part %u failed", segnb);

                xxh64_update(&crcnewstate, testverify, testsize);
                crcnew = xxh64_digest(&crcnewstate);
                if (crcorig!=crcnew)
                {
                    size_t c=0;
                    while (testverify[c] == testinput[teststart+c]) c++;
                    display("bad decompression at %u / %u \n", (u32)c, (u32)testsize);
                }
                fuz_checktest(crcorig!=crcnew, "lz4_decompress_safe_usingdict() part %u corruption", segnb);

                oldstart = teststart;
                oldsize = testsize;
                totaltestdone += testsize;

                segnb ++;
            }

            display("\r");
        }
    }

    printf("all unit tests completed successfully \n");
    return;
_output_error:
    exit(1);
}


static int fuz_usage(char* programname)
{
    display( "usage :\n");
    display( "      %s [args]\n", programname);
    display( "\n");
    display( "arguments :\n");
    display( " -i#    : nb of tests (default:%i) \n", nb_attempts);
    display( " -s#    : select seed (default:prompt user)\n");
    display( " -t#    : select starting test number (default:0)\n");
    display( " -p#    : select compressibility in %% (default:%i%%)\n", fuz_compressibility_default);
    display( " -v     : verbose\n");
    display( " -p     : pause at the end\n");
    display( " -h     : display help and exit\n");
    return 0;
}


int main(int argc, char** argv)
{
    u32 seed=0;
    int seedset=0;
    int argnb;
    int nbtests = nb_attempts;
    int testnb = 0;
    int proba = fuz_compressibility_default;
    int pause = 0;
    char* programname = argv[0];

    // check command line
    for(argnb=1; argnb<argc; argnb++)
    {
        char* argument = argv[argnb];

        if(!argument) continue;   // protection if argument empty

        // decode command (note : aggregated commands are allowed)
        if (argument[0]=='-')
        {
            if (!strcmp(argument, "--no-prompt")) { pause=0; seedset=1; g_displaylevel=1; continue; }
            argument++;

            while (*argument!=0)
            {
                switch(*argument)
                {
                case 'h':   /* display help */
                    return fuz_usage(programname);

                case 'v':   /* verbose mode */
                    argument++;
                    g_displaylevel=4;
                    break;

                case 'p':   /* pause at the end */
                    argument++;
                    pause=1;
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
                    seed=0; seedset=1;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        seed *= 10;
                        seed += *argument - '0';
                        argument++;
                    }
                    break;

                case 't':   /* select starting test nb */
                    argument++;
                    testnb=0;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        testnb *= 10;
                        testnb += *argument - '0';
                        argument++;
                    }
                    break;

                case 'p':  /* change probability */
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
                default: ;
                }
            }
        }
    }

    // get seed
    printf("starting lz4 fuzzer (%i-bits, %s)\n", (int)(sizeof(size_t)*8), lz4_version);

    if (!seedset) seed = fuz_getmillistart() % 10000;
    printf("seed = %u\n", seed);
    if (proba!=fuz_compressibility_default) printf("compressibility : %i%%\n", proba);

    if ((seedset==0) && (testnb==0)) fuz_unittests();

    if (nbtests<=0) nbtests=1;

    {
        int result = fuz_test(seed, nbtests, testnb, ((double)proba) / 100);
        if (pause)
        {
            display("press enter ... \n");
            getchar();
        }
        return result;
    }
}
