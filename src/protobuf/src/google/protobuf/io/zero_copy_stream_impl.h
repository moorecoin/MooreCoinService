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
// zero_copy_stream.h which are only included in the full (non-lite)
// protobuf library.  these implementations include unix file descriptors
// and c++ iostreams.  see also:  zero_copy_stream_impl_lite.h

#ifndef google_protobuf_io_zero_copy_stream_impl_h__
#define google_protobuf_io_zero_copy_stream_impl_h__

#include <string>
#include <iosfwd>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/stubs/common.h>


namespace google {
namespace protobuf {
namespace io {


// ===================================================================

// a zerocopyinputstream which reads from a file descriptor.
//
// fileinputstream is preferred over using an ifstream with istreaminputstream.
// the latter will introduce an extra layer of buffering, harming performance.
// also, it's conceivable that fileinputstream could someday be enhanced
// to use zero-copy file descriptors on oss which support them.
class libprotobuf_export fileinputstream : public zerocopyinputstream {
 public:
  // creates a stream that reads from the given unix file descriptor.
  // if a block_size is given, it specifies the number of bytes that
  // should be read and returned with each call to next().  otherwise,
  // a reasonable default is used.
  explicit fileinputstream(int file_descriptor, int block_size = -1);
  ~fileinputstream();

  // flushes any buffers and closes the underlying file.  returns false if
  // an error occurs during the process; use geterrno() to examine the error.
  // even if an error occurs, the file descriptor is closed when this returns.
  bool close();

  // by default, the file descriptor is not closed when the stream is
  // destroyed.  call setcloseondelete(true) to change that.  warning:
  // this leaves no way for the caller to detect if close() fails.  if
  // detecting close() errors is important to you, you should arrange
  // to close the descriptor yourself.
  void setcloseondelete(bool value) { copying_input_.setcloseondelete(value); }

  // if an i/o error has occurred on this file descriptor, this is the
  // errno from that error.  otherwise, this is zero.  once an error
  // occurs, the stream is broken and all subsequent operations will
  // fail.
  int geterrno() { return copying_input_.geterrno(); }

  // implements zerocopyinputstream ----------------------------------
  bool next(const void** data, int* size);
  void backup(int count);
  bool skip(int count);
  int64 bytecount() const;

 private:
  class libprotobuf_export copyingfileinputstream : public copyinginputstream {
   public:
    copyingfileinputstream(int file_descriptor);
    ~copyingfileinputstream();

    bool close();
    void setcloseondelete(bool value) { close_on_delete_ = value; }
    int geterrno() { return errno_; }

    // implements copyinginputstream ---------------------------------
    int read(void* buffer, int size);
    int skip(int count);

   private:
    // the file descriptor.
    const int file_;
    bool close_on_delete_;
    bool is_closed_;

    // the errno of the i/o error, if one has occurred.  otherwise, zero.
    int errno_;

    // did we try to seek once and fail?  if so, we assume this file descriptor
    // doesn't support seeking and won't try again.
    bool previous_seek_failed_;

    google_disallow_evil_constructors(copyingfileinputstream);
  };

  copyingfileinputstream copying_input_;
  copyinginputstreamadaptor impl_;

  google_disallow_evil_constructors(fileinputstream);
};

// ===================================================================

// a zerocopyoutputstream which writes to a file descriptor.
//
// fileoutputstream is preferred over using an ofstream with
// ostreamoutputstream.  the latter will introduce an extra layer of buffering,
// harming performance.  also, it's conceivable that fileoutputstream could
// someday be enhanced to use zero-copy file descriptors on oss which
// support them.
class libprotobuf_export fileoutputstream : public zerocopyoutputstream {
 public:
  // creates a stream that writes to the given unix file descriptor.
  // if a block_size is given, it specifies the size of the buffers
  // that should be returned by next().  otherwise, a reasonable default
  // is used.
  explicit fileoutputstream(int file_descriptor, int block_size = -1);
  ~fileoutputstream();

  // flushes any buffers and closes the underlying file.  returns false if
  // an error occurs during the process; use geterrno() to examine the error.
  // even if an error occurs, the file descriptor is closed when this returns.
  bool close();

  // flushes fileoutputstream's buffers but does not close the
  // underlying file. no special measures are taken to ensure that
  // underlying operating system file object is synchronized to disk.
  bool flush();

  // by default, the file descriptor is not closed when the stream is
  // destroyed.  call setcloseondelete(true) to change that.  warning:
  // this leaves no way for the caller to detect if close() fails.  if
  // detecting close() errors is important to you, you should arrange
  // to close the descriptor yourself.
  void setcloseondelete(bool value) { copying_output_.setcloseondelete(value); }

  // if an i/o error has occurred on this file descriptor, this is the
  // errno from that error.  otherwise, this is zero.  once an error
  // occurs, the stream is broken and all subsequent operations will
  // fail.
  int geterrno() { return copying_output_.geterrno(); }

  // implements zerocopyoutputstream ---------------------------------
  bool next(void** data, int* size);
  void backup(int count);
  int64 bytecount() const;

 private:
  class libprotobuf_export copyingfileoutputstream : public copyingoutputstream {
   public:
    copyingfileoutputstream(int file_descriptor);
    ~copyingfileoutputstream();

    bool close();
    void setcloseondelete(bool value) { close_on_delete_ = value; }
    int geterrno() { return errno_; }

    // implements copyingoutputstream --------------------------------
    bool write(const void* buffer, int size);

   private:
    // the file descriptor.
    const int file_;
    bool close_on_delete_;
    bool is_closed_;

    // the errno of the i/o error, if one has occurred.  otherwise, zero.
    int errno_;

    google_disallow_evil_constructors(copyingfileoutputstream);
  };

  copyingfileoutputstream copying_output_;
  copyingoutputstreamadaptor impl_;

  google_disallow_evil_constructors(fileoutputstream);
};

// ===================================================================

// a zerocopyinputstream which reads from a c++ istream.
//
// note that for reading files (or anything represented by a file descriptor),
// fileinputstream is more efficient.
class libprotobuf_export istreaminputstream : public zerocopyinputstream {
 public:
  // creates a stream that reads from the given c++ istream.
  // if a block_size is given, it specifies the number of bytes that
  // should be read and returned with each call to next().  otherwise,
  // a reasonable default is used.
  explicit istreaminputstream(istream* stream, int block_size = -1);
  ~istreaminputstream();

  // implements zerocopyinputstream ----------------------------------
  bool next(const void** data, int* size);
  void backup(int count);
  bool skip(int count);
  int64 bytecount() const;

 private:
  class libprotobuf_export copyingistreaminputstream : public copyinginputstream {
   public:
    copyingistreaminputstream(istream* input);
    ~copyingistreaminputstream();

    // implements copyinginputstream ---------------------------------
    int read(void* buffer, int size);
    // (we use the default implementation of skip().)

   private:
    // the stream.
    istream* input_;

    google_disallow_evil_constructors(copyingistreaminputstream);
  };

  copyingistreaminputstream copying_input_;
  copyinginputstreamadaptor impl_;

  google_disallow_evil_constructors(istreaminputstream);
};

// ===================================================================

// a zerocopyoutputstream which writes to a c++ ostream.
//
// note that for writing files (or anything represented by a file descriptor),
// fileoutputstream is more efficient.
class libprotobuf_export ostreamoutputstream : public zerocopyoutputstream {
 public:
  // creates a stream that writes to the given c++ ostream.
  // if a block_size is given, it specifies the size of the buffers
  // that should be returned by next().  otherwise, a reasonable default
  // is used.
  explicit ostreamoutputstream(ostream* stream, int block_size = -1);
  ~ostreamoutputstream();

  // implements zerocopyoutputstream ---------------------------------
  bool next(void** data, int* size);
  void backup(int count);
  int64 bytecount() const;

 private:
  class libprotobuf_export copyingostreamoutputstream : public copyingoutputstream {
   public:
    copyingostreamoutputstream(ostream* output);
    ~copyingostreamoutputstream();

    // implements copyingoutputstream --------------------------------
    bool write(const void* buffer, int size);

   private:
    // the stream.
    ostream* output_;

    google_disallow_evil_constructors(copyingostreamoutputstream);
  };

  copyingostreamoutputstream copying_output_;
  copyingoutputstreamadaptor impl_;

  google_disallow_evil_constructors(ostreamoutputstream);
};

// ===================================================================

// a zerocopyinputstream which reads from several other streams in sequence.
// concatenatinginputstream is unable to distinguish between end-of-stream
// and read errors in the underlying streams, so it assumes any errors mean
// end-of-stream.  so, if the underlying streams fail for any other reason,
// concatenatinginputstream may do odd things.  it is suggested that you do
// not use concatenatinginputstream on streams that might produce read errors
// other than end-of-stream.
class libprotobuf_export concatenatinginputstream : public zerocopyinputstream {
 public:
  // all streams passed in as well as the array itself must remain valid
  // until the concatenatinginputstream is destroyed.
  concatenatinginputstream(zerocopyinputstream* const streams[], int count);
  ~concatenatinginputstream();

  // implements zerocopyinputstream ----------------------------------
  bool next(const void** data, int* size);
  void backup(int count);
  bool skip(int count);
  int64 bytecount() const;


 private:
  // as streams are retired, streams_ is incremented and count_ is
  // decremented.
  zerocopyinputstream* const* streams_;
  int stream_count_;
  int64 bytes_retired_;  // bytes read from previous streams.

  google_disallow_evil_constructors(concatenatinginputstream);
};

// ===================================================================

// a zerocopyinputstream which wraps some other stream and limits it to
// a particular byte count.
class libprotobuf_export limitinginputstream : public zerocopyinputstream {
 public:
  limitinginputstream(zerocopyinputstream* input, int64 limit);
  ~limitinginputstream();

  // implements zerocopyinputstream ----------------------------------
  bool next(const void** data, int* size);
  void backup(int count);
  bool skip(int count);
  int64 bytecount() const;


 private:
  zerocopyinputstream* input_;
  int64 limit_;  // decreases as we go, becomes negative if we overshoot.

  google_disallow_evil_constructors(limitinginputstream);
};

// ===================================================================

}  // namespace io
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_io_zero_copy_stream_impl_h__
