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
//         wink@google.com (wink saville) (refactored from wire_format.h)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.

#ifndef google_protobuf_wire_format_lite_inl_h__
#define google_protobuf_wire_format_lite_inl_h__

#include <string>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/io/coded_stream.h>


namespace google {
namespace protobuf {
namespace internal {

// implementation details of readprimitive.

template <>
inline bool wireformatlite::readprimitive<int32, wireformatlite::type_int32>(
    io::codedinputstream* input,
    int32* value) {
  uint32 temp;
  if (!input->readvarint32(&temp)) return false;
  *value = static_cast<int32>(temp);
  return true;
}
template <>
inline bool wireformatlite::readprimitive<int64, wireformatlite::type_int64>(
    io::codedinputstream* input,
    int64* value) {
  uint64 temp;
  if (!input->readvarint64(&temp)) return false;
  *value = static_cast<int64>(temp);
  return true;
}
template <>
inline bool wireformatlite::readprimitive<uint32, wireformatlite::type_uint32>(
    io::codedinputstream* input,
    uint32* value) {
  return input->readvarint32(value);
}
template <>
inline bool wireformatlite::readprimitive<uint64, wireformatlite::type_uint64>(
    io::codedinputstream* input,
    uint64* value) {
  return input->readvarint64(value);
}
template <>
inline bool wireformatlite::readprimitive<int32, wireformatlite::type_sint32>(
    io::codedinputstream* input,
    int32* value) {
  uint32 temp;
  if (!input->readvarint32(&temp)) return false;
  *value = zigzagdecode32(temp);
  return true;
}
template <>
inline bool wireformatlite::readprimitive<int64, wireformatlite::type_sint64>(
    io::codedinputstream* input,
    int64* value) {
  uint64 temp;
  if (!input->readvarint64(&temp)) return false;
  *value = zigzagdecode64(temp);
  return true;
}
template <>
inline bool wireformatlite::readprimitive<uint32, wireformatlite::type_fixed32>(
    io::codedinputstream* input,
    uint32* value) {
  return input->readlittleendian32(value);
}
template <>
inline bool wireformatlite::readprimitive<uint64, wireformatlite::type_fixed64>(
    io::codedinputstream* input,
    uint64* value) {
  return input->readlittleendian64(value);
}
template <>
inline bool wireformatlite::readprimitive<int32, wireformatlite::type_sfixed32>(
    io::codedinputstream* input,
    int32* value) {
  uint32 temp;
  if (!input->readlittleendian32(&temp)) return false;
  *value = static_cast<int32>(temp);
  return true;
}
template <>
inline bool wireformatlite::readprimitive<int64, wireformatlite::type_sfixed64>(
    io::codedinputstream* input,
    int64* value) {
  uint64 temp;
  if (!input->readlittleendian64(&temp)) return false;
  *value = static_cast<int64>(temp);
  return true;
}
template <>
inline bool wireformatlite::readprimitive<float, wireformatlite::type_float>(
    io::codedinputstream* input,
    float* value) {
  uint32 temp;
  if (!input->readlittleendian32(&temp)) return false;
  *value = decodefloat(temp);
  return true;
}
template <>
inline bool wireformatlite::readprimitive<double, wireformatlite::type_double>(
    io::codedinputstream* input,
    double* value) {
  uint64 temp;
  if (!input->readlittleendian64(&temp)) return false;
  *value = decodedouble(temp);
  return true;
}
template <>
inline bool wireformatlite::readprimitive<bool, wireformatlite::type_bool>(
    io::codedinputstream* input,
    bool* value) {
  uint32 temp;
  if (!input->readvarint32(&temp)) return false;
  *value = temp != 0;
  return true;
}
template <>
inline bool wireformatlite::readprimitive<int, wireformatlite::type_enum>(
    io::codedinputstream* input,
    int* value) {
  uint32 temp;
  if (!input->readvarint32(&temp)) return false;
  *value = static_cast<int>(temp);
  return true;
}

template <>
inline const uint8* wireformatlite::readprimitivefromarray<
  uint32, wireformatlite::type_fixed32>(
    const uint8* buffer,
    uint32* value) {
  return io::codedinputstream::readlittleendian32fromarray(buffer, value);
}
template <>
inline const uint8* wireformatlite::readprimitivefromarray<
  uint64, wireformatlite::type_fixed64>(
    const uint8* buffer,
    uint64* value) {
  return io::codedinputstream::readlittleendian64fromarray(buffer, value);
}
template <>
inline const uint8* wireformatlite::readprimitivefromarray<
  int32, wireformatlite::type_sfixed32>(
    const uint8* buffer,
    int32* value) {
  uint32 temp;
  buffer = io::codedinputstream::readlittleendian32fromarray(buffer, &temp);
  *value = static_cast<int32>(temp);
  return buffer;
}
template <>
inline const uint8* wireformatlite::readprimitivefromarray<
  int64, wireformatlite::type_sfixed64>(
    const uint8* buffer,
    int64* value) {
  uint64 temp;
  buffer = io::codedinputstream::readlittleendian64fromarray(buffer, &temp);
  *value = static_cast<int64>(temp);
  return buffer;
}
template <>
inline const uint8* wireformatlite::readprimitivefromarray<
  float, wireformatlite::type_float>(
    const uint8* buffer,
    float* value) {
  uint32 temp;
  buffer = io::codedinputstream::readlittleendian32fromarray(buffer, &temp);
  *value = decodefloat(temp);
  return buffer;
}
template <>
inline const uint8* wireformatlite::readprimitivefromarray<
  double, wireformatlite::type_double>(
    const uint8* buffer,
    double* value) {
  uint64 temp;
  buffer = io::codedinputstream::readlittleendian64fromarray(buffer, &temp);
  *value = decodedouble(temp);
  return buffer;
}

template <typename ctype, enum wireformatlite::fieldtype declaredtype>
inline bool wireformatlite::readrepeatedprimitive(int, // tag_size, unused.
                                               uint32 tag,
                                               io::codedinputstream* input,
                                               repeatedfield<ctype>* values) {
  ctype value;
  if (!readprimitive<ctype, declaredtype>(input, &value)) return false;
  values->add(value);
  int elements_already_reserved = values->capacity() - values->size();
  while (elements_already_reserved > 0 && input->expecttag(tag)) {
    if (!readprimitive<ctype, declaredtype>(input, &value)) return false;
    values->addalreadyreserved(value);
    elements_already_reserved--;
  }
  return true;
}

template <typename ctype, enum wireformatlite::fieldtype declaredtype>
inline bool wireformatlite::readrepeatedfixedsizeprimitive(
    int tag_size,
    uint32 tag,
    io::codedinputstream* input,
    repeatedfield<ctype>* values) {
  google_dcheck_eq(uint32size(tag), tag_size);
  ctype value;
  if (!readprimitive<ctype, declaredtype>(input, &value))
    return false;
  values->add(value);

  // for fixed size values, repeated values can be read more quickly by
  // reading directly from a raw array.
  //
  // we can get a tight loop by only reading as many elements as can be
  // added to the repeatedfield without having to do any resizing. additionally,
  // we only try to read as many elements as are available from the current
  // buffer space. doing so avoids having to perform boundary checks when
  // reading the value: the maximum number of elements that can be read is
  // known outside of the loop.
  const void* void_pointer;
  int size;
  input->getdirectbufferpointerinline(&void_pointer, &size);
  if (size > 0) {
    const uint8* buffer = reinterpret_cast<const uint8*>(void_pointer);
    // the number of bytes each type occupies on the wire.
    const int per_value_size = tag_size + sizeof(value);

    int elements_available = min(values->capacity() - values->size(),
                                 size / per_value_size);
    int num_read = 0;
    while (num_read < elements_available &&
           (buffer = io::codedinputstream::expecttagfromarray(
               buffer, tag)) != null) {
      buffer = readprimitivefromarray<ctype, declaredtype>(buffer, &value);
      values->addalreadyreserved(value);
      ++num_read;
    }
    const int read_bytes = num_read * per_value_size;
    if (read_bytes > 0) {
      input->skip(read_bytes);
    }
  }
  return true;
}

// specializations of readrepeatedprimitive for the fixed size types, which use 
// the optimized code path.
#define read_repeated_fixed_size_primitive(cpptype, declared_type)             \
template <>                                                                    \
inline bool wireformatlite::readrepeatedprimitive<                             \
  cpptype, wireformatlite::declared_type>(                                     \
    int tag_size,                                                              \
    uint32 tag,                                                                \
    io::codedinputstream* input,                                               \
    repeatedfield<cpptype>* values) {                                          \
  return readrepeatedfixedsizeprimitive<                                       \
    cpptype, wireformatlite::declared_type>(                                   \
      tag_size, tag, input, values);                                           \
}

read_repeated_fixed_size_primitive(uint32, type_fixed32)
read_repeated_fixed_size_primitive(uint64, type_fixed64)
read_repeated_fixed_size_primitive(int32, type_sfixed32)
read_repeated_fixed_size_primitive(int64, type_sfixed64)
read_repeated_fixed_size_primitive(float, type_float)
read_repeated_fixed_size_primitive(double, type_double)

#undef read_repeated_fixed_size_primitive

template <typename ctype, enum wireformatlite::fieldtype declaredtype>
bool wireformatlite::readrepeatedprimitivenoinline(
    int tag_size,
    uint32 tag,
    io::codedinputstream* input,
    repeatedfield<ctype>* value) {
  return readrepeatedprimitive<ctype, declaredtype>(
      tag_size, tag, input, value);
}

template <typename ctype, enum wireformatlite::fieldtype declaredtype>
inline bool wireformatlite::readpackedprimitive(io::codedinputstream* input,
                                                repeatedfield<ctype>* values) {
  uint32 length;
  if (!input->readvarint32(&length)) return false;
  io::codedinputstream::limit limit = input->pushlimit(length);
  while (input->bytesuntillimit() > 0) {
    ctype value;
    if (!readprimitive<ctype, declaredtype>(input, &value)) return false;
    values->add(value);
  }
  input->poplimit(limit);
  return true;
}

template <typename ctype, enum wireformatlite::fieldtype declaredtype>
bool wireformatlite::readpackedprimitivenoinline(io::codedinputstream* input,
                                                 repeatedfield<ctype>* values) {
  return readpackedprimitive<ctype, declaredtype>(input, values);
}


inline bool wireformatlite::readgroup(int field_number,
                                      io::codedinputstream* input,
                                      messagelite* value) {
  if (!input->incrementrecursiondepth()) return false;
  if (!value->mergepartialfromcodedstream(input)) return false;
  input->decrementrecursiondepth();
  // make sure the last thing read was an end tag for this group.
  if (!input->lasttagwas(maketag(field_number, wiretype_end_group))) {
    return false;
  }
  return true;
}
inline bool wireformatlite::readmessage(io::codedinputstream* input,
                                        messagelite* value) {
  uint32 length;
  if (!input->readvarint32(&length)) return false;
  if (!input->incrementrecursiondepth()) return false;
  io::codedinputstream::limit limit = input->pushlimit(length);
  if (!value->mergepartialfromcodedstream(input)) return false;
  // make sure that parsing stopped when the limit was hit, not at an endgroup
  // tag.
  if (!input->consumedentiremessage()) return false;
  input->poplimit(limit);
  input->decrementrecursiondepth();
  return true;
}

// we name the template parameter something long and extremely unlikely to occur
// elsewhere because a *qualified* member access expression designed to avoid
// virtual dispatch, c++03 [basic.lookup.classref] 3.4.5/4 requires that the
// name of the qualifying class to be looked up both in the context of the full
// expression (finding the template parameter) and in the context of the object
// whose member we are accessing. this could potentially find a nested type
// within that object. the standard goes on to require these names to refer to
// the same entity, which this collision would violate. the lack of a safe way
// to avoid this collision appears to be a defect in the standard, but until it
// is corrected, we choose the name to avoid accidental collisions.
template<typename messagetype_workaroundcpplookupdefect>
inline bool wireformatlite::readgroupnovirtual(
    int field_number, io::codedinputstream* input,
    messagetype_workaroundcpplookupdefect* value) {
  if (!input->incrementrecursiondepth()) return false;
  if (!value->
      messagetype_workaroundcpplookupdefect::mergepartialfromcodedstream(input))
    return false;
  input->decrementrecursiondepth();
  // make sure the last thing read was an end tag for this group.
  if (!input->lasttagwas(maketag(field_number, wiretype_end_group))) {
    return false;
  }
  return true;
}
template<typename messagetype_workaroundcpplookupdefect>
inline bool wireformatlite::readmessagenovirtual(
    io::codedinputstream* input, messagetype_workaroundcpplookupdefect* value) {
  uint32 length;
  if (!input->readvarint32(&length)) return false;
  if (!input->incrementrecursiondepth()) return false;
  io::codedinputstream::limit limit = input->pushlimit(length);
  if (!value->
      messagetype_workaroundcpplookupdefect::mergepartialfromcodedstream(input))
    return false;
  // make sure that parsing stopped when the limit was hit, not at an endgroup
  // tag.
  if (!input->consumedentiremessage()) return false;
  input->poplimit(limit);
  input->decrementrecursiondepth();
  return true;
}

// ===================================================================

inline void wireformatlite::writetag(int field_number, wiretype type,
                                     io::codedoutputstream* output) {
  output->writetag(maketag(field_number, type));
}

inline void wireformatlite::writeint32notag(int32 value,
                                            io::codedoutputstream* output) {
  output->writevarint32signextended(value);
}
inline void wireformatlite::writeint64notag(int64 value,
                                            io::codedoutputstream* output) {
  output->writevarint64(static_cast<uint64>(value));
}
inline void wireformatlite::writeuint32notag(uint32 value,
                                             io::codedoutputstream* output) {
  output->writevarint32(value);
}
inline void wireformatlite::writeuint64notag(uint64 value,
                                             io::codedoutputstream* output) {
  output->writevarint64(value);
}
inline void wireformatlite::writesint32notag(int32 value,
                                             io::codedoutputstream* output) {
  output->writevarint32(zigzagencode32(value));
}
inline void wireformatlite::writesint64notag(int64 value,
                                             io::codedoutputstream* output) {
  output->writevarint64(zigzagencode64(value));
}
inline void wireformatlite::writefixed32notag(uint32 value,
                                              io::codedoutputstream* output) {
  output->writelittleendian32(value);
}
inline void wireformatlite::writefixed64notag(uint64 value,
                                              io::codedoutputstream* output) {
  output->writelittleendian64(value);
}
inline void wireformatlite::writesfixed32notag(int32 value,
                                               io::codedoutputstream* output) {
  output->writelittleendian32(static_cast<uint32>(value));
}
inline void wireformatlite::writesfixed64notag(int64 value,
                                               io::codedoutputstream* output) {
  output->writelittleendian64(static_cast<uint64>(value));
}
inline void wireformatlite::writefloatnotag(float value,
                                            io::codedoutputstream* output) {
  output->writelittleendian32(encodefloat(value));
}
inline void wireformatlite::writedoublenotag(double value,
                                             io::codedoutputstream* output) {
  output->writelittleendian64(encodedouble(value));
}
inline void wireformatlite::writeboolnotag(bool value,
                                           io::codedoutputstream* output) {
  output->writevarint32(value ? 1 : 0);
}
inline void wireformatlite::writeenumnotag(int value,
                                           io::codedoutputstream* output) {
  output->writevarint32signextended(value);
}

// see comment on readgroupnovirtual to understand the need for this template
// parameter name.
template<typename messagetype_workaroundcpplookupdefect>
inline void wireformatlite::writegroupnovirtual(
    int field_number, const messagetype_workaroundcpplookupdefect& value,
    io::codedoutputstream* output) {
  writetag(field_number, wiretype_start_group, output);
  value.messagetype_workaroundcpplookupdefect::serializewithcachedsizes(output);
  writetag(field_number, wiretype_end_group, output);
}
template<typename messagetype_workaroundcpplookupdefect>
inline void wireformatlite::writemessagenovirtual(
    int field_number, const messagetype_workaroundcpplookupdefect& value,
    io::codedoutputstream* output) {
  writetag(field_number, wiretype_length_delimited, output);
  output->writevarint32(
      value.messagetype_workaroundcpplookupdefect::getcachedsize());
  value.messagetype_workaroundcpplookupdefect::serializewithcachedsizes(output);
}

// ===================================================================

inline uint8* wireformatlite::writetagtoarray(int field_number,
                                              wiretype type,
                                              uint8* target) {
  return io::codedoutputstream::writetagtoarray(maketag(field_number, type),
                                                target);
}

inline uint8* wireformatlite::writeint32notagtoarray(int32 value,
                                                     uint8* target) {
  return io::codedoutputstream::writevarint32signextendedtoarray(value, target);
}
inline uint8* wireformatlite::writeint64notagtoarray(int64 value,
                                                     uint8* target) {
  return io::codedoutputstream::writevarint64toarray(
      static_cast<uint64>(value), target);
}
inline uint8* wireformatlite::writeuint32notagtoarray(uint32 value,
                                                      uint8* target) {
  return io::codedoutputstream::writevarint32toarray(value, target);
}
inline uint8* wireformatlite::writeuint64notagtoarray(uint64 value,
                                                      uint8* target) {
  return io::codedoutputstream::writevarint64toarray(value, target);
}
inline uint8* wireformatlite::writesint32notagtoarray(int32 value,
                                                      uint8* target) {
  return io::codedoutputstream::writevarint32toarray(zigzagencode32(value),
                                                     target);
}
inline uint8* wireformatlite::writesint64notagtoarray(int64 value,
                                                      uint8* target) {
  return io::codedoutputstream::writevarint64toarray(zigzagencode64(value),
                                                     target);
}
inline uint8* wireformatlite::writefixed32notagtoarray(uint32 value,
                                                       uint8* target) {
  return io::codedoutputstream::writelittleendian32toarray(value, target);
}
inline uint8* wireformatlite::writefixed64notagtoarray(uint64 value,
                                                       uint8* target) {
  return io::codedoutputstream::writelittleendian64toarray(value, target);
}
inline uint8* wireformatlite::writesfixed32notagtoarray(int32 value,
                                                        uint8* target) {
  return io::codedoutputstream::writelittleendian32toarray(
      static_cast<uint32>(value), target);
}
inline uint8* wireformatlite::writesfixed64notagtoarray(int64 value,
                                                        uint8* target) {
  return io::codedoutputstream::writelittleendian64toarray(
      static_cast<uint64>(value), target);
}
inline uint8* wireformatlite::writefloatnotagtoarray(float value,
                                                     uint8* target) {
  return io::codedoutputstream::writelittleendian32toarray(encodefloat(value),
                                                           target);
}
inline uint8* wireformatlite::writedoublenotagtoarray(double value,
                                                      uint8* target) {
  return io::codedoutputstream::writelittleendian64toarray(encodedouble(value),
                                                           target);
}
inline uint8* wireformatlite::writeboolnotagtoarray(bool value,
                                                    uint8* target) {
  return io::codedoutputstream::writevarint32toarray(value ? 1 : 0, target);
}
inline uint8* wireformatlite::writeenumnotagtoarray(int value,
                                                    uint8* target) {
  return io::codedoutputstream::writevarint32signextendedtoarray(value, target);
}

inline uint8* wireformatlite::writeint32toarray(int field_number,
                                                int32 value,
                                                uint8* target) {
  target = writetagtoarray(field_number, wiretype_varint, target);
  return writeint32notagtoarray(value, target);
}
inline uint8* wireformatlite::writeint64toarray(int field_number,
                                                int64 value,
                                                uint8* target) {
  target = writetagtoarray(field_number, wiretype_varint, target);
  return writeint64notagtoarray(value, target);
}
inline uint8* wireformatlite::writeuint32toarray(int field_number,
                                                 uint32 value,
                                                 uint8* target) {
  target = writetagtoarray(field_number, wiretype_varint, target);
  return writeuint32notagtoarray(value, target);
}
inline uint8* wireformatlite::writeuint64toarray(int field_number,
                                                 uint64 value,
                                                 uint8* target) {
  target = writetagtoarray(field_number, wiretype_varint, target);
  return writeuint64notagtoarray(value, target);
}
inline uint8* wireformatlite::writesint32toarray(int field_number,
                                                 int32 value,
                                                 uint8* target) {
  target = writetagtoarray(field_number, wiretype_varint, target);
  return writesint32notagtoarray(value, target);
}
inline uint8* wireformatlite::writesint64toarray(int field_number,
                                                 int64 value,
                                                 uint8* target) {
  target = writetagtoarray(field_number, wiretype_varint, target);
  return writesint64notagtoarray(value, target);
}
inline uint8* wireformatlite::writefixed32toarray(int field_number,
                                                  uint32 value,
                                                  uint8* target) {
  target = writetagtoarray(field_number, wiretype_fixed32, target);
  return writefixed32notagtoarray(value, target);
}
inline uint8* wireformatlite::writefixed64toarray(int field_number,
                                                  uint64 value,
                                                  uint8* target) {
  target = writetagtoarray(field_number, wiretype_fixed64, target);
  return writefixed64notagtoarray(value, target);
}
inline uint8* wireformatlite::writesfixed32toarray(int field_number,
                                                   int32 value,
                                                   uint8* target) {
  target = writetagtoarray(field_number, wiretype_fixed32, target);
  return writesfixed32notagtoarray(value, target);
}
inline uint8* wireformatlite::writesfixed64toarray(int field_number,
                                                   int64 value,
                                                   uint8* target) {
  target = writetagtoarray(field_number, wiretype_fixed64, target);
  return writesfixed64notagtoarray(value, target);
}
inline uint8* wireformatlite::writefloattoarray(int field_number,
                                                float value,
                                                uint8* target) {
  target = writetagtoarray(field_number, wiretype_fixed32, target);
  return writefloatnotagtoarray(value, target);
}
inline uint8* wireformatlite::writedoubletoarray(int field_number,
                                                 double value,
                                                 uint8* target) {
  target = writetagtoarray(field_number, wiretype_fixed64, target);
  return writedoublenotagtoarray(value, target);
}
inline uint8* wireformatlite::writebooltoarray(int field_number,
                                               bool value,
                                               uint8* target) {
  target = writetagtoarray(field_number, wiretype_varint, target);
  return writeboolnotagtoarray(value, target);
}
inline uint8* wireformatlite::writeenumtoarray(int field_number,
                                               int value,
                                               uint8* target) {
  target = writetagtoarray(field_number, wiretype_varint, target);
  return writeenumnotagtoarray(value, target);
}

inline uint8* wireformatlite::writestringtoarray(int field_number,
                                                 const string& value,
                                                 uint8* target) {
  // string is for utf-8 text only
  // warning:  in wire_format.cc, both strings and bytes are handled by
  //   writestring() to avoid code duplication.  if the implementations become
  //   different, you will need to update that usage.
  target = writetagtoarray(field_number, wiretype_length_delimited, target);
  target = io::codedoutputstream::writevarint32toarray(value.size(), target);
  return io::codedoutputstream::writestringtoarray(value, target);
}
inline uint8* wireformatlite::writebytestoarray(int field_number,
                                                const string& value,
                                                uint8* target) {
  target = writetagtoarray(field_number, wiretype_length_delimited, target);
  target = io::codedoutputstream::writevarint32toarray(value.size(), target);
  return io::codedoutputstream::writestringtoarray(value, target);
}


inline uint8* wireformatlite::writegrouptoarray(int field_number,
                                                const messagelite& value,
                                                uint8* target) {
  target = writetagtoarray(field_number, wiretype_start_group, target);
  target = value.serializewithcachedsizestoarray(target);
  return writetagtoarray(field_number, wiretype_end_group, target);
}
inline uint8* wireformatlite::writemessagetoarray(int field_number,
                                                  const messagelite& value,
                                                  uint8* target) {
  target = writetagtoarray(field_number, wiretype_length_delimited, target);
  target = io::codedoutputstream::writevarint32toarray(
    value.getcachedsize(), target);
  return value.serializewithcachedsizestoarray(target);
}

// see comment on readgroupnovirtual to understand the need for this template
// parameter name.
template<typename messagetype_workaroundcpplookupdefect>
inline uint8* wireformatlite::writegroupnovirtualtoarray(
    int field_number, const messagetype_workaroundcpplookupdefect& value,
    uint8* target) {
  target = writetagtoarray(field_number, wiretype_start_group, target);
  target = value.messagetype_workaroundcpplookupdefect
      ::serializewithcachedsizestoarray(target);
  return writetagtoarray(field_number, wiretype_end_group, target);
}
template<typename messagetype_workaroundcpplookupdefect>
inline uint8* wireformatlite::writemessagenovirtualtoarray(
    int field_number, const messagetype_workaroundcpplookupdefect& value,
    uint8* target) {
  target = writetagtoarray(field_number, wiretype_length_delimited, target);
  target = io::codedoutputstream::writevarint32toarray(
    value.messagetype_workaroundcpplookupdefect::getcachedsize(), target);
  return value.messagetype_workaroundcpplookupdefect
      ::serializewithcachedsizestoarray(target);
}

// ===================================================================

inline int wireformatlite::int32size(int32 value) {
  return io::codedoutputstream::varintsize32signextended(value);
}
inline int wireformatlite::int64size(int64 value) {
  return io::codedoutputstream::varintsize64(static_cast<uint64>(value));
}
inline int wireformatlite::uint32size(uint32 value) {
  return io::codedoutputstream::varintsize32(value);
}
inline int wireformatlite::uint64size(uint64 value) {
  return io::codedoutputstream::varintsize64(value);
}
inline int wireformatlite::sint32size(int32 value) {
  return io::codedoutputstream::varintsize32(zigzagencode32(value));
}
inline int wireformatlite::sint64size(int64 value) {
  return io::codedoutputstream::varintsize64(zigzagencode64(value));
}
inline int wireformatlite::enumsize(int value) {
  return io::codedoutputstream::varintsize32signextended(value);
}

inline int wireformatlite::stringsize(const string& value) {
  return io::codedoutputstream::varintsize32(value.size()) +
         value.size();
}
inline int wireformatlite::bytessize(const string& value) {
  return io::codedoutputstream::varintsize32(value.size()) +
         value.size();
}


inline int wireformatlite::groupsize(const messagelite& value) {
  return value.bytesize();
}
inline int wireformatlite::messagesize(const messagelite& value) {
  return lengthdelimitedsize(value.bytesize());
}

// see comment on readgroupnovirtual to understand the need for this template
// parameter name.
template<typename messagetype_workaroundcpplookupdefect>
inline int wireformatlite::groupsizenovirtual(
    const messagetype_workaroundcpplookupdefect& value) {
  return value.messagetype_workaroundcpplookupdefect::bytesize();
}
template<typename messagetype_workaroundcpplookupdefect>
inline int wireformatlite::messagesizenovirtual(
    const messagetype_workaroundcpplookupdefect& value) {
  return lengthdelimitedsize(
      value.messagetype_workaroundcpplookupdefect::bytesize());
}

inline int wireformatlite::lengthdelimitedsize(int length) {
  return io::codedoutputstream::varintsize32(length) + length;
}

}  // namespace internal
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_wire_format_lite_inl_h__
