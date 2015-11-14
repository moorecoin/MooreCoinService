/*
    datagen.c - compressible data generator test tool
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
 remove visual warning messages
**************************************/
#define _crt_secure_no_warnings   // fgets


/**************************************
 includes
**************************************/
#include <stdio.h>      // fgets, sscanf
#include <string.h>     // strcmp


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
#  define lz4_version "r125"
#endif

#define kb *(1 <<10)
#define mb *(1 <<20)
#define gb *(1u<<30)

#define cdg_size_default (64 kb)
#define cdg_seed_default 0
#define cdg_compressibility_default 50
#define prime1   2654435761u
#define prime2   2246822519u


/**************************************
  macros
**************************************/
#define display(...)         fprintf(stderr, __va_args__)
#define displaylevel(l, ...) if (displaylevel>=l) { display(__va_args__); }


/**************************************
  local parameters
**************************************/
static unsigned no_prompt = 0;
static char*    programname;
static unsigned displaylevel = 2;


/*********************************************************
  functions
*********************************************************/

#define cdg_rotl32(x,r) ((x << r) | (x >> (32 - r)))
static unsigned int cdg_rand(u32* src)
{
    u32 rand32 = *src;
    rand32 *= prime1;
    rand32 += prime2;
    rand32  = cdg_rotl32(rand32, 13);
    *src = rand32;
    return rand32;
}


#define cdg_rand15bits  ((cdg_rand(seed) >> 3) & 32767)
#define cdg_randlength  ( ((cdg_rand(seed) >> 7) & 3) ? (cdg_rand(seed) % 14) : (cdg_rand(seed) & 511) + 15)
#define cdg_randchar    (((cdg_rand(seed) >> 9) & 63) + '0')
static void cdg_generate(u64 size, u32* seed, double proba)
{
    byte fullbuff[32 kb + 128 kb + 1];
    byte* buff = fullbuff + 32 kb;
    u64 total=0;
    u32 p32 = (u32)(32768 * proba);
    u32 pos=1;
    u32 genblocksize = 128 kb;

    // build initial prefix
    fullbuff[0] = cdg_randchar;
    while (pos<32 kb)
    {
        // select : literal (char) or match (within 32k)
        if (cdg_rand15bits < p32)
        {
            // copy (within 64k)
            u32 d;
            int ref;
            int length = cdg_randlength + 4;
            u32 offset = cdg_rand15bits + 1;
            if (offset > pos) offset = pos;
            ref = pos - offset;
            d = pos + length;
            while (pos < d) fullbuff[pos++] = fullbuff[ref++];
        }
        else
        {
            // literal (noise)
            u32 d = pos + cdg_randlength;
            while (pos < d) fullbuff[pos++] = cdg_randchar;
        }
    }

    // generate compressible data
    pos = 0;
    while (total < size)
    {
        if (size-total < 128 kb) genblocksize = (u32)(size-total);
        total += genblocksize;
        buff[genblocksize] = 0;
        pos = 0;
        while (pos<genblocksize)
        {
            // select : literal (char) or match (within 32k)
            if (cdg_rand15bits < p32)
            {
                // copy (within 64k)
                int ref;
                u32 d;
                int length = cdg_randlength + 4;
                u32 offset = cdg_rand15bits + 1;
                if (pos + length > genblocksize ) length = genblocksize - pos;
                ref = pos - offset;
                d = pos + length;
                while (pos < d) buff[pos++] = buff[ref++];
            }
            else
            {
                // literal (noise)
                u32 d;
                int length = cdg_randlength;
                if (pos + length > genblocksize) length = genblocksize - pos;
                d = pos + length;
                while (pos < d) buff[pos++] = cdg_randchar;
            }
        }
        // output datagen
        pos=0;
        for (;pos+512<=genblocksize;pos+=512)
            printf("%512.512s", buff+pos);
        for (;pos<genblocksize;pos++) printf("%c", buff[pos]);
        // regenerate prefix
        memcpy(fullbuff, buff + 96 kb, 32 kb);
    }
}


int cdg_usage(void)
{
    display( "compressible data generator\n");
    display( "usage :\n");
    display( "      %s [size] [args]\n", programname);
    display( "\n");
    display( "arguments :\n");
    display( " -g#    : generate # data (default:%i)\n", cdg_size_default);
    display( " -s#    : select seed (default:%i)\n", cdg_seed_default);
    display( " -p#    : select compressibility in %% (default:%i%%)\n", cdg_compressibility_default);
    display( " -h     : display help and exit\n");
    return 0;
}


int main(int argc, char** argv)
{
    int argnb;
    int proba = cdg_compressibility_default;
    u64 size = cdg_size_default;
    u32 seed = cdg_seed_default;

    // check command line
    programname = argv[0];
    for(argnb=1; argnb<argc; argnb++)
    {
        char* argument = argv[argnb];

        if(!argument) continue;   // protection if argument empty

        // decode command (note : aggregated commands are allowed)
        if (*argument=='-')
        {
            if (!strcmp(argument, "--no-prompt")) { no_prompt=1; continue; }

            argument++;
            while (*argument!=0)
            {
                switch(*argument)
                {
                case 'h':
                    return cdg_usage();
                case 'g':
                    argument++;
                    size=0;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        size *= 10;
                        size += *argument - '0';
                        argument++;
                    }
                    if (*argument=='k') { size <<= 10; argument++; }
                    if (*argument=='m') { size <<= 20; argument++; }
                    if (*argument=='g') { size <<= 30; argument++; }
                    if (*argument=='b') { argument++; }
                    break;
                case 's':
                    argument++;
                    seed=0;
                    while ((*argument>='0') && (*argument<='9'))
                    {
                        seed *= 10;
                        seed += *argument - '0';
                        argument++;
                    }
                    break;
                case 'p':
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
                case 'v':
                    displaylevel = 4;
                    argument++;
                    break;
                default: ;
                }
            }

        }
    }

    // get seed
    displaylevel(4, "data generator %s \n", lz4_version);
    displaylevel(3, "seed = %u \n", seed);
    if (proba!=cdg_compressibility_default) displaylevel(3, "compressibility : %i%%\n", proba);

    cdg_generate(size, &seed, ((double)proba) / 100);

    return 0;
}
