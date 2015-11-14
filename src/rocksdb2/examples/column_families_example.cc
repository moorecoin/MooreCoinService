// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
#include <cstdio>
#include <string>
#include <vector>

#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/options.h"

using namespace rocksdb;

std::string kdbpath = "/tmp/rocksdb_column_families_example";

int main() {
  // open db
  options options;
  options.create_if_missing = true;
  db* db;
  status s = db::open(options, kdbpath, &db);
  assert(s.ok());

  // create column family
  columnfamilyhandle* cf;
  s = db->createcolumnfamily(columnfamilyoptions(), "new_cf", &cf);
  assert(s.ok());

  // close db
  delete cf;
  delete db;

  // open db with two column families
  std::vector<columnfamilydescriptor> column_families;
  // have to open default column familiy
  column_families.push_back(columnfamilydescriptor(
      kdefaultcolumnfamilyname, columnfamilyoptions()));
  // open the new one, too
  column_families.push_back(columnfamilydescriptor(
      "new_cf", columnfamilyoptions()));
  std::vector<columnfamilyhandle*> handles;
  s = db::open(dboptions(), kdbpath, column_families, &handles, &db);
  assert(s.ok());

  // put and get from non-default column family
  s = db->put(writeoptions(), handles[1], slice("key"), slice("value"));
  assert(s.ok());
  std::string value;
  s = db->get(readoptions(), handles[1], slice("key"), &value);
  assert(s.ok());

  // atomic write
  writebatch batch;
  batch.put(handles[0], slice("key2"), slice("value2"));
  batch.put(handles[1], slice("key3"), slice("value3"));
  batch.delete(handles[0], slice("key"));
  s = db->write(writeoptions(), &batch);
  assert(s.ok());

  // drop column family
  s = db->dropcolumnfamily(handles[1]);
  assert(s.ok());

  // close db
  for (auto handle : handles) {
    delete handle;
  }
  delete db;

  return 0;
}
