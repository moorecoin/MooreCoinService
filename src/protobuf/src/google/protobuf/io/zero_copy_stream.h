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
// this file contains the zerocopyinputstream and zerocopyoutputstream
// interfaces, which represent abstract i/o streams to and from which
// protocol buffers can be read and written.  for a few simple
// implementations of these interfaces, see zero_copy_stream_impl.h.
//
// these interfaces are different from classic i/o streams in that they
// try to minimize the amount of data copying that needs to be done.
// to accomplish this, responsibility for allocating buffers is moved to
// the stream object, rather than being the responsibility of the caller.
// so, the stream can return a buffer which actually points directly into
// the final data structure where the bytes are to be stored, and the caller
// can interact directly with that buffer, eliminating an intermediate copy
// operation.
//
// as an example, consider the common case in which you are reading bytes
// from an array that is already in memory (or perhaps an mmap()ed file).
// with classic i/o streams, you would do something like:
//   char buffer[buffer_size];
//   input->read(buffer, buffer_size);
//   dosomething(buffer, buffer_size);
// then, the stream basically just calls memcpy() to copy the data from
// the array into your buffer.  with a zerocopyinputstream, you would do
// this instead:
//   const void* buffer;
//   int size;
//   input->next(&buffer, &size);
//   dosomething(buffer, size);
// here, no copy is performed.  the input stream returns a pointer directly
// into the backing array, and the caller ends up reading directly from it.
//
// if you want to be able to read the old-fashion way, you can create
// a codedinputstream or codedoutputstream wrapping these objects and use
// their readraw()/writeraw() methods.  these will, of course, add a copy
// step, but coded*stream will handle buffering so at least it will be
// reasonably efficient.
//
// zerocopyinputstream example:
//   // read in a file and print its contents to stdout.
//   int fd = open("myfile", o_rdonly);
//   zerocopyinputstream* input = new fileinputstream(fd);
//
//   const void* buffer;
//   int size;
//   while (input->next(&buffer, &size)) {
//     cout.write(buffer, size);
//   }
//
//   delete input;
//   close(fd);
//
// zerocopyoutputstream example:
//   // copy the contents of "infile" to "outfile", using plain read() for
//   // "infile" but a zerocopyoutputstream for "outfile".
//   int infd = open("infile", o_rdonly);
//   int outfd = open("outfile", o_wronly);
//   zerocopyoutputstream* output = new fileoutputstream(outfd);
//
//   void* buffer;
//   int size;
//   while (output->next(&buffer, &size)) {
//     int bytes = read(infd, buffer, size);
//     if (bytes < size) {
//       // reached eof.
//       output->backup(size - bytes);
//       break;
//     }
//   }
//
//   delete output;
//   close(infd);
//   close(outfd);

#ifndef google_protobuf_io_zero_copy_stream_h__
#define google_protobuf_io_zero_copy_stream_h__

#include <string>
#include <google/protobuf/stubs/common.h>

namespace google {

namespace protobuf {
namespace io {

// defined in this file.
class zerocopyinputstream;
class zerocopyoutputstream;

// abstract interface similar to an input stream but designed to minimize
// copying.
class libprotobuf_export zerocopyinputstream {
 public:
  inline zerocopyinputstream() {}
  virtual ~zerocopyinputstream();

  // obtains a chunk of data from the stream.
  //
  // preconditions:
  // * "size" and "data" are not null.
  //
  // postconditions:
  // * if the returned value is false, there is no more data to return or
  //   an error occurred.  all errors are permanent.
  // * otherwise, "size" points to the actual number of bytes read and "data"
  //   points to a pointer to a buffer containing these bytes.
  // * ownership of this buffer remains with the stream, and the buffer
  //   remains valid only until some other method of the stream is called
  //   or the stream is destroyed.
  // * it is legal for the returned buffer to have zero size, as long
  //   as repeatedly calling next() eventually yields a buffer with non-zero
  //   size.
  virtual bool next(const void** data, int* size) = 0;

  // backs up a number of bytes, so that the next call to next() returns
  // data again that was already returned by the last call to next().  this
  // is useful when writing procedures that are only supposed to read up
  // to a certain point in the input, then return.  if next() returns a
  // buffer that goes beyond what you wanted to read, you can use backup()
  // to return to the point where you intended to finish.
  //
  // preconditions:
  // * the last method called must have been next().
  // * count must be less than or equal to the size of the last buffer
  //   returned by next().
  //
  // postconditions:
  // * the last "count" bytes of the last buffer returned by next() will be
  //   pushed back into the stream.  subsequent calls to next() will return
  //   the same data again before producing new data.
  virtual void backup(int count) = 0;

  // skips a number of bytes.  returns false if the end of the stream is
  // reached or some input error occurred.  in the end-of-stream case, the
  // stream is advanced to the end of the stream (so bytecount() will return
  // the total size of the stream).
  virtual bool skip(int count) = 0;

  // returns the total number of bytes read since this object was created.
  virtual int64 bytecount() const = 0;


 private:
  google_disallow_evil_constructors(zerocopyinputstream);
};

// abstract interface similar to an output stream but designed to minimize
// copying.
class libprotobuf_export zerocopyoutputstream {
 public:
  inline zerocopyoutputstream() {}
  virtual ~zerocopyoutputstream();

  // obtains a buffer into which data can be written.  any data written
  // into this buffer will eventually (maybe instantly, maybe later on)
  // be written to the output.
  //
  // preconditions:
  // * "size" and "data" are not null.
  //
  // postconditions:
  // * if the returned value is false, an error occurred.  all errors are
  //   permanent.
  // * otherwise, "size" points to the actual number of bytes in the buffer
  //   and "data" points to the buffer.
  // * ownership of this buffer remains with the stream, and the buffer
  //   remains valid only until some other method of the stream is called
  //   or the stream is destroyed.
  // * any data which the caller stores in this buffer will eventually be
  //   written to the output (unless backup() is called).
  // * it is legal for the returned buffer to have zero size, as long
  //   as repeatedly calling next() eventually yields a buffer with non-zero
  //   size.
  virtual bool next(void** data, int* size) = 0;

  // backs up a number of bytes, so that the end of the last buffer returned
  // by next() is not actually written.  this is needed when you finish
  // writing all the data you want to write, but the last buffer was bigger
  // than you needed.  you don't want to write a bunch of garbage after the
  // end of your data, so you use backup() to back up.
  //
  // preconditions:
  // * the last method called must have been next().
  // * count must be less than or equal to the size of the last buffer
  //   returned by next().
  // * the caller must not have written anything to the last "count" bytes
  //   of that buffer.
  //
  // postconditions:
  // * the last "count" bytes of the last buffer returned by next() will be
  //   ignored.
  virtual void backup(int count) = 0;

  // returns the total number of bytes written since this object was created.
  virtual int64 bytecount() const = 0;


 private:
  google_disallow_evil_constructors(zerocopyoutputstream);
};

}  // namespace io
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_io_zero_copy_stream_h__
