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
//
// defines messagelite, the abstract interface implemented by all (lite
// and non-lite) protocol message objects.

#ifndef google_protobuf_message_lite_h__
#define google_protobuf_message_lite_h__

#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

namespace io {
  class codedinputstream;
  class codedoutputstream;
  class zerocopyinputstream;
  class zerocopyoutputstream;
}

// interface to light weight protocol messages.
//
// this interface is implemented by all protocol message objects.  non-lite
// messages additionally implement the message interface, which is a
// subclass of messagelite.  use messagelite instead when you only need
// the subset of features which it supports -- namely, nothing that uses
// descriptors or reflection.  you can instruct the protocol compiler
// to generate classes which implement only messagelite, not the full
// message interface, by adding the following line to the .proto file:
//
//   option optimize_for = lite_runtime;
//
// this is particularly useful on resource-constrained systems where
// the full protocol buffers runtime library is too big.
//
// note that on non-constrained systems (e.g. servers) when you need
// to link in lots of protocol definitions, a better way to reduce
// total code footprint is to use optimize_for = code_size.  this
// will make the generated code smaller while still supporting all the
// same features (at the expense of speed).  optimize_for = lite_runtime
// is best when you only have a small number of message types linked
// into your binary, in which case the size of the protocol buffers
// runtime itself is the biggest problem.
class libprotobuf_export messagelite {
 public:
  inline messagelite() {}
  virtual ~messagelite();

  // basic operations ------------------------------------------------

  // get the name of this message type, e.g. "foo.bar.bazproto".
  virtual string gettypename() const = 0;

  // construct a new instance of the same type.  ownership is passed to the
  // caller.
  virtual messagelite* new() const = 0;

  // clear all fields of the message and set them to their default values.
  // clear() avoids freeing memory, assuming that any memory allocated
  // to hold parts of the message will be needed again to hold the next
  // message.  if you actually want to free the memory used by a message,
  // you must delete it.
  virtual void clear() = 0;

  // quickly check if all required fields have values set.
  virtual bool isinitialized() const = 0;

  // this is not implemented for lite messages -- it just returns "(cannot
  // determine missing fields for lite message)".  however, it is implemented
  // for full messages.  see message.h.
  virtual string initializationerrorstring() const;

  // if |other| is the exact same class as this, calls mergefrom().  otherwise,
  // results are undefined (probably crash).
  virtual void checktypeandmergefrom(const messagelite& other) = 0;

  // parsing ---------------------------------------------------------
  // methods for parsing in protocol buffer format.  most of these are
  // just simple wrappers around mergefromcodedstream().

  // fill the message with a protocol buffer parsed from the given input
  // stream.  returns false on a read error or if the input is in the
  // wrong format.
  bool parsefromcodedstream(io::codedinputstream* input);
  // like parsefromcodedstream(), but accepts messages that are missing
  // required fields.
  bool parsepartialfromcodedstream(io::codedinputstream* input);
  // read a protocol buffer from the given zero-copy input stream.  if
  // successful, the entire input will be consumed.
  bool parsefromzerocopystream(io::zerocopyinputstream* input);
  // like parsefromzerocopystream(), but accepts messages that are missing
  // required fields.
  bool parsepartialfromzerocopystream(io::zerocopyinputstream* input);
  // read a protocol buffer from the given zero-copy input stream, expecting
  // the message to be exactly "size" bytes long.  if successful, exactly
  // this many bytes will have been consumed from the input.
  bool parsefromboundedzerocopystream(io::zerocopyinputstream* input, int size);
  // like parsefromboundedzerocopystream(), but accepts messages that are
  // missing required fields.
  bool parsepartialfromboundedzerocopystream(io::zerocopyinputstream* input,
                                             int size);
  // parse a protocol buffer contained in a string.
  bool parsefromstring(const string& data);
  // like parsefromstring(), but accepts messages that are missing
  // required fields.
  bool parsepartialfromstring(const string& data);
  // parse a protocol buffer contained in an array of bytes.
  bool parsefromarray(const void* data, int size);
  // like parsefromarray(), but accepts messages that are missing
  // required fields.
  bool parsepartialfromarray(const void* data, int size);


  // reads a protocol buffer from the stream and merges it into this
  // message.  singular fields read from the input overwrite what is
  // already in the message and repeated fields are appended to those
  // already present.
  //
  // it is the responsibility of the caller to call input->lasttagwas()
  // (for groups) or input->consumedentiremessage() (for non-groups) after
  // this returns to verify that the message's end was delimited correctly.
  //
  // parsefromcodedstream() is implemented as clear() followed by
  // mergefromcodedstream().
  bool mergefromcodedstream(io::codedinputstream* input);

  // like mergefromcodedstream(), but succeeds even if required fields are
  // missing in the input.
  //
  // mergefromcodedstream() is just implemented as mergepartialfromcodedstream()
  // followed by isinitialized().
  virtual bool mergepartialfromcodedstream(io::codedinputstream* input) = 0;


  // serialization ---------------------------------------------------
  // methods for serializing in protocol buffer format.  most of these
  // are just simple wrappers around bytesize() and serializewithcachedsizes().

  // write a protocol buffer of this message to the given output.  returns
  // false on a write error.  if the message is missing required fields,
  // this may google_check-fail.
  bool serializetocodedstream(io::codedoutputstream* output) const;
  // like serializetocodedstream(), but allows missing required fields.
  bool serializepartialtocodedstream(io::codedoutputstream* output) const;
  // write the message to the given zero-copy output stream.  all required
  // fields must be set.
  bool serializetozerocopystream(io::zerocopyoutputstream* output) const;
  // like serializetozerocopystream(), but allows missing required fields.
  bool serializepartialtozerocopystream(io::zerocopyoutputstream* output) const;
  // serialize the message and store it in the given string.  all required
  // fields must be set.
  bool serializetostring(string* output) const;
  // like serializetostring(), but allows missing required fields.
  bool serializepartialtostring(string* output) const;
  // serialize the message and store it in the given byte array.  all required
  // fields must be set.
  bool serializetoarray(void* data, int size) const;
  // like serializetoarray(), but allows missing required fields.
  bool serializepartialtoarray(void* data, int size) const;

  // make a string encoding the message. is equivalent to calling
  // serializetostring() on a string and using that.  returns the empty
  // string if serializetostring() would have returned an error.
  // note: if you intend to generate many such strings, you may
  // reduce heap fragmentation by instead re-using the same string
  // object with calls to serializetostring().
  string serializeasstring() const;
  // like serializeasstring(), but allows missing required fields.
  string serializepartialasstring() const;

  // like serializetostring(), but appends to the data to the string's existing
  // contents.  all required fields must be set.
  bool appendtostring(string* output) const;
  // like appendtostring(), but allows missing required fields.
  bool appendpartialtostring(string* output) const;

  // computes the serialized size of the message.  this recursively calls
  // bytesize() on all embedded messages.  if a subclass does not override
  // this, it must override setcachedsize().
  virtual int bytesize() const = 0;

  // serializes the message without recomputing the size.  the message must
  // not have changed since the last call to bytesize(); if it has, the results
  // are undefined.
  virtual void serializewithcachedsizes(
      io::codedoutputstream* output) const = 0;

  // like serializewithcachedsizes, but writes directly to *target, returning
  // a pointer to the byte immediately after the last byte written.  "target"
  // must point at a byte array of at least bytesize() bytes.
  virtual uint8* serializewithcachedsizestoarray(uint8* target) const;

  // returns the result of the last call to bytesize().  an embedded message's
  // size is needed both to serialize it (because embedded messages are
  // length-delimited) and to compute the outer message's size.  caching
  // the size avoids computing it multiple times.
  //
  // bytesize() does not automatically use the cached size when available
  // because this would require invalidating it every time the message was
  // modified, which would be too hard and expensive.  (e.g. if a deeply-nested
  // sub-message is changed, all of its parents' cached sizes would need to be
  // invalidated, which is too much work for an otherwise inlined setter
  // method.)
  virtual int getcachedsize() const = 0;

 private:
  google_disallow_evil_constructors(messagelite);
};

}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_message_lite_h__
