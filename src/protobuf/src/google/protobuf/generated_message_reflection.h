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
// this header is logically internal, but is made public because it is used
// from protocol-compiler-generated code, which may reside in other components.

#ifndef google_protobuf_generated_message_reflection_h__
#define google_protobuf_generated_message_reflection_h__

#include <string>
#include <vector>
#include <google/protobuf/stubs/common.h>
// todo(jasonh): remove this once the compiler change to directly include this
// is released to components.
#include <google/protobuf/generated_enum_reflection.h>
#include <google/protobuf/message.h>
#include <google/protobuf/unknown_field_set.h>


namespace google {
namespace upb {
namespace google_opensource {
class gmr_handlers;
}  // namespace google_opensource
}  // namespace upb

namespace protobuf {
  class descriptorpool;
}

namespace protobuf {
namespace internal {

// defined in this file.
class generatedmessagereflection;

// defined in other files.
class extensionset;             // extension_set.h

// this class is not intended for direct use.  it is intended for use
// by generated code.  this class is just a big hack that reduces code
// size.
//
// a generatedmessagereflection is an implementation of reflection
// which expects all fields to be backed by simple variables located in
// memory.  the locations are given using a base pointer and a set of
// offsets.
//
// it is required that the user represents fields of each type in a standard
// way, so that generatedmessagereflection can cast the void* pointer to
// the appropriate type.  for primitive fields and string fields, each field
// should be represented using the obvious c++ primitive type.  enums and
// messages are different:
//  - singular message fields are stored as a pointer to a message.  these
//    should start out null, except for in the default instance where they
//    should start out pointing to other default instances.
//  - enum fields are stored as an int.  this int must always contain
//    a valid value, such that enumdescriptor::findvaluebynumber() would
//    not return null.
//  - repeated fields are stored as repeatedfields or repeatedptrfields
//    of whatever type the individual field would be.  strings and
//    messages use repeatedptrfields while everything else uses
//    repeatedfields.
class libprotobuf_export generatedmessagereflection : public reflection {
 public:
  // constructs a generatedmessagereflection.
  // parameters:
  //   descriptor:    the descriptor for the message type being implemented.
  //   default_instance:  the default instance of the message.  this is only
  //                  used to obtain pointers to default instances of embedded
  //                  messages, which getmessage() will return if the particular
  //                  sub-message has not been initialized yet.  (thus, all
  //                  embedded message fields *must* have non-null pointers
  //                  in the default instance.)
  //   offsets:       an array of ints giving the byte offsets, relative to
  //                  the start of the message object, of each field.  these can
  //                  be computed at compile time using the
  //                  google_protobuf_generated_message_field_offset() macro, defined
  //                  below.
  //   has_bits_offset:  offset in the message of an array of uint32s of size
  //                  descriptor->field_count()/32, rounded up.  this is a
  //                  bitfield where each bit indicates whether or not the
  //                  corresponding field of the message has been initialized.
  //                  the bit for field index i is obtained by the expression:
  //                    has_bits[i / 32] & (1 << (i % 32))
  //   unknown_fields_offset:  offset in the message of the unknownfieldset for
  //                  the message.
  //   extensions_offset:  offset in the message of the extensionset for the
  //                  message, or -1 if the message type has no extension
  //                  ranges.
  //   pool:          descriptorpool to search for extension definitions.  only
  //                  used by findknownextensionbyname() and
  //                  findknownextensionbynumber().
  //   factory:       messagefactory to use to construct extension messages.
  //   object_size:   the size of a message object of this type, as measured
  //                  by sizeof().
  generatedmessagereflection(const descriptor* descriptor,
                             const message* default_instance,
                             const int offsets[],
                             int has_bits_offset,
                             int unknown_fields_offset,
                             int extensions_offset,
                             const descriptorpool* pool,
                             messagefactory* factory,
                             int object_size);
  ~generatedmessagereflection();

  // implements reflection -------------------------------------------

  const unknownfieldset& getunknownfields(const message& message) const;
  unknownfieldset* mutableunknownfields(message* message) const;

  int spaceused(const message& message) const;

  bool hasfield(const message& message, const fielddescriptor* field) const;
  int fieldsize(const message& message, const fielddescriptor* field) const;
  void clearfield(message* message, const fielddescriptor* field) const;
  void removelast(message* message, const fielddescriptor* field) const;
  message* releaselast(message* message, const fielddescriptor* field) const;
  void swap(message* message1, message* message2) const;
  void swapelements(message* message, const fielddescriptor* field,
            int index1, int index2) const;
  void listfields(const message& message,
                  vector<const fielddescriptor*>* output) const;

  int32  getint32 (const message& message,
                   const fielddescriptor* field) const;
  int64  getint64 (const message& message,
                   const fielddescriptor* field) const;
  uint32 getuint32(const message& message,
                   const fielddescriptor* field) const;
  uint64 getuint64(const message& message,
                   const fielddescriptor* field) const;
  float  getfloat (const message& message,
                   const fielddescriptor* field) const;
  double getdouble(const message& message,
                   const fielddescriptor* field) const;
  bool   getbool  (const message& message,
                   const fielddescriptor* field) const;
  string getstring(const message& message,
                   const fielddescriptor* field) const;
  const string& getstringreference(const message& message,
                                   const fielddescriptor* field,
                                   string* scratch) const;
  const enumvaluedescriptor* getenum(const message& message,
                                     const fielddescriptor* field) const;
  const message& getmessage(const message& message,
                            const fielddescriptor* field,
                            messagefactory* factory = null) const;

  void setint32 (message* message,
                 const fielddescriptor* field, int32  value) const;
  void setint64 (message* message,
                 const fielddescriptor* field, int64  value) const;
  void setuint32(message* message,
                 const fielddescriptor* field, uint32 value) const;
  void setuint64(message* message,
                 const fielddescriptor* field, uint64 value) const;
  void setfloat (message* message,
                 const fielddescriptor* field, float  value) const;
  void setdouble(message* message,
                 const fielddescriptor* field, double value) const;
  void setbool  (message* message,
                 const fielddescriptor* field, bool   value) const;
  void setstring(message* message,
                 const fielddescriptor* field,
                 const string& value) const;
  void setenum  (message* message, const fielddescriptor* field,
                 const enumvaluedescriptor* value) const;
  message* mutablemessage(message* message, const fielddescriptor* field,
                          messagefactory* factory = null) const;
  message* releasemessage(message* message, const fielddescriptor* field,
                          messagefactory* factory = null) const;

  int32  getrepeatedint32 (const message& message,
                           const fielddescriptor* field, int index) const;
  int64  getrepeatedint64 (const message& message,
                           const fielddescriptor* field, int index) const;
  uint32 getrepeateduint32(const message& message,
                           const fielddescriptor* field, int index) const;
  uint64 getrepeateduint64(const message& message,
                           const fielddescriptor* field, int index) const;
  float  getrepeatedfloat (const message& message,
                           const fielddescriptor* field, int index) const;
  double getrepeateddouble(const message& message,
                           const fielddescriptor* field, int index) const;
  bool   getrepeatedbool  (const message& message,
                           const fielddescriptor* field, int index) const;
  string getrepeatedstring(const message& message,
                           const fielddescriptor* field, int index) const;
  const string& getrepeatedstringreference(const message& message,
                                           const fielddescriptor* field,
                                           int index, string* scratch) const;
  const enumvaluedescriptor* getrepeatedenum(const message& message,
                                             const fielddescriptor* field,
                                             int index) const;
  const message& getrepeatedmessage(const message& message,
                                    const fielddescriptor* field,
                                    int index) const;

  // set the value of a field.
  void setrepeatedint32 (message* message,
                         const fielddescriptor* field, int index, int32  value) const;
  void setrepeatedint64 (message* message,
                         const fielddescriptor* field, int index, int64  value) const;
  void setrepeateduint32(message* message,
                         const fielddescriptor* field, int index, uint32 value) const;
  void setrepeateduint64(message* message,
                         const fielddescriptor* field, int index, uint64 value) const;
  void setrepeatedfloat (message* message,
                         const fielddescriptor* field, int index, float  value) const;
  void setrepeateddouble(message* message,
                         const fielddescriptor* field, int index, double value) const;
  void setrepeatedbool  (message* message,
                         const fielddescriptor* field, int index, bool   value) const;
  void setrepeatedstring(message* message,
                         const fielddescriptor* field, int index,
                         const string& value) const;
  void setrepeatedenum(message* message, const fielddescriptor* field,
                       int index, const enumvaluedescriptor* value) const;
  // get a mutable pointer to a field with a message type.
  message* mutablerepeatedmessage(message* message,
                                  const fielddescriptor* field,
                                  int index) const;

  void addint32 (message* message,
                 const fielddescriptor* field, int32  value) const;
  void addint64 (message* message,
                 const fielddescriptor* field, int64  value) const;
  void adduint32(message* message,
                 const fielddescriptor* field, uint32 value) const;
  void adduint64(message* message,
                 const fielddescriptor* field, uint64 value) const;
  void addfloat (message* message,
                 const fielddescriptor* field, float  value) const;
  void adddouble(message* message,
                 const fielddescriptor* field, double value) const;
  void addbool  (message* message,
                 const fielddescriptor* field, bool   value) const;
  void addstring(message* message,
                 const fielddescriptor* field, const string& value) const;
  void addenum(message* message,
               const fielddescriptor* field,
               const enumvaluedescriptor* value) const;
  message* addmessage(message* message, const fielddescriptor* field,
                      messagefactory* factory = null) const;

  const fielddescriptor* findknownextensionbyname(const string& name) const;
  const fielddescriptor* findknownextensionbynumber(int number) const;

 protected:
  virtual void* mutablerawrepeatedfield(
      message* message, const fielddescriptor* field, fielddescriptor::cpptype,
      int ctype, const descriptor* desc) const;

 private:
  friend class generatedmessage;

  // to parse directly into a proto2 generated class, the class gmr_handlers
  // needs access to member offsets and hasbits.
  friend class libprotobuf_export upb::google_opensource::gmr_handlers;

  const descriptor* descriptor_;
  const message* default_instance_;
  const int* offsets_;

  int has_bits_offset_;
  int unknown_fields_offset_;
  int extensions_offset_;
  int object_size_;

  const descriptorpool* descriptor_pool_;
  messagefactory* message_factory_;

  template <typename type>
  inline const type& getraw(const message& message,
                            const fielddescriptor* field) const;
  template <typename type>
  inline type* mutableraw(message* message,
                          const fielddescriptor* field) const;
  template <typename type>
  inline const type& defaultraw(const fielddescriptor* field) const;

  inline const uint32* gethasbits(const message& message) const;
  inline uint32* mutablehasbits(message* message) const;
  inline const extensionset& getextensionset(const message& message) const;
  inline extensionset* mutableextensionset(message* message) const;

  inline bool hasbit(const message& message,
                     const fielddescriptor* field) const;
  inline void setbit(message* message,
                     const fielddescriptor* field) const;
  inline void clearbit(message* message,
                       const fielddescriptor* field) const;

  template <typename type>
  inline const type& getfield(const message& message,
                              const fielddescriptor* field) const;
  template <typename type>
  inline void setfield(message* message,
                       const fielddescriptor* field, const type& value) const;
  template <typename type>
  inline type* mutablefield(message* message,
                            const fielddescriptor* field) const;
  template <typename type>
  inline const type& getrepeatedfield(const message& message,
                                      const fielddescriptor* field,
                                      int index) const;
  template <typename type>
  inline const type& getrepeatedptrfield(const message& message,
                                         const fielddescriptor* field,
                                         int index) const;
  template <typename type>
  inline void setrepeatedfield(message* message,
                               const fielddescriptor* field, int index,
                               type value) const;
  template <typename type>
  inline type* mutablerepeatedfield(message* message,
                                    const fielddescriptor* field,
                                    int index) const;
  template <typename type>
  inline void addfield(message* message,
                       const fielddescriptor* field, const type& value) const;
  template <typename type>
  inline type* addfield(message* message,
                        const fielddescriptor* field) const;

  int getextensionnumberordie(const descriptor* type) const;

  google_disallow_evil_constructors(generatedmessagereflection);
};

// returns the offset of the given field within the given aggregate type.
// this is equivalent to the ansi c offsetof() macro.  however, according
// to the c++ standard, offsetof() only works on pod types, and gcc
// enforces this requirement with a warning.  in practice, this rule is
// unnecessarily strict; there is probably no compiler or platform on
// which the offsets of the direct fields of a class are non-constant.
// fields inherited from superclasses *can* have non-constant offsets,
// but that's not what this macro will be used for.
//
// note that we calculate relative to the pointer value 16 here since if we
// just use zero, gcc complains about dereferencing a null pointer.  we
// choose 16 rather than some other number just in case the compiler would
// be confused by an unaligned pointer.
#define google_protobuf_generated_message_field_offset(type, field)    \
  static_cast<int>(                                           \
    reinterpret_cast<const char*>(                            \
      &reinterpret_cast<const type*>(16)->field) -            \
    reinterpret_cast<const char*>(16))

// there are some places in proto2 where dynamic_cast would be useful as an
// optimization.  for example, take message::mergefrom(const message& other).
// for a given generated message foomessage, we generate these two methods:
//   void mergefrom(const foomessage& other);
//   void mergefrom(const message& other);
// the former method can be implemented directly in terms of foomessage's
// inline accessors, but the latter method must work with the reflection
// interface.  however, if the parameter to the latter method is actually of
// type foomessage, then we'd like to be able to just call the other method
// as an optimization.  so, we use dynamic_cast to check this.
//
// that said, dynamic_cast requires rtti, which many people like to disable
// for performance and code size reasons.  when rtti is not available, we
// still need to produce correct results.  so, in this case we have to fall
// back to using reflection, which is what we would have done anyway if the
// objects were not of the exact same class.
//
// dynamic_cast_if_available() implements this logic.  if rtti is
// enabled, it does a dynamic_cast.  if rtti is disabled, it just returns
// null.
//
// if you need to compile without rtti, simply #define google_protobuf_no_rtti.
// on msvc, this should be detected automatically.
template<typename to, typename from>
inline to dynamic_cast_if_available(from from) {
#if defined(google_protobuf_no_rtti) || (defined(_msc_ver)&&!defined(_cpprtti))
  return null;
#else
  return dynamic_cast<to>(from);
#endif
}

}  // namespace internal
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_generated_message_reflection_h__
