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
// contains classes used to keep track of unrecognized fields seen while
// parsing a protocol message.

#ifndef google_protobuf_unknown_field_set_h__
#define google_protobuf_unknown_field_set_h__

#include <assert.h>
#include <string>
#include <vector>
#include <google/protobuf/stubs/common.h>
// todo(jasonh): some people seem to rely on protobufs to include this for them!

namespace google {
namespace protobuf {
  namespace io {
    class codedinputstream;         // coded_stream.h
    class codedoutputstream;        // coded_stream.h
    class zerocopyinputstream;      // zero_copy_stream.h
  }
  namespace internal {
    class wireformat;               // wire_format.h
    class unknownfieldsetfieldskipperusingcord;
                                    // extension_set_heavy.cc
  }

class message;                      // message.h
class unknownfield;                 // below

// an unknownfieldset contains fields that were encountered while parsing a
// message but were not defined by its type.  keeping track of these can be
// useful, especially in that they may be written if the message is serialized
// again without being cleared in between.  this means that software which
// simply receives messages and forwards them to other servers does not need
// to be updated every time a new field is added to the message definition.
//
// to get the unknownfieldset attached to any message, call
// reflection::getunknownfields().
//
// this class is necessarily tied to the protocol buffer wire format, unlike
// the reflection interface which is independent of any serialization scheme.
class libprotobuf_export unknownfieldset {
 public:
  unknownfieldset();
  ~unknownfieldset();

  // remove all fields.
  inline void clear();

  // remove all fields and deallocate internal data objects
  void clearandfreememory();

  // is this set empty?
  inline bool empty() const;

  // merge the contents of some other unknownfieldset with this one.
  void mergefrom(const unknownfieldset& other);

  // swaps the contents of some other unknownfieldset with this one.
  inline void swap(unknownfieldset* x);

  // computes (an estimate of) the total number of bytes currently used for
  // storing the unknown fields in memory. does not include
  // sizeof(*this) in the calculation.
  int spaceusedexcludingself() const;

  // version of spaceused() including sizeof(*this).
  int spaceused() const;

  // returns the number of fields present in the unknownfieldset.
  inline int field_count() const;
  // get a field in the set, where 0 <= index < field_count().  the fields
  // appear in the order in which they were added.
  inline const unknownfield& field(int index) const;
  // get a mutable pointer to a field in the set, where
  // 0 <= index < field_count().  the fields appear in the order in which
  // they were added.
  inline unknownfield* mutable_field(int index);

  // adding fields ---------------------------------------------------

  void addvarint(int number, uint64 value);
  void addfixed32(int number, uint32 value);
  void addfixed64(int number, uint64 value);
  void addlengthdelimited(int number, const string& value);
  string* addlengthdelimited(int number);
  unknownfieldset* addgroup(int number);

  // adds an unknown field from another set.
  void addfield(const unknownfield& field);

  // delete fields with indices in the range [start .. start+num-1].
  // caution: implementation moves all fields with indices [start+num .. ].
  void deletesubrange(int start, int num);

  // delete all fields with a specific field number. the order of left fields
  // is preserved.
  // caution: implementation moves all fields after the first deleted field.
  void deletebynumber(int number);

  // parsing helpers -------------------------------------------------
  // these work exactly like the similarly-named methods of message.

  bool mergefromcodedstream(io::codedinputstream* input);
  bool parsefromcodedstream(io::codedinputstream* input);
  bool parsefromzerocopystream(io::zerocopyinputstream* input);
  bool parsefromarray(const void* data, int size);
  inline bool parsefromstring(const string& data) {
    return parsefromarray(data.data(), data.size());
  }

 private:

  void clearfallback();

  vector<unknownfield>* fields_;

  google_disallow_evil_constructors(unknownfieldset);
};

// represents one field in an unknownfieldset.
class libprotobuf_export unknownfield {
 public:
  enum type {
    type_varint,
    type_fixed32,
    type_fixed64,
    type_length_delimited,
    type_group
  };

  // the field's tag number, as seen on the wire.
  inline int number() const;

  // the field type.
  inline type type() const;

  // accessors -------------------------------------------------------
  // each method works only for unknownfields of the corresponding type.

  inline uint64 varint() const;
  inline uint32 fixed32() const;
  inline uint64 fixed64() const;
  inline const string& length_delimited() const;
  inline const unknownfieldset& group() const;

  inline void set_varint(uint64 value);
  inline void set_fixed32(uint32 value);
  inline void set_fixed64(uint64 value);
  inline void set_length_delimited(const string& value);
  inline string* mutable_length_delimited();
  inline unknownfieldset* mutable_group();

  // serialization api.
  // these methods can take advantage of the underlying implementation and may
  // archieve a better performance than using getters to retrieve the data and
  // do the serialization yourself.
  void serializelengthdelimitednotag(io::codedoutputstream* output) const;
  uint8* serializelengthdelimitednotagtoarray(uint8* target) const;

  inline int getlengthdelimitedsize() const;

 private:
  friend class unknownfieldset;

  // if this unknownfield contains a pointer, delete it.
  void delete();

  // make a deep copy of any pointers in this unknownfield.
  void deepcopy();


  unsigned int number_ : 29;
  unsigned int type_   : 3;
  union {
    uint64 varint_;
    uint32 fixed32_;
    uint64 fixed64_;
    mutable union {
      string* string_value_;
    } length_delimited_;
    unknownfieldset* group_;
  };
};

// ===================================================================
// inline implementations

inline void unknownfieldset::clear() {
  if (fields_ != null) {
    clearfallback();
  }
}

inline bool unknownfieldset::empty() const {
  return fields_ == null || fields_->empty();
}

inline void unknownfieldset::swap(unknownfieldset* x) {
  std::swap(fields_, x->fields_);
}

inline int unknownfieldset::field_count() const {
  return (fields_ == null) ? 0 : fields_->size();
}
inline const unknownfield& unknownfieldset::field(int index) const {
  return (*fields_)[index];
}
inline unknownfield* unknownfieldset::mutable_field(int index) {
  return &(*fields_)[index];
}

inline void unknownfieldset::addlengthdelimited(
    int number, const string& value) {
  addlengthdelimited(number)->assign(value);
}


inline int unknownfield::number() const { return number_; }
inline unknownfield::type unknownfield::type() const {
  return static_cast<type>(type_);
}

inline uint64 unknownfield::varint () const {
  assert(type_ == type_varint);
  return varint_;
}
inline uint32 unknownfield::fixed32() const {
  assert(type_ == type_fixed32);
  return fixed32_;
}
inline uint64 unknownfield::fixed64() const {
  assert(type_ == type_fixed64);
  return fixed64_;
}
inline const string& unknownfield::length_delimited() const {
  assert(type_ == type_length_delimited);
  return *length_delimited_.string_value_;
}
inline const unknownfieldset& unknownfield::group() const {
  assert(type_ == type_group);
  return *group_;
}

inline void unknownfield::set_varint(uint64 value) {
  assert(type_ == type_varint);
  varint_ = value;
}
inline void unknownfield::set_fixed32(uint32 value) {
  assert(type_ == type_fixed32);
  fixed32_ = value;
}
inline void unknownfield::set_fixed64(uint64 value) {
  assert(type_ == type_fixed64);
  fixed64_ = value;
}
inline void unknownfield::set_length_delimited(const string& value) {
  assert(type_ == type_length_delimited);
  length_delimited_.string_value_->assign(value);
}
inline string* unknownfield::mutable_length_delimited() {
  assert(type_ == type_length_delimited);
  return length_delimited_.string_value_;
}
inline unknownfieldset* unknownfield::mutable_group() {
  assert(type_ == type_group);
  return group_;
}

inline int unknownfield::getlengthdelimitedsize() const {
  google_dcheck_eq(type_length_delimited, type_);
  return length_delimited_.string_value_->size();
}

}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_unknown_field_set_h__
