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
// contains methods defined in extension_set.h which cannot be part of the
// lite library because they use descriptors or reflection.

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/wire_format_lite_inl.h>

namespace google {

namespace protobuf {
namespace internal {


// implementation of extensionfinder which finds extensions in a given
// descriptorpool, using the given messagefactory to construct sub-objects.
// this class is implemented in extension_set_heavy.cc.
class descriptorpoolextensionfinder : public extensionfinder {
 public:
  descriptorpoolextensionfinder(const descriptorpool* pool,
                                messagefactory* factory,
                                const descriptor* containing_type)
      : pool_(pool), factory_(factory), containing_type_(containing_type) {}
  virtual ~descriptorpoolextensionfinder() {}

  virtual bool find(int number, extensioninfo* output);

 private:
  const descriptorpool* pool_;
  messagefactory* factory_;
  const descriptor* containing_type_;
};

void extensionset::appendtolist(const descriptor* containing_type,
                                const descriptorpool* pool,
                                vector<const fielddescriptor*>* output) const {
  for (map<int, extension>::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    bool has = false;
    if (iter->second.is_repeated) {
      has = iter->second.getsize() > 0;
    } else {
      has = !iter->second.is_cleared;
    }

    if (has) {
      // todo(kenton): looking up each field by number is somewhat unfortunate.
      //   is there a better way?  the problem is that descriptors are lazily-
      //   initialized, so they might not even be constructed until
      //   appendtolist() is called.

      if (iter->second.descriptor == null) {
        output->push_back(pool->findextensionbynumber(
            containing_type, iter->first));
      } else {
        output->push_back(iter->second.descriptor);
      }
    }
  }
}

inline fielddescriptor::type real_type_heavy(fieldtype type) {
  google_dcheck(type > 0 && type <= fielddescriptor::max_type);
  return static_cast<fielddescriptor::type>(type);
}

inline fielddescriptor::cpptype cpp_type_heavy(fieldtype type) {
  return fielddescriptor::typetocpptype(
      static_cast<fielddescriptor::type>(type));
}

inline wireformatlite::fieldtype field_type(fieldtype type) {
  google_dcheck(type > 0 && type <= wireformatlite::max_field_type);
  return static_cast<wireformatlite::fieldtype>(type);
}

#define google_dcheck_type(extension, label, cpptype)                            \
  google_dcheck_eq((extension).is_repeated ? fielddescriptor::label_repeated     \
                                  : fielddescriptor::label_optional,      \
            fielddescriptor::label_##label);                              \
  google_dcheck_eq(cpp_type_heavy((extension).type), fielddescriptor::cpptype_##cpptype)

const messagelite& extensionset::getmessage(int number,
                                            const descriptor* message_type,
                                            messagefactory* factory) const {
  map<int, extension>::const_iterator iter = extensions_.find(number);
  if (iter == extensions_.end() || iter->second.is_cleared) {
    // not present.  return the default value.
    return *factory->getprototype(message_type);
  } else {
    google_dcheck_type(iter->second, optional, message);
    if (iter->second.is_lazy) {
      return iter->second.lazymessage_value->getmessage(
          *factory->getprototype(message_type));
    } else {
      return *iter->second.message_value;
    }
  }
}

messagelite* extensionset::mutablemessage(const fielddescriptor* descriptor,
                                          messagefactory* factory) {
  extension* extension;
  if (maybenewextension(descriptor->number(), descriptor, &extension)) {
    extension->type = descriptor->type();
    google_dcheck_eq(cpp_type_heavy(extension->type), fielddescriptor::cpptype_message);
    extension->is_repeated = false;
    extension->is_packed = false;
    const messagelite* prototype =
        factory->getprototype(descriptor->message_type());
    extension->is_lazy = false;
    extension->message_value = prototype->new();
    extension->is_cleared = false;
    return extension->message_value;
  } else {
    google_dcheck_type(*extension, optional, message);
    extension->is_cleared = false;
    if (extension->is_lazy) {
      return extension->lazymessage_value->mutablemessage(
          *factory->getprototype(descriptor->message_type()));
    } else {
      return extension->message_value;
    }
  }
}

messagelite* extensionset::releasemessage(const fielddescriptor* descriptor,
                                          messagefactory* factory) {
  map<int, extension>::iterator iter = extensions_.find(descriptor->number());
  if (iter == extensions_.end()) {
    // not present.  return null.
    return null;
  } else {
    google_dcheck_type(iter->second, optional, message);
    messagelite* ret = null;
    if (iter->second.is_lazy) {
      ret = iter->second.lazymessage_value->releasemessage(
        *factory->getprototype(descriptor->message_type()));
      delete iter->second.lazymessage_value;
    } else {
      ret = iter->second.message_value;
    }
    extensions_.erase(descriptor->number());
    return ret;
  }
}

messagelite* extensionset::addmessage(const fielddescriptor* descriptor,
                                      messagefactory* factory) {
  extension* extension;
  if (maybenewextension(descriptor->number(), descriptor, &extension)) {
    extension->type = descriptor->type();
    google_dcheck_eq(cpp_type_heavy(extension->type), fielddescriptor::cpptype_message);
    extension->is_repeated = true;
    extension->repeated_message_value =
      new repeatedptrfield<messagelite>();
  } else {
    google_dcheck_type(*extension, repeated, message);
  }

  // repeatedptrfield<message> does not know how to add() since it cannot
  // allocate an abstract object, so we have to be tricky.
  messagelite* result = extension->repeated_message_value
      ->addfromcleared<generictypehandler<messagelite> >();
  if (result == null) {
    const messagelite* prototype;
    if (extension->repeated_message_value->size() == 0) {
      prototype = factory->getprototype(descriptor->message_type());
      google_check(prototype != null);
    } else {
      prototype = &extension->repeated_message_value->get(0);
    }
    result = prototype->new();
    extension->repeated_message_value->addallocated(result);
  }
  return result;
}

static bool validateenumusingdescriptor(const void* arg, int number) {
  return reinterpret_cast<const enumdescriptor*>(arg)
      ->findvaluebynumber(number) != null;
}

bool descriptorpoolextensionfinder::find(int number, extensioninfo* output) {
  const fielddescriptor* extension =
      pool_->findextensionbynumber(containing_type_, number);
  if (extension == null) {
    return false;
  } else {
    output->type = extension->type();
    output->is_repeated = extension->is_repeated();
    output->is_packed = extension->options().packed();
    output->descriptor = extension;
    if (extension->cpp_type() == fielddescriptor::cpptype_message) {
      output->message_prototype =
          factory_->getprototype(extension->message_type());
      google_check(output->message_prototype != null)
          << "extension factory's getprototype() returned null for extension: "
          << extension->full_name();
    } else if (extension->cpp_type() == fielddescriptor::cpptype_enum) {
      output->enum_validity_check.func = validateenumusingdescriptor;
      output->enum_validity_check.arg = extension->enum_type();
    }

    return true;
  }
}

bool extensionset::parsefield(uint32 tag, io::codedinputstream* input,
                              const message* containing_type,
                              unknownfieldset* unknown_fields) {
  unknownfieldsetfieldskipper skipper(unknown_fields);
  if (input->getextensionpool() == null) {
    generatedextensionfinder finder(containing_type);
    return parsefield(tag, input, &finder, &skipper);
  } else {
    descriptorpoolextensionfinder finder(input->getextensionpool(),
                                         input->getextensionfactory(),
                                         containing_type->getdescriptor());
    return parsefield(tag, input, &finder, &skipper);
  }
}

bool extensionset::parsemessageset(io::codedinputstream* input,
                                   const message* containing_type,
                                   unknownfieldset* unknown_fields) {
  unknownfieldsetfieldskipper skipper(unknown_fields);
  if (input->getextensionpool() == null) {
    generatedextensionfinder finder(containing_type);
    return parsemessageset(input, &finder, &skipper);
  } else {
    descriptorpoolextensionfinder finder(input->getextensionpool(),
                                         input->getextensionfactory(),
                                         containing_type->getdescriptor());
    return parsemessageset(input, &finder, &skipper);
  }
}

int extensionset::spaceusedexcludingself() const {
  int total_size =
      extensions_.size() * sizeof(map<int, extension>::value_type);
  for (map<int, extension>::const_iterator iter = extensions_.begin(),
       end = extensions_.end();
       iter != end;
       ++iter) {
    total_size += iter->second.spaceusedexcludingself();
  }
  return total_size;
}

inline int extensionset::repeatedmessage_spaceusedexcludingself(
    repeatedptrfieldbase* field) {
  return field->spaceusedexcludingself<generictypehandler<message> >();
}

int extensionset::extension::spaceusedexcludingself() const {
  int total_size = 0;
  if (is_repeated) {
    switch (cpp_type_heavy(type)) {
#define handle_type(uppercase, lowercase)                          \
      case fielddescriptor::cpptype_##uppercase:                   \
        total_size += sizeof(*repeated_##lowercase##_value) +      \
            repeated_##lowercase##_value->spaceusedexcludingself();\
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
#undef handle_type

      case fielddescriptor::cpptype_message:
        // repeated_message_value is actually a repeatedptrfield<messagelite>,
        // but messagelite has no spaceused(), so we must directly call
        // repeatedptrfieldbase::spaceusedexcludingself() with a different type
        // handler.
        total_size += sizeof(*repeated_message_value) +
            repeatedmessage_spaceusedexcludingself(repeated_message_value);
        break;
    }
  } else {
    switch (cpp_type_heavy(type)) {
      case fielddescriptor::cpptype_string:
        total_size += sizeof(*string_value) +
                      stringspaceusedexcludingself(*string_value);
        break;
      case fielddescriptor::cpptype_message:
        if (is_lazy) {
          total_size += lazymessage_value->spaceused();
        } else {
          total_size += down_cast<message*>(message_value)->spaceused();
        }
        break;
      default:
        // no extra storage costs for primitive types.
        break;
    }
  }
  return total_size;
}

// the serialize*toarray methods are only needed in the heavy library, as
// the lite library only generates serializewithcachedsizes.
uint8* extensionset::serializewithcachedsizestoarray(
    int start_field_number, int end_field_number,
    uint8* target) const {
  map<int, extension>::const_iterator iter;
  for (iter = extensions_.lower_bound(start_field_number);
       iter != extensions_.end() && iter->first < end_field_number;
       ++iter) {
    target = iter->second.serializefieldwithcachedsizestoarray(iter->first,
                                                               target);
  }
  return target;
}

uint8* extensionset::serializemessagesetwithcachedsizestoarray(
    uint8* target) const {
  map<int, extension>::const_iterator iter;
  for (iter = extensions_.begin(); iter != extensions_.end(); ++iter) {
    target = iter->second.serializemessagesetitemwithcachedsizestoarray(
        iter->first, target);
  }
  return target;
}

uint8* extensionset::extension::serializefieldwithcachedsizestoarray(
    int number, uint8* target) const {
  if (is_repeated) {
    if (is_packed) {
      if (cached_size == 0) return target;

      target = wireformatlite::writetagtoarray(number,
          wireformatlite::wiretype_length_delimited, target);
      target = wireformatlite::writeint32notagtoarray(cached_size, target);

      switch (real_type_heavy(type)) {
#define handle_type(uppercase, camelcase, lowercase)                        \
        case fielddescriptor::type_##uppercase:                             \
          for (int i = 0; i < repeated_##lowercase##_value->size(); i++) {  \
            target = wireformatlite::write##camelcase##notagtoarray(        \
              repeated_##lowercase##_value->get(i), target);                \
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
      switch (real_type_heavy(type)) {
#define handle_type(uppercase, camelcase, lowercase)                        \
        case fielddescriptor::type_##uppercase:                             \
          for (int i = 0; i < repeated_##lowercase##_value->size(); i++) {  \
            target = wireformatlite::write##camelcase##toarray(number,      \
              repeated_##lowercase##_value->get(i), target);                \
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
    switch (real_type_heavy(type)) {
#define handle_type(uppercase, camelcase, value)                 \
      case fielddescriptor::type_##uppercase:                    \
        target = wireformatlite::write##camelcase##toarray(      \
            number, value, target); \
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
      case fielddescriptor::type_message:
        if (is_lazy) {
          target = lazymessage_value->writemessagetoarray(number, target);
        } else {
          target = wireformatlite::writemessagetoarray(
              number, *message_value, target);
        }
        break;
    }
  }
  return target;
}

uint8* extensionset::extension::serializemessagesetitemwithcachedsizestoarray(
    int number,
    uint8* target) const {
  if (type != wireformatlite::type_message || is_repeated) {
    // not a valid messageset extension, but serialize it the normal way.
    google_log(warning) << "invalid message set extension.";
    return serializefieldwithcachedsizestoarray(number, target);
  }

  if (is_cleared) return target;

  // start group.
  target = io::codedoutputstream::writetagtoarray(
      wireformatlite::kmessagesetitemstarttag, target);
  // write type id.
  target = wireformatlite::writeuint32toarray(
      wireformatlite::kmessagesettypeidnumber, number, target);
  // write message.
  if (is_lazy) {
    target = lazymessage_value->writemessagetoarray(
        wireformatlite::kmessagesetmessagenumber, target);
  } else {
    target = wireformatlite::writemessagetoarray(
        wireformatlite::kmessagesetmessagenumber, *message_value, target);
  }
  // end group.
  target = io::codedoutputstream::writetagtoarray(
      wireformatlite::kmessagesetitemendtag, target);
  return target;
}


bool extensionset::parsefieldmaybelazily(
    uint32 tag, io::codedinputstream* input,
    extensionfinder* extension_finder,
    fieldskipper* field_skipper) {
  return parsefield(tag, input, extension_finder, field_skipper);
}

bool extensionset::parsemessageset(io::codedinputstream* input,
                                   extensionfinder* extension_finder,
                                   fieldskipper* field_skipper) {
  while (true) {
    uint32 tag = input->readtag();
    switch (tag) {
      case 0:
        return true;
      case wireformatlite::kmessagesetitemstarttag:
        if (!parsemessagesetitem(input, extension_finder, field_skipper)) {
          return false;
        }
        break;
      default:
        if (!parsefield(tag, input, extension_finder, field_skipper)) {
          return false;
        }
        break;
    }
  }
}

bool extensionset::parsemessageset(io::codedinputstream* input,
                                   const messagelite* containing_type) {
  fieldskipper skipper;
  generatedextensionfinder finder(containing_type);
  return parsemessageset(input, &finder, &skipper);
}

bool extensionset::parsemessagesetitem(io::codedinputstream* input,
                                       extensionfinder* extension_finder,
                                       fieldskipper* field_skipper) {
  // todo(kenton):  it would be nice to share code between this and
  // wireformatlite::parseandmergemessagesetitem(), but i think the
  // differences would be hard to factor out.

  // this method parses a group which should contain two fields:
  //   required int32 type_id = 2;
  //   required data message = 3;

  // once we see a type_id, we'll construct a fake tag for this extension
  // which is the tag it would have had under the proto2 extensions wire
  // format.
  uint32 fake_tag = 0;

  // if we see message data before the type_id, we'll append it to this so
  // we can parse it later.
  string message_data;

  while (true) {
    uint32 tag = input->readtag();
    if (tag == 0) return false;

    switch (tag) {
      case wireformatlite::kmessagesettypeidtag: {
        uint32 type_id;
        if (!input->readvarint32(&type_id)) return false;
        fake_tag = wireformatlite::maketag(type_id,
            wireformatlite::wiretype_length_delimited);

        if (!message_data.empty()) {
          // we saw some message data before the type_id.  have to parse it
          // now.
          io::codedinputstream sub_input(
              reinterpret_cast<const uint8*>(message_data.data()),
              message_data.size());
          if (!parsefieldmaybelazily(fake_tag, &sub_input,
                                     extension_finder, field_skipper)) {
            return false;
          }
          message_data.clear();
        }

        break;
      }

      case wireformatlite::kmessagesetmessagetag: {
        if (fake_tag == 0) {
          // we haven't seen a type_id yet.  append this data to message_data.
          string temp;
          uint32 length;
          if (!input->readvarint32(&length)) return false;
          if (!input->readstring(&temp, length)) return false;
          io::stringoutputstream output_stream(&message_data);
          io::codedoutputstream coded_output(&output_stream);
          coded_output.writevarint32(length);
          coded_output.writestring(temp);
        } else {
          // already saw type_id, so we can parse this directly.
          if (!parsefieldmaybelazily(fake_tag, input,
                                     extension_finder, field_skipper)) {
            return false;
          }
        }

        break;
      }

      case wireformatlite::kmessagesetitemendtag: {
        return true;
      }

      default: {
        if (!field_skipper->skipfield(input, tag)) return false;
      }
    }
  }
}

void extensionset::extension::serializemessagesetitemwithcachedsizes(
    int number,
    io::codedoutputstream* output) const {
  if (type != wireformatlite::type_message || is_repeated) {
    // not a valid messageset extension, but serialize it the normal way.
    serializefieldwithcachedsizes(number, output);
    return;
  }

  if (is_cleared) return;

  // start group.
  output->writetag(wireformatlite::kmessagesetitemstarttag);

  // write type id.
  wireformatlite::writeuint32(wireformatlite::kmessagesettypeidnumber,
                              number,
                              output);
  // write message.
  if (is_lazy) {
    lazymessage_value->writemessage(
        wireformatlite::kmessagesetmessagenumber, output);
  } else {
    wireformatlite::writemessagemaybetoarray(
        wireformatlite::kmessagesetmessagenumber,
        *message_value,
        output);
  }

  // end group.
  output->writetag(wireformatlite::kmessagesetitemendtag);
}

int extensionset::extension::messagesetitembytesize(int number) const {
  if (type != wireformatlite::type_message || is_repeated) {
    // not a valid messageset extension, but compute the byte size for it the
    // normal way.
    return bytesize(number);
  }

  if (is_cleared) return 0;

  int our_size = wireformatlite::kmessagesetitemtagssize;

  // type_id
  our_size += io::codedoutputstream::varintsize32(number);

  // message
  int message_size = 0;
  if (is_lazy) {
    message_size = lazymessage_value->bytesize();
  } else {
    message_size = message_value->bytesize();
  }

  our_size += io::codedoutputstream::varintsize32(message_size);
  our_size += message_size;

  return our_size;
}

void extensionset::serializemessagesetwithcachedsizes(
    io::codedoutputstream* output) const {
  map<int, extension>::const_iterator iter;
  for (iter = extensions_.begin(); iter != extensions_.end(); ++iter) {
    iter->second.serializemessagesetitemwithcachedsizes(iter->first, output);
  }
}

int extensionset::messagesetbytesize() const {
  int total_size = 0;

  for (map<int, extension>::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    total_size += iter->second.messagesetitembytesize(iter->first);
  }

  return total_size;
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google
