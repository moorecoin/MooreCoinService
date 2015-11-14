/*
   xxhash - fast hash algorithm
   header file
   copyright (c) 2012-2014, yann collet.
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
   - xxhash source repository : http://code.google.com/p/xxhash/
*/

/* notice extracted from xxhash homepage :

xxhash is an extremely fast hash algorithm, running at ram speed limits.
it also successfully passes all tests from the smhasher suite.

comparison (single thread, windows seven 32 bits, using smhasher on a core 2 duo @3ghz)

name            speed       q.score   author
xxhash          5.4 gb/s     10
crapwow         3.2 gb/s      2       andrew
mumurhash 3a    2.7 gb/s     10       austin appleby
spookyhash      2.0 gb/s     10       bob jenkins
sbox            1.4 gb/s      9       bret mulvey
lookup3         1.2 gb/s      9       bob jenkins
superfasthash   1.2 gb/s      1       paul hsieh
cityhash64      1.05 gb/s    10       pike & alakuijala
fnv             0.55 gb/s     5       fowler, noll, vo
crc32           0.43 gb/s     9
md5-32          0.33 gb/s    10       ronald l. rivest
sha1-32         0.28 gb/s    10

q.score is a measure of quality of the hash function.
it depends on successfully passing smhasher test set.
10 is a perfect score.
*/

#pragma once

#if defined (__cplusplus)
extern "c" {
#endif


//****************************
// type
//****************************
typedef enum { xxh_ok=0, xxh_error } xxh_errorcode;



//****************************
// simple hash functions
//****************************

unsigned int xxh32 (const void* input, int len, unsigned int seed);

/*
xxh32() :
    calculate the 32-bits hash of sequence of length "len" stored at memory address "input".
    the memory between input & input+len must be valid (allocated and read-accessible).
    "seed" can be used to alter the result predictably.
    this function successfully passes all smhasher tests.
    speed on core 2 duo @ 3 ghz (single thread, smhasher benchmark) : 5.4 gb/s
    note that "len" is type "int", which means it is limited to 2^31-1.
    if your data is larger, use the advanced functions below.
*/



//****************************
// advanced hash functions
//****************************

void*         xxh32_init   (unsigned int seed);
xxh_errorcode xxh32_update (void* state, const void* input, int len);
unsigned int  xxh32_digest (void* state);

/*
these functions calculate the xxhash of an input provided in several small packets,
as opposed to an input provided as a single block.

it must be started with :
void* xxh32_init()
the function returns a pointer which holds the state of calculation.

this pointer must be provided as "void* state" parameter for xxh32_update().
xxh32_update() can be called as many times as necessary.
the user must provide a valid (allocated) input.
the function returns an error code, with 0 meaning ok, and any other value meaning there is an error.
note that "len" is type "int", which means it is limited to 2^31-1.
if your data is larger, it is recommended to chunk your data into blocks
of size for example 2^30 (1gb) to avoid any "int" overflow issue.

finally, you can end the calculation anytime, by using xxh32_digest().
this function returns the final 32-bits hash.
you must provide the same "void* state" parameter created by xxh32_init().
memory will be freed by xxh32_digest().
*/


int           xxh32_sizeofstate();
xxh_errorcode xxh32_resetstate(void* state, unsigned int seed);

#define       xxh32_sizeofstate 48
typedef struct { long long ll[(xxh32_sizeofstate+(sizeof(long long)-1))/sizeof(long long)]; } xxh32_statespace_t;
/*
these functions allow user application to make its own allocation for state.

xxh32_sizeofstate() is used to know how much space must be allocated for the xxhash 32-bits state.
note that the state must be aligned to access 'long long' fields. memory must be allocated and referenced by a pointer.
this pointer must then be provided as 'state' into xxh32_resetstate(), which initializes the state.

for static allocation purposes (such as allocation on stack, or freestanding systems without malloc()),
use the structure xxh32_statespace_t, which will ensure that memory space is large enough and correctly aligned to access 'long long' fields.
*/


unsigned int xxh32_intermediatedigest (void* state);
/*
this function does the same as xxh32_digest(), generating a 32-bit hash,
but preserve memory context.
this way, it becomes possible to generate intermediate hashes, and then continue feeding data with xxh32_update().
to free memory context, use xxh32_digest(), or free().
*/



//****************************
// deprecated function names
//****************************
// the following translations are provided to ease code transition
// you are encouraged to no longer this function names
#define xxh32_feed   xxh32_update
#define xxh32_result xxh32_digest
#define xxh32_getintermediateresult xxh32_intermediatedigest



#if defined (__cplusplus)
}
#endif
