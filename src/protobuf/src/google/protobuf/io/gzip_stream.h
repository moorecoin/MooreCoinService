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

// author: brianolson@google.com (brian olson)
//
// this file contains the definition for classes gzipinputstream and
// gzipoutputstream.
//
// gzipinputstream decompresses data from an underlying
// zerocopyinputstream and provides the decompressed data as a
// zerocopyinputstream.
//
// gzipoutputstream is an zerocopyoutputstream that compresses data to
// an underlying zerocopyoutputstream.

#ifndef google_protobuf_io_gzip_stream_h__
#define google_protobuf_io_gzip_stream_h__

#include <zlib.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/zero_copy_stream.h>

namespace google {
namespace protobuf {
namespace io {

// a zerocopyinputstream that reads compressed data through zlib
class libprotobuf_export gzipinputstream : public zerocopyinputstream {
 public:
  // format key for constructor
  enum format {
    // zlib will autodetect gzip header or deflate stream
    auto = 0,

    // gzip streams have some extra header data for file attributes.
    gzip = 1,

    // simpler zlib stream format.
    zlib = 2,
  };

  // buffer_size and format may be -1 for default of 64kb and gzip format
  explicit gzipinputstream(
      zerocopyinputstream* sub_stream,
      format format = auto,
      int buffer_size = -1);
  virtual ~gzipinputstream();

  // return last error message or null if no error.
  inline const char* zliberrormessage() const {
    return zcontext_.msg;
  }
  inline int zliberrorcode() const {
    return zerror_;
  }

  // implements zerocopyinputstream ----------------------------------
  bool next(const void** data, int* size);
  void backup(int count);
  bool skip(int count);
  int64 bytecount() const;

 private:
  format format_;

  zerocopyinputstream* sub_stream_;

  z_stream zcontext_;
  int zerror_;

  void* output_buffer_;
  void* output_position_;
  size_t output_buffer_length_;

  int inflate(int flush);
  void donextoutput(const void** data, int* size);

  google_disallow_evil_constructors(gzipinputstream);
};


class libprotobuf_export gzipoutputstream : public zerocopyoutputstream {
 public:
  // format key for constructor
  enum format {
    // gzip streams have some extra header data for file attributes.
    gzip = 1,

    // simpler zlib stream format.
    zlib = 2,
  };

  struct libprotobuf_export options {
    // defaults to gzip.
    format format;

    // what size buffer to use internally.  defaults to 64kb.
    int buffer_size;

    // a number between 0 and 9, where 0 is no compression and 9 is best
    // compression.  defaults to z_default_compression (see zlib.h).
    int compression_level;

    // defaults to z_default_strategy.  can also be set to z_filtered,
    // z_huffman_only, or z_rle.  see the documentation for deflateinit2 in
    // zlib.h for definitions of these constants.
    int compression_strategy;

    options();  // initializes with default values.
  };

  // create a gzipoutputstream with default options.
  explicit gzipoutputstream(zerocopyoutputstream* sub_stream);

  // create a gzipoutputstream with the given options.
  gzipoutputstream(
      zerocopyoutputstream* sub_stream,
      const options& options);

  virtual ~gzipoutputstream();

  // return last error message or null if no error.
  inline const char* zliberrormessage() const {
    return zcontext_.msg;
  }
  inline int zliberrorcode() const {
    return zerror_;
  }

  // flushes data written so far to zipped data in the underlying stream.
  // it is the caller's responsibility to flush the underlying stream if
  // necessary.
  // compression may be less efficient stopping and starting around flushes.
  // returns true if no error.
  //
  // please ensure that block size is > 6. here is an excerpt from the zlib
  // doc that explains why:
  //
  // in the case of a z_full_flush or z_sync_flush, make sure that avail_out
  // is greater than six to avoid repeated flush markers due to
  // avail_out == 0 on return.
  bool flush();

  // writes out all data and closes the gzip stream.
  // it is the caller's responsibility to close the underlying stream if
  // necessary.
  // returns true if no error.
  bool close();

  // implements zerocopyoutputstream ---------------------------------
  bool next(void** data, int* size);
  void backup(int count);
  int64 bytecount() const;

 private:
  zerocopyoutputstream* sub_stream_;
  // result from calling next() on sub_stream_
  void* sub_data_;
  int sub_data_size_;

  z_stream zcontext_;
  int zerror_;
  void* input_buffer_;
  size_t input_buffer_length_;

  // shared constructor code.
  void init(zerocopyoutputstream* sub_stream, const options& options);

  // do some compression.
  // takes zlib flush mode.
  // returns zlib error code.
  int deflate(int flush);

  google_disallow_evil_constructors(gzipoutputstream);
};

}  // namespace io
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_io_gzip_stream_h__
