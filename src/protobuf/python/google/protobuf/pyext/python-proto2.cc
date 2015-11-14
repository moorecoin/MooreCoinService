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
#include <map>
#include <string>
#include <vector>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/pyext/python_descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/message.h>
#include <google/protobuf/unknown_field_set.h>
#include <google/protobuf/pyext/python_protobuf.h>

/* is 64bit */
#define is_64bit (sizeof_long == 8)

#define field_belongs_to_message(field_descriptor, message) \
    ((message)->getdescriptor() == (field_descriptor)->containing_type())

#define field_is_repeated(field_descriptor)                 \
    ((field_descriptor)->label() == google::protobuf::fielddescriptor::label_repeated)

#define google_check_get_int32(arg, value)                         \
    int32 value;                                            \
    if (!checkandgetinteger(arg, &value, kint32min_py, kint32max_py)) { \
      return null;                                          \
    }

#define google_check_get_int64(arg, value)                         \
    int64 value;                                            \
    if (!checkandgetinteger(arg, &value, kint64min_py, kint64max_py)) { \
      return null;                                          \
    }

#define google_check_get_uint32(arg, value)                        \
    uint32 value;                                           \
    if (!checkandgetinteger(arg, &value, kpythonzero, kuint32max_py)) { \
      return null;                                          \
    }

#define google_check_get_uint64(arg, value)                        \
    uint64 value;                                           \
    if (!checkandgetinteger(arg, &value, kpythonzero, kuint64max_py)) { \
      return null;                                          \
    }

#define google_check_get_float(arg, value)                         \
    float value;                                            \
    if (!checkandgetfloat(arg, &value)) {                   \
      return null;                                          \
    }                                                       \

#define google_check_get_double(arg, value)                        \
    double value;                                           \
    if (!checkandgetdouble(arg, &value)) {                  \
      return null;                                          \
    }

#define google_check_get_bool(arg, value)                          \
    bool value;                                             \
    if (!checkandgetbool(arg, &value)) {                    \
      return null;                                          \
    }

#define c(str) const_cast<char*>(str)

// --- globals:

// constants used for integer type range checking.
static pyobject* kpythonzero;
static pyobject* kint32min_py;
static pyobject* kint32max_py;
static pyobject* kuint32max_py;
static pyobject* kint64min_py;
static pyobject* kint64max_py;
static pyobject* kuint64max_py;

namespace google {
namespace protobuf {
namespace python {

// --- support routines:

static void addconstants(pyobject* module) {
  struct namevalue {
    char* name;
    int32 value;
  } constants[] = {
    // labels:
    {"label_optional", google::protobuf::fielddescriptor::label_optional},
    {"label_required", google::protobuf::fielddescriptor::label_required},
    {"label_repeated", google::protobuf::fielddescriptor::label_repeated},
    // cpp types:
    {"cpptype_message", google::protobuf::fielddescriptor::cpptype_message},
    // field types:
    {"type_message", google::protobuf::fielddescriptor::type_message},
    // end.
    {null, 0}
  };

  for (namevalue* constant = constants;
       constant->name != null; constant++) {
    pymodule_addintconstant(module, constant->name, constant->value);
  }
}

// --- cmessage custom type:

// ------ type forward declaration:

struct cmessage;
struct cmessage_type;

static void cmessagedealloc(cmessage* self);
static int cmessageinit(cmessage* self, pyobject *args, pyobject *kwds);
static pyobject* cmessagestr(cmessage* self);

static pyobject* cmessage_addmessage(cmessage* self, pyobject* args);
static pyobject* cmessage_addrepeatedscalar(cmessage* self, pyobject* args);
static pyobject* cmessage_assignrepeatedscalar(cmessage* self, pyobject* args);
static pyobject* cmessage_bytesize(cmessage* self, pyobject* args);
static pyobject* cmessage_clear(cmessage* self, pyobject* args);
static pyobject* cmessage_clearfield(cmessage* self, pyobject* args);
static pyobject* cmessage_clearfieldbydescriptor(
    cmessage* self, pyobject* args);
static pyobject* cmessage_copyfrom(cmessage* self, pyobject* args);
static pyobject* cmessage_debugstring(cmessage* self, pyobject* args);
static pyobject* cmessage_deleterepeatedfield(cmessage* self, pyobject* args);
static pyobject* cmessage_equals(cmessage* self, pyobject* args);
static pyobject* cmessage_fieldlength(cmessage* self, pyobject* args);
static pyobject* cmessage_findinitializationerrors(cmessage* self);
static pyobject* cmessage_getrepeatedmessage(cmessage* self, pyobject* args);
static pyobject* cmessage_getrepeatedscalar(cmessage* self, pyobject* args);
static pyobject* cmessage_getscalar(cmessage* self, pyobject* args);
static pyobject* cmessage_hasfield(cmessage* self, pyobject* args);
static pyobject* cmessage_hasfieldbydescriptor(cmessage* self, pyobject* args);
static pyobject* cmessage_isinitialized(cmessage* self, pyobject* args);
static pyobject* cmessage_listfields(cmessage* self, pyobject* args);
static pyobject* cmessage_mergefrom(cmessage* self, pyobject* args);
static pyobject* cmessage_mergefromstring(cmessage* self, pyobject* args);
static pyobject* cmessage_mutablemessage(cmessage* self, pyobject* args);
static pyobject* cmessage_newsubmessage(cmessage* self, pyobject* args);
static pyobject* cmessage_setscalar(cmessage* self, pyobject* args);
static pyobject* cmessage_serializepartialtostring(
    cmessage* self, pyobject* args);
static pyobject* cmessage_serializetostring(cmessage* self, pyobject* args);
static pyobject* cmessage_setinparent(cmessage* self, pyobject* args);
static pyobject* cmessage_swaprepeatedfieldelements(
    cmessage* self, pyobject* args);

// ------ object definition:

typedef struct cmessage {
  pyobject_head

  struct cmessage* parent;  // null if wasn't created from another message.
  cfielddescriptor* parent_field;
  const char* full_name;
  google::protobuf::message* message;
  bool free_message;
  bool read_only;
} cmessage;

// ------ method table:

#define cmethod(name, args, doc)   \
  { c(#name), (pycfunction)cmessage_##name, args, c(doc) }
static pymethoddef cmessagemethods[] = {
  cmethod(addmessage, meth_o,
          "adds a new message to a repeated composite field."),
  cmethod(addrepeatedscalar, meth_varargs,
          "adds a scalar to a repeated scalar field."),
  cmethod(assignrepeatedscalar, meth_varargs,
          "clears and sets the values of a repeated scalar field."),
  cmethod(bytesize, meth_noargs,
          "returns the size of the message in bytes."),
  cmethod(clear, meth_o,
          "clears a protocol message."),
  cmethod(clearfield, meth_varargs,
          "clears a protocol message field by name."),
  cmethod(clearfieldbydescriptor, meth_o,
          "clears a protocol message field by descriptor."),
  cmethod(copyfrom, meth_o,
          "copies a protocol message into the current message."),
  cmethod(debugstring, meth_noargs,
          "returns the debug string of a protocol message."),
  cmethod(deleterepeatedfield, meth_varargs,
          "deletes a slice of values from a repeated field."),
  cmethod(equals, meth_o,
          "checks if two protocol messages are equal (by identity)."),
  cmethod(fieldlength, meth_o,
          "returns the number of elements in a repeated field."),
  cmethod(findinitializationerrors, meth_noargs,
          "returns the initialization errors of a message."),
  cmethod(getrepeatedmessage, meth_varargs,
          "returns a message from a repeated composite field."),
  cmethod(getrepeatedscalar, meth_varargs,
          "returns a scalar value from a repeated scalar field."),
  cmethod(getscalar, meth_o,
          "returns the scalar value of a field."),
  cmethod(hasfield, meth_o,
          "checks if a message field is set."),
  cmethod(hasfieldbydescriptor, meth_o,
          "checks if a message field is set by given its descriptor"),
  cmethod(isinitialized, meth_noargs,
          "checks if all required fields of a protocol message are set."),
  cmethod(listfields, meth_noargs,
          "lists all set fields of a message."),
  cmethod(mergefrom, meth_o,
          "merges a protocol message into the current message."),
  cmethod(mergefromstring, meth_o,
          "merges a serialized message into the current message."),
  cmethod(mutablemessage, meth_o,
          "returns a new instance of a nested protocol message."),
  cmethod(newsubmessage, meth_o,
          "creates and returns a python message given the descriptor of a "
          "composite field of the current message."),
  cmethod(setscalar, meth_varargs,
          "sets the value of a singular scalar field."),
  cmethod(serializepartialtostring, meth_varargs,
          "serializes the message to a string, even if it isn't initialized."),
  cmethod(serializetostring, meth_noargs,
          "serializes the message to a string, only for initialized messages."),
  cmethod(setinparent, meth_noargs,
          "sets the has bit of the given field in its parent message."),
  cmethod(swaprepeatedfieldelements, meth_varargs,
          "swaps the elements in two positions in a repeated field."),
  { null, null }
};
#undef cmethod

static pymemberdef cmessagemembers[] = {
  { c("full_name"), t_string, offsetof(cmessage, full_name), 0, "full name" },
  { null }
};

// ------ type definition:

// the definition for the type object that captures the type of cmessage
// in python.
pytypeobject cmessage_type = {
  pyobject_head_init(&pytype_type)
  0,
  c("google.protobuf.internal."
    "_net_proto2___python."
    "cmessage"),                       // tp_name
  sizeof(cmessage),                    //  tp_basicsize
  0,                                   //  tp_itemsize
  (destructor)cmessagedealloc,         //  tp_dealloc
  0,                                   //  tp_print
  0,                                   //  tp_getattr
  0,                                   //  tp_setattr
  0,                                   //  tp_compare
  0,                                   //  tp_repr
  0,                                   //  tp_as_number
  0,                                   //  tp_as_sequence
  0,                                   //  tp_as_mapping
  0,                                   //  tp_hash
  0,                                   //  tp_call
  (reprfunc)cmessagestr,               //  tp_str
  0,                                   //  tp_getattro
  0,                                   //  tp_setattro
  0,                                   //  tp_as_buffer
  py_tpflags_default,                  //  tp_flags
  c("a protocolmessage"),              //  tp_doc
  0,                                   //  tp_traverse
  0,                                   //  tp_clear
  0,                                   //  tp_richcompare
  0,                                   //  tp_weaklistoffset
  0,                                   //  tp_iter
  0,                                   //  tp_iternext
  cmessagemethods,                     //  tp_methods
  cmessagemembers,                     //  tp_members
  0,                                   //  tp_getset
  0,                                   //  tp_base
  0,                                   //  tp_dict
  0,                                   //  tp_descr_get
  0,                                   //  tp_descr_set
  0,                                   //  tp_dictoffset
  (initproc)cmessageinit,              //  tp_init
  pytype_genericalloc,                 //  tp_alloc
  pytype_genericnew,                   //  tp_new
  pyobject_del,                        //  tp_free
};

// ------ helper functions:

static void formattypeerror(pyobject* arg, char* expected_types) {
  pyobject* repr = pyobject_repr(arg);
  pyerr_format(pyexc_typeerror,
               "%.100s has type %.100s, but expected one of: %s",
               pystring_as_string(repr),
               arg->ob_type->tp_name,
               expected_types);
  py_decref(repr);
}

template <class t>
static bool checkandgetinteger(
    pyobject* arg, t* value, pyobject* min, pyobject* max) {
  bool is_long = pylong_check(arg);
  if (!pyint_check(arg) && !is_long) {
    formattypeerror(arg, "int, long");
    return false;
  }

  if (pyobject_compare(min, arg) > 0 || pyobject_compare(max, arg) < 0) {
    pyobject* s = pyobject_str(arg);
    pyerr_format(pyexc_valueerror,
                 "value out of range: %s",
                 pystring_as_string(s));
    py_decref(s);
    return false;
  }
  if (is_long) {
    if (min == kpythonzero) {
      *value = static_cast<t>(pylong_asunsignedlonglong(arg));
    } else {
      *value = static_cast<t>(pylong_aslonglong(arg));
    }
  } else {
    *value = static_cast<t>(pyint_aslong(arg));
  }
  return true;
}

static bool checkandgetdouble(pyobject* arg, double* value) {
  if (!pyint_check(arg) && !pylong_check(arg) &&
      !pyfloat_check(arg)) {
    formattypeerror(arg, "int, long, float");
    return false;
  }
  *value = pyfloat_asdouble(arg);
  return true;
}

static bool checkandgetfloat(pyobject* arg, float* value) {
  double double_value;
  if (!checkandgetdouble(arg, &double_value)) {
    return false;
  }
  *value = static_cast<float>(double_value);
  return true;
}

static bool checkandgetbool(pyobject* arg, bool* value) {
  if (!pyint_check(arg) && !pybool_check(arg) && !pylong_check(arg)) {
    formattypeerror(arg, "int, long, bool");
    return false;
  }
  *value = static_cast<bool>(pyint_aslong(arg));
  return true;
}

google::protobuf::dynamicmessagefactory* global_message_factory = null;
static const google::protobuf::message* createmessage(const char* message_type) {
  string message_name(message_type);
  const google::protobuf::descriptor* descriptor =
      getdescriptorpool()->findmessagetypebyname(message_name);
  if (descriptor == null) {
    return null;
  }
  return global_message_factory->getprototype(descriptor);
}

static void releasesubmessage(google::protobuf::message* message,
                           const google::protobuf::fielddescriptor* field_descriptor,
                           cmessage* child_cmessage) {
  message* released_message = message->getreflection()->releasemessage(
      message, field_descriptor, global_message_factory);
  google_dcheck(child_cmessage->message != null);
  // releasemessage will return null which differs from
  // child_cmessage->message, if the field does not exist.  in this case,
  // the latter points to the default instance via a const_cast<>, so we
  // have to reset it to a new mutable object since we are taking ownership.
  if (released_message == null) {
    const message* prototype = global_message_factory->getprototype(
        child_cmessage->message->getdescriptor());
    google_dcheck(prototype != null);
    child_cmessage->message = prototype->new();
  }
  child_cmessage->parent = null;
  child_cmessage->parent_field = null;
  child_cmessage->free_message = true;
  child_cmessage->read_only = false;
}

static bool checkandsetstring(
    pyobject* arg, google::protobuf::message* message,
    const google::protobuf::fielddescriptor* descriptor,
    const google::protobuf::reflection* reflection,
    bool append,
    int index) {
  google_dcheck(descriptor->type() == google::protobuf::fielddescriptor::type_string ||
         descriptor->type() == google::protobuf::fielddescriptor::type_bytes);
  if (descriptor->type() == google::protobuf::fielddescriptor::type_string) {
    if (!pystring_check(arg) && !pyunicode_check(arg)) {
      formattypeerror(arg, "str, unicode");
      return false;
    }

    if (pystring_check(arg)) {
      pyobject* unicode = pyunicode_fromencodedobject(arg, "ascii", null);
      if (unicode == null) {
        pyobject* repr = pyobject_repr(arg);
        pyerr_format(pyexc_valueerror,
                     "%s has type str, but isn't in 7-bit ascii "
                     "encoding. non-ascii strings must be converted to "
                     "unicode objects before being added.",
                     pystring_as_string(repr));
        py_decref(repr);
        return false;
      } else {
        py_decref(unicode);
      }
    }
  } else if (!pystring_check(arg)) {
    formattypeerror(arg, "str");
    return false;
  }

  pyobject* encoded_string = null;
  if (descriptor->type() == google::protobuf::fielddescriptor::type_string) {
    if (pystring_check(arg)) {
      encoded_string = pystring_asencodedobject(arg, "utf-8", null);
    } else {
      encoded_string = pyunicode_asencodedobject(arg, "utf-8", null);
    }
  } else {
    // in this case field type is "bytes".
    encoded_string = arg;
    py_incref(encoded_string);
  }

  if (encoded_string == null) {
    return false;
  }

  char* value;
  py_ssize_t value_len;
  if (pystring_asstringandsize(encoded_string, &value, &value_len) < 0) {
    py_decref(encoded_string);
    return false;
  }

  string value_string(value, value_len);
  if (append) {
    reflection->addstring(message, descriptor, value_string);
  } else if (index < 0) {
    reflection->setstring(message, descriptor, value_string);
  } else {
    reflection->setrepeatedstring(message, descriptor, index, value_string);
  }
  py_decref(encoded_string);
  return true;
}

static pyobject* tostringobject(
    const google::protobuf::fielddescriptor* descriptor, string value) {
  if (descriptor->type() != google::protobuf::fielddescriptor::type_string) {
    return pystring_fromstringandsize(value.c_str(), value.length());
  }

  pyobject* result = pyunicode_decodeutf8(value.c_str(), value.length(), null);
  // if the string can't be decoded in utf-8, just return a string object that
  // contains the raw bytes. this can't happen if the value was assigned using
  // the members of the python message object, but can happen if the values were
  // parsed from the wire (binary).
  if (result == null) {
    pyerr_clear();
    result = pystring_fromstringandsize(value.c_str(), value.length());
  }
  return result;
}

static void assurewritable(cmessage* self) {
  if (self == null ||
      self->parent == null ||
      self->parent_field == null) {
    return;
  }

  if (!self->read_only) {
    return;
  }

  assurewritable(self->parent);

  google::protobuf::message* message = self->parent->message;
  const google::protobuf::reflection* reflection = message->getreflection();
  self->message = reflection->mutablemessage(
      message, self->parent_field->descriptor, global_message_factory);
  self->read_only = false;
}

static pyobject* internalgetscalar(
    google::protobuf::message* message,
    const google::protobuf::fielddescriptor* field_descriptor) {
  const google::protobuf::reflection* reflection = message->getreflection();

  if (!field_belongs_to_message(field_descriptor, message)) {
    pyerr_setstring(
        pyexc_keyerror, "field does not belong to message!");
    return null;
  }

  pyobject* result = null;
  switch (field_descriptor->cpp_type()) {
    case google::protobuf::fielddescriptor::cpptype_int32: {
      int32 value = reflection->getint32(*message, field_descriptor);
      result = pyint_fromlong(value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_int64: {
      int64 value = reflection->getint64(*message, field_descriptor);
#if is_64bit
      result = pyint_fromlong(value);
#else
      result = pylong_fromlonglong(value);
#endif
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_uint32: {
      uint32 value = reflection->getuint32(*message, field_descriptor);
#if is_64bit
      result = pyint_fromlong(value);
#else
      result = pylong_fromlonglong(value);
#endif
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_uint64: {
      uint64 value = reflection->getuint64(*message, field_descriptor);
#if is_64bit
      if (value <= static_cast<uint64>(kint64max)) {
        result = pyint_fromlong(static_cast<uint64>(value));
      }
#else
      if (value <= static_cast<uint32>(kint32max)) {
        result = pyint_fromlong(static_cast<uint32>(value));
      }
#endif
      else {  // nolint
        result = pylong_fromunsignedlonglong(value);
      }
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_float: {
      float value = reflection->getfloat(*message, field_descriptor);
      result = pyfloat_fromdouble(value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_double: {
      double value = reflection->getdouble(*message, field_descriptor);
      result = pyfloat_fromdouble(value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_bool: {
      bool value = reflection->getbool(*message, field_descriptor);
      result = pybool_fromlong(value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_string: {
      string value = reflection->getstring(*message, field_descriptor);
      result = tostringobject(field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_enum: {
      if (!message->getreflection()->hasfield(*message, field_descriptor)) {
        // look for the value in the unknown fields.
        google::protobuf::unknownfieldset* unknown_field_set =
            message->getreflection()->mutableunknownfields(message);
        for (int i = 0; i < unknown_field_set->field_count(); ++i) {
          if (unknown_field_set->field(i).number() ==
              field_descriptor->number()) {
            result = pyint_fromlong(unknown_field_set->field(i).varint());
            break;
          }
        }
      }

      if (result == null) {
        const google::protobuf::enumvaluedescriptor* enum_value =
            message->getreflection()->getenum(*message, field_descriptor);
        result = pyint_fromlong(enum_value->number());
      }
      break;
    }
    default:
      pyerr_format(
          pyexc_systemerror, "getting a value from a field of unknown type %d",
          field_descriptor->cpp_type());
  }

  return result;
}

static pyobject* internalsetscalar(
    google::protobuf::message* message, const google::protobuf::fielddescriptor* field_descriptor,
    pyobject* arg) {
  const google::protobuf::reflection* reflection = message->getreflection();

  if (!field_belongs_to_message(field_descriptor, message)) {
    pyerr_setstring(
        pyexc_keyerror, "field does not belong to message!");
    return null;
  }

  switch (field_descriptor->cpp_type()) {
    case google::protobuf::fielddescriptor::cpptype_int32: {
      google_check_get_int32(arg, value);
      reflection->setint32(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_int64: {
      google_check_get_int64(arg, value);
      reflection->setint64(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_uint32: {
      google_check_get_uint32(arg, value);
      reflection->setuint32(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_uint64: {
      google_check_get_uint64(arg, value);
      reflection->setuint64(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_float: {
      google_check_get_float(arg, value);
      reflection->setfloat(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_double: {
      google_check_get_double(arg, value);
      reflection->setdouble(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_bool: {
      google_check_get_bool(arg, value);
      reflection->setbool(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_string: {
      if (!checkandsetstring(
          arg, message, field_descriptor, reflection, false, -1)) {
        return null;
      }
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_enum: {
      google_check_get_int32(arg, value);
      const google::protobuf::enumdescriptor* enum_descriptor =
          field_descriptor->enum_type();
      const google::protobuf::enumvaluedescriptor* enum_value =
          enum_descriptor->findvaluebynumber(value);
      if (enum_value != null) {
        reflection->setenum(message, field_descriptor, enum_value);
      } else {
        bool added = false;
        // add the value to the unknown fields.
        google::protobuf::unknownfieldset* unknown_field_set =
            message->getreflection()->mutableunknownfields(message);
        for (int i = 0; i < unknown_field_set->field_count(); ++i) {
          if (unknown_field_set->field(i).number() ==
              field_descriptor->number()) {
            unknown_field_set->mutable_field(i)->set_varint(value);
            added = true;
            break;
          }
        }

        if (!added) {
          unknown_field_set->addvarint(field_descriptor->number(), value);
        }
        reflection->clearfield(message, field_descriptor);
      }
      break;
    }
    default:
      pyerr_format(
          pyexc_systemerror, "setting value to a field of unknown type %d",
          field_descriptor->cpp_type());
  }

  py_return_none;
}

static pyobject* internaladdrepeatedscalar(
    google::protobuf::message* message, const google::protobuf::fielddescriptor* field_descriptor,
    pyobject* arg) {

  if (!field_belongs_to_message(field_descriptor, message)) {
    pyerr_setstring(
        pyexc_keyerror, "field does not belong to message!");
    return null;
  }

  const google::protobuf::reflection* reflection = message->getreflection();
  switch (field_descriptor->cpp_type()) {
    case google::protobuf::fielddescriptor::cpptype_int32: {
      google_check_get_int32(arg, value);
      reflection->addint32(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_int64: {
      google_check_get_int64(arg, value);
      reflection->addint64(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_uint32: {
      google_check_get_uint32(arg, value);
      reflection->adduint32(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_uint64: {
      google_check_get_uint64(arg, value);
      reflection->adduint64(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_float: {
      google_check_get_float(arg, value);
      reflection->addfloat(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_double: {
      google_check_get_double(arg, value);
      reflection->adddouble(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_bool: {
      google_check_get_bool(arg, value);
      reflection->addbool(message, field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_string: {
      if (!checkandsetstring(
          arg, message, field_descriptor, reflection, true, -1)) {
        return null;
      }
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_enum: {
      google_check_get_int32(arg, value);
      const google::protobuf::enumdescriptor* enum_descriptor =
          field_descriptor->enum_type();
      const google::protobuf::enumvaluedescriptor* enum_value =
          enum_descriptor->findvaluebynumber(value);
      if (enum_value != null) {
        reflection->addenum(message, field_descriptor, enum_value);
      } else {
        pyobject* s = pyobject_str(arg);
        pyerr_format(pyexc_valueerror, "unknown enum value: %s",
                     pystring_as_string(s));
        py_decref(s);
        return null;
      }
      break;
    }
    default:
      pyerr_format(
          pyexc_systemerror, "adding value to a field of unknown type %d",
          field_descriptor->cpp_type());
  }

  py_return_none;
}

static pyobject* internalgetrepeatedscalar(
    cmessage* cmessage, const google::protobuf::fielddescriptor* field_descriptor,
    int index) {
  google::protobuf::message* message = cmessage->message;
  const google::protobuf::reflection* reflection = message->getreflection();

  int field_size = reflection->fieldsize(*message, field_descriptor);
  if (index < 0) {
    index = field_size + index;
  }
  if (index < 0 || index >= field_size) {
    pyerr_format(pyexc_indexerror,
                 "list assignment index (%d) out of range", index);
    return null;
  }

  pyobject* result = null;
  switch (field_descriptor->cpp_type()) {
    case google::protobuf::fielddescriptor::cpptype_int32: {
      int32 value = reflection->getrepeatedint32(
          *message, field_descriptor, index);
      result = pyint_fromlong(value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_int64: {
      int64 value = reflection->getrepeatedint64(
          *message, field_descriptor, index);
      result = pylong_fromlonglong(value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_uint32: {
      uint32 value = reflection->getrepeateduint32(
          *message, field_descriptor, index);
      result = pylong_fromlonglong(value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_uint64: {
      uint64 value = reflection->getrepeateduint64(
          *message, field_descriptor, index);
      result = pylong_fromunsignedlonglong(value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_float: {
      float value = reflection->getrepeatedfloat(
          *message, field_descriptor, index);
      result = pyfloat_fromdouble(value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_double: {
      double value = reflection->getrepeateddouble(
          *message, field_descriptor, index);
      result = pyfloat_fromdouble(value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_bool: {
      bool value = reflection->getrepeatedbool(
          *message, field_descriptor, index);
      result = pybool_fromlong(value ? 1 : 0);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_enum: {
      const google::protobuf::enumvaluedescriptor* enum_value =
          message->getreflection()->getrepeatedenum(
              *message, field_descriptor, index);
      result = pyint_fromlong(enum_value->number());
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_string: {
      string value = reflection->getrepeatedstring(
          *message, field_descriptor, index);
      result = tostringobject(field_descriptor, value);
      break;
    }
    case google::protobuf::fielddescriptor::cpptype_message: {
      cmessage* py_cmsg = pyobject_new(cmessage, &cmessage_type);
      if (py_cmsg == null) {
        return null;
      }
      const google::protobuf::message& msg = reflection->getrepeatedmessage(
          *message, field_descriptor, index);
      py_cmsg->parent = cmessage;
      py_cmsg->full_name = field_descriptor->full_name().c_str();
      py_cmsg->message = const_cast<google::protobuf::message*>(&msg);
      py_cmsg->free_message = false;
      py_cmsg->read_only = false;
      result = reinterpret_cast<pyobject*>(py_cmsg);
      break;
    }
    default:
      pyerr_format(
          pyexc_systemerror,
          "getting value from a repeated field of unknown type %d",
          field_descriptor->cpp_type());
  }

  return result;
}

static pyobject* internalgetrepeatedscalarslice(
    cmessage* cmessage, const google::protobuf::fielddescriptor* field_descriptor,
    pyobject* slice) {
  py_ssize_t from;
  py_ssize_t to;
  py_ssize_t step;
  py_ssize_t length;
  bool return_list = false;
  google::protobuf::message* message = cmessage->message;

  if (pyint_check(slice)) {
    from = to = pyint_aslong(slice);
  } else if (pylong_check(slice)) {
    from = to = pylong_aslong(slice);
  } else if (pyslice_check(slice)) {
    const google::protobuf::reflection* reflection = message->getreflection();
    length = reflection->fieldsize(*message, field_descriptor);
    pyslice_getindices(
        reinterpret_cast<pysliceobject*>(slice), length, &from, &to, &step);
    return_list = true;
  } else {
    pyerr_setstring(pyexc_typeerror, "list indices must be integers");
    return null;
  }

  if (!return_list) {
    return internalgetrepeatedscalar(cmessage, field_descriptor, from);
  }

  pyobject* list = pylist_new(0);
  if (list == null) {
    return null;
  }

  if (from <= to) {
    if (step < 0) return list;
    for (py_ssize_t index = from; index < to; index += step) {
      if (index < 0 || index >= length) break;
      pyobject* s = internalgetrepeatedscalar(
          cmessage, field_descriptor, index);
      pylist_append(list, s);
      py_decref(s);
    }
  } else {
    if (step > 0) return list;
    for (py_ssize_t index = from; index > to; index += step) {
      if (index < 0 || index >= length) break;
      pyobject* s = internalgetrepeatedscalar(
          cmessage, field_descriptor, index);
      pylist_append(list, s);
      py_decref(s);
    }
  }
  return list;
}

// ------ c constructor/destructor:

static int cmessageinit(cmessage* self, pyobject *args, pyobject *kwds) {
  self->message = null;
  return 0;
}

static void cmessagedealloc(cmessage* self) {
  if (self->free_message) {
    if (self->read_only) {
      pyerr_writeunraisable(reinterpret_cast<pyobject*>(self));
    }
    delete self->message;
  }
  self->ob_type->tp_free(reinterpret_cast<pyobject*>(self));
}

// ------ methods:

static pyobject* cmessage_clear(cmessage* self, pyobject* arg) {
  assurewritable(self);
  google::protobuf::message* message = self->message;

  // this block of code is equivalent to the following:
  // for cfield_descriptor, child_cmessage in arg:
  //   releasesubmessage(cfield_descriptor, child_cmessage)
  if (!pylist_check(arg)) {
    pyerr_setstring(pyexc_typeerror, "must be a list");
    return null;
  }
  pyobject* messages_to_clear = arg;
  py_ssize_t num_messages_to_clear = pylist_get_size(messages_to_clear);
  for(int i = 0; i < num_messages_to_clear; ++i) {
    pyobject* message_tuple = pylist_get_item(messages_to_clear, i);
    if (!pytuple_check(message_tuple) || pytuple_get_size(message_tuple) != 2) {
      pyerr_setstring(pyexc_typeerror, "must be a tuple of size 2");
      return null;
    }

    pyobject* py_cfield_descriptor = pytuple_get_item(message_tuple, 0);
    pyobject* py_child_cmessage = pytuple_get_item(message_tuple, 1);
    if (!pyobject_typecheck(py_cfield_descriptor, &cfielddescriptor_type) ||
        !pyobject_typecheck(py_child_cmessage, &cmessage_type)) {
      pyerr_setstring(pyexc_valueerror, "invalid tuple");
      return null;
    }

    cfielddescriptor* cfield_descriptor = reinterpret_cast<cfielddescriptor *>(
        py_cfield_descriptor);
    cmessage* child_cmessage = reinterpret_cast<cmessage *>(py_child_cmessage);
    releasesubmessage(message, cfield_descriptor->descriptor, child_cmessage);
  }

  message->clear();
  py_return_none;
}

static pyobject* cmessage_isinitialized(cmessage* self, pyobject* args) {
  return pybool_fromlong(self->message->isinitialized() ? 1 : 0);
}

static pyobject* cmessage_hasfield(cmessage* self, pyobject* arg) {
  char* field_name;
  if (pystring_asstringandsize(arg, &field_name, null) < 0) {
    return null;
  }

  google::protobuf::message* message = self->message;
  const google::protobuf::descriptor* descriptor = message->getdescriptor();
  const google::protobuf::fielddescriptor* field_descriptor =
      descriptor->findfieldbyname(field_name);
  if (field_descriptor == null) {
    pyerr_format(pyexc_valueerror, "unknown field %s.", field_name);
    return null;
  }

  bool has_field =
      message->getreflection()->hasfield(*message, field_descriptor);
  return pybool_fromlong(has_field ? 1 : 0);
}

static pyobject* cmessage_hasfieldbydescriptor(cmessage* self, pyobject* arg) {
  cfielddescriptor* cfield_descriptor = null;
  if (!pyobject_typecheck(reinterpret_cast<pyobject *>(arg),
                          &cfielddescriptor_type)) {
    pyerr_setstring(pyexc_typeerror, "must be a field descriptor");
    return null;
  }
  cfield_descriptor = reinterpret_cast<cfielddescriptor*>(arg);

  google::protobuf::message* message = self->message;
  const google::protobuf::fielddescriptor* field_descriptor =
      cfield_descriptor->descriptor;

  if (!field_belongs_to_message(field_descriptor, message)) {
    pyerr_setstring(pyexc_keyerror,
                    "field does not belong to message!");
    return null;
  }

  if (field_is_repeated(field_descriptor)) {
    pyerr_setstring(pyexc_keyerror,
                    "field is repeated. a singular method is required.");
    return null;
  }

  bool has_field =
      message->getreflection()->hasfield(*message, field_descriptor);
  return pybool_fromlong(has_field ? 1 : 0);
}

static pyobject* cmessage_clearfieldbydescriptor(
    cmessage* self, pyobject* arg) {
  cfielddescriptor* cfield_descriptor = null;
  if (!pyobject_typecheck(reinterpret_cast<pyobject *>(arg),
                          &cfielddescriptor_type)) {
    pyerr_setstring(pyexc_typeerror, "must be a field descriptor");
    return null;
  }
  cfield_descriptor = reinterpret_cast<cfielddescriptor*>(arg);

  google::protobuf::message* message = self->message;
  const google::protobuf::fielddescriptor* field_descriptor =
      cfield_descriptor->descriptor;

  if (!field_belongs_to_message(field_descriptor, message)) {
    pyerr_setstring(pyexc_keyerror,
                    "field does not belong to message!");
    return null;
  }

  message->getreflection()->clearfield(message, field_descriptor);
  py_return_none;
}

static pyobject* cmessage_clearfield(cmessage* self, pyobject* args) {
  char* field_name;
  cmessage* child_cmessage = null;
  if (!pyarg_parsetuple(args, c("s|o!:clearfield"), &field_name,
                        &cmessage_type, &child_cmessage)) {
    return null;
  }

  google::protobuf::message* message = self->message;
  const google::protobuf::descriptor* descriptor = message->getdescriptor();
  const google::protobuf::fielddescriptor* field_descriptor =
      descriptor->findfieldbyname(field_name);
  if (field_descriptor == null) {
    pyerr_format(pyexc_valueerror, "unknown field %s.", field_name);
    return null;
  }

  if (child_cmessage != null && !field_is_repeated(field_descriptor)) {
    releasesubmessage(message, field_descriptor, child_cmessage);
  } else {
    message->getreflection()->clearfield(message, field_descriptor);
  }
  py_return_none;
}

static pyobject* cmessage_getscalar(cmessage* self, pyobject* arg) {
  cfielddescriptor* cdescriptor = null;
  if (!pyobject_typecheck(reinterpret_cast<pyobject *>(arg),
                          &cfielddescriptor_type)) {
    pyerr_setstring(pyexc_typeerror, "must be a field descriptor");
    return null;
  }
  cdescriptor = reinterpret_cast<cfielddescriptor*>(arg);

  google::protobuf::message* message = self->message;
  return internalgetscalar(message, cdescriptor->descriptor);
}

static pyobject* cmessage_getrepeatedscalar(cmessage* self, pyobject* args) {
  cfielddescriptor* cfield_descriptor;
  pyobject* slice;
  if (!pyarg_parsetuple(args, c("o!o:getrepeatedscalar"),
                        &cfielddescriptor_type, &cfield_descriptor, &slice)) {
    return null;
  }

  return internalgetrepeatedscalarslice(
      self, cfield_descriptor->descriptor, slice);
}

static pyobject* cmessage_assignrepeatedscalar(cmessage* self, pyobject* args) {
  cfielddescriptor* cfield_descriptor;
  pyobject* slice;
  if (!pyarg_parsetuple(args, c("o!o:assignrepeatedscalar"),
                        &cfielddescriptor_type, &cfield_descriptor, &slice)) {
    return null;
  }

  assurewritable(self);
  google::protobuf::message* message = self->message;
  message->getreflection()->clearfield(message, cfield_descriptor->descriptor);

  pyobject* iter = pyobject_getiter(slice);
  pyobject* next;
  while ((next = pyiter_next(iter)) != null) {
    if (internaladdrepeatedscalar(
            message, cfield_descriptor->descriptor, next) == null) {
      py_decref(next);
      py_decref(iter);
      return null;
    }
    py_decref(next);
  }
  py_decref(iter);
  py_return_none;
}

static pyobject* cmessage_deleterepeatedfield(cmessage* self, pyobject* args) {
  cfielddescriptor* cfield_descriptor;
  pyobject* slice;
  if (!pyarg_parsetuple(args, c("o!o:deleterepeatedfield"),
                        &cfielddescriptor_type, &cfield_descriptor, &slice)) {
    return null;
  }
  assurewritable(self);

  py_ssize_t length, from, to, step, slice_length;
  google::protobuf::message* message = self->message;
  const google::protobuf::fielddescriptor* field_descriptor =
      cfield_descriptor->descriptor;
  const google::protobuf::reflection* reflection = message->getreflection();
  int min, max;
  length = reflection->fieldsize(*message, field_descriptor);

  if (pyint_check(slice) || pylong_check(slice)) {
    from = to = pylong_aslong(slice);
    if (from < 0) {
      from = to = length + from;
    }
    step = 1;
    min = max = from;

    // range check.
    if (from < 0 || from >= length) {
      pyerr_format(pyexc_indexerror, "list assignment index out of range");
      return null;
    }
  } else if (pyslice_check(slice)) {
    from = to = step = slice_length = 0;
    pyslice_getindicesex(
        reinterpret_cast<pysliceobject*>(slice),
        length, &from, &to, &step, &slice_length);
    if (from < to) {
      min = from;
      max = to - 1;
    } else {
      min = to + 1;
      max = from;
    }
  } else {
    pyerr_setstring(pyexc_typeerror, "list indices must be integers");
    return null;
  }

  py_ssize_t i = from;
  std::vector<bool> to_delete(length, false);
  while (i >= min && i <= max) {
    to_delete[i] = true;
    i += step;
  }

  to = 0;
  for (i = 0; i < length; ++i) {
    if (!to_delete[i]) {
      if (i != to) {
        reflection->swapelements(message, field_descriptor, i, to);
      }
      ++to;
    }
  }

  while (i > to) {
    reflection->removelast(message, field_descriptor);
    --i;
  }

  py_return_none;
}


static pyobject* cmessage_setscalar(cmessage* self, pyobject* args) {
  cfielddescriptor* cfield_descriptor;
  pyobject* arg;
  if (!pyarg_parsetuple(args, c("o!o:setscalar"),
                        &cfielddescriptor_type, &cfield_descriptor, &arg)) {
    return null;
  }
  assurewritable(self);

  return internalsetscalar(self->message, cfield_descriptor->descriptor, arg);
}

static pyobject* cmessage_addrepeatedscalar(cmessage* self, pyobject* args) {
  cfielddescriptor* cfield_descriptor;
  pyobject* value;
  if (!pyarg_parsetuple(args, c("o!o:addrepeatedscalar"),
                        &cfielddescriptor_type, &cfield_descriptor, &value)) {
    return null;
  }
  assurewritable(self);

  return internaladdrepeatedscalar(
      self->message, cfield_descriptor->descriptor, value);
}

static pyobject* cmessage_fieldlength(cmessage* self, pyobject* arg) {
  cfielddescriptor* cfield_descriptor;
  if (!pyobject_typecheck(reinterpret_cast<pyobject *>(arg),
                          &cfielddescriptor_type)) {
    pyerr_setstring(pyexc_typeerror, "must be a field descriptor");
    return null;
  }
  cfield_descriptor = reinterpret_cast<cfielddescriptor*>(arg);

  google::protobuf::message* message = self->message;
  int length = message->getreflection()->fieldsize(
      *message, cfield_descriptor->descriptor);
  return pyint_fromlong(length);
}

static pyobject* cmessage_debugstring(cmessage* self, pyobject* args) {
  return pystring_fromstring(self->message->debugstring().c_str());
}

static pyobject* cmessage_serializetostring(cmessage* self, pyobject* args) {
  int size = self->message->bytesize();
  if (size <= 0) {
    return pystring_fromstring("");
  }
  pyobject* result = pystring_fromstringandsize(null, size);
  if (result == null) {
    return null;
  }
  char* buffer = pystring_as_string(result);
  self->message->serializewithcachedsizestoarray(
      reinterpret_cast<uint8*>(buffer));
  return result;
}

static pyobject* cmessage_serializepartialtostring(
    cmessage* self, pyobject* args) {
  string contents;
  self->message->serializepartialtostring(&contents);
  return pystring_fromstringandsize(contents.c_str(), contents.size());
}

static pyobject* cmessagestr(cmessage* self) {
  char str[1024];
  str[sizeof(str) - 1] = 0;
  snprintf(str, sizeof(str) - 1, "cmessage: <%p>", self->message);
  return pystring_fromstring(str);
}

static pyobject* cmessage_mergefrom(cmessage* self, pyobject* arg) {
  cmessage* other_message;
  if (!pyobject_typecheck(reinterpret_cast<pyobject *>(arg), &cmessage_type)) {
    pyerr_setstring(pyexc_typeerror, "must be a message");
    return null;
  }

  other_message = reinterpret_cast<cmessage*>(arg);
  if (other_message->message->getdescriptor() !=
      self->message->getdescriptor()) {
    pyerr_format(pyexc_typeerror,
                 "tried to merge from a message with a different type. "
                 "to: %s, from: %s",
                 self->message->getdescriptor()->full_name().c_str(),
                 other_message->message->getdescriptor()->full_name().c_str());
    return null;
  }
  assurewritable(self);

  self->message->mergefrom(*other_message->message);
  py_return_none;
}

static pyobject* cmessage_copyfrom(cmessage* self, pyobject* arg) {
  cmessage* other_message;
  if (!pyobject_typecheck(reinterpret_cast<pyobject *>(arg), &cmessage_type)) {
    pyerr_setstring(pyexc_typeerror, "must be a message");
    return null;
  }

  other_message = reinterpret_cast<cmessage*>(arg);
  if (other_message->message->getdescriptor() !=
      self->message->getdescriptor()) {
    pyerr_format(pyexc_typeerror,
                 "tried to copy from a message with a different type. "
                 "to: %s, from: %s",
                 self->message->getdescriptor()->full_name().c_str(),
                 other_message->message->getdescriptor()->full_name().c_str());
    return null;
  }

  assurewritable(self);

  self->message->copyfrom(*other_message->message);
  py_return_none;
}

static pyobject* cmessage_mergefromstring(cmessage* self, pyobject* arg) {
  const void* data;
  py_ssize_t data_length;
  if (pyobject_asreadbuffer(arg, &data, &data_length) < 0) {
    return null;
  }

  assurewritable(self);
  google::protobuf::io::codedinputstream input(
      reinterpret_cast<const uint8*>(data), data_length);
  input.setextensionregistry(getdescriptorpool(), global_message_factory);
  bool success = self->message->mergepartialfromcodedstream(&input);
  if (success) {
    return pyint_fromlong(self->message->bytesize());
  } else {
    return pyint_fromlong(-1);
  }
}

static pyobject* cmessage_bytesize(cmessage* self, pyobject* args) {
  return pylong_fromlong(self->message->bytesize());
}

static pyobject* cmessage_setinparent(cmessage* self, pyobject* args) {
  assurewritable(self);
  py_return_none;
}

static pyobject* cmessage_swaprepeatedfieldelements(
    cmessage* self, pyobject* args) {
  cfielddescriptor* cfield_descriptor;
  int index1, index2;
  if (!pyarg_parsetuple(args, c("o!ii:swaprepeatedfieldelements"),
                        &cfielddescriptor_type, &cfield_descriptor,
                        &index1, &index2)) {
    return null;
  }

  google::protobuf::message* message = self->message;
  const google::protobuf::reflection* reflection = message->getreflection();

  reflection->swapelements(
      message, cfield_descriptor->descriptor, index1, index2);
  py_return_none;
}

static pyobject* cmessage_addmessage(cmessage* self, pyobject* arg) {
  cfielddescriptor* cfield_descriptor;
  if (!pyobject_typecheck(reinterpret_cast<pyobject *>(arg),
                          &cfielddescriptor_type)) {
    pyerr_setstring(pyexc_typeerror, "must be a field descriptor");
    return null;
  }
  cfield_descriptor = reinterpret_cast<cfielddescriptor*>(arg);
  assurewritable(self);

  cmessage* py_cmsg = pyobject_new(cmessage, &cmessage_type);
  if (py_cmsg == null) {
    return null;
  }

  google::protobuf::message* message = self->message;
  const google::protobuf::reflection* reflection = message->getreflection();
  google::protobuf::message* sub_message =
      reflection->addmessage(message, cfield_descriptor->descriptor);

  py_cmsg->parent = null;
  py_cmsg->full_name = sub_message->getdescriptor()->full_name().c_str();
  py_cmsg->message = sub_message;
  py_cmsg->free_message = false;
  py_cmsg->read_only = false;
  return reinterpret_cast<pyobject*>(py_cmsg);
}

static pyobject* cmessage_getrepeatedmessage(cmessage* self, pyobject* args) {
  cfielddescriptor* cfield_descriptor;
  pyobject* slice;
  if (!pyarg_parsetuple(args, c("o!o:getrepeatedmessage"),
                        &cfielddescriptor_type, &cfield_descriptor, &slice)) {
    return null;
  }

  return internalgetrepeatedscalarslice(
      self, cfield_descriptor->descriptor, slice);
}

static pyobject* cmessage_newsubmessage(cmessage* self, pyobject* arg) {
  cfielddescriptor* cfield_descriptor;
  if (!pyobject_typecheck(reinterpret_cast<pyobject *>(arg),
                          &cfielddescriptor_type)) {
    pyerr_setstring(pyexc_typeerror, "must be a field descriptor");
    return null;
  }
  cfield_descriptor = reinterpret_cast<cfielddescriptor*>(arg);

  cmessage* py_cmsg = pyobject_new(cmessage, &cmessage_type);
  if (py_cmsg == null) {
    return null;
  }

  google::protobuf::message* message = self->message;
  const google::protobuf::reflection* reflection = message->getreflection();
  const google::protobuf::message& sub_message =
      reflection->getmessage(*message, cfield_descriptor->descriptor,
                             global_message_factory);

  py_cmsg->full_name = sub_message.getdescriptor()->full_name().c_str();
  py_cmsg->parent = self;
  py_cmsg->parent_field = cfield_descriptor;
  py_cmsg->message = const_cast<google::protobuf::message*>(&sub_message);
  py_cmsg->free_message = false;
  py_cmsg->read_only = true;
  return reinterpret_cast<pyobject*>(py_cmsg);
}

static pyobject* cmessage_mutablemessage(cmessage* self, pyobject* arg) {
  cfielddescriptor* cfield_descriptor;
  if (!pyobject_typecheck(reinterpret_cast<pyobject *>(arg),
                          &cfielddescriptor_type)) {
    pyerr_setstring(pyexc_typeerror, "must be a field descriptor");
    return null;
  }
  cfield_descriptor = reinterpret_cast<cfielddescriptor*>(arg);
  assurewritable(self);

  cmessage* py_cmsg = pyobject_new(cmessage, &cmessage_type);
  if (py_cmsg == null) {
    return null;
  }

  google::protobuf::message* message = self->message;
  const google::protobuf::reflection* reflection = message->getreflection();
  google::protobuf::message* mutable_message =
      reflection->mutablemessage(message, cfield_descriptor->descriptor,
                                 global_message_factory);

  py_cmsg->full_name = mutable_message->getdescriptor()->full_name().c_str();
  py_cmsg->message = mutable_message;
  py_cmsg->free_message = false;
  py_cmsg->read_only = false;
  return reinterpret_cast<pyobject*>(py_cmsg);
}

static pyobject* cmessage_equals(cmessage* self, pyobject* arg) {
  cmessage* other_message;
  if (!pyobject_typecheck(reinterpret_cast<pyobject *>(arg), &cmessage_type)) {
    pyerr_setstring(pyexc_typeerror, "must be a message");
    return null;
  }
  other_message = reinterpret_cast<cmessage*>(arg);

  if (other_message->message == self->message) {
    return pybool_fromlong(1);
  }

  if (other_message->message->getdescriptor() !=
      self->message->getdescriptor()) {
    return pybool_fromlong(0);
  }

  return pybool_fromlong(1);
}

static pyobject* cmessage_listfields(cmessage* self, pyobject* args) {
  google::protobuf::message* message = self->message;
  const google::protobuf::reflection* reflection = message->getreflection();
  vector<const google::protobuf::fielddescriptor*> fields;
  reflection->listfields(*message, &fields);

  pyobject* list = pylist_new(fields.size());
  if (list == null) {
    return null;
  }

  for (unsigned int i = 0; i < fields.size(); ++i) {
    bool is_extension = fields[i]->is_extension();
    pyobject* t = pytuple_new(2);
    if (t == null) {
      py_decref(list);
      return null;
    }

    pyobject* is_extension_object = pybool_fromlong(is_extension ? 1 : 0);

    pyobject* field_name;
    const string* s;
    if (is_extension) {
      s = &fields[i]->full_name();
    } else {
      s = &fields[i]->name();
    }
    field_name = pystring_fromstringandsize(s->c_str(), s->length());
    if (field_name == null) {
      py_decref(list);
      py_decref(t);
      return null;
    }

    pytuple_set_item(t, 0, is_extension_object);
    pytuple_set_item(t, 1, field_name);
    pylist_set_item(list, i, t);
  }

  return list;
}

static pyobject* cmessage_findinitializationerrors(cmessage* self) {
  google::protobuf::message* message = self->message;
  vector<string> errors;
  message->findinitializationerrors(&errors);

  pyobject* error_list = pylist_new(errors.size());
  if (error_list == null) {
    return null;
  }
  for (unsigned int i = 0; i < errors.size(); ++i) {
    const string& error = errors[i];
    pyobject* error_string = pystring_fromstringandsize(
        error.c_str(), error.length());
    if (error_string == null) {
      py_decref(error_list);
      return null;
    }
    pylist_set_item(error_list, i, error_string);
  }
  return error_list;
}

// ------ python constructor:

pyobject* python_newcmessage(pyobject* ignored, pyobject* arg) {
  const char* message_type = pystring_asstring(arg);
  if (message_type == null) {
    return null;
  }

  const google::protobuf::message* message = createmessage(message_type);
  if (message == null) {
    pyerr_format(pyexc_typeerror, "couldn't create message of type %s!",
                 message_type);
    return null;
  }

  cmessage* py_cmsg = pyobject_new(cmessage, &cmessage_type);
  if (py_cmsg == null) {
    return null;
  }
  py_cmsg->message = message->new();
  py_cmsg->free_message = true;
  py_cmsg->full_name = message->getdescriptor()->full_name().c_str();
  py_cmsg->read_only = false;
  py_cmsg->parent = null;
  py_cmsg->parent_field = null;
  return reinterpret_cast<pyobject*>(py_cmsg);
}

// --- module functions (exposed to python):

pymethoddef methods[] = {
  { c("newcmessage"), (pycfunction)python_newcmessage,
    meth_o,
    c("creates a new c++ protocol message, given its full name.") },
  { c("newcdescriptorpool"), (pycfunction)python_newcdescriptorpool,
    meth_noargs,
    c("creates a new c++ descriptor pool.") },
  { c("buildfile"), (pycfunction)python_buildfile,
    meth_o,
    c("registers a new protocol buffer file in the global c++ descriptor "
      "pool.") },
  {null}
};

// --- exposing the c proto living inside python proto to c code:

extern const message* (*getcprotoinsidepyprotoptr)(pyobject* msg);
extern message* (*mutablecprotoinsidepyprotoptr)(pyobject* msg);

static const google::protobuf::message* getcprotoinsidepyprotoimpl(pyobject* msg) {
  pyobject* c_msg_obj = pyobject_getattrstring(msg, "_cmsg");
  if (c_msg_obj == null) {
    pyerr_clear();
    return null;
  }
  py_decref(c_msg_obj);
  if (!pyobject_typecheck(c_msg_obj, &cmessage_type)) {
    return null;
  }
  cmessage* c_msg = reinterpret_cast<cmessage*>(c_msg_obj);
  return c_msg->message;
}

static google::protobuf::message* mutablecprotoinsidepyprotoimpl(pyobject* msg) {
  pyobject* c_msg_obj = pyobject_getattrstring(msg, "_cmsg");
  if (c_msg_obj == null) {
    pyerr_clear();
    return null;
  }
  py_decref(c_msg_obj);
  if (!pyobject_typecheck(c_msg_obj, &cmessage_type)) {
    return null;
  }
  cmessage* c_msg = reinterpret_cast<cmessage*>(c_msg_obj);
  assurewritable(c_msg);
  return c_msg->message;
}

// --- module init function:

static const char module_docstring[] =
"python-proto2 is a module that can be used to enhance proto2 python api\n"
"performance.\n"
"\n"
"it provides access to the protocol buffers c++ reflection api that\n"
"implements the basic protocol buffer functions.";

extern "c" {
  void init_net_proto2___python() {
    // initialize constants.
    kpythonzero = pyint_fromlong(0);
    kint32min_py = pyint_fromlong(kint32min);
    kint32max_py = pyint_fromlong(kint32max);
    kuint32max_py = pylong_fromlonglong(kuint32max);
    kint64min_py = pylong_fromlonglong(kint64min);
    kint64max_py = pylong_fromlonglong(kint64max);
    kuint64max_py = pylong_fromunsignedlonglong(kuint64max);

    global_message_factory = new dynamicmessagefactory(getdescriptorpool());
    global_message_factory->setdelegatetogeneratedfactory(true);

    // export our functions to python.
    pyobject *m;
    m = py_initmodule3(c("_net_proto2___python"), methods, c(module_docstring));
    if (m == null) {
      return;
    }

    addconstants(m);

    cmessage_type.tp_new = pytype_genericnew;
    if (pytype_ready(&cmessage_type) < 0) {
      return;
    }

    if (!initdescriptor()) {
      return;
    }

    // override {get,mutable}cprotoinsidepyproto.
    getcprotoinsidepyprotoptr = getcprotoinsidepyprotoimpl;
    mutablecprotoinsidepyprotoptr = mutablecprotoinsidepyprotoimpl;
  }
}

}  // namespace python
}  // namespace protobuf
}  // namespace google
