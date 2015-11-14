// test for issue 200: iterator reversal brings in keys newer than snapshots
#include <iostream>
#include <sstream>
#include <cstdlib>

#include "hyperleveldb/db.h"
#include "hyperleveldb/write_batch.h"
#include "util/testharness.h"

namespace {

class issue200 { };

test(issue200, test) {
  // get rid of any state from an old run.
  std::string dbpath = leveldb::test::tmpdir() + "/leveldb_200_iterator_test";
  destroydb(dbpath, leveldb::options());

  // open database.
  leveldb::db* db;
  leveldb::options db_options;
  db_options.create_if_missing = true;
  db_options.compression = leveldb::knocompression;
  assert_ok(leveldb::db::open(db_options, dbpath, &db));

  leveldb::writeoptions write_options;
  db->put(write_options, leveldb::slice("1", 1), leveldb::slice("b", 1));
  db->put(write_options, leveldb::slice("2", 1), leveldb::slice("c", 1));
  db->put(write_options, leveldb::slice("3", 1), leveldb::slice("d", 1));
  db->put(write_options, leveldb::slice("4", 1), leveldb::slice("e", 1));
  db->put(write_options, leveldb::slice("5", 1), leveldb::slice("f", 1));

  const leveldb::snapshot *snapshot = db->getsnapshot();

  leveldb::readoptions read_options;
  read_options.snapshot = snapshot;
  leveldb::iterator *iter = db->newiterator(read_options);

  // commenting out this put changes the iterator behavior,
  // gives the expected behavior. this is unexpected because
  // the iterator is taken on a snapshot that was taken
  // before the put
  db->put(write_options, leveldb::slice("25", 2), leveldb::slice("cd", 2));

  iter->seek(leveldb::slice("5", 1));
  assert_eq("5", iter->key().tostring());
  iter->prev();
  assert_eq("4", iter->key().tostring());
  iter->prev();
  assert_eq("3", iter->key().tostring());

  // at this point the iterator is at "3", i expect the next() call will
  // move it to "4". but it stays on "3"
  iter->next();
  assert_eq("4", iter->key().tostring());
  iter->next();
  assert_eq("5", iter->key().tostring());

  // close database
  delete iter;
  db->releasesnapshot(snapshot);
  delete db;
  destroydb(dbpath, leveldb::options());
}

}  // anonymous namespace

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
