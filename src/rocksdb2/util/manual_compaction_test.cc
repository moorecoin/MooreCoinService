//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// test for issue 178: a manual compaction causes deleted data to reappear.
#include <iostream>
#include <sstream>
#include <cstdlib>

#include "rocksdb/db.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/slice.h"
#include "rocksdb/write_batch.h"
#include "util/testharness.h"

using namespace rocksdb;

namespace {

const int knumkeys = 1100000;

std::string key1(int i) {
  char buf[100];
  snprintf(buf, sizeof(buf), "my_key_%d", i);
  return buf;
}

std::string key2(int i) {
  return key1(i) + "_xxx";
}

class manualcompactiontest {
 public:
  manualcompactiontest() {
    // get rid of any state from an old run.
    dbname_ = rocksdb::test::tmpdir() + "/rocksdb_cbug_test";
    destroydb(dbname_, rocksdb::options());
  }

  std::string dbname_;
};

class destroyallcompactionfilter : public compactionfilter {
 public:
  destroyallcompactionfilter() {}

  virtual bool filter(int level,
                      const slice& key,
                      const slice& existing_value,
                      std::string* new_value,
                      bool* value_changed) const {
    return existing_value.tostring() == "destroy";
  }

  virtual const char* name() const {
    return "destroyallcompactionfilter";
  }
};

test(manualcompactiontest, compacttouchesallkeys) {
  for (int iter = 0; iter < 2; ++iter) {
    db* db;
    options options;
    if (iter == 0) { // level compaction
      options.num_levels = 3;
      options.compaction_style = kcompactionstylelevel;
    } else { // universal compaction
      options.compaction_style = kcompactionstyleuniversal;
    }
    options.create_if_missing = true;
    options.compression = rocksdb::knocompression;
    options.compaction_filter = new destroyallcompactionfilter();
    assert_ok(db::open(options, dbname_, &db));

    db->put(writeoptions(), slice("key1"), slice("destroy"));
    db->put(writeoptions(), slice("key2"), slice("destroy"));
    db->put(writeoptions(), slice("key3"), slice("value3"));
    db->put(writeoptions(), slice("key4"), slice("destroy"));

    slice key4("key4");
    db->compactrange(nullptr, &key4);
    iterator* itr = db->newiterator(readoptions());
    itr->seektofirst();
    assert_true(itr->valid());
    assert_eq("key3", itr->key().tostring());
    itr->next();
    assert_true(!itr->valid());
    delete itr;

    delete options.compaction_filter;
    delete db;
    destroydb(dbname_, options);
  }
}

test(manualcompactiontest, test) {

  // open database.  disable compression since it affects the creation
  // of layers and the code below is trying to test against a very
  // specific scenario.
  rocksdb::db* db;
  rocksdb::options db_options;
  db_options.create_if_missing = true;
  db_options.compression = rocksdb::knocompression;
  assert_ok(rocksdb::db::open(db_options, dbname_, &db));

  // create first key range
  rocksdb::writebatch batch;
  for (int i = 0; i < knumkeys; i++) {
    batch.put(key1(i), "value for range 1 key");
  }
  assert_ok(db->write(rocksdb::writeoptions(), &batch));

  // create second key range
  batch.clear();
  for (int i = 0; i < knumkeys; i++) {
    batch.put(key2(i), "value for range 2 key");
  }
  assert_ok(db->write(rocksdb::writeoptions(), &batch));

  // delete second key range
  batch.clear();
  for (int i = 0; i < knumkeys; i++) {
    batch.delete(key2(i));
  }
  assert_ok(db->write(rocksdb::writeoptions(), &batch));

  // compact database
  std::string start_key = key1(0);
  std::string end_key = key1(knumkeys - 1);
  rocksdb::slice least(start_key.data(), start_key.size());
  rocksdb::slice greatest(end_key.data(), end_key.size());

  // commenting out the line below causes the example to work correctly
  db->compactrange(&least, &greatest);

  // count the keys
  rocksdb::iterator* iter = db->newiterator(rocksdb::readoptions());
  int num_keys = 0;
  for (iter->seektofirst(); iter->valid(); iter->next()) {
    num_keys++;
  }
  delete iter;
  assert_eq(knumkeys, num_keys) << "bad number of keys";

  // close database
  delete db;
  destroydb(dbname_, rocksdb::options());
}

}  // anonymous namespace

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
