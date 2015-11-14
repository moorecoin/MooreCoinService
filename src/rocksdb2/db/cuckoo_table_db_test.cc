//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include "db/db_impl.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "table/meta_blocks.h"
#include "table/cuckoo_table_factory.h"
#include "table/cuckoo_table_reader.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

class cuckootabledbtest {
 private:
  std::string dbname_;
  env* env_;
  db* db_;

 public:
  cuckootabledbtest() : env_(env::default()) {
    dbname_ = test::tmpdir() + "/cuckoo_table_db_test";
    assert_ok(destroydb(dbname_, options()));
    db_ = nullptr;
    reopen();
  }

  ~cuckootabledbtest() {
    delete db_;
    assert_ok(destroydb(dbname_, options()));
  }

  options currentoptions() {
    options options;
    options.table_factory.reset(newcuckootablefactory());
    options.memtable_factory.reset(newhashlinklistrepfactory(4, 0, 3, true));
    options.allow_mmap_reads = true;
    options.create_if_missing = true;
    options.max_mem_compaction_level = 0;
    return options;
  }

  dbimpl* dbfull() {
    return reinterpret_cast<dbimpl*>(db_);
  }

  // the following util methods are copied from plain_table_db_test.
  void reopen(options* options = nullptr) {
    delete db_;
    db_ = nullptr;
    options opts;
    if (options != nullptr) {
      opts = *options;
    } else {
      opts = currentoptions();
      opts.create_if_missing = true;
    }
    assert_ok(db::open(opts, dbname_, &db_));
  }

  status put(const slice& k, const slice& v) {
    return db_->put(writeoptions(), k, v);
  }

  status delete(const std::string& k) {
    return db_->delete(writeoptions(), k);
  }

  std::string get(const std::string& k) {
    readoptions options;
    std::string result;
    status s = db_->get(options, k, &result);
    if (s.isnotfound()) {
      result = "not_found";
    } else if (!s.ok()) {
      result = s.tostring();
    }
    return result;
  }

  int numtablefilesatlevel(int level) {
    std::string property;
    assert_true(
        db_->getproperty("rocksdb.num-files-at-level" + numbertostring(level),
                         &property));
    return atoi(property.c_str());
  }

  // return spread of files per level
  std::string filesperlevel() {
    std::string result;
    int last_non_zero_offset = 0;
    for (int level = 0; level < db_->numberlevels(); level++) {
      int f = numtablefilesatlevel(level);
      char buf[100];
      snprintf(buf, sizeof(buf), "%s%d", (level ? "," : ""), f);
      result += buf;
      if (f > 0) {
        last_non_zero_offset = result.size();
      }
    }
    result.resize(last_non_zero_offset);
    return result;
  }
};

test(cuckootabledbtest, flush) {
  // try with empty db first.
  assert_true(dbfull() != nullptr);
  assert_eq("not_found", get("key2"));

  // add some values to db.
  options options = currentoptions();
  reopen(&options);

  assert_ok(put("key1", "v1"));
  assert_ok(put("key2", "v2"));
  assert_ok(put("key3", "v3"));
  dbfull()->test_flushmemtable();

  tablepropertiescollection ptc;
  reinterpret_cast<db*>(dbfull())->getpropertiesofalltables(&ptc);
  assert_eq(1u, ptc.size());
  assert_eq(3u, ptc.begin()->second->num_entries);
  assert_eq("1", filesperlevel());

  assert_eq("v1", get("key1"));
  assert_eq("v2", get("key2"));
  assert_eq("v3", get("key3"));
  assert_eq("not_found", get("key4"));

  // now add more keys and flush.
  assert_ok(put("key4", "v4"));
  assert_ok(put("key5", "v5"));
  assert_ok(put("key6", "v6"));
  dbfull()->test_flushmemtable();

  reinterpret_cast<db*>(dbfull())->getpropertiesofalltables(&ptc);
  assert_eq(2u, ptc.size());
  auto row = ptc.begin();
  assert_eq(3u, row->second->num_entries);
  assert_eq(3u, (++row)->second->num_entries);
  assert_eq("2", filesperlevel());
  assert_eq("v1", get("key1"));
  assert_eq("v2", get("key2"));
  assert_eq("v3", get("key3"));
  assert_eq("v4", get("key4"));
  assert_eq("v5", get("key5"));
  assert_eq("v6", get("key6"));

  assert_ok(delete("key6"));
  assert_ok(delete("key5"));
  assert_ok(delete("key4"));
  dbfull()->test_flushmemtable();
  reinterpret_cast<db*>(dbfull())->getpropertiesofalltables(&ptc);
  assert_eq(3u, ptc.size());
  row = ptc.begin();
  assert_eq(3u, row->second->num_entries);
  assert_eq(3u, (++row)->second->num_entries);
  assert_eq(3u, (++row)->second->num_entries);
  assert_eq("3", filesperlevel());
  assert_eq("v1", get("key1"));
  assert_eq("v2", get("key2"));
  assert_eq("v3", get("key3"));
  assert_eq("not_found", get("key4"));
  assert_eq("not_found", get("key5"));
  assert_eq("not_found", get("key6"));
}

test(cuckootabledbtest, flushwithduplicatekeys) {
  options options = currentoptions();
  reopen(&options);
  assert_ok(put("key1", "v1"));
  assert_ok(put("key2", "v2"));
  assert_ok(put("key1", "v3"));  // duplicate
  dbfull()->test_flushmemtable();

  tablepropertiescollection ptc;
  reinterpret_cast<db*>(dbfull())->getpropertiesofalltables(&ptc);
  assert_eq(1u, ptc.size());
  assert_eq(2u, ptc.begin()->second->num_entries);
  assert_eq("1", filesperlevel());
  assert_eq("v3", get("key1"));
  assert_eq("v2", get("key2"));
}

namespace {
static std::string key(int i) {
  char buf[100];
  snprintf(buf, sizeof(buf), "key_______%06d", i);
  return std::string(buf);
}
static std::string uint64key(uint64_t i) {
  std::string str;
  str.resize(8);
  memcpy(&str[0], static_cast<void*>(&i), 8);
  return str;
}
}  // namespace.

test(cuckootabledbtest, uint64comparator) {
  options options = currentoptions();
  options.comparator = test::uint64comparator();
  reopen(&options);

  assert_ok(put(uint64key(1), "v1"));
  assert_ok(put(uint64key(2), "v2"));
  assert_ok(put(uint64key(3), "v3"));
  dbfull()->test_flushmemtable();

  assert_eq("v1", get(uint64key(1)));
  assert_eq("v2", get(uint64key(2)));
  assert_eq("v3", get(uint64key(3)));
  assert_eq("not_found", get(uint64key(4)));

  // add more keys.
  assert_ok(delete(uint64key(2)));  // delete.
  assert_ok(put(uint64key(3), "v0"));  // update.
  assert_ok(put(uint64key(4), "v4"));
  dbfull()->test_flushmemtable();
  assert_eq("v1", get(uint64key(1)));
  assert_eq("not_found", get(uint64key(2)));
  assert_eq("v0", get(uint64key(3)));
  assert_eq("v4", get(uint64key(4)));
}

test(cuckootabledbtest, compactiontrigger) {
  options options = currentoptions();
  options.write_buffer_size = 100 << 10;  // 100kb
  options.level0_file_num_compaction_trigger = 2;
  reopen(&options);

  // write 11 values, each 10016 b
  for (int idx = 0; idx < 11; ++idx) {
    assert_ok(put(key(idx), std::string(10000, 'a' + idx)));
  }
  dbfull()->test_waitforflushmemtable();
  assert_eq("1", filesperlevel());

  // generate one more file in level-0, and should trigger level-0 compaction
  for (int idx = 11; idx < 22; ++idx) {
    assert_ok(put(key(idx), std::string(10000, 'a' + idx)));
  }
  dbfull()->test_waitforflushmemtable();
  dbfull()->test_compactrange(0, nullptr, nullptr);

  assert_eq("0,2", filesperlevel());
  for (int idx = 0; idx < 22; ++idx) {
    assert_eq(std::string(10000, 'a' + idx), get(key(idx)));
  }
}

test(cuckootabledbtest, samekeyinsertedintwodifferentfilesandcompacted) {
  // insert same key twice so that they go to different sst files. then wait for
  // compaction and check if the latest value is stored and old value removed.
  options options = currentoptions();
  options.write_buffer_size = 100 << 10;  // 100kb
  options.level0_file_num_compaction_trigger = 2;
  reopen(&options);

  // write 11 values, each 10016 b
  for (int idx = 0; idx < 11; ++idx) {
    assert_ok(put(key(idx), std::string(10000, 'a')));
  }
  dbfull()->test_waitforflushmemtable();
  assert_eq("1", filesperlevel());

  // generate one more file in level-0, and should trigger level-0 compaction
  for (int idx = 0; idx < 11; ++idx) {
    assert_ok(put(key(idx), std::string(10000, 'a' + idx)));
  }
  dbfull()->test_waitforflushmemtable();
  dbfull()->test_compactrange(0, nullptr, nullptr);

  assert_eq("0,1", filesperlevel());
  for (int idx = 0; idx < 11; ++idx) {
    assert_eq(std::string(10000, 'a' + idx), get(key(idx)));
  }
}

test(cuckootabledbtest, adaptivetable) {
  options options = currentoptions();

  // write some keys using cuckoo table.
  options.table_factory.reset(newcuckootablefactory());
  reopen(&options);

  assert_ok(put("key1", "v1"));
  assert_ok(put("key2", "v2"));
  assert_ok(put("key3", "v3"));
  dbfull()->test_flushmemtable();

  // write some keys using plain table.
  options.create_if_missing = false;
  options.table_factory.reset(newplaintablefactory());
  reopen(&options);
  assert_ok(put("key4", "v4"));
  assert_ok(put("key1", "v5"));
  dbfull()->test_flushmemtable();

  // write some keys using block based table.
  std::shared_ptr<tablefactory> block_based_factory(
      newblockbasedtablefactory());
  options.table_factory.reset(newadaptivetablefactory(block_based_factory));
  reopen(&options);
  assert_ok(put("key5", "v6"));
  assert_ok(put("key2", "v7"));
  dbfull()->test_flushmemtable();

  assert_eq("v5", get("key1"));
  assert_eq("v7", get("key2"));
  assert_eq("v3", get("key3"));
  assert_eq("v4", get("key4"));
  assert_eq("v6", get("key5"));
}
}  // namespace rocksdb

int main(int argc, char** argv) { return rocksdb::test::runalltests(); }
