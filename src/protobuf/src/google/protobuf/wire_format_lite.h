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
//         wink@google.com (wink saville) (refactored from wire_format.h)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.
//
// this header is logically internal, but is made public because it is used
// from protocol-compiler-generated code, which may reside in other components.

#ifndef google_protobuf_wire_format_lite_h__
#define google_protobuf_wire_format_lite_h__

#include <string>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/io/coded_stream.h>  // for codedoutputstream::varint32size

namespace google {

namespace protobuf {
  template <typename t> class repeatedfield;  // repeated_field.h
}

namespace protobuf {
namespace internal {

class stringpiecefield;

// this class is for internal use by the protocol buffer library and by
// protocol-complier-generated message classes.  it must not be called
// directly by clients.
//
// this class contains helpers for implementing the binary protocol buffer
// wire format without the need for reflection. use wireformat when using
// reflection.
//
// this class is really a namespace that contains only static methods.
class libprotobuf_export wireformatlite {
 public:

  // -----------------------------------------------------------------
  // helper constants and functions related to the format.  these are
  // mostly meant for internal and generated code to use.

  // the wire format is composed of a sequence of tag/value pairs, each
  // of which contains the value of one field (or one element of a repeated
  // field).  each tag is encoded as a varint.  the lower bits of the tag
  // identify its wire type, which specifies the format of the data to follow.
  // the rest of the bits contain the field number.  each type of field (as
  // declared by fielddescriptor::type, in descriptor.h) maps to one of
  // these wire types.  immediately following each tag is the field's value,
  // encoded in the format specified by the wire type.  because the tag
  // identifies the encoding of this data, it is possible to skip
  // unrecognized fields for forwards compatibility.

  enum wiretype {
    wiretype_varint           = 0,
    wiretype_fixed64          = 1,
    wiretype_length_delimited = 2,
    wiretype_start_group      = 3,
    wiretype_end_group        = 4,
    wiretype_fixed32          = 5,
  };

  // lite alternative to fielddescriptor::type.  must be kept in sync.
  enum fieldtype {
    type_double         = 1,
    type_float          = 2,
    type_int64          = 3,
    type_uint64         = 4,
    type_int32          = 5,
    type_fixed64        = 6,
    type_fixed32        = 7,
    type_bool           = 8,
    type_string         = 9,
    type_group          = 10,
    type_message        = 11,
    type_bytes          = 12,
    type_uint32         = 13,
    type_enum           = 14,
    type_sfixed32       = 15,
    type_sfixed64       = 16,
    type_sint32         = 17,
    type_sint64         = 18,
    max_field_type      = 18,
  };

  // lite alternative to fielddescriptor::cpptype.  must be kept in sync.
  enum cpptype {
    cpptype_int32       = 1,
    cpptype_int64       = 2,
    cpptype_uint32      = 3,
    cpptype_uint64      = 4,
    cpptype_double      = 5,
    cpptype_float       = 6,
    cpptype_bool        = 7,
    cpptype_enum        = 8,
    cpptype_string      = 9,
    cpptype_message     = 10,
    max_cpptype         = 10,
  };

  // helper method to get the cpptype for a particular type.
  static cpptype fieldtypetocpptype(fieldtype type);

  // given a fieldsescriptor::type return its wiretype
  static inline wireformatlite::wiretype wiretypeforfieldtype(
      wireformatlite::fieldtype type) {
    return kwiretypeforfieldtype[type];
  }

  // number of bits in a tag which identify the wire type.
  static const int ktagtypebits = 3;
  // mask for those bits.
  static const uint32 ktagtypemask = (1 << ktagtypebits) - 1;

  // helper functions for encoding and decoding tags.  (inlined below and in
  // _inl.h)
  //
  // this is different from maketag(field->number(), field->type()) in the case
  // of packed repeated fields.
  static uint32 maketag(int field_number, wiretype type);
  static wiretype gettagwiretype(uint32 tag);
  static int gettagfieldnumber(uint32 tag);

  // compute the byte size of a tag.  for groups, this includes both the start
  // and end tags.
  static inline int tagsize(int field_number, wireformatlite::fieldtype type);

  // skips a field value with the given tag.  the input should start
  // positioned immediately after the tag.  skipped values are simply discarded,
  // not recorded anywhere.  see wireformat::skipfield() for a version that
  // records to an unknownfieldset.
  static bool skipfield(io::codedinputstream* input, uint32 tag);

  // reads and ignores a message from the input.  skipped values are simply
  // discarded, not recorded anywhere.  see wireformat::skipmessage() for a
  // version that records to an unknownfieldset.
  static bool skipmessage(io::codedinputstream* input);

// this macro does the same thing as wireformatlite::maketag(), but the
// result is usable as a compile-time constant, which makes it usable
// as a switch case or a template input.  wireformatlite::maketag() is more
// type-safe, though, so prefer it if possible.
#define google_protobuf_wire_format_make_tag(field_number, type)                  \
  static_cast<uint32>(                                                   \
    ((field_number) << ::google::protobuf::internal::wireformatlite::ktagtypebits) \
      | (type))

  // these are the tags for the old messageset format, which was defined as:
  //   message messageset {
  //     repeated group item = 1 {
  //       required int32 type_id = 2;
  //       required string message = 3;
  //     }
  //   }
  static const int kmessagesetitemnumber = 1;
  static const int kmessagesettypeidnumber = 2;
  static const int kmessagesetmessagenumber = 3;
  static const int kmessagesetitemstarttag =
    google_protobuf_wire_format_make_tag(kmessagesetitemnumber,
                                wireformatlite::wiretype_start_group);
  static const int kmessagesetitemendtag =
    google_protobuf_wire_format_make_tag(kmessagesetitemnumber,
                                wireformatlite::wiretype_end_group);
  static const int kmessagesettypeidtag =
    google_protobuf_wire_format_make_tag(kmessagesettypeidnumber,
                                wireformatlite::wiretype_varint);
  static const int kmessagesetmessagetag =
    google_protobuf_wire_format_make_tag(kmessagesetmessagenumber,
                                wireformatlite::wiretype_length_delimited);

  // byte size of all tags of a messageset::item combined.
  static const int kmessagesetitemtagssize;

  // helper functions for converting between floats/doubles and ieee-754
  // uint32s/uint64s so that they can be written.  (assumes your platform
  // uses ieee-754 floats.)
  static uint32 encodefloat(float value);
  static float decodefloat(uint32 value);
  static uint64 encodedouble(double value);
  static double decodedouble(uint64 value);

  // helper functions for mapping signed integers to unsigned integers in
  // such a way that numbers with small magnitudes will encode to smaller
  // varints.  if you simply static_cast a negative number to an unsigned
  // number and varint-encode it, it will always take 10 bytes, defeating
  // the purpose of varint.  so, for the "sint32" and "sint64" field types,
  // we zigzag-encode the values.
  static uint32 zigzagencode32(int32 n);
  static int32  zigzagdecode32(uint32 n);
  static uint64 zigzagencode64(int64 n);
  static int64  zigzagdecode64(uint64 n);

  // =================================================================
  // methods for reading/writing individual field.  the implementations
  // of these methods are defined in wire_format_lite_inl.h; you must #include
  // that file to use these.

// avoid ugly line wrapping
#define input  io::codedinputstream*  input
#define output io::codedoutputstream* output
#define field_number int field_number
#define inl google_attribute_always_inline

  // read fields, not including tags.  the assumption is that you already
  // read the tag to determine what field to read.

  // for primitive fields, we just use a templatized routine parameterized by
  // the represented type and the fieldtype. these are specialized with the
  // appropriate definition for each declared type.
  template <typename ctype, enum fieldtype declaredtype>
  static inline bool readprimitive(input, ctype* value) inl;

  // reads repeated primitive values, with optimizations for repeats.
  // tag_size and tag should both be compile-time constants provided by the
  // protocol compiler.
  template <typename ctype, enum fieldtype declaredtype>
  static inline bool readrepeatedprimitive(int tag_size,
                                           uint32 tag,
                                           input,
                                           repeatedfield<ctype>* value) inl;

  // identical to readrepeatedprimitive, except will not inline the
  // implementation.
  template <typename ctype, enum fieldtype declaredtype>
  static bool readrepeatedprimitivenoinline(int tag_size,
                                            uint32 tag,
                                            input,
                                            repeatedfield<ctype>* value);

  // reads a primitive value directly from the provided buffer. it returns a
  // pointer past the segment of data that was read.
  //
  // this is only implemented for the types with fixed wire size, e.g.
  // float, double, and the (s)fixed* types.
  template <typename ctype, enum fieldtype declaredtype>
  static inline const uint8* readprimitivefromarray(const uint8* buffer,
                                                    ctype* value) inl;

  // reads a primitive packed field.
  //
  // this is only implemented for packable types.
  template <typename ctype, enum fieldtype declaredtype>
  static inline bool readpackedprimitive(input,
                                         repeatedfield<ctype>* value) inl;

  // identical to readpackedprimitive, except will not inline the
  // implementation.
  template <typename ctype, enum fieldtype declaredtype>
  static bool readpackedprimitivenoinline(input, repeatedfield<ctype>* value);

  // read a packed enum field. values for which is_valid() returns false are
  // dropped.
  static bool readpackedenumnoinline(input,
                                     bool (*is_valid)(int),
                                     repeatedfield<int>* value);

  static bool readstring(input, string* value);
  static bool readbytes (input, string* value);

  static inline bool readgroup  (field_number, input, messagelite* value);
  static inline bool readmessage(input, messagelite* value);

  // like above, but de-virtualize the call to mergepartialfromcodedstream().
  // the pointer must point at an instance of messagetype, *not* a subclass (or
  // the subclass must not override mergepartialfromcodedstream()).
  template<typename messagetype>
  static inline bool readgroupnovirtual(field_number, input,
                                        messagetype* value);
  template<typename messagetype>
  static inline bool readmessagenovirtual(input, messagetype* value);

  // write a tag.  the write*() functions typically include the tag, so
  // normally there's no need to call this unless using the write*notag()
  // variants.
  static inline void writetag(field_number, wiretype type, output) inl;

  // write fields, without tags.
  static inline void writeint32notag   (int32 value, output) inl;
  static inline void writeint64notag   (int64 value, output) inl;
  static inline void writeuint32notag  (uint32 value, output) inl;
  static inline void writeuint64notag  (uint64 value, output) inl;
  static inline void writesint32notag  (int32 value, output) inl;
  static inline void writesint64notag  (int64 value, output) inl;
  static inline void writefixed32notag (uint32 value, output) inl;
  static inline void writefixed64notag (uint64 value, output) inl;
  static inline void writesfixed32notag(int32 value, output) inl;
  static inline void writesfixed64notag(int64 value, output) inl;
  static inline void writefloatnotag   (float value, output) inl;
  static inline void writedoublenotag  (double value, output) inl;
  static inline void writeboolnotag    (bool value, output) inl;
  static inline void writeenumnotag    (int value, output) inl;

  // write fields, including tags.
  static void writeint32   (field_number,  int32 value, output);
  static void writeint64   (field_number,  int64 value, output);
  static void writeuint32  (field_number, uint32 value, output);
  static void writeuint64  (field_number, uint64 value, output);
  static void writesint32  (field_number,  int32 value, output);
  static void writesint64  (field_number,  int64 value, output);
  static void writefixed32 (field_number, uint32 value, output);
  static void writefixed64 (field_number, uint64 value, output);
  static void writesfixed32(field_number,  int32 value, output);
  static void writesfixed64(field_number,  int64 value, output);
  static void writefloat   (field_number,  float value, output);
  static void writedouble  (field_number, double value, output);
  static void writebool    (field_number,   bool value, output);
  static void writeenum    (field_number,    int value, output);

  static void writestring(field_number, const string& value, output);
  static void writebytes (field_number, const string& value, output);

  static void writegroup(
    field_number, const messagelite& value, output);
  static void writemessage(
    field_number, const messagelite& value, output);
  // like above, but these will check if the output stream has enough
  // space to write directly to a flat array.
  static void writegroupmaybetoarray(
    field_number, const messagelite& value, output);
  static void writemessagemaybetoarray(
    field_number, const messagelite& value, output);

  // like above, but de-virtualize the call to serializewithcachedsizes().  the
  // pointer must point at an instance of messagetype, *not* a subclass (or
  // the subclass must not override serializewithcachedsizes()).
  template<typename messagetype>
  static inline void writegroupnovirtual(
    field_number, const messagetype& value, output);
  template<typename messagetype>
  static inline void writemessagenovirtual(
    field_number, const messagetype& value, output);

#undef output
#define output uint8* target

  // like above, but use only *toarray methods of codedoutputstream.
  static inline uint8* writetagtoarray(field_number, wiretype type, output) inl;

  // write fields, without tags.
  static inline uint8* writeint32notagtoarray   (int32 value, output) inl;
  static inline uint8* writeint64notagtoarray   (int64 value, output) inl;
  static inline uint8* writeuint32notagtoarray  (uint32 value, output) inl;
  static inline uint8* writeuint64notagtoarray  (uint64 value, output) inl;
  static inline uint8* writesint32notagtoarray  (int32 value, output) inl;
  static inline uint8* writesint64notagtoarray  (int64 value, output) inl;
  static inline uint8* writefixed32notagtoarray (uint32 value, output) inl;
  static inline uint8* writefixed64notagtoarray (uint64 value, output) inl;
  static inline uint8* writesfixed32notagtoarray(int32 value, output) inl;
  static inline uint8* writesfixed64notagtoarray(int64 value, output) inl;
  static inline uint8* writefloatnotagtoarray   (float value, output) inl;
  static inline uint8* writedoublenotagtoarray  (double value, output) inl;
  static inline uint8* writeboolnotagtoarray    (bool value, output) inl;
  static inline uint8* writeenumnotagtoarray    (int value, output) inl;

  // write fields, including tags.
  static inline uint8* writeint32toarray(
    field_number, int32 value, output) inl;
  static inline uint8* writeint64toarray(
    field_number, int64 value, output) inl;
  static inline uint8* writeuint32toarray(
    field_number, uint32 value, output) inl;
  static inline uint8* writeuint64toarray(
    field_number, uint64 value, output) inl;
  static inline uint8* writesint32toarray(
    field_number, int32 value, output) inl;
  static inline uint8* writesint64toarray(
    field_number, int64 value, output) inl;
  static inline uint8* writefixed32toarray(
    field_number, uint32 value, output) inl;
  static inline uint8* writefixed64toarray(
    field_number, uint64 value, output) inl;
  static inline uint8* writesfixed32toarray(
    field_number, int32 value, output) inl;
  static inline uint8* writesfixed64toarray(
    field_number, int64 value, output) inl;
  static inline uint8* writefloattoarray(
    field_number, float value, output) inl;
  static inline uint8* writedoubletoarray(
    field_number, double value, output) inl;
  static inline uint8* writebooltoarray(
    field_number, bool value, output) inl;
  static inline uint8* writeenumtoarray(
    field_number, int value, output) inl;

  static inline uint8* writestringtoarray(
    field_number, const string& value, output) inl;
  static inline uint8* writebytestoarray(
    field_number, const string& value, output) inl;

  static inline uint8* writegrouptoarray(
      field_number, const messagelite& value, output) inl;
  static inline uint8* writemessagetoarray(
      field_number, const messagelite& value, output) inl;

  // like above, but de-virtualize the call to serializewithcachedsizes().  the
  // pointer must point at an instance of messagetype, *not* a subclass (or
  // the subclass must not override serializewithcachedsizes()).
  template<typename messagetype>
  static inline uint8* writegroupnovirtualtoarray(
    field_number, const messagetype& value, output) inl;
  template<typename messagetype>
  static inline uint8* writemessagenovirtualtoarray(
    field_number, const messagetype& value, output) inl;

#undef output
#undef input
#undef inl

#undef field_number

  // compute the byte size of a field.  the xxsize() functions do not include
  // the tag, so you must also call tagsize().  (this is because, for repeated
  // fields, you should only call tagsize() once and multiply it by the element
  // count, but you may have to call xxsize() for each individual element.)
  static inline int int32size   ( int32 value);
  static inline int int64size   ( int64 value);
  static inline int uint32size  (uint32 value);
  static inline int uint64size  (uint64 value);
  static inline int sint32size  ( int32 value);
  static inline int sint64size  ( int64 value);
  static inline int enumsize    (   int value);

  // these types always have the same size.
  static const int kfixed32size  = 4;
  static const int kfixed64size  = 8;
  static const int ksfixed32size = 4;
  static const int ksfixed64size = 8;
  static const int kfloatsize    = 4;
  static const int kdoublesize   = 8;
  static const int kboolsize     = 1;

  static inline int stringsize(const string& value);
  static inline int bytessize (const string& value);

  static inline int groupsize  (const messagelite& value);
  static inline int messagesize(const messagelite& value);

  // like above, but de-virtualize the call to bytesize().  the
  // pointer must point at an instance of messagetype, *not* a subclass (or
  // the subclass must not override bytesize()).
  template<typename messagetype>
  static inline int groupsizenovirtual  (const messagetype& value);
  template<typename messagetype>
  static inline int messagesizenovirtual(const messagetype& value);

  // given the length of data, calculate the byte size of the data on the
  // wire if we encode the data as a length delimited field.
  static inline int lengthdelimitedsize(int length);

 private:
  // a helper method for the repeated primitive reader. this method has
  // optimizations for primitive types that have fixed size on the wire, and
  // can be read using potentially faster paths.
  template <typename ctype, enum fieldtype declaredtype>
  static inline bool readrepeatedfixedsizeprimitive(
      int tag_size,
      uint32 tag,
      google::protobuf::io::codedinputstream* input,
      repeatedfield<ctype>* value) google_attribute_always_inline;

  static const cpptype kfieldtypetocpptypemap[];
  static const wireformatlite::wiretype kwiretypeforfieldtype[];

  google_disallow_evil_constructors(wireformatlite);
};

// a class which deals with unknown values.  the default implementation just
// discards them.  wireformat defines a subclass which writes to an
// unknownfieldset.  this class is used by extensionset::parsefield(), since
// extensionset is part of the lite library but unknownfieldset is not.
class libprotobuf_export fieldskipper {
 public:
  fieldskipper() {}
  virtual ~fieldskipper() {}

  // skip a field whose tag has already been consumed.
  virtual bool skipfield(io::codedinputstream* input, uint32 tag);

  // skip an entire message or group, up to an end-group tag (which is consumed)
  // or end-of-stream.
  virtual bool skipmessage(io::codedinputstream* input);

  // deal with an already-parsed unrecognized enum value.  the default
  // implementation does nothing, but the unknownfieldset-based implementation
  // saves it as an unknown varint.
  virtual void skipunknownenum(int field_number, int value);
};

// inline methods ====================================================

inline wireformatlite::cpptype
wireformatlite::fieldtypetocpptype(fieldtype type) {
  return kfieldtypetocpptypemap[type];
}

inline uint32 wireformatlite::maketag(int field_number, wiretype type) {
  return google_protobuf_wire_format_make_tag(field_number, type);
}

inline wireformatlite::wiretype wireformatlite::gettagwiretype(uint32 tag) {
  return static_cast<wiretype>(tag & ktagtypemask);
}

inline int wireformatlite::gettagfieldnumber(uint32 tag) {
  return static_cast<int>(tag >> ktagtypebits);
}

inline int wireformatlite::tagsize(int field_number,
                                   wireformatlite::fieldtype type) {
  int result = io::codedoutputstream::varintsize32(
    field_number << ktagtypebits);
  if (type == type_group) {
    // groups have both a start and an end tag.
    return result * 2;
  } else {
    return result;
  }
}

inline uint32 wireformatlite::encodefloat(float value) {
  union {float f; uint32 i;};
  f = value;
  return i;
}

inline float wireformatlite::decodefloat(uint32 value) {
  union {float f; uint32 i;};
  i = value;
  return f;
}

inline uint64 wireformatlite::encodedouble(double value) {
  union {double f; uint64 i;};
  f = value;
  return i;
}

inline double wireformatlite::decodedouble(uint64 value) {
  union {double f; uint64 i;};
  i = value;
  return f;
}

// zigzag transform:  encodes signed integers so that they can be
// effectively used with varint encoding.
//
// varint operates on unsigned integers, encoding smaller numbers into
// fewer bytes.  if you try to use it on a signed integer, it will treat
// this number as a very large unsigned integer, which means that even
// small signed numbers like -1 will take the maximum number of bytes
// (10) to encode.  zigzagencode() maps signed integers to unsigned
// in such a way that those with a small absolute value will have smaller
// encoded values, making them appropriate for encoding using varint.
//
//       int32 ->     uint32
// -------------------------
//           0 ->          0
//          -1 ->          1
//           1 ->          2
//          -2 ->          3
//         ... ->        ...
//  2147483647 -> 4294967294
// -2147483648 -> 4294967295
//
//        >> encode >>
//        << decode <<

inline uint32 wireformatlite::zigzagencode32(int32 n) {
  // note:  the right-shift must be arithmetic
  return (n << 1) ^ (n >> 31);
}

inline int32 wireformatlite::zigzagdecode32(uint32 n) {
  return (n >> 1) ^ -static_cast<int32>(n & 1);
}

inline uint64 wireformatlite::zigzagencode64(int64 n) {
  // note:  the right-shift must be arithmetic
  return (n << 1) ^ (n >> 63);
}

inline int64 wireformatlite::zigzagdecode64(uint64 n) {
  return (n >> 1) ^ -static_cast<int64>(n & 1);
}

}  // namespace internal
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_wire_format_lite_h__
