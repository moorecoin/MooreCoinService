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

// from google3/util/gtl/stl_util.h

#ifndef google_protobuf_stubs_stl_util_h__
#define google_protobuf_stubs_stl_util_h__

#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

// stldeletecontainerpointers()
//  for a range within a container of pointers, calls delete
//  (non-array version) on these pointers.
// note: for these three functions, we could just implement a deleteobject
// functor and then call for_each() on the range and functor, but this
// requires us to pull in all of algorithm.h, which seems expensive.
// for hash_[multi]set, it is important that this deletes behind the iterator
// because the hash_set may call the hash function on the iterator when it is
// advanced, which could result in the hash function trying to deference a
// stale pointer.
template <class forwarditerator>
void stldeletecontainerpointers(forwarditerator begin,
                                forwarditerator end) {
  while (begin != end) {
    forwarditerator temp = begin;
    ++begin;
    delete *temp;
  }
}

// inside google, this function implements a horrible, disgusting hack in which
// we reach into the string's private implementation and resize it without
// initializing the new bytes.  in some cases doing this can significantly
// improve performance.  however, since it's totally non-portable it has no
// place in open source code.  feel free to fill this function in with your
// own disgusting hack if you want the perf boost.
inline void stlstringresizeuninitialized(string* s, size_t new_size) {
  s->resize(new_size);
}

// return a mutable char* pointing to a string's internal buffer,
// which may not be null-terminated. writing through this pointer will
// modify the string.
//
// string_as_array(&str)[i] is valid for 0 <= i < str.size() until the
// next call to a string method that invalidates iterators.
//
// as of 2006-04, there is no standard-blessed way of getting a
// mutable reference to a string's internal buffer. however, issue 530
// (http://www.open-std.org/jtc1/sc22/wg21/docs/lwg-active.html#530)
// proposes this as the method. according to matt austern, this should
// already work on all current implementations.
inline char* string_as_array(string* str) {
  // do not use const_cast<char*>(str->data())! see the unittest for why.
  return str->empty() ? null : &*str->begin();
}

// stldeleteelements() deletes all the elements in an stl container and clears
// the container.  this function is suitable for use with a vector, set,
// hash_set, or any other stl container which defines sensible begin(), end(),
// and clear() methods.
//
// if container is null, this function is a no-op.
//
// as an alternative to calling stldeleteelements() directly, consider
// elementdeleter (defined below), which ensures that your container's elements
// are deleted when the elementdeleter goes out of scope.
template <class t>
void stldeleteelements(t *container) {
  if (!container) return;
  stldeletecontainerpointers(container->begin(), container->end());
  container->clear();
}

// given an stl container consisting of (key, value) pairs, stldeletevalues
// deletes all the "value" components and clears the container.  does nothing
// in the case it's given a null pointer.

template <class t>
void stldeletevalues(t *v) {
  if (!v) return;
  for (typename t::iterator i = v->begin(); i != v->end(); ++i) {
    delete i->second;
  }
  v->clear();
}

}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_stubs_stl_util_h__
