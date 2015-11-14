/*
   lz4 auto-framing library
   header file for static linking only
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

#pragma once

#if defined (__cplusplus)
extern "c" {
#endif

/* lz4frame_static.h should be used solely in the context of static linking.
 * */


/**************************************
 * error management
 * ************************************/
#define lz4f_list_errors(item) \
        item(ok_noerror) item(error_generic) \
        item(error_maxblocksize_invalid) item(error_blockmode_invalid) item(error_contentchecksumflag_invalid) \
        item(error_compressionlevel_invalid) \
        item(error_allocation_failed) \
        item(error_srcsize_toolarge) item(error_dstmaxsize_toosmall) \
        item(error_decompressionfailed) \
        item(error_checksum_invalid) \
        item(error_maxcode)

#define lz4f_generate_enum(enum) enum,
typedef enum { lz4f_list_errors(lz4f_generate_enum) } lz4f_errorcodes;  /* enum is exposed, to handle specific errors; compare function result to -enum value */


/**************************************
   includes
**************************************/
#include "lz4frame.h"


#if defined (__cplusplus)
}
#endif
