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

#include <istream>
#include <stack>
#include <google/protobuf/stubs/hash.h>

#include <google/protobuf/message.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/once.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/map-util.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
namespace protobuf {

using internal::wireformat;
using internal::reflectionops;

message::~message() {}

void message::mergefrom(const message& from) {
  const descriptor* descriptor = getdescriptor();
  google_check_eq(from.getdescriptor(), descriptor)
    << ": tried to merge from a message with a different type.  "
       "to: " << descriptor->full_name() << ", "
       "from:" << from.getdescriptor()->full_name();
  reflectionops::merge(from, this);
}

void message::checktypeandmergefrom(const messagelite& other) {
  mergefrom(*down_cast<const message*>(&other));
}

void message::copyfrom(const message& from) {
  const descriptor* descriptor = getdescriptor();
  google_check_eq(from.getdescriptor(), descriptor)
    << ": tried to copy from a message with a different type."
       "to: " << descriptor->full_name() << ", "
       "from:" << from.getdescriptor()->full_name();
  reflectionops::copy(from, this);
}

string message::gettypename() const {
  return getdescriptor()->full_name();
}

void message::clear() {
  reflectionops::clear(this);
}

bool message::isinitialized() const {
  return reflectionops::isinitialized(*this);
}

void message::findinitializationerrors(vector<string>* errors) const {
  return reflectionops::findinitializationerrors(*this, "", errors);
}

string message::initializationerrorstring() const {
  vector<string> errors;
  findinitializationerrors(&errors);
  return joinstrings(errors, ", ");
}

void message::checkinitialized() const {
  google_check(isinitialized())
    << "message of type \"" << getdescriptor()->full_name()
    << "\" is missing required fields: " << initializationerrorstring();
}

void message::discardunknownfields() {
  return reflectionops::discardunknownfields(this);
}

bool message::mergepartialfromcodedstream(io::codedinputstream* input) {
  return wireformat::parseandmergepartial(input, this);
}

bool message::parsefromfiledescriptor(int file_descriptor) {
  io::fileinputstream input(file_descriptor);
  return parsefromzerocopystream(&input) && input.geterrno() == 0;
}

bool message::parsepartialfromfiledescriptor(int file_descriptor) {
  io::fileinputstream input(file_descriptor);
  return parsepartialfromzerocopystream(&input) && input.geterrno() == 0;
}

bool message::parsefromistream(istream* input) {
  io::istreaminputstream zero_copy_input(input);
  return parsefromzerocopystream(&zero_copy_input) && input->eof();
}

bool message::parsepartialfromistream(istream* input) {
  io::istreaminputstream zero_copy_input(input);
  return parsepartialfromzerocopystream(&zero_copy_input) && input->eof();
}


void message::serializewithcachedsizes(
    io::codedoutputstream* output) const {
  wireformat::serializewithcachedsizes(*this, getcachedsize(), output);
}

int message::bytesize() const {
  int size = wireformat::bytesize(*this);
  setcachedsize(size);
  return size;
}

void message::setcachedsize(int size) const {
  google_log(fatal) << "message class \"" << getdescriptor()->full_name()
             << "\" implements neither setcachedsize() nor bytesize().  "
                "must implement one or the other.";
}

int message::spaceused() const {
  return getreflection()->spaceused(*this);
}

bool message::serializetofiledescriptor(int file_descriptor) const {
  io::fileoutputstream output(file_descriptor);
  return serializetozerocopystream(&output);
}

bool message::serializepartialtofiledescriptor(int file_descriptor) const {
  io::fileoutputstream output(file_descriptor);
  return serializepartialtozerocopystream(&output);
}

bool message::serializetoostream(ostream* output) const {
  {
    io::ostreamoutputstream zero_copy_output(output);
    if (!serializetozerocopystream(&zero_copy_output)) return false;
  }
  return output->good();
}

bool message::serializepartialtoostream(ostream* output) const {
  io::ostreamoutputstream zero_copy_output(output);
  return serializepartialtozerocopystream(&zero_copy_output);
}


// =============================================================================
// reflection and associated template specializations

reflection::~reflection() {}

#define handle_type(type, cpptype, ctype)                             \
template<>                                                            \
const repeatedfield<type>& reflection::getrepeatedfield<type>(        \
    const message& message, const fielddescriptor* field) const {     \
  return *static_cast<repeatedfield<type>* >(                         \
      mutablerawrepeatedfield(const_cast<message*>(&message),         \
                          field, cpptype, ctype, null));              \
}                                                                     \
                                                                      \
template<>                                                            \
repeatedfield<type>* reflection::mutablerepeatedfield<type>(          \
    message* message, const fielddescriptor* field) const {           \
  return static_cast<repeatedfield<type>* >(                          \
      mutablerawrepeatedfield(message, field, cpptype, ctype, null)); \
}

handle_type(int32,  fielddescriptor::cpptype_int32,  -1);
handle_type(int64,  fielddescriptor::cpptype_int64,  -1);
handle_type(uint32, fielddescriptor::cpptype_uint32, -1);
handle_type(uint64, fielddescriptor::cpptype_uint64, -1);
handle_type(float,  fielddescriptor::cpptype_float,  -1);
handle_type(double, fielddescriptor::cpptype_double, -1);
handle_type(bool,   fielddescriptor::cpptype_bool,   -1);


#undef handle_type

void* reflection::mutablerawrepeatedstring(
    message* message, const fielddescriptor* field, bool is_string) const {
  return mutablerawrepeatedfield(message, field,
      fielddescriptor::cpptype_string, fieldoptions::string, null);
}


// =============================================================================
// messagefactory

messagefactory::~messagefactory() {}

namespace {

class generatedmessagefactory : public messagefactory {
 public:
  generatedmessagefactory();
  ~generatedmessagefactory();

  static generatedmessagefactory* singleton();

  typedef void registrationfunc(const string&);
  void registerfile(const char* file, registrationfunc* registration_func);
  void registertype(const descriptor* descriptor, const message* prototype);

  // implements messagefactory ---------------------------------------
  const message* getprototype(const descriptor* type);

 private:
  // only written at static init time, so does not require locking.
  hash_map<const char*, registrationfunc*,
           hash<const char*>, streq> file_map_;

  // initialized lazily, so requires locking.
  mutex mutex_;
  hash_map<const descriptor*, const message*> type_map_;
};

generatedmessagefactory* generated_message_factory_ = null;
google_protobuf_declare_once(generated_message_factory_once_init_);

void shutdowngeneratedmessagefactory() {
  delete generated_message_factory_;
}

void initgeneratedmessagefactory() {
  generated_message_factory_ = new generatedmessagefactory;
  internal::onshutdown(&shutdowngeneratedmessagefactory);
}

generatedmessagefactory::generatedmessagefactory() {}
generatedmessagefactory::~generatedmessagefactory() {}

generatedmessagefactory* generatedmessagefactory::singleton() {
  ::google::protobuf::googleonceinit(&generated_message_factory_once_init_,
                 &initgeneratedmessagefactory);
  return generated_message_factory_;
}

void generatedmessagefactory::registerfile(
    const char* file, registrationfunc* registration_func) {
  if (!insertifnotpresent(&file_map_, file, registration_func)) {
    google_log(fatal) << "file is already registered: " << file;
  }
}

void generatedmessagefactory::registertype(const descriptor* descriptor,
                                           const message* prototype) {
  google_dcheck_eq(descriptor->file()->pool(), descriptorpool::generated_pool())
    << "tried to register a non-generated type with the generated "
       "type registry.";

  // this should only be called as a result of calling a file registration
  // function during getprototype(), in which case we already have locked
  // the mutex.
  mutex_.assertheld();
  if (!insertifnotpresent(&type_map_, descriptor, prototype)) {
    google_log(dfatal) << "type is already registered: " << descriptor->full_name();
  }
}


const message* generatedmessagefactory::getprototype(const descriptor* type) {
  {
    readermutexlock lock(&mutex_);
    const message* result = findptrornull(type_map_, type);
    if (result != null) return result;
  }

  // if the type is not in the generated pool, then we can't possibly handle
  // it.
  if (type->file()->pool() != descriptorpool::generated_pool()) return null;

  // apparently the file hasn't been registered yet.  let's do that now.
  registrationfunc* registration_func =
      findptrornull(file_map_, type->file()->name().c_str());
  if (registration_func == null) {
    google_log(dfatal) << "file appears to be in generated pool but wasn't "
                   "registered: " << type->file()->name();
    return null;
  }

  writermutexlock lock(&mutex_);

  // check if another thread preempted us.
  const message* result = findptrornull(type_map_, type);
  if (result == null) {
    // nope.  ok, register everything.
    registration_func(type->file()->name());
    // should be here now.
    result = findptrornull(type_map_, type);
  }

  if (result == null) {
    google_log(dfatal) << "type appears to be in generated pool but wasn't "
                << "registered: " << type->full_name();
  }

  return result;
}

}  // namespace

messagefactory* messagefactory::generated_factory() {
  return generatedmessagefactory::singleton();
}

void messagefactory::internalregistergeneratedfile(
    const char* filename, void (*register_messages)(const string&)) {
  generatedmessagefactory::singleton()->registerfile(filename,
                                                     register_messages);
}

void messagefactory::internalregistergeneratedmessage(
    const descriptor* descriptor, const message* prototype) {
  generatedmessagefactory::singleton()->registertype(descriptor, prototype);
}


}  // namespace protobuf
}  // namespace google
