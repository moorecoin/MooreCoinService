// protocol buffers - google's data interchange format
// copyright 2008 google inc.  all rights reserved.
// http://code.google.com/p/protobuf/
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

// author: jasonh@google.com (jason hsueh)
//
// implements methods of coded_stream.h that need to be inlined for performance
// reasons, but should not be defined in a public header.

#ifndef google_protobuf_io_coded_stream_inl_h__
#define google_protobuf_io_coded_stream_inl_h__

#include <google/protobuf/io/coded_stream.h>
#include <string>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
namespace protobuf {
namespace io {

inline bool codedinputstream::internalreadstringinline(string* buffer,
                                                       int size) {
  if (size < 0) return false;  // security: size is often user-supplied

  if (buffersize() >= size) {
    stlstringresizeuninitialized(buffer, size);
    // when buffer is empty, string_as_array(buffer) will return null but memcpy
    // requires non-null pointers even when size is 0. hench this check.
    if (size > 0) {
      memcpy(string_as_array(buffer), buffer_, size);
      advance(size);
    }
    return true;
  }

  return readstringfallback(buffer, size);
}

}  // namespace io
}  // namespace protobuf
}  // namespace google
#endif  // google_protobuf_io_coded_stream_inl_h__
