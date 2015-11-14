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
// this file contains miscellaneous helper code used by generated code --
// including lite types -- but which should not be used directly by users.

#ifndef google_protobuf_generated_message_util_h__
#define google_protobuf_generated_message_util_h__

#include <string>

#include <google/protobuf/stubs/common.h>
namespace google {
namespace protobuf {
namespace internal {

// annotation for the compiler to emit a deprecation message if a field marked
// with option 'deprecated=true' is used in the code, or for other things in
// generated code which are deprecated.
//
// for internal use in the pb.cc files, deprecation warnings are suppressed
// there.
#undef deprecated_protobuf_field
#define protobuf_deprecated


// constants for special floating point values.
libprotobuf_export double infinity();
libprotobuf_export double nan();

// constant used for empty default strings.
libprotobuf_export extern const ::std::string kemptystring;

// defined in generated_message_reflection.cc -- not actually part of the lite
// library.
//
// todo(jasonh): the various callers get this declaration from a variety of
// places: probably in most cases repeated_field.h. clean these up so they all
// get the declaration from this file.
libprotobuf_export int stringspaceusedexcludingself(const string& str);

}  // namespace internal
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_generated_message_util_h__
