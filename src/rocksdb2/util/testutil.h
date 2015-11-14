//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include <string>
#include "db/dbformat.h"
#include "rocksdb/env.h"
#include "rocksdb/slice.h"
#include "util/random.h"

namespace rocksdb {
namespace test {

// store in *dst a random string of length "len" and return a slice that
// references the generated data.
extern slice randomstring(random* rnd, int len, std::string* dst);

// return a random key with the specified length that may contain interesting
// characters (e.g. \x00, \xff, etc.).
extern std::string randomkey(random* rnd, int len);

// store in *dst a string of length "len" that will compress to
// "n*compressed_fraction" bytes and return a slice that references
// the generated data.
extern slice compressiblestring(random* rnd, double compressed_fraction,
                                int len, std::string* dst);

// a wrapper that allows injection of errors.
class errorenv : public envwrapper {
 public:
  bool writable_file_error_;
  int num_writable_file_errors_;

  errorenv() : envwrapper(env::default()),
               writable_file_error_(false),
               num_writable_file_errors_(0) { }

  virtual status newwritablefile(const std::string& fname,
                                 unique_ptr<writablefile>* result,
                                 const envoptions& soptions) {
    result->reset();
    if (writable_file_error_) {
      ++num_writable_file_errors_;
      return status::ioerror(fname, "fake error");
    }
    return target()->newwritablefile(fname, result, soptions);
  }
};

// an internal comparator that just forward comparing results from the
// user comparator in it. can be used to test entities that have no dependency
// on internal key structure but consumes internalkeycomparator, like
// blockbasedtable.
class plaininternalkeycomparator : public internalkeycomparator {
 public:
  explicit plaininternalkeycomparator(const comparator* c)
      : internalkeycomparator(c) {}

  virtual ~plaininternalkeycomparator() {}

  virtual int compare(const slice& a, const slice& b) const override {
    return user_comparator()->compare(a, b);
  }
  virtual void findshortestseparator(std::string* start,
                                     const slice& limit) const override {
    user_comparator()->findshortestseparator(start, limit);
  }
  virtual void findshortsuccessor(std::string* key) const override {
    user_comparator()->findshortsuccessor(key);
  }
};

// returns a user key comparator that can be used for comparing two uint64_t
// slices. instead of comparing slices byte-wise, it compares all the 8 bytes
// at once. assumes same endian-ness is used though the database's lifetime.
// symantics of comparison would differ from bytewise comparator in little
// endian machines.
extern const comparator* uint64comparator();

}  // namespace test
}  // namespace rocksdb
