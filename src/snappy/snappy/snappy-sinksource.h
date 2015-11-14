// copyright 2011 google inc. all rights reserved.
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

#ifndef util_snappy_snappy_sinksource_h_
#define util_snappy_snappy_sinksource_h_

#include <stddef.h>


namespace snappy {

// a sink is an interface that consumes a sequence of bytes.
class sink {
 public:
  sink() { }
  virtual ~sink();

  // append "bytes[0,n-1]" to this.
  virtual void append(const char* bytes, size_t n) = 0;

  // returns a writable buffer of the specified length for appending.
  // may return a pointer to the caller-owned scratch buffer which
  // must have at least the indicated length.  the returned buffer is
  // only valid until the next operation on this sink.
  //
  // after writing at most "length" bytes, call append() with the
  // pointer returned from this function and the number of bytes
  // written.  many append() implementations will avoid copying
  // bytes if this function returned an internal buffer.
  //
  // if a non-scratch buffer is returned, the caller may only pass a
  // prefix of it to append().  that is, it is not correct to pass an
  // interior pointer of the returned array to append().
  //
  // the default implementation always returns the scratch buffer.
  virtual char* getappendbuffer(size_t length, char* scratch);


 private:
  // no copying
  sink(const sink&);
  void operator=(const sink&);
};

// a source is an interface that yields a sequence of bytes
class source {
 public:
  source() { }
  virtual ~source();

  // return the number of bytes left to read from the source
  virtual size_t available() const = 0;

  // peek at the next flat region of the source.  does not reposition
  // the source.  the returned region is empty iff available()==0.
  //
  // returns a pointer to the beginning of the region and store its
  // length in *len.
  //
  // the returned region is valid until the next call to skip() or
  // until this object is destroyed, whichever occurs first.
  //
  // the returned region may be larger than available() (for example
  // if this bytesource is a view on a substring of a larger source).
  // the caller is responsible for ensuring that it only reads the
  // available() bytes.
  virtual const char* peek(size_t* len) = 0;

  // skip the next n bytes.  invalidates any buffer returned by
  // a previous call to peek().
  // requires: available() >= n
  virtual void skip(size_t n) = 0;

 private:
  // no copying
  source(const source&);
  void operator=(const source&);
};

// a source implementation that yields the contents of a flat array
class bytearraysource : public source {
 public:
  bytearraysource(const char* p, size_t n) : ptr_(p), left_(n) { }
  virtual ~bytearraysource();
  virtual size_t available() const;
  virtual const char* peek(size_t* len);
  virtual void skip(size_t n);
 private:
  const char* ptr_;
  size_t left_;
};

// a sink implementation that writes to a flat array without any bound checks.
class uncheckedbytearraysink : public sink {
 public:
  explicit uncheckedbytearraysink(char* dest) : dest_(dest) { }
  virtual ~uncheckedbytearraysink();
  virtual void append(const char* data, size_t n);
  virtual char* getappendbuffer(size_t len, char* scratch);

  // return the current output pointer so that a caller can see how
  // many bytes were produced.
  // note: this is not a sink method.
  char* currentdestination() const { return dest_; }
 private:
  char* dest_;
};


}

#endif  // util_snappy_snappy_sinksource_h_
