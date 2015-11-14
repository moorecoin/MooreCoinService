/*
  lz4cli - lz4 command line interface
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
  it is not part of lz4 compression library, it is a user program of the lz4 library.
  the license of lz4 library is bsd.
  the license of xxhash library is bsd.
  the license of this compression cli program is gplv2.
*/

/**************************************
*  tuning parameters
***************************************/
/* enable_lz4c_legacy_options :
   control the availability of -c0, -c1 and -hc legacy arguments
   default : legacy options are disabled */
/* #define enable_lz4c_legacy_options */


/**************************************
*  compiler options
***************************************/
/* disable some visual warning messages */
#ifdef _msc_ver
#  define _crt_secure_no_warnings
#  define _crt_secure_no_deprecate     /* vs2005 */
#  pragma warning(disable : 4127)      /* disable: c4127: conditional expression is constant */
#endif

#define _posix_source 1        /* for fileno() within <stdio.h> on unix */


/****************************
*  includes
*****************************/
#include <stdio.h>    /* fprintf, getchar */
#include <stdlib.h>   /* exit, calloc, free */
#include <string.h>   /* strcmp, strlen */
#include "bench.h"    /* bmk_benchfile, bmk_setnbiterations, bmk_setblocksize, bmk_setpause */
#include "lz4io.h"


/****************************
*  os-specific includes
*****************************/
#if defined(msdos) || defined(os2) || defined(win32) || defined(_win32) || defined(__cygwin__)
#  include <fcntl.h>    /* _o_binary */
#  include <io.h>       /* _setmode, _isatty */
#  ifdef __mingw32__
   int _fileno(file *stream);   /* mingw somehow forgets to include this prototype into <stdio.h> */
#  endif
#  define set_binary_mode(file) _setmode(_fileno(file), _o_binary)
#  define is_console(stdstream) _isatty(_fileno(stdstream))
#else
#  include <unistd.h>   /* isatty */
#  define set_binary_mode(file)
#  define is_console(stdstream) isatty(fileno(stdstream))
#endif


/*****************************
*  constants
******************************/
#define compressor_name "lz4 command line interface"
#ifndef lz4_version
#  define lz4_version "r126"
#endif
#define author "yann collet"
#define welcome_message "*** %s %i-bits %s, by %s (%s) ***\n", compressor_name, (int)(sizeof(void*)*8), lz4_version, author, __date__
#define lz4_extension ".lz4"
#define lz4_cat "lz4cat"

#define kb *(1u<<10)
#define mb *(1u<<20)
#define gb *(1u<<30)

#define lz4_blocksizeid_default 7


/**************************************
*  macros
***************************************/
#define display(...)           fprintf(stderr, __va_args__)
#define displaylevel(l, ...)   if (displaylevel>=l) { display(__va_args__); }
static unsigned displaylevel = 2;   /* 0 : no display ; 1: errors ; 2 : + result + interaction + warnings ; 3 : + progression; 4 : + information */


/**************************************
*  local variables
***************************************/
static char* programname;


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
#define default_compressor   lz4io_compressfilename
#define default_decompressor lz4io_decompressfilename
int lz4io_compressfilename_legacy(char* input_filename, char* output_filename, int compressionlevel);   /* hidden function */


/****************************
*  functions
*****************************/
static int usage(void)
{
    display( "usage :\n");
    display( "      %s [arg] [input] [output]\n", programname);
    display( "\n");
    display( "input   : a filename\n");
    display( "          with no file, or when file is - or %s, read standard input\n", stdinmark);
    display( "arguments :\n");
    display( " -1     : fast compression (default) \n");
    display( " -9     : high compression \n");
    display( " -d     : decompression (default for %s extension)\n", lz4_extension);
    display( " -z     : force compression\n");
    display( " -f     : overwrite output without prompting \n");
    display( " -h/-h  : display help/long help and exit\n");
    return 0;
}

static int usage_advanced(void)
{
    display(welcome_message);
    usage();
    display( "\n");
    display( "advanced arguments :\n");
    display( " -v     : display version number and exit\n");
    display( " -v     : verbose mode\n");
    display( " -q     : suppress warnings; specify twice to suppress errors too\n");
    display( " -c     : force write to standard output, even if it is the console\n");
    display( " -t     : test compressed file integrity\n");
    display( " -l     : compress using legacy format (linux kernel compression)\n");
    display( " -b#    : block size [4-7](default : 7)\n");
    display( " -bd    : block dependency (improve compression ratio)\n");
    /* display( " -bx    : enable block checksum (default:disabled)\n");   *//* option currently inactive */
    display( " -sx    : disable stream checksum (default:enabled)\n");
    display( "benchmark arguments :\n");
    display( " -b     : benchmark file(s)\n");
    display( " -i#    : iteration loops [1-9](default : 3), benchmark mode only\n");
#if defined(enable_lz4c_legacy_options)
    display( "legacy arguments :\n");
    display( " -c0    : fast compression\n");
    display( " -c1    : high compression\n");
    display( " -hc    : high compression\n");
    display( " -y     : overwrite output without prompting \n");
    display( " -s     : suppress warnings \n");
#endif /* enable_lz4c_legacy_options */
    extended_help;
    return 0;
}

static int usage_longhelp(void)
{
    display( "\n");
    display( "which values can get [output] ? \n");
    display( "[output] : a filename\n");
    display( "          '%s', or '-' for standard output (pipe mode)\n", stdoutmark);
    display( "          '%s' to discard output (test mode)\n", null_output);
    display( "[output] can be left empty. in this case, it receives the following value : \n");
    display( "          - if stdout is not the console, then [output] = stdout \n");
    display( "          - if stdout is console : \n");
    display( "               + if compression selected, output to filename%s \n", lz4_extension);
    display( "               + if decompression selected, output to filename without '%s'\n", lz4_extension);
    display( "                    > if input filename has no '%s' extension : error\n", lz4_extension);
    display( "\n");
    display( "compression levels : \n");
    display( "there are technically 2 accessible compression levels.\n");
    display( "-0 ... -2 => fast compression\n");
    display( "-3 ... -9 => high compression\n");
    display( "\n");
    display( "stdin, stdout and the console : \n");
    display( "to protect the console from binary flooding (bad argument mistake)\n");
    display( "%s will refuse to read from console, or write to console \n", programname);
    display( "except if '-c' command is specified, to force output to console \n");
    display( "\n");
    display( "simple example :\n");
    display( "1 : compress 'filename' fast, using default output name 'filename.lz4'\n");
    display( "          %s filename\n", programname);
    display( "\n");
    display( "arguments can be appended together, or provided independently. for example :\n");
    display( "2 : compress 'filename' in high compression mode, overwrite output if exists\n");
    display( "          %s -f9 filename \n", programname);
    display( "    is equivalent to :\n");
    display( "          %s -f -9 filename \n", programname);
    display( "\n");
    display( "%s can be used in 'pure pipe mode', for example :\n", programname);
    display( "3 : compress data stream from 'generator', send result to 'consumer'\n");
    display( "          generator | %s | consumer \n", programname);
#if defined(enable_lz4c_legacy_options)
    display( "\n");
    display( "warning :\n");
    display( "legacy arguments take precedence. therefore : \n");
    display( "          %s -hc filename\n", programname);
    display( "means 'compress filename in high compression mode'\n");
    display( "it is not equivalent to :\n");
    display( "          %s -h -c filename\n", programname);
    display( "which would display help text and exit\n");
#endif /* enable_lz4c_legacy_options */
    return 0;
}

static int badusage(void)
{
    displaylevel(1, "incorrect parameters\n");
    if (displaylevel >= 1) usage();
    exit(1);
}


static void waitenter(void)
{
    display("press enter to continue...\n");
    getchar();
}


int main(int argc, char** argv)
{
    int i,
        clevel=0,
        decode=0,
        bench=0,
        filenamesstart=2,
        legacy_format=0,
        forcestdout=0,
        forcecompress=0,
        main_pause=0;
    char* input_filename=0;
    char* output_filename=0;
    char* dynnamespace=0;
    char nulloutput[] = null_output;
    char extension[] = lz4_extension;
    int blocksize;

    /* init */
    programname = argv[0];
    lz4io_setoverwrite(0);
    blocksize = lz4io_setblocksizeid(lz4_blocksizeid_default);

    /* lz4cat behavior */
    if (!strcmp(programname, lz4_cat)) { decode=1; forcestdout=1; output_filename=stdoutmark; displaylevel=1; }

    /* command switches */
    for(i=1; i<argc; i++)
    {
        char* argument = argv[i];

        if(!argument) continue;   /* protection if argument empty */

        /* decode command (note : aggregated commands are allowed) */
        if (argument[0]=='-')
        {
            /* '-' means stdin/stdout */
            if (argument[1]==0)
            {
                if (!input_filename) input_filename=stdinmark;
                else output_filename=stdoutmark;
            }

            while (argument[1]!=0)
            {
                argument ++;

#if defined(enable_lz4c_legacy_options)
                /* legacy arguments (-c0, -c1, -hc, -y, -s) */
                if ((argument[0]=='c') && (argument[1]=='0')) { clevel=0; argument++; continue; }  /* -c0 (fast compression) */
                if ((argument[0]=='c') && (argument[1]=='1')) { clevel=9; argument++; continue; }  /* -c1 (high compression) */
                if ((argument[0]=='h') && (argument[1]=='c')) { clevel=9; argument++; continue; }  /* -hc (high compression) */
                if (*argument=='y') { lz4io_setoverwrite(1); continue; }                           /* -y (answer 'yes' to overwrite permission) */
                if (*argument=='s') { displaylevel=1; continue; }                                  /* -s (silent mode) */
#endif /* enable_lz4c_legacy_options */

                if ((*argument>='0') && (*argument<='9'))
                {
                    clevel = 0;
                    while ((*argument >= '0') && (*argument <= '9'))
                    {
                        clevel *= 10;
                        clevel += *argument - '0';
                        argument++;
                    }
                    argument--;
                    continue;
                }

                switch(argument[0])
                {
                    /* display help */
                case 'v': display(welcome_message); return 0;   /* version */
                case 'h': usage_advanced(); return 0;
                case 'h': usage_advanced(); usage_longhelp(); return 0;

                    /* compression (default) */
                case 'z': forcecompress = 1; break;

                    /* use legacy format (ex : linux kernel compression) */
                case 'l': legacy_format = 1; blocksize = 8 mb; break;

                    /* decoding */
                case 'd': decode=1; break;

                    /* force stdout, even if stdout==console */
                case 'c': forcestdout=1; output_filename=stdoutmark; displaylevel=1; break;

                    /* test integrity */
                case 't': decode=1; lz4io_setoverwrite(1); output_filename=nulmark; break;

                    /* overwrite */
                case 'f': lz4io_setoverwrite(1); break;

                    /* verbose mode */
                case 'v': displaylevel=4; break;

                    /* quiet mode */
                case 'q': displaylevel--; break;

                    /* keep source file (default anyway, so useless) (for xz/lzma compatibility) */
                case 'k': break;

                    /* modify block properties */
                case 'b':
                    while (argument[1]!=0)
                    {
                        int exitblockproperties=0;
                        switch(argument[1])
                        {
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        {
                            int b = argument[1] - '0';
                            blocksize = lz4io_setblocksizeid(b);
                            bmk_setblocksize(blocksize);
                            argument++;
                            break;
                        }
                        case 'd': lz4io_setblockmode(lz4io_blocklinked); argument++; break;
                        case 'x': lz4io_setblockchecksummode(1); argument ++; break;   /* currently disables */
                        default : exitblockproperties=1;
                        }
                        if (exitblockproperties) break;
                    }
                    break;

                    /* modify stream properties */
                case 's': if (argument[1]=='x') { lz4io_setstreamchecksummode(0); argument++; break; } else { badusage(); }

                    /* benchmark */
                case 'b': bench=1; break;

                    /* modify nb iterations (benchmark only) */
                case 'i':
                    if ((argument[1] >='1') && (argument[1] <='9'))
                    {
                        int iters = argument[1] - '0';
                        bmk_setnbiterations(iters);
                        argument++;
                    }
                    break;

                    /* pause at the end (hidden option) */
                case 'p': main_pause=1; bmk_setpause(); break;

                    /* specific commands for customized versions */
                extended_arguments;

                    /* unrecognised command */
                default : badusage();
                }
            }
            continue;
        }

        /* first provided filename is input */
        if (!input_filename) { input_filename=argument; filenamesstart=i; continue; }

        /* second provided filename is output */
        if (!output_filename)
        {
            output_filename=argument;
            if (!strcmp (output_filename, nulloutput)) output_filename = nulmark;
            continue;
        }
    }

    displaylevel(3, welcome_message);
    if (!decode) displaylevel(4, "blocks size : %i kb\n", blocksize>>10);

    /* no input filename ==> use stdin */
    if(!input_filename) { input_filename=stdinmark; }

    /* check if input or output are defined as console; trigger an error in this case */
    if (!strcmp(input_filename, stdinmark) && is_console(stdin) ) badusage();

    /* check if benchmark is selected */
    if (bench) return bmk_benchfile(argv+filenamesstart, argc-filenamesstart, clevel);

    /* no output filename ==> try to select one automatically (when possible) */
    while (!output_filename)
    {
        if (!is_console(stdout)) { output_filename=stdoutmark; break; }   /* default to stdout whenever possible (i.e. not a console) */
        if ((!decode) && !(forcecompress))   /* auto-determine compression or decompression, based on file extension */
        {
            size_t l = strlen(input_filename);
            if (!strcmp(input_filename+(l-4), lz4_extension)) decode=1;
        }
        if (!decode)   /* compression to file */
        {
            size_t l = strlen(input_filename);
            dynnamespace = (char*)calloc(1,l+5);
            output_filename = dynnamespace;
            strcpy(output_filename, input_filename);
            strcpy(output_filename+l, lz4_extension);
            displaylevel(2, "compressed filename will be : %s \n", output_filename);
            break;
        }
        /* decompression to file (automatic name will work only if input filename has correct format extension) */
        {
            size_t outl;
            size_t inl = strlen(input_filename);
            dynnamespace = (char*)calloc(1,inl+1);
            output_filename = dynnamespace;
            strcpy(output_filename, input_filename);
            outl = inl;
            if (inl>4)
                while ((outl >= inl-4) && (input_filename[outl] ==  extension[outl-inl+4])) output_filename[outl--]=0;
            if (outl != inl-5) { displaylevel(1, "cannot determine an output filename\n"); badusage(); }
            displaylevel(2, "decoding file %s \n", output_filename);
        }
    }

    /* check if output is defined as console; trigger an error in this case */
    if (!strcmp(output_filename,stdoutmark) && is_console(stdout) && !forcestdout) badusage();

    /* no warning message in pure pipe mode (stdin + stdout) */
    if (!strcmp(input_filename, stdinmark) && !strcmp(output_filename,stdoutmark) && (displaylevel==2)) displaylevel=1;


    /* io stream/file */
    lz4io_setnotificationlevel(displaylevel);
    if (decode) default_decompressor(input_filename, output_filename);
    else
    {
        /* compression is default action */
        if (legacy_format)
        {
            displaylevel(3, "! generating compressed lz4 using legacy format (deprecated) ! \n");
            lz4io_compressfilename_legacy(input_filename, output_filename, clevel);
        }
        else
        {
            default_compressor(input_filename, output_filename, clevel);
        }
    }

    if (main_pause) waitenter();
    free(dynnamespace);
    return 0;
}
