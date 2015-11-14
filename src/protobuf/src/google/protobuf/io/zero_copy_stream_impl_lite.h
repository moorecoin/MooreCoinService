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
// this file contains common implementations of the interfaces defined in
// zero_copy_stream.h which are included in the "lite" protobuf library.
// these implementations cover i/o on raw arrays and strings, as well as
// adaptors which make it easy to implement streams based on traditional
// streams.  of course, many users will probably want to write their own
// implementations of these interfaces specific to the particular i/o
// abstractions they prefer to use, but these should cover the most common
// cases.

#ifndef google_protobuf_io_zero_copy_stream_impl_lite_h__
#define google_protobuf_io_zero_copy_stream_impl_lite_h__

#include <string>
#include <iosfwd>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/common.h>


namespace google {
namespace protobuf {
namespace io {

// ===================================================================

// a zerocopyinputstream backed by an in-memory array of bytes.
class libprotobuf_export arrayinputstream : public zerocopyinputstream {
 public:
  // create an inputstream that returns the bytes pointed to by "data".
  // "data" remains the property of the caller but must remain valid until
  // the stream is destroyed.  if a block_size is given, calls to next()
  // will return data blocks no larger than the given size.  otherwise, the
  // first call to next() returns the entire array.  block_size is mainly
  // useful for testing; in production you would probably never want to set
  // it.
  arrayinputstream(const void* data, int size, int block_size = -1);
  ~arrayinputstream();

  // implements zerocopyinputstream ----------------------------------
  bool next(const void** data, int* size);
  void backup(int count);
  bool skip(int count);
  int64 bytecount() const;


 private:
  const uint8* const data_;  // the byte array.
  const int size_;           // total size of the array.
  const int block_size_;     // how many bytes to return at a time.

  int position_;
  int last_returned_size_;   // how many bytes we returned last time next()
                             // was called (used for error checking only).

  google_disallow_evil_constructors(arrayinputstream);
};

// ===================================================================

// a zerocopyoutputstream backed by an in-memory array of bytes.
class libprotobuf_export arrayoutputstream : public zerocopyoutputstream {
 public:
  // create an outputstream that writes to the bytes pointed to by "data".
  // "data" remains the property of the caller but must remain valid until
  // the stream is destroyed.  if a block_size is given, calls to next()
  // will return data blocks no larger than the given size.  otherwise, the
  // first call to next() returns the entire array.  block_size is mainly
  // useful for testing; in production you would probably never want to set
  // it.
  arrayoutputstream(void* data, int size, int block_size = -1);
  ~arrayoutputstream();

  // implements zerocopyoutputstream ---------------------------------
  bool next(void** data, int* size);
  void backup(int count);
  int64 bytecount() const;

 private:
  uint8* const data_;        // the byte array.
  const int size_;           // total size of the array.
  const int block_size_;     // how many bytes to return at a time.

  int position_;
  int last_returned_size_;   // how many bytes we returned last time next()
                             // was called (used for error checking only).

  google_disallow_evil_constructors(arrayoutputstream);
};

// ===================================================================

// a zerocopyoutputstream which appends bytes to a string.
class libprotobuf_export stringoutputstream : public zerocopyoutputstream {
 public:
  // create a stringoutputstream which appends bytes to the given string.
  // the string remains property of the caller, but it must not be accessed
  // in any way until the stream is destroyed.
  //
  // hint:  if you call target->reserve(n) before creating the stream,
  //   the first call to next() will return at least n bytes of buffer
  //   space.
  explicit stringoutputstream(string* target);
  ~stringoutputstream();

  // implements zerocopyoutputstream ---------------------------------
  bool next(void** data, int* size);
  void backup(int count);
  int64 bytecount() const;

 private:
  static const int kminimumsize = 16;

  string* target_;

  google_disallow_evil_constructors(stringoutputstream);
};

// note:  there is no stringinputstream.  instead, just create an
// arrayinputstream as follows:
//   arrayinputstream input(str.data(), str.size());

// ===================================================================

// a generic traditional input stream interface.
//
// lots of traditional input streams (e.g. file descriptors, c stdio
// streams, and c++ iostreams) expose an interface where every read
// involves copying bytes into a buffer.  if you want to take such an
// interface and make a zerocopyinputstream based on it, simply implement
// copyinginputstream and then use copyinginputstreamadaptor.
//
// copyinginputstream implementations should avoid buffering if possible.
// copyinginputstreamadaptor does its own buffering and will read data
// in large blocks.
class libprotobuf_export copyinginputstream {
 public:
  virtual ~copyinginputstream();

  // reads up to "size" bytes into the given buffer.  returns the number of
  // bytes read.  read() waits until at least one byte is available, or
  // returns zero if no bytes will ever become available (eof), or -1 if a
  // permanent read error occurred.
  virtual int read(void* buffer, int size) = 0;

  // skips the next "count" bytes of input.  returns the number of bytes
  // actually skipped.  this will always be exactly equal to "count" unless
  // eof was reached or a permanent read error occurred.
  //
  // the default implementation just repeatedly calls read() into a scratch
  // buffer.
  virtual int skip(int count);
};

// a zerocopyinputstream which reads from a copyinginputstream.  this is
// useful for implementing zerocopyinputstreams that read from traditional
// streams.  note that this class is not really zero-copy.
//
// if you want to read from file descriptors or c++ istreams, this is
// already implemented for you:  use fileinputstream or istreaminputstream
// respectively.
class libprotobuf_export copyinginputstreamadaptor : public zerocopyinputstream {
 public:
  // creates a stream that reads from the given copyinginputstream.
  // if a block_size is given, it specifies the number of bytes that
  // should be read and returned with each call to next().  otherwise,
  // a reasonable default is used.  the caller retains ownership of
  // copying_stream unless setownscopyingstream(true) is called.
  explicit copyinginputstreamadaptor(copyinginputstream* copying_stream,
                                     int block_size = -1);
  ~copyinginputstreamadaptor();

  // call setownscopyingstream(true) to tell the copyinginputstreamadaptor to
  // delete the underlying copyinginputstream when it is destroyed.
  void setownscopyingstream(bool value) { owns_copying_stream_ = value; }

  // implements zerocopyinputstream ----------------------------------
  bool next(const void** data, int* size);
  void backup(int count);
  bool skip(int count);
  int64 bytecount() const;

 private:
  // insures that buffer_ is not null.
  void allocatebufferifneeded();
  // frees the buffer and resets buffer_used_.
  void freebuffer();

  // the underlying copying stream.
  copyinginputstream* copying_stream_;
  bool owns_copying_stream_;

  // true if we have seen a permenant error from the underlying stream.
  bool failed_;

  // the current position of copying_stream_, relative to the point where
  // we started reading.
  int64 position_;

  // data is read into this buffer.  it may be null if no buffer is currently
  // in use.  otherwise, it points to an array of size buffer_size_.
  scoped_array<uint8> buffer_;
  const int buffer_size_;

  // number of valid bytes currently in the buffer (i.e. the size last
  // returned by next()).  0 <= buffer_used_ <= buffer_size_.
  int buffer_used_;

  // number of bytes in the buffer which were backed up over by a call to
  // backup().  these need to be returned again.
  // 0 <= backup_bytes_ <= buffer_used_
  int backup_bytes_;

  google_disallow_evil_constructors(copyinginputstreamadaptor);
};

// ===================================================================

// a generic traditional output stream interface.
//
// lots of traditional output streams (e.g. file descriptors, c stdio
// streams, and c++ iostreams) expose an interface where every write
// involves copying bytes from a buffer.  if you want to take such an
// interface and make a zerocopyoutputstream based on it, simply implement
// copyingoutputstream and then use copyingoutputstreamadaptor.
//
// copyingoutputstream implementations should avoid buffering if possible.
// copyingoutputstreamadaptor does its own buffering and will write data
// in large blocks.
class libprotobuf_export copyingoutputstream {
 public:
  virtual ~copyingoutputstream();

  // writes "size" bytes from the given buffer to the output.  returns true
  // if successful, false on a write error.
  virtual bool write(const void* buffer, int size) = 0;
};

// a zerocopyoutputstream which writes to a copyingoutputstream.  this is
// useful for implementing zerocopyoutputstreams that write to traditional
// streams.  note that this class is not really zero-copy.
//
// if you want to write to file descriptors or c++ ostreams, this is
// already implemented for you:  use fileoutputstream or ostreamoutputstream
// respectively.
class libprotobuf_export copyingoutputstreamadaptor : public zerocopyoutputstream {
 public:
  // creates a stream that writes to the given unix file descriptor.
  // if a block_size is given, it specifies the size of the buffers
  // that should be returned by next().  otherwise, a reasonable default
  // is used.
  explicit copyingoutputstreamadaptor(copyingoutputstream* copying_stream,
                                      int block_size = -1);
  ~copyingoutputstreamadaptor();

  // writes all pending data to the underlying stream.  returns false if a
  // write error occurred on the underlying stream.  (the underlying
  // stream itself is not necessarily flushed.)
  bool flush();

  // call setownscopyingstream(true) to tell the copyingoutputstreamadaptor to
  // delete the underlying copyingoutputstream when it is destroyed.
  void setownscopyingstream(bool value) { owns_copying_stream_ = value; }

  // implements zerocopyoutputstream ---------------------------------
  bool next(void** data, int* size);
  void backup(int count);
  int64 bytecount() const;

 private:
  // write the current buffer, if it is present.
  bool writebuffer();
  // insures that buffer_ is not null.
  void allocatebufferifneeded();
  // frees the buffer.
  void freebuffer();

  // the underlying copying stream.
  copyingoutputstream* copying_stream_;
  bool owns_copying_stream_;

  // true if we have seen a permenant error from the underlying stream.
  bool failed_;

  // the current position of copying_stream_, relative to the point where
  // we started writing.
  int64 position_;

  // data is written from this buffer.  it may be null if no buffer is
  // currently in use.  otherwise, it points to an array of size buffer_size_.
  scoped_array<uint8> buffer_;
  const int buffer_size_;

  // number of valid bytes currently in the buffer (i.e. the size last
  // returned by next()).  when backup() is called, we just reduce this.
  // 0 <= buffer_used_ <= buffer_size_.
  int buffer_used_;

  google_disallow_evil_constructors(copyingoutputstreamadaptor);
};

// ===================================================================

}  // namespace io
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_io_zero_copy_stream_impl_lite_h__
