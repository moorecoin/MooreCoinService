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
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
namespace internal {

int stringspaceusedexcludingself(const string& str) {
  const void* start = &str;
  const void* end = &str + 1;

  if (start <= str.data() && str.data() <= end) {
    // the string's data is stored inside the string object itself.
    return 0;
  } else {
    return str.capacity();
  }
}

bool parsenamedenum(const enumdescriptor* descriptor,
                    const string& name,
                    int* value) {
  const enumvaluedescriptor* d = descriptor->findvaluebyname(name);
  if (d == null) return false;
  *value = d->number();
  return true;
}

const string& nameofenum(const enumdescriptor* descriptor, int value) {
  const enumvaluedescriptor* d = descriptor->findvaluebynumber(value);
  return (d == null ? kemptystring : d->name());
}

// ===================================================================
// helpers for reporting usage errors (e.g. trying to use getint32() on
// a string field).

namespace {

void reportreflectionusageerror(
    const descriptor* descriptor, const fielddescriptor* field,
    const char* method, const char* description) {
  google_log(fatal)
    << "protocol buffer reflection usage error:\n"
       "  method      : google::protobuf::reflection::" << method << "\n"
       "  message type: " << descriptor->full_name() << "\n"
       "  field       : " << field->full_name() << "\n"
       "  problem     : " << description;
}

const char* cpptype_names_[fielddescriptor::max_cpptype + 1] = {
  "invalid_cpptype",
  "cpptype_int32",
  "cpptype_int64",
  "cpptype_uint32",
  "cpptype_uint64",
  "cpptype_double",
  "cpptype_float",
  "cpptype_bool",
  "cpptype_enum",
  "cpptype_string",
  "cpptype_message"
};

static void reportreflectionusagetypeerror(
    const descriptor* descriptor, const fielddescriptor* field,
    const char* method,
    fielddescriptor::cpptype expected_type) {
  google_log(fatal)
    << "protocol buffer reflection usage error:\n"
       "  method      : google::protobuf::reflection::" << method << "\n"
       "  message type: " << descriptor->full_name() << "\n"
       "  field       : " << field->full_name() << "\n"
       "  problem     : field is not the right type for this message:\n"
       "    expected  : " << cpptype_names_[expected_type] << "\n"
       "    field type: " << cpptype_names_[field->cpp_type()];
}

static void reportreflectionusageenumtypeerror(
    const descriptor* descriptor, const fielddescriptor* field,
    const char* method, const enumvaluedescriptor* value) {
  google_log(fatal)
    << "protocol buffer reflection usage error:\n"
       "  method      : google::protobuf::reflection::" << method << "\n"
       "  message type: " << descriptor->full_name() << "\n"
       "  field       : " << field->full_name() << "\n"
       "  problem     : enum value did not match field type:\n"
       "    expected  : " << field->enum_type()->full_name() << "\n"
       "    actual    : " << value->full_name();
}

#define usage_check(condition, method, error_description)                      \
  if (!(condition))                                                            \
    reportreflectionusageerror(descriptor_, field, #method, error_description)
#define usage_check_eq(a, b, method, error_description)                        \
  usage_check((a) == (b), method, error_description)
#define usage_check_ne(a, b, method, error_description)                        \
  usage_check((a) != (b), method, error_description)

#define usage_check_type(method, cpptype)                                      \
  if (field->cpp_type() != fielddescriptor::cpptype_##cpptype)                 \
    reportreflectionusagetypeerror(descriptor_, field, #method,                \
                                   fielddescriptor::cpptype_##cpptype)

#define usage_check_enum_value(method)                                         \
  if (value->type() != field->enum_type())                                     \
    reportreflectionusageenumtypeerror(descriptor_, field, #method, value)

#define usage_check_message_type(method)                                       \
  usage_check_eq(field->containing_type(), descriptor_,                        \
                 method, "field does not match message type.");
#define usage_check_singular(method)                                           \
  usage_check_ne(field->label(), fielddescriptor::label_repeated, method,      \
                 "field is repeated; the method requires a singular field.")
#define usage_check_repeated(method)                                           \
  usage_check_eq(field->label(), fielddescriptor::label_repeated, method,      \
                 "field is singular; the method requires a repeated field.")

#define usage_check_all(method, label, cpptype)                       \
    usage_check_message_type(method);                                 \
    usage_check_##label(method);                                      \
    usage_check_type(method, cpptype)

}  // namespace

// ===================================================================

generatedmessagereflection::generatedmessagereflection(
    const descriptor* descriptor,
    const message* default_instance,
    const int offsets[],
    int has_bits_offset,
    int unknown_fields_offset,
    int extensions_offset,
    const descriptorpool* descriptor_pool,
    messagefactory* factory,
    int object_size)
  : descriptor_       (descriptor),
    default_instance_ (default_instance),
    offsets_          (offsets),
    has_bits_offset_  (has_bits_offset),
    unknown_fields_offset_(unknown_fields_offset),
    extensions_offset_(extensions_offset),
    object_size_      (object_size),
    descriptor_pool_  ((descriptor_pool == null) ?
                         descriptorpool::generated_pool() :
                         descriptor_pool),
    message_factory_  (factory) {
}

generatedmessagereflection::~generatedmessagereflection() {}

const unknownfieldset& generatedmessagereflection::getunknownfields(
    const message& message) const {
  const void* ptr = reinterpret_cast<const uint8*>(&message) +
                    unknown_fields_offset_;
  return *reinterpret_cast<const unknownfieldset*>(ptr);
}
unknownfieldset* generatedmessagereflection::mutableunknownfields(
    message* message) const {
  void* ptr = reinterpret_cast<uint8*>(message) + unknown_fields_offset_;
  return reinterpret_cast<unknownfieldset*>(ptr);
}

int generatedmessagereflection::spaceused(const message& message) const {
  // object_size_ already includes the in-memory representation of each field
  // in the message, so we only need to account for additional memory used by
  // the fields.
  int total_size = object_size_;

  total_size += getunknownfields(message).spaceusedexcludingself();

  if (extensions_offset_ != -1) {
    total_size += getextensionset(message).spaceusedexcludingself();
  }

  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);

    if (field->is_repeated()) {
      switch (field->cpp_type()) {
#define handle_type(uppercase, lowercase)                                     \
        case fielddescriptor::cpptype_##uppercase :                           \
          total_size += getraw<repeatedfield<lowercase> >(message, field)     \
                          .spaceusedexcludingself();                          \
          break

        handle_type( int32,  int32);
        handle_type( int64,  int64);
        handle_type(uint32, uint32);
        handle_type(uint64, uint64);
        handle_type(double, double);
        handle_type( float,  float);
        handle_type(  bool,   bool);
        handle_type(  enum,    int);
#undef handle_type

        case fielddescriptor::cpptype_string:
          switch (field->options().ctype()) {
            default:  // todo(kenton):  support other string reps.
            case fieldoptions::string:
              total_size += getraw<repeatedptrfield<string> >(message, field)
                              .spaceusedexcludingself();
              break;
          }
          break;

        case fielddescriptor::cpptype_message:
          // we don't know which subclass of repeatedptrfieldbase the type is,
          // so we use repeatedptrfieldbase directly.
          total_size +=
              getraw<repeatedptrfieldbase>(message, field)
                .spaceusedexcludingself<generictypehandler<message> >();
          break;
      }
    } else {
      switch (field->cpp_type()) {
        case fielddescriptor::cpptype_int32 :
        case fielddescriptor::cpptype_int64 :
        case fielddescriptor::cpptype_uint32:
        case fielddescriptor::cpptype_uint64:
        case fielddescriptor::cpptype_double:
        case fielddescriptor::cpptype_float :
        case fielddescriptor::cpptype_bool  :
        case fielddescriptor::cpptype_enum  :
          // field is inline, so we've already counted it.
          break;

        case fielddescriptor::cpptype_string: {
          switch (field->options().ctype()) {
            default:  // todo(kenton):  support other string reps.
            case fieldoptions::string: {
              const string* ptr = getfield<const string*>(message, field);

              // initially, the string points to the default value stored in
              // the prototype. only count the string if it has been changed
              // from the default value.
              const string* default_ptr = defaultraw<const string*>(field);

              if (ptr != default_ptr) {
                // string fields are represented by just a pointer, so also
                // include sizeof(string) as well.
                total_size += sizeof(*ptr) + stringspaceusedexcludingself(*ptr);
              }
              break;
            }
          }
          break;
        }

        case fielddescriptor::cpptype_message:
          if (&message == default_instance_) {
            // for singular fields, the prototype just stores a pointer to the
            // external type's prototype, so there is no extra memory usage.
          } else {
            const message* sub_message = getraw<const message*>(message, field);
            if (sub_message != null) {
              total_size += sub_message->spaceused();
            }
          }
          break;
      }
    }
  }

  return total_size;
}

void generatedmessagereflection::swap(
    message* message1,
    message* message2) const {
  if (message1 == message2) return;

  // todo(kenton):  other reflection methods should probably check this too.
  google_check_eq(message1->getreflection(), this)
    << "first argument to swap() (of type \""
    << message1->getdescriptor()->full_name()
    << "\") is not compatible with this reflection object (which is for type \""
    << descriptor_->full_name()
    << "\").  note that the exact same class is required; not just the same "
       "descriptor.";
  google_check_eq(message2->getreflection(), this)
    << "second argument to swap() (of type \""
    << message2->getdescriptor()->full_name()
    << "\") is not compatible with this reflection object (which is for type \""
    << descriptor_->full_name()
    << "\").  note that the exact same class is required; not just the same "
       "descriptor.";

  uint32* has_bits1 = mutablehasbits(message1);
  uint32* has_bits2 = mutablehasbits(message2);
  int has_bits_size = (descriptor_->field_count() + 31) / 32;

  for (int i = 0; i < has_bits_size; i++) {
    std::swap(has_bits1[i], has_bits2[i]);
  }

  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);
    if (field->is_repeated()) {
      switch (field->cpp_type()) {
#define swap_arrays(cpptype, type)                                           \
        case fielddescriptor::cpptype_##cpptype:                             \
          mutableraw<repeatedfield<type> >(message1, field)->swap(           \
              mutableraw<repeatedfield<type> >(message2, field));            \
          break;

          swap_arrays(int32 , int32 );
          swap_arrays(int64 , int64 );
          swap_arrays(uint32, uint32);
          swap_arrays(uint64, uint64);
          swap_arrays(float , float );
          swap_arrays(double, double);
          swap_arrays(bool  , bool  );
          swap_arrays(enum  , int   );
#undef swap_arrays

        case fielddescriptor::cpptype_string:
        case fielddescriptor::cpptype_message:
          mutableraw<repeatedptrfieldbase>(message1, field)->swap(
              mutableraw<repeatedptrfieldbase>(message2, field));
          break;

        default:
          google_log(fatal) << "unimplemented type: " << field->cpp_type();
      }
    } else {
      switch (field->cpp_type()) {
#define swap_values(cpptype, type)                                           \
        case fielddescriptor::cpptype_##cpptype:                             \
          std::swap(*mutableraw<type>(message1, field),                      \
                    *mutableraw<type>(message2, field));                     \
          break;

          swap_values(int32 , int32 );
          swap_values(int64 , int64 );
          swap_values(uint32, uint32);
          swap_values(uint64, uint64);
          swap_values(float , float );
          swap_values(double, double);
          swap_values(bool  , bool  );
          swap_values(enum  , int   );
#undef swap_values
        case fielddescriptor::cpptype_message:
          std::swap(*mutableraw<message*>(message1, field),
                    *mutableraw<message*>(message2, field));
          break;

        case fielddescriptor::cpptype_string:
          switch (field->options().ctype()) {
            default:  // todo(kenton):  support other string reps.
            case fieldoptions::string:
              std::swap(*mutableraw<string*>(message1, field),
                        *mutableraw<string*>(message2, field));
              break;
          }
          break;

        default:
          google_log(fatal) << "unimplemented type: " << field->cpp_type();
      }
    }
  }

  if (extensions_offset_ != -1) {
    mutableextensionset(message1)->swap(mutableextensionset(message2));
  }

  mutableunknownfields(message1)->swap(mutableunknownfields(message2));
}

// -------------------------------------------------------------------

bool generatedmessagereflection::hasfield(const message& message,
                                          const fielddescriptor* field) const {
  usage_check_message_type(hasfield);
  usage_check_singular(hasfield);

  if (field->is_extension()) {
    return getextensionset(message).has(field->number());
  } else {
    return hasbit(message, field);
  }
}

int generatedmessagereflection::fieldsize(const message& message,
                                          const fielddescriptor* field) const {
  usage_check_message_type(fieldsize);
  usage_check_repeated(fieldsize);

  if (field->is_extension()) {
    return getextensionset(message).extensionsize(field->number());
  } else {
    switch (field->cpp_type()) {
#define handle_type(uppercase, lowercase)                                     \
      case fielddescriptor::cpptype_##uppercase :                             \
        return getraw<repeatedfield<lowercase> >(message, field).size()

      handle_type( int32,  int32);
      handle_type( int64,  int64);
      handle_type(uint32, uint32);
      handle_type(uint64, uint64);
      handle_type(double, double);
      handle_type( float,  float);
      handle_type(  bool,   bool);
      handle_type(  enum,    int);
#undef handle_type

      case fielddescriptor::cpptype_string:
      case fielddescriptor::cpptype_message:
        return getraw<repeatedptrfieldbase>(message, field).size();
    }

    google_log(fatal) << "can't get here.";
    return 0;
  }
}

void generatedmessagereflection::clearfield(
    message* message, const fielddescriptor* field) const {
  usage_check_message_type(clearfield);

  if (field->is_extension()) {
    mutableextensionset(message)->clearextension(field->number());
  } else if (!field->is_repeated()) {
    if (hasbit(*message, field)) {
      clearbit(message, field);

      // we need to set the field back to its default value.
      switch (field->cpp_type()) {
#define clear_type(cpptype, type)                                            \
        case fielddescriptor::cpptype_##cpptype:                             \
          *mutableraw<type>(message, field) =                                \
            field->default_value_##type();                                   \
          break;

        clear_type(int32 , int32 );
        clear_type(int64 , int64 );
        clear_type(uint32, uint32);
        clear_type(uint64, uint64);
        clear_type(float , float );
        clear_type(double, double);
        clear_type(bool  , bool  );
#undef clear_type

        case fielddescriptor::cpptype_enum:
          *mutableraw<int>(message, field) =
            field->default_value_enum()->number();
          break;

        case fielddescriptor::cpptype_string: {
          switch (field->options().ctype()) {
            default:  // todo(kenton):  support other string reps.
            case fieldoptions::string:
              const string* default_ptr = defaultraw<const string*>(field);
              string** value = mutableraw<string*>(message, field);
              if (*value != default_ptr) {
                if (field->has_default_value()) {
                  (*value)->assign(field->default_value_string());
                } else {
                  (*value)->clear();
                }
              }
              break;
          }
          break;
        }

        case fielddescriptor::cpptype_message:
          (*mutableraw<message*>(message, field))->clear();
          break;
      }
    }
  } else {
    switch (field->cpp_type()) {
#define handle_type(uppercase, lowercase)                                     \
      case fielddescriptor::cpptype_##uppercase :                             \
        mutableraw<repeatedfield<lowercase> >(message, field)->clear();       \
        break

      handle_type( int32,  int32);
      handle_type( int64,  int64);
      handle_type(uint32, uint32);
      handle_type(uint64, uint64);
      handle_type(double, double);
      handle_type( float,  float);
      handle_type(  bool,   bool);
      handle_type(  enum,    int);
#undef handle_type

      case fielddescriptor::cpptype_string: {
        switch (field->options().ctype()) {
          default:  // todo(kenton):  support other string reps.
          case fieldoptions::string:
            mutableraw<repeatedptrfield<string> >(message, field)->clear();
            break;
        }
        break;
      }

      case fielddescriptor::cpptype_message: {
        // we don't know which subclass of repeatedptrfieldbase the type is,
        // so we use repeatedptrfieldbase directly.
        mutableraw<repeatedptrfieldbase>(message, field)
            ->clear<generictypehandler<message> >();
        break;
      }
    }
  }
}

void generatedmessagereflection::removelast(
    message* message,
    const fielddescriptor* field) const {
  usage_check_message_type(removelast);
  usage_check_repeated(removelast);

  if (field->is_extension()) {
    mutableextensionset(message)->removelast(field->number());
  } else {
    switch (field->cpp_type()) {
#define handle_type(uppercase, lowercase)                                     \
      case fielddescriptor::cpptype_##uppercase :                             \
        mutableraw<repeatedfield<lowercase> >(message, field)->removelast();  \
        break

      handle_type( int32,  int32);
      handle_type( int64,  int64);
      handle_type(uint32, uint32);
      handle_type(uint64, uint64);
      handle_type(double, double);
      handle_type( float,  float);
      handle_type(  bool,   bool);
      handle_type(  enum,    int);
#undef handle_type

      case fielddescriptor::cpptype_string:
        switch (field->options().ctype()) {
          default:  // todo(kenton):  support other string reps.
          case fieldoptions::string:
            mutableraw<repeatedptrfield<string> >(message, field)->removelast();
            break;
        }
        break;

      case fielddescriptor::cpptype_message:
        mutableraw<repeatedptrfieldbase>(message, field)
            ->removelast<generictypehandler<message> >();
        break;
    }
  }
}

message* generatedmessagereflection::releaselast(
    message* message,
    const fielddescriptor* field) const {
  usage_check_all(releaselast, repeated, message);

  if (field->is_extension()) {
    return static_cast<message*>(
        mutableextensionset(message)->releaselast(field->number()));
  } else {
    return mutableraw<repeatedptrfieldbase>(message, field)
        ->releaselast<generictypehandler<message> >();
  }
}

void generatedmessagereflection::swapelements(
    message* message,
    const fielddescriptor* field,
    int index1,
    int index2) const {
  usage_check_message_type(swap);
  usage_check_repeated(swap);

  if (field->is_extension()) {
    mutableextensionset(message)->swapelements(field->number(), index1, index2);
  } else {
    switch (field->cpp_type()) {
#define handle_type(uppercase, lowercase)                                     \
      case fielddescriptor::cpptype_##uppercase :                             \
        mutableraw<repeatedfield<lowercase> >(message, field)                 \
            ->swapelements(index1, index2);                                   \
        break

      handle_type( int32,  int32);
      handle_type( int64,  int64);
      handle_type(uint32, uint32);
      handle_type(uint64, uint64);
      handle_type(double, double);
      handle_type( float,  float);
      handle_type(  bool,   bool);
      handle_type(  enum,    int);
#undef handle_type

      case fielddescriptor::cpptype_string:
      case fielddescriptor::cpptype_message:
        mutableraw<repeatedptrfieldbase>(message, field)
            ->swapelements(index1, index2);
        break;
    }
  }
}

namespace {
// comparison functor for sorting fielddescriptors by field number.
struct fieldnumbersorter {
  bool operator()(const fielddescriptor* left,
                  const fielddescriptor* right) const {
    return left->number() < right->number();
  }
};
}  // namespace

void generatedmessagereflection::listfields(
    const message& message,
    vector<const fielddescriptor*>* output) const {
  output->clear();

  // optimization:  the default instance never has any fields set.
  if (&message == default_instance_) return;

  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);
    if (field->is_repeated()) {
      if (fieldsize(message, field) > 0) {
        output->push_back(field);
      }
    } else {
      if (hasbit(message, field)) {
        output->push_back(field);
      }
    }
  }

  if (extensions_offset_ != -1) {
    getextensionset(message).appendtolist(descriptor_, descriptor_pool_,
                                          output);
  }

  // listfields() must sort output by field number.
  sort(output->begin(), output->end(), fieldnumbersorter());
}

// -------------------------------------------------------------------

#undef define_primitive_accessors
#define define_primitive_accessors(typename, type, passtype, cpptype)        \
  passtype generatedmessagereflection::get##typename(                        \
      const message& message, const fielddescriptor* field) const {          \
    usage_check_all(get##typename, singular, cpptype);                       \
    if (field->is_extension()) {                                             \
      return getextensionset(message).get##typename(                         \
        field->number(), field->default_value_##passtype());                 \
    } else {                                                                 \
      return getfield<type>(message, field);                                 \
    }                                                                        \
  }                                                                          \
                                                                             \
  void generatedmessagereflection::set##typename(                            \
      message* message, const fielddescriptor* field,                        \
      passtype value) const {                                                \
    usage_check_all(set##typename, singular, cpptype);                       \
    if (field->is_extension()) {                                             \
      return mutableextensionset(message)->set##typename(                    \
        field->number(), field->type(), value, field);                       \
    } else {                                                                 \
      setfield<type>(message, field, value);                                 \
    }                                                                        \
  }                                                                          \
                                                                             \
  passtype generatedmessagereflection::getrepeated##typename(                \
      const message& message,                                                \
      const fielddescriptor* field, int index) const {                       \
    usage_check_all(getrepeated##typename, repeated, cpptype);               \
    if (field->is_extension()) {                                             \
      return getextensionset(message).getrepeated##typename(                 \
        field->number(), index);                                             \
    } else {                                                                 \
      return getrepeatedfield<type>(message, field, index);                  \
    }                                                                        \
  }                                                                          \
                                                                             \
  void generatedmessagereflection::setrepeated##typename(                    \
      message* message, const fielddescriptor* field,                        \
      int index, passtype value) const {                                     \
    usage_check_all(setrepeated##typename, repeated, cpptype);               \
    if (field->is_extension()) {                                             \
      mutableextensionset(message)->setrepeated##typename(                   \
        field->number(), index, value);                                      \
    } else {                                                                 \
      setrepeatedfield<type>(message, field, index, value);                  \
    }                                                                        \
  }                                                                          \
                                                                             \
  void generatedmessagereflection::add##typename(                            \
      message* message, const fielddescriptor* field,                        \
      passtype value) const {                                                \
    usage_check_all(add##typename, repeated, cpptype);                       \
    if (field->is_extension()) {                                             \
      mutableextensionset(message)->add##typename(                           \
        field->number(), field->type(), field->options().packed(), value,    \
        field);                                                              \
    } else {                                                                 \
      addfield<type>(message, field, value);                                 \
    }                                                                        \
  }

define_primitive_accessors(int32 , int32 , int32 , int32 )
define_primitive_accessors(int64 , int64 , int64 , int64 )
define_primitive_accessors(uint32, uint32, uint32, uint32)
define_primitive_accessors(uint64, uint64, uint64, uint64)
define_primitive_accessors(float , float , float , float )
define_primitive_accessors(double, double, double, double)
define_primitive_accessors(bool  , bool  , bool  , bool  )
#undef define_primitive_accessors

// -------------------------------------------------------------------

string generatedmessagereflection::getstring(
    const message& message, const fielddescriptor* field) const {
  usage_check_all(getstring, singular, string);
  if (field->is_extension()) {
    return getextensionset(message).getstring(field->number(),
                                              field->default_value_string());
  } else {
    switch (field->options().ctype()) {
      default:  // todo(kenton):  support other string reps.
      case fieldoptions::string:
        return *getfield<const string*>(message, field);
    }

    google_log(fatal) << "can't get here.";
    return kemptystring;  // make compiler happy.
  }
}

const string& generatedmessagereflection::getstringreference(
    const message& message,
    const fielddescriptor* field, string* scratch) const {
  usage_check_all(getstringreference, singular, string);
  if (field->is_extension()) {
    return getextensionset(message).getstring(field->number(),
                                              field->default_value_string());
  } else {
    switch (field->options().ctype()) {
      default:  // todo(kenton):  support other string reps.
      case fieldoptions::string:
        return *getfield<const string*>(message, field);
    }

    google_log(fatal) << "can't get here.";
    return kemptystring;  // make compiler happy.
  }
}


void generatedmessagereflection::setstring(
    message* message, const fielddescriptor* field,
    const string& value) const {
  usage_check_all(setstring, singular, string);
  if (field->is_extension()) {
    return mutableextensionset(message)->setstring(field->number(),
                                                   field->type(), value, field);
  } else {
    switch (field->options().ctype()) {
      default:  // todo(kenton):  support other string reps.
      case fieldoptions::string: {
        string** ptr = mutablefield<string*>(message, field);
        if (*ptr == defaultraw<const string*>(field)) {
          *ptr = new string(value);
        } else {
          (*ptr)->assign(value);
        }
        break;
      }
    }
  }
}


string generatedmessagereflection::getrepeatedstring(
    const message& message, const fielddescriptor* field, int index) const {
  usage_check_all(getrepeatedstring, repeated, string);
  if (field->is_extension()) {
    return getextensionset(message).getrepeatedstring(field->number(), index);
  } else {
    switch (field->options().ctype()) {
      default:  // todo(kenton):  support other string reps.
      case fieldoptions::string:
        return getrepeatedptrfield<string>(message, field, index);
    }

    google_log(fatal) << "can't get here.";
    return kemptystring;  // make compiler happy.
  }
}

const string& generatedmessagereflection::getrepeatedstringreference(
    const message& message, const fielddescriptor* field,
    int index, string* scratch) const {
  usage_check_all(getrepeatedstringreference, repeated, string);
  if (field->is_extension()) {
    return getextensionset(message).getrepeatedstring(field->number(), index);
  } else {
    switch (field->options().ctype()) {
      default:  // todo(kenton):  support other string reps.
      case fieldoptions::string:
        return getrepeatedptrfield<string>(message, field, index);
    }

    google_log(fatal) << "can't get here.";
    return kemptystring;  // make compiler happy.
  }
}


void generatedmessagereflection::setrepeatedstring(
    message* message, const fielddescriptor* field,
    int index, const string& value) const {
  usage_check_all(setrepeatedstring, repeated, string);
  if (field->is_extension()) {
    mutableextensionset(message)->setrepeatedstring(
      field->number(), index, value);
  } else {
    switch (field->options().ctype()) {
      default:  // todo(kenton):  support other string reps.
      case fieldoptions::string:
        *mutablerepeatedfield<string>(message, field, index) = value;
        break;
    }
  }
}


void generatedmessagereflection::addstring(
    message* message, const fielddescriptor* field,
    const string& value) const {
  usage_check_all(addstring, repeated, string);
  if (field->is_extension()) {
    mutableextensionset(message)->addstring(field->number(),
                                            field->type(), value, field);
  } else {
    switch (field->options().ctype()) {
      default:  // todo(kenton):  support other string reps.
      case fieldoptions::string:
        *addfield<string>(message, field) = value;
        break;
    }
  }
}


// -------------------------------------------------------------------

const enumvaluedescriptor* generatedmessagereflection::getenum(
    const message& message, const fielddescriptor* field) const {
  usage_check_all(getenum, singular, enum);

  int value;
  if (field->is_extension()) {
    value = getextensionset(message).getenum(
      field->number(), field->default_value_enum()->number());
  } else {
    value = getfield<int>(message, field);
  }
  const enumvaluedescriptor* result =
    field->enum_type()->findvaluebynumber(value);
  google_check(result != null) << "value " << value << " is not valid for field "
                        << field->full_name() << " of type "
                        << field->enum_type()->full_name() << ".";
  return result;
}

void generatedmessagereflection::setenum(
    message* message, const fielddescriptor* field,
    const enumvaluedescriptor* value) const {
  usage_check_all(setenum, singular, enum);
  usage_check_enum_value(setenum);

  if (field->is_extension()) {
    mutableextensionset(message)->setenum(field->number(), field->type(),
                                          value->number(), field);
  } else {
    setfield<int>(message, field, value->number());
  }
}

const enumvaluedescriptor* generatedmessagereflection::getrepeatedenum(
    const message& message, const fielddescriptor* field, int index) const {
  usage_check_all(getrepeatedenum, repeated, enum);

  int value;
  if (field->is_extension()) {
    value = getextensionset(message).getrepeatedenum(field->number(), index);
  } else {
    value = getrepeatedfield<int>(message, field, index);
  }
  const enumvaluedescriptor* result =
    field->enum_type()->findvaluebynumber(value);
  google_check(result != null) << "value " << value << " is not valid for field "
                        << field->full_name() << " of type "
                        << field->enum_type()->full_name() << ".";
  return result;
}

void generatedmessagereflection::setrepeatedenum(
    message* message,
    const fielddescriptor* field, int index,
    const enumvaluedescriptor* value) const {
  usage_check_all(setrepeatedenum, repeated, enum);
  usage_check_enum_value(setrepeatedenum);

  if (field->is_extension()) {
    mutableextensionset(message)->setrepeatedenum(
      field->number(), index, value->number());
  } else {
    setrepeatedfield<int>(message, field, index, value->number());
  }
}

void generatedmessagereflection::addenum(
    message* message, const fielddescriptor* field,
    const enumvaluedescriptor* value) const {
  usage_check_all(addenum, repeated, enum);
  usage_check_enum_value(addenum);

  if (field->is_extension()) {
    mutableextensionset(message)->addenum(field->number(), field->type(),
                                          field->options().packed(),
                                          value->number(), field);
  } else {
    addfield<int>(message, field, value->number());
  }
}

// -------------------------------------------------------------------

const message& generatedmessagereflection::getmessage(
    const message& message, const fielddescriptor* field,
    messagefactory* factory) const {
  usage_check_all(getmessage, singular, message);

  if (factory == null) factory = message_factory_;

  if (field->is_extension()) {
    return static_cast<const message&>(
        getextensionset(message).getmessage(
          field->number(), field->message_type(), factory));
  } else {
    const message* result;
    result = getraw<const message*>(message, field);
    if (result == null) {
      result = defaultraw<const message*>(field);
    }
    return *result;
  }
}

message* generatedmessagereflection::mutablemessage(
    message* message, const fielddescriptor* field,
    messagefactory* factory) const {
  usage_check_all(mutablemessage, singular, message);

  if (factory == null) factory = message_factory_;

  if (field->is_extension()) {
    return static_cast<message*>(
        mutableextensionset(message)->mutablemessage(field, factory));
  } else {
    message* result;
    message** result_holder = mutablefield<message*>(message, field);
    if (*result_holder == null) {
      const message* default_message = defaultraw<const message*>(field);
      *result_holder = default_message->new();
    }
    result = *result_holder;
    return result;
  }
}

message* generatedmessagereflection::releasemessage(
    message* message,
    const fielddescriptor* field,
    messagefactory* factory) const {
  usage_check_all(releasemessage, singular, message);

  if (factory == null) factory = message_factory_;

  if (field->is_extension()) {
    return static_cast<message*>(
        mutableextensionset(message)->releasemessage(field, factory));
  } else {
    clearbit(message, field);
    message** result = mutableraw<message*>(message, field);
    message* ret = *result;
    *result = null;
    return ret;
  }
}

const message& generatedmessagereflection::getrepeatedmessage(
    const message& message, const fielddescriptor* field, int index) const {
  usage_check_all(getrepeatedmessage, repeated, message);

  if (field->is_extension()) {
    return static_cast<const message&>(
        getextensionset(message).getrepeatedmessage(field->number(), index));
  } else {
    return getraw<repeatedptrfieldbase>(message, field)
        .get<generictypehandler<message> >(index);
  }
}

message* generatedmessagereflection::mutablerepeatedmessage(
    message* message, const fielddescriptor* field, int index) const {
  usage_check_all(mutablerepeatedmessage, repeated, message);

  if (field->is_extension()) {
    return static_cast<message*>(
        mutableextensionset(message)->mutablerepeatedmessage(
          field->number(), index));
  } else {
    return mutableraw<repeatedptrfieldbase>(message, field)
        ->mutable<generictypehandler<message> >(index);
  }
}

message* generatedmessagereflection::addmessage(
    message* message, const fielddescriptor* field,
    messagefactory* factory) const {
  usage_check_all(addmessage, repeated, message);

  if (factory == null) factory = message_factory_;

  if (field->is_extension()) {
    return static_cast<message*>(
        mutableextensionset(message)->addmessage(field, factory));
  } else {
    // we can't use addfield<message>() because repeatedptrfieldbase doesn't
    // know how to allocate one.
    repeatedptrfieldbase* repeated =
        mutableraw<repeatedptrfieldbase>(message, field);
    message* result = repeated->addfromcleared<generictypehandler<message> >();
    if (result == null) {
      // we must allocate a new object.
      const message* prototype;
      if (repeated->size() == 0) {
        prototype = factory->getprototype(field->message_type());
      } else {
        prototype = &repeated->get<generictypehandler<message> >(0);
      }
      result = prototype->new();
      repeated->addallocated<generictypehandler<message> >(result);
    }
    return result;
  }
}

void* generatedmessagereflection::mutablerawrepeatedfield(
    message* message, const fielddescriptor* field,
    fielddescriptor::cpptype cpptype,
    int ctype, const descriptor* desc) const {
  usage_check_repeated("mutablerawrepeatedfield");
  if (field->cpp_type() != cpptype)
    reportreflectionusagetypeerror(descriptor_,
        field, "mutablerawrepeatedfield", cpptype);
  if (ctype >= 0)
    google_check_eq(field->options().ctype(), ctype) << "subtype mismatch";
  if (desc != null)
    google_check_eq(field->message_type(), desc) << "wrong submessage type";
  if (field->is_extension())
    return mutableextensionset(message)->mutablerawrepeatedfield(
        field->number());
  else
    return reinterpret_cast<uint8*>(message) + offsets_[field->index()];
}

// -----------------------------------------------------------------------------

const fielddescriptor* generatedmessagereflection::findknownextensionbyname(
    const string& name) const {
  if (extensions_offset_ == -1) return null;

  const fielddescriptor* result = descriptor_pool_->findextensionbyname(name);
  if (result != null && result->containing_type() == descriptor_) {
    return result;
  }

  if (descriptor_->options().message_set_wire_format()) {
    // messageset extensions may be identified by type name.
    const descriptor* type = descriptor_pool_->findmessagetypebyname(name);
    if (type != null) {
      // look for a matching extension in the foreign type's scope.
      for (int i = 0; i < type->extension_count(); i++) {
        const fielddescriptor* extension = type->extension(i);
        if (extension->containing_type() == descriptor_ &&
            extension->type() == fielddescriptor::type_message &&
            extension->is_optional() &&
            extension->message_type() == type) {
          // found it.
          return extension;
        }
      }
    }
  }

  return null;
}

const fielddescriptor* generatedmessagereflection::findknownextensionbynumber(
    int number) const {
  if (extensions_offset_ == -1) return null;
  return descriptor_pool_->findextensionbynumber(descriptor_, number);
}

// ===================================================================
// some private helpers.

// these simple template accessors obtain pointers (or references) to
// the given field.
template <typename type>
inline const type& generatedmessagereflection::getraw(
    const message& message, const fielddescriptor* field) const {
  const void* ptr = reinterpret_cast<const uint8*>(&message) +
                    offsets_[field->index()];
  return *reinterpret_cast<const type*>(ptr);
}

template <typename type>
inline type* generatedmessagereflection::mutableraw(
    message* message, const fielddescriptor* field) const {
  void* ptr = reinterpret_cast<uint8*>(message) + offsets_[field->index()];
  return reinterpret_cast<type*>(ptr);
}

template <typename type>
inline const type& generatedmessagereflection::defaultraw(
    const fielddescriptor* field) const {
  const void* ptr = reinterpret_cast<const uint8*>(default_instance_) +
                    offsets_[field->index()];
  return *reinterpret_cast<const type*>(ptr);
}

inline const uint32* generatedmessagereflection::gethasbits(
    const message& message) const {
  const void* ptr = reinterpret_cast<const uint8*>(&message) + has_bits_offset_;
  return reinterpret_cast<const uint32*>(ptr);
}
inline uint32* generatedmessagereflection::mutablehasbits(
    message* message) const {
  void* ptr = reinterpret_cast<uint8*>(message) + has_bits_offset_;
  return reinterpret_cast<uint32*>(ptr);
}

inline const extensionset& generatedmessagereflection::getextensionset(
    const message& message) const {
  google_dcheck_ne(extensions_offset_, -1);
  const void* ptr = reinterpret_cast<const uint8*>(&message) +
                    extensions_offset_;
  return *reinterpret_cast<const extensionset*>(ptr);
}
inline extensionset* generatedmessagereflection::mutableextensionset(
    message* message) const {
  google_dcheck_ne(extensions_offset_, -1);
  void* ptr = reinterpret_cast<uint8*>(message) + extensions_offset_;
  return reinterpret_cast<extensionset*>(ptr);
}

// simple accessors for manipulating has_bits_.
inline bool generatedmessagereflection::hasbit(
    const message& message, const fielddescriptor* field) const {
  return gethasbits(message)[field->index() / 32] &
    (1 << (field->index() % 32));
}

inline void generatedmessagereflection::setbit(
    message* message, const fielddescriptor* field) const {
  mutablehasbits(message)[field->index() / 32] |= (1 << (field->index() % 32));
}

inline void generatedmessagereflection::clearbit(
    message* message, const fielddescriptor* field) const {
  mutablehasbits(message)[field->index() / 32] &= ~(1 << (field->index() % 32));
}

// template implementations of basic accessors.  inline because each
// template instance is only called from one location.  these are
// used for all types except messages.
template <typename type>
inline const type& generatedmessagereflection::getfield(
    const message& message, const fielddescriptor* field) const {
  return getraw<type>(message, field);
}

template <typename type>
inline void generatedmessagereflection::setfield(
    message* message, const fielddescriptor* field, const type& value) const {
  *mutableraw<type>(message, field) = value;
  setbit(message, field);
}

template <typename type>
inline type* generatedmessagereflection::mutablefield(
    message* message, const fielddescriptor* field) const {
  setbit(message, field);
  return mutableraw<type>(message, field);
}

template <typename type>
inline const type& generatedmessagereflection::getrepeatedfield(
    const message& message, const fielddescriptor* field, int index) const {
  return getraw<repeatedfield<type> >(message, field).get(index);
}

template <typename type>
inline const type& generatedmessagereflection::getrepeatedptrfield(
    const message& message, const fielddescriptor* field, int index) const {
  return getraw<repeatedptrfield<type> >(message, field).get(index);
}

template <typename type>
inline void generatedmessagereflection::setrepeatedfield(
    message* message, const fielddescriptor* field,
    int index, type value) const {
  mutableraw<repeatedfield<type> >(message, field)->set(index, value);
}

template <typename type>
inline type* generatedmessagereflection::mutablerepeatedfield(
    message* message, const fielddescriptor* field, int index) const {
  repeatedptrfield<type>* repeated =
    mutableraw<repeatedptrfield<type> >(message, field);
  return repeated->mutable(index);
}

template <typename type>
inline void generatedmessagereflection::addfield(
    message* message, const fielddescriptor* field, const type& value) const {
  mutableraw<repeatedfield<type> >(message, field)->add(value);
}

template <typename type>
inline type* generatedmessagereflection::addfield(
    message* message, const fielddescriptor* field) const {
  repeatedptrfield<type>* repeated =
    mutableraw<repeatedptrfield<type> >(message, field);
  return repeated->add();
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google
