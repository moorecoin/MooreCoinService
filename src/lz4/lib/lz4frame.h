/*
   lz4 auto-framing library
   header file
   copyright (c) 2011-2015, yann collet.
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
   - lz4 source repository : http://code.google.com/p/lz4/
   - lz4 source mirror : https://github.com/cyan4973/lz4
   - lz4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/* lz4f is a stand-alone api to create lz4-compressed frames
 * fully conformant to specification v1.4.1.
 * all related operations, including memory management, are handled by the library.
 * you don't need lz4.h when using lz4frame.h.
 * */

#pragma once

#if defined (__cplusplus)
extern "c" {
#endif

/**************************************
   includes
**************************************/
#include <stddef.h>   /* size_t */


/**************************************
 * error management
 * ************************************/
typedef size_t lz4f_errorcode_t;

unsigned    lz4f_iserror(lz4f_errorcode_t code);
const char* lz4f_geterrorname(lz4f_errorcode_t code);   /* return error code string; useful for debugging */


/**************************************
 * frame compression types
 * ************************************/
typedef enum { lz4f_default=0, max64kb=4, max256kb=5, max1mb=6, max4mb=7 } blocksizeid_t;
typedef enum { blocklinked=0, blockindependent} blockmode_t;
typedef enum { nocontentchecksum=0, contentchecksumenabled } contentchecksum_t;

typedef struct {
  blocksizeid_t     blocksizeid;           /* max64kb, max256kb, max1mb, max4mb ; 0 == default */
  blockmode_t       blockmode;             /* blocklinked, blockindependent ; 0 == default */
  contentchecksum_t contentchecksumflag;   /* nocontentchecksum, contentchecksumenabled ; 0 == default  */
  unsigned          reserved[5];
} lz4f_frameinfo_t;

typedef struct {
  lz4f_frameinfo_t frameinfo;
  unsigned         compressionlevel;       /* 0 == default (fast mode); values above 16 count as 16 */
  unsigned         autoflush;              /* 1 == always flush : reduce need for tmp buffer */
  unsigned         reserved[4];
} lz4f_preferences_t;


/***********************************
 * simple compression function
 * *********************************/
size_t lz4f_compressframebound(size_t srcsize, const lz4f_preferences_t* preferencesptr);

size_t lz4f_compressframe(void* dstbuffer, size_t dstmaxsize, const void* srcbuffer, size_t srcsize, const lz4f_preferences_t* preferencesptr);
/* lz4f_compressframe()
 * compress an entire srcbuffer into a valid lz4 frame, as defined by specification v1.4.1.
 * the most important rule is that dstbuffer must be large enough (dstmaxsize) to ensure compression completion even in worst case.
 * you can get the minimum value of dstmaxsize by using lz4f_compressframebound()
 * if this condition is not respected, lz4f_compressframe() will fail (result is an errorcode)
 * the lz4f_preferences_t structure is optional : you can provide null as argument. all preferences will be set to default.
 * the result of the function is the number of bytes written into dstbuffer.
 * the function outputs an error code if it fails (can be tested using lz4f_iserror())
 */



/**********************************
 * advanced compression functions
 * ********************************/
typedef void* lz4f_compressioncontext_t;

typedef struct {
  unsigned stablesrc;    /* 1 == src content will remain available on future calls to lz4f_compress(); avoid saving src content within tmp buffer as future dictionary */
  unsigned reserved[3];
} lz4f_compressoptions_t;

/* resource management */

#define lz4f_version 100
lz4f_errorcode_t lz4f_createcompressioncontext(lz4f_compressioncontext_t* lz4f_compressioncontextptr, unsigned version);
lz4f_errorcode_t lz4f_freecompressioncontext(lz4f_compressioncontext_t lz4f_compressioncontext);
/* lz4f_createcompressioncontext() :
 * the first thing to do is to create a compressioncontext object, which will be used in all compression operations.
 * this is achieved using lz4f_createcompressioncontext(), which takes as argument a version and an lz4f_preferences_t structure.
 * the version provided must be lz4f_version. it is intended to track potential version differences between different binaries.
 * the function will provide a pointer to a fully allocated lz4f_compressioncontext_t object.
 * if the result lz4f_errorcode_t is not zero, there was an error during context creation.
 * object can release its memory using lz4f_freecompressioncontext();
 */


/* compression */

size_t lz4f_compressbegin(lz4f_compressioncontext_t compressioncontext, void* dstbuffer, size_t dstmaxsize, const lz4f_preferences_t* preferencesptr);
/* lz4f_compressbegin() :
 * will write the frame header into dstbuffer.
 * dstbuffer must be large enough to accommodate a header (dstmaxsize). maximum header size is 19 bytes.
 * the lz4f_preferences_t structure is optional : you can provide null as argument, all preferences will then be set to default.
 * the result of the function is the number of bytes written into dstbuffer for the header
 * or an error code (can be tested using lz4f_iserror())
 */

size_t lz4f_compressbound(size_t srcsize, const lz4f_preferences_t* preferencesptr);
/* lz4f_compressbound() :
 * provides the minimum size of dst buffer given srcsize to handle worst case situations.
 * preferencesptr is optional : you can provide null as argument, all preferences will then be set to default.
 * note that different preferences will produce in different results.
 */

size_t lz4f_compressupdate(lz4f_compressioncontext_t compressioncontext, void* dstbuffer, size_t dstmaxsize, const void* srcbuffer, size_t srcsize, const lz4f_compressoptions_t* compressoptionsptr);
/* lz4f_compressupdate()
 * lz4f_compressupdate() can be called repetitively to compress as much data as necessary.
 * the most important rule is that dstbuffer must be large enough (dstmaxsize) to ensure compression completion even in worst case.
 * if this condition is not respected, lz4f_compress() will fail (result is an errorcode)
 * you can get the minimum value of dstmaxsize by using lz4f_compressbound()
 * the lz4f_compressoptions_t structure is optional : you can provide null as argument.
 * the result of the function is the number of bytes written into dstbuffer : it can be zero, meaning input data was just buffered.
 * the function outputs an error code if it fails (can be tested using lz4f_iserror())
 */

size_t lz4f_flush(lz4f_compressioncontext_t compressioncontext, void* dstbuffer, size_t dstmaxsize, const lz4f_compressoptions_t* compressoptionsptr);
/* lz4f_flush()
 * should you need to create compressed data immediately, without waiting for a block to be filled,
 * you can call lz4_flush(), which will immediately compress any remaining data buffered within compressioncontext.
 * the lz4f_compressoptions_t structure is optional : you can provide null as argument.
 * the result of the function is the number of bytes written into dstbuffer
 * (it can be zero, this means there was no data left within compressioncontext)
 * the function outputs an error code if it fails (can be tested using lz4f_iserror())
 */

size_t lz4f_compressend(lz4f_compressioncontext_t compressioncontext, void* dstbuffer, size_t dstmaxsize, const lz4f_compressoptions_t* compressoptionsptr);
/* lz4f_compressend()
 * when you want to properly finish the compressed frame, just call lz4f_compressend().
 * it will flush whatever data remained within compressioncontext (like lz4_flush())
 * but also properly finalize the frame, with an endmark and a checksum.
 * the result of the function is the number of bytes written into dstbuffer (necessarily >= 4 (endmark size))
 * the function outputs an error code if it fails (can be tested using lz4f_iserror())
 * the lz4f_compressoptions_t structure is optional : you can provide null as argument.
 * compressioncontext can then be used again, starting with lz4f_compressbegin().
 */


/***********************************
 * decompression functions
 * *********************************/

typedef void* lz4f_decompressioncontext_t;

typedef struct {
  unsigned stabledst;       /* guarantee that decompressed data will still be there on next function calls (avoid storage into tmp buffers) */
  unsigned reserved[3];
} lz4f_decompressoptions_t;


/* resource management */

lz4f_errorcode_t lz4f_createdecompressioncontext(lz4f_decompressioncontext_t* ctxptr, unsigned version);
lz4f_errorcode_t lz4f_freedecompressioncontext(lz4f_decompressioncontext_t ctx);
/* lz4f_createdecompressioncontext() :
 * the first thing to do is to create a decompressioncontext object, which will be used in all decompression operations.
 * this is achieved using lz4f_createdecompressioncontext().
 * the version provided must be lz4f_version. it is intended to track potential version differences between different binaries.
 * the function will provide a pointer to a fully allocated and initialized lz4f_decompressioncontext_t object.
 * if the result lz4f_errorcode_t is not ok_noerror, there was an error during context creation.
 * object can release its memory using lz4f_freedecompressioncontext();
 */


/* decompression */

size_t lz4f_getframeinfo(lz4f_decompressioncontext_t ctx,
                         lz4f_frameinfo_t* frameinfoptr,
                         const void* srcbuffer, size_t* srcsizeptr);
/* lz4f_getframeinfo()
 * this function decodes frame header information, such as blocksize.
 * it is optional : you could start by calling directly lz4f_decompress() instead.
 * the objective is to extract header information without starting decompression, typically for allocation purposes.
 * lz4f_getframeinfo() can also be used *after* starting decompression, on a valid lz4f_decompressioncontext_t.
 * the number of bytes read from srcbuffer will be provided within *srcsizeptr (necessarily <= original value).
 * you are expected to resume decompression from where it stopped (srcbuffer + *srcsizeptr)
 * the function result is an hint of how many srcsize bytes lz4f_decompress() expects for next call,
 * or an error code which can be tested using lz4f_iserror().
 */

size_t lz4f_decompress(lz4f_decompressioncontext_t ctx,
                       void* dstbuffer, size_t* dstsizeptr,
                       const void* srcbuffer, size_t* srcsizeptr,
                       const lz4f_decompressoptions_t* optionsptr);
/* lz4f_decompress()
 * call this function repetitively to regenerate data compressed within srcbuffer.
 * the function will attempt to decode *srcsizeptr bytes from srcbuffer, into dstbuffer of maximum size *dstsizeptr.
 *
 * the number of bytes regenerated into dstbuffer will be provided within *dstsizeptr (necessarily <= original value).
 *
 * the number of bytes read from srcbuffer will be provided within *srcsizeptr (necessarily <= original value).
 * if number of bytes read is < number of bytes provided, then decompression operation is not completed.
 * it typically happens when dstbuffer is not large enough to contain all decoded data.
 * lz4f_decompress() must be called again, starting from where it stopped (srcbuffer + *srcsizeptr)
 * the function will check this condition, and refuse to continue if it is not respected.
 *
 * dstbuffer is supposed to be flushed between each call to the function, since its content will be overwritten.
 * dst arguments can be changed at will with each consecutive call to the function.
 *
 * the function result is an hint of how many srcsize bytes lz4f_decompress() expects for next call.
 * schematically, it's the size of the current (or remaining) compressed block + header of next block.
 * respecting the hint provides some boost to performance, since it does skip intermediate buffers.
 * this is just a hint, you can always provide any srcsize you want.
 * when a frame is fully decoded, the function result will be 0. (no more data expected)
 * if decompression failed, function result is an error code, which can be tested using lz4f_iserror().
 */



#if defined (__cplusplus)
}
#endif
