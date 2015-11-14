//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include "rocksdb/db.h"
#include "db/db_impl.h"
#include "db/version_set.h"
#include "util/logging.h"
#include "util/testutil.h"
#include "util/testharness.h"
#include "util/ldb_cmd.h"

namespace rocksdb {

class reduceleveltest {
public:
  reduceleveltest() {
    dbname_ = test::tmpdir() + "/db_reduce_levels_test";
    destroydb(dbname_, options());
    db_ = nullptr;
  }

  status opendb(bool create_if_missing, int levels,
      int mem_table_compact_level);

  status put(const std::string& k, const std::string& v) {
    return db_->put(writeoptions(), k, v);
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

  status compactmemtable() {
    if (db_ == nullptr) {
      return status::invalidargument("db not opened.");
    }
    dbimpl* db_impl = reinterpret_cast<dbimpl*>(db_);
    return db_impl->test_flushmemtable();
  }

  void closedb() {
    if (db_ != nullptr) {
      delete db_;
      db_ = nullptr;
    }
  }

  bool reducelevels(int target_level);

  int filesonlevel(int level) {
    std::string property;
    assert_true(
        db_->getproperty("rocksdb.num-files-at-level" + numbertostring(level),
                         &property));
    return atoi(property.c_str());
  }

private:
  std::string dbname_;
  db* db_;
};

status reduceleveltest::opendb(bool create_if_missing, int num_levels,
    int mem_table_compact_level) {
  rocksdb::options opt;
  opt.num_levels = num_levels;
  opt.create_if_missing = create_if_missing;
  opt.max_mem_compaction_level = mem_table_compact_level;
  rocksdb::status st = rocksdb::db::open(opt, dbname_, &db_);
  if (!st.ok()) {
    fprintf(stderr, "can't open the db:%s\n", st.tostring().c_str());
  }
  return st;
}

bool reduceleveltest::reducelevels(int target_level) {
  std::vector<std::string> args = rocksdb::reducedblevelscommand::prepareargs(
      dbname_, target_level, false);
  ldbcommand* level_reducer = ldbcommand::initfromcmdlineargs(
      args, options(), ldboptions());
  level_reducer->run();
  bool is_succeed = level_reducer->getexecutestate().issucceed();
  delete level_reducer;
  return is_succeed;
}

test(reduceleveltest, last_level) {
  // create files on all levels;
  assert_ok(opendb(true, 4, 3));
  assert_ok(put("aaaa", "11111"));
  assert_ok(compactmemtable());
  assert_eq(filesonlevel(3), 1);
  closedb();

  assert_true(reducelevels(3));
  assert_ok(opendb(true, 3, 1));
  assert_eq(filesonlevel(2), 1);
  closedb();

  assert_true(reducelevels(2));
  assert_ok(opendb(true, 2, 1));
  assert_eq(filesonlevel(1), 1);
  closedb();
}

test(reduceleveltest, top_level) {
  // create files on all levels;
  assert_ok(opendb(true, 5, 0));
  assert_ok(put("aaaa", "11111"));
  assert_ok(compactmemtable());
  assert_eq(filesonlevel(0), 1);
  closedb();

  assert_true(reducelevels(4));
  assert_ok(opendb(true, 4, 0));
  closedb();

  assert_true(reducelevels(3));
  assert_ok(opendb(true, 3, 0));
  closedb();

  assert_true(reducelevels(2));
  assert_ok(opendb(true, 2, 0));
  closedb();
}

test(reduceleveltest, all_levels) {
  // create files on all levels;
  assert_ok(opendb(true, 5, 1));
  assert_ok(put("a", "a11111"));
  assert_ok(compactmemtable());
  assert_eq(filesonlevel(1), 1);
  closedb();

  assert_ok(opendb(true, 5, 2));
  assert_ok(put("b", "b11111"));
  assert_ok(compactmemtable());
  assert_eq(filesonlevel(1), 1);
  assert_eq(filesonlevel(2), 1);
  closedb();

  assert_ok(opendb(true, 5, 3));
  assert_ok(put("c", "c11111"));
  assert_ok(compactmemtable());
  assert_eq(filesonlevel(1), 1);
  assert_eq(filesonlevel(2), 1);
  assert_eq(filesonlevel(3), 1);
  closedb();

  assert_ok(opendb(true, 5, 4));
  assert_ok(put("d", "d11111"));
  assert_ok(compactmemtable());
  assert_eq(filesonlevel(1), 1);
  assert_eq(filesonlevel(2), 1);
  assert_eq(filesonlevel(3), 1);
  assert_eq(filesonlevel(4), 1);
  closedb();

  assert_true(reducelevels(4));
  assert_ok(opendb(true, 4, 0));
  assert_eq("a11111", get("a"));
  assert_eq("b11111", get("b"));
  assert_eq("c11111", get("c"));
  assert_eq("d11111", get("d"));
  closedb();

  assert_true(reducelevels(3));
  assert_ok(opendb(true, 3, 0));
  assert_eq("a11111", get("a"));
  assert_eq("b11111", get("b"));
  assert_eq("c11111", get("c"));
  assert_eq("d11111", get("d"));
  closedb();

  assert_true(reducelevels(2));
  assert_ok(opendb(true, 2, 0));
  assert_eq("a11111", get("a"));
  assert_eq("b11111", get("b"));
  assert_eq("c11111", get("c"));
  assert_eq("d11111", get("d"));
  closedb();
}

}

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
