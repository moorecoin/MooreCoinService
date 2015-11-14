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

#include <google/protobuf/wire_format_lite_inl.h>

#include <stack>
#include <string>
#include <vector>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/coded_stream_inl.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

namespace google {
namespace protobuf {
namespace internal {

#ifndef _msc_ver    // msvc doesn't like definitions of inline constants, gcc
                    // requires them.
const int wireformatlite::kmessagesetitemstarttag;
const int wireformatlite::kmessagesetitemendtag;
const int wireformatlite::kmessagesettypeidtag;
const int wireformatlite::kmessagesetmessagetag;

#endif

const int wireformatlite::kmessagesetitemtagssize =
  io::codedoutputstream::staticvarintsize32<kmessagesetitemstarttag>::value +
  io::codedoutputstream::staticvarintsize32<kmessagesetitemendtag>::value +
  io::codedoutputstream::staticvarintsize32<kmessagesettypeidtag>::value +
  io::codedoutputstream::staticvarintsize32<kmessagesetmessagetag>::value;

const wireformatlite::cpptype
wireformatlite::kfieldtypetocpptypemap[max_field_type + 1] = {
  static_cast<cpptype>(0),  // 0 is reserved for errors

  cpptype_double,   // type_double
  cpptype_float,    // type_float
  cpptype_int64,    // type_int64
  cpptype_uint64,   // type_uint64
  cpptype_int32,    // type_int32
  cpptype_uint64,   // type_fixed64
  cpptype_uint32,   // type_fixed32
  cpptype_bool,     // type_bool
  cpptype_string,   // type_string
  cpptype_message,  // type_group
  cpptype_message,  // type_message
  cpptype_string,   // type_bytes
  cpptype_uint32,   // type_uint32
  cpptype_enum,     // type_enum
  cpptype_int32,    // type_sfixed32
  cpptype_int64,    // type_sfixed64
  cpptype_int32,    // type_sint32
  cpptype_int64,    // type_sint64
};

const wireformatlite::wiretype
wireformatlite::kwiretypeforfieldtype[max_field_type + 1] = {
  static_cast<wireformatlite::wiretype>(-1),  // invalid
  wireformatlite::wiretype_fixed64,           // type_double
  wireformatlite::wiretype_fixed32,           // type_float
  wireformatlite::wiretype_varint,            // type_int64
  wireformatlite::wiretype_varint,            // type_uint64
  wireformatlite::wiretype_varint,            // type_int32
  wireformatlite::wiretype_fixed64,           // type_fixed64
  wireformatlite::wiretype_fixed32,           // type_fixed32
  wireformatlite::wiretype_varint,            // type_bool
  wireformatlite::wiretype_length_delimited,  // type_string
  wireformatlite::wiretype_start_group,       // type_group
  wireformatlite::wiretype_length_delimited,  // type_message
  wireformatlite::wiretype_length_delimited,  // type_bytes
  wireformatlite::wiretype_varint,            // type_uint32
  wireformatlite::wiretype_varint,            // type_enum
  wireformatlite::wiretype_fixed32,           // type_sfixed32
  wireformatlite::wiretype_fixed64,           // type_sfixed64
  wireformatlite::wiretype_varint,            // type_sint32
  wireformatlite::wiretype_varint,            // type_sint64
};

bool wireformatlite::skipfield(
    io::codedinputstream* input, uint32 tag) {
  switch (wireformatlite::gettagwiretype(tag)) {
    case wireformatlite::wiretype_varint: {
      uint64 value;
      if (!input->readvarint64(&value)) return false;
      return true;
    }
    case wireformatlite::wiretype_fixed64: {
      uint64 value;
      if (!input->readlittleendian64(&value)) return false;
      return true;
    }
    case wireformatlite::wiretype_length_delimited: {
      uint32 length;
      if (!input->readvarint32(&length)) return false;
      if (!input->skip(length)) return false;
      return true;
    }
    case wireformatlite::wiretype_start_group: {
      if (!input->incrementrecursiondepth()) return false;
      if (!skipmessage(input)) return false;
      input->decrementrecursiondepth();
      // check that the ending tag matched the starting tag.
      if (!input->lasttagwas(wireformatlite::maketag(
          wireformatlite::gettagfieldnumber(tag),
          wireformatlite::wiretype_end_group))) {
        return false;
      }
      return true;
    }
    case wireformatlite::wiretype_end_group: {
      return false;
    }
    case wireformatlite::wiretype_fixed32: {
      uint32 value;
      if (!input->readlittleendian32(&value)) return false;
      return true;
    }
    default: {
      return false;
    }
  }
}

bool wireformatlite::skipmessage(io::codedinputstream* input) {
  while(true) {
    uint32 tag = input->readtag();
    if (tag == 0) {
      // end of input.  this is a valid place to end, so return true.
      return true;
    }

    wireformatlite::wiretype wire_type = wireformatlite::gettagwiretype(tag);

    if (wire_type == wireformatlite::wiretype_end_group) {
      // must be the end of the message.
      return true;
    }

    if (!skipfield(input, tag)) return false;
  }
}

bool fieldskipper::skipfield(
    io::codedinputstream* input, uint32 tag) {
  return wireformatlite::skipfield(input, tag);
}

bool fieldskipper::skipmessage(io::codedinputstream* input) {
  return wireformatlite::skipmessage(input);
}

void fieldskipper::skipunknownenum(
    int field_number, int value) {
  // nothing.
}

bool wireformatlite::readpackedenumnoinline(io::codedinputstream* input,
                                            bool (*is_valid)(int),
                                            repeatedfield<int>* values) {
  uint32 length;
  if (!input->readvarint32(&length)) return false;
  io::codedinputstream::limit limit = input->pushlimit(length);
  while (input->bytesuntillimit() > 0) {
    int value;
    if (!google::protobuf::internal::wireformatlite::readprimitive<
        int, wireformatlite::type_enum>(input, &value)) {
      return false;
    }
    if (is_valid(value)) {
      values->add(value);
    }
  }
  input->poplimit(limit);
  return true;
}

void wireformatlite::writeint32(int field_number, int32 value,
                                io::codedoutputstream* output) {
  writetag(field_number, wiretype_varint, output);
  writeint32notag(value, output);
}
void wireformatlite::writeint64(int field_number, int64 value,
                                io::codedoutputstream* output) {
  writetag(field_number, wiretype_varint, output);
  writeint64notag(value, output);
}
void wireformatlite::writeuint32(int field_number, uint32 value,
                                 io::codedoutputstream* output) {
  writetag(field_number, wiretype_varint, output);
  writeuint32notag(value, output);
}
void wireformatlite::writeuint64(int field_number, uint64 value,
                                 io::codedoutputstream* output) {
  writetag(field_number, wiretype_varint, output);
  writeuint64notag(value, output);
}
void wireformatlite::writesint32(int field_number, int32 value,
                                 io::codedoutputstream* output) {
  writetag(field_number, wiretype_varint, output);
  writesint32notag(value, output);
}
void wireformatlite::writesint64(int field_number, int64 value,
                                 io::codedoutputstream* output) {
  writetag(field_number, wiretype_varint, output);
  writesint64notag(value, output);
}
void wireformatlite::writefixed32(int field_number, uint32 value,
                                  io::codedoutputstream* output) {
  writetag(field_number, wiretype_fixed32, output);
  writefixed32notag(value, output);
}
void wireformatlite::writefixed64(int field_number, uint64 value,
                                  io::codedoutputstream* output) {
  writetag(field_number, wiretype_fixed64, output);
  writefixed64notag(value, output);
}
void wireformatlite::writesfixed32(int field_number, int32 value,
                                   io::codedoutputstream* output) {
  writetag(field_number, wiretype_fixed32, output);
  writesfixed32notag(value, output);
}
void wireformatlite::writesfixed64(int field_number, int64 value,
                                   io::codedoutputstream* output) {
  writetag(field_number, wiretype_fixed64, output);
  writesfixed64notag(value, output);
}
void wireformatlite::writefloat(int field_number, float value,
                                io::codedoutputstream* output) {
  writetag(field_number, wiretype_fixed32, output);
  writefloatnotag(value, output);
}
void wireformatlite::writedouble(int field_number, double value,
                                 io::codedoutputstream* output) {
  writetag(field_number, wiretype_fixed64, output);
  writedoublenotag(value, output);
}
void wireformatlite::writebool(int field_number, bool value,
                               io::codedoutputstream* output) {
  writetag(field_number, wiretype_varint, output);
  writeboolnotag(value, output);
}
void wireformatlite::writeenum(int field_number, int value,
                               io::codedoutputstream* output) {
  writetag(field_number, wiretype_varint, output);
  writeenumnotag(value, output);
}

void wireformatlite::writestring(int field_number, const string& value,
                                 io::codedoutputstream* output) {
  // string is for utf-8 text only
  writetag(field_number, wiretype_length_delimited, output);
  google_check(value.size() <= kint32max);
  output->writevarint32(value.size());
  output->writestring(value);
}
void wireformatlite::writebytes(int field_number, const string& value,
                                io::codedoutputstream* output) {
  writetag(field_number, wiretype_length_delimited, output);
  google_check(value.size() <= kint32max);
  output->writevarint32(value.size());
  output->writestring(value);
}


void wireformatlite::writegroup(int field_number,
                                const messagelite& value,
                                io::codedoutputstream* output) {
  writetag(field_number, wiretype_start_group, output);
  value.serializewithcachedsizes(output);
  writetag(field_number, wiretype_end_group, output);
}

void wireformatlite::writemessage(int field_number,
                                  const messagelite& value,
                                  io::codedoutputstream* output) {
  writetag(field_number, wiretype_length_delimited, output);
  const int size = value.getcachedsize();
  output->writevarint32(size);
  value.serializewithcachedsizes(output);
}

void wireformatlite::writegroupmaybetoarray(int field_number,
                                            const messagelite& value,
                                            io::codedoutputstream* output) {
  writetag(field_number, wiretype_start_group, output);
  const int size = value.getcachedsize();
  uint8* target = output->getdirectbufferfornbytesandadvance(size);
  if (target != null) {
    uint8* end = value.serializewithcachedsizestoarray(target);
    google_dcheck_eq(end - target, size);
  } else {
    value.serializewithcachedsizes(output);
  }
  writetag(field_number, wiretype_end_group, output);
}

void wireformatlite::writemessagemaybetoarray(int field_number,
                                              const messagelite& value,
                                              io::codedoutputstream* output) {
  writetag(field_number, wiretype_length_delimited, output);
  const int size = value.getcachedsize();
  output->writevarint32(size);
  uint8* target = output->getdirectbufferfornbytesandadvance(size);
  if (target != null) {
    uint8* end = value.serializewithcachedsizestoarray(target);
    google_dcheck_eq(end - target, size);
  } else {
    value.serializewithcachedsizes(output);
  }
}

bool wireformatlite::readstring(io::codedinputstream* input,
                                string* value) {
  // string is for utf-8 text only
  uint32 length;
  if (!input->readvarint32(&length)) return false;
  if (!input->internalreadstringinline(value, length)) return false;
  return true;
}
bool wireformatlite::readbytes(io::codedinputstream* input,
                               string* value) {
  uint32 length;
  if (!input->readvarint32(&length)) return false;
  return input->internalreadstringinline(value, length);
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google
