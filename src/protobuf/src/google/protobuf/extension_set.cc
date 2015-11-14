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

#include <google/protobuf/stubs/hash.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/once.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/wire_format_lite_inl.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/stubs/map-util.h>

namespace google {
namespace protobuf {
namespace internal {

namespace {

inline wireformatlite::fieldtype real_type(fieldtype type) {
  google_dcheck(type > 0 && type <= wireformatlite::max_field_type);
  return static_cast<wireformatlite::fieldtype>(type);
}

inline wireformatlite::cpptype cpp_type(fieldtype type) {
  return wireformatlite::fieldtypetocpptype(real_type(type));
}

// registry stuff.
typedef hash_map<pair<const messagelite*, int>,
                 extensioninfo> extensionregistry;
extensionregistry* registry_ = null;
google_protobuf_declare_once(registry_init_);

void deleteregistry() {
  delete registry_;
  registry_ = null;
}

void initregistry() {
  registry_ = new extensionregistry;
  onshutdown(&deleteregistry);
}

// this function is only called at startup, so there is no need for thread-
// safety.
void register(const messagelite* containing_type,
              int number, extensioninfo info) {
  ::google::protobuf::googleonceinit(&registry_init_, &initregistry);

  if (!insertifnotpresent(registry_, make_pair(containing_type, number),
                          info)) {
    google_log(fatal) << "multiple extension registrations for type \""
               << containing_type->gettypename()
               << "\", field number " << number << ".";
  }
}

const extensioninfo* findregisteredextension(
    const messagelite* containing_type, int number) {
  return (registry_ == null) ? null :
         findornull(*registry_, make_pair(containing_type, number));
}

}  // namespace

extensionfinder::~extensionfinder() {}

bool generatedextensionfinder::find(int number, extensioninfo* output) {
  const extensioninfo* extension =
      findregisteredextension(containing_type_, number);
  if (extension == null) {
    return false;
  } else {
    *output = *extension;
    return true;
  }
}

void extensionset::registerextension(const messagelite* containing_type,
                                     int number, fieldtype type,
                                     bool is_repeated, bool is_packed) {
  google_check_ne(type, wireformatlite::type_enum);
  google_check_ne(type, wireformatlite::type_message);
  google_check_ne(type, wireformatlite::type_group);
  extensioninfo info(type, is_repeated, is_packed);
  register(containing_type, number, info);
}

static bool callnoargvalidityfunc(const void* arg, int number) {
  // note:  must use c-style cast here rather than reinterpret_cast because
  //   the c++ standard at one point did not allow casts between function and
  //   data pointers and some compilers enforce this for c++-style casts.  no
  //   compiler enforces it for c-style casts since lots of c-style code has
  //   relied on these kinds of casts for a long time, despite being
  //   technically undefined.  see:
  //     http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_defects.html#195
  // also note:  some compilers do not allow function pointers to be "const".
  //   which makes sense, i suppose, because it's meaningless.
  return ((enumvalidityfunc*)arg)(number);
}

void extensionset::registerenumextension(const messagelite* containing_type,
                                         int number, fieldtype type,
                                         bool is_repeated, bool is_packed,
                                         enumvalidityfunc* is_valid) {
  google_check_eq(type, wireformatlite::type_enum);
  extensioninfo info(type, is_repeated, is_packed);
  info.enum_validity_check.func = callnoargvalidityfunc;
  // see comment in callnoargvalidityfunc() about why we use a c-style cast.
  info.enum_validity_check.arg = (void*)is_valid;
  register(containing_type, number, info);
}

void extensionset::registermessageextension(const messagelite* containing_type,
                                            int number, fieldtype type,
                                            bool is_repeated, bool is_packed,
                                            const messagelite* prototype) {
  google_check(type == wireformatlite::type_message ||
        type == wireformatlite::type_group);
  extensioninfo info(type, is_repeated, is_packed);
  info.message_prototype = prototype;
  register(containing_type, number, info);
}


// ===================================================================
// constructors and basic methods.

extensionset::extensionset() {}

extensionset::~extensionset() {
  for (map<int, extension>::iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    iter->second.free();
  }
}

// defined in extension_set_heavy.cc.
// void extensionset::appendtolist(const descriptor* containing_type,
//                                 const descriptorpool* pool,
//                                 vector<const fielddescriptor*>* output) const

bool extensionset::has(int number) const {
  map<int, extension>::const_iterator iter = extensions_.find(number);
  if (iter == extensions_.end()) return false;
  google_dcheck(!iter->second.is_repeated);
  return !iter->second.is_cleared;
}

int extensionset::numextensions() const {
  int result = 0;
  for (map<int, extension>::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    if (!iter->second.is_cleared) {
      ++result;
    }
  }
  return result;
}

int extensionset::extensionsize(int number) const {
  map<int, extension>::const_iterator iter = extensions_.find(number);
  if (iter == extensions_.end()) return false;
  return iter->second.getsize();
}

fieldtype extensionset::extensiontype(int number) const {
  map<int, extension>::const_iterator iter = extensions_.find(number);
  if (iter == extensions_.end()) {
    google_log(dfatal) << "don't lookup extension types if they aren't present (1). ";
    return 0;
  }
  if (iter->second.is_cleared) {
    google_log(dfatal) << "don't lookup extension types if they aren't present (2). ";
  }
  return iter->second.type;
}

void extensionset::clearextension(int number) {
  map<int, extension>::iterator iter = extensions_.find(number);
  if (iter == extensions_.end()) return;
  iter->second.clear();
}

// ===================================================================
// field accessors

namespace {

enum cardinality {
  repeated,
  optional
};

}  // namespace

#define google_dcheck_type(extension, label, cpptype)                             \
  google_dcheck_eq((extension).is_repeated ? repeated : optional, label);         \
  google_dcheck_eq(cpp_type((extension).type), wireformatlite::cpptype_##cpptype)

// -------------------------------------------------------------------
// primitives

#define primitive_accessors(uppercase, lowercase, camelcase)                   \
                                                                               \
lowercase extensionset::get##camelcase(int number,                             \
                                       lowercase default_value) const {        \
  map<int, extension>::const_iterator iter = extensions_.find(number);         \
  if (iter == extensions_.end() || iter->second.is_cleared) {                  \
    return default_value;                                                      \
  } else {                                                                     \
    google_dcheck_type(iter->second, optional, uppercase);                            \
    return iter->second.lowercase##_value;                                     \
  }                                                                            \
}                                                                              \
                                                                               \
void extensionset::set##camelcase(int number, fieldtype type,                  \
                                  lowercase value,                             \
                                  const fielddescriptor* descriptor) {         \
  extension* extension;                                                        \
  if (maybenewextension(number, descriptor, &extension)) {                     \
    extension->type = type;                                                    \
    google_dcheck_eq(cpp_type(extension->type), wireformatlite::cpptype_##uppercase); \
    extension->is_repeated = false;                                            \
  } else {                                                                     \
    google_dcheck_type(*extension, optional, uppercase);                              \
  }                                                                            \
  extension->is_cleared = false;                                               \
  extension->lowercase##_value = value;                                        \
}                                                                              \
                                                                               \
lowercase extensionset::getrepeated##camelcase(int number, int index) const {  \
  map<int, extension>::const_iterator iter = extensions_.find(number);         \
  google_check(iter != extensions_.end()) << "index out-of-bounds (field is empty)."; \
  google_dcheck_type(iter->second, repeated, uppercase);                              \
  return iter->second.repeated_##lowercase##_value->get(index);                \
}                                                                              \
                                                                               \
void extensionset::setrepeated##camelcase(                                     \
    int number, int index, lowercase value) {                                  \
  map<int, extension>::iterator iter = extensions_.find(number);               \
  google_check(iter != extensions_.end()) << "index out-of-bounds (field is empty)."; \
  google_dcheck_type(iter->second, repeated, uppercase);                              \
  iter->second.repeated_##lowercase##_value->set(index, value);                \
}                                                                              \
                                                                               \
void extensionset::add##camelcase(int number, fieldtype type,                  \
                                  bool packed, lowercase value,                \
                                  const fielddescriptor* descriptor) {         \
  extension* extension;                                                        \
  if (maybenewextension(number, descriptor, &extension)) {                     \
    extension->type = type;                                                    \
    google_dcheck_eq(cpp_type(extension->type), wireformatlite::cpptype_##uppercase); \
    extension->is_repeated = true;                                             \
    extension->is_packed = packed;                                             \
    extension->repeated_##lowercase##_value = new repeatedfield<lowercase>();  \
  } else {                                                                     \
    google_dcheck_type(*extension, repeated, uppercase);                              \
    google_dcheck_eq(extension->is_packed, packed);                                   \
  }                                                                            \
  extension->repeated_##lowercase##_value->add(value);                         \
}

primitive_accessors( int32,  int32,  int32)
primitive_accessors( int64,  int64,  int64)
primitive_accessors(uint32, uint32, uint32)
primitive_accessors(uint64, uint64, uint64)
primitive_accessors( float,  float,  float)
primitive_accessors(double, double, double)
primitive_accessors(  bool,   bool,   bool)

#undef primitive_accessors

void* extensionset::mutablerawrepeatedfield(int number) {
  // we assume that all the repeatedfield<>* pointers have the same
  // size and alignment within the anonymous union in extension.
  map<int, extension>::const_iterator iter = extensions_.find(number);
  google_check(iter != extensions_.end()) << "no extension numbered " << number;
  return iter->second.repeated_int32_value;
}

// -------------------------------------------------------------------
// enums

int extensionset::getenum(int number, int default_value) const {
  map<int, extension>::const_iterator iter = extensions_.find(number);
  if (iter == extensions_.end() || iter->second.is_cleared) {
    // not present.  return the default value.
    return default_value;
  } else {
    google_dcheck_type(iter->second, optional, enum);
    return iter->second.enum_value;
  }
}

void extensionset::setenum(int number, fieldtype type, int value,
                           const fielddescriptor* descriptor) {
  extension* extension;
  if (maybenewextension(number, descriptor, &extension)) {
    extension->type = type;
    google_dcheck_eq(cpp_type(extension->type), wireformatlite::cpptype_enum);
    extension->is_repeated = false;
  } else {
    google_dcheck_type(*extension, optional, enum);
  }
  extension->is_cleared = false;
  extension->enum_value = value;
}

int extensionset::getrepeatedenum(int number, int index) const {
  map<int, extension>::const_iterator iter = extensions_.find(number);
  google_check(iter != extensions_.end()) << "index out-of-bounds (field is empty).";
  google_dcheck_type(iter->second, repeated, enum);
  return iter->second.repeated_enum_value->get(index);
}

void extensionset::setrepeatedenum(int number, int index, int value) {
  map<int, extension>::iterator iter = extensions_.find(number);
  google_check(iter != extensions_.end()) << "index out-of-bounds (field is empty).";
  google_dcheck_type(iter->second, repeated, enum);
  iter->second.repeated_enum_value->set(index, value);
}

void extensionset::addenum(int number, fieldtype type,
                           bool packed, int value,
                           const fielddescriptor* descriptor) {
  extension* extension;
  if (maybenewextension(number, descriptor, &extension)) {
    extension->type = type;
    google_dcheck_eq(cpp_type(extension->type), wireformatlite::cpptype_enum);
    extension->is_repeated = true;
    extension->is_packed = packed;
    extension->repeated_enum_value = new repeatedfield<int>();
  } else {
    google_dcheck_type(*extension, repeated, enum);
    google_dcheck_eq(extension->is_packed, packed);
  }
  extension->repeated_enum_value->add(value);
}

// -------------------------------------------------------------------
// strings

const string& extensionset::getstring(int number,
                                      const string& default_value) const {
  map<int, extension>::const_iterator iter = extensions_.find(number);
  if (iter == extensions_.end() || iter->second.is_cleared) {
    // not present.  return the default value.
    return default_value;
  } else {
    google_dcheck_type(iter->second, optional, string);
    return *iter->second.string_value;
  }
}

string* extensionset::mutablestring(int number, fieldtype type,
                                    const fielddescriptor* descriptor) {
  extension* extension;
  if (maybenewextension(number, descriptor, &extension)) {
    extension->type = type;
    google_dcheck_eq(cpp_type(extension->type), wireformatlite::cpptype_string);
    extension->is_repeated = false;
    extension->string_value = new string;
  } else {
    google_dcheck_type(*extension, optional, string);
  }
  extension->is_cleared = false;
  return extension->string_value;
}

const string& extensionset::getrepeatedstring(int number, int index) const {
  map<int, extension>::const_iterator iter = extensions_.find(number);
  google_check(iter != extensions_.end()) << "index out-of-bounds (field is empty).";
  google_dcheck_type(iter->second, repeated, string);
  return iter->second.repeated_string_value->get(index);
}

string* extensionset::mutablerepeatedstring(int number, int index) {
  map<int, extension>::iterator iter = extensions_.find(number);
  google_check(iter != extensions_.end()) << "index out-of-bounds (field is empty).";
  google_dcheck_type(iter->second, repeated, string);
  return iter->second.repeated_string_value->mutable(index);
}

string* extensionset::addstring(int number, fieldtype type,
                                const fielddescriptor* descriptor) {
  extension* extension;
  if (maybenewextension(number, descriptor, &extension)) {
    extension->type = type;
    google_dcheck_eq(cpp_type(extension->type), wireformatlite::cpptype_string);
    extension->is_repeated = true;
    extension->is_packed = false;
    extension->repeated_string_value = new repeatedptrfield<string>();
  } else {
    google_dcheck_type(*extension, repeated, string);
  }
  return extension->repeated_string_value->add();
}

// -------------------------------------------------------------------
// messages

const messagelite& extensionset::getmessage(
    int number, const messagelite& default_value) const {
  map<int, extension>::const_iterator iter = extensions_.find(number);
  if (iter == extensions_.end()) {
    // not present.  return the default value.
    return default_value;
  } else {
    google_dcheck_type(iter->second, optional, message);
    if (iter->second.is_lazy) {
      return iter->second.lazymessage_value->getmessage(default_value);
    } else {
      return *iter->second.message_value;
    }
  }
}

// defined in extension_set_heavy.cc.
// const messagelite& extensionset::getmessage(int number,
//                                             const descriptor* message_type,
//                                             messagefactory* factory) const

messagelite* extensionset::mutablemessage(int number, fieldtype type,
                                          const messagelite& prototype,
                                          const fielddescriptor* descriptor) {
  extension* extension;
  if (maybenewextension(number, descriptor, &extension)) {
    extension->type = type;
    google_dcheck_eq(cpp_type(extension->type), wireformatlite::cpptype_message);
    extension->is_repeated = false;
    extension->is_lazy = false;
    extension->message_value = prototype.new();
    extension->is_cleared = false;
    return extension->message_value;
  } else {
    google_dcheck_type(*extension, optional, message);
    extension->is_cleared = false;
    if (extension->is_lazy) {
      return extension->lazymessage_value->mutablemessage(prototype);
    } else {
      return extension->message_value;
    }
  }
}

// defined in extension_set_heavy.cc.
// messagelite* extensionset::mutablemessage(int number, fieldtype type,
//                                           const descriptor* message_type,
//                                           messagefactory* factory)

void extensionset::setallocatedmessage(int number, fieldtype type,
                                       const fielddescriptor* descriptor,
                                       messagelite* message) {
  if (message == null) {
    clearextension(number);
    return;
  }
  extension* extension;
  if (maybenewextension(number, descriptor, &extension)) {
    extension->type = type;
    google_dcheck_eq(cpp_type(extension->type), wireformatlite::cpptype_message);
    extension->is_repeated = false;
    extension->is_lazy = false;
    extension->message_value = message;
  } else {
    google_dcheck_type(*extension, optional, message);
    if (extension->is_lazy) {
      extension->lazymessage_value->setallocatedmessage(message);
    } else {
      delete extension->message_value;
      extension->message_value = message;
    }
  }
  extension->is_cleared = false;
}

messagelite* extensionset::releasemessage(int number,
                                          const messagelite& prototype) {
  map<int, extension>::iterator iter = extensions_.find(number);
  if (iter == extensions_.end()) {
    // not present.  return null.
    return null;
  } else {
    google_dcheck_type(iter->second, optional, message);
    messagelite* ret = null;
    if (iter->second.is_lazy) {
      ret = iter->second.lazymessage_value->releasemessage(prototype);
      delete iter->second.lazymessage_value;
    } else {
      ret = iter->second.message_value;
    }
    extensions_.erase(number);
    return ret;
  }
}

// defined in extension_set_heavy.cc.
// messagelite* extensionset::releasemessage(const fielddescriptor* descriptor,
//                                           messagefactory* factory);

const messagelite& extensionset::getrepeatedmessage(
    int number, int index) const {
  map<int, extension>::const_iterator iter = extensions_.find(number);
  google_check(iter != extensions_.end()) << "index out-of-bounds (field is empty).";
  google_dcheck_type(iter->second, repeated, message);
  return iter->second.repeated_message_value->get(index);
}

messagelite* extensionset::mutablerepeatedmessage(int number, int index) {
  map<int, extension>::iterator iter = extensions_.find(number);
  google_check(iter != extensions_.end()) << "index out-of-bounds (field is empty).";
  google_dcheck_type(iter->second, repeated, message);
  return iter->second.repeated_message_value->mutable(index);
}

messagelite* extensionset::addmessage(int number, fieldtype type,
                                      const messagelite& prototype,
                                      const fielddescriptor* descriptor) {
  extension* extension;
  if (maybenewextension(number, descriptor, &extension)) {
    extension->type = type;
    google_dcheck_eq(cpp_type(extension->type), wireformatlite::cpptype_message);
    extension->is_repeated = true;
    extension->repeated_message_value =
      new repeatedptrfield<messagelite>();
  } else {
    google_dcheck_type(*extension, repeated, message);
  }

  // repeatedptrfield<messagelite> does not know how to add() since it cannot
  // allocate an abstract object, so we have to be tricky.
  messagelite* result = extension->repeated_message_value
      ->addfromcleared<generictypehandler<messagelite> >();
  if (result == null) {
    result = prototype.new();
    extension->repeated_message_value->addallocated(result);
  }
  return result;
}

// defined in extension_set_heavy.cc.
// messagelite* extensionset::addmessage(int number, fieldtype type,
//                                       const descriptor* message_type,
//                                       messagefactory* factory)

#undef google_dcheck_type

void extensionset::removelast(int number) {
  map<int, extension>::iterator iter = extensions_.find(number);
  google_check(iter != extensions_.end()) << "index out-of-bounds (field is empty).";

  extension* extension = &iter->second;
  google_dcheck(extension->is_repeated);

  switch(cpp_type(extension->type)) {
    case wireformatlite::cpptype_int32:
      extension->repeated_int32_value->removelast();
      break;
    case wireformatlite::cpptype_int64:
      extension->repeated_int64_value->removelast();
      break;
    case wireformatlite::cpptype_uint32:
      extension->repeated_uint32_value->removelast();
      break;
    case wireformatlite::cpptype_uint64:
      extension->repeated_uint64_value->removelast();
      break;
    case wireformatlite::cpptype_float:
      extension->repeated_float_value->removelast();
      break;
    case wireformatlite::cpptype_double:
      extension->repeated_double_value->removelast();
      break;
    case wireformatlite::cpptype_bool:
      extension->repeated_bool_value->removelast();
      break;
    case wireformatlite::cpptype_enum:
      extension->repeated_enum_value->removelast();
      break;
    case wireformatlite::cpptype_string:
      extension->repeated_string_value->removelast();
      break;
    case wireformatlite::cpptype_message:
      extension->repeated_message_value->removelast();
      break;
  }
}

messagelite* extensionset::releaselast(int number) {
  map<int, extension>::iterator iter = extensions_.find(number);
  google_check(iter != extensions_.end()) << "index out-of-bounds (field is empty).";

  extension* extension = &iter->second;
  google_dcheck(extension->is_repeated);
  google_dcheck(cpp_type(extension->type) == wireformatlite::cpptype_message);
  return extension->repeated_message_value->releaselast();
}

void extensionset::swapelements(int number, int index1, int index2) {
  map<int, extension>::iterator iter = extensions_.find(number);
  google_check(iter != extensions_.end()) << "index out-of-bounds (field is empty).";

  extension* extension = &iter->second;
  google_dcheck(extension->is_repeated);

  switch(cpp_type(extension->type)) {
    case wireformatlite::cpptype_int32:
      extension->repeated_int32_value->swapelements(index1, index2);
      break;
    case wireformatlite::cpptype_int64:
      extension->repeated_int64_value->swapelements(index1, index2);
      break;
    case wireformatlite::cpptype_uint32:
      extension->repeated_uint32_value->swapelements(index1, index2);
      break;
    case wireformatlite::cpptype_uint64:
      extension->repeated_uint64_value->swapelements(index1, index2);
      break;
    case wireformatlite::cpptype_float:
      extension->repeated_float_value->swapelements(index1, index2);
      break;
    case wireformatlite::cpptype_double:
      extension->repeated_double_value->swapelements(index1, index2);
      break;
    case wireformatlite::cpptype_bool:
      extension->repeated_bool_value->swapelements(index1, index2);
      break;
    case wireformatlite::cpptype_enum:
      extension->repeated_enum_value->swapelements(index1, index2);
      break;
    case wireformatlite::cpptype_string:
      extension->repeated_string_value->swapelements(index1, index2);
      break;
    case wireformatlite::cpptype_message:
      extension->repeated_message_value->swapelements(index1, index2);
      break;
  }
}

// ===================================================================

void extensionset::clear() {
  for (map<int, extension>::iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    iter->second.clear();
  }
}

void extensionset::mergefrom(const extensionset& other) {
  for (map<int, extension>::const_iterator iter = other.extensions_.begin();
       iter != other.extensions_.end(); ++iter) {
    const extension& other_extension = iter->second;

    if (other_extension.is_repeated) {
      extension* extension;
      bool is_new = maybenewextension(iter->first, other_extension.descriptor,
                                      &extension);
      if (is_new) {
        // extension did not already exist in set.
        extension->type = other_extension.type;
        extension->is_packed = other_extension.is_packed;
        extension->is_repeated = true;
      } else {
        google_dcheck_eq(extension->type, other_extension.type);
        google_dcheck_eq(extension->is_packed, other_extension.is_packed);
        google_dcheck(extension->is_repeated);
      }

      switch (cpp_type(other_extension.type)) {
#define handle_type(uppercase, lowercase, repeated_type)             \
        case wireformatlite::cpptype_##uppercase:                    \
          if (is_new) {                                              \
            extension->repeated_##lowercase##_value =                \
              new repeated_type;                                     \
          }                                                          \
          extension->repeated_##lowercase##_value->mergefrom(        \
            *other_extension.repeated_##lowercase##_value);          \
          break;

        handle_type(  int32,   int32, repeatedfield   <  int32>);
        handle_type(  int64,   int64, repeatedfield   <  int64>);
        handle_type( uint32,  uint32, repeatedfield   < uint32>);
        handle_type( uint64,  uint64, repeatedfield   < uint64>);
        handle_type(  float,   float, repeatedfield   <  float>);
        handle_type( double,  double, repeatedfield   < double>);
        handle_type(   bool,    bool, repeatedfield   <   bool>);
        handle_type(   enum,    enum, repeatedfield   <    int>);
        handle_type( string,  string, repeatedptrfield< string>);
#undef handle_type

        case wireformatlite::cpptype_message:
          if (is_new) {
            extension->repeated_message_value =
              new repeatedptrfield<messagelite>();
          }
          // we can't call repeatedptrfield<messagelite>::mergefrom() because
          // it would attempt to allocate new objects.
          repeatedptrfield<messagelite>* other_repeated_message =
              other_extension.repeated_message_value;
          for (int i = 0; i < other_repeated_message->size(); i++) {
            const messagelite& other_message = other_repeated_message->get(i);
            messagelite* target = extension->repeated_message_value
                     ->addfromcleared<generictypehandler<messagelite> >();
            if (target == null) {
              target = other_message.new();
              extension->repeated_message_value->addallocated(target);
            }
            target->checktypeandmergefrom(other_message);
          }
          break;
      }
    } else {
      if (!other_extension.is_cleared) {
        switch (cpp_type(other_extension.type)) {
#define handle_type(uppercase, lowercase, camelcase)                         \
          case wireformatlite::cpptype_##uppercase:                          \
            set##camelcase(iter->first, other_extension.type,                \
                           other_extension.lowercase##_value,                \
                           other_extension.descriptor);                      \
            break;

          handle_type( int32,  int32,  int32);
          handle_type( int64,  int64,  int64);
          handle_type(uint32, uint32, uint32);
          handle_type(uint64, uint64, uint64);
          handle_type( float,  float,  float);
          handle_type(double, double, double);
          handle_type(  bool,   bool,   bool);
          handle_type(  enum,   enum,   enum);
#undef handle_type
          case wireformatlite::cpptype_string:
            setstring(iter->first, other_extension.type,
                      *other_extension.string_value,
                      other_extension.descriptor);
            break;
          case wireformatlite::cpptype_message: {
            extension* extension;
            bool is_new = maybenewextension(iter->first,
                                            other_extension.descriptor,
                                            &extension);
            if (is_new) {
              extension->type = other_extension.type;
              extension->is_packed = other_extension.is_packed;
              extension->is_repeated = false;
              if (other_extension.is_lazy) {
                extension->is_lazy = true;
                extension->lazymessage_value =
                    other_extension.lazymessage_value->new();
                extension->lazymessage_value->mergefrom(
                    *other_extension.lazymessage_value);
              } else {
                extension->is_lazy = false;
                extension->message_value =
                    other_extension.message_value->new();
                extension->message_value->checktypeandmergefrom(
                    *other_extension.message_value);
              }
            } else {
              google_dcheck_eq(extension->type, other_extension.type);
              google_dcheck_eq(extension->is_packed,other_extension.is_packed);
              google_dcheck(!extension->is_repeated);
              if (other_extension.is_lazy) {
                if (extension->is_lazy) {
                  extension->lazymessage_value->mergefrom(
                      *other_extension.lazymessage_value);
                } else {
                  extension->message_value->checktypeandmergefrom(
                      other_extension.lazymessage_value->getmessage(
                          *extension->message_value));
                }
              } else {
                if (extension->is_lazy) {
                  extension->lazymessage_value->mutablemessage(
                      *other_extension.message_value)->checktypeandmergefrom(
                          *other_extension.message_value);
                } else {
                  extension->message_value->checktypeandmergefrom(
                      *other_extension.message_value);
                }
              }
            }
            extension->is_cleared = false;
            break;
          }
        }
      }
    }
  }
}

void extensionset::swap(extensionset* x) {
  extensions_.swap(x->extensions_);
}

bool extensionset::isinitialized() const {
  // extensions are never required.  however, we need to check that all
  // embedded messages are initialized.
  for (map<int, extension>::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    const extension& extension = iter->second;
    if (cpp_type(extension.type) == wireformatlite::cpptype_message) {
      if (extension.is_repeated) {
        for (int i = 0; i < extension.repeated_message_value->size(); i++) {
          if (!extension.repeated_message_value->get(i).isinitialized()) {
            return false;
          }
        }
      } else {
        if (!extension.is_cleared) {
          if (extension.is_lazy) {
            if (!extension.lazymessage_value->isinitialized()) return false;
          } else {
            if (!extension.message_value->isinitialized()) return false;
          }
        }
      }
    }
  }

  return true;
}

bool extensionset::findextensioninfofromtag(
    uint32 tag, extensionfinder* extension_finder,
    int* field_number, extensioninfo* extension) {
  *field_number = wireformatlite::gettagfieldnumber(tag);
  wireformatlite::wiretype wire_type = wireformatlite::gettagwiretype(tag);

  bool is_unknown;
  if (!extension_finder->find(*field_number, extension)) {
    is_unknown = true;
  } else if (extension->is_packed) {
    is_unknown = (wire_type != wireformatlite::wiretype_length_delimited);
  } else {
    wireformatlite::wiretype expected_wire_type =
        wireformatlite::wiretypeforfieldtype(real_type(extension->type));
    is_unknown = (wire_type != expected_wire_type);
  }
  return !is_unknown;
}

bool extensionset::parsefield(uint32 tag, io::codedinputstream* input,
                              extensionfinder* extension_finder,
                              fieldskipper* field_skipper) {
  int number;
  extensioninfo extension;
  if (!findextensioninfofromtag(tag, extension_finder, &number, &extension)) {
    return field_skipper->skipfield(input, tag);
  } else {
    return parsefieldwithextensioninfo(number, extension, input, field_skipper);
  }
}

bool extensionset::parsefieldwithextensioninfo(
    int number, const extensioninfo& extension,
    io::codedinputstream* input,
    fieldskipper* field_skipper) {
  if (extension.is_packed) {
    uint32 size;
    if (!input->readvarint32(&size)) return false;
    io::codedinputstream::limit limit = input->pushlimit(size);

    switch (extension.type) {
#define handle_type(uppercase, cpp_camelcase, cpp_lowercase)        \
      case wireformatlite::type_##uppercase:                                   \
        while (input->bytesuntillimit() > 0) {                                 \
          cpp_lowercase value;                                                 \
          if (!wireformatlite::readprimitive<                                  \
                  cpp_lowercase, wireformatlite::type_##uppercase>(            \
                input, &value)) return false;                                  \
          add##cpp_camelcase(number, wireformatlite::type_##uppercase,         \
                             true, value, extension.descriptor);               \
        }                                                                      \
        break

      handle_type(   int32,  int32,   int32);
      handle_type(   int64,  int64,   int64);
      handle_type(  uint32, uint32,  uint32);
      handle_type(  uint64, uint64,  uint64);
      handle_type(  sint32,  int32,   int32);
      handle_type(  sint64,  int64,   int64);
      handle_type( fixed32, uint32,  uint32);
      handle_type( fixed64, uint64,  uint64);
      handle_type(sfixed32,  int32,   int32);
      handle_type(sfixed64,  int64,   int64);
      handle_type(   float,  float,   float);
      handle_type(  double, double,  double);
      handle_type(    bool,   bool,    bool);
#undef handle_type

      case wireformatlite::type_enum:
        while (input->bytesuntillimit() > 0) {
          int value;
          if (!wireformatlite::readprimitive<int, wireformatlite::type_enum>(
                  input, &value)) return false;
          if (extension.enum_validity_check.func(
                  extension.enum_validity_check.arg, value)) {
            addenum(number, wireformatlite::type_enum, true, value,
                    extension.descriptor);
          }
        }
        break;

      case wireformatlite::type_string:
      case wireformatlite::type_bytes:
      case wireformatlite::type_group:
      case wireformatlite::type_message:
        google_log(fatal) << "non-primitive types can't be packed.";
        break;
    }

    input->poplimit(limit);
  } else {
    switch (extension.type) {
#define handle_type(uppercase, cpp_camelcase, cpp_lowercase)                   \
      case wireformatlite::type_##uppercase: {                                 \
        cpp_lowercase value;                                                   \
        if (!wireformatlite::readprimitive<                                    \
                cpp_lowercase, wireformatlite::type_##uppercase>(              \
               input, &value)) return false;                                   \
        if (extension.is_repeated) {                                          \
          add##cpp_camelcase(number, wireformatlite::type_##uppercase,         \
                             false, value, extension.descriptor);              \
        } else {                                                               \
          set##cpp_camelcase(number, wireformatlite::type_##uppercase, value,  \
                             extension.descriptor);                            \
        }                                                                      \
      } break

      handle_type(   int32,  int32,   int32);
      handle_type(   int64,  int64,   int64);
      handle_type(  uint32, uint32,  uint32);
      handle_type(  uint64, uint64,  uint64);
      handle_type(  sint32,  int32,   int32);
      handle_type(  sint64,  int64,   int64);
      handle_type( fixed32, uint32,  uint32);
      handle_type( fixed64, uint64,  uint64);
      handle_type(sfixed32,  int32,   int32);
      handle_type(sfixed64,  int64,   int64);
      handle_type(   float,  float,   float);
      handle_type(  double, double,  double);
      handle_type(    bool,   bool,    bool);
#undef handle_type

      case wireformatlite::type_enum: {
        int value;
        if (!wireformatlite::readprimitive<int, wireformatlite::type_enum>(
                input, &value)) return false;

        if (!extension.enum_validity_check.func(
                extension.enum_validity_check.arg, value)) {
          // invalid value.  treat as unknown.
          field_skipper->skipunknownenum(number, value);
        } else if (extension.is_repeated) {
          addenum(number, wireformatlite::type_enum, false, value,
                  extension.descriptor);
        } else {
          setenum(number, wireformatlite::type_enum, value,
                  extension.descriptor);
        }
        break;
      }

      case wireformatlite::type_string:  {
        string* value = extension.is_repeated ?
          addstring(number, wireformatlite::type_string, extension.descriptor) :
          mutablestring(number, wireformatlite::type_string,
                        extension.descriptor);
        if (!wireformatlite::readstring(input, value)) return false;
        break;
      }

      case wireformatlite::type_bytes:  {
        string* value = extension.is_repeated ?
          addstring(number, wireformatlite::type_bytes, extension.descriptor) :
          mutablestring(number, wireformatlite::type_bytes,
                        extension.descriptor);
        if (!wireformatlite::readbytes(input, value)) return false;
        break;
      }

      case wireformatlite::type_group: {
        messagelite* value = extension.is_repeated ?
            addmessage(number, wireformatlite::type_group,
                       *extension.message_prototype, extension.descriptor) :
            mutablemessage(number, wireformatlite::type_group,
                           *extension.message_prototype, extension.descriptor);
        if (!wireformatlite::readgroup(number, input, value)) return false;
        break;
      }

      case wireformatlite::type_message: {
        messagelite* value = extension.is_repeated ?
            addmessage(number, wireformatlite::type_message,
                       *extension.message_prototype, extension.descriptor) :
            mutablemessage(number, wireformatlite::type_message,
                           *extension.message_prototype, extension.descriptor);
        if (!wireformatlite::readmessage(input, value)) return false;
        break;
      }
    }
  }

  return true;
}

bool extensionset::parsefield(uint32 tag, io::codedinputstream* input,
                              const messagelite* containing_type) {
  fieldskipper skipper;
  generatedextensionfinder finder(containing_type);
  return parsefield(tag, input, &finder, &skipper);
}

// defined in extension_set_heavy.cc.
// bool extensionset::parsefield(uint32 tag, io::codedinputstream* input,
//                               const messagelite* containing_type,
//                               unknownfieldset* unknown_fields)

// defined in extension_set_heavy.cc.
// bool extensionset::parsemessageset(io::codedinputstream* input,
//                                    const messagelite* containing_type,
//                                    unknownfieldset* unknown_fields);

void extensionset::serializewithcachedsizes(
    int start_field_number, int end_field_number,
    io::codedoutputstream* output) const {
  map<int, extension>::const_iterator iter;
  for (iter = extensions_.lower_bound(start_field_number);
       iter != extensions_.end() && iter->first < end_field_number;
       ++iter) {
    iter->second.serializefieldwithcachedsizes(iter->first, output);
  }
}

int extensionset::bytesize() const {
  int total_size = 0;

  for (map<int, extension>::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    total_size += iter->second.bytesize(iter->first);
  }

  return total_size;
}

// defined in extension_set_heavy.cc.
// int extensionset::spaceusedexcludingself() const

bool extensionset::maybenewextension(int number,
                                     const fielddescriptor* descriptor,
                                     extension** result) {
  pair<map<int, extension>::iterator, bool> insert_result =
      extensions_.insert(make_pair(number, extension()));
  *result = &insert_result.first->second;
  (*result)->descriptor = descriptor;
  return insert_result.second;
}

// ===================================================================
// methods of extensionset::extension

void extensionset::extension::clear() {
  if (is_repeated) {
    switch (cpp_type(type)) {
#define handle_type(uppercase, lowercase)                          \
      case wireformatlite::cpptype_##uppercase:                    \
        repeated_##lowercase##_value->clear();                     \
        break

      handle_type(  int32,   int32);
      handle_type(  int64,   int64);
      handle_type( uint32,  uint32);
      handle_type( uint64,  uint64);
      handle_type(  float,   float);
      handle_type( double,  double);
      handle_type(   bool,    bool);
      handle_type(   enum,    enum);
      handle_type( string,  string);
      handle_type(message, message);
#undef handle_type
    }
  } else {
    if (!is_cleared) {
      switch (cpp_type(type)) {
        case wireformatlite::cpptype_string:
          string_value->clear();
          break;
        case wireformatlite::cpptype_message:
          if (is_lazy) {
            lazymessage_value->clear();
          } else {
            message_value->clear();
          }
          break;
        default:
          // no need to do anything.  get*() will return the default value
          // as long as is_cleared is true and set*() will overwrite the
          // previous value.
          break;
      }

      is_cleared = true;
    }
  }
}

void extensionset::extension::serializefieldwithcachedsizes(
    int number,
    io::codedoutputstream* output) const {
  if (is_repeated) {
    if (is_packed) {
      if (cached_size == 0) return;

      wireformatlite::writetag(number,
          wireformatlite::wiretype_length_delimited, output);
      output->writevarint32(cached_size);

      switch (real_type(type)) {
#define handle_type(uppercase, camelcase, lowercase)                        \
        case wireformatlite::type_##uppercase:                              \
          for (int i = 0; i < repeated_##lowercase##_value->size(); i++) {  \
            wireformatlite::write##camelcase##notag(                        \
              repeated_##lowercase##_value->get(i), output);                \
          }                                                                 \
          break

        handle_type(   int32,    int32,   int32);
        handle_type(   int64,    int64,   int64);
        handle_type(  uint32,   uint32,  uint32);
        handle_type(  uint64,   uint64,  uint64);
        handle_type(  sint32,   sint32,   int32);
        handle_type(  sint64,   sint64,   int64);
        handle_type( fixed32,  fixed32,  uint32);
        handle_type( fixed64,  fixed64,  uint64);
        handle_type(sfixed32, sfixed32,   int32);
        handle_type(sfixed64, sfixed64,   int64);
        handle_type(   float,    float,   float);
        handle_type(  double,   double,  double);
        handle_type(    bool,     bool,    bool);
        handle_type(    enum,     enum,    enum);
#undef handle_type

        case wireformatlite::type_string:
        case wireformatlite::type_bytes:
        case wireformatlite::type_group:
        case wireformatlite::type_message:
          google_log(fatal) << "non-primitive types can't be packed.";
          break;
      }
    } else {
      switch (real_type(type)) {
#define handle_type(uppercase, camelcase, lowercase)                        \
        case wireformatlite::type_##uppercase:                              \
          for (int i = 0; i < repeated_##lowercase##_value->size(); i++) {  \
            wireformatlite::write##camelcase(number,                        \
              repeated_##lowercase##_value->get(i), output);                \
          }                                                                 \
          break

        handle_type(   int32,    int32,   int32);
        handle_type(   int64,    int64,   int64);
        handle_type(  uint32,   uint32,  uint32);
        handle_type(  uint64,   uint64,  uint64);
        handle_type(  sint32,   sint32,   int32);
        handle_type(  sint64,   sint64,   int64);
        handle_type( fixed32,  fixed32,  uint32);
        handle_type( fixed64,  fixed64,  uint64);
        handle_type(sfixed32, sfixed32,   int32);
        handle_type(sfixed64, sfixed64,   int64);
        handle_type(   float,    float,   float);
        handle_type(  double,   double,  double);
        handle_type(    bool,     bool,    bool);
        handle_type(  string,   string,  string);
        handle_type(   bytes,    bytes,  string);
        handle_type(    enum,     enum,    enum);
        handle_type(   group,    group, message);
        handle_type( message,  message, message);
#undef handle_type
      }
    }
  } else if (!is_cleared) {
    switch (real_type(type)) {
#define handle_type(uppercase, camelcase, value)                 \
      case wireformatlite::type_##uppercase:                     \
        wireformatlite::write##camelcase(number, value, output); \
        break

      handle_type(   int32,    int32,    int32_value);
      handle_type(   int64,    int64,    int64_value);
      handle_type(  uint32,   uint32,   uint32_value);
      handle_type(  uint64,   uint64,   uint64_value);
      handle_type(  sint32,   sint32,    int32_value);
      handle_type(  sint64,   sint64,    int64_value);
      handle_type( fixed32,  fixed32,   uint32_value);
      handle_type( fixed64,  fixed64,   uint64_value);
      handle_type(sfixed32, sfixed32,    int32_value);
      handle_type(sfixed64, sfixed64,    int64_value);
      handle_type(   float,    float,    float_value);
      handle_type(  double,   double,   double_value);
      handle_type(    bool,     bool,     bool_value);
      handle_type(  string,   string,  *string_value);
      handle_type(   bytes,    bytes,  *string_value);
      handle_type(    enum,     enum,     enum_value);
      handle_type(   group,    group, *message_value);
#undef handle_type
      case wireformatlite::type_message:
        if (is_lazy) {
          lazymessage_value->writemessage(number, output);
        } else {
          wireformatlite::writemessage(number, *message_value, output);
        }
        break;
    }
  }
}

int extensionset::extension::bytesize(int number) const {
  int result = 0;

  if (is_repeated) {
    if (is_packed) {
      switch (real_type(type)) {
#define handle_type(uppercase, camelcase, lowercase)                        \
        case wireformatlite::type_##uppercase:                              \
          for (int i = 0; i < repeated_##lowercase##_value->size(); i++) {  \
            result += wireformatlite::camelcase##size(                      \
              repeated_##lowercase##_value->get(i));                        \
          }                                                                 \
          break

        handle_type(   int32,    int32,   int32);
        handle_type(   int64,    int64,   int64);
        handle_type(  uint32,   uint32,  uint32);
        handle_type(  uint64,   uint64,  uint64);
        handle_type(  sint32,   sint32,   int32);
        handle_type(  sint64,   sint64,   int64);
        handle_type(    enum,     enum,    enum);
#undef handle_type

        // stuff with fixed size.
#define handle_type(uppercase, camelcase, lowercase)                        \
        case wireformatlite::type_##uppercase:                              \
          result += wireformatlite::k##camelcase##size *                    \
                    repeated_##lowercase##_value->size();                   \
          break
        handle_type( fixed32,  fixed32, uint32);
        handle_type( fixed64,  fixed64, uint64);
        handle_type(sfixed32, sfixed32,  int32);
        handle_type(sfixed64, sfixed64,  int64);
        handle_type(   float,    float,  float);
        handle_type(  double,   double, double);
        handle_type(    bool,     bool,   bool);
#undef handle_type

        case wireformatlite::type_string:
        case wireformatlite::type_bytes:
        case wireformatlite::type_group:
        case wireformatlite::type_message:
          google_log(fatal) << "non-primitive types can't be packed.";
          break;
      }

      cached_size = result;
      if (result > 0) {
        result += io::codedoutputstream::varintsize32(result);
        result += io::codedoutputstream::varintsize32(
            wireformatlite::maketag(number,
                wireformatlite::wiretype_length_delimited));
      }
    } else {
      int tag_size = wireformatlite::tagsize(number, real_type(type));

      switch (real_type(type)) {
#define handle_type(uppercase, camelcase, lowercase)                        \
        case wireformatlite::type_##uppercase:                              \
          result += tag_size * repeated_##lowercase##_value->size();        \
          for (int i = 0; i < repeated_##lowercase##_value->size(); i++) {  \
            result += wireformatlite::camelcase##size(                      \
              repeated_##lowercase##_value->get(i));                        \
          }                                                                 \
          break

        handle_type(   int32,    int32,   int32);
        handle_type(   int64,    int64,   int64);
        handle_type(  uint32,   uint32,  uint32);
        handle_type(  uint64,   uint64,  uint64);
        handle_type(  sint32,   sint32,   int32);
        handle_type(  sint64,   sint64,   int64);
        handle_type(  string,   string,  string);
        handle_type(   bytes,    bytes,  string);
        handle_type(    enum,     enum,    enum);
        handle_type(   group,    group, message);
        handle_type( message,  message, message);
#undef handle_type

        // stuff with fixed size.
#define handle_type(uppercase, camelcase, lowercase)                        \
        case wireformatlite::type_##uppercase:                              \
          result += (tag_size + wireformatlite::k##camelcase##size) *       \
                    repeated_##lowercase##_value->size();                   \
          break
        handle_type( fixed32,  fixed32, uint32);
        handle_type( fixed64,  fixed64, uint64);
        handle_type(sfixed32, sfixed32,  int32);
        handle_type(sfixed64, sfixed64,  int64);
        handle_type(   float,    float,  float);
        handle_type(  double,   double, double);
        handle_type(    bool,     bool,   bool);
#undef handle_type
      }
    }
  } else if (!is_cleared) {
    result += wireformatlite::tagsize(number, real_type(type));
    switch (real_type(type)) {
#define handle_type(uppercase, camelcase, lowercase)                      \
      case wireformatlite::type_##uppercase:                              \
        result += wireformatlite::camelcase##size(lowercase);             \
        break

      handle_type(   int32,    int32,    int32_value);
      handle_type(   int64,    int64,    int64_value);
      handle_type(  uint32,   uint32,   uint32_value);
      handle_type(  uint64,   uint64,   uint64_value);
      handle_type(  sint32,   sint32,    int32_value);
      handle_type(  sint64,   sint64,    int64_value);
      handle_type(  string,   string,  *string_value);
      handle_type(   bytes,    bytes,  *string_value);
      handle_type(    enum,     enum,     enum_value);
      handle_type(   group,    group, *message_value);
#undef handle_type
      case wireformatlite::type_message: {
        if (is_lazy) {
          int size = lazymessage_value->bytesize();
          result += io::codedoutputstream::varintsize32(size) + size;
        } else {
          result += wireformatlite::messagesize(*message_value);
        }
        break;
      }

      // stuff with fixed size.
#define handle_type(uppercase, camelcase)                                 \
      case wireformatlite::type_##uppercase:                              \
        result += wireformatlite::k##camelcase##size;                     \
        break
      handle_type( fixed32,  fixed32);
      handle_type( fixed64,  fixed64);
      handle_type(sfixed32, sfixed32);
      handle_type(sfixed64, sfixed64);
      handle_type(   float,    float);
      handle_type(  double,   double);
      handle_type(    bool,     bool);
#undef handle_type
    }
  }

  return result;
}

int extensionset::extension::getsize() const {
  google_dcheck(is_repeated);
  switch (cpp_type(type)) {
#define handle_type(uppercase, lowercase)                        \
    case wireformatlite::cpptype_##uppercase:                    \
      return repeated_##lowercase##_value->size()

    handle_type(  int32,   int32);
    handle_type(  int64,   int64);
    handle_type( uint32,  uint32);
    handle_type( uint64,  uint64);
    handle_type(  float,   float);
    handle_type( double,  double);
    handle_type(   bool,    bool);
    handle_type(   enum,    enum);
    handle_type( string,  string);
    handle_type(message, message);
#undef handle_type
  }

  google_log(fatal) << "can't get here.";
  return 0;
}

void extensionset::extension::free() {
  if (is_repeated) {
    switch (cpp_type(type)) {
#define handle_type(uppercase, lowercase)                          \
      case wireformatlite::cpptype_##uppercase:                    \
        delete repeated_##lowercase##_value;                       \
        break

      handle_type(  int32,   int32);
      handle_type(  int64,   int64);
      handle_type( uint32,  uint32);
      handle_type( uint64,  uint64);
      handle_type(  float,   float);
      handle_type( double,  double);
      handle_type(   bool,    bool);
      handle_type(   enum,    enum);
      handle_type( string,  string);
      handle_type(message, message);
#undef handle_type
    }
  } else {
    switch (cpp_type(type)) {
      case wireformatlite::cpptype_string:
        delete string_value;
        break;
      case wireformatlite::cpptype_message:
        if (is_lazy) {
          delete lazymessage_value;
        } else {
          delete message_value;
        }
        break;
      default:
        break;
    }
  }
}

// defined in extension_set_heavy.cc.
// int extensionset::extension::spaceusedexcludingself() const

}  // namespace internal
}  // namespace protobuf
}  // namespace google
