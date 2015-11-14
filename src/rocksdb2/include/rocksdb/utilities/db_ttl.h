//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#pragma once
#ifndef rocksdb_lite

#include <string>
#include <vector>

#include "rocksdb/utilities/stackable_db.h"
#include "rocksdb/db.h"

namespace rocksdb {

// database with ttl support.
//
// use-cases:
// this api should be used to open the db when key-values inserted are
//  meant to be removed from the db in a non-strict 'ttl' amount of time
//  therefore, this guarantees that key-values inserted will remain in the
//  db for >= ttl amount of time and the db will make efforts to remove the
//  key-values as soon as possible after ttl seconds of their insertion.
//
// behaviour:
// ttl is accepted in seconds
// (int32_t)timestamp(creation) is suffixed to values in put internally
// expired ttl values deleted in compaction only:(timestamp+ttl<time_now)
// get/iterator may return expired entries(compaction not run on them yet)
// different ttl may be used during different opens
// example: open1 at t=0 with ttl=4 and insert k1,k2, close at t=2
//          open2 at t=3 with ttl=5. now k1,k2 should be deleted at t>=5
// read_only=true opens in the usual read-only mode. compactions will not be
//  triggered(neither manual nor automatic), so no expired entries removed
//
// constraints:
// not specifying/passing or non-positive ttl behaves like ttl = infinity
//
// !!!warning!!!:
// calling db::open directly to re-open a db created by this api will get
//  corrupt values(timestamp suffixed) and no ttl effect will be there
//  during the second open, so use this api consistently to open the db
// be careful when passing ttl with a small positive value because the
//  whole database may be deleted in a small amount of time

class dbwithttl : public stackabledb {
 public:
  virtual status createcolumnfamilywithttl(
      const columnfamilyoptions& options, const std::string& column_family_name,
      columnfamilyhandle** handle, int ttl) = 0;

  static status open(const options& options, const std::string& dbname,
                     dbwithttl** dbptr, int32_t ttl = 0,
                     bool read_only = false);

  static status open(const dboptions& db_options, const std::string& dbname,
                     const std::vector<columnfamilydescriptor>& column_families,
                     std::vector<columnfamilyhandle*>* handles,
                     dbwithttl** dbptr, std::vector<int32_t> ttls,
                     bool read_only = false);

 protected:
  explicit dbwithttl(db* db) : stackabledb(db) {}
};

}  // namespace rocksdb
#endif  // rocksdb_lite
