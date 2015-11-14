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

// author: kenton@google.com (kenton varda)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.

#include <algorithm>

#include <google/protobuf/repeated_field.h>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

namespace internal {

void repeatedptrfieldbase::reserve(int new_size) {
  if (total_size_ >= new_size) return;

  void** old_elements = elements_;
  total_size_ = max(kminrepeatedfieldallocationsize,
                    max(total_size_ * 2, new_size));
  elements_ = new void*[total_size_];
  if (old_elements != null) {
    memcpy(elements_, old_elements, allocated_size_ * sizeof(elements_[0]));
    delete [] old_elements;
  }
}

void repeatedptrfieldbase::swap(repeatedptrfieldbase* other) {
  if (this == other) return;
  void** swap_elements       = elements_;
  int    swap_current_size   = current_size_;
  int    swap_allocated_size = allocated_size_;
  int    swap_total_size     = total_size_;

  elements_       = other->elements_;
  current_size_   = other->current_size_;
  allocated_size_ = other->allocated_size_;
  total_size_     = other->total_size_;

  other->elements_       = swap_elements;
  other->current_size_   = swap_current_size;
  other->allocated_size_ = swap_allocated_size;
  other->total_size_     = swap_total_size;
}

string* stringtypehandlerbase::new() {
  return new string;
}
void stringtypehandlerbase::delete(string* value) {
  delete value;
}

}  // namespace internal


}  // namespace protobuf
}  // namespace google
