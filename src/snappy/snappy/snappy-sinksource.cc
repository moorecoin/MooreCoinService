// copyright 2011 google inc. all rights reserved.
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

#include <string.h>

#include "snappy-sinksource.h"

namespace snappy {

source::~source() { }

sink::~sink() { }

char* sink::getappendbuffer(size_t length, char* scratch) {
  return scratch;
}

bytearraysource::~bytearraysource() { }

size_t bytearraysource::available() const { return left_; }

const char* bytearraysource::peek(size_t* len) {
  *len = left_;
  return ptr_;
}

void bytearraysource::skip(size_t n) {
  left_ -= n;
  ptr_ += n;
}

uncheckedbytearraysink::~uncheckedbytearraysink() { }

void uncheckedbytearraysink::append(const char* data, size_t n) {
  // do no copying if the caller filled in the result of getappendbuffer()
  if (data != dest_) {
    memcpy(dest_, data, n);
  }
  dest_ += n;
}

char* uncheckedbytearraysink::getappendbuffer(size_t len, char* scratch) {
  return dest_;
}

}
