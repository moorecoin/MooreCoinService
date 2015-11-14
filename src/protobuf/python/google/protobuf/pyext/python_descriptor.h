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

// author: petar@google.com (petar petrov)

#ifndef google_protobuf_python_descriptor_h__
#define google_protobuf_python_descriptor_h__

#include <python.h>
#include <structmember.h>

#include <google/protobuf/descriptor.h>

#if py_version_hex < 0x02050000 && !defined(py_ssize_t_min)
typedef int py_ssize_t;
#define py_ssize_t_max int_max
#define py_ssize_t_min int_min
#endif

namespace google {
namespace protobuf {
namespace python {

typedef struct {
  pyobject_head

  // the proto2 descriptor that this object represents.
  const google::protobuf::fielddescriptor* descriptor;

  // full name of the field (pystring).
  pyobject* full_name;

  // name of the field (pystring).
  pyobject* name;

  // c++ type of the field (pylong).
  pyobject* cpp_type;

  // name of the field (pylong).
  pyobject* label;

  // identity of the descriptor (pylong used as a poiner).
  pyobject* id;
} cfielddescriptor;

extern pytypeobject cfielddescriptor_type;

extern pytypeobject cdescriptorpool_type;


pyobject* python_newcdescriptorpool(pyobject* ignored, pyobject* args);
pyobject* python_buildfile(pyobject* ignored, pyobject* args);
bool initdescriptor();
google::protobuf::descriptorpool* getdescriptorpool();

}  // namespace python
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_python_descriptor_h__
