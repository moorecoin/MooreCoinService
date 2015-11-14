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
//         atenasio@google.com (chris atenasio) (zigzag transform)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.
//
// this header is logically internal, but is made public because it is used
// from protocol-compiler-generated code, which may reside in other components.

#ifndef google_protobuf_wire_format_h__
#define google_protobuf_wire_format_h__

#include <string>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/wire_format_lite.h>

// do utf-8 validation on string type in debug build only
#ifndef ndebug
#define google_protobuf_utf8_validation_enabled
#endif

namespace google {
namespace protobuf {
  namespace io {
    class codedinputstream;      // coded_stream.h
    class codedoutputstream;     // coded_stream.h
  }
  class unknownfieldset;         // unknown_field_set.h
}

namespace protobuf {
namespace internal {

// this class is for internal use by the protocol buffer library and by
// protocol-complier-generated message classes.  it must not be called
// directly by clients.
//
// this class contains code for implementing the binary protocol buffer
// wire format via reflection.  the wireformatlite class implements the
// non-reflection based routines.
//
// this class is really a namespace that contains only static methods
class libprotobuf_export wireformat {
 public:

  // given a field return its wiretype
  static inline wireformatlite::wiretype wiretypeforfield(
      const fielddescriptor* field);

  // given a fieldsescriptor::type return its wiretype
  static inline wireformatlite::wiretype wiretypeforfieldtype(
      fielddescriptor::type type);

  // compute the byte size of a tag.  for groups, this includes both the start
  // and end tags.
  static inline int tagsize(int field_number, fielddescriptor::type type);

  // these procedures can be used to implement the methods of message which
  // handle parsing and serialization of the protocol buffer wire format
  // using only the reflection interface.  when you ask the protocol
  // compiler to optimize for code size rather than speed, it will implement
  // those methods in terms of these procedures.  of course, these are much
  // slower than the specialized implementations which the protocol compiler
  // generates when told to optimize for speed.

  // read a message in protocol buffer wire format.
  //
  // this procedure reads either to the end of the input stream or through
  // a wiretype_end_group tag ending the message, whichever comes first.
  // it returns false if the input is invalid.
  //
  // required fields are not checked by this method.  you must call
  // isinitialized() on the resulting message yourself.
  static bool parseandmergepartial(io::codedinputstream* input,
                                   message* message);

  // serialize a message in protocol buffer wire format.
  //
  // any embedded messages within the message must have their correct sizes
  // cached.  however, the top-level message need not; its size is passed as
  // a parameter to this procedure.
  //
  // these return false iff the underlying stream returns a write error.
  static void serializewithcachedsizes(
      const message& message,
      int size, io::codedoutputstream* output);

  // implements message::bytesize() via reflection.  warning:  the result
  // of this method is *not* cached anywhere.  however, all embedded messages
  // will have their bytesize() methods called, so their sizes will be cached.
  // therefore, calling this method is sufficient to allow you to call
  // wireformat::serializewithcachedsizes() on the same object.
  static int bytesize(const message& message);

  // -----------------------------------------------------------------
  // helpers for dealing with unknown fields

  // skips a field value of the given wiretype.  the input should start
  // positioned immediately after the tag.  if unknown_fields is non-null,
  // the contents of the field will be added to it.
  static bool skipfield(io::codedinputstream* input, uint32 tag,
                        unknownfieldset* unknown_fields);

  // reads and ignores a message from the input.  if unknown_fields is non-null,
  // the contents will be added to it.
  static bool skipmessage(io::codedinputstream* input,
                          unknownfieldset* unknown_fields);

  // write the contents of an unknownfieldset to the output.
  static void serializeunknownfields(const unknownfieldset& unknown_fields,
                                     io::codedoutputstream* output);
  // same as above, except writing directly to the provided buffer.
  // requires that the buffer have sufficient capacity for
  // computeunknownfieldssize(unknown_fields).
  //
  // returns a pointer past the last written byte.
  static uint8* serializeunknownfieldstoarray(
      const unknownfieldset& unknown_fields,
      uint8* target);

  // same thing except for messages that have the message_set_wire_format
  // option.
  static void serializeunknownmessagesetitems(
      const unknownfieldset& unknown_fields,
      io::codedoutputstream* output);
  // same as above, except writing directly to the provided buffer.
  // requires that the buffer have sufficient capacity for
  // computeunknownmessagesetitemssize(unknown_fields).
  //
  // returns a pointer past the last written byte.
  static uint8* serializeunknownmessagesetitemstoarray(
      const unknownfieldset& unknown_fields,
      uint8* target);

  // compute the size of the unknownfieldset on the wire.
  static int computeunknownfieldssize(const unknownfieldset& unknown_fields);

  // same thing except for messages that have the message_set_wire_format
  // option.
  static int computeunknownmessagesetitemssize(
      const unknownfieldset& unknown_fields);


  // helper functions for encoding and decoding tags.  (inlined below and in
  // _inl.h)
  //
  // this is different from maketag(field->number(), field->type()) in the case
  // of packed repeated fields.
  static uint32 maketag(const fielddescriptor* field);

  // parse a single field.  the input should start out positioned immidately
  // after the tag.
  static bool parseandmergefield(
      uint32 tag,
      const fielddescriptor* field,        // may be null for unknown
      message* message,
      io::codedinputstream* input);

  // serialize a single field.
  static void serializefieldwithcachedsizes(
      const fielddescriptor* field,        // cannot be null
      const message& message,
      io::codedoutputstream* output);

  // compute size of a single field.  if the field is a message type, this
  // will call bytesize() for the embedded message, insuring that it caches
  // its size.
  static int fieldbytesize(
      const fielddescriptor* field,        // cannot be null
      const message& message);

  // parse/serialize a messageset::item group.  used with messages that use
  // opion message_set_wire_format = true.
  static bool parseandmergemessagesetitem(
      io::codedinputstream* input,
      message* message);
  static void serializemessagesetitemwithcachedsizes(
      const fielddescriptor* field,
      const message& message,
      io::codedoutputstream* output);
  static int messagesetitembytesize(
      const fielddescriptor* field,
      const message& message);

  // computes the byte size of a field, excluding tags. for packed fields, it
  // only includes the size of the raw data, and not the size of the total
  // length, but for other length-delimited types, the size of the length is
  // included.
  static int fielddataonlybytesize(
      const fielddescriptor* field,        // cannot be null
      const message& message);

  enum operation {
    parse,
    serialize,
  };

  // verifies that a string field is valid utf8, logging an error if not.
  static void verifyutf8string(const char* data, int size, operation op);

 private:
  // verifies that a string field is valid utf8, logging an error if not.
  static void verifyutf8stringfallback(
      const char* data,
      int size,
      operation op);



  google_disallow_evil_constructors(wireformat);
};

// subclass of fieldskipper which saves skipped fields to an unknownfieldset.
class libprotobuf_export unknownfieldsetfieldskipper : public fieldskipper {
 public:
  unknownfieldsetfieldskipper(unknownfieldset* unknown_fields)
      : unknown_fields_(unknown_fields) {}
  virtual ~unknownfieldsetfieldskipper() {}

  // implements fieldskipper -----------------------------------------
  virtual bool skipfield(io::codedinputstream* input, uint32 tag);
  virtual bool skipmessage(io::codedinputstream* input);
  virtual void skipunknownenum(int field_number, int value);

 protected:
  unknownfieldset* unknown_fields_;
};

// inline methods ====================================================

inline wireformatlite::wiretype wireformat::wiretypeforfield(
    const fielddescriptor* field) {
  if (field->options().packed()) {
    return wireformatlite::wiretype_length_delimited;
  } else {
    return wiretypeforfieldtype(field->type());
  }
}

inline wireformatlite::wiretype wireformat::wiretypeforfieldtype(
    fielddescriptor::type type) {
  // some compilers don't like enum -> enum casts, so we implicit_cast to
  // int first.
  return wireformatlite::wiretypeforfieldtype(
      static_cast<wireformatlite::fieldtype>(
        implicit_cast<int>(type)));
}

inline uint32 wireformat::maketag(const fielddescriptor* field) {
  return wireformatlite::maketag(field->number(), wiretypeforfield(field));
}

inline int wireformat::tagsize(int field_number, fielddescriptor::type type) {
  // some compilers don't like enum -> enum casts, so we implicit_cast to
  // int first.
  return wireformatlite::tagsize(field_number,
      static_cast<wireformatlite::fieldtype>(
        implicit_cast<int>(type)));
}

inline void wireformat::verifyutf8string(const char* data, int size,
    wireformat::operation op) {
#ifdef google_protobuf_utf8_validation_enabled
  wireformat::verifyutf8stringfallback(data, size, op);
#else
  // avoid the compiler warning about unsued variables.
  (void)data; (void)size; (void)op;
#endif
}


}  // namespace internal
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_wire_format_h__
