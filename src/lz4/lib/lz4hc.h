/*
   lz4 hc - high compression mode of lz4
   header file
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
#pragma once


#if defined (__cplusplus)
extern "c" {
#endif


int lz4_compresshc (const char* source, char* dest, int inputsize);
/*
lz4_compresshc :
    return : the number of bytes in compressed buffer dest
             or 0 if compression fails.
    note : destination buffer must be already allocated.
        to avoid any problem, size it to handle worst cases situations (input data not compressible)
        worst case size evaluation is provided by function lz4_compressbound() (see "lz4.h")
*/

int lz4_compresshc_limitedoutput (const char* source, char* dest, int inputsize, int maxoutputsize);
/*
lz4_compress_limitedoutput() :
    compress 'inputsize' bytes from 'source' into an output buffer 'dest' of maximum size 'maxoutputsize'.
    if it cannot achieve it, compression will stop, and result of the function will be zero.
    this function never writes outside of provided output buffer.

    inputsize  : max supported value is 1 gb
    maxoutputsize : is maximum allowed size into the destination buffer (which must be already allocated)
    return : the number of output bytes written in buffer 'dest'
             or 0 if compression fails.
*/


int lz4_compresshc2 (const char* source, char* dest, int inputsize, int compressionlevel);
int lz4_compresshc2_limitedoutput (const char* source, char* dest, int inputsize, int maxoutputsize, int compressionlevel);
/*
    same functions as above, but with programmable 'compressionlevel'.
    recommended values are between 4 and 9, although any value between 0 and 16 will work.
    'compressionlevel'==0 means use default 'compressionlevel' value.
    values above 16 behave the same as 16.
    equivalent variants exist for all other compression functions below.
*/

/* note :
   decompression functions are provided within lz4 source code (see "lz4.h") (bsd license)
*/


/**************************************
   using an external allocation
**************************************/
int lz4_sizeofstatehc(void);
int lz4_compresshc_withstatehc               (void* state, const char* source, char* dest, int inputsize);
int lz4_compresshc_limitedoutput_withstatehc (void* state, const char* source, char* dest, int inputsize, int maxoutputsize);

int lz4_compresshc2_withstatehc              (void* state, const char* source, char* dest, int inputsize, int compressionlevel);
int lz4_compresshc2_limitedoutput_withstatehc(void* state, const char* source, char* dest, int inputsize, int maxoutputsize, int compressionlevel);

/*
these functions are provided should you prefer to allocate memory for compression tables with your own allocation methods.
to know how much memory must be allocated for the compression tables, use :
int lz4_sizeofstatehc();

note that tables must be aligned for pointer (32 or 64 bits), otherwise compression will fail (return code 0).

the allocated memory can be provided to the compression functions using 'void* state' parameter.
lz4_compress_withstatehc() and lz4_compress_limitedoutput_withstatehc() are equivalent to previously described functions.
they just use the externally allocated memory for state instead of allocating their own (on stack, or on heap).
*/



/**************************************
   experimental streaming functions
**************************************/
#define lz4_streamhcsize_u64 32774
#define lz4_streamhcsize     (lz4_streamhcsize_u64 * sizeof(unsigned long long))
typedef struct { unsigned long long table[lz4_streamhcsize_u64]; } lz4_streamhc_t;
/*
lz4_streamhc_t
this structure allows static allocation of lz4 hc streaming state.
state must then be initialized using lz4_resetstreamhc() before first use.

static allocation should only be used with statically linked library.
if you want to use lz4 as a dll, please use construction functions below, which are more future-proof.
*/


lz4_streamhc_t* lz4_createstreamhc(void);
int             lz4_freestreamhc (lz4_streamhc_t* lz4_streamhcptr);
/*
these functions create and release memory for lz4 hc streaming state.
newly created states are already initialized.
existing state space can be re-used anytime using lz4_resetstreamhc().
if you use lz4 as a dll, please use these functions instead of direct struct allocation,
to avoid size mismatch between different versions.
*/

void lz4_resetstreamhc (lz4_streamhc_t* lz4_streamhcptr, int compressionlevel);
int  lz4_loaddicthc (lz4_streamhc_t* lz4_streamhcptr, const char* dictionary, int dictsize);

int lz4_compresshc_continue (lz4_streamhc_t* lz4_streamhcptr, const char* source, char* dest, int inputsize);
int lz4_compresshc_limitedoutput_continue (lz4_streamhc_t* lz4_streamhcptr, const char* source, char* dest, int inputsize, int maxoutputsize);

int lz4_savedicthc (lz4_streamhc_t* lz4_streamhcptr, char* safebuffer, int maxdictsize);

/*
these functions compress data in successive blocks of any size, using previous blocks as dictionary.
one key assumption is that each previous block will remain read-accessible while compressing next block.

before starting compression, state must be properly initialized, using lz4_resetstreamhc().
a first "fictional block" can then be designated as initial dictionary, using lz4_loaddicthc() (optional).

then, use lz4_compresshc_continue() or lz4_compresshc_limitedoutput_continue() to compress each successive block.
they work like usual lz4_compresshc() or lz4_compresshc_limitedoutput(), but use previous memory blocks to improve compression.
previous memory blocks (including initial dictionary when present) must remain accessible and unmodified during compression.

if, for any reason, previous data block can't be preserved in memory during next compression block,
you must save it to a safer memory space,
using lz4_savedicthc().
*/



/**************************************
 * deprecated streaming functions
 * ************************************/
/* note : these streaming functions follows the older model, and should no longer be used */
void* lz4_createhc (const char* inputbuffer);
char* lz4_slideinputbufferhc (void* lz4hc_data);
int   lz4_freehc (void* lz4hc_data);

int   lz4_compresshc2_continue (void* lz4hc_data, const char* source, char* dest, int inputsize, int compressionlevel);
int   lz4_compresshc2_limitedoutput_continue (void* lz4hc_data, const char* source, char* dest, int inputsize, int maxoutputsize, int compressionlevel);

int   lz4_sizeofstreamstatehc(void);
int   lz4_resetstreamstatehc(void* state, const char* inputbuffer);


#if defined (__cplusplus)
}
#endif
