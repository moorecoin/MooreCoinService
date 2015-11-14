//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include <assert.h>
#include <memory>
#include <iostream>

#include "rocksdb/cache.h"
#include "rocksdb/comparator.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/merge_operator.h"
#include "rocksdb/utilities/db_ttl.h"
#include "db/dbformat.h"
#include "db/db_impl.h"
#include "db/write_batch_internal.h"
#include "utilities/merge_operators.h"
#include "util/testharness.h"

using namespace std;
using namespace rocksdb;

namespace {
  int nummergeoperatorcalls;
  void resetnummergeoperatorcalls() {
    nummergeoperatorcalls = 0;
  }

  int num_partial_merge_calls;
  void resetnumpartialmergecalls() {
    num_partial_merge_calls = 0;
  }
}

class countmergeoperator : public associativemergeoperator {
 public:
  countmergeoperator() {
    mergeoperator_ = mergeoperators::createuint64addoperator();
  }

  virtual bool merge(const slice& key,
                     const slice* existing_value,
                     const slice& value,
                     std::string* new_value,
                     logger* logger) const override {
    ++nummergeoperatorcalls;
    if (existing_value == nullptr) {
      new_value->assign(value.data(), value.size());
      return true;
    }

    return mergeoperator_->partialmerge(
        key,
        *existing_value,
        value,
        new_value,
        logger);
  }

  virtual bool partialmergemulti(const slice& key,
                                 const std::deque<slice>& operand_list,
                                 std::string* new_value, logger* logger) const {
    ++num_partial_merge_calls;
    return mergeoperator_->partialmergemulti(key, operand_list, new_value,
                                             logger);
  }

  virtual const char* name() const override {
    return "uint64addoperator";
  }

 private:
  std::shared_ptr<mergeoperator> mergeoperator_;
};

namespace {
std::shared_ptr<db> opendb(const string& dbname, const bool ttl = false,
                           const size_t max_successive_merges = 0,
                           const uint32_t min_partial_merge_operands = 2) {
  db* db;
  options options;
  options.create_if_missing = true;
  options.merge_operator = std::make_shared<countmergeoperator>();
  options.max_successive_merges = max_successive_merges;
  options.min_partial_merge_operands = min_partial_merge_operands;
  status s;
  destroydb(dbname, options());
  if (ttl) {
    cout << "opening database with ttl\n";
    dbwithttl* db_with_ttl;
    s = dbwithttl::open(options, dbname, &db_with_ttl);
    db = db_with_ttl;
  } else {
    s = db::open(options, dbname, &db);
  }
  if (!s.ok()) {
    cerr << s.tostring() << endl;
    assert(false);
  }
  return std::shared_ptr<db>(db);
}
}  // namespace

// imagine we are maintaining a set of uint64 counters.
// each counter has a distinct name. and we would like
// to support four high level operations:
// set, add, get and remove
// this is a quick implementation without a merge operation.
class counters {

 protected:
  std::shared_ptr<db> db_;

  writeoptions put_option_;
  readoptions get_option_;
  writeoptions delete_option_;

  uint64_t default_;

 public:
  explicit counters(std::shared_ptr<db> db, uint64_t defaultcount = 0)
      : db_(db),
        put_option_(),
        get_option_(),
        delete_option_(),
        default_(defaultcount) {
    assert(db_);
  }

  virtual ~counters() {}

  // public interface of counters.
  // all four functions return false
  // if the underlying level db operation failed.

  // mapped to a levedb put
  bool set(const string& key, uint64_t value) {
    // just treat the internal rep of int64 as the string
    slice slice((char *)&value, sizeof(value));
    auto s = db_->put(put_option_, key, slice);

    if (s.ok()) {
      return true;
    } else {
      cerr << s.tostring() << endl;
      return false;
    }
  }

  // mapped to a rocksdb delete
  bool remove(const string& key) {
    auto s = db_->delete(delete_option_, key);

    if (s.ok()) {
      return true;
    } else {
      cerr << s.tostring() << std::endl;
      return false;
    }
  }

  // mapped to a rocksdb get
  bool get(const string& key, uint64_t *value) {
    string str;
    auto s = db_->get(get_option_, key, &str);

    if (s.isnotfound()) {
      // return default value if not found;
      *value = default_;
      return true;
    } else if (s.ok()) {
      // deserialization
      if (str.size() != sizeof(uint64_t)) {
        cerr << "value corruption\n";
        return false;
      }
      *value = decodefixed64(&str[0]);
      return true;
    } else {
      cerr << s.tostring() << std::endl;
      return false;
    }
  }

  // 'add' is implemented as get -> modify -> set
  // an alternative is a single merge operation, see mergebasedcounters
  virtual bool add(const string& key, uint64_t value) {
    uint64_t base = default_;
    return get(key, &base) && set(key, base + value);
  }


  // convenience functions for testing
  void assert_set(const string& key, uint64_t value) {
    assert(set(key, value));
  }

  void assert_remove(const string& key) {
    assert(remove(key));
  }

  uint64_t assert_get(const string& key) {
    uint64_t value = default_;
    int result = get(key, &value);
    assert(result);
    if (result == 0) exit(1); // disable unused variable warning.
    return value;
  }

  void assert_add(const string& key, uint64_t value) {
    int result = add(key, value);
    assert(result);
    if (result == 0) exit(1); // disable unused variable warning.
  }
};

// implement 'add' directly with the new merge operation
class mergebasedcounters : public counters {
 private:
  writeoptions merge_option_; // for merge

 public:
  explicit mergebasedcounters(std::shared_ptr<db> db, uint64_t defaultcount = 0)
      : counters(db, defaultcount),
        merge_option_() {
  }

  // mapped to a rocksdb merge operation
  virtual bool add(const string& key, uint64_t value) override {
    char encoded[sizeof(uint64_t)];
    encodefixed64(encoded, value);
    slice slice(encoded, sizeof(uint64_t));
    auto s = db_->merge(merge_option_, key, slice);

    if (s.ok()) {
      return true;
    } else {
      cerr << s.tostring() << endl;
      return false;
    }
  }
};

namespace {
void dumpdb(db* db) {
  auto it = unique_ptr<iterator>(db->newiterator(readoptions()));
  for (it->seektofirst(); it->valid(); it->next()) {
    uint64_t value = decodefixed64(it->value().data());
    cout << it->key().tostring() << ": "  << value << endl;
  }
  assert(it->status().ok());  // check for any errors found during the scan
}

void testcounters(counters& counters, db* db, bool test_compaction) {

  flushoptions o;
  o.wait = true;

  counters.assert_set("a", 1);

  if (test_compaction) db->flush(o);

  assert(counters.assert_get("a") == 1);

  counters.assert_remove("b");

  // defaut value is 0 if non-existent
  assert(counters.assert_get("b") == 0);

  counters.assert_add("a", 2);

  if (test_compaction) db->flush(o);

  // 1+2 = 3
  assert(counters.assert_get("a")== 3);

  dumpdb(db);

  std::cout << "1\n";

  // 1+...+49 = ?
  uint64_t sum = 0;
  for (int i = 1; i < 50; i++) {
    counters.assert_add("b", i);
    sum += i;
  }
  assert(counters.assert_get("b") == sum);

  std::cout << "2\n";
  dumpdb(db);

  std::cout << "3\n";

  if (test_compaction) {
    db->flush(o);

    cout << "compaction started ...\n";
    db->compactrange(nullptr, nullptr);
    cout << "compaction ended\n";

    dumpdb(db);

    assert(counters.assert_get("a")== 3);
    assert(counters.assert_get("b") == sum);
  }
}

void testsuccessivemerge(
    counters& counters, int max_num_merges, int num_merges) {

  counters.assert_remove("z");
  uint64_t sum = 0;

  for (int i = 1; i <= num_merges; ++i) {
    resetnummergeoperatorcalls();
    counters.assert_add("z", i);
    sum += i;

    if (i % (max_num_merges + 1) == 0) {
      assert(nummergeoperatorcalls == max_num_merges + 1);
    } else {
      assert(nummergeoperatorcalls == 0);
    }

    resetnummergeoperatorcalls();
    assert(counters.assert_get("z") == sum);
    assert(nummergeoperatorcalls == i % (max_num_merges + 1));
  }
}

void testpartialmerge(counters* counters, db* db, int max_merge, int min_merge,
                      int count) {
  flushoptions o;
  o.wait = true;

  // test case 1: partial merge should be called when the number of merge
  //              operands exceeds the threshold.
  uint64_t tmp_sum = 0;
  resetnumpartialmergecalls();
  for (int i = 1; i <= count; i++) {
    counters->assert_add("b", i);
    tmp_sum += i;
  }
  db->flush(o);
  db->compactrange(nullptr, nullptr);
  assert_eq(tmp_sum, counters->assert_get("b"));
  if (count > max_merge) {
    // in this case, fullmerge should be called instead.
    assert_eq(num_partial_merge_calls, 0);
  } else {
    // if count >= min_merge, then partial merge should be called once.
    assert_eq((count >= min_merge), (num_partial_merge_calls == 1));
  }

  // test case 2: partial merge should not be called when a put is found.
  resetnumpartialmergecalls();
  tmp_sum = 0;
  db->put(rocksdb::writeoptions(), "c", "10");
  for (int i = 1; i <= count; i++) {
    counters->assert_add("c", i);
    tmp_sum += i;
  }
  db->flush(o);
  db->compactrange(nullptr, nullptr);
  assert_eq(tmp_sum, counters->assert_get("c"));
  assert_eq(num_partial_merge_calls, 0);
}

void testsinglebatchsuccessivemerge(
    db* db,
    int max_num_merges,
    int num_merges) {
  assert(num_merges > max_num_merges);

  slice key("batchsuccessivemerge");
  uint64_t merge_value = 1;
  slice merge_value_slice((char *)&merge_value, sizeof(merge_value));

  // create the batch
  writebatch batch;
  for (int i = 0; i < num_merges; ++i) {
    batch.merge(key, merge_value_slice);
  }

  // apply to memtable and count the number of merges
  resetnummergeoperatorcalls();
  {
    status s = db->write(writeoptions(), &batch);
    assert(s.ok());
  }
  assert(nummergeoperatorcalls ==
      num_merges - (num_merges % (max_num_merges + 1)));

  // get the value
  resetnummergeoperatorcalls();
  string get_value_str;
  {
    status s = db->get(readoptions(), key, &get_value_str);
    assert(s.ok());
  }
  assert(get_value_str.size() == sizeof(uint64_t));
  uint64_t get_value = decodefixed64(&get_value_str[0]);
  assert_eq(get_value, num_merges * merge_value);
  assert_eq(nummergeoperatorcalls, (num_merges % (max_num_merges + 1)));
}

void runtest(int argc, const string& dbname, const bool use_ttl = false) {
  auto db = opendb(dbname, use_ttl);

  {
    cout << "test read-modify-write counters... \n";
    counters counters(db, 0);
    testcounters(counters, db.get(), true);
  }

  bool compact = false;
  if (argc > 1) {
    compact = true;
    cout << "turn on compaction\n";
  }

  {
    cout << "test merge-based counters... \n";
    mergebasedcounters counters(db, 0);
    testcounters(counters, db.get(), compact);
  }

  destroydb(dbname, options());
  db.reset();

  {
    cout << "test merge in memtable... \n";
    size_t max_merge = 5;
    auto db = opendb(dbname, use_ttl, max_merge);
    mergebasedcounters counters(db, 0);
    testcounters(counters, db.get(), compact);
    testsuccessivemerge(counters, max_merge, max_merge * 2);
    testsinglebatchsuccessivemerge(db.get(), 5, 7);
    destroydb(dbname, options());
  }

  {
    cout << "test partial-merge\n";
    size_t max_merge = 100;
    for (uint32_t min_merge = 5; min_merge < 25; min_merge += 5) {
      for (uint32_t count = min_merge - 1; count <= min_merge + 1; count++) {
        auto db = opendb(dbname, use_ttl, max_merge, min_merge);
        mergebasedcounters counters(db, 0);
        testpartialmerge(&counters, db.get(), max_merge, min_merge, count);
        destroydb(dbname, options());
      }
      {
        auto db = opendb(dbname, use_ttl, max_merge, min_merge);
        mergebasedcounters counters(db, 0);
        testpartialmerge(&counters, db.get(), max_merge, min_merge,
                         min_merge * 10);
        destroydb(dbname, options());
      }
    }
  }

  {
    cout << "test merge-operator not set after reopen\n";
    {
      auto db = opendb(dbname);
      mergebasedcounters counters(db, 0);
      counters.add("test-key", 1);
      counters.add("test-key", 1);
      counters.add("test-key", 1);
      db->compactrange(nullptr, nullptr);
    }

    db* reopen_db;
    assert_ok(db::open(options(), dbname, &reopen_db));
    std::string value;
    assert_true(!(reopen_db->get(readoptions(), "test-key", &value).ok()));
    delete reopen_db;
    destroydb(dbname, options());
  }

  /* temporary remove this test
  {
    cout << "test merge-operator not set after reopen (recovery case)\n";
    {
      auto db = opendb(dbname);
      mergebasedcounters counters(db, 0);
      counters.add("test-key", 1);
      counters.add("test-key", 1);
      counters.add("test-key", 1);
    }

    db* reopen_db;
    assert_true(db::open(options(), dbname, &reopen_db).isinvalidargument());
  }
  */
}
}  // namespace

int main(int argc, char *argv[]) {
  //todo: make this test like a general rocksdb unit-test
  runtest(argc, test::tmpdir() + "/merge_testdb");
  runtest(argc, test::tmpdir() + "/merge_testdbttl", true); // run test on ttl database
  printf("passed all tests!\n");
  return 0;
}
