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

#ifndef google_protobuf_extension_set_h__
#define google_protobuf_extension_set_h__

#include <vector>
#include <map>
#include <utility>
#include <string>


#include <google/protobuf/stubs/common.h>

namespace google {

namespace protobuf {
  class descriptor;                                    // descriptor.h
  class fielddescriptor;                               // descriptor.h
  class descriptorpool;                                // descriptor.h
  class messagelite;                                   // message_lite.h
  class message;                                       // message.h
  class messagefactory;                                // message.h
  class unknownfieldset;                               // unknown_field_set.h
  namespace io {
    class codedinputstream;                              // coded_stream.h
    class codedoutputstream;                             // coded_stream.h
  }
  namespace internal {
    class fieldskipper;                                  // wire_format_lite.h
    class repeatedptrfieldbase;                          // repeated_field.h
  }
  template <typename element> class repeatedfield;     // repeated_field.h
  template <typename element> class repeatedptrfield;  // repeated_field.h
}

namespace protobuf {
namespace internal {

// used to store values of type wireformatlite::fieldtype without having to
// #include wire_format_lite.h.  also, ensures that we use only one byte to
// store these values, which is important to keep the layout of
// extensionset::extension small.
typedef uint8 fieldtype;

// a function which, given an integer value, returns true if the number
// matches one of the defined values for the corresponding enum type.  this
// is used with registerenumextension, below.
typedef bool enumvalidityfunc(int number);

// version of the above which takes an argument.  this is needed to deal with
// extensions that are not compiled in.
typedef bool enumvalidityfuncwitharg(const void* arg, int number);

// information about a registered extension.
struct extensioninfo {
  inline extensioninfo() {}
  inline extensioninfo(fieldtype type_param, bool isrepeated, bool ispacked)
      : type(type_param), is_repeated(isrepeated), is_packed(ispacked),
        descriptor(null) {}

  fieldtype type;
  bool is_repeated;
  bool is_packed;

  struct enumvaliditycheck {
    enumvalidityfuncwitharg* func;
    const void* arg;
  };

  union {
    enumvaliditycheck enum_validity_check;
    const messagelite* message_prototype;
  };

  // the descriptor for this extension, if one exists and is known.  may be
  // null.  must not be null if the descriptor for the extension does not
  // live in the same pool as the descriptor for the containing type.
  const fielddescriptor* descriptor;
};

// abstract interface for an object which looks up extension definitions.  used
// when parsing.
class libprotobuf_export extensionfinder {
 public:
  virtual ~extensionfinder();

  // find the extension with the given containing type and number.
  virtual bool find(int number, extensioninfo* output) = 0;
};

// implementation of extensionfinder which finds extensions defined in .proto
// files which have been compiled into the binary.
class libprotobuf_export generatedextensionfinder : public extensionfinder {
 public:
  generatedextensionfinder(const messagelite* containing_type)
      : containing_type_(containing_type) {}
  virtual ~generatedextensionfinder() {}

  // returns true and fills in *output if found, otherwise returns false.
  virtual bool find(int number, extensioninfo* output);

 private:
  const messagelite* containing_type_;
};

// note:  extension_set_heavy.cc defines descriptorpoolextensionfinder for
// finding extensions from a descriptorpool.

// this is an internal helper class intended for use within the protocol buffer
// library and generated classes.  clients should not use it directly.  instead,
// use the generated accessors such as getextension() of the class being
// extended.
//
// this class manages extensions for a protocol message object.  the
// message's hasextension(), getextension(), mutableextension(), and
// clearextension() methods are just thin wrappers around the embedded
// extensionset.  when parsing, if a tag number is encountered which is
// inside one of the message type's extension ranges, the tag is passed
// off to the extensionset for parsing.  etc.
class libprotobuf_export extensionset {
 public:
  extensionset();
  ~extensionset();

  // these are called at startup by protocol-compiler-generated code to
  // register known extensions.  the registrations are used by parsefield()
  // to look up extensions for parsed field numbers.  note that dynamic parsing
  // does not use parsefield(); only protocol-compiler-generated parsing
  // methods do.
  static void registerextension(const messagelite* containing_type,
                                int number, fieldtype type,
                                bool is_repeated, bool is_packed);
  static void registerenumextension(const messagelite* containing_type,
                                    int number, fieldtype type,
                                    bool is_repeated, bool is_packed,
                                    enumvalidityfunc* is_valid);
  static void registermessageextension(const messagelite* containing_type,
                                       int number, fieldtype type,
                                       bool is_repeated, bool is_packed,
                                       const messagelite* prototype);

  // =================================================================

  // add all fields which are currently present to the given vector.  this
  // is useful to implement reflection::listfields().
  void appendtolist(const descriptor* containing_type,
                    const descriptorpool* pool,
                    vector<const fielddescriptor*>* output) const;

  // =================================================================
  // accessors
  //
  // generated message classes include type-safe templated wrappers around
  // these methods.  generally you should use those rather than call these
  // directly, unless you are doing low-level memory management.
  //
  // when calling any of these accessors, the extension number requested
  // must exist in the descriptorpool provided to the constructor.  otheriwse,
  // the method will fail an assert.  normally, though, you would not call
  // these directly; you would either call the generated accessors of your
  // message class (e.g. getextension()) or you would call the accessors
  // of the reflection interface.  in both cases, it is impossible to
  // trigger this assert failure:  the generated accessors only accept
  // linked-in extension types as parameters, while the reflection interface
  // requires you to provide the fielddescriptor describing the extension.
  //
  // when calling any of these accessors, a protocol-compiler-generated
  // implementation of the extension corresponding to the number must
  // be linked in, and the fielddescriptor used to refer to it must be
  // the one generated by that linked-in code.  otherwise, the method will
  // die on an assert failure.  the message objects returned by the message
  // accessors are guaranteed to be of the correct linked-in type.
  //
  // these methods pretty much match reflection except that:
  // - they're not virtual.
  // - they identify fields by number rather than fielddescriptors.
  // - they identify enum values using integers rather than descriptors.
  // - strings provide mutable() in addition to set() accessors.

  bool has(int number) const;
  int extensionsize(int number) const;   // size of a repeated extension.
  int numextensions() const;  // the number of extensions
  fieldtype extensiontype(int number) const;
  void clearextension(int number);

  // singular fields -------------------------------------------------

  int32  getint32 (int number, int32  default_value) const;
  int64  getint64 (int number, int64  default_value) const;
  uint32 getuint32(int number, uint32 default_value) const;
  uint64 getuint64(int number, uint64 default_value) const;
  float  getfloat (int number, float  default_value) const;
  double getdouble(int number, double default_value) const;
  bool   getbool  (int number, bool   default_value) const;
  int    getenum  (int number, int    default_value) const;
  const string & getstring (int number, const string&  default_value) const;
  const messagelite& getmessage(int number,
                                const messagelite& default_value) const;
  const messagelite& getmessage(int number, const descriptor* message_type,
                                messagefactory* factory) const;

  // |descriptor| may be null so long as it is known that the descriptor for
  // the extension lives in the same pool as the descriptor for the containing
  // type.
#define desc const fielddescriptor* descriptor  // avoid line wrapping
  void setint32 (int number, fieldtype type, int32  value, desc);
  void setint64 (int number, fieldtype type, int64  value, desc);
  void setuint32(int number, fieldtype type, uint32 value, desc);
  void setuint64(int number, fieldtype type, uint64 value, desc);
  void setfloat (int number, fieldtype type, float  value, desc);
  void setdouble(int number, fieldtype type, double value, desc);
  void setbool  (int number, fieldtype type, bool   value, desc);
  void setenum  (int number, fieldtype type, int    value, desc);
  void setstring(int number, fieldtype type, const string& value, desc);
  string * mutablestring (int number, fieldtype type, desc);
  messagelite* mutablemessage(int number, fieldtype type,
                              const messagelite& prototype, desc);
  messagelite* mutablemessage(const fielddescriptor* decsriptor,
                              messagefactory* factory);
  // adds the given message to the extensionset, taking ownership of the
  // message object. existing message with the same number will be deleted.
  // if "message" is null, this is equivalent to "clearextension(number)".
  void setallocatedmessage(int number, fieldtype type,
                           const fielddescriptor* descriptor,
                           messagelite* message);
  messagelite* releasemessage(int number, const messagelite& prototype);
  messagelite* releasemessage(const fielddescriptor* descriptor,
                              messagefactory* factory);
#undef desc

  // repeated fields -------------------------------------------------

  void* mutablerawrepeatedfield(int number);

  int32  getrepeatedint32 (int number, int index) const;
  int64  getrepeatedint64 (int number, int index) const;
  uint32 getrepeateduint32(int number, int index) const;
  uint64 getrepeateduint64(int number, int index) const;
  float  getrepeatedfloat (int number, int index) const;
  double getrepeateddouble(int number, int index) const;
  bool   getrepeatedbool  (int number, int index) const;
  int    getrepeatedenum  (int number, int index) const;
  const string & getrepeatedstring (int number, int index) const;
  const messagelite& getrepeatedmessage(int number, int index) const;

  void setrepeatedint32 (int number, int index, int32  value);
  void setrepeatedint64 (int number, int index, int64  value);
  void setrepeateduint32(int number, int index, uint32 value);
  void setrepeateduint64(int number, int index, uint64 value);
  void setrepeatedfloat (int number, int index, float  value);
  void setrepeateddouble(int number, int index, double value);
  void setrepeatedbool  (int number, int index, bool   value);
  void setrepeatedenum  (int number, int index, int    value);
  void setrepeatedstring(int number, int index, const string& value);
  string * mutablerepeatedstring (int number, int index);
  messagelite* mutablerepeatedmessage(int number, int index);

#define desc const fielddescriptor* descriptor  // avoid line wrapping
  void addint32 (int number, fieldtype type, bool packed, int32  value, desc);
  void addint64 (int number, fieldtype type, bool packed, int64  value, desc);
  void adduint32(int number, fieldtype type, bool packed, uint32 value, desc);
  void adduint64(int number, fieldtype type, bool packed, uint64 value, desc);
  void addfloat (int number, fieldtype type, bool packed, float  value, desc);
  void adddouble(int number, fieldtype type, bool packed, double value, desc);
  void addbool  (int number, fieldtype type, bool packed, bool   value, desc);
  void addenum  (int number, fieldtype type, bool packed, int    value, desc);
  void addstring(int number, fieldtype type, const string& value, desc);
  string * addstring (int number, fieldtype type, desc);
  messagelite* addmessage(int number, fieldtype type,
                          const messagelite& prototype, desc);
  messagelite* addmessage(const fielddescriptor* descriptor,
                          messagefactory* factory);
#undef desc

  void removelast(int number);
  messagelite* releaselast(int number);
  void swapelements(int number, int index1, int index2);

  // -----------------------------------------------------------------
  // todo(kenton):  hardcore memory management accessors

  // =================================================================
  // convenience methods for implementing methods of message
  //
  // these could all be implemented in terms of the other methods of this
  // class, but providing them here helps keep the generated code size down.

  void clear();
  void mergefrom(const extensionset& other);
  void swap(extensionset* other);
  bool isinitialized() const;

  // parses a single extension from the input. the input should start out
  // positioned immediately after the tag.
  bool parsefield(uint32 tag, io::codedinputstream* input,
                  extensionfinder* extension_finder,
                  fieldskipper* field_skipper);

  // specific versions for lite or full messages (constructs the appropriate
  // fieldskipper automatically).  |containing_type| is the default
  // instance for the containing message; it is used only to look up the
  // extension by number.  see registerextension(), above.  unlike the other
  // methods of extensionset, this only works for generated message types --
  // it looks up extensions registered using registerextension().
  bool parsefield(uint32 tag, io::codedinputstream* input,
                  const messagelite* containing_type);
  bool parsefield(uint32 tag, io::codedinputstream* input,
                  const message* containing_type,
                  unknownfieldset* unknown_fields);

  // parse an entire message in messageset format.  such messages have no
  // fields, only extensions.
  bool parsemessageset(io::codedinputstream* input,
                       extensionfinder* extension_finder,
                       fieldskipper* field_skipper);

  // specific versions for lite or full messages (constructs the appropriate
  // fieldskipper automatically).
  bool parsemessageset(io::codedinputstream* input,
                       const messagelite* containing_type);
  bool parsemessageset(io::codedinputstream* input,
                       const message* containing_type,
                       unknownfieldset* unknown_fields);

  // write all extension fields with field numbers in the range
  //   [start_field_number, end_field_number)
  // to the output stream, using the cached sizes computed when bytesize() was
  // last called.  note that the range bounds are inclusive-exclusive.
  void serializewithcachedsizes(int start_field_number,
                                int end_field_number,
                                io::codedoutputstream* output) const;

  // same as serializewithcachedsizes, but without any bounds checking.
  // the caller must ensure that target has sufficient capacity for the
  // serialized extensions.
  //
  // returns a pointer past the last written byte.
  uint8* serializewithcachedsizestoarray(int start_field_number,
                                         int end_field_number,
                                         uint8* target) const;

  // like above but serializes in messageset format.
  void serializemessagesetwithcachedsizes(io::codedoutputstream* output) const;
  uint8* serializemessagesetwithcachedsizestoarray(uint8* target) const;

  // returns the total serialized size of all the extensions.
  int bytesize() const;

  // like bytesize() but uses messageset format.
  int messagesetbytesize() const;

  // returns (an estimate of) the total number of bytes used for storing the
  // extensions in memory, excluding sizeof(*this).  if the extensionset is
  // for a lite message (and thus possibly contains lite messages), the results
  // are undefined (might work, might crash, might corrupt data, might not even
  // be linked in).  it's up to the protocol compiler to avoid calling this on
  // such extensionsets (easy enough since lite messages don't implement
  // spaceused()).
  int spaceusedexcludingself() const;

 private:

  // interface of a lazily parsed singular message extension.
  class libprotobuf_export lazymessageextension {
   public:
    lazymessageextension() {}
    virtual ~lazymessageextension() {}

    virtual lazymessageextension* new() const = 0;
    virtual const messagelite& getmessage(
        const messagelite& prototype) const = 0;
    virtual messagelite* mutablemessage(const messagelite& prototype) = 0;
    virtual void setallocatedmessage(messagelite *message) = 0;
    virtual messagelite* releasemessage(const messagelite& prototype) = 0;

    virtual bool isinitialized() const = 0;
    virtual int bytesize() const = 0;
    virtual int spaceused() const = 0;

    virtual void mergefrom(const lazymessageextension& other) = 0;
    virtual void clear() = 0;

    virtual bool readmessage(const messagelite& prototype,
                             io::codedinputstream* input) = 0;
    virtual void writemessage(int number,
                              io::codedoutputstream* output) const = 0;
    virtual uint8* writemessagetoarray(int number, uint8* target) const = 0;
   private:
    google_disallow_evil_constructors(lazymessageextension);
  };
  struct extension {
    // the order of these fields packs extension into 24 bytes when using 8
    // byte alignment. consider this when adding or removing fields here.
    union {
      int32                 int32_value;
      int64                 int64_value;
      uint32                uint32_value;
      uint64                uint64_value;
      float                 float_value;
      double                double_value;
      bool                  bool_value;
      int                   enum_value;
      string*               string_value;
      messagelite*          message_value;
      lazymessageextension* lazymessage_value;

      repeatedfield   <int32      >* repeated_int32_value;
      repeatedfield   <int64      >* repeated_int64_value;
      repeatedfield   <uint32     >* repeated_uint32_value;
      repeatedfield   <uint64     >* repeated_uint64_value;
      repeatedfield   <float      >* repeated_float_value;
      repeatedfield   <double     >* repeated_double_value;
      repeatedfield   <bool       >* repeated_bool_value;
      repeatedfield   <int        >* repeated_enum_value;
      repeatedptrfield<string     >* repeated_string_value;
      repeatedptrfield<messagelite>* repeated_message_value;
    };

    fieldtype type;
    bool is_repeated;

    // for singular types, indicates if the extension is "cleared".  this
    // happens when an extension is set and then later cleared by the caller.
    // we want to keep the extension object around for reuse, so instead of
    // removing it from the map, we just set is_cleared = true.  this has no
    // meaning for repeated types; for those, the size of the repeatedfield
    // simply becomes zero when cleared.
    bool is_cleared : 4;

    // for singular message types, indicates whether lazy parsing is enabled
    // for this extension. this field is only valid when type == type_message
    // and !is_repeated because we only support lazy parsing for singular
    // message types currently. if is_lazy = true, the extension is stored in
    // lazymessage_value. otherwise, the extension will be message_value.
    bool is_lazy : 4;

    // for repeated types, this indicates if the [packed=true] option is set.
    bool is_packed;

    // for packed fields, the size of the packed data is recorded here when
    // bytesize() is called then used during serialization.
    // todo(kenton):  use atomic<int> when c++ supports it.
    mutable int cached_size;

    // the descriptor for this extension, if one exists and is known.  may be
    // null.  must not be null if the descriptor for the extension does not
    // live in the same pool as the descriptor for the containing type.
    const fielddescriptor* descriptor;

    // some helper methods for operations on a single extension.
    void serializefieldwithcachedsizes(
        int number,
        io::codedoutputstream* output) const;
    uint8* serializefieldwithcachedsizestoarray(
        int number,
        uint8* target) const;
    void serializemessagesetitemwithcachedsizes(
        int number,
        io::codedoutputstream* output) const;
    uint8* serializemessagesetitemwithcachedsizestoarray(
        int number,
        uint8* target) const;
    int bytesize(int number) const;
    int messagesetitembytesize(int number) const;
    void clear();
    int getsize() const;
    void free();
    int spaceusedexcludingself() const;
  };


  // returns true and fills field_number and extension if extension is found.
  bool findextensioninfofromtag(uint32 tag, extensionfinder* extension_finder,
                                int* field_number, extensioninfo* extension);

  // parses a single extension from the input. the input should start out
  // positioned immediately after the wire tag. this method is called in
  // parsefield() after field number is extracted from the wire tag and
  // extensioninfo is found by the field number.
  bool parsefieldwithextensioninfo(int field_number,
                                   const extensioninfo& extension,
                                   io::codedinputstream* input,
                                   fieldskipper* field_skipper);

  // like parsefield(), but this method may parse singular message extensions
  // lazily depending on the value of flags_eagerly_parse_message_sets.
  bool parsefieldmaybelazily(uint32 tag, io::codedinputstream* input,
                             extensionfinder* extension_finder,
                             fieldskipper* field_skipper);

  // gets the extension with the given number, creating it if it does not
  // already exist.  returns true if the extension did not already exist.
  bool maybenewextension(int number, const fielddescriptor* descriptor,
                         extension** result);

  // parse a single messageset item -- called just after the item group start
  // tag has been read.
  bool parsemessagesetitem(io::codedinputstream* input,
                           extensionfinder* extension_finder,
                           fieldskipper* field_skipper);


  // hack:  repeatedptrfieldbase declares extensionset as a friend.  this
  //   friendship should automatically extend to extensionset::extension, but
  //   unfortunately some older compilers (e.g. gcc 3.4.4) do not implement this
  //   correctly.  so, we must provide helpers for calling methods of that
  //   class.

  // defined in extension_set_heavy.cc.
  static inline int repeatedmessage_spaceusedexcludingself(
      repeatedptrfieldbase* field);

  // the extension struct is small enough to be passed by value, so we use it
  // directly as the value type in the map rather than use pointers.  we use
  // a map rather than hash_map here because we expect most extensionsets will
  // only contain a small number of extensions whereas hash_map is optimized
  // for 100 elements or more.  also, we want appendtolist() to order fields
  // by field number.
  std::map<int, extension> extensions_;

  google_disallow_evil_constructors(extensionset);
};

// these are just for convenience...
inline void extensionset::setstring(int number, fieldtype type,
                                    const string& value,
                                    const fielddescriptor* descriptor) {
  mutablestring(number, type, descriptor)->assign(value);
}
inline void extensionset::setrepeatedstring(int number, int index,
                                            const string& value) {
  mutablerepeatedstring(number, index)->assign(value);
}
inline void extensionset::addstring(int number, fieldtype type,
                                    const string& value,
                                    const fielddescriptor* descriptor) {
  addstring(number, type, descriptor)->assign(value);
}

// ===================================================================
// glue for generated extension accessors

// -------------------------------------------------------------------
// template magic

// first we have a set of classes representing "type traits" for different
// field types.  a type traits class knows how to implement basic accessors
// for extensions of a particular type given an extensionset.  the signature
// for a type traits class looks like this:
//
//   class typetraits {
//    public:
//     typedef ? consttype;
//     typedef ? mutabletype;
//
//     static inline consttype get(int number, const extensionset& set);
//     static inline void set(int number, consttype value, extensionset* set);
//     static inline mutabletype mutable(int number, extensionset* set);
//
//     // variants for repeated fields.
//     static inline consttype get(int number, const extensionset& set,
//                                 int index);
//     static inline void set(int number, int index,
//                            consttype value, extensionset* set);
//     static inline mutabletype mutable(int number, int index,
//                                       extensionset* set);
//     static inline void add(int number, consttype value, extensionset* set);
//     static inline mutabletype add(int number, extensionset* set);
//   };
//
// not all of these methods make sense for all field types.  for example, the
// "mutable" methods only make sense for strings and messages, and the
// repeated methods only make sense for repeated types.  so, each type
// traits class implements only the set of methods from this signature that it
// actually supports.  this will cause a compiler error if the user tries to
// access an extension using a method that doesn't make sense for its type.
// for example, if "foo" is an extension of type "optional int32", then if you
// try to write code like:
//   my_message.mutableextension(foo)
// you will get a compile error because primitivetypetraits<int32> does not
// have a "mutable()" method.

// -------------------------------------------------------------------
// primitivetypetraits

// since the extensionset has different methods for each primitive type,
// we must explicitly define the methods of the type traits class for each
// known type.
template <typename type>
class primitivetypetraits {
 public:
  typedef type consttype;

  static inline consttype get(int number, const extensionset& set,
                              consttype default_value);
  static inline void set(int number, fieldtype field_type,
                         consttype value, extensionset* set);
};

template <typename type>
class repeatedprimitivetypetraits {
 public:
  typedef type consttype;

  static inline type get(int number, const extensionset& set, int index);
  static inline void set(int number, int index, type value, extensionset* set);
  static inline void add(int number, fieldtype field_type,
                         bool is_packed, type value, extensionset* set);
};

#define protobuf_define_primitive_type(type, method)                       \
template<> inline type primitivetypetraits<type>::get(                     \
    int number, const extensionset& set, type default_value) {             \
  return set.get##method(number, default_value);                           \
}                                                                          \
template<> inline void primitivetypetraits<type>::set(                     \
    int number, fieldtype field_type, type value, extensionset* set) {     \
  set->set##method(number, field_type, value, null);                       \
}                                                                          \
                                                                           \
template<> inline type repeatedprimitivetypetraits<type>::get(             \
    int number, const extensionset& set, int index) {                      \
  return set.getrepeated##method(number, index);                           \
}                                                                          \
template<> inline void repeatedprimitivetypetraits<type>::set(             \
    int number, int index, type value, extensionset* set) {                \
  set->setrepeated##method(number, index, value);                          \
}                                                                          \
template<> inline void repeatedprimitivetypetraits<type>::add(             \
    int number, fieldtype field_type, bool is_packed,                      \
    type value, extensionset* set) {                                       \
  set->add##method(number, field_type, is_packed, value, null);            \
}

protobuf_define_primitive_type( int32,  int32)
protobuf_define_primitive_type( int64,  int64)
protobuf_define_primitive_type(uint32, uint32)
protobuf_define_primitive_type(uint64, uint64)
protobuf_define_primitive_type( float,  float)
protobuf_define_primitive_type(double, double)
protobuf_define_primitive_type(  bool,   bool)

#undef protobuf_define_primitive_type

// -------------------------------------------------------------------
// stringtypetraits

// strings support both set() and mutable().
class libprotobuf_export stringtypetraits {
 public:
  typedef const string& consttype;
  typedef string* mutabletype;

  static inline const string& get(int number, const extensionset& set,
                                  consttype default_value) {
    return set.getstring(number, default_value);
  }
  static inline void set(int number, fieldtype field_type,
                         const string& value, extensionset* set) {
    set->setstring(number, field_type, value, null);
  }
  static inline string* mutable(int number, fieldtype field_type,
                                extensionset* set) {
    return set->mutablestring(number, field_type, null);
  }
};

class libprotobuf_export repeatedstringtypetraits {
 public:
  typedef const string& consttype;
  typedef string* mutabletype;

  static inline const string& get(int number, const extensionset& set,
                                  int index) {
    return set.getrepeatedstring(number, index);
  }
  static inline void set(int number, int index,
                         const string& value, extensionset* set) {
    set->setrepeatedstring(number, index, value);
  }
  static inline string* mutable(int number, int index, extensionset* set) {
    return set->mutablerepeatedstring(number, index);
  }
  static inline void add(int number, fieldtype field_type,
                         bool /*is_packed*/, const string& value,
                         extensionset* set) {
    set->addstring(number, field_type, value, null);
  }
  static inline string* add(int number, fieldtype field_type,
                            extensionset* set) {
    return set->addstring(number, field_type, null);
  }
};

// -------------------------------------------------------------------
// enumtypetraits

// extensionset represents enums using integers internally, so we have to
// static_cast around.
template <typename type, bool isvalid(int)>
class enumtypetraits {
 public:
  typedef type consttype;

  static inline consttype get(int number, const extensionset& set,
                              consttype default_value) {
    return static_cast<type>(set.getenum(number, default_value));
  }
  static inline void set(int number, fieldtype field_type,
                         consttype value, extensionset* set) {
    google_dcheck(isvalid(value));
    set->setenum(number, field_type, value, null);
  }
};

template <typename type, bool isvalid(int)>
class repeatedenumtypetraits {
 public:
  typedef type consttype;

  static inline consttype get(int number, const extensionset& set, int index) {
    return static_cast<type>(set.getrepeatedenum(number, index));
  }
  static inline void set(int number, int index,
                         consttype value, extensionset* set) {
    google_dcheck(isvalid(value));
    set->setrepeatedenum(number, index, value);
  }
  static inline void add(int number, fieldtype field_type,
                         bool is_packed, consttype value, extensionset* set) {
    google_dcheck(isvalid(value));
    set->addenum(number, field_type, is_packed, value, null);
  }
};

// -------------------------------------------------------------------
// messagetypetraits

// extensionset guarantees that when manipulating extensions with message
// types, the implementation used will be the compiled-in class representing
// that type.  so, we can static_cast down to the exact type we expect.
template <typename type>
class messagetypetraits {
 public:
  typedef const type& consttype;
  typedef type* mutabletype;

  static inline consttype get(int number, const extensionset& set,
                              consttype default_value) {
    return static_cast<const type&>(
        set.getmessage(number, default_value));
  }
  static inline mutabletype mutable(int number, fieldtype field_type,
                                    extensionset* set) {
    return static_cast<type*>(
      set->mutablemessage(number, field_type, type::default_instance(), null));
  }
  static inline void setallocated(int number, fieldtype field_type,
                                  mutabletype message, extensionset* set) {
    set->setallocatedmessage(number, field_type, null, message);
  }
  static inline mutabletype release(int number, fieldtype field_type,
                                    extensionset* set) {
    return static_cast<type*>(set->releasemessage(
        number, type::default_instance()));
  }
};

template <typename type>
class repeatedmessagetypetraits {
 public:
  typedef const type& consttype;
  typedef type* mutabletype;

  static inline consttype get(int number, const extensionset& set, int index) {
    return static_cast<const type&>(set.getrepeatedmessage(number, index));
  }
  static inline mutabletype mutable(int number, int index, extensionset* set) {
    return static_cast<type*>(set->mutablerepeatedmessage(number, index));
  }
  static inline mutabletype add(int number, fieldtype field_type,
                                extensionset* set) {
    return static_cast<type*>(
        set->addmessage(number, field_type, type::default_instance(), null));
  }
};

// -------------------------------------------------------------------
// extensionidentifier

// this is the type of actual extension objects.  e.g. if you have:
//   extends foo with optional int32 bar = 1234;
// then "bar" will be defined in c++ as:
//   extensionidentifier<foo, primitivetypetraits<int32>, 1, false> bar(1234);
//
// note that we could, in theory, supply the field number as a template
// parameter, and thus make an instance of extensionidentifier have no
// actual contents.  however, if we did that, then using at extension
// identifier would not necessarily cause the compiler to output any sort
// of reference to any simple defined in the extension's .pb.o file.  some
// linkers will actually drop object files that are not explicitly referenced,
// but that would be bad because it would cause this extension to not be
// registered at static initialization, and therefore using it would crash.

template <typename extendeetype, typename typetraitstype,
          fieldtype field_type, bool is_packed>
class extensionidentifier {
 public:
  typedef typetraitstype typetraits;
  typedef extendeetype extendee;

  extensionidentifier(int number, typename typetraits::consttype default_value)
      : number_(number), default_value_(default_value) {}
  inline int number() const { return number_; }
  typename typetraits::consttype default_value() const {
    return default_value_;
  }

 private:
  const int number_;
  typename typetraits::consttype default_value_;
};

// -------------------------------------------------------------------
// generated accessors

// this macro should be expanded in the context of a generated type which
// has extensions.
//
// we use "_proto_typetraits" as a type name below because "typetraits"
// causes problems if the class has a nested message or enum type with that
// name and "_typetraits" is technically reserved for the c++ library since
// it starts with an underscore followed by a capital letter.
//
// for similar reason, we use "_field_type" and "_is_packed" as parameter names
// below, so that "field_type" and "is_packed" can be used as field names.
#define google_protobuf_extension_accessors(classname)                        \
  /* has, size, clear */                                                      \
  template <typename _proto_typetraits,                                       \
            ::google::protobuf::internal::fieldtype _field_type,                        \
            bool _is_packed>                                                  \
  inline bool hasextension(                                                   \
      const ::google::protobuf::internal::extensionidentifier<                          \
        classname, _proto_typetraits, _field_type, _is_packed>& id) const {   \
    return _extensions_.has(id.number());                                     \
  }                                                                           \
                                                                              \
  template <typename _proto_typetraits,                                       \
            ::google::protobuf::internal::fieldtype _field_type,                        \
            bool _is_packed>                                                  \
  inline void clearextension(                                                 \
      const ::google::protobuf::internal::extensionidentifier<                          \
        classname, _proto_typetraits, _field_type, _is_packed>& id) {         \
    _extensions_.clearextension(id.number());                                 \
  }                                                                           \
                                                                              \
  template <typename _proto_typetraits,                                       \
            ::google::protobuf::internal::fieldtype _field_type,                        \
            bool _is_packed>                                                  \
  inline int extensionsize(                                                   \
      const ::google::protobuf::internal::extensionidentifier<                          \
        classname, _proto_typetraits, _field_type, _is_packed>& id) const {   \
    return _extensions_.extensionsize(id.number());                           \
  }                                                                           \
                                                                              \
  /* singular accessors */                                                    \
  template <typename _proto_typetraits,                                       \
            ::google::protobuf::internal::fieldtype _field_type,                        \
            bool _is_packed>                                                  \
  inline typename _proto_typetraits::consttype getextension(                  \
      const ::google::protobuf::internal::extensionidentifier<                          \
        classname, _proto_typetraits, _field_type, _is_packed>& id) const {   \
    return _proto_typetraits::get(id.number(), _extensions_,                  \
                                  id.default_value());                        \
  }                                                                           \
                                                                              \
  template <typename _proto_typetraits,                                       \
            ::google::protobuf::internal::fieldtype _field_type,                        \
            bool _is_packed>                                                  \
  inline typename _proto_typetraits::mutabletype mutableextension(            \
      const ::google::protobuf::internal::extensionidentifier<                          \
        classname, _proto_typetraits, _field_type, _is_packed>& id) {         \
    return _proto_typetraits::mutable(id.number(), _field_type,               \
                                      &_extensions_);                         \
  }                                                                           \
                                                                              \
  template <typename _proto_typetraits,                                       \
            ::google::protobuf::internal::fieldtype _field_type,                        \
            bool _is_packed>                                                  \
  inline void setextension(                                                   \
      const ::google::protobuf::internal::extensionidentifier<                          \
        classname, _proto_typetraits, _field_type, _is_packed>& id,           \
      typename _proto_typetraits::consttype value) {                          \
    _proto_typetraits::set(id.number(), _field_type, value, &_extensions_);   \
  }                                                                           \
                                                                              \
  template <typename _proto_typetraits,                                       \
            ::google::protobuf::internal::fieldtype _field_type,                        \
            bool _is_packed>                                                  \
  inline void setallocatedextension(                                          \
      const ::google::protobuf::internal::extensionidentifier<                          \
        classname, _proto_typetraits, _field_type, _is_packed>& id,           \
      typename _proto_typetraits::mutabletype value) {                        \
    _proto_typetraits::setallocated(id.number(), _field_type,                 \
                                    value, &_extensions_);                    \
  }                                                                           \
  template <typename _proto_typetraits,                                       \
            ::google::protobuf::internal::fieldtype _field_type,                        \
            bool _is_packed>                                                  \
  inline typename _proto_typetraits::mutabletype releaseextension(            \
      const ::google::protobuf::internal::extensionidentifier<                          \
        classname, _proto_typetraits, _field_type, _is_packed>& id) {         \
    return _proto_typetraits::release(id.number(), _field_type,               \
                                      &_extensions_);                         \
  }                                                                           \
                                                                              \
  /* repeated accessors */                                                    \
  template <typename _proto_typetraits,                                       \
            ::google::protobuf::internal::fieldtype _field_type,                        \
            bool _is_packed>                                                  \
  inline typename _proto_typetraits::consttype getextension(                  \
      const ::google::protobuf::internal::extensionidentifier<                          \
        classname, _proto_typetraits, _field_type, _is_packed>& id,           \
      int index) const {                                                      \
    return _proto_typetraits::get(id.number(), _extensions_, index);          \
  }                                                                           \
                                                                              \
  template <typename _proto_typetraits,                                       \
            ::google::protobuf::internal::fieldtype _field_type,                        \
            bool _is_packed>                                                  \
  inline typename _proto_typetraits::mutabletype mutableextension(            \
      const ::google::protobuf::internal::extensionidentifier<                          \
        classname, _proto_typetraits, _field_type, _is_packed>& id,           \
      int index) {                                                            \
    return _proto_typetraits::mutable(id.number(), index, &_extensions_);     \
  }                                                                           \
                                                                              \
  template <typename _proto_typetraits,                                       \
            ::google::protobuf::internal::fieldtype _field_type,                        \
            bool _is_packed>                                                  \
  inline void setextension(                                                   \
      const ::google::protobuf::internal::extensionidentifier<                          \
        classname, _proto_typetraits, _field_type, _is_packed>& id,           \
      int index, typename _proto_typetraits::consttype value) {               \
    _proto_typetraits::set(id.number(), index, value, &_extensions_);         \
  }                                                                           \
                                                                              \
  template <typename _proto_typetraits,                                       \
            ::google::protobuf::internal::fieldtype _field_type,                        \
            bool _is_packed>                                                  \
  inline typename _proto_typetraits::mutabletype addextension(                \
      const ::google::protobuf::internal::extensionidentifier<                          \
        classname, _proto_typetraits, _field_type, _is_packed>& id) {         \
    return _proto_typetraits::add(id.number(), _field_type, &_extensions_);   \
  }                                                                           \
                                                                              \
  template <typename _proto_typetraits,                                       \
            ::google::protobuf::internal::fieldtype _field_type,                        \
            bool _is_packed>                                                  \
  inline void addextension(                                                   \
      const ::google::protobuf::internal::extensionidentifier<                          \
        classname, _proto_typetraits, _field_type, _is_packed>& id,           \
      typename _proto_typetraits::consttype value) {                          \
    _proto_typetraits::add(id.number(), _field_type, _is_packed,              \
                           value, &_extensions_);                             \
  }

}  // namespace internal
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_extension_set_h__
