/*
   xxhash - extremely fast hash algorithm
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

#ifndef beast_container_xxhash_h_included
#define beast_container_xxhash_h_included

/*****************************
   includes
*****************************/
#include <stddef.h>   /* size_t */

namespace beast {
namespace detail {

/*****************************
   type
*****************************/
typedef enum { xxh_ok=0, xxh_error } xxh_errorcode;



/*****************************
   simple hash functions
*****************************/

unsigned int       xxh32 (const void* input, size_t length, unsigned seed);
unsigned long long xxh64 (const void* input, size_t length, unsigned long long seed);

/*
xxh32() :
    calculate the 32-bits hash of sequence "length" bytes stored at memory address "input".
    the memory between input & input+length must be valid (allocated and read-accessible).
    "seed" can be used to alter the result predictably.
    this function successfully passes all smhasher tests.
    speed on core 2 duo @ 3 ghz (single thread, smhasher benchmark) : 5.4 gb/s
xxh64() :
    calculate the 64-bits hash of sequence of length "len" stored at memory address "input".
*/



/*****************************
   advanced hash functions
*****************************/
typedef struct { long long ll[ 6]; } xxh32_state_t;
typedef struct { long long ll[11]; } xxh64_state_t;

/*
these structures allow static allocation of xxh states.
states must then be initialized using xxhnn_reset() before first use.

if you prefer dynamic allocation, please refer to functions below.
*/

xxh32_state_t* xxh32_createstate(void);
xxh_errorcode  xxh32_freestate(xxh32_state_t* stateptr);

xxh64_state_t* xxh64_createstate(void);
xxh_errorcode  xxh64_freestate(xxh64_state_t* stateptr);

/*
these functions create and release memory for xxh state.
states must then be initialized using xxhnn_reset() before first use.
*/


xxh_errorcode xxh32_reset  (xxh32_state_t* stateptr, unsigned seed);
xxh_errorcode xxh32_update (xxh32_state_t* stateptr, const void* input, size_t length);
unsigned int  xxh32_digest (const xxh32_state_t* stateptr);

xxh_errorcode      xxh64_reset  (xxh64_state_t* stateptr, unsigned long long seed);
xxh_errorcode      xxh64_update (xxh64_state_t* stateptr, const void* input, size_t length);
unsigned long long xxh64_digest (const xxh64_state_t* stateptr);

/*
these functions calculate the xxhash of an input provided in multiple smaller packets,
as opposed to an input provided as a single block.

xxh state space must first be allocated, using either static or dynamic method provided above.

start a new hash by initializing state with a seed, using xxhnn_reset().

then, feed the hash state by calling xxhnn_update() as many times as necessary.
obviously, input must be valid, meaning allocated and read accessible.
the function returns an error code, with 0 meaning ok, and any other value meaning there is an error.

finally, you can produce a hash anytime, by using xxhnn_digest().
this function returns the final nn-bits hash.
you can nonetheless continue feeding the hash state with more input,
and therefore get some new hashes, by calling again xxhnn_digest().

when you are done, don't forget to free xxh state space, using typically xxhnn_freestate().
*/

} // detail
} // beast

#endif
