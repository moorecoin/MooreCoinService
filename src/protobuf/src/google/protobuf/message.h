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
// defines message, the abstract interface implemented by non-lite
// protocol message objects.  although it's possible to implement this
// interface manually, most users will use the protocol compiler to
// generate implementations.
//
// example usage:
//
// say you have a message defined as:
//
//   message foo {
//     optional string text = 1;
//     repeated int32 numbers = 2;
//   }
//
// then, if you used the protocol compiler to generate a class from the above
// definition, you could use it like so:
//
//   string data;  // will store a serialized version of the message.
//
//   {
//     // create a message and serialize it.
//     foo foo;
//     foo.set_text("hello world!");
//     foo.add_numbers(1);
//     foo.add_numbers(5);
//     foo.add_numbers(42);
//
//     foo.serializetostring(&data);
//   }
//
//   {
//     // parse the serialized message and check that it contains the
//     // correct data.
//     foo foo;
//     foo.parsefromstring(data);
//
//     assert(foo.text() == "hello world!");
//     assert(foo.numbers_size() == 3);
//     assert(foo.numbers(0) == 1);
//     assert(foo.numbers(1) == 5);
//     assert(foo.numbers(2) == 42);
//   }
//
//   {
//     // same as the last block, but do it dynamically via the message
//     // reflection interface.
//     message* foo = new foo;
//     const descriptor* descriptor = foo->getdescriptor();
//
//     // get the descriptors for the fields we're interested in and verify
//     // their types.
//     const fielddescriptor* text_field = descriptor->findfieldbyname("text");
//     assert(text_field != null);
//     assert(text_field->type() == fielddescriptor::type_string);
//     assert(text_field->label() == fielddescriptor::label_optional);
//     const fielddescriptor* numbers_field = descriptor->
//                                            findfieldbyname("numbers");
//     assert(numbers_field != null);
//     assert(numbers_field->type() == fielddescriptor::type_int32);
//     assert(numbers_field->label() == fielddescriptor::label_repeated);
//
//     // parse the message.
//     foo->parsefromstring(data);
//
//     // use the reflection interface to examine the contents.
//     const reflection* reflection = foo->getreflection();
//     assert(reflection->getstring(foo, text_field) == "hello world!");
//     assert(reflection->fieldsize(foo, numbers_field) == 3);
//     assert(reflection->getrepeatedint32(foo, numbers_field, 0) == 1);
//     assert(reflection->getrepeatedint32(foo, numbers_field, 1) == 5);
//     assert(reflection->getrepeatedint32(foo, numbers_field, 2) == 42);
//
//     delete foo;
//   }

#ifndef google_protobuf_message_h__
#define google_protobuf_message_h__

#include <vector>
#include <string>

#ifdef __deccxx
// hp c++'s iosfwd doesn't work.
#include <iostream>
#else
#include <iosfwd>
#endif

#include <google/protobuf/message_lite.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/descriptor.h>


namespace google {
namespace protobuf {

// defined in this file.
class message;
class reflection;
class messagefactory;

// defined in other files.
class unknownfieldset;         // unknown_field_set.h
namespace io {
  class zerocopyinputstream;   // zero_copy_stream.h
  class zerocopyoutputstream;  // zero_copy_stream.h
  class codedinputstream;      // coded_stream.h
  class codedoutputstream;     // coded_stream.h
}


template<typename t>
class repeatedfield;     // repeated_field.h

template<typename t>
class repeatedptrfield;  // repeated_field.h

// a container to hold message metadata.
struct metadata {
  const descriptor* descriptor;
  const reflection* reflection;
};

// abstract interface for protocol messages.
//
// see also messagelite, which contains most every-day operations.  message
// adds descriptors and reflection on top of that.
//
// the methods of this class that are virtual but not pure-virtual have
// default implementations based on reflection.  message classes which are
// optimized for speed will want to override these with faster implementations,
// but classes optimized for code size may be happy with keeping them.  see
// the optimize_for option in descriptor.proto.
class libprotobuf_export message : public messagelite {
 public:
  inline message() {}
  virtual ~message();

  // basic operations ------------------------------------------------

  // construct a new instance of the same type.  ownership is passed to the
  // caller.  (this is also defined in messagelite, but is defined again here
  // for return-type covariance.)
  virtual message* new() const = 0;

  // make this message into a copy of the given message.  the given message
  // must have the same descriptor, but need not necessarily be the same class.
  // by default this is just implemented as "clear(); mergefrom(from);".
  virtual void copyfrom(const message& from);

  // merge the fields from the given message into this message.  singular
  // fields will be overwritten, except for embedded messages which will
  // be merged.  repeated fields will be concatenated.  the given message
  // must be of the same type as this message (i.e. the exact same class).
  virtual void mergefrom(const message& from);

  // verifies that isinitialized() returns true.  google_check-fails otherwise, with
  // a nice error message.
  void checkinitialized() const;

  // slowly build a list of all required fields that are not set.
  // this is much, much slower than isinitialized() as it is implemented
  // purely via reflection.  generally, you should not call this unless you
  // have already determined that an error exists by calling isinitialized().
  void findinitializationerrors(vector<string>* errors) const;

  // like findinitializationerrors, but joins all the strings, delimited by
  // commas, and returns them.
  string initializationerrorstring() const;

  // clears all unknown fields from this message and all embedded messages.
  // normally, if unknown tag numbers are encountered when parsing a message,
  // the tag and value are stored in the message's unknownfieldset and
  // then written back out when the message is serialized.  this allows servers
  // which simply route messages to other servers to pass through messages
  // that have new field definitions which they don't yet know about.  however,
  // this behavior can have security implications.  to avoid it, call this
  // method after parsing.
  //
  // see reflection::getunknownfields() for more on unknown fields.
  virtual void discardunknownfields();

  // computes (an estimate of) the total number of bytes currently used for
  // storing the message in memory.  the default implementation calls the
  // reflection object's spaceused() method.
  virtual int spaceused() const;

  // debugging & testing----------------------------------------------

  // generates a human readable form of this message, useful for debugging
  // and other purposes.
  string debugstring() const;
  // like debugstring(), but with less whitespace.
  string shortdebugstring() const;
  // like debugstring(), but do not escape utf-8 byte sequences.
  string utf8debugstring() const;
  // convenience function useful in gdb.  prints debugstring() to stdout.
  void printdebugstring() const;

  // heavy i/o -------------------------------------------------------
  // additional parsing and serialization methods not implemented by
  // messagelite because they are not supported by the lite library.

  // parse a protocol buffer from a file descriptor.  if successful, the entire
  // input will be consumed.
  bool parsefromfiledescriptor(int file_descriptor);
  // like parsefromfiledescriptor(), but accepts messages that are missing
  // required fields.
  bool parsepartialfromfiledescriptor(int file_descriptor);
  // parse a protocol buffer from a c++ istream.  if successful, the entire
  // input will be consumed.
  bool parsefromistream(istream* input);
  // like parsefromistream(), but accepts messages that are missing
  // required fields.
  bool parsepartialfromistream(istream* input);

  // serialize the message and write it to the given file descriptor.  all
  // required fields must be set.
  bool serializetofiledescriptor(int file_descriptor) const;
  // like serializetofiledescriptor(), but allows missing required fields.
  bool serializepartialtofiledescriptor(int file_descriptor) const;
  // serialize the message and write it to the given c++ ostream.  all
  // required fields must be set.
  bool serializetoostream(ostream* output) const;
  // like serializetoostream(), but allows missing required fields.
  bool serializepartialtoostream(ostream* output) const;


  // reflection-based methods ----------------------------------------
  // these methods are pure-virtual in messagelite, but message provides
  // reflection-based default implementations.

  virtual string gettypename() const;
  virtual void clear();
  virtual bool isinitialized() const;
  virtual void checktypeandmergefrom(const messagelite& other);
  virtual bool mergepartialfromcodedstream(io::codedinputstream* input);
  virtual int bytesize() const;
  virtual void serializewithcachedsizes(io::codedoutputstream* output) const;

 private:
  // this is called only by the default implementation of bytesize(), to
  // update the cached size.  if you override bytesize(), you do not need
  // to override this.  if you do not override bytesize(), you must override
  // this; the default implementation will crash.
  //
  // the method is private because subclasses should never call it; only
  // override it.  yes, c++ lets you do that.  crazy, huh?
  virtual void setcachedsize(int size) const;

 public:

  // introspection ---------------------------------------------------

  // typedef for backwards-compatibility.
  typedef google::protobuf::reflection reflection;

  // get a descriptor for this message's type.  this describes what
  // fields the message contains, the types of those fields, etc.
  const descriptor* getdescriptor() const { return getmetadata().descriptor; }

  // get the reflection interface for this message, which can be used to
  // read and modify the fields of the message dynamically (in other words,
  // without knowing the message type at compile time).  this object remains
  // property of the message.
  //
  // this method remains virtual in case a subclass does not implement
  // reflection and wants to override the default behavior.
  virtual const reflection* getreflection() const {
    return getmetadata().reflection;
  }

 protected:
  // get a struct containing the metadata for the message. most subclasses only
  // need to implement this method, rather than the getdescriptor() and
  // getreflection() wrappers.
  virtual metadata getmetadata() const  = 0;


 private:
  google_disallow_evil_constructors(message);
};

// this interface contains methods that can be used to dynamically access
// and modify the fields of a protocol message.  their semantics are
// similar to the accessors the protocol compiler generates.
//
// to get the reflection for a given message, call message::getreflection().
//
// this interface is separate from message only for efficiency reasons;
// the vast majority of implementations of message will share the same
// implementation of reflection (generatedmessagereflection,
// defined in generated_message.h), and all messages of a particular class
// should share the same reflection object (though you should not rely on
// the latter fact).
//
// there are several ways that these methods can be used incorrectly.  for
// example, any of the following conditions will lead to undefined
// results (probably assertion failures):
// - the fielddescriptor is not a field of this message type.
// - the method called is not appropriate for the field's type.  for
//   each field type in fielddescriptor::type_*, there is only one
//   get*() method, one set*() method, and one add*() method that is
//   valid for that type.  it should be obvious which (except maybe
//   for type_bytes, which are represented using strings in c++).
// - a get*() or set*() method for singular fields is called on a repeated
//   field.
// - getrepeated*(), setrepeated*(), or add*() is called on a non-repeated
//   field.
// - the message object passed to any method is not of the right type for
//   this reflection object (i.e. message.getreflection() != reflection).
//
// you might wonder why there is not any abstract representation for a field
// of arbitrary type.  e.g., why isn't there just a "getfield()" method that
// returns "const field&", where "field" is some class with accessors like
// "getint32value()".  the problem is that someone would have to deal with
// allocating these field objects.  for generated message classes, having to
// allocate space for an additional object to wrap every field would at least
// double the message's memory footprint, probably worse.  allocating the
// objects on-demand, on the other hand, would be expensive and prone to
// memory leaks.  so, instead we ended up with this flat interface.
//
// todo(kenton):  create a utility class which callers can use to read and
//   write fields from a reflection without paying attention to the type.
class libprotobuf_export reflection {
 public:
  inline reflection() {}
  virtual ~reflection();

  // get the unknownfieldset for the message.  this contains fields which
  // were seen when the message was parsed but were not recognized according
  // to the message's definition.
  virtual const unknownfieldset& getunknownfields(
      const message& message) const = 0;
  // get a mutable pointer to the unknownfieldset for the message.  this
  // contains fields which were seen when the message was parsed but were not
  // recognized according to the message's definition.
  virtual unknownfieldset* mutableunknownfields(message* message) const = 0;

  // estimate the amount of memory used by the message object.
  virtual int spaceused(const message& message) const = 0;

  // check if the given non-repeated field is set.
  virtual bool hasfield(const message& message,
                        const fielddescriptor* field) const = 0;

  // get the number of elements of a repeated field.
  virtual int fieldsize(const message& message,
                        const fielddescriptor* field) const = 0;

  // clear the value of a field, so that hasfield() returns false or
  // fieldsize() returns zero.
  virtual void clearfield(message* message,
                          const fielddescriptor* field) const = 0;

  // removes the last element of a repeated field.
  // we don't provide a way to remove any element other than the last
  // because it invites inefficient use, such as o(n^2) filtering loops
  // that should have been o(n).  if you want to remove an element other
  // than the last, the best way to do it is to re-arrange the elements
  // (using swap()) so that the one you want removed is at the end, then
  // call removelast().
  virtual void removelast(message* message,
                          const fielddescriptor* field) const = 0;
  // removes the last element of a repeated message field, and returns the
  // pointer to the caller.  caller takes ownership of the returned pointer.
  virtual message* releaselast(message* message,
                               const fielddescriptor* field) const = 0;

  // swap the complete contents of two messages.
  virtual void swap(message* message1, message* message2) const = 0;

  // swap two elements of a repeated field.
  virtual void swapelements(message* message,
                    const fielddescriptor* field,
                    int index1,
                    int index2) const = 0;

  // list all fields of the message which are currently set.  this includes
  // extensions.  singular fields will only be listed if hasfield(field) would
  // return true and repeated fields will only be listed if fieldsize(field)
  // would return non-zero.  fields (both normal fields and extension fields)
  // will be listed ordered by field number.
  virtual void listfields(const message& message,
                          vector<const fielddescriptor*>* output) const = 0;

  // singular field getters ------------------------------------------
  // these get the value of a non-repeated field.  they return the default
  // value for fields that aren't set.

  virtual int32  getint32 (const message& message,
                           const fielddescriptor* field) const = 0;
  virtual int64  getint64 (const message& message,
                           const fielddescriptor* field) const = 0;
  virtual uint32 getuint32(const message& message,
                           const fielddescriptor* field) const = 0;
  virtual uint64 getuint64(const message& message,
                           const fielddescriptor* field) const = 0;
  virtual float  getfloat (const message& message,
                           const fielddescriptor* field) const = 0;
  virtual double getdouble(const message& message,
                           const fielddescriptor* field) const = 0;
  virtual bool   getbool  (const message& message,
                           const fielddescriptor* field) const = 0;
  virtual string getstring(const message& message,
                           const fielddescriptor* field) const = 0;
  virtual const enumvaluedescriptor* getenum(
      const message& message, const fielddescriptor* field) const = 0;
  // see mutablemessage() for the meaning of the "factory" parameter.
  virtual const message& getmessage(const message& message,
                                    const fielddescriptor* field,
                                    messagefactory* factory = null) const = 0;

  // get a string value without copying, if possible.
  //
  // getstring() necessarily returns a copy of the string.  this can be
  // inefficient when the string is already stored in a string object in the
  // underlying message.  getstringreference() will return a reference to the
  // underlying string in this case.  otherwise, it will copy the string into
  // *scratch and return that.
  //
  // note:  it is perfectly reasonable and useful to write code like:
  //     str = reflection->getstringreference(field, &str);
  //   this line would ensure that only one copy of the string is made
  //   regardless of the field's underlying representation.  when initializing
  //   a newly-constructed string, though, it's just as fast and more readable
  //   to use code like:
  //     string str = reflection->getstring(field);
  virtual const string& getstringreference(const message& message,
                                           const fielddescriptor* field,
                                           string* scratch) const = 0;


  // singular field mutators -----------------------------------------
  // these mutate the value of a non-repeated field.

  virtual void setint32 (message* message,
                         const fielddescriptor* field, int32  value) const = 0;
  virtual void setint64 (message* message,
                         const fielddescriptor* field, int64  value) const = 0;
  virtual void setuint32(message* message,
                         const fielddescriptor* field, uint32 value) const = 0;
  virtual void setuint64(message* message,
                         const fielddescriptor* field, uint64 value) const = 0;
  virtual void setfloat (message* message,
                         const fielddescriptor* field, float  value) const = 0;
  virtual void setdouble(message* message,
                         const fielddescriptor* field, double value) const = 0;
  virtual void setbool  (message* message,
                         const fielddescriptor* field, bool   value) const = 0;
  virtual void setstring(message* message,
                         const fielddescriptor* field,
                         const string& value) const = 0;
  virtual void setenum  (message* message,
                         const fielddescriptor* field,
                         const enumvaluedescriptor* value) const = 0;
  // get a mutable pointer to a field with a message type.  if a messagefactory
  // is provided, it will be used to construct instances of the sub-message;
  // otherwise, the default factory is used.  if the field is an extension that
  // does not live in the same pool as the containing message's descriptor (e.g.
  // it lives in an overlay pool), then a messagefactory must be provided.
  // if you have no idea what that meant, then you probably don't need to worry
  // about it (don't provide a messagefactory).  warning:  if the
  // fielddescriptor is for a compiled-in extension, then
  // factory->getprototype(field->message_type() must return an instance of the
  // compiled-in class for this type, not dynamicmessage.
  virtual message* mutablemessage(message* message,
                                  const fielddescriptor* field,
                                  messagefactory* factory = null) const = 0;
  // releases the message specified by 'field' and returns the pointer,
  // releasemessage() will return the message the message object if it exists.
  // otherwise, it may or may not return null.  in any case, if the return value
  // is non-null, the caller takes ownership of the pointer.
  // if the field existed (hasfield() is true), then the returned pointer will
  // be the same as the pointer returned by mutablemessage().
  // this function has the same effect as clearfield().
  virtual message* releasemessage(message* message,
                                  const fielddescriptor* field,
                                  messagefactory* factory = null) const = 0;


  // repeated field getters ------------------------------------------
  // these get the value of one element of a repeated field.

  virtual int32  getrepeatedint32 (const message& message,
                                   const fielddescriptor* field,
                                   int index) const = 0;
  virtual int64  getrepeatedint64 (const message& message,
                                   const fielddescriptor* field,
                                   int index) const = 0;
  virtual uint32 getrepeateduint32(const message& message,
                                   const fielddescriptor* field,
                                   int index) const = 0;
  virtual uint64 getrepeateduint64(const message& message,
                                   const fielddescriptor* field,
                                   int index) const = 0;
  virtual float  getrepeatedfloat (const message& message,
                                   const fielddescriptor* field,
                                   int index) const = 0;
  virtual double getrepeateddouble(const message& message,
                                   const fielddescriptor* field,
                                   int index) const = 0;
  virtual bool   getrepeatedbool  (const message& message,
                                   const fielddescriptor* field,
                                   int index) const = 0;
  virtual string getrepeatedstring(const message& message,
                                   const fielddescriptor* field,
                                   int index) const = 0;
  virtual const enumvaluedescriptor* getrepeatedenum(
      const message& message,
      const fielddescriptor* field, int index) const = 0;
  virtual const message& getrepeatedmessage(
      const message& message,
      const fielddescriptor* field, int index) const = 0;

  // see getstringreference(), above.
  virtual const string& getrepeatedstringreference(
      const message& message, const fielddescriptor* field,
      int index, string* scratch) const = 0;


  // repeated field mutators -----------------------------------------
  // these mutate the value of one element of a repeated field.

  virtual void setrepeatedint32 (message* message,
                                 const fielddescriptor* field,
                                 int index, int32  value) const = 0;
  virtual void setrepeatedint64 (message* message,
                                 const fielddescriptor* field,
                                 int index, int64  value) const = 0;
  virtual void setrepeateduint32(message* message,
                                 const fielddescriptor* field,
                                 int index, uint32 value) const = 0;
  virtual void setrepeateduint64(message* message,
                                 const fielddescriptor* field,
                                 int index, uint64 value) const = 0;
  virtual void setrepeatedfloat (message* message,
                                 const fielddescriptor* field,
                                 int index, float  value) const = 0;
  virtual void setrepeateddouble(message* message,
                                 const fielddescriptor* field,
                                 int index, double value) const = 0;
  virtual void setrepeatedbool  (message* message,
                                 const fielddescriptor* field,
                                 int index, bool   value) const = 0;
  virtual void setrepeatedstring(message* message,
                                 const fielddescriptor* field,
                                 int index, const string& value) const = 0;
  virtual void setrepeatedenum(message* message,
                               const fielddescriptor* field, int index,
                               const enumvaluedescriptor* value) const = 0;
  // get a mutable pointer to an element of a repeated field with a message
  // type.
  virtual message* mutablerepeatedmessage(
      message* message, const fielddescriptor* field, int index) const = 0;


  // repeated field adders -------------------------------------------
  // these add an element to a repeated field.

  virtual void addint32 (message* message,
                         const fielddescriptor* field, int32  value) const = 0;
  virtual void addint64 (message* message,
                         const fielddescriptor* field, int64  value) const = 0;
  virtual void adduint32(message* message,
                         const fielddescriptor* field, uint32 value) const = 0;
  virtual void adduint64(message* message,
                         const fielddescriptor* field, uint64 value) const = 0;
  virtual void addfloat (message* message,
                         const fielddescriptor* field, float  value) const = 0;
  virtual void adddouble(message* message,
                         const fielddescriptor* field, double value) const = 0;
  virtual void addbool  (message* message,
                         const fielddescriptor* field, bool   value) const = 0;
  virtual void addstring(message* message,
                         const fielddescriptor* field,
                         const string& value) const = 0;
  virtual void addenum  (message* message,
                         const fielddescriptor* field,
                         const enumvaluedescriptor* value) const = 0;
  // see mutablemessage() for comments on the "factory" parameter.
  virtual message* addmessage(message* message,
                              const fielddescriptor* field,
                              messagefactory* factory = null) const = 0;


  // repeated field accessors  -------------------------------------------------
  // the methods above, e.g. getrepeatedint32(msg, fd, index), provide singular
  // access to the data in a repeatedfield.  the methods below provide aggregate
  // access by exposing the repeatedfield object itself with the message.
  // applying these templates to inappropriate types will lead to an undefined
  // reference at link time (e.g. getrepeatedfield<***double>), or possibly a
  // template matching error at compile time (e.g. getrepeatedptrfield<file>).
  //
  // usage example: my_doubs = refl->getrepeatedfield<double>(msg, fd);

  // for t = cord and all protobuf scalar types except enums.
  template<typename t>
  const repeatedfield<t>& getrepeatedfield(
      const message&, const fielddescriptor*) const;

  // for t = cord and all protobuf scalar types except enums.
  template<typename t>
  repeatedfield<t>* mutablerepeatedfield(
      message*, const fielddescriptor*) const;

  // for t = string, google::protobuf::internal::stringpiecefield
  //         google::protobuf::message & descendants.
  template<typename t>
  const repeatedptrfield<t>& getrepeatedptrfield(
      const message&, const fielddescriptor*) const;

  // for t = string, google::protobuf::internal::stringpiecefield
  //         google::protobuf::message & descendants.
  template<typename t>
  repeatedptrfield<t>* mutablerepeatedptrfield(
      message*, const fielddescriptor*) const;

  // extensions ----------------------------------------------------------------

  // try to find an extension of this message type by fully-qualified field
  // name.  returns null if no extension is known for this name or number.
  virtual const fielddescriptor* findknownextensionbyname(
      const string& name) const = 0;

  // try to find an extension of this message type by field number.
  // returns null if no extension is known for this name or number.
  virtual const fielddescriptor* findknownextensionbynumber(
      int number) const = 0;

  // ---------------------------------------------------------------------------

 protected:
  // obtain a pointer to a repeated field structure and do some type checking:
  //   on field->cpp_type(),
  //   on field->field_option().ctype() (if ctype >= 0)
  //   of field->message_type() (if message_type != null).
  // we use 1 routine rather than 4 (const vs mutable) x (scalar vs pointer).
  virtual void* mutablerawrepeatedfield(
      message* message, const fielddescriptor* field, fielddescriptor::cpptype,
      int ctype, const descriptor* message_type) const = 0;

 private:
  // special version for specialized implementations of string.  we can't call
  // mutablerawrepeatedfield directly here because we don't have access to
  // fieldoptions::* which are defined in descriptor.pb.h.  including that
  // file here is not possible because it would cause a circular include cycle.
  void* mutablerawrepeatedstring(
      message* message, const fielddescriptor* field, bool is_string) const;

  google_disallow_evil_constructors(reflection);
};

// abstract interface for a factory for message objects.
class libprotobuf_export messagefactory {
 public:
  inline messagefactory() {}
  virtual ~messagefactory();

  // given a descriptor, gets or constructs the default (prototype) message
  // of that type.  you can then call that message's new() method to construct
  // a mutable message of that type.
  //
  // calling this method twice with the same descriptor returns the same
  // object.  the returned object remains property of the factory.  also, any
  // objects created by calling the prototype's new() method share some data
  // with the prototype, so these must be destoyed before the messagefactory
  // is destroyed.
  //
  // the given descriptor must outlive the returned message, and hence must
  // outlive the messagefactory.
  //
  // some implementations do not support all types.  getprototype() will
  // return null if the descriptor passed in is not supported.
  //
  // this method may or may not be thread-safe depending on the implementation.
  // each implementation should document its own degree thread-safety.
  virtual const message* getprototype(const descriptor* type) = 0;

  // gets a messagefactory which supports all generated, compiled-in messages.
  // in other words, for any compiled-in type foomessage, the following is true:
  //   messagefactory::generated_factory()->getprototype(
  //     foomessage::descriptor()) == foomessage::default_instance()
  // this factory supports all types which are found in
  // descriptorpool::generated_pool().  if given a descriptor from any other
  // pool, getprototype() will return null.  (you can also check if a
  // descriptor is for a generated message by checking if
  // descriptor->file()->pool() == descriptorpool::generated_pool().)
  //
  // this factory is 100% thread-safe; calling getprototype() does not modify
  // any shared data.
  //
  // this factory is a singleton.  the caller must not delete the object.
  static messagefactory* generated_factory();

  // for internal use only:  registers a .proto file at static initialization
  // time, to be placed in generated_factory.  the first time getprototype()
  // is called with a descriptor from this file, |register_messages| will be
  // called, with the file name as the parameter.  it must call
  // internalregistergeneratedmessage() (below) to register each message type
  // in the file.  this strange mechanism is necessary because descriptors are
  // built lazily, so we can't register types by their descriptor until we
  // know that the descriptor exists.  |filename| must be a permanent string.
  static void internalregistergeneratedfile(
      const char* filename, void (*register_messages)(const string&));

  // for internal use only:  registers a message type.  called only by the
  // functions which are registered with internalregistergeneratedfile(),
  // above.
  static void internalregistergeneratedmessage(const descriptor* descriptor,
                                               const message* prototype);


 private:
  google_disallow_evil_constructors(messagefactory);
};

#define declare_get_repeated_field(type)                         \
template<>                                                       \
libprotobuf_export                                               \
const repeatedfield<type>& reflection::getrepeatedfield<type>(   \
    const message& message, const fielddescriptor* field) const; \
                                                                 \
template<>                                                       \
libprotobuf_export                                               \
repeatedfield<type>* reflection::mutablerepeatedfield<type>(     \
    message* message, const fielddescriptor* field) const;

declare_get_repeated_field(int32)
declare_get_repeated_field(int64)
declare_get_repeated_field(uint32)
declare_get_repeated_field(uint64)
declare_get_repeated_field(float)
declare_get_repeated_field(double)
declare_get_repeated_field(bool)

#undef declare_get_repeated_field

// =============================================================================
// implementation details for {get,mutable}rawrepeatedptrfield.  we provide
// specializations for <string>, <stringpiecefield> and <message> and handle
// everything else with the default template which will match any type having
// a method with signature "static const google::protobuf::descriptor* descriptor()".
// such a type presumably is a descendant of google::protobuf::message.

template<>
inline const repeatedptrfield<string>& reflection::getrepeatedptrfield<string>(
    const message& message, const fielddescriptor* field) const {
  return *static_cast<repeatedptrfield<string>* >(
      mutablerawrepeatedstring(const_cast<message*>(&message), field, true));
}

template<>
inline repeatedptrfield<string>* reflection::mutablerepeatedptrfield<string>(
    message* message, const fielddescriptor* field) const {
  return static_cast<repeatedptrfield<string>* >(
      mutablerawrepeatedstring(message, field, true));
}


// -----

template<>
inline const repeatedptrfield<message>& reflection::getrepeatedptrfield(
    const message& message, const fielddescriptor* field) const {
  return *static_cast<repeatedptrfield<message>* >(
      mutablerawrepeatedfield(const_cast<message*>(&message), field,
          fielddescriptor::cpptype_message, -1,
          null));
}

template<>
inline repeatedptrfield<message>* reflection::mutablerepeatedptrfield(
    message* message, const fielddescriptor* field) const {
  return static_cast<repeatedptrfield<message>* >(
      mutablerawrepeatedfield(message, field,
          fielddescriptor::cpptype_message, -1,
          null));
}

template<typename pb>
inline const repeatedptrfield<pb>& reflection::getrepeatedptrfield(
    const message& message, const fielddescriptor* field) const {
  return *static_cast<repeatedptrfield<pb>* >(
      mutablerawrepeatedfield(const_cast<message*>(&message), field,
          fielddescriptor::cpptype_message, -1,
          pb::default_instance().getdescriptor()));
}

template<typename pb>
inline repeatedptrfield<pb>* reflection::mutablerepeatedptrfield(
    message* message, const fielddescriptor* field) const {
  return static_cast<repeatedptrfield<pb>* >(
      mutablerawrepeatedfield(message, field,
          fielddescriptor::cpptype_message, -1,
          pb::default_instance().getdescriptor()));
}

}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_message_h__
