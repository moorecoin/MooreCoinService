// copyright 2011 martin gieseking <martin.gieseking@uos.de>.
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * neither the name of google inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// owner or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.

#include "snappy.h"
#include "snappy-c.h"

extern "c" {

snappy_status snappy_compress(const char* input,
                              size_t input_length,
                              char* compressed,
                              size_t *compressed_length) {
  if (*compressed_length < snappy_max_compressed_length(input_length)) {
    return snappy_buffer_too_small;
  }
  snappy::rawcompress(input, input_length, compressed, compressed_length);
  return snappy_ok;
}

snappy_status snappy_uncompress(const char* compressed,
                                size_t compressed_length,
                                char* uncompressed,
                                size_t* uncompressed_length) {
  size_t real_uncompressed_length;
  if (!snappy::getuncompressedlength(compressed,
                                     compressed_length,
                                     &real_uncompressed_length)) {
    return snappy_invalid_input;
  }
  if (*uncompressed_length < real_uncompressed_length) {
    return snappy_buffer_too_small;
  }
  if (!snappy::rawuncompress(compressed, compressed_length, uncompressed)) {
    return snappy_invalid_input;
  }
  *uncompressed_length = real_uncompressed_length;
  return snappy_ok;
}

size_t snappy_max_compressed_length(size_t source_length) {
  return snappy::maxcompressedlength(source_length);
}

snappy_status snappy_uncompressed_length(const char *compressed,
                                         size_t compressed_length,
                                         size_t *result) {
  if (snappy::getuncompressedlength(compressed,
                                    compressed_length,
                                    result)) {
    return snappy_ok;
  } else {
    return snappy_invalid_input;
  }
}

snappy_status snappy_validate_compressed_buffer(const char *compressed,
                                                size_t compressed_length) {
  if (snappy::isvalidcompressedbuffer(compressed, compressed_length)) {
    return snappy_ok;
  } else {
    return snappy_invalid_input;
  }
}

}  // extern "c"
