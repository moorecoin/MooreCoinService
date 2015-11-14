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

#include <python.h>
#include <string>

#include <google/protobuf/pyext/python_descriptor.h>
#include <google/protobuf/descriptor.pb.h>

#define c(str) const_cast<char*>(str)

namespace google {
namespace protobuf {
namespace python {


static void cfielddescriptordealloc(cfielddescriptor* self);

static google::protobuf::descriptorpool* g_descriptor_pool = null;

static pyobject* cfielddescriptor_getfullname(
    cfielddescriptor* self, void *closure) {
  py_xincref(self->full_name);
  return self->full_name;
}

static pyobject* cfielddescriptor_getname(
    cfielddescriptor *self, void *closure) {
  py_xincref(self->name);
  return self->name;
}

static pyobject* cfielddescriptor_getcpptype(
    cfielddescriptor *self, void *closure) {
  py_xincref(self->cpp_type);
  return self->cpp_type;
}

static pyobject* cfielddescriptor_getlabel(
    cfielddescriptor *self, void *closure) {
  py_xincref(self->label);
  return self->label;
}

static pyobject* cfielddescriptor_getid(
    cfielddescriptor *self, void *closure) {
  py_xincref(self->id);
  return self->id;
}


static pygetsetdef cfielddescriptorgetters[] = {
  { c("full_name"),
    (getter)cfielddescriptor_getfullname, null, "full name", null},
  { c("name"),
    (getter)cfielddescriptor_getname, null, "last name", null},
  { c("cpp_type"),
    (getter)cfielddescriptor_getcpptype, null, "c++ type", null},
  { c("label"),
    (getter)cfielddescriptor_getlabel, null, "label", null},
  { c("id"),
    (getter)cfielddescriptor_getid, null, "id", null},
  {null}
};

pytypeobject cfielddescriptor_type = {
  pyobject_head_init(&pytype_type)
  0,
  c("google.protobuf.internal."
    "_net_proto2___python."
    "cfielddescriptor"),                // tp_name
  sizeof(cfielddescriptor),             // tp_basicsize
  0,                                    // tp_itemsize
  (destructor)cfielddescriptordealloc,  // tp_dealloc
  0,                                    // tp_print
  0,                                    // tp_getattr
  0,                                    // tp_setattr
  0,                                    // tp_compare
  0,                                    // tp_repr
  0,                                    // tp_as_number
  0,                                    // tp_as_sequence
  0,                                    // tp_as_mapping
  0,                                    // tp_hash
  0,                                    // tp_call
  0,                                    // tp_str
  0,                                    // tp_getattro
  0,                                    // tp_setattro
  0,                                    // tp_as_buffer
  py_tpflags_default,                   // tp_flags
  c("a field descriptor"),              // tp_doc
  0,                                    // tp_traverse
  0,                                    // tp_clear
  0,                                    // tp_richcompare
  0,                                    // tp_weaklistoffset
  0,                                    // tp_iter
  0,                                    // tp_iternext
  0,                                    // tp_methods
  0,                                    // tp_members
  cfielddescriptorgetters,              // tp_getset
  0,                                    // tp_base
  0,                                    // tp_dict
  0,                                    // tp_descr_get
  0,                                    // tp_descr_set
  0,                                    // tp_dictoffset
  0,                                    // tp_init
  pytype_genericalloc,                  // tp_alloc
  pytype_genericnew,                    // tp_new
  pyobject_del,                         // tp_free
};

static void cfielddescriptordealloc(cfielddescriptor* self) {
  py_decref(self->full_name);
  py_decref(self->name);
  py_decref(self->cpp_type);
  py_decref(self->label);
  py_decref(self->id);
  self->ob_type->tp_free(reinterpret_cast<pyobject*>(self));
}

typedef struct {
  pyobject_head

  const google::protobuf::descriptorpool* pool;
} cdescriptorpool;

static void cdescriptorpooldealloc(cdescriptorpool* self);

static pyobject* cdescriptorpool_newcdescriptor(
    const google::protobuf::fielddescriptor* field_descriptor) {
  cfielddescriptor* cfield_descriptor = pyobject_new(
      cfielddescriptor, &cfielddescriptor_type);
  if (cfield_descriptor == null) {
    return null;
  }
  cfield_descriptor->descriptor = field_descriptor;

  cfield_descriptor->full_name = pystring_fromstring(
      field_descriptor->full_name().c_str());
  cfield_descriptor->name = pystring_fromstring(
      field_descriptor->name().c_str());
  cfield_descriptor->cpp_type = pylong_fromlong(field_descriptor->cpp_type());
  cfield_descriptor->label = pylong_fromlong(field_descriptor->label());
  cfield_descriptor->id = pylong_fromvoidptr(cfield_descriptor);
  return reinterpret_cast<pyobject*>(cfield_descriptor);
}

static pyobject* cdescriptorpool_findfieldbyname(
    cdescriptorpool* self, pyobject* arg) {
  const char* full_field_name = pystring_asstring(arg);
  if (full_field_name == null) {
    return null;
  }

  const google::protobuf::fielddescriptor* field_descriptor = null;

  field_descriptor = self->pool->findfieldbyname(full_field_name);


  if (field_descriptor == null) {
    pyerr_format(pyexc_typeerror, "couldn't find field %.200s",
                 full_field_name);
    return null;
  }

  return cdescriptorpool_newcdescriptor(field_descriptor);
}

static pyobject* cdescriptorpool_findextensionbyname(
    cdescriptorpool* self, pyobject* arg) {
  const char* full_field_name = pystring_asstring(arg);
  if (full_field_name == null) {
    return null;
  }

  const google::protobuf::fielddescriptor* field_descriptor =
      self->pool->findextensionbyname(full_field_name);
  if (field_descriptor == null) {
    pyerr_format(pyexc_typeerror, "couldn't find field %.200s",
                 full_field_name);
    return null;
  }

  return cdescriptorpool_newcdescriptor(field_descriptor);
}

static pymethoddef cdescriptorpoolmethods[] = {
  { c("findfieldbyname"),
    (pycfunction)cdescriptorpool_findfieldbyname,
    meth_o,
    c("searches for a field descriptor by full name.") },
  { c("findextensionbyname"),
    (pycfunction)cdescriptorpool_findextensionbyname,
    meth_o,
    c("searches for extension descriptor by full name.") },
  {null}
};

pytypeobject cdescriptorpool_type = {
  pyobject_head_init(&pytype_type)
  0,
  c("google.protobuf.internal."
    "_net_proto2___python."
    "cfielddescriptor"),               // tp_name
  sizeof(cdescriptorpool),             // tp_basicsize
  0,                                   // tp_itemsize
  (destructor)cdescriptorpooldealloc,  // tp_dealloc
  0,                                   // tp_print
  0,                                   // tp_getattr
  0,                                   // tp_setattr
  0,                                   // tp_compare
  0,                                   // tp_repr
  0,                                   // tp_as_number
  0,                                   // tp_as_sequence
  0,                                   // tp_as_mapping
  0,                                   // tp_hash
  0,                                   // tp_call
  0,                                   // tp_str
  0,                                   // tp_getattro
  0,                                   // tp_setattro
  0,                                   // tp_as_buffer
  py_tpflags_default,                  // tp_flags
  c("a descriptor pool"),              // tp_doc
  0,                                   // tp_traverse
  0,                                   // tp_clear
  0,                                   // tp_richcompare
  0,                                   // tp_weaklistoffset
  0,                                   // tp_iter
  0,                                   // tp_iternext
  cdescriptorpoolmethods,              // tp_methods
  0,                                   // tp_members
  0,                                   // tp_getset
  0,                                   // tp_base
  0,                                   // tp_dict
  0,                                   // tp_descr_get
  0,                                   // tp_descr_set
  0,                                   // tp_dictoffset
  0,                                   // tp_init
  pytype_genericalloc,                 // tp_alloc
  pytype_genericnew,                   // tp_new
  pyobject_del,                        // tp_free
};

static void cdescriptorpooldealloc(cdescriptorpool* self) {
  self->ob_type->tp_free(reinterpret_cast<pyobject*>(self));
}

google::protobuf::descriptorpool* getdescriptorpool() {
  if (g_descriptor_pool == null) {
    g_descriptor_pool = new google::protobuf::descriptorpool(
        google::protobuf::descriptorpool::generated_pool());
  }
  return g_descriptor_pool;
}

pyobject* python_newcdescriptorpool(pyobject* ignored, pyobject* args) {
  cdescriptorpool* cdescriptor_pool = pyobject_new(
      cdescriptorpool, &cdescriptorpool_type);
  if (cdescriptor_pool == null) {
    return null;
  }
  cdescriptor_pool->pool = getdescriptorpool();
  return reinterpret_cast<pyobject*>(cdescriptor_pool);
}

pyobject* python_buildfile(pyobject* ignored, pyobject* arg) {
  char* message_type;
  py_ssize_t message_len;

  if (pystring_asstringandsize(arg, &message_type, &message_len) < 0) {
    return null;
  }

  google::protobuf::filedescriptorproto file_proto;
  if (!file_proto.parsefromarray(message_type, message_len)) {
    pyerr_setstring(pyexc_typeerror, "couldn't parse file content!");
    return null;
  }

  if (google::protobuf::descriptorpool::generated_pool()->findfilebyname(
      file_proto.name()) != null) {
    py_return_none;
  }

  const google::protobuf::filedescriptor* descriptor = getdescriptorpool()->buildfile(
      file_proto);
  if (descriptor == null) {
    pyerr_setstring(pyexc_typeerror,
                    "couldn't build proto file into descriptor pool!");
    return null;
  }

  py_return_none;
}

bool initdescriptor() {
  cfielddescriptor_type.tp_new = pytype_genericnew;
  if (pytype_ready(&cfielddescriptor_type) < 0)
    return false;

  cdescriptorpool_type.tp_new = pytype_genericnew;
  if (pytype_ready(&cdescriptorpool_type) < 0)
    return false;
  return true;
}

}  // namespace python
}  // namespace protobuf
}  // namespace google
