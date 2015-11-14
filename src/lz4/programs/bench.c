/*
    bench.c - demo program to benchmark open-source compression algorithm
    copyright (c) yann collet 2012-2014
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
    - lz4 homepage : http://fastcompression.blogspot.com/p/lz4.html
    - lz4 source repository : http://code.google.com/p/lz4/
*/

/**************************************
*  compiler options
***************************************/
/* disable some visual warning messages */
#define _crt_secure_no_warnings
#define _crt_secure_no_deprecate     /* vs2005 */

/* unix large files support (>4gb) */
#define _file_offset_bits 64
#if (defined(__sun__) && (!defined(__lp64__)))   /* sun solaris 32-bits requires specific definitions */
#  define _largefile_source
#elif ! defined(__lp64__)                        /* no point defining large file for 64 bit */
#  define _largefile64_source
#endif

/* s_isreg & gettimeofday() are not supported by msvc */
#if defined(_msc_ver) || defined(_win32)
#  define bmk_legacy_timer 1
#endif


/**************************************
*  includes
***************************************/
#include <stdlib.h>      /* malloc */
#include <stdio.h>       /* fprintf, fopen, ftello64 */
#include <sys/types.h>   /* stat64 */
#include <sys/stat.h>    /* stat64 */

/* use ftime() if gettimeofday() is not available on your target */
#if defined(bmk_legacy_timer)
#  include <sys/timeb.h>   /* timeb, ftime */
#else
#  include <sys/time.h>    /* gettimeofday */
#endif

#include "lz4.h"
#define compressor0 lz4_compress_local
static int lz4_compress_local(const char* src, char* dst, int size, int clevel) { (void)clevel; return lz4_compress(src, dst, size); }
#include "lz4hc.h"
#define compressor1 lz4_compresshc2
#define defaultcompressor compressor0

#include "xxhash.h"


/**************************************
*  compiler specifics
***************************************/
#if !defined(s_isreg)
#  define s_isreg(x) (((x) & s_ifmt) == s_ifreg)
#endif


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
  typedef unsigned char       byte;
  typedef unsigned short      u16;
  typedef unsigned int        u32;
  typedef   signed int        s32;
  typedef unsigned long long  u64;
#endif


/**************************************
*  constants
***************************************/
#define nbloops    3
#define timeloop   2000

#define kb *(1 <<10)
#define mb *(1 <<20)
#define gb *(1u<<30)

#define max_mem             (2 gb - 64 mb)
#define default_chunksize   (4 mb)


/**************************************
*  local structures
***************************************/
struct chunkparameters
{
    u32   id;
    char* origbuffer;
    char* compressedbuffer;
    int   origsize;
    int   compressedsize;
};

struct compressionparameters
{
    int (*compressionfunction)(const char*, char*, int, int);
    int (*decompressionfunction)(const char*, char*, int);
};


/**************************************
*  macro
***************************************/
#define display(...) fprintf(stderr, __va_args__)


/**************************************
*  benchmark parameters
***************************************/
static int chunksize = default_chunksize;
static int nbiterations = nbloops;
static int bmk_pause = 0;

void bmk_setblocksize(int bsize) { chunksize = bsize; }

void bmk_setnbiterations(int nbloops)
{
    nbiterations = nbloops;
    display("- %i iterations -\n", nbiterations);
}

void bmk_setpause(void) { bmk_pause = 1; }


/*********************************************************
*  private functions
**********************************************************/

#if defined(bmk_legacy_timer)

static int bmk_getmillistart(void)
{
  /* based on legacy ftime()
     rolls over every ~ 12.1 days (0x100000/24/60/60)
     use getmillispan to correct for rollover */
  struct timeb tb;
  int ncount;
  ftime( &tb );
  ncount = (int) (tb.millitm + (tb.time & 0xfffff) * 1000);
  return ncount;
}

#else

static int bmk_getmillistart(void)
{
  /* based on newer gettimeofday()
     use getmillispan to correct for rollover */
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
    size_t step = 64 mb;
    byte* testmem=null;

    requiredmem = (((requiredmem >> 26) + 1) << 26);
    requiredmem += 2*step;
    if (requiredmem > max_mem) requiredmem = max_mem;

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
    if (r || !s_isreg(statbuf.st_mode)) return 0;   /* no good... */
    return (u64)statbuf.st_size;
}


/*********************************************************
*  public function
**********************************************************/

int bmk_benchfile(char** filenamestable, int nbfiles, int clevel)
{
  int fileidx=0;
  char* orig_buff;
  struct compressionparameters compp;
  int cfunctionid;

  u64 totals = 0;
  u64 totalz = 0;
  double totalc = 0.;
  double totald = 0.;


  /* init */
  if (clevel <= 3) cfunctionid = 0; else cfunctionid = 1;
  switch (cfunctionid)
  {
#ifdef compressor0
  case 0 : compp.compressionfunction = compressor0; break;
#endif
#ifdef compressor1
  case 1 : compp.compressionfunction = compressor1; break;
#endif
  default : compp.compressionfunction = defaultcompressor;
  }
  compp.decompressionfunction = lz4_decompress_fast;

  /* loop for each file */
  while (fileidx<nbfiles)
  {
      file*  infile;
      char*  infilename;
      u64    infilesize;
      size_t benchedsize;
      int nbchunks;
      int maxcompressedchunksize;
      size_t readsize;
      char* compressedbuffer; int compressedbuffsize;
      struct chunkparameters* chunkp;
      u32 crcorig;

      /* check file existence */
      infilename = filenamestable[fileidx++];
      infile = fopen( infilename, "rb" );
      if (infile==null)
      {
        display( "pb opening %s\n", infilename);
        return 11;
      }

      /* memory allocation & restrictions */
      infilesize = bmk_getfilesize(infilename);
      benchedsize = (size_t) bmk_findmaxmem(infilesize * 2) / 2;
      if ((u64)benchedsize > infilesize) benchedsize = (size_t)infilesize;
      if (benchedsize < infilesize)
      {
          display("not enough memory for '%s' full size; testing %i mb only...\n", infilename, (int)(benchedsize>>20));
      }

      /* alloc */
      chunkp = (struct chunkparameters*) malloc(((benchedsize / (size_t)chunksize)+1) * sizeof(struct chunkparameters));
      orig_buff = (char*)malloc((size_t )benchedsize);
      nbchunks = (int) ((int)benchedsize / chunksize) + 1;
      maxcompressedchunksize = lz4_compressbound(chunksize);
      compressedbuffsize = nbchunks * maxcompressedchunksize;
      compressedbuffer = (char*)malloc((size_t )compressedbuffsize);


      if (!orig_buff || !compressedbuffer)
      {
        display("\nerror: not enough memory!\n");
        free(orig_buff);
        free(compressedbuffer);
        free(chunkp);
        fclose(infile);
        return 12;
      }

      /* init chunks data */
      {
          int i;
          size_t remaining = benchedsize;
          char* in = orig_buff;
          char* out = compressedbuffer;
          for (i=0; i<nbchunks; i++)
          {
              chunkp[i].id = i;
              chunkp[i].origbuffer = in; in += chunksize;
              if ((int)remaining > chunksize) { chunkp[i].origsize = chunksize; remaining -= chunksize; } else { chunkp[i].origsize = (int)remaining; remaining = 0; }
              chunkp[i].compressedbuffer = out; out += maxcompressedchunksize;
              chunkp[i].compressedsize = 0;
          }
      }

      /* fill input buffer */
      display("loading %s...       \r", infilename);
      readsize = fread(orig_buff, 1, benchedsize, infile);
      fclose(infile);

      if (readsize != benchedsize)
      {
        display("\nerror: problem reading file '%s' !!    \n", infilename);
        free(orig_buff);
        free(compressedbuffer);
        free(chunkp);
        return 13;
      }

      /* calculating input checksum */
      crcorig = xxh32(orig_buff, (unsigned int)benchedsize,0);


      /* bench */
      {
        int loopnb, chunknb;
        size_t csize=0;
        double fastestc = 100000000., fastestd = 100000000.;
        double ratio=0.;
        u32 crccheck=0;

        display("\r%79s\r", "");
        for (loopnb = 1; loopnb <= nbiterations; loopnb++)
        {
          int nbloops;
          int millitime;

          /* compression */
          display("%1i-%-14.14s : %9i ->\r", loopnb, infilename, (int)benchedsize);
          { size_t i; for (i=0; i<benchedsize; i++) compressedbuffer[i]=(char)i; }     /* warmimg up memory */

          nbloops = 0;
          millitime = bmk_getmillistart();
          while(bmk_getmillistart() == millitime);
          millitime = bmk_getmillistart();
          while(bmk_getmillispan(millitime) < timeloop)
          {
            for (chunknb=0; chunknb<nbchunks; chunknb++)
                chunkp[chunknb].compressedsize = compp.compressionfunction(chunkp[chunknb].origbuffer, chunkp[chunknb].compressedbuffer, chunkp[chunknb].origsize, clevel);
            nbloops++;
          }
          millitime = bmk_getmillispan(millitime);

          if ((double)millitime < fastestc*nbloops) fastestc = (double)millitime/nbloops;
          csize=0; for (chunknb=0; chunknb<nbchunks; chunknb++) csize += chunkp[chunknb].compressedsize;
          ratio = (double)csize/(double)benchedsize*100.;

          display("%1i-%-14.14s : %9i -> %9i (%5.2f%%),%7.1f mb/s\r", loopnb, infilename, (int)benchedsize, (int)csize, ratio, (double)benchedsize / fastestc / 1000.);

          /* decompression */
          { size_t i; for (i=0; i<benchedsize; i++) orig_buff[i]=0; }     /* zeroing area, for crc checking */

          nbloops = 0;
          millitime = bmk_getmillistart();
          while(bmk_getmillistart() == millitime);
          millitime = bmk_getmillistart();
          while(bmk_getmillispan(millitime) < timeloop)
          {
            for (chunknb=0; chunknb<nbchunks; chunknb++)
                chunkp[chunknb].compressedsize = lz4_decompress_fast(chunkp[chunknb].compressedbuffer, chunkp[chunknb].origbuffer, chunkp[chunknb].origsize);
            nbloops++;
          }
          millitime = bmk_getmillispan(millitime);

          if ((double)millitime < fastestd*nbloops) fastestd = (double)millitime/nbloops;
          display("%1i-%-14.14s : %9i -> %9i (%5.2f%%),%7.1f mb/s ,%7.1f mb/s\r", loopnb, infilename, (int)benchedsize, (int)csize, ratio, (double)benchedsize / fastestc / 1000., (double)benchedsize / fastestd / 1000.);

          /* crc checking */
          crccheck = xxh32(orig_buff, (unsigned int)benchedsize,0);
          if (crcorig!=crccheck) { display("\n!!! warning !!! %14s : invalid checksum : %x != %x\n", infilename, (unsigned)crcorig, (unsigned)crccheck); break; }
        }

        if (crcorig==crccheck)
        {
            if (ratio<100.)
                display("%-16.16s : %9i -> %9i (%5.2f%%),%7.1f mb/s ,%7.1f mb/s\n", infilename, (int)benchedsize, (int)csize, ratio, (double)benchedsize / fastestc / 1000., (double)benchedsize / fastestd / 1000.);
            else
                display("%-16.16s : %9i -> %9i (%5.1f%%),%7.1f mb/s ,%7.1f mb/s \n", infilename, (int)benchedsize, (int)csize, ratio, (double)benchedsize / fastestc / 1000., (double)benchedsize / fastestd / 1000.);
        }
        totals += benchedsize;
        totalz += csize;
        totalc += fastestc;
        totald += fastestd;
      }

      free(orig_buff);
      free(compressedbuffer);
      free(chunkp);
  }

  if (nbfiles > 1)
        display("%-16.16s :%10llu ->%10llu (%5.2f%%), %6.1f mb/s , %6.1f mb/s\n", "  total", (long long unsigned int)totals, (long long unsigned int)totalz, (double)totalz/(double)totals*100., (double)totals/totalc/1000., (double)totals/totald/1000.);

  if (bmk_pause) { display("\npress enter...\n"); getchar(); }

  return 0;
}



