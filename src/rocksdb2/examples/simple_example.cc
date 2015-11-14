// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
#include <cstdio>
#include <string>

#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/options.h"

using namespace rocksdb;

std::string kdbpath = "/tmp/rocksdb_simple_example";

int main() {
  db* db;
  options options;
  // optimize rocksdb. this is the easiest way to get rocksdb to perform well
  options.increaseparallelism();
  options.optimizelevelstylecompaction();
  // create the db if it's not already present
  options.create_if_missing = true;

  // open db
  status s = db::open(options, kdbpath, &db);
  assert(s.ok());

  // put key-value
  s = db->put(writeoptions(), "key", "value");
  assert(s.ok());
  std::string value;
  // get value
  s = db->get(readoptions(), "key", &value);
  assert(s.ok());
  assert(value == "value");

  delete db;

  return 0;
}
