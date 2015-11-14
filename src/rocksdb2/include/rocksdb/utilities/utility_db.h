// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#ifndef rocksdb_lite
#include <vector>
#include <string>

#include "rocksdb/utilities/stackable_db.h"
#include "rocksdb/utilities/db_ttl.h"
#include "rocksdb/db.h"

namespace rocksdb {

// please don't use this class. it's deprecated
class utilitydb {
 public:
  // this function is here only for backwards compatibility. please use the
  // functions defined in dbwithttl (rocksdb/utilities/db_ttl.h)
  // (deprecated)
  __attribute__((deprecated)) static status openttldb(const options& options,
                                                      const std::string& name,
                                                      stackabledb** dbptr,
                                                      int32_t ttl = 0,
                                                      bool read_only = false);
};

} //  namespace rocksdb
#endif  // rocksdb_lite
