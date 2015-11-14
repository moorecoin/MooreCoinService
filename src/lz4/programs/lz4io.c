/*
  lz4io.c - lz4 file/stream interface
  copyright (c) yann collet 2011-2014
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
/*
  note : this is stand-alone program.
  it is not part of lz4 compression library, it is a user code of the lz4 library.
  - the license of lz4 library is bsd.
  - the license of xxhash library is bsd.
  - the license of this source file is gplv2.
*/

/**************************************
*  compiler options
***************************************/
#ifdef _msc_ver    /* visual studio */
#  define _crt_secure_no_warnings
#  define _crt_secure_no_deprecate     /* vs2005 */
#  pragma warning(disable : 4127)      /* disable: c4127: conditional expression is constant */
#endif

#define gcc_version (__gnuc__ * 100 + __gnuc_minor__)
#ifdef __gnuc__
#  pragma gcc diagnostic ignored "-wmissing-braces"   /* gcc bug 53119 : doesn't accept { 0 } as initializer (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119) */
#  pragma gcc diagnostic ignored "-wmissing-field-initializers"   /* gcc bug 53119 : doesn't accept { 0 } as initializer (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119) */
#endif

#define _large_files           /* large file support on 32-bits aix */
#define _file_offset_bits 64   /* large file support on 32-bits unix */
#define _posix_source 1        /* for fileno() within <stdio.h> on unix */


/****************************
*  includes
*****************************/
#include <stdio.h>    /* fprintf, fopen, fread, _fileno, stdin, stdout */
#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* strcmp, strlen */
#include <time.h>     /* clock */
#include "lz4io.h"
#include "lz4.h"      /* still required for legacy format */
#include "lz4hc.h"    /* still required for legacy format */
#include "lz4frame.h"


/****************************
*  os-specific includes
*****************************/
#if defined(msdos) || defined(os2) || defined(win32) || defined(_win32) || defined(__cygwin__)
#  include <fcntl.h>   /* _o_binary */
#  include <io.h>      /* _setmode, _isatty */
#  ifdef __mingw32__
   int _fileno(file *stream);   /* mingw somehow forgets to include this windows declaration into <stdio.h> */
#  endif
#  define set_binary_mode(file) _setmode(_fileno(file), _o_binary)
#  define is_console(stdstream) _isatty(_fileno(stdstream))
#else
#  include <unistd.h>  /* isatty */
#  define set_binary_mode(file)
#  define is_console(stdstream) isatty(fileno(stdstream))
#endif


/****************************
*  constants
*****************************/
#define kb *(1 <<10)
#define mb *(1 <<20)
#define gb *(1u<<30)

#define _1bit  0x01
#define _2bits 0x03
#define _3bits 0x07
#define _4bits 0x0f
#define _8bits 0xff

#define magicnumber_size   4
#define lz4s_magicnumber   0x184d2204
#define lz4s_skippable0    0x184d2a50
#define lz4s_skippablemask 0xfffffff0
#define legacy_magicnumber 0x184c2102

#define cacheline 64
#define legacy_blocksize   (8 mb)
#define min_stream_bufsize (192 kb)
#define lz4s_blocksizeid_default 7
#define lz4s_checksum_seed 0
#define lz4s_eos 0
#define lz4s_maxheadersize (magicnumber_size+2+8+4+1)


/**************************************
*  macros
***************************************/
#define display(...)         fprintf(stderr, __va_args__)
#define displaylevel(l, ...) if (displaylevel>=l) { display(__va_args__); }
#define displayupdate(l, ...) if (displaylevel>=l) { \
            if ((lz4io_getmillispan(g_time) > refreshrate) || (displaylevel>=4)) \
            { g_time = clock(); display(__va_args__); \
            if (displaylevel>=4) fflush(stdout); } }
static const unsigned refreshrate = 150;
static clock_t g_time = 0;


/**************************************
*  local parameters
***************************************/
static int displaylevel = 0;   /* 0 : no display  ; 1: errors  ; 2 : + result + interaction + warnings ; 3 : + progression; 4 : + information */
static int overwrite = 1;
static int globalblocksizeid = lz4s_blocksizeid_default;
static int blockchecksum = 0;
static int streamchecksum = 1;
static int blockindependence = 1;

static const int minblocksizeid = 4;
static const int maxblocksizeid = 7;


/**************************************
*  exceptions
***************************************/
#define debug 0
#define debugoutput(...) if (debug) display(__va_args__);
#define exm_throw(error, ...)                                             \
{                                                                         \
    debugoutput("error defined at %s, line %i : \n", __file__, __line__); \
    displaylevel(1, "error %i : ", error);                                \
    displaylevel(1, __va_args__);                                         \
    displaylevel(1, "\n");                                                \
    exit(error);                                                          \
}


/**************************************
*  version modifiers
***************************************/
#define extended_arguments
#define extended_help
#define extended_format
#define default_compressor   compress_file
#define default_decompressor decodelz4s


/* ************************************************** */
/* ****************** parameters ******************** */
/* ************************************************** */

/* default setting : overwrite = 1; return : overwrite mode (0/1) */
int lz4io_setoverwrite(int yes)
{
   overwrite = (yes!=0);
   return overwrite;
}

/* blocksizeid : valid values : 4-5-6-7 */
int lz4io_setblocksizeid(int bsid)
{
    static const int blocksizetable[] = { 64 kb, 256 kb, 1 mb, 4 mb };
    if ((bsid < minblocksizeid) || (bsid > maxblocksizeid)) return -1;
    globalblocksizeid = bsid;
    return blocksizetable[globalblocksizeid-minblocksizeid];
}

int lz4io_setblockmode(lz4io_blockmode_t blockmode)
{
    blockindependence = (blockmode == lz4io_blockindependent);
    return blockindependence;
}

/* default setting : no checksum */
int lz4io_setblockchecksummode(int xxhash)
{
    blockchecksum = (xxhash != 0);
    return blockchecksum;
}

/* default setting : checksum enabled */
int lz4io_setstreamchecksummode(int xxhash)
{
    streamchecksum = (xxhash != 0);
    return streamchecksum;
}

/* default setting : 0 (no notification) */
int lz4io_setnotificationlevel(int level)
{
    displaylevel = level;
    return displaylevel;
}

static unsigned lz4io_getmillispan(clock_t nprevious)
{
    clock_t ncurrent = clock();
    unsigned nspan = (unsigned)(((ncurrent - nprevious) * 1000) / clocks_per_sec);
    return nspan;
}


/* ************************************************************************ */
/* ********************** lz4 file / pipe compression ********************* */
/* ************************************************************************ */

static int          lz4s_getblocksize_fromblockid (int id) { return (1 << (8 + (2 * id))); }
static int          lz4s_isskippablemagicnumber(unsigned int magic) { return (magic & lz4s_skippablemask) == lz4s_skippable0; }


static int get_filehandle(char* input_filename, char* output_filename, file** pfinput, file** pfoutput)
{

    if (!strcmp (input_filename, stdinmark))
    {
        displaylevel(4,"using stdin for input\n");
        *pfinput = stdin;
        set_binary_mode(stdin);
    }
    else
    {
        *pfinput = fopen(input_filename, "rb");
    }

    if (!strcmp (output_filename, stdoutmark))
    {
        displaylevel(4,"using stdout for output\n");
        *pfoutput = stdout;
        set_binary_mode(stdout);
    }
    else
    {
        /* check if destination file already exists */
        *pfoutput=0;
        if (output_filename != nulmark) *pfoutput = fopen( output_filename, "rb" );
        if (*pfoutput!=0)
        {
            fclose(*pfoutput);
            if (!overwrite)
            {
                char ch;
                displaylevel(2, "warning : %s already exists\n", output_filename);
                displaylevel(2, "overwrite ? (y/n) : ");
                if (displaylevel <= 1) exm_throw(11, "operation aborted : %s already exists", output_filename);   /* no interaction possible */
                ch = (char)getchar();
                if ((ch!='y') && (ch!='y')) exm_throw(11, "operation aborted : %s already exists", output_filename);
            }
        }
        *pfoutput = fopen( output_filename, "wb" );
    }

    if ( *pfinput==0 ) exm_throw(12, "pb opening %s", input_filename);
    if ( *pfoutput==0) exm_throw(13, "pb opening %s", output_filename);

    return 0;
}




/***************************************
 *   legacy compression
 * *************************************/

/* unoptimized version; solves endianess & alignment issues */
static void lz4io_writele32 (void* p, unsigned value32)
{
    unsigned char* dstptr = p;
    dstptr[0] = (unsigned char)value32;
    dstptr[1] = (unsigned char)(value32 >> 8);
    dstptr[2] = (unsigned char)(value32 >> 16);
    dstptr[3] = (unsigned char)(value32 >> 24);
}

/* lz4io_compressfilename_legacy :
 * this function is intentionally "hidden" (not published in .h)
 * it generates compressed streams using the old 'legacy' format */
int lz4io_compressfilename_legacy(char* input_filename, char* output_filename, int compressionlevel)
{
    int (*compressionfunction)(const char*, char*, int);
    unsigned long long filesize = 0;
    unsigned long long compressedfilesize = magicnumber_size;
    char* in_buff;
    char* out_buff;
    file* finput;
    file* foutput;
    clock_t start, end;
    size_t sizecheck;


    /* init */
    start = clock();
    if (compressionlevel < 3) compressionfunction = lz4_compress; else compressionfunction = lz4_compresshc;

    get_filehandle(input_filename, output_filename, &finput, &foutput);
    if ((displaylevel==2) && (compressionlevel==1)) displaylevel=3;

    /* allocate memory */
    in_buff = (char*)malloc(legacy_blocksize);
    out_buff = (char*)malloc(lz4_compressbound(legacy_blocksize));
    if (!in_buff || !out_buff) exm_throw(21, "allocation error : not enough memory");

    /* write archive header */
    lz4io_writele32(out_buff, legacy_magicnumber);
    sizecheck = fwrite(out_buff, 1, magicnumber_size, foutput);
    if (sizecheck!=magicnumber_size) exm_throw(22, "write error : cannot write header");

    /* main loop */
    while (1)
    {
        unsigned int outsize;
        /* read block */
        int insize = (int) fread(in_buff, (size_t)1, (size_t)legacy_blocksize, finput);
        if( insize<=0 ) break;
        filesize += insize;

        /* compress block */
        outsize = compressionfunction(in_buff, out_buff+4, insize);
        compressedfilesize += outsize+4;
        displayupdate(3, "\rread : %i mb  ==> %.2f%%   ", (int)(filesize>>20), (double)compressedfilesize/filesize*100);

        /* write block */
        lz4io_writele32(out_buff, outsize);
        sizecheck = fwrite(out_buff, 1, outsize+4, foutput);
        if (sizecheck!=(size_t)(outsize+4)) exm_throw(23, "write error : cannot write compressed block");
    }

    /* status */
    end = clock();
    displaylevel(2, "\r%79s\r", "");
    displaylevel(2,"compressed %llu bytes into %llu bytes ==> %.2f%%\n",
        (unsigned long long) filesize, (unsigned long long) compressedfilesize, (double)compressedfilesize/filesize*100);
    {
        double seconds = (double)(end - start)/clocks_per_sec;
        displaylevel(4,"done in %.2f s ==> %.2f mb/s\n", seconds, (double)filesize / seconds / 1024 / 1024);
    }

    /* close & free */
    free(in_buff);
    free(out_buff);
    fclose(finput);
    fclose(foutput);

    return 0;
}


/***********************************************
 *   compression using frame format
 * ********************************************/

int lz4io_compressfilename(char* input_filename, char* output_filename, int compressionlevel)
{
    unsigned long long filesize = 0;
    unsigned long long compressedfilesize = 0;
    char* in_buff;
    char* out_buff;
    file* finput;
    file* foutput;
    clock_t start, end;
    int blocksize;
    size_t sizecheck, headersize, readsize, outbuffsize;
    lz4f_compressioncontext_t ctx;
    lz4f_errorcode_t errorcode;
    lz4f_preferences_t prefs = {0};


    /* init */
    start = clock();
    if ((displaylevel==2) && (compressionlevel>=3)) displaylevel=3;
    errorcode = lz4f_createcompressioncontext(&ctx, lz4f_version);
    if (lz4f_iserror(errorcode)) exm_throw(30, "allocation error : can't create lz4f context : %s", lz4f_geterrorname(errorcode));
    get_filehandle(input_filename, output_filename, &finput, &foutput);
    blocksize = lz4s_getblocksize_fromblockid (globalblocksizeid);

    /* set compression parameters */
    prefs.autoflush = 1;
    prefs.compressionlevel = compressionlevel;
    prefs.frameinfo.blockmode = blockindependence;
    prefs.frameinfo.blocksizeid = globalblocksizeid;
    prefs.frameinfo.contentchecksumflag = streamchecksum;

    /* allocate memory */
    in_buff  = (char*)malloc(blocksize);
    outbuffsize = lz4f_compressbound(blocksize, &prefs);
    out_buff = (char*)malloc(outbuffsize);
    if (!in_buff || !out_buff) exm_throw(31, "allocation error : not enough memory");

    /* write archive header */
    headersize = lz4f_compressbegin(ctx, out_buff, outbuffsize, &prefs);
    if (lz4f_iserror(headersize)) exm_throw(32, "file header generation failed : %s", lz4f_geterrorname(headersize));
    sizecheck = fwrite(out_buff, 1, headersize, foutput);
    if (sizecheck!=headersize) exm_throw(33, "write error : cannot write header");
    compressedfilesize += headersize;

    /* read first block */
    readsize = fread(in_buff, (size_t)1, (size_t)blocksize, finput);
    filesize += readsize;

    /* main loop */
    while (readsize>0)
    {
        size_t outsize;

        /* compress block */
        outsize = lz4f_compressupdate(ctx, out_buff, outbuffsize, in_buff, readsize, null);
        if (lz4f_iserror(outsize)) exm_throw(34, "compression failed : %s", lz4f_geterrorname(outsize));
        compressedfilesize += outsize;
        displayupdate(3, "\rread : %i mb   ==> %.2f%%   ", (int)(filesize>>20), (double)compressedfilesize/filesize*100);

        /* write block */
        sizecheck = fwrite(out_buff, 1, outsize, foutput);
        if (sizecheck!=outsize) exm_throw(35, "write error : cannot write compressed block");

        /* read next block */
        readsize = fread(in_buff, (size_t)1, (size_t)blocksize, finput);
        filesize += readsize;
    }

    /* end of stream mark */
    headersize = lz4f_compressend(ctx, out_buff, outbuffsize, null);
    if (lz4f_iserror(headersize)) exm_throw(36, "end of file generation failed : %s", lz4f_geterrorname(headersize));

    sizecheck = fwrite(out_buff, 1, headersize, foutput);
    if (sizecheck!=headersize) exm_throw(37, "write error : cannot write end of stream");
    compressedfilesize += headersize;

    /* close & free */
    free(in_buff);
    free(out_buff);
    fclose(finput);
    fclose(foutput);
    errorcode = lz4f_freecompressioncontext(ctx);
    if (lz4f_iserror(errorcode)) exm_throw(38, "error : can't free lz4f context resource : %s", lz4f_geterrorname(errorcode));

    /* final status */
    end = clock();
    displaylevel(2, "\r%79s\r", "");
    displaylevel(2, "compressed %llu bytes into %llu bytes ==> %.2f%%\n",
        (unsigned long long) filesize, (unsigned long long) compressedfilesize, (double)compressedfilesize/filesize*100);
    {
        double seconds = (double)(end - start)/clocks_per_sec;
        displaylevel(4, "done in %.2f s ==> %.2f mb/s\n", seconds, (double)filesize / seconds / 1024 / 1024);
    }

    return 0;
}


/* ********************************************************************* */
/* ********************** lz4 file / stream decoding ******************* */
/* ********************************************************************* */

static unsigned lz4io_readle32 (const void* s)
{
    const unsigned char* srcptr = s;
    unsigned value32 = srcptr[0];
    value32 += (srcptr[1]<<8);
    value32 += (srcptr[2]<<16);
    value32 += (srcptr[3]<<24);
    return value32;
}

static unsigned long long decodelegacystream(file* finput, file* foutput)
{
    unsigned long long filesize = 0;
    char* in_buff;
    char* out_buff;

    /* allocate memory */
    in_buff = (char*)malloc(lz4_compressbound(legacy_blocksize));
    out_buff = (char*)malloc(legacy_blocksize);
    if (!in_buff || !out_buff) exm_throw(51, "allocation error : not enough memory");

    /* main loop */
    while (1)
    {
        int decodesize;
        size_t sizecheck;
        unsigned int blocksize;

        /* block size */
        sizecheck = fread(in_buff, 1, 4, finput);
        if (sizecheck==0) break;                   /* nothing to read : file read is completed */
        blocksize = lz4io_readle32(in_buff);       /* convert to little endian */
        if (blocksize > lz4_compressbound(legacy_blocksize))
        {   /* cannot read next block : maybe new stream ? */
            fseek(finput, -4, seek_cur);
            break;
        }

        /* read block */
        sizecheck = fread(in_buff, 1, blocksize, finput);

        /* decode block */
        decodesize = lz4_decompress_safe(in_buff, out_buff, blocksize, legacy_blocksize);
        if (decodesize < 0) exm_throw(52, "decoding failed ! corrupted input detected !");
        filesize += decodesize;

        /* write block */
        sizecheck = fwrite(out_buff, 1, decodesize, foutput);
        if (sizecheck != (size_t)decodesize) exm_throw(53, "write error : cannot write decoded block into output\n");
    }

    /* free */
    free(in_buff);
    free(out_buff);

    return filesize;
}


static unsigned long long decodelz4s(file* finput, file* foutput)
{
    unsigned long long filesize = 0;
    char* inbuff;
    char* outbuff;
#   define headermax 20
    char  headerbuff[headermax];
    size_t sizecheck, nexttoread, outbuffsize, inbuffsize;
    lz4f_decompressioncontext_t ctx;
    lz4f_errorcode_t errorcode;
    lz4f_frameinfo_t frameinfo;

    /* init */
    errorcode = lz4f_createdecompressioncontext(&ctx, lz4f_version);
    if (lz4f_iserror(errorcode)) exm_throw(60, "allocation error : can't create context : %s", lz4f_geterrorname(errorcode));
    lz4io_writele32(headerbuff, lz4s_magicnumber);   /* regenerated here, as it was already read from finput */

    /* decode stream descriptor */
    outbuffsize = 0; inbuffsize = 0; sizecheck = magicnumber_size;
    nexttoread = lz4f_decompress(ctx, null, &outbuffsize, headerbuff, &sizecheck, null);
    if (lz4f_iserror(nexttoread)) exm_throw(61, "decompression error : %s", lz4f_geterrorname(nexttoread));
    if (nexttoread > headermax) exm_throw(62, "header too large (%i>%i)", (int)nexttoread, headermax);
    sizecheck = fread(headerbuff, 1, nexttoread, finput);
    if (sizecheck!=nexttoread) exm_throw(63, "read error ");
    nexttoread = lz4f_decompress(ctx, null, &outbuffsize, headerbuff, &sizecheck, null);
    errorcode = lz4f_getframeinfo(ctx, &frameinfo, null, &inbuffsize);
    if (lz4f_iserror(errorcode)) exm_throw(64, "can't decode frame header : %s", lz4f_geterrorname(errorcode));

    /* allocate memory */
    outbuffsize = lz4io_setblocksizeid(frameinfo.blocksizeid);
    inbuffsize = outbuffsize + 4;
    inbuff = (char*)malloc(inbuffsize);
    outbuff = (char*)malloc(outbuffsize);
    if (!inbuff || !outbuff) exm_throw(65, "allocation error : not enough memory");

    /* main loop */
    while (nexttoread != 0)
    {
        size_t decodedbytes = outbuffsize;

        /* read block */
        sizecheck = fread(inbuff, 1, nexttoread, finput);
        if (sizecheck!=nexttoread) exm_throw(66, "read error ");

        /* decode block */
        errorcode = lz4f_decompress(ctx, outbuff, &decodedbytes, inbuff, &sizecheck, null);
        if (lz4f_iserror(errorcode)) exm_throw(67, "decompression error : %s", lz4f_geterrorname(errorcode));
        if (sizecheck!=nexttoread) exm_throw(67, "synchronization error");
        nexttoread = errorcode;
        filesize += decodedbytes;

        /* write block */
        sizecheck = fwrite(outbuff, 1, decodedbytes, foutput);
        if (sizecheck != decodedbytes) exm_throw(68, "write error : cannot write decoded block\n");
    }

    /* free */
    free(inbuff);
    free(outbuff);
    errorcode = lz4f_freedecompressioncontext(ctx);
    if (lz4f_iserror(errorcode)) exm_throw(69, "error : can't free lz4f context resource : %s", lz4f_geterrorname(errorcode));

    return filesize;
}


#define endofstream ((unsigned long long)-1)
static unsigned long long selectdecoder( file* finput,  file* foutput)
{
    unsigned char u32store[magicnumber_size];
    unsigned magicnumber, size;
    int errornb;
    size_t nbreadbytes;

    /* check archive header */
    nbreadbytes = fread(u32store, 1, magicnumber_size, finput);
    if (nbreadbytes==0) return endofstream;                  /* eof */
    if (nbreadbytes != magicnumber_size) exm_throw(40, "unrecognized header : magic number unreadable");
    magicnumber = lz4io_readle32(u32store);   /* little endian format */
    if (lz4s_isskippablemagicnumber(magicnumber)) magicnumber = lz4s_skippable0;  /* fold skippable magic numbers */

    switch(magicnumber)
    {
    case lz4s_magicnumber:
        return default_decompressor(finput, foutput);
    case legacy_magicnumber:
        displaylevel(4, "detected : legacy format \n");
        return decodelegacystream(finput, foutput);
    case lz4s_skippable0:
        displaylevel(4, "skipping detected skippable area \n");
        nbreadbytes = fread(u32store, 1, 4, finput);
        if (nbreadbytes != 4) exm_throw(42, "stream error : skippable size unreadable");
        size = lz4io_readle32(u32store);     /* little endian format */
        errornb = fseek(finput, size, seek_cur);
        if (errornb != 0) exm_throw(43, "stream error : cannot skip skippable area");
        return selectdecoder(finput, foutput);
    extended_format;
    default:
        if (ftell(finput) == magicnumber_size) exm_throw(44,"unrecognized header : file cannot be decoded");   /* wrong magic number at the beginning of 1st stream */
        displaylevel(2, "stream followed by unrecognized data\n");
        return endofstream;
    }
}


int lz4io_decompressfilename(char* input_filename, char* output_filename)
{
    unsigned long long filesize = 0, decodedsize=0;
    file* finput;
    file* foutput;
    clock_t start, end;


    /* init */
    start = clock();
    get_filehandle(input_filename, output_filename, &finput, &foutput);

    /* loop over multiple streams */
    do
    {
        decodedsize = selectdecoder(finput, foutput);
        if (decodedsize != endofstream)
            filesize += decodedsize;
    } while (decodedsize != endofstream);

    /* final status */
    end = clock();
    displaylevel(2, "\r%79s\r", "");
    displaylevel(2, "successfully decoded %llu bytes \n", filesize);
    {
        double seconds = (double)(end - start)/clocks_per_sec;
        displaylevel(4, "done in %.2f s ==> %.2f mb/s\n", seconds, (double)filesize / seconds / 1024 / 1024);
    }

    /* close */
    fclose(finput);
    fclose(foutput);

    /*  error status = ok */
    return 0;
}

