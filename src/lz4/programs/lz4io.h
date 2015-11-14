/*
  lz4io.h - lz4 file/stream interface
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


/* ************************************************** */
/* special input/output values                        */
/* ************************************************** */
#define null_output "null"
static char stdinmark[] = "stdin";
static char stdoutmark[] = "stdout";
#ifdef _win32
static char nulmark[] = "nul";
#else
static char nulmark[] = "/dev/null";
#endif


/* ************************************************** */
/* ****************** functions ********************* */
/* ************************************************** */

int lz4io_compressfilename  (char* input_filename, char* output_filename, int compressionlevel);
int lz4io_decompressfilename(char* input_filename, char* output_filename);


/* ************************************************** */
/* ****************** parameters ******************** */
/* ************************************************** */

/* default setting : overwrite = 1;
   return : overwrite mode (0/1) */
int lz4io_setoverwrite(int yes);

/* blocksizeid : valid values : 4-5-6-7
   return : -1 if error, blocksize if ok */
int lz4io_setblocksizeid(int blocksizeid);

/* default setting : independent blocks */
typedef enum { lz4io_blocklinked=0, lz4io_blockindependent} lz4io_blockmode_t;
int lz4io_setblockmode(lz4io_blockmode_t blockmode);

/* default setting : no checksum */
int lz4io_setblockchecksummode(int xxhash);

/* default setting : checksum enabled */
int lz4io_setstreamchecksummode(int xxhash);

/* default setting : 0 (no notification) */
int lz4io_setnotificationlevel(int level);
