/*
   lz4 - fast lz compression algorithm
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
   - lz4 source repository : http://code.google.com/p/lz4/
   - lz4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/
#pragma once

#if defined (__cplusplus)
extern "c" {
#endif

/*
 * lz4.h provides raw compression format functions, for optimal performance and integration into programs.
 * if you need to generate data using an inter-operable format (respecting the framing specification),
 * please use lz4frame.h instead.
*/

/**************************************
   version
**************************************/
#define lz4_version_major    1    /* for breaking interface changes  */
#define lz4_version_minor    5    /* for new (non-breaking) interface capabilities */
#define lz4_version_release  0    /* for tweaks, bug-fixes, or development */
#define lz4_version_number (lz4_version_major *100*100 + lz4_version_minor *100 + lz4_version_release)
int lz4_versionnumber (void);

/**************************************
   tuning parameter
**************************************/
/*
 * lz4_memory_usage :
 * memory usage formula : n->2^n bytes (examples : 10 -> 1kb; 12 -> 4kb ; 16 -> 64kb; 20 -> 1mb; etc.)
 * increasing memory usage improves compression ratio
 * reduced memory usage can improve speed, due to cache effect
 * default value is 14, for 16kb, which nicely fits into intel x86 l1 cache
 */
#define lz4_memory_usage 14


/**************************************
   simple functions
**************************************/

int lz4_compress        (const char* source, char* dest, int sourcesize);
int lz4_decompress_safe (const char* source, char* dest, int compressedsize, int maxdecompressedsize);

/*
lz4_compress() :
    compresses 'sourcesize' bytes from 'source' into 'dest'.
    destination buffer must be already allocated,
    and must be sized to handle worst cases situations (input data not compressible)
    worst case size evaluation is provided by function lz4_compressbound()
    inputsize : max supported value is lz4_max_input_size
    return : the number of bytes written in buffer dest
             or 0 if the compression fails

lz4_decompress_safe() :
    compressedsize : is obviously the source size
    maxdecompressedsize : is the size of the destination buffer, which must be already allocated.
    return : the number of bytes decompressed into the destination buffer (necessarily <= maxdecompressedsize)
             if the destination buffer is not large enough, decoding will stop and output an error code (<0).
             if the source stream is detected malformed, the function will stop decoding and return a negative result.
             this function is protected against buffer overflow exploits,
             and never writes outside of output buffer, nor reads outside of input buffer.
             it is also protected against malicious data packets.
*/


/**************************************
   advanced functions
**************************************/
#define lz4_max_input_size        0x7e000000   /* 2 113 929 216 bytes */
#define lz4_compressbound(isize)  ((unsigned int)(isize) > (unsigned int)lz4_max_input_size ? 0 : (isize) + ((isize)/255) + 16)

/*
lz4_compressbound() :
    provides the maximum size that lz4 compression may output in a "worst case" scenario (input data not compressible)
    this function is primarily useful for memory allocation purposes (output buffer size).
    macro lz4_compressbound() is also provided for compilation-time evaluation (stack memory allocation for example).

    isize  : is the input size. max supported value is lz4_max_input_size
    return : maximum output size in a "worst case" scenario
             or 0, if input size is too large ( > lz4_max_input_size)
*/
int lz4_compressbound(int isize);


/*
lz4_compress_limitedoutput() :
    compress 'sourcesize' bytes from 'source' into an output buffer 'dest' of maximum size 'maxoutputsize'.
    if it cannot achieve it, compression will stop, and result of the function will be zero.
    this saves time and memory on detecting non-compressible (or barely compressible) data.
    this function never writes outside of provided output buffer.

    sourcesize  : max supported value is lz4_max_input_value
    maxoutputsize : is the size of the destination buffer (which must be already allocated)
    return : the number of bytes written in buffer 'dest'
             or 0 if compression fails
*/
int lz4_compress_limitedoutput (const char* source, char* dest, int sourcesize, int maxoutputsize);


/*
lz4_compress_withstate() :
    same compression functions, but using an externally allocated memory space to store compression state.
    use lz4_sizeofstate() to know how much memory must be allocated,
    and then, provide it as 'void* state' to compression functions.
*/
int lz4_sizeofstate(void);
int lz4_compress_withstate               (void* state, const char* source, char* dest, int inputsize);
int lz4_compress_limitedoutput_withstate (void* state, const char* source, char* dest, int inputsize, int maxoutputsize);


/*
lz4_decompress_fast() :
    originalsize : is the original and therefore uncompressed size
    return : the number of bytes read from the source buffer (in other words, the compressed size)
             if the source stream is detected malformed, the function will stop decoding and return a negative result.
             destination buffer must be already allocated. its size must be a minimum of 'originalsize' bytes.
    note : this function fully respect memory boundaries for properly formed compressed data.
           it is a bit faster than lz4_decompress_safe().
           however, it does not provide any protection against intentionally modified data stream (malicious input).
           use this function in trusted environment only (data to decode comes from a trusted source).
*/
int lz4_decompress_fast (const char* source, char* dest, int originalsize);


/*
lz4_decompress_safe_partial() :
    this function decompress a compressed block of size 'compressedsize' at position 'source'
    into destination buffer 'dest' of size 'maxdecompressedsize'.
    the function tries to stop decompressing operation as soon as 'targetoutputsize' has been reached,
    reducing decompression time.
    return : the number of bytes decoded in the destination buffer (necessarily <= maxdecompressedsize)
       note : this number can be < 'targetoutputsize' should the compressed block to decode be smaller.
             always control how many bytes were decoded.
             if the source stream is detected malformed, the function will stop decoding and return a negative result.
             this function never writes outside of output buffer, and never reads outside of input buffer. it is therefore protected against malicious data packets
*/
int lz4_decompress_safe_partial (const char* source, char* dest, int compressedsize, int targetoutputsize, int maxdecompressedsize);


/***********************************************
   streaming compression functions
***********************************************/

#define lz4_streamsize_u64 ((1 << (lz4_memory_usage-3)) + 4)
#define lz4_streamsize     (lz4_streamsize_u64 * sizeof(long long))
/*
 * lz4_stream_t
 * information structure to track an lz4 stream.
 * important : init this structure content before first use !
 * note : only allocated directly the structure if you are statically linking lz4
 *        if you are using liblz4 as a dll, please use below construction methods instead.
 */
typedef struct { long long table[lz4_streamsize_u64]; } lz4_stream_t;

/*
 * lz4_resetstream
 * use this function to init an allocated lz4_stream_t structure
 */
void lz4_resetstream (lz4_stream_t* lz4_streamptr);

/*
 * lz4_createstream will allocate and initialize an lz4_stream_t structure
 * lz4_freestream releases its memory.
 * in the context of a dll (liblz4), please use these methods rather than the static struct.
 * they are more future proof, in case of a change of lz4_stream_t size.
 */
lz4_stream_t* lz4_createstream(void);
int           lz4_freestream (lz4_stream_t* lz4_streamptr);

/*
 * lz4_loaddict
 * use this function to load a static dictionary into lz4_stream.
 * any previous data will be forgotten, only 'dictionary' will remain in memory.
 * loading a size of 0 is allowed.
 * return : dictionary size, in bytes (necessarily <= 64 kb)
 */
int lz4_loaddict (lz4_stream_t* lz4_streamptr, const char* dictionary, int dictsize);

/*
 * lz4_compress_continue
 * compress data block 'source', using blocks compressed before as dictionary to improve compression ratio
 * previous data blocks are assumed to still be present at their previous location.
 */
int lz4_compress_continue (lz4_stream_t* lz4_streamptr, const char* source, char* dest, int inputsize);

/*
 * lz4_compress_limitedoutput_continue
 * same as before, but also specify a maximum target compressed size (maxoutputsize)
 * if objective cannot be met, compression exits, and returns a zero.
 */
int lz4_compress_limitedoutput_continue (lz4_stream_t* lz4_streamptr, const char* source, char* dest, int inputsize, int maxoutputsize);

/*
 * lz4_savedict
 * if previously compressed data block is not guaranteed to remain available at its memory location
 * save it into a safer place (char* safebuffer)
 * note : you don't need to call lz4_loaddict() afterwards,
 *        dictionary is immediately usable, you can therefore call again lz4_compress_continue()
 * return : dictionary size in bytes, or 0 if error
 * note : any dictsize > 64 kb will be interpreted as 64kb.
 */
int lz4_savedict (lz4_stream_t* lz4_streamptr, char* safebuffer, int dictsize);


/************************************************
   streaming decompression functions
************************************************/

#define lz4_streamdecodesize_u64  4
#define lz4_streamdecodesize     (lz4_streamdecodesize_u64 * sizeof(unsigned long long))
typedef struct { unsigned long long table[lz4_streamdecodesize_u64]; } lz4_streamdecode_t;
/*
 * lz4_streamdecode_t
 * information structure to track an lz4 stream.
 * init this structure content using lz4_setstreamdecode or memset() before first use !
 *
 * in the context of a dll (liblz4) please prefer usage of construction methods below.
 * they are more future proof, in case of a change of lz4_streamdecode_t size in the future.
 * lz4_createstreamdecode will allocate and initialize an lz4_streamdecode_t structure
 * lz4_freestreamdecode releases its memory.
 */
lz4_streamdecode_t* lz4_createstreamdecode(void);
int                 lz4_freestreamdecode (lz4_streamdecode_t* lz4_stream);

/*
 * lz4_setstreamdecode
 * use this function to instruct where to find the dictionary.
 * setting a size of 0 is allowed (same effect as reset).
 * return : 1 if ok, 0 if error
 */
int lz4_setstreamdecode (lz4_streamdecode_t* lz4_streamdecode, const char* dictionary, int dictsize);

/*
*_continue() :
    these decoding functions allow decompression of multiple blocks in "streaming" mode.
    previously decoded blocks *must* remain available at the memory position where they were decoded (up to 64 kb)
    if this condition is not possible, save the relevant part of decoded data into a safe buffer,
    and indicate where is its new address using lz4_setstreamdecode()
*/
int lz4_decompress_safe_continue (lz4_streamdecode_t* lz4_streamdecode, const char* source, char* dest, int compressedsize, int maxdecompressedsize);
int lz4_decompress_fast_continue (lz4_streamdecode_t* lz4_streamdecode, const char* source, char* dest, int originalsize);


/*
advanced decoding functions :
*_usingdict() :
    these decoding functions work the same as
    a combination of lz4_setdictdecode() followed by lz4_decompress_x_continue()
    they are stand-alone and don't use nor update an lz4_streamdecode_t structure.
*/
int lz4_decompress_safe_usingdict (const char* source, char* dest, int compressedsize, int maxdecompressedsize, const char* dictstart, int dictsize);
int lz4_decompress_fast_usingdict (const char* source, char* dest, int originalsize, const char* dictstart, int dictsize);



/**************************************
   obsolete functions
**************************************/
/*
obsolete decompression functions
these function names are deprecated and should no longer be used.
they are only provided here for compatibility with older user programs.
- lz4_uncompress is the same as lz4_decompress_fast
- lz4_uncompress_unknownoutputsize is the same as lz4_decompress_safe
these function prototypes are now disabled; uncomment them if you really need them.
it is highly recommended to stop using these functions and migrate to newer ones */
/* int lz4_uncompress (const char* source, char* dest, int outputsize); */
/* int lz4_uncompress_unknownoutputsize (const char* source, char* dest, int isize, int maxoutputsize); */


/* obsolete streaming functions; use new streaming interface whenever possible */
void* lz4_create (const char* inputbuffer);
int   lz4_sizeofstreamstate(void);
int   lz4_resetstreamstate(void* state, const char* inputbuffer);
char* lz4_slideinputbuffer (void* state);

/* obsolete streaming decoding functions */
int lz4_decompress_safe_withprefix64k (const char* source, char* dest, int compressedsize, int maxoutputsize);
int lz4_decompress_fast_withprefix64k (const char* source, char* dest, int originalsize);


#if defined (__cplusplus)
}
#endif
