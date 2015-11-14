// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
#ifndef rocksdb_lite
#pragma once
#include <string>
#include "rocksdb/options.h"

namespace rocksdb {

// an interface for converting a slice to a readable string
class sliceformatter {
 public:
  virtual ~sliceformatter() {}
  virtual std::string format(const slice& s) const = 0;
};

// options for customizing ldb tool (beyond the db options)
struct ldboptions {
  // create ldboptions with default values for all fields
  ldboptions();

  // key formatter that converts a slice to a readable string.
  // default: slice::tostring()
  std::shared_ptr<sliceformatter> key_formatter;
};

class ldbtool {
 public:
  void run(int argc, char** argv, options db_options= options(),
           const ldboptions& ldb_options = ldboptions());
};

} // namespace rocksdb

#endif  // rocksdb_lite
