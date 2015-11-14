// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// slice is a simple structure containing a pointer into some external
// storage and a size.  the user of a slice must ensure that the slice
// is not used after the corresponding external storage has been
// deallocated.
//
// multiple threads can invoke const methods on a slice without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same slice must use
// external synchronization.

#ifndef storage_rocksdb_include_slice_h_
#define storage_rocksdb_include_slice_h_

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <string>

namespace rocksdb {

class slice {
 public:
  // create an empty slice.
  slice() : data_(""), size_(0) { }

  // create a slice that refers to d[0,n-1].
  slice(const char* d, size_t n) : data_(d), size_(n) { }

  // create a slice that refers to the contents of "s"
  /* implicit */
  slice(const std::string& s) : data_(s.data()), size_(s.size()) { }

  // create a slice that refers to s[0,strlen(s)-1]
  /* implicit */
  slice(const char* s) : data_(s), size_(strlen(s)) { }

  // return a pointer to the beginning of the referenced data
  const char* data() const { return data_; }

  // return the length (in bytes) of the referenced data
  size_t size() const { return size_; }

  // return true iff the length of the referenced data is zero
  bool empty() const { return size_ == 0; }

  // return the ith byte in the referenced data.
  // requires: n < size()
  char operator[](size_t n) const {
    assert(n < size());
    return data_[n];
  }

  // change this slice to refer to an empty array
  void clear() { data_ = ""; size_ = 0; }

  // drop the first "n" bytes from this slice.
  void remove_prefix(size_t n) {
    assert(n <= size());
    data_ += n;
    size_ -= n;
  }

  // return a string that contains the copy of the referenced data.
  std::string tostring(bool hex = false) const {
    if (hex) {
      std::string result;
      char buf[10];
      for (size_t i = 0; i < size_; i++) {
        snprintf(buf, 10, "%02x", (unsigned char)data_[i]);
        result += buf;
      }
      return result;
    } else {
      return std::string(data_, size_);
    }
  }

  // three-way comparison.  returns value:
  //   <  0 iff "*this" <  "b",
  //   == 0 iff "*this" == "b",
  //   >  0 iff "*this" >  "b"
  int compare(const slice& b) const;

  // return true iff "x" is a prefix of "*this"
  bool starts_with(const slice& x) const {
    return ((size_ >= x.size_) &&
            (memcmp(data_, x.data_, x.size_) == 0));
  }

 // private: make these public for rocksdbjni access
  const char* data_;
  size_t size_;

  // intentionally copyable
};

// a set of slices that are virtually concatenated together.  'parts' points
// to an array of slices.  the number of elements in the array is 'num_parts'.
struct sliceparts {
  sliceparts(const slice* _parts, int _num_parts) :
      parts(_parts), num_parts(_num_parts) { }
  sliceparts() : parts(nullptr), num_parts(0) {}

  const slice* parts;
  int num_parts;
};

inline bool operator==(const slice& x, const slice& y) {
  return ((x.size() == y.size()) &&
          (memcmp(x.data(), y.data(), x.size()) == 0));
}

inline bool operator!=(const slice& x, const slice& y) {
  return !(x == y);
}

inline int slice::compare(const slice& b) const {
  const int min_len = (size_ < b.size_) ? size_ : b.size_;
  int r = memcmp(data_, b.data_, min_len);
  if (r == 0) {
    if (size_ < b.size_) r = -1;
    else if (size_ > b.size_) r = +1;
  }
  return r;
}

}  // namespace rocksdb

#endif  // storage_rocksdb_include_slice_h_
