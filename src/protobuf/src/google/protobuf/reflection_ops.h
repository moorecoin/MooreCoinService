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
//
// this header is logically internal, but is made public because it is used
// from protocol-compiler-generated code, which may reside in other components.

#ifndef google_protobuf_reflection_ops_h__
#define google_protobuf_reflection_ops_h__

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/message.h>

namespace google {
namespace protobuf {
namespace internal {

// basic operations that can be performed using reflection.
// these can be used as a cheap way to implement the corresponding
// methods of the message interface, though they are likely to be
// slower than implementations tailored for the specific message type.
//
// this class should stay limited to operations needed to implement
// the message interface.
//
// this class is really a namespace that contains only static methods.
class libprotobuf_export reflectionops {
 public:
  static void copy(const message& from, message* to);
  static void merge(const message& from, message* to);
  static void clear(message* message);
  static bool isinitialized(const message& message);
  static void discardunknownfields(message* message);

  // finds all unset required fields in the message and adds their full
  // paths (e.g. "foo.bar[5].baz") to *names.  "prefix" will be attached to
  // the front of each name.
  static void findinitializationerrors(const message& message,
                                       const string& prefix,
                                       vector<string>* errors);

 private:
  // all methods are static.  no need to construct.
  google_disallow_evil_constructors(reflectionops);
};

}  // namespace internal
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_reflection_ops_h__
