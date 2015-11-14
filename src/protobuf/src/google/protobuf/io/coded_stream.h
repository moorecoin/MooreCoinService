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
// this file contains the codedinputstream and codedoutputstream classes,
// which wrap a zerocopyinputstream or zerocopyoutputstream, respectively,
// and allow you to read or write individual pieces of data in various
// formats.  in particular, these implement the varint encoding for
// integers, a simple variable-length encoding in which smaller numbers
// take fewer bytes.
//
// typically these classes will only be used internally by the protocol
// buffer library in order to encode and decode protocol buffers.  clients
// of the library only need to know about this class if they wish to write
// custom message parsing or serialization procedures.
//
// codedoutputstream example:
//   // write some data to "myfile".  first we write a 4-byte "magic number"
//   // to identify the file type, then write a length-delimited string.  the
//   // string is composed of a varint giving the length followed by the raw
//   // bytes.
//   int fd = open("myfile", o_wronly);
//   zerocopyoutputstream* raw_output = new fileoutputstream(fd);
//   codedoutputstream* coded_output = new codedoutputstream(raw_output);
//
//   int magic_number = 1234;
//   char text[] = "hello world!";
//   coded_output->writelittleendian32(magic_number);
//   coded_output->writevarint32(strlen(text));
//   coded_output->writeraw(text, strlen(text));
//
//   delete coded_output;
//   delete raw_output;
//   close(fd);
//
// codedinputstream example:
//   // read a file created by the above code.
//   int fd = open("myfile", o_rdonly);
//   zerocopyinputstream* raw_input = new fileinputstream(fd);
//   codedinputstream coded_input = new codedinputstream(raw_input);
//
//   coded_input->readlittleendian32(&magic_number);
//   if (magic_number != 1234) {
//     cerr << "file not in expected format." << endl;
//     return;
//   }
//
//   uint32 size;
//   coded_input->readvarint32(&size);
//
//   char* text = new char[size + 1];
//   coded_input->readraw(buffer, size);
//   text[size] = '\0';
//
//   delete coded_input;
//   delete raw_input;
//   close(fd);
//
//   cout << "text is: " << text << endl;
//   delete [] text;
//
// for those who are interested, varint encoding is defined as follows:
//
// the encoding operates on unsigned integers of up to 64 bits in length.
// each byte of the encoded value has the format:
// * bits 0-6: seven bits of the number being encoded.
// * bit 7: zero if this is the last byte in the encoding (in which
//   case all remaining bits of the number are zero) or 1 if
//   more bytes follow.
// the first byte contains the least-significant 7 bits of the number, the
// second byte (if present) contains the next-least-significant 7 bits,
// and so on.  so, the binary number 1011000101011 would be encoded in two
// bytes as "10101011 00101100".
//
// in theory, varint could be used to encode integers of any length.
// however, for practicality we set a limit at 64 bits.  the maximum encoded
// length of a number is thus 10 bytes.

#ifndef google_protobuf_io_coded_stream_h__
#define google_protobuf_io_coded_stream_h__

#include <string>
#ifdef _msc_ver
  #if defined(_m_ix86) && \
      !defined(protobuf_disable_little_endian_opt_for_test)
    #define protobuf_little_endian 1
  #endif
  #if _msc_ver >= 1300
    // if msvc has "/rtcc" set, it will complain about truncating casts at
    // runtime.  this file contains some intentional truncating casts.
    #pragma runtime_checks("c", off)
  #endif
#else
  #include <sys/param.h>   // __byte_order
  #if defined(__byte_order) && __byte_order == __little_endian && \
      !defined(protobuf_disable_little_endian_opt_for_test)
    #define protobuf_little_endian 1
  #endif
#endif
#include <google/protobuf/stubs/common.h>


namespace google {
namespace protobuf {

class descriptorpool;
class messagefactory;

namespace io {

// defined in this file.
class codedinputstream;
class codedoutputstream;

// defined in other files.
class zerocopyinputstream;           // zero_copy_stream.h
class zerocopyoutputstream;          // zero_copy_stream.h

// class which reads and decodes binary data which is composed of varint-
// encoded integers and fixed-width pieces.  wraps a zerocopyinputstream.
// most users will not need to deal with codedinputstream.
//
// most methods of codedinputstream that return a bool return false if an
// underlying i/o error occurs or if the data is malformed.  once such a
// failure occurs, the codedinputstream is broken and is no longer useful.
class libprotobuf_export codedinputstream {
 public:
  // create a codedinputstream that reads from the given zerocopyinputstream.
  explicit codedinputstream(zerocopyinputstream* input);

  // create a codedinputstream that reads from the given flat array.  this is
  // faster than using an arrayinputstream.  pushlimit(size) is implied by
  // this constructor.
  explicit codedinputstream(const uint8* buffer, int size);

  // destroy the codedinputstream and position the underlying
  // zerocopyinputstream at the first unread byte.  if an error occurred while
  // reading (causing a method to return false), then the exact position of
  // the input stream may be anywhere between the last value that was read
  // successfully and the stream's byte limit.
  ~codedinputstream();

  // return true if this codedinputstream reads from a flat array instead of
  // a zerocopyinputstream.
  inline bool isflat() const;

  // skips a number of bytes.  returns false if an underlying read error
  // occurs.
  bool skip(int count);

  // sets *data to point directly at the unread part of the codedinputstream's
  // underlying buffer, and *size to the size of that buffer, but does not
  // advance the stream's current position.  this will always either produce
  // a non-empty buffer or return false.  if the caller consumes any of
  // this data, it should then call skip() to skip over the consumed bytes.
  // this may be useful for implementing external fast parsing routines for
  // types of data not covered by the codedinputstream interface.
  bool getdirectbufferpointer(const void** data, int* size);

  // like getdirectbufferpointer, but this method is inlined, and does not
  // attempt to refresh() if the buffer is currently empty.
  inline void getdirectbufferpointerinline(const void** data,
                                           int* size) google_attribute_always_inline;

  // read raw bytes, copying them into the given buffer.
  bool readraw(void* buffer, int size);

  // like readraw, but reads into a string.
  //
  // implementation note:  readstring() grows the string gradually as it
  // reads in the data, rather than allocating the entire requested size
  // upfront.  this prevents denial-of-service attacks in which a client
  // could claim that a string is going to be max_int bytes long in order to
  // crash the server because it can't allocate this much space at once.
  bool readstring(string* buffer, int size);
  // like the above, with inlined optimizations. this should only be used
  // by the protobuf implementation.
  inline bool internalreadstringinline(string* buffer,
                                       int size) google_attribute_always_inline;


  // read a 32-bit little-endian integer.
  bool readlittleendian32(uint32* value);
  // read a 64-bit little-endian integer.
  bool readlittleendian64(uint64* value);

  // these methods read from an externally provided buffer. the caller is
  // responsible for ensuring that the buffer has sufficient space.
  // read a 32-bit little-endian integer.
  static const uint8* readlittleendian32fromarray(const uint8* buffer,
                                                   uint32* value);
  // read a 64-bit little-endian integer.
  static const uint8* readlittleendian64fromarray(const uint8* buffer,
                                                   uint64* value);

  // read an unsigned integer with varint encoding, truncating to 32 bits.
  // reading a 32-bit value is equivalent to reading a 64-bit one and casting
  // it to uint32, but may be more efficient.
  bool readvarint32(uint32* value);
  // read an unsigned integer with varint encoding.
  bool readvarint64(uint64* value);

  // read a tag.  this calls readvarint32() and returns the result, or returns
  // zero (which is not a valid tag) if readvarint32() fails.  also, it updates
  // the last tag value, which can be checked with lasttagwas().
  // always inline because this is only called in once place per parse loop
  // but it is called for every iteration of said loop, so it should be fast.
  // gcc doesn't want to inline this by default.
  uint32 readtag() google_attribute_always_inline;

  // usually returns true if calling readvarint32() now would produce the given
  // value.  will always return false if readvarint32() would not return the
  // given value.  if expecttag() returns true, it also advances past
  // the varint.  for best performance, use a compile-time constant as the
  // parameter.
  // always inline because this collapses to a small number of instructions
  // when given a constant parameter, but gcc doesn't want to inline by default.
  bool expecttag(uint32 expected) google_attribute_always_inline;

  // like above, except this reads from the specified buffer. the caller is
  // responsible for ensuring that the buffer is large enough to read a varint
  // of the expected size. for best performance, use a compile-time constant as
  // the expected tag parameter.
  //
  // returns a pointer beyond the expected tag if it was found, or null if it
  // was not.
  static const uint8* expecttagfromarray(
      const uint8* buffer,
      uint32 expected) google_attribute_always_inline;

  // usually returns true if no more bytes can be read.  always returns false
  // if more bytes can be read.  if expectatend() returns true, a subsequent
  // call to lasttagwas() will act as if readtag() had been called and returned
  // zero, and consumedentiremessage() will return true.
  bool expectatend();

  // if the last call to readtag() returned the given value, returns true.
  // otherwise, returns false;
  //
  // this is needed because parsers for some types of embedded messages
  // (with field type type_group) don't actually know that they've reached the
  // end of a message until they see an endgroup tag, which was actually part
  // of the enclosing message.  the enclosing message would like to check that
  // tag to make sure it had the right number, so it calls lasttagwas() on
  // return from the embedded parser to check.
  bool lasttagwas(uint32 expected);

  // when parsing message (but not a group), this method must be called
  // immediately after mergefromcodedstream() returns (if it returns true)
  // to further verify that the message ended in a legitimate way.  for
  // example, this verifies that parsing did not end on an end-group tag.
  // it also checks for some cases where, due to optimizations,
  // mergefromcodedstream() can incorrectly return true.
  bool consumedentiremessage();

  // limits ----------------------------------------------------------
  // limits are used when parsing length-delimited embedded messages.
  // after the message's length is read, pushlimit() is used to prevent
  // the codedinputstream from reading beyond that length.  once the
  // embedded message has been parsed, poplimit() is called to undo the
  // limit.

  // opaque type used with pushlimit() and poplimit().  do not modify
  // values of this type yourself.  the only reason that this isn't a
  // struct with private internals is for efficiency.
  typedef int limit;

  // places a limit on the number of bytes that the stream may read,
  // starting from the current position.  once the stream hits this limit,
  // it will act like the end of the input has been reached until poplimit()
  // is called.
  //
  // as the names imply, the stream conceptually has a stack of limits.  the
  // shortest limit on the stack is always enforced, even if it is not the
  // top limit.
  //
  // the value returned by pushlimit() is opaque to the caller, and must
  // be passed unchanged to the corresponding call to poplimit().
  limit pushlimit(int byte_limit);

  // pops the last limit pushed by pushlimit().  the input must be the value
  // returned by that call to pushlimit().
  void poplimit(limit limit);

  // returns the number of bytes left until the nearest limit on the
  // stack is hit, or -1 if no limits are in place.
  int bytesuntillimit() const;

  // returns current position relative to the beginning of the input stream.
  int currentposition() const;

  // total bytes limit -----------------------------------------------
  // to prevent malicious users from sending excessively large messages
  // and causing integer overflows or memory exhaustion, codedinputstream
  // imposes a hard limit on the total number of bytes it will read.

  // sets the maximum number of bytes that this codedinputstream will read
  // before refusing to continue.  to prevent integer overflows in the
  // protocol buffers implementation, as well as to prevent servers from
  // allocating enormous amounts of memory to hold parsed messages, the
  // maximum message length should be limited to the shortest length that
  // will not harm usability.  the theoretical shortest message that could
  // cause integer overflows is 512mb.  the default limit is 64mb.  apps
  // should set shorter limits if possible.  if warning_threshold is not -1,
  // a warning will be printed to stderr after warning_threshold bytes are
  // read.  for backwards compatibility all negative values get squached to -1,
  // as other negative values might have special internal meanings.
  // an error will always be printed to stderr if the limit is reached.
  //
  // this is unrelated to pushlimit()/poplimit().
  //
  // hint:  if you are reading this because your program is printing a
  //   warning about dangerously large protocol messages, you may be
  //   confused about what to do next.  the best option is to change your
  //   design such that excessively large messages are not necessary.
  //   for example, try to design file formats to consist of many small
  //   messages rather than a single large one.  if this is infeasible,
  //   you will need to increase the limit.  chances are, though, that
  //   your code never constructs a codedinputstream on which the limit
  //   can be set.  you probably parse messages by calling things like
  //   message::parsefromstring().  in this case, you will need to change
  //   your code to instead construct some sort of zerocopyinputstream
  //   (e.g. an arrayinputstream), construct a codedinputstream around
  //   that, then call message::parsefromcodedstream() instead.  then
  //   you can adjust the limit.  yes, it's more work, but you're doing
  //   something unusual.
  void settotalbyteslimit(int total_bytes_limit, int warning_threshold);

  // recursion limit -------------------------------------------------
  // to prevent corrupt or malicious messages from causing stack overflows,
  // we must keep track of the depth of recursion when parsing embedded
  // messages and groups.  codedinputstream keeps track of this because it
  // is the only object that is passed down the stack during parsing.

  // sets the maximum recursion depth.  the default is 100.
  void setrecursionlimit(int limit);


  // increments the current recursion depth.  returns true if the depth is
  // under the limit, false if it has gone over.
  bool incrementrecursiondepth();

  // decrements the recursion depth.
  void decrementrecursiondepth();

  // extension registry ----------------------------------------------
  // advanced usage:  99.9% of people can ignore this section.
  //
  // by default, when parsing extensions, the parser looks for extension
  // definitions in the pool which owns the outer message's descriptor.
  // however, you may call setextensionregistry() to provide an alternative
  // pool instead.  this makes it possible, for example, to parse a message
  // using a generated class, but represent some extensions using
  // dynamicmessage.

  // set the pool used to look up extensions.  most users do not need to call
  // this as the correct pool will be chosen automatically.
  //
  // warning:  it is very easy to misuse this.  carefully read the requirements
  //   below.  do not use this unless you are sure you need it.  almost no one
  //   does.
  //
  // let's say you are parsing a message into message object m, and you want
  // to take advantage of setextensionregistry().  you must follow these
  // requirements:
  //
  // the given descriptorpool must contain m->getdescriptor().  it is not
  // sufficient for it to simply contain a descriptor that has the same name
  // and content -- it must be the *exact object*.  in other words:
  //   assert(pool->findmessagetypebyname(m->getdescriptor()->full_name()) ==
  //          m->getdescriptor());
  // there are two ways to satisfy this requirement:
  // 1) use m->getdescriptor()->pool() as the pool.  this is generally useless
  //    because this is the pool that would be used anyway if you didn't call
  //    setextensionregistry() at all.
  // 2) use a descriptorpool which has m->getdescriptor()->pool() as an
  //    "underlay".  read the documentation for descriptorpool for more
  //    information about underlays.
  //
  // you must also provide a messagefactory.  this factory will be used to
  // construct message objects representing extensions.  the factory's
  // getprototype() must return non-null for any descriptor which can be found
  // through the provided pool.
  //
  // if the provided factory might return instances of protocol-compiler-
  // generated (i.e. compiled-in) types, or if the outer message object m is
  // a generated type, then the given factory must have this property:  if
  // getprototype() is given a descriptor which resides in
  // descriptorpool::generated_pool(), the factory must return the same
  // prototype which messagefactory::generated_factory() would return.  that
  // is, given a descriptor for a generated type, the factory must return an
  // instance of the generated class (not dynamicmessage).  however, when
  // given a descriptor for a type that is not in generated_pool, the factory
  // is free to return any implementation.
  //
  // the reason for this requirement is that generated sub-objects may be
  // accessed via the standard (non-reflection) extension accessor methods,
  // and these methods will down-cast the object to the generated class type.
  // if the object is not actually of that type, the results would be undefined.
  // on the other hand, if an extension is not compiled in, then there is no
  // way the code could end up accessing it via the standard accessors -- the
  // only way to access the extension is via reflection.  when using reflection,
  // dynamicmessage and generated messages are indistinguishable, so it's fine
  // if these objects are represented using dynamicmessage.
  //
  // using dynamicmessagefactory on which you have called
  // setdelegatetogeneratedfactory(true) should be sufficient to satisfy the
  // above requirement.
  //
  // if either pool or factory is null, both must be null.
  //
  // note that this feature is ignored when parsing "lite" messages as they do
  // not have descriptors.
  void setextensionregistry(const descriptorpool* pool,
                            messagefactory* factory);

  // get the descriptorpool set via setextensionregistry(), or null if no pool
  // has been provided.
  const descriptorpool* getextensionpool();

  // get the messagefactory set via setextensionregistry(), or null if no
  // factory has been provided.
  messagefactory* getextensionfactory();

 private:
  google_disallow_evil_constructors(codedinputstream);

  zerocopyinputstream* input_;
  const uint8* buffer_;
  const uint8* buffer_end_;     // pointer to the end of the buffer.
  int total_bytes_read_;  // total bytes read from input_, including
                          // the current buffer

  // if total_bytes_read_ surpasses int_max, we record the extra bytes here
  // so that we can backup() on destruction.
  int overflow_bytes_;

  // lasttagwas() stuff.
  uint32 last_tag_;         // result of last readtag().

  // this is set true by readtag{fallback/slow}() if it is called when exactly
  // at eof, or by expectatend() when it returns true.  this happens when we
  // reach the end of a message and attempt to read another tag.
  bool legitimate_message_end_;

  // see enablealiasing().
  bool aliasing_enabled_;

  // limits
  limit current_limit_;   // if position = -1, no limit is applied

  // for simplicity, if the current buffer crosses a limit (either a normal
  // limit created by pushlimit() or the total bytes limit), buffer_size_
  // only tracks the number of bytes before that limit.  this field
  // contains the number of bytes after it.  note that this implies that if
  // buffer_size_ == 0 and buffer_size_after_limit_ > 0, we know we've
  // hit a limit.  however, if both are zero, it doesn't necessarily mean
  // we aren't at a limit -- the buffer may have ended exactly at the limit.
  int buffer_size_after_limit_;

  // maximum number of bytes to read, period.  this is unrelated to
  // current_limit_.  set using settotalbyteslimit().
  int total_bytes_limit_;

  // if positive/0: limit for bytes read after which a warning due to size
  // should be logged.
  // if -1: printing of warning disabled. can be set by client.
  // if -2: internal: limit has been reached, print full size when destructing.
  int total_bytes_warning_threshold_;

  // current recursion depth, controlled by incrementrecursiondepth() and
  // decrementrecursiondepth().
  int recursion_depth_;
  // recursion depth limit, set by setrecursionlimit().
  int recursion_limit_;

  // see setextensionregistry().
  const descriptorpool* extension_pool_;
  messagefactory* extension_factory_;

  // private member functions.

  // advance the buffer by a given number of bytes.
  void advance(int amount);

  // back up input_ to the current buffer position.
  void backupinputtocurrentposition();

  // recomputes the value of buffer_size_after_limit_.  must be called after
  // current_limit_ or total_bytes_limit_ changes.
  void recomputebufferlimits();

  // writes an error message saying that we hit total_bytes_limit_.
  void printtotalbyteslimiterror();

  // called when the buffer runs out to request more data.  implies an
  // advance(buffersize()).
  bool refresh();

  // when parsing varints, we optimize for the common case of small values, and
  // then optimize for the case when the varint fits within the current buffer
  // piece. the fallback method is used when we can't use the one-byte
  // optimization. the slow method is yet another fallback when the buffer is
  // not large enough. making the slow path out-of-line speeds up the common
  // case by 10-15%. the slow path is fairly uncommon: it only triggers when a
  // message crosses multiple buffers.
  bool readvarint32fallback(uint32* value);
  bool readvarint64fallback(uint64* value);
  bool readvarint32slow(uint32* value);
  bool readvarint64slow(uint64* value);
  bool readlittleendian32fallback(uint32* value);
  bool readlittleendian64fallback(uint64* value);
  // fallback/slow methods for reading tags. these do not update last_tag_,
  // but will set legitimate_message_end_ if we are at the end of the input
  // stream.
  uint32 readtagfallback();
  uint32 readtagslow();
  bool readstringfallback(string* buffer, int size);

  // return the size of the buffer.
  int buffersize() const;

  static const int kdefaulttotalbyteslimit = 64 << 20;  // 64mb

  static const int kdefaulttotalbyteswarningthreshold = 32 << 20;  // 32mb

  static int default_recursion_limit_;  // 100 by default.
};

// class which encodes and writes binary data which is composed of varint-
// encoded integers and fixed-width pieces.  wraps a zerocopyoutputstream.
// most users will not need to deal with codedoutputstream.
//
// most methods of codedoutputstream which return a bool return false if an
// underlying i/o error occurs.  once such a failure occurs, the
// codedoutputstream is broken and is no longer useful. the write* methods do
// not return the stream status, but will invalidate the stream if an error
// occurs. the client can probe haderror() to determine the status.
//
// note that every method of codedoutputstream which writes some data has
// a corresponding static "toarray" version. these versions write directly
// to the provided buffer, returning a pointer past the last written byte.
// they require that the buffer has sufficient capacity for the encoded data.
// this allows an optimization where we check if an output stream has enough
// space for an entire message before we start writing and, if there is, we
// call only the toarray methods to avoid doing bound checks for each
// individual value.
// i.e., in the example above:
//
//   codedoutputstream coded_output = new codedoutputstream(raw_output);
//   int magic_number = 1234;
//   char text[] = "hello world!";
//
//   int coded_size = sizeof(magic_number) +
//                    codedoutputstream::varintsize32(strlen(text)) +
//                    strlen(text);
//
//   uint8* buffer =
//       coded_output->getdirectbufferfornbytesandadvance(coded_size);
//   if (buffer != null) {
//     // the output stream has enough space in the buffer: write directly to
//     // the array.
//     buffer = codedoutputstream::writelittleendian32toarray(magic_number,
//                                                            buffer);
//     buffer = codedoutputstream::writevarint32toarray(strlen(text), buffer);
//     buffer = codedoutputstream::writerawtoarray(text, strlen(text), buffer);
//   } else {
//     // make bound-checked writes, which will ask the underlying stream for
//     // more space as needed.
//     coded_output->writelittleendian32(magic_number);
//     coded_output->writevarint32(strlen(text));
//     coded_output->writeraw(text, strlen(text));
//   }
//
//   delete coded_output;
class libprotobuf_export codedoutputstream {
 public:
  // create an codedoutputstream that writes to the given zerocopyoutputstream.
  explicit codedoutputstream(zerocopyoutputstream* output);

  // destroy the codedoutputstream and position the underlying
  // zerocopyoutputstream immediately after the last byte written.
  ~codedoutputstream();

  // skips a number of bytes, leaving the bytes unmodified in the underlying
  // buffer.  returns false if an underlying write error occurs.  this is
  // mainly useful with getdirectbufferpointer().
  bool skip(int count);

  // sets *data to point directly at the unwritten part of the
  // codedoutputstream's underlying buffer, and *size to the size of that
  // buffer, but does not advance the stream's current position.  this will
  // always either produce a non-empty buffer or return false.  if the caller
  // writes any data to this buffer, it should then call skip() to skip over
  // the consumed bytes.  this may be useful for implementing external fast
  // serialization routines for types of data not covered by the
  // codedoutputstream interface.
  bool getdirectbufferpointer(void** data, int* size);

  // if there are at least "size" bytes available in the current buffer,
  // returns a pointer directly into the buffer and advances over these bytes.
  // the caller may then write directly into this buffer (e.g. using the
  // *toarray static methods) rather than go through codedoutputstream.  if
  // there are not enough bytes available, returns null.  the return pointer is
  // invalidated as soon as any other non-const method of codedoutputstream
  // is called.
  inline uint8* getdirectbufferfornbytesandadvance(int size);

  // write raw bytes, copying them from the given buffer.
  void writeraw(const void* buffer, int size);
  // like writeraw()  but writing directly to the target array.
  // this is _not_ inlined, as the compiler often optimizes memcpy into inline
  // copy loops. since this gets called by every field with string or bytes
  // type, inlining may lead to a significant amount of code bloat, with only a
  // minor performance gain.
  static uint8* writerawtoarray(const void* buffer, int size, uint8* target);

  // equivalent to writeraw(str.data(), str.size()).
  void writestring(const string& str);
  // like writestring()  but writing directly to the target array.
  static uint8* writestringtoarray(const string& str, uint8* target);


  // write a 32-bit little-endian integer.
  void writelittleendian32(uint32 value);
  // like writelittleendian32()  but writing directly to the target array.
  static uint8* writelittleendian32toarray(uint32 value, uint8* target);
  // write a 64-bit little-endian integer.
  void writelittleendian64(uint64 value);
  // like writelittleendian64()  but writing directly to the target array.
  static uint8* writelittleendian64toarray(uint64 value, uint8* target);

  // write an unsigned integer with varint encoding.  writing a 32-bit value
  // is equivalent to casting it to uint64 and writing it as a 64-bit value,
  // but may be more efficient.
  void writevarint32(uint32 value);
  // like writevarint32()  but writing directly to the target array.
  static uint8* writevarint32toarray(uint32 value, uint8* target);
  // write an unsigned integer with varint encoding.
  void writevarint64(uint64 value);
  // like writevarint64()  but writing directly to the target array.
  static uint8* writevarint64toarray(uint64 value, uint8* target);

  // equivalent to writevarint32() except when the value is negative,
  // in which case it must be sign-extended to a full 10 bytes.
  void writevarint32signextended(int32 value);
  // like writevarint32signextended()  but writing directly to the target array.
  static uint8* writevarint32signextendedtoarray(int32 value, uint8* target);

  // this is identical to writevarint32(), but optimized for writing tags.
  // in particular, if the input is a compile-time constant, this method
  // compiles down to a couple instructions.
  // always inline because otherwise the aformentioned optimization can't work,
  // but gcc by default doesn't want to inline this.
  void writetag(uint32 value);
  // like writetag()  but writing directly to the target array.
  static uint8* writetagtoarray(
      uint32 value, uint8* target) google_attribute_always_inline;

  // returns the number of bytes needed to encode the given value as a varint.
  static int varintsize32(uint32 value);
  // returns the number of bytes needed to encode the given value as a varint.
  static int varintsize64(uint64 value);

  // if negative, 10 bytes.  otheriwse, same as varintsize32().
  static int varintsize32signextended(int32 value);

  // compile-time equivalent of varintsize32().
  template <uint32 value>
  struct staticvarintsize32 {
    static const int value =
        (value < (1 << 7))
            ? 1
            : (value < (1 << 14))
                ? 2
                : (value < (1 << 21))
                    ? 3
                    : (value < (1 << 28))
                        ? 4
                        : 5;
  };

  // returns the total number of bytes written since this object was created.
  inline int bytecount() const;

  // returns true if there was an underlying i/o error since this object was
  // created.
  bool haderror() const { return had_error_; }

 private:
  google_disallow_evil_constructors(codedoutputstream);

  zerocopyoutputstream* output_;
  uint8* buffer_;
  int buffer_size_;
  int total_bytes_;  // sum of sizes of all buffers seen so far.
  bool had_error_;   // whether an error occurred during output.

  // advance the buffer by a given number of bytes.
  void advance(int amount);

  // called when the buffer runs out to request more data.  implies an
  // advance(buffer_size_).
  bool refresh();

  static uint8* writevarint32fallbacktoarray(uint32 value, uint8* target);

  // always-inlined versions of writevarint* functions so that code can be
  // reused, while still controlling size. for instance, writevarint32toarray()
  // should not directly call this: since it is inlined itself, doing so
  // would greatly increase the size of generated code. instead, it should call
  // writevarint32fallbacktoarray.  meanwhile, writevarint32() is already
  // out-of-line, so it should just invoke this directly to avoid any extra
  // function call overhead.
  static uint8* writevarint32fallbacktoarrayinline(
      uint32 value, uint8* target) google_attribute_always_inline;
  static uint8* writevarint64toarrayinline(
      uint64 value, uint8* target) google_attribute_always_inline;

  static int varintsize32fallback(uint32 value);
};

// inline methods ====================================================
// the vast majority of varints are only one byte.  these inline
// methods optimize for that case.

inline bool codedinputstream::readvarint32(uint32* value) {
  if (google_predict_true(buffer_ < buffer_end_) && *buffer_ < 0x80) {
    *value = *buffer_;
    advance(1);
    return true;
  } else {
    return readvarint32fallback(value);
  }
}

inline bool codedinputstream::readvarint64(uint64* value) {
  if (google_predict_true(buffer_ < buffer_end_) && *buffer_ < 0x80) {
    *value = *buffer_;
    advance(1);
    return true;
  } else {
    return readvarint64fallback(value);
  }
}

// static
inline const uint8* codedinputstream::readlittleendian32fromarray(
    const uint8* buffer,
    uint32* value) {
#if defined(protobuf_little_endian)
  memcpy(value, buffer, sizeof(*value));
  return buffer + sizeof(*value);
#else
  *value = (static_cast<uint32>(buffer[0])      ) |
           (static_cast<uint32>(buffer[1]) <<  8) |
           (static_cast<uint32>(buffer[2]) << 16) |
           (static_cast<uint32>(buffer[3]) << 24);
  return buffer + sizeof(*value);
#endif
}
// static
inline const uint8* codedinputstream::readlittleendian64fromarray(
    const uint8* buffer,
    uint64* value) {
#if defined(protobuf_little_endian)
  memcpy(value, buffer, sizeof(*value));
  return buffer + sizeof(*value);
#else
  uint32 part0 = (static_cast<uint32>(buffer[0])      ) |
                 (static_cast<uint32>(buffer[1]) <<  8) |
                 (static_cast<uint32>(buffer[2]) << 16) |
                 (static_cast<uint32>(buffer[3]) << 24);
  uint32 part1 = (static_cast<uint32>(buffer[4])      ) |
                 (static_cast<uint32>(buffer[5]) <<  8) |
                 (static_cast<uint32>(buffer[6]) << 16) |
                 (static_cast<uint32>(buffer[7]) << 24);
  *value = static_cast<uint64>(part0) |
          (static_cast<uint64>(part1) << 32);
  return buffer + sizeof(*value);
#endif
}

inline bool codedinputstream::readlittleendian32(uint32* value) {
#if defined(protobuf_little_endian)
  if (google_predict_true(buffersize() >= static_cast<int>(sizeof(*value)))) {
    memcpy(value, buffer_, sizeof(*value));
    advance(sizeof(*value));
    return true;
  } else {
    return readlittleendian32fallback(value);
  }
#else
  return readlittleendian32fallback(value);
#endif
}

inline bool codedinputstream::readlittleendian64(uint64* value) {
#if defined(protobuf_little_endian)
  if (google_predict_true(buffersize() >= static_cast<int>(sizeof(*value)))) {
    memcpy(value, buffer_, sizeof(*value));
    advance(sizeof(*value));
    return true;
  } else {
    return readlittleendian64fallback(value);
  }
#else
  return readlittleendian64fallback(value);
#endif
}

inline uint32 codedinputstream::readtag() {
  if (google_predict_true(buffer_ < buffer_end_) && buffer_[0] < 0x80) {
    last_tag_ = buffer_[0];
    advance(1);
    return last_tag_;
  } else {
    last_tag_ = readtagfallback();
    return last_tag_;
  }
}

inline bool codedinputstream::lasttagwas(uint32 expected) {
  return last_tag_ == expected;
}

inline bool codedinputstream::consumedentiremessage() {
  return legitimate_message_end_;
}

inline bool codedinputstream::expecttag(uint32 expected) {
  if (expected < (1 << 7)) {
    if (google_predict_true(buffer_ < buffer_end_) && buffer_[0] == expected) {
      advance(1);
      return true;
    } else {
      return false;
    }
  } else if (expected < (1 << 14)) {
    if (google_predict_true(buffersize() >= 2) &&
        buffer_[0] == static_cast<uint8>(expected | 0x80) &&
        buffer_[1] == static_cast<uint8>(expected >> 7)) {
      advance(2);
      return true;
    } else {
      return false;
    }
  } else {
    // don't bother optimizing for larger values.
    return false;
  }
}

inline const uint8* codedinputstream::expecttagfromarray(
    const uint8* buffer, uint32 expected) {
  if (expected < (1 << 7)) {
    if (buffer[0] == expected) {
      return buffer + 1;
    }
  } else if (expected < (1 << 14)) {
    if (buffer[0] == static_cast<uint8>(expected | 0x80) &&
        buffer[1] == static_cast<uint8>(expected >> 7)) {
      return buffer + 2;
    }
  }
  return null;
}

inline void codedinputstream::getdirectbufferpointerinline(const void** data,
                                                           int* size) {
  *data = buffer_;
  *size = buffer_end_ - buffer_;
}

inline bool codedinputstream::expectatend() {
  // if we are at a limit we know no more bytes can be read.  otherwise, it's
  // hard to say without calling refresh(), and we'd rather not do that.

  if (buffer_ == buffer_end_ &&
      ((buffer_size_after_limit_ != 0) ||
       (total_bytes_read_ == current_limit_))) {
    last_tag_ = 0;                   // pretend we called readtag()...
    legitimate_message_end_ = true;  // ... and it hit eof.
    return true;
  } else {
    return false;
  }
}

inline int codedinputstream::currentposition() const {
  return total_bytes_read_ - (buffersize() + buffer_size_after_limit_);
}

inline uint8* codedoutputstream::getdirectbufferfornbytesandadvance(int size) {
  if (buffer_size_ < size) {
    return null;
  } else {
    uint8* result = buffer_;
    advance(size);
    return result;
  }
}

inline uint8* codedoutputstream::writevarint32toarray(uint32 value,
                                                        uint8* target) {
  if (value < 0x80) {
    *target = value;
    return target + 1;
  } else {
    return writevarint32fallbacktoarray(value, target);
  }
}

inline void codedoutputstream::writevarint32signextended(int32 value) {
  if (value < 0) {
    writevarint64(static_cast<uint64>(value));
  } else {
    writevarint32(static_cast<uint32>(value));
  }
}

inline uint8* codedoutputstream::writevarint32signextendedtoarray(
    int32 value, uint8* target) {
  if (value < 0) {
    return writevarint64toarray(static_cast<uint64>(value), target);
  } else {
    return writevarint32toarray(static_cast<uint32>(value), target);
  }
}

inline uint8* codedoutputstream::writelittleendian32toarray(uint32 value,
                                                            uint8* target) {
#if defined(protobuf_little_endian)
  memcpy(target, &value, sizeof(value));
#else
  target[0] = static_cast<uint8>(value);
  target[1] = static_cast<uint8>(value >>  8);
  target[2] = static_cast<uint8>(value >> 16);
  target[3] = static_cast<uint8>(value >> 24);
#endif
  return target + sizeof(value);
}

inline uint8* codedoutputstream::writelittleendian64toarray(uint64 value,
                                                            uint8* target) {
#if defined(protobuf_little_endian)
  memcpy(target, &value, sizeof(value));
#else
  uint32 part0 = static_cast<uint32>(value);
  uint32 part1 = static_cast<uint32>(value >> 32);

  target[0] = static_cast<uint8>(part0);
  target[1] = static_cast<uint8>(part0 >>  8);
  target[2] = static_cast<uint8>(part0 >> 16);
  target[3] = static_cast<uint8>(part0 >> 24);
  target[4] = static_cast<uint8>(part1);
  target[5] = static_cast<uint8>(part1 >>  8);
  target[6] = static_cast<uint8>(part1 >> 16);
  target[7] = static_cast<uint8>(part1 >> 24);
#endif
  return target + sizeof(value);
}

inline void codedoutputstream::writetag(uint32 value) {
  writevarint32(value);
}

inline uint8* codedoutputstream::writetagtoarray(
    uint32 value, uint8* target) {
  if (value < (1 << 7)) {
    target[0] = value;
    return target + 1;
  } else if (value < (1 << 14)) {
    target[0] = static_cast<uint8>(value | 0x80);
    target[1] = static_cast<uint8>(value >> 7);
    return target + 2;
  } else {
    return writevarint32fallbacktoarray(value, target);
  }
}

inline int codedoutputstream::varintsize32(uint32 value) {
  if (value < (1 << 7)) {
    return 1;
  } else  {
    return varintsize32fallback(value);
  }
}

inline int codedoutputstream::varintsize32signextended(int32 value) {
  if (value < 0) {
    return 10;     // todo(kenton):  make this a symbolic constant.
  } else {
    return varintsize32(static_cast<uint32>(value));
  }
}

inline void codedoutputstream::writestring(const string& str) {
  writeraw(str.data(), static_cast<int>(str.size()));
}

inline uint8* codedoutputstream::writestringtoarray(
    const string& str, uint8* target) {
  return writerawtoarray(str.data(), static_cast<int>(str.size()), target);
}

inline int codedoutputstream::bytecount() const {
  return total_bytes_ - buffer_size_;
}

inline void codedinputstream::advance(int amount) {
  buffer_ += amount;
}

inline void codedoutputstream::advance(int amount) {
  buffer_ += amount;
  buffer_size_ -= amount;
}

inline void codedinputstream::setrecursionlimit(int limit) {
  recursion_limit_ = limit;
}

inline bool codedinputstream::incrementrecursiondepth() {
  ++recursion_depth_;
  return recursion_depth_ <= recursion_limit_;
}

inline void codedinputstream::decrementrecursiondepth() {
  if (recursion_depth_ > 0) --recursion_depth_;
}

inline void codedinputstream::setextensionregistry(const descriptorpool* pool,
                                                   messagefactory* factory) {
  extension_pool_ = pool;
  extension_factory_ = factory;
}

inline const descriptorpool* codedinputstream::getextensionpool() {
  return extension_pool_;
}

inline messagefactory* codedinputstream::getextensionfactory() {
  return extension_factory_;
}

inline int codedinputstream::buffersize() const {
  return buffer_end_ - buffer_;
}

inline codedinputstream::codedinputstream(zerocopyinputstream* input)
  : input_(input),
    buffer_(null),
    buffer_end_(null),
    total_bytes_read_(0),
    overflow_bytes_(0),
    last_tag_(0),
    legitimate_message_end_(false),
    aliasing_enabled_(false),
    current_limit_(kint32max),
    buffer_size_after_limit_(0),
    total_bytes_limit_(kdefaulttotalbyteslimit),
    total_bytes_warning_threshold_(kdefaulttotalbyteswarningthreshold),
    recursion_depth_(0),
    recursion_limit_(default_recursion_limit_),
    extension_pool_(null),
    extension_factory_(null) {
  // eagerly refresh() so buffer space is immediately available.
  refresh();
}

inline codedinputstream::codedinputstream(const uint8* buffer, int size)
  : input_(null),
    buffer_(buffer),
    buffer_end_(buffer + size),
    total_bytes_read_(size),
    overflow_bytes_(0),
    last_tag_(0),
    legitimate_message_end_(false),
    aliasing_enabled_(false),
    current_limit_(size),
    buffer_size_after_limit_(0),
    total_bytes_limit_(kdefaulttotalbyteslimit),
    total_bytes_warning_threshold_(kdefaulttotalbyteswarningthreshold),
    recursion_depth_(0),
    recursion_limit_(default_recursion_limit_),
    extension_pool_(null),
    extension_factory_(null) {
  // note that setting current_limit_ == size is important to prevent some
  // code paths from trying to access input_ and segfaulting.
}

inline bool codedinputstream::isflat() const {
  return input_ == null;
}

}  // namespace io
}  // namespace protobuf


#if defined(_msc_ver) && _msc_ver >= 1300
  #pragma runtime_checks("c", restore)
#endif  // _msc_ver

}  // namespace google
#endif  // google_protobuf_io_coded_stream_h__
