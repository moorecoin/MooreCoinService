/*
    bench.c - demo program to benchmark open-source compression algorithm
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

//**************************************
// compiler options
//**************************************
// disable some visual warning messages
#define _crt_secure_no_warnings
#define _crt_secure_no_deprecate     // vs2005

// unix large files support (>4gb)
#if (defined(__sun__) && (!defined(__lp64__)))   // sun solaris 32-bits requires specific definitions
#  define _largefile_source
#  define _file_offset_bits 64
#elif ! defined(__lp64__)                        // no point defining large file for 64 bit
#  define _largefile64_source
#endif

// s_isreg & gettimeofday() are not supported by msvc
#if defined(_msc_ver) || defined(_win32)
#  define bmk_legacy_timer 1
#endif


//**************************************
// includes
//**************************************
#include <stdlib.h>      // malloc
#include <stdio.h>       // fprintf, fopen, ftello64
#include <sys/types.h>   // stat64
#include <sys/stat.h>    // stat64
#include <string.h>      // strcmp

// use ftime() if gettimeofday() is not available on your target
#if defined(bmk_legacy_timer)
#  include <sys/timeb.h>   // timeb, ftime
#else
#  include <sys/time.h>    // gettimeofday
#endif

#include "lz4.h"
#include "lz4hc.h"
#include "lz4frame.h"

#include "xxhash.h"


//**************************************
// compiler options
//**************************************
// s_isreg & gettimeofday() are not supported by msvc
#if !defined(s_isreg)
#  define s_isreg(x) (((x) & s_ifmt) == s_ifreg)
#endif

// gcc does not support _rotl outside of windows
#if !defined(_win32)
#  define _rotl(x,r) ((x << r) | (x >> (32 - r)))
#endif


//**************************************
// basic types
//**************************************
#if defined (__stdc_version__) && __stdc_version__ >= 199901l   // c99
# include <stdint.h>
  typedef uint8_t  byte;
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


//****************************
// constants
//****************************
#define program_description "lz4 speed analyzer"
#ifndef lz4_version
#  define lz4_version ""
#endif
#define author "yann collet"
#define welcome_message "*** %s %s %i-bits, by %s (%s) ***\n", program_description, lz4_version, (int)(sizeof(void*)*8), author, __date__

#define nbloops    6
#define timeloop   2500

#define knuth      2654435761u
#define max_mem    (1984<<20)
#define default_chunksize   (4<<20)

#define all_compressors 0
#define all_decompressors 0


//**************************************
// local structures
//**************************************
struct chunkparameters
{
    u32   id;
    char* origbuffer;
    char* compressedbuffer;
    int   origsize;
    int   compressedsize;
};


//**************************************
// macro
//**************************************
#define display(...) fprintf(stderr, __va_args__)
#define progress(...) no_prompt ? 0 : display(__va_args__)



//**************************************
// benchmark parameters
//**************************************
static int chunksize = default_chunksize;
static int nbiterations = nbloops;
static int bmk_pause = 0;
static int compressiontest = 1;
static int decompressiontest = 1;
static int compressionalgo = all_compressors;
static int decompressionalgo = all_decompressors;
static int no_prompt = 0;

void bmk_setblocksize(int bsize)
{
    chunksize = bsize;
    display("-using block size of %i kb-\n", chunksize>>10);
}

void bmk_setnbiterations(int nbloops)
{
    nbiterations = nbloops;
    display("- %i iterations -\n", nbiterations);
}

void bmk_setpause(void)
{
    bmk_pause = 1;
}

//*********************************************************
//  private functions
//*********************************************************

#if defined(bmk_legacy_timer)

static int bmk_getmillistart(void)
{
  // based on legacy ftime()
  // rolls over every ~ 12.1 days (0x100000/24/60/60)
  // use getmillispan to correct for rollover
  struct timeb tb;
  int ncount;
  ftime( &tb );
  ncount = (int) (tb.millitm + (tb.time & 0xfffff) * 1000);
  return ncount;
}

#else

static int bmk_getmillistart(void)
{
  // based on newer gettimeofday()
  // use getmillispan to correct for rollover
  struct timeval tv;
  int ncount;
  gettimeofday(&tv, null);
  ncount = (int) (tv.tv_usec/1000 + (tv.tv_sec & 0xfffff) * 1000);
  return ncount;
}

#endif


static int bmk_getmillispan( int ntimestart )
{
  int nspan = bmk_getmillistart() - ntimestart;
  if ( nspan < 0 )
    nspan += 0x100000 * 1000;
  return nspan;
}


static size_t bmk_findmaxmem(u64 requiredmem)
{
    size_t step = (64u<<20);   // 64 mb
    byte* testmem=null;

    requiredmem = (((requiredmem >> 25) + 1) << 26);
    if (requiredmem > max_mem) requiredmem = max_mem;

    requiredmem += 2*step;
    while (!testmem)
    {
        requiredmem -= step;
        testmem = (byte*) malloc ((size_t)requiredmem);
    }

    free (testmem);
    return (size_t) (requiredmem - step);
}


static u64 bmk_getfilesize(char* infilename)
{
    int r;
#if defined(_msc_ver)
    struct _stat64 statbuf;
    r = _stat64(infilename, &statbuf);
#else
    struct stat statbuf;
    r = stat(infilename, &statbuf);
#endif
    if (r || !s_isreg(statbuf.st_mode)) return 0;   // no good...
    return (u64)statbuf.st_size;
}


/*********************************************************
  benchmark function
*********************************************************/

static int local_lz4_compress_limitedoutput(const char* in, char* out, int insize)
{
    return lz4_compress_limitedoutput(in, out, insize, lz4_compressbound(insize));
}

static void* statelz4;
static int local_lz4_compress_withstate(const char* in, char* out, int insize)
{
    return lz4_compress_withstate(statelz4, in, out, insize);
}

static int local_lz4_compress_limitedoutput_withstate(const char* in, char* out, int insize)
{
    return lz4_compress_limitedoutput_withstate(statelz4, in, out, insize, lz4_compressbound(insize));
}

static lz4_stream_t* ctx;
static int local_lz4_compress_continue(const char* in, char* out, int insize)
{
    return lz4_compress_continue(ctx, in, out, insize);
}

static int local_lz4_compress_limitedoutput_continue(const char* in, char* out, int insize)
{
    return lz4_compress_limitedoutput_continue(ctx, in, out, insize, lz4_compressbound(insize));
}


lz4_stream_t lz4_dict;
static void* local_lz4_resetdictt(const char* fake)
{
    (void)fake;
    memset(&lz4_dict, 0, sizeof(lz4_stream_t));
    return null;
}

int lz4_compress_forceextdict (lz4_stream_t* lz4_dict, const char* source, char* dest, int inputsize);
static int local_lz4_compress_forcedict(const char* in, char* out, int insize)
{
    return lz4_compress_forceextdict(&lz4_dict, in, out, insize);
}


static void* statelz4hc;
static int local_lz4_compresshc_withstatehc(const char* in, char* out, int insize)
{
    return lz4_compresshc_withstatehc(statelz4hc, in, out, insize);
}

static int local_lz4_compresshc_limitedoutput_withstatehc(const char* in, char* out, int insize)
{
    return lz4_compresshc_limitedoutput_withstatehc(statelz4hc, in, out, insize, lz4_compressbound(insize));
}

static int local_lz4_compresshc_limitedoutput(const char* in, char* out, int insize)
{
    return lz4_compresshc_limitedoutput(in, out, insize, lz4_compressbound(insize));
}

static int local_lz4_compresshc_continue(const char* in, char* out, int insize)
{
    return lz4_compresshc_continue((lz4_streamhc_t*)ctx, in, out, insize);
}

static int local_lz4_compresshc_limitedoutput_continue(const char* in, char* out, int insize)
{
    return lz4_compresshc_limitedoutput_continue((lz4_streamhc_t*)ctx, in, out, insize, lz4_compressbound(insize));
}

static int local_lz4f_compressframe(const char* in, char* out, int insize)
{
    return (int)lz4f_compressframe(out, 2*insize + 16, in, insize, null);
}

static int local_lz4_savedict(const char* in, char* out, int insize)
{
    (void)in;
    return lz4_savedict(&lz4_dict, out, insize);
}

lz4_streamhc_t lz4_dicthc;
static int local_lz4_savedicthc(const char* in, char* out, int insize)
{
    (void)in;
    return lz4_savedicthc(&lz4_dicthc, out, insize);
}


static int local_lz4_decompress_fast(const char* in, char* out, int insize, int outsize)
{
    (void)insize;
    lz4_decompress_fast(in, out, outsize);
    return outsize;
}

static int local_lz4_decompress_fast_withprefix64k(const char* in, char* out, int insize, int outsize)
{
    (void)insize;
    lz4_decompress_fast_withprefix64k(in, out, outsize);
    return outsize;
}

static int local_lz4_decompress_fast_usingdict(const char* in, char* out, int insize, int outsize)
{
    (void)insize;
    lz4_decompress_fast_usingdict(in, out, outsize, out - 65536, 65536);
    return outsize;
}

static int local_lz4_decompress_safe_usingdict(const char* in, char* out, int insize, int outsize)
{
    (void)insize;
    lz4_decompress_safe_usingdict(in, out, insize, outsize, out - 65536, 65536);
    return outsize;
}

extern int lz4_decompress_safe_forceextdict(const char* in, char* out, int insize, int outsize, const char* dict, int dictsize);

static int local_lz4_decompress_safe_forceextdict(const char* in, char* out, int insize, int outsize)
{
    (void)insize;
    lz4_decompress_safe_forceextdict(in, out, insize, outsize, out - 65536, 65536);
    return outsize;
}

static int local_lz4_decompress_safe_partial(const char* in, char* out, int insize, int outsize)
{
    return lz4_decompress_safe_partial(in, out, insize, outsize - 5, outsize);
}

static lz4f_decompressioncontext_t g_dctx;

static int local_lz4f_decompress(const char* in, char* out, int insize, int outsize)
{
    size_t srcsize = insize;
    size_t dstsize = outsize;
    size_t result;
    result = lz4f_decompress(g_dctx, out, &dstsize, in, &srcsize, null);
    if (result!=0) { display("error decompressing frame : unfinished frame\n"); exit(8); }
    if (srcsize != (size_t)insize) { display("error decompressing frame : read size incorrect\n"); exit(9); }
    return (int)dstsize;
}


int fullspeedbench(char** filenamestable, int nbfiles)
{
  int fileidx=0;
  char* orig_buff;
# define nb_compression_algorithms 16
  double totalctime[nb_compression_algorithms+1] = {0};
  double totalcsize[nb_compression_algorithms+1] = {0};
# define nb_decompression_algorithms 9
  double totaldtime[nb_decompression_algorithms+1] = {0};
  size_t errorcode;

  errorcode = lz4f_createdecompressioncontext(&g_dctx, lz4f_version);
  if (lz4f_iserror(errorcode))
  {
     display("dctx allocation issue \n");
     return 10;
  }

  // loop for each file
  while (fileidx<nbfiles)
  {
      file* infile;
      char* infilename;
      u64   infilesize;
      size_t benchedsize;
      int nbchunks;
      int maxcompressedchunksize;
      struct chunkparameters* chunkp;
      size_t readsize;
      char* compressed_buff; int compressedbuffsize;
      u32 crcoriginal;


      // init
      statelz4   = lz4_createstream();
      statelz4hc = lz4_createstreamhc();

      // check file existence
      infilename = filenamestable[fileidx++];
      infile = fopen( infilename, "rb" );
      if (infile==null)
      {
        display( "pb opening %s\n", infilename);
        return 11;
      }

      // memory allocation & restrictions
      infilesize = bmk_getfilesize(infilename);
      benchedsize = (size_t) bmk_findmaxmem(infilesize) / 2;
      if ((u64)benchedsize > infilesize) benchedsize = (size_t)infilesize;
      if (benchedsize < infilesize)
      {
          display("not enough memory for '%s' full size; testing %i mb only...\n", infilename, (int)(benchedsize>>20));
      }

      // alloc
      chunkp = (struct chunkparameters*) malloc(((benchedsize / (size_t)chunksize)+1) * sizeof(struct chunkparameters));
      orig_buff = (char*) malloc((size_t)benchedsize);
      nbchunks = (int) (((int)benchedsize + (chunksize-1))/ chunksize);
      maxcompressedchunksize = lz4_compressbound(chunksize);
      compressedbuffsize = nbchunks * maxcompressedchunksize;
      compressed_buff = (char*)malloc((size_t)compressedbuffsize);


      if(!orig_buff || !compressed_buff)
      {
        display("\nerror: not enough memory!\n");
        free(orig_buff);
        free(compressed_buff);
        free(chunkp);
        fclose(infile);
        return 12;
      }

      // fill input buffer
      display("loading %s...       \r", infilename);
      readsize = fread(orig_buff, 1, benchedsize, infile);
      fclose(infile);

      if(readsize != benchedsize)
      {
        display("\nerror: problem reading file '%s' !!    \n", infilename);
        free(orig_buff);
        free(compressed_buff);
        free(chunkp);
        return 13;
      }

      // calculating input checksum
      crcoriginal = xxh32(orig_buff, (unsigned int)benchedsize,0);


      // bench
      {
        int loopnb, nb_loops, chunknb, calgnb, dalgnb;
        size_t csize=0;
        double ratio=0.;

        display("\r%79s\r", "");
        display(" %s : \n", infilename);

        // compression algorithms
        for (calgnb=1; (calgnb <= nb_compression_algorithms) && (compressiontest); calgnb++)
        {
            const char* compressorname;
            int (*compressionfunction)(const char*, char*, int);
            void* (*initfunction)(const char*) = null;
            double besttime = 100000000.;

            // init data chunks
            {
              int i;
              size_t remaining = benchedsize;
              char* in = orig_buff;
              char* out = compressed_buff;
                nbchunks = (int) (((int)benchedsize + (chunksize-1))/ chunksize);
              for (i=0; i<nbchunks; i++)
              {
                  chunkp[i].id = i;
                  chunkp[i].origbuffer = in; in += chunksize;
                  if ((int)remaining > chunksize) { chunkp[i].origsize = chunksize; remaining -= chunksize; } else { chunkp[i].origsize = (int)remaining; remaining = 0; }
                  chunkp[i].compressedbuffer = out; out += maxcompressedchunksize;
                  chunkp[i].compressedsize = 0;
              }
            }

            if ((compressionalgo != all_compressors) && (compressionalgo != calgnb)) continue;

            switch(calgnb)
            {
            case 1 : compressionfunction = lz4_compress; compressorname = "lz4_compress"; break;
            case 2 : compressionfunction = local_lz4_compress_limitedoutput; compressorname = "lz4_compress_limitedoutput"; break;
            case 3 : compressionfunction = local_lz4_compress_withstate; compressorname = "lz4_compress_withstate"; break;
            case 4 : compressionfunction = local_lz4_compress_limitedoutput_withstate; compressorname = "lz4_compress_limitedoutput_withstate"; break;
            case 5 : compressionfunction = local_lz4_compress_continue; initfunction = lz4_create; compressorname = "lz4_compress_continue"; break;
            case 6 : compressionfunction = local_lz4_compress_limitedoutput_continue; initfunction = lz4_create; compressorname = "lz4_compress_limitedoutput_continue"; break;
            case 7 : compressionfunction = lz4_compresshc; compressorname = "lz4_compresshc"; break;
            case 8 : compressionfunction = local_lz4_compresshc_limitedoutput; compressorname = "lz4_compresshc_limitedoutput"; break;
            case 9 : compressionfunction = local_lz4_compresshc_withstatehc; compressorname = "lz4_compresshc_withstatehc"; break;
            case 10: compressionfunction = local_lz4_compresshc_limitedoutput_withstatehc; compressorname = "lz4_compresshc_limitedoutput_withstatehc"; break;
            case 11: compressionfunction = local_lz4_compresshc_continue; initfunction = lz4_createhc; compressorname = "lz4_compresshc_continue"; break;
            case 12: compressionfunction = local_lz4_compresshc_limitedoutput_continue; initfunction = lz4_createhc; compressorname = "lz4_compresshc_limitedoutput_continue"; break;
            case 13: compressionfunction = local_lz4_compress_forcedict; initfunction = local_lz4_resetdictt; compressorname = "lz4_compress_forcedict"; break;
            case 14: compressionfunction = local_lz4f_compressframe; compressorname = "lz4f_compressframe";
                        chunkp[0].origsize = (int)benchedsize; nbchunks=1;
                        break;
            case 15: compressionfunction = local_lz4_savedict; compressorname = "lz4_savedict";
                        lz4_loaddict(&lz4_dict, chunkp[0].origbuffer, chunkp[0].origsize);
                        break;
            case 16: compressionfunction = local_lz4_savedicthc; compressorname = "lz4_savedicthc";
                        lz4_loaddicthc(&lz4_dicthc, chunkp[0].origbuffer, chunkp[0].origsize);
                        break;
            default : display("error ! bad algorithm id !! \n"); free(chunkp); return 1;
            }

            for (loopnb = 1; loopnb <= nbiterations; loopnb++)
            {
                double averagetime;
                int millitime;

                progress("%1i- %-28.28s :%9i ->\r", loopnb, compressorname, (int)benchedsize);
                { size_t i; for (i=0; i<benchedsize; i++) compressed_buff[i]=(char)i; }     // warming up memory

                nb_loops = 0;
                millitime = bmk_getmillistart();
                while(bmk_getmillistart() == millitime);
                millitime = bmk_getmillistart();
                while(bmk_getmillispan(millitime) < timeloop)
                {
                    if (initfunction!=null) ctx = initfunction(chunkp[0].origbuffer);
                    for (chunknb=0; chunknb<nbchunks; chunknb++)
                    {
                        chunkp[chunknb].compressedsize = compressionfunction(chunkp[chunknb].origbuffer, chunkp[chunknb].compressedbuffer, chunkp[chunknb].origsize);
                        if (chunkp[chunknb].compressedsize==0) display("error ! %s() = 0 !! \n", compressorname), exit(1);
                    }
                    if (initfunction!=null) free(ctx);
                    nb_loops++;
                }
                millitime = bmk_getmillispan(millitime);

                averagetime = (double)millitime / nb_loops;
                if (averagetime < besttime) besttime = averagetime;
                csize=0; for (chunknb=0; chunknb<nbchunks; chunknb++) csize += chunkp[chunknb].compressedsize;
                ratio = (double)csize/(double)benchedsize*100.;
                progress("%1i- %-28.28s :%9i ->%9i (%5.2f%%),%7.1f mb/s\r", loopnb, compressorname, (int)benchedsize, (int)csize, ratio, (double)benchedsize / besttime / 1000.);
            }

            if (ratio<100.)
                display("%2i-%-28.28s :%9i ->%9i (%5.2f%%),%7.1f mb/s\n", calgnb, compressorname, (int)benchedsize, (int)csize, ratio, (double)benchedsize / besttime / 1000.);
            else
                display("%2i-%-28.28s :%9i ->%9i (%5.1f%%),%7.1f mb/s\n", calgnb, compressorname, (int)benchedsize, (int)csize, ratio, (double)benchedsize / besttime / 1000.);

            totalctime[calgnb] += besttime;
            totalcsize[calgnb] += csize;
        }

        // prepare layout for decompression
        // init data chunks
        {
          int i;
          size_t remaining = benchedsize;
          char* in = orig_buff;
          char* out = compressed_buff;
            nbchunks = (int) (((int)benchedsize + (chunksize-1))/ chunksize);
          for (i=0; i<nbchunks; i++)
          {
              chunkp[i].id = i;
              chunkp[i].origbuffer = in; in += chunksize;
              if ((int)remaining > chunksize) { chunkp[i].origsize = chunksize; remaining -= chunksize; } else { chunkp[i].origsize = (int)remaining; remaining = 0; }
              chunkp[i].compressedbuffer = out; out += maxcompressedchunksize;
              chunkp[i].compressedsize = 0;
          }
        }
        for (chunknb=0; chunknb<nbchunks; chunknb++)
        {
            chunkp[chunknb].compressedsize = lz4_compress(chunkp[chunknb].origbuffer, chunkp[chunknb].compressedbuffer, chunkp[chunknb].origsize);
            if (chunkp[chunknb].compressedsize==0) display("error ! %s() = 0 !! \n", "lz4_compress"), exit(1);
        }

        // decompression algorithms
        for (dalgnb=1; (dalgnb <= nb_decompression_algorithms) && (decompressiontest); dalgnb++)
        {
            //const char* dname = decompressionnames[dalgnb];
            const char* dname;
            int (*decompressionfunction)(const char*, char*, int, int);
            double besttime = 100000000.;

            if ((decompressionalgo != all_decompressors) && (decompressionalgo != dalgnb)) continue;

            switch(dalgnb)
            {
            case 1: decompressionfunction = local_lz4_decompress_fast; dname = "lz4_decompress_fast"; break;
            case 2: decompressionfunction = local_lz4_decompress_fast_withprefix64k; dname = "lz4_decompress_fast_withprefix64k"; break;
            case 3: decompressionfunction = local_lz4_decompress_fast_usingdict; dname = "lz4_decompress_fast_usingdict"; break;
            case 4: decompressionfunction = lz4_decompress_safe; dname = "lz4_decompress_safe"; break;
            case 5: decompressionfunction = lz4_decompress_safe_withprefix64k; dname = "lz4_decompress_safe_withprefix64k"; break;
            case 6: decompressionfunction = local_lz4_decompress_safe_usingdict; dname = "lz4_decompress_safe_usingdict"; break;
            case 7: decompressionfunction = local_lz4_decompress_safe_partial; dname = "lz4_decompress_safe_partial"; break;
            case 8: decompressionfunction = local_lz4_decompress_safe_forceextdict; dname = "lz4_decompress_safe_forceextdict"; break;
            case 9: decompressionfunction = local_lz4f_decompress; dname = "lz4f_decompress";
                    errorcode = lz4f_compressframe(compressed_buff, compressedbuffsize, orig_buff, benchedsize, null);
                    if (lz4f_iserror(errorcode)) { display("preparation error compressing frame\n"); return 1; }
                    chunkp[0].origsize = (int)benchedsize;
                    chunkp[0].compressedsize = (int)errorcode;
                    nbchunks = 1;
                    break;
            default : display("error ! bad decompression algorithm id !! \n"); free(chunkp); return 1;
            }

            { size_t i; for (i=0; i<benchedsize; i++) orig_buff[i]=0; }     // zeroing source area, for crc checking

            for (loopnb = 1; loopnb <= nbiterations; loopnb++)
            {
                double averagetime;
                int millitime;
                u32 crcdecoded;

                progress("%1i- %-29.29s :%10i ->\r", loopnb, dname, (int)benchedsize);

                nb_loops = 0;
                millitime = bmk_getmillistart();
                while(bmk_getmillistart() == millitime);
                millitime = bmk_getmillistart();
                while(bmk_getmillispan(millitime) < timeloop)
                {
                    for (chunknb=0; chunknb<nbchunks; chunknb++)
                    {
                        int decodedsize = decompressionfunction(chunkp[chunknb].compressedbuffer, chunkp[chunknb].origbuffer, chunkp[chunknb].compressedsize, chunkp[chunknb].origsize);
                        if (chunkp[chunknb].origsize != decodedsize) display("error ! %s() == %i != %i !! \n", dname, decodedsize, chunkp[chunknb].origsize), exit(1);
                    }
                    nb_loops++;
                }
                millitime = bmk_getmillispan(millitime);

                averagetime = (double)millitime / nb_loops;
                if (averagetime < besttime) besttime = averagetime;

                progress("%1i- %-29.29s :%10i -> %7.1f mb/s\r", loopnb, dname, (int)benchedsize, (double)benchedsize / besttime / 1000.);

                // crc checking
                crcdecoded = xxh32(orig_buff, (int)benchedsize, 0);
                if (crcoriginal!=crcdecoded) { display("\n!!! warning !!! %14s : invalid checksum : %x != %x\n", infilename, (unsigned)crcoriginal, (unsigned)crcdecoded); exit(1); }
            }

            display("%2i-%-29.29s :%10i -> %7.1f mb/s\n", dalgnb, dname, (int)benchedsize, (double)benchedsize / besttime / 1000.);

            totaldtime[dalgnb] += besttime;
        }

      }

      free(orig_buff);
      free(compressed_buff);
      free(chunkp);
  }

  if (bmk_pause) { printf("press enter...\n"); getchar(); }

  return 0;
}


int usage(char* exename)
{
    display( "usage :\n");
    display( "      %s [arg] file1 file2 ... filex\n", exename);
    display( "arguments :\n");
    display( " -c     : compression tests only\n");
    display( " -d     : decompression tests only\n");
    display( " -h/-h  : help (this text + advanced options)\n");
    return 0;
}

int usage_advanced(void)
{
    display( "\nadvanced options :\n");
    display( " -c#    : test only compression function # [1-%i]\n", nb_compression_algorithms);
    display( " -d#    : test only decompression function # [1-%i]\n", nb_decompression_algorithms);
    display( " -i#    : iteration loops [1-9](default : %i)\n", nbloops);
    display( " -b#    : block size [4-7](default : 7)\n");
    return 0;
}

int badusage(char* exename)
{
    display("wrong parameters\n");
    usage(exename);
    return 0;
}

int main(int argc, char** argv)
{
    int i,
        filenamesstart=2;
    char* exename=argv[0];
    char* input_filename=0;

    // welcome message
    display(welcome_message);

    if (argc<2) { badusage(exename); return 1; }

    for(i=1; i<argc; i++)
    {
        char* argument = argv[i];

        if(!argument) continue;   // protection if argument empty
        if (!strcmp(argument, "--no-prompt"))
        {
            no_prompt = 1;
            continue;
        }

        // decode command (note : aggregated commands are allowed)
        if (argument[0]=='-')
        {
            while (argument[1]!=0)
            {
                argument ++;

                switch(argument[0])
                {
                    // select compression algorithm only
                case 'c':
                    decompressiontest = 0;
                    while ((argument[1]>= '0') && (argument[1]<= '9'))
                    {
                        compressionalgo *= 10;
                        compressionalgo += argument[1] - '0';
                        argument++;
                    }
                    break;

                    // select decompression algorithm only
                case 'd':
                    compressiontest = 0;
                    while ((argument[1]>= '0') && (argument[1]<= '9'))
                    {
                        decompressionalgo *= 10;
                        decompressionalgo += argument[1] - '0';
                        argument++;
                    }
                    break;

                    // display help on usage
                case 'h' :
                case 'h': usage(exename); usage_advanced(); return 0;

                    // modify block properties
                case 'b':
                    while (argument[1]!=0)
                    switch(argument[1])
                    {
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    {
                        int b = argument[1] - '0';
                        int s = 1 << (8 + 2*b);
                        bmk_setblocksize(s);
                        argument++;
                        break;
                    }
                    case 'd': argument++; break;
                    default : goto _exit_blockproperties;
                    }
_exit_blockproperties:
                    break;

                    // modify nb iterations
                case 'i':
                    if ((argument[1] >='1') && (argument[1] <='9'))
                    {
                        int iters = argument[1] - '0';
                        bmk_setnbiterations(iters);
                        argument++;
                    }
                    break;

                    // pause at the end (hidden option)
                case 'p': bmk_setpause(); break;

                    // unknown command
                default : badusage(exename); return 1;
                }
            }
            continue;
        }

        // first provided filename is input
        if (!input_filename) { input_filename=argument; filenamesstart=i; continue; }

    }

    // no input filename ==> error
    if(!input_filename) { badusage(exename); return 1; }

    return fullspeedbench(argv+filenamesstart, argc-filenamesstart);

}

