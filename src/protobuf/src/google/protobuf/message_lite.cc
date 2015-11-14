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

// authors: wink@google.com (wink saville),
//          kenton@google.com (kenton varda)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.

#include <google/protobuf/message_lite.h>
#include <string>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
namespace protobuf {

messagelite::~messagelite() {}

string messagelite::initializationerrorstring() const {
  return "(cannot determine missing fields for lite message)";
}

namespace {

// when serializing, we first compute the byte size, then serialize the message.
// if serialization produces a different number of bytes than expected, we
// call this function, which crashes.  the problem could be due to a bug in the
// protobuf implementation but is more likely caused by concurrent modification
// of the message.  this function attempts to distinguish between the two and
// provide a useful error message.
void bytesizeconsistencyerror(int byte_size_before_serialization,
                              int byte_size_after_serialization,
                              int bytes_produced_by_serialization) {
  google_check_eq(byte_size_before_serialization, byte_size_after_serialization)
      << "protocol message was modified concurrently during serialization.";
  google_check_eq(bytes_produced_by_serialization, byte_size_before_serialization)
      << "byte size calculation and serialization were inconsistent.  this "
         "may indicate a bug in protocol buffers or it may be caused by "
         "concurrent modification of the message.";
  google_log(fatal) << "this shouldn't be called if all the sizes are equal.";
}

string initializationerrormessage(const char* action,
                                  const messagelite& message) {
  // note:  we want to avoid depending on strutil in the lite library, otherwise
  //   we'd use:
  //
  // return strings::substitute(
  //   "can't $0 message of type \"$1\" because it is missing required "
  //   "fields: $2",
  //   action, message.gettypename(),
  //   message.initializationerrorstring());

  string result;
  result += "can't ";
  result += action;
  result += " message of type \"";
  result += message.gettypename();
  result += "\" because it is missing required fields: ";
  result += message.initializationerrorstring();
  return result;
}

// several of the parse methods below just do one thing and then call another
// method.  in a naive implementation, we might have parsefromstring() call
// parsefromarray() which would call parsefromzerocopystream() which would call
// parsefromcodedstream() which would call mergefromcodedstream() which would
// call mergepartialfromcodedstream().  however, when parsing very small
// messages, every function call introduces significant overhead.  to avoid
// this without reproducing code, we use these forced-inline helpers.
//
// note:  gcc only allows google_attribute_always_inline on declarations, not
//   definitions.
inline bool inlinemergefromcodedstream(io::codedinputstream* input,
                                       messagelite* message)
                                       google_attribute_always_inline;
inline bool inlineparsefromcodedstream(io::codedinputstream* input,
                                       messagelite* message)
                                       google_attribute_always_inline;
inline bool inlineparsepartialfromcodedstream(io::codedinputstream* input,
                                              messagelite* message)
                                              google_attribute_always_inline;
inline bool inlineparsefromarray(const void* data, int size,
                                 messagelite* message)
                                 google_attribute_always_inline;
inline bool inlineparsepartialfromarray(const void* data, int size,
                                        messagelite* message)
                                        google_attribute_always_inline;

bool inlinemergefromcodedstream(io::codedinputstream* input,
                                messagelite* message) {
  if (!message->mergepartialfromcodedstream(input)) return false;
  if (!message->isinitialized()) {
    google_log(error) << initializationerrormessage("parse", *message);
    return false;
  }
  return true;
}

bool inlineparsefromcodedstream(io::codedinputstream* input,
                                messagelite* message) {
  message->clear();
  return inlinemergefromcodedstream(input, message);
}

bool inlineparsepartialfromcodedstream(io::codedinputstream* input,
                                       messagelite* message) {
  message->clear();
  return message->mergepartialfromcodedstream(input);
}

bool inlineparsefromarray(const void* data, int size, messagelite* message) {
  io::codedinputstream input(reinterpret_cast<const uint8*>(data), size);
  return inlineparsefromcodedstream(&input, message) &&
         input.consumedentiremessage();
}

bool inlineparsepartialfromarray(const void* data, int size,
                                 messagelite* message) {
  io::codedinputstream input(reinterpret_cast<const uint8*>(data), size);
  return inlineparsepartialfromcodedstream(&input, message) &&
         input.consumedentiremessage();
}

}  // namespace

bool messagelite::mergefromcodedstream(io::codedinputstream* input) {
  return inlinemergefromcodedstream(input, this);
}

bool messagelite::parsefromcodedstream(io::codedinputstream* input) {
  return inlineparsefromcodedstream(input, this);
}

bool messagelite::parsepartialfromcodedstream(io::codedinputstream* input) {
  return inlineparsepartialfromcodedstream(input, this);
}

bool messagelite::parsefromzerocopystream(io::zerocopyinputstream* input) {
  io::codedinputstream decoder(input);
  return parsefromcodedstream(&decoder) && decoder.consumedentiremessage();
}

bool messagelite::parsepartialfromzerocopystream(
    io::zerocopyinputstream* input) {
  io::codedinputstream decoder(input);
  return parsepartialfromcodedstream(&decoder) &&
         decoder.consumedentiremessage();
}

bool messagelite::parsefromboundedzerocopystream(
    io::zerocopyinputstream* input, int size) {
  io::codedinputstream decoder(input);
  decoder.pushlimit(size);
  return parsefromcodedstream(&decoder) &&
         decoder.consumedentiremessage() &&
         decoder.bytesuntillimit() == 0;
}

bool messagelite::parsepartialfromboundedzerocopystream(
    io::zerocopyinputstream* input, int size) {
  io::codedinputstream decoder(input);
  decoder.pushlimit(size);
  return parsepartialfromcodedstream(&decoder) &&
         decoder.consumedentiremessage() &&
         decoder.bytesuntillimit() == 0;
}

bool messagelite::parsefromstring(const string& data) {
  return inlineparsefromarray(data.data(), data.size(), this);
}

bool messagelite::parsepartialfromstring(const string& data) {
  return inlineparsepartialfromarray(data.data(), data.size(), this);
}

bool messagelite::parsefromarray(const void* data, int size) {
  return inlineparsefromarray(data, size, this);
}

bool messagelite::parsepartialfromarray(const void* data, int size) {
  return inlineparsepartialfromarray(data, size, this);
}


// ===================================================================

uint8* messagelite::serializewithcachedsizestoarray(uint8* target) const {
  // we only optimize this when using optimize_for = speed.  in other cases
  // we just use the codedoutputstream path.
  int size = getcachedsize();
  io::arrayoutputstream out(target, size);
  io::codedoutputstream coded_out(&out);
  serializewithcachedsizes(&coded_out);
  google_check(!coded_out.haderror());
  return target + size;
}

bool messagelite::serializetocodedstream(io::codedoutputstream* output) const {
  google_dcheck(isinitialized()) << initializationerrormessage("serialize", *this);
  return serializepartialtocodedstream(output);
}

bool messagelite::serializepartialtocodedstream(
    io::codedoutputstream* output) const {
  const int size = bytesize();  // force size to be cached.
  uint8* buffer = output->getdirectbufferfornbytesandadvance(size);
  if (buffer != null) {
    uint8* end = serializewithcachedsizestoarray(buffer);
    if (end - buffer != size) {
      bytesizeconsistencyerror(size, bytesize(), end - buffer);
    }
    return true;
  } else {
    int original_byte_count = output->bytecount();
    serializewithcachedsizes(output);
    if (output->haderror()) {
      return false;
    }
    int final_byte_count = output->bytecount();

    if (final_byte_count - original_byte_count != size) {
      bytesizeconsistencyerror(size, bytesize(),
                               final_byte_count - original_byte_count);
    }

    return true;
  }
}

bool messagelite::serializetozerocopystream(
    io::zerocopyoutputstream* output) const {
  io::codedoutputstream encoder(output);
  return serializetocodedstream(&encoder);
}

bool messagelite::serializepartialtozerocopystream(
    io::zerocopyoutputstream* output) const {
  io::codedoutputstream encoder(output);
  return serializepartialtocodedstream(&encoder);
}

bool messagelite::appendtostring(string* output) const {
  google_dcheck(isinitialized()) << initializationerrormessage("serialize", *this);
  return appendpartialtostring(output);
}

bool messagelite::appendpartialtostring(string* output) const {
  int old_size = output->size();
  int byte_size = bytesize();
  stlstringresizeuninitialized(output, old_size + byte_size);
  uint8* start = reinterpret_cast<uint8*>(string_as_array(output) + old_size);
  uint8* end = serializewithcachedsizestoarray(start);
  if (end - start != byte_size) {
    bytesizeconsistencyerror(byte_size, bytesize(), end - start);
  }
  return true;
}

bool messagelite::serializetostring(string* output) const {
  output->clear();
  return appendtostring(output);
}

bool messagelite::serializepartialtostring(string* output) const {
  output->clear();
  return appendpartialtostring(output);
}

bool messagelite::serializetoarray(void* data, int size) const {
  google_dcheck(isinitialized()) << initializationerrormessage("serialize", *this);
  return serializepartialtoarray(data, size);
}

bool messagelite::serializepartialtoarray(void* data, int size) const {
  int byte_size = bytesize();
  if (size < byte_size) return false;
  uint8* start = reinterpret_cast<uint8*>(data);
  uint8* end = serializewithcachedsizestoarray(start);
  if (end - start != byte_size) {
    bytesizeconsistencyerror(byte_size, bytesize(), end - start);
  }
  return true;
}

string messagelite::serializeasstring() const {
  // if the compiler implements the (named) return value optimization,
  // the local variable 'result' will not actually reside on the stack
  // of this function, but will be overlaid with the object that the
  // caller supplied for the return value to be constructed in.
  string output;
  if (!appendtostring(&output))
    output.clear();
  return output;
}

string messagelite::serializepartialasstring() const {
  string output;
  if (!appendpartialtostring(&output))
    output.clear();
  return output;
}

}  // namespace protobuf
}  // namespace google
