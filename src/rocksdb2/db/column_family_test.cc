//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <algorithm>
#include <vector>
#include <string>

#include "db/db_impl.h"
#include "rocksdb/env.h"
#include "rocksdb/db.h"
#include "util/testharness.h"
#include "util/testutil.h"
#include "util/coding.h"
#include "utilities/merge_operators.h"

namespace rocksdb {

namespace {
std::string randomstring(random* rnd, int len) {
  std::string r;
  test::randomstring(rnd, len, &r);
  return r;
}
}  // anonymous namespace

// counts how many operations were performed
class envcounter : public envwrapper {
 public:
  explicit envcounter(env* base)
      : envwrapper(base), num_new_writable_file_(0) {}
  int getnumberofnewwritablefilecalls() {
    return num_new_writable_file_;
  }
  status newwritablefile(const std::string& f, unique_ptr<writablefile>* r,
                         const envoptions& soptions) {
    ++num_new_writable_file_;
    return envwrapper::newwritablefile(f, r, soptions);
  }

 private:
  int num_new_writable_file_;
};

class columnfamilytest {
 public:
  columnfamilytest() : rnd_(139) {
    env_ = new envcounter(env::default());
    dbname_ = test::tmpdir() + "/column_family_test";
    db_options_.create_if_missing = true;
    db_options_.env = env_;
    destroydb(dbname_, options(db_options_, column_family_options_));
  }

  ~columnfamilytest() {
    delete env_;
  }

  void close() {
    for (auto h : handles_) {
      delete h;
    }
    handles_.clear();
    names_.clear();
    delete db_;
    db_ = nullptr;
  }

  status tryopen(std::vector<std::string> cf,
                 std::vector<columnfamilyoptions> options = {}) {
    std::vector<columnfamilydescriptor> column_families;
    names_.clear();
    for (size_t i = 0; i < cf.size(); ++i) {
      column_families.push_back(columnfamilydescriptor(
          cf[i], options.size() == 0 ? column_family_options_ : options[i]));
      names_.push_back(cf[i]);
    }
    return db::open(db_options_, dbname_, column_families, &handles_, &db_);
  }

  status openreadonly(std::vector<std::string> cf,
                         std::vector<columnfamilyoptions> options = {}) {
    std::vector<columnfamilydescriptor> column_families;
    names_.clear();
    for (size_t i = 0; i < cf.size(); ++i) {
      column_families.push_back(columnfamilydescriptor(
          cf[i], options.size() == 0 ? column_family_options_ : options[i]));
      names_.push_back(cf[i]);
    }
    return db::openforreadonly(db_options_, dbname_, column_families, &handles_,
                               &db_);
  }

  void assertopenreadonly(std::vector<std::string> cf,
                    std::vector<columnfamilyoptions> options = {}) {
    assert_ok(openreadonly(cf, options));
  }


  void open(std::vector<std::string> cf,
            std::vector<columnfamilyoptions> options = {}) {
    assert_ok(tryopen(cf, options));
  }

  void open() {
    open({"default"});
  }

  dbimpl* dbfull() { return reinterpret_cast<dbimpl*>(db_); }

  int getproperty(int cf, std::string property) {
    std::string value;
    assert_true(dbfull()->getproperty(handles_[cf], property, &value));
    return std::stoi(value);
  }

  void destroy() {
    for (auto h : handles_) {
      delete h;
    }
    handles_.clear();
    names_.clear();
    delete db_;
    db_ = nullptr;
    assert_ok(destroydb(dbname_, options(db_options_, column_family_options_)));
  }

  void createcolumnfamilies(
      const std::vector<std::string>& cfs,
      const std::vector<columnfamilyoptions> options = {}) {
    int cfi = handles_.size();
    handles_.resize(cfi + cfs.size());
    names_.resize(cfi + cfs.size());
    for (size_t i = 0; i < cfs.size(); ++i) {
      assert_ok(db_->createcolumnfamily(
          options.size() == 0 ? column_family_options_ : options[i], cfs[i],
          &handles_[cfi]));
      names_[cfi] = cfs[i];
      cfi++;
    }
  }

  void reopen(const std::vector<columnfamilyoptions> options = {}) {
    std::vector<std::string> names;
    for (auto name : names_) {
      if (name != "") {
        names.push_back(name);
      }
    }
    close();
    assert(options.size() == 0 || names.size() == options.size());
    open(names, options);
  }

  void createcolumnfamiliesandreopen(const std::vector<std::string>& cfs) {
    createcolumnfamilies(cfs);
    reopen();
  }

  void dropcolumnfamilies(const std::vector<int>& cfs) {
    for (auto cf : cfs) {
      assert_ok(db_->dropcolumnfamily(handles_[cf]));
      delete handles_[cf];
      handles_[cf] = nullptr;
      names_[cf] = "";
    }
  }

  void putrandomdata(int cf, int num, int key_value_size) {
    for (int i = 0; i < num; ++i) {
      // 10 bytes for key, rest is value
      assert_ok(put(cf, test::randomkey(&rnd_, 10),
                    randomstring(&rnd_, key_value_size - 10)));
    }
  }

  void waitforflush(int cf) {
    assert_ok(dbfull()->test_waitforflushmemtable(handles_[cf]));
  }

  void waitforcompaction() { assert_ok(dbfull()->test_waitforcompact()); }

  status put(int cf, const std::string& key, const std::string& value) {
    return db_->put(writeoptions(), handles_[cf], slice(key), slice(value));
  }
  status merge(int cf, const std::string& key, const std::string& value) {
    return db_->merge(writeoptions(), handles_[cf], slice(key), slice(value));
  }
  status flush(int cf) {
    return db_->flush(flushoptions(), handles_[cf]);
  }

  std::string get(int cf, const std::string& key) {
    readoptions options;
    options.verify_checksums = true;
    std::string result;
    status s = db_->get(options, handles_[cf], slice(key), &result);
    if (s.isnotfound()) {
      result = "not_found";
    } else if (!s.ok()) {
      result = s.tostring();
    }
    return result;
  }

  void compactall(int cf) {
    assert_ok(db_->compactrange(handles_[cf], nullptr, nullptr));
  }

  void compact(int cf, const slice& start, const slice& limit) {
    assert_ok(db_->compactrange(handles_[cf], &start, &limit));
  }

  int numtablefilesatlevel(int level, int cf) {
    return getproperty(cf,
                       "rocksdb.num-files-at-level" + std::to_string(level));
  }

  // return spread of files per level
  std::string filesperlevel(int cf) {
    std::string result;
    int last_non_zero_offset = 0;
    for (int level = 0; level < dbfull()->numberlevels(handles_[cf]); level++) {
      int f = numtablefilesatlevel(level, cf);
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

  int countlivefiles() {
    std::vector<livefilemetadata> metadata;
    db_->getlivefilesmetadata(&metadata);
    return static_cast<int>(metadata.size());
  }

  // do n memtable flushes, each of which produces an sstable
  // covering the range [small,large].
  void maketables(int cf, int n, const std::string& small,
                  const std::string& large) {
    for (int i = 0; i < n; i++) {
      assert_ok(put(cf, small, "begin"));
      assert_ok(put(cf, large, "end"));
      assert_ok(db_->flush(flushoptions(), handles_[cf]));
    }
  }

  int countlivelogfiles() {
    int micros_wait_for_log_deletion = 20000;
    env_->sleepformicroseconds(micros_wait_for_log_deletion);
    int ret = 0;
    vectorlogptr wal_files;
    status s;
    // getsortedwalfiles is a flakey function -- it gets all the wal_dir
    // children files and then later checks for their existance. if some of the
    // log files doesn't exist anymore, it reports an error. it does all of this
    // without db mutex held, so if a background process deletes the log file
    // while the function is being executed, it returns an error. we retry the
    // function 10 times to avoid the error failing the test
    for (int retries = 0; retries < 10; ++retries) {
      wal_files.clear();
      s = db_->getsortedwalfiles(wal_files);
      if (s.ok()) {
        break;
      }
    }
    assert_ok(s);
    for (const auto& wal : wal_files) {
      if (wal->type() == kalivelogfile) {
        ++ret;
      }
    }
    return ret;
  }

  void assertnumberofimmutablememtables(std::vector<int> num_per_cf) {
    assert(num_per_cf.size() == handles_.size());

    for (size_t i = 0; i < num_per_cf.size(); ++i) {
      assert_eq(num_per_cf[i],
                getproperty(i, "rocksdb.num-immutable-mem-table"));
    }
  }

  void copyfile(const std::string& source, const std::string& destination,
                uint64_t size = 0) {
    const envoptions soptions;
    unique_ptr<sequentialfile> srcfile;
    assert_ok(env_->newsequentialfile(source, &srcfile, soptions));
    unique_ptr<writablefile> destfile;
    assert_ok(env_->newwritablefile(destination, &destfile, soptions));

    if (size == 0) {
      // default argument means copy everything
      assert_ok(env_->getfilesize(source, &size));
    }

    char buffer[4096];
    slice slice;
    while (size > 0) {
      uint64_t one = std::min(uint64_t(sizeof(buffer)), size);
      assert_ok(srcfile->read(one, &slice, buffer));
      assert_ok(destfile->append(slice));
      size -= slice.size();
    }
    assert_ok(destfile->close());
  }

  std::vector<columnfamilyhandle*> handles_;
  std::vector<std::string> names_;
  columnfamilyoptions column_family_options_;
  dboptions db_options_;
  std::string dbname_;
  db* db_ = nullptr;
  envcounter* env_;
  random rnd_;
};

test(columnfamilytest, dontreusecolumnfamilyid) {
  for (int iter = 0; iter < 3; ++iter) {
    open();
    createcolumnfamilies({"one", "two", "three"});
    for (size_t i = 0; i < handles_.size(); ++i) {
      auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(handles_[i]);
      assert_eq(i, cfh->getid());
    }
    if (iter == 1) {
      reopen();
    }
    dropcolumnfamilies({3});
    reopen();
    if (iter == 2) {
      // this tests if max_column_family is correctly persisted with
      // writesnapshot()
      reopen();
    }
    createcolumnfamilies({"three2"});
    // id 3 that was used for dropped column family "three" should not be reused
    auto cfh3 = reinterpret_cast<columnfamilyhandleimpl*>(handles_[3]);
    assert_eq(4u, cfh3->getid());
    close();
    destroy();
  }
}


test(columnfamilytest, adddrop) {
  open();
  createcolumnfamilies({"one", "two", "three"});
  assert_eq("not_found", get(1, "fodor"));
  assert_eq("not_found", get(2, "fodor"));
  dropcolumnfamilies({2});
  assert_eq("not_found", get(1, "fodor"));
  createcolumnfamilies({"four"});
  assert_eq("not_found", get(3, "fodor"));
  assert_ok(put(1, "fodor", "mirko"));
  assert_eq("mirko", get(1, "fodor"));
  assert_eq("not_found", get(3, "fodor"));
  close();
  assert_true(tryopen({"default"}).isinvalidargument());
  open({"default", "one", "three", "four"});
  dropcolumnfamilies({1});
  reopen();
  close();

  std::vector<std::string> families;
  assert_ok(db::listcolumnfamilies(db_options_, dbname_, &families));
  sort(families.begin(), families.end());
  assert_true(families ==
              std::vector<std::string>({"default", "four", "three"}));
}

test(columnfamilytest, droptest) {
  // first iteration - dont reopen db before dropping
  // second iteration - reopen db before dropping
  for (int iter = 0; iter < 2; ++iter) {
    open({"default"});
    createcolumnfamiliesandreopen({"pikachu"});
    for (int i = 0; i < 100; ++i) {
      assert_ok(put(1, std::to_string(i), "bar" + std::to_string(i)));
    }
    assert_ok(flush(1));

    if (iter == 1) {
      reopen();
    }
    assert_eq("bar1", get(1, "1"));

    assert_eq(countlivefiles(), 1);
    dropcolumnfamilies({1});
    // make sure that all files are deleted when we drop the column family
    assert_eq(countlivefiles(), 0);
    destroy();
  }
}

test(columnfamilytest, writebatchfailure) {
  open();
  createcolumnfamiliesandreopen({"one", "two"});
  writebatch batch;
  batch.put(handles_[0], slice("existing"), slice("column-family"));
  batch.put(handles_[1], slice("non-existing"), slice("column-family"));
  assert_ok(db_->write(writeoptions(), &batch));
  dropcolumnfamilies({1});
  writeoptions woptions_ignore_missing_cf;
  woptions_ignore_missing_cf.ignore_missing_column_families = true;
  batch.put(handles_[0], slice("still here"), slice("column-family"));
  assert_ok(db_->write(woptions_ignore_missing_cf, &batch));
  assert_eq("column-family", get(0, "still here"));
  status s = db_->write(writeoptions(), &batch);
  assert_true(s.isinvalidargument());
  close();
}

test(columnfamilytest, readwrite) {
  open();
  createcolumnfamiliesandreopen({"one", "two"});
  assert_ok(put(0, "foo", "v1"));
  assert_ok(put(0, "bar", "v2"));
  assert_ok(put(1, "mirko", "v3"));
  assert_ok(put(0, "foo", "v2"));
  assert_ok(put(2, "fodor", "v5"));

  for (int iter = 0; iter <= 3; ++iter) {
    assert_eq("v2", get(0, "foo"));
    assert_eq("v2", get(0, "bar"));
    assert_eq("v3", get(1, "mirko"));
    assert_eq("v5", get(2, "fodor"));
    assert_eq("not_found", get(0, "fodor"));
    assert_eq("not_found", get(1, "fodor"));
    assert_eq("not_found", get(2, "foo"));
    if (iter <= 1) {
      reopen();
    }
  }
  close();
}

test(columnfamilytest, ignorerecoveredlog) {
  std::string backup_logs = dbname_ + "/backup_logs";

  // delete old files in backup_logs directory
  assert_ok(env_->createdirifmissing(dbname_));
  assert_ok(env_->createdirifmissing(backup_logs));
  std::vector<std::string> old_files;
  env_->getchildren(backup_logs, &old_files);
  for (auto& file : old_files) {
    if (file != "." && file != "..") {
      env_->deletefile(backup_logs + "/" + file);
    }
  }

  column_family_options_.merge_operator =
      mergeoperators::createuint64addoperator();
  db_options_.wal_dir = dbname_ + "/logs";
  destroy();
  open();
  createcolumnfamilies({"cf1", "cf2"});

  // fill up the db
  std::string one, two, three;
  putfixed64(&one, 1);
  putfixed64(&two, 2);
  putfixed64(&three, 3);
  assert_ok(merge(0, "foo", one));
  assert_ok(merge(1, "mirko", one));
  assert_ok(merge(0, "foo", one));
  assert_ok(merge(2, "bla", one));
  assert_ok(merge(2, "fodor", one));
  assert_ok(merge(0, "bar", one));
  assert_ok(merge(2, "bla", one));
  assert_ok(merge(1, "mirko", two));
  assert_ok(merge(1, "franjo", one));

  // copy the logs to backup
  std::vector<std::string> logs;
  env_->getchildren(db_options_.wal_dir, &logs);
  for (auto& log : logs) {
    if (log != ".." && log != ".") {
      copyfile(db_options_.wal_dir + "/" + log, backup_logs + "/" + log);
    }
  }

  // recover the db
  close();

  // 1. check consistency
  // 2. copy the logs from backup back to wal dir. if the recovery happens
  // again on the same log files, this should lead to incorrect results
  // due to applying merge operator twice
  // 3. check consistency
  for (int iter = 0; iter < 2; ++iter) {
    // assert consistency
    open({"default", "cf1", "cf2"});
    assert_eq(two, get(0, "foo"));
    assert_eq(one, get(0, "bar"));
    assert_eq(three, get(1, "mirko"));
    assert_eq(one, get(1, "franjo"));
    assert_eq(one, get(2, "fodor"));
    assert_eq(two, get(2, "bla"));
    close();

    if (iter == 0) {
      // copy the logs from backup back to wal dir
      for (auto& log : logs) {
        if (log != ".." && log != ".") {
          copyfile(backup_logs + "/" + log, db_options_.wal_dir + "/" + log);
        }
      }
    }
  }
}

test(columnfamilytest, flushtest) {
  open();
  createcolumnfamiliesandreopen({"one", "two"});
  assert_ok(put(0, "foo", "v1"));
  assert_ok(put(0, "bar", "v2"));
  assert_ok(put(1, "mirko", "v3"));
  assert_ok(put(0, "foo", "v2"));
  assert_ok(put(2, "fodor", "v5"));
  for (int i = 0; i < 3; ++i) {
    flush(i);
  }
  reopen();

  for (int iter = 0; iter <= 2; ++iter) {
    assert_eq("v2", get(0, "foo"));
    assert_eq("v2", get(0, "bar"));
    assert_eq("v3", get(1, "mirko"));
    assert_eq("v5", get(2, "fodor"));
    assert_eq("not_found", get(0, "fodor"));
    assert_eq("not_found", get(1, "fodor"));
    assert_eq("not_found", get(2, "foo"));
    if (iter <= 1) {
      reopen();
    }
  }
  close();
}

// makes sure that obsolete log files get deleted
test(columnfamilytest, logdeletiontest) {
  db_options_.max_total_wal_size = std::numeric_limits<uint64_t>::max();
  column_family_options_.write_buffer_size = 100000;  // 100kb
  open();
  createcolumnfamilies({"one", "two", "three", "four"});
  // each bracket is one log file. if number is in (), it means
  // we don't need it anymore (it's been flushed)
  // []
  assert_eq(countlivelogfiles(), 0);
  putrandomdata(0, 1, 100);
  // [0]
  putrandomdata(1, 1, 100);
  // [0, 1]
  putrandomdata(1, 1000, 100);
  waitforflush(1);
  // [0, (1)] [1]
  assert_eq(countlivelogfiles(), 2);
  putrandomdata(0, 1, 100);
  // [0, (1)] [0, 1]
  assert_eq(countlivelogfiles(), 2);
  putrandomdata(2, 1, 100);
  // [0, (1)] [0, 1, 2]
  putrandomdata(2, 1000, 100);
  waitforflush(2);
  // [0, (1)] [0, 1, (2)] [2]
  assert_eq(countlivelogfiles(), 3);
  putrandomdata(2, 1000, 100);
  waitforflush(2);
  // [0, (1)] [0, 1, (2)] [(2)] [2]
  assert_eq(countlivelogfiles(), 4);
  putrandomdata(3, 1, 100);
  // [0, (1)] [0, 1, (2)] [(2)] [2, 3]
  putrandomdata(1, 1, 100);
  // [0, (1)] [0, 1, (2)] [(2)] [1, 2, 3]
  assert_eq(countlivelogfiles(), 4);
  putrandomdata(1, 1000, 100);
  waitforflush(1);
  // [0, (1)] [0, (1), (2)] [(2)] [(1), 2, 3] [1]
  assert_eq(countlivelogfiles(), 5);
  putrandomdata(0, 1000, 100);
  waitforflush(0);
  // [(0), (1)] [(0), (1), (2)] [(2)] [(1), 2, 3] [1, (0)] [0]
  // delete obsolete logs -->
  // [(1), 2, 3] [1, (0)] [0]
  assert_eq(countlivelogfiles(), 3);
  putrandomdata(0, 1000, 100);
  waitforflush(0);
  // [(1), 2, 3] [1, (0)], [(0)] [0]
  assert_eq(countlivelogfiles(), 4);
  putrandomdata(1, 1000, 100);
  waitforflush(1);
  // [(1), 2, 3] [(1), (0)] [(0)] [0, (1)] [1]
  assert_eq(countlivelogfiles(), 5);
  putrandomdata(2, 1000, 100);
  waitforflush(2);
  // [(1), (2), 3] [(1), (0)] [(0)] [0, (1)] [1, (2)], [2]
  assert_eq(countlivelogfiles(), 6);
  putrandomdata(3, 1000, 100);
  waitforflush(3);
  // [(1), (2), (3)] [(1), (0)] [(0)] [0, (1)] [1, (2)], [2, (3)] [3]
  // delete obsolete logs -->
  // [0, (1)] [1, (2)], [2, (3)] [3]
  assert_eq(countlivelogfiles(), 4);
  close();
}

// makes sure that obsolete log files get deleted
test(columnfamilytest, differentwritebuffersizes) {
  // disable flushing stale column families
  db_options_.max_total_wal_size = std::numeric_limits<uint64_t>::max();
  open();
  createcolumnfamilies({"one", "two", "three"});
  columnfamilyoptions default_cf, one, two, three;
  // setup options. all column families have max_write_buffer_number setup to 10
  // "default" -> 100kb memtable, start flushing immediatelly
  // "one" -> 200kb memtable, start flushing with two immutable memtables
  // "two" -> 1mb memtable, start flushing with three immutable memtables
  // "three" -> 90kb memtable, start flushing with four immutable memtables
  default_cf.write_buffer_size = 100000;
  default_cf.max_write_buffer_number = 10;
  default_cf.min_write_buffer_number_to_merge = 1;
  one.write_buffer_size = 200000;
  one.max_write_buffer_number = 10;
  one.min_write_buffer_number_to_merge = 2;
  two.write_buffer_size = 1000000;
  two.max_write_buffer_number = 10;
  two.min_write_buffer_number_to_merge = 3;
  three.write_buffer_size = 90000;
  three.max_write_buffer_number = 10;
  three.min_write_buffer_number_to_merge = 4;

  reopen({default_cf, one, two, three});

  int micros_wait_for_flush = 10000;
  putrandomdata(0, 100, 1000);
  waitforflush(0);
  assertnumberofimmutablememtables({0, 0, 0, 0});
  assert_eq(countlivelogfiles(), 1);
  putrandomdata(1, 200, 1000);
  env_->sleepformicroseconds(micros_wait_for_flush);
  assertnumberofimmutablememtables({0, 1, 0, 0});
  assert_eq(countlivelogfiles(), 2);
  putrandomdata(2, 1000, 1000);
  env_->sleepformicroseconds(micros_wait_for_flush);
  assertnumberofimmutablememtables({0, 1, 1, 0});
  assert_eq(countlivelogfiles(), 3);
  putrandomdata(2, 1000, 1000);
  env_->sleepformicroseconds(micros_wait_for_flush);
  assertnumberofimmutablememtables({0, 1, 2, 0});
  assert_eq(countlivelogfiles(), 4);
  putrandomdata(3, 90, 1000);
  env_->sleepformicroseconds(micros_wait_for_flush);
  assertnumberofimmutablememtables({0, 1, 2, 1});
  assert_eq(countlivelogfiles(), 5);
  putrandomdata(3, 90, 1000);
  env_->sleepformicroseconds(micros_wait_for_flush);
  assertnumberofimmutablememtables({0, 1, 2, 2});
  assert_eq(countlivelogfiles(), 6);
  putrandomdata(3, 90, 1000);
  env_->sleepformicroseconds(micros_wait_for_flush);
  assertnumberofimmutablememtables({0, 1, 2, 3});
  assert_eq(countlivelogfiles(), 7);
  putrandomdata(0, 100, 1000);
  waitforflush(0);
  assertnumberofimmutablememtables({0, 1, 2, 3});
  assert_eq(countlivelogfiles(), 8);
  putrandomdata(2, 100, 10000);
  waitforflush(2);
  assertnumberofimmutablememtables({0, 1, 0, 3});
  assert_eq(countlivelogfiles(), 9);
  putrandomdata(3, 90, 1000);
  waitforflush(3);
  assertnumberofimmutablememtables({0, 1, 0, 0});
  assert_eq(countlivelogfiles(), 10);
  putrandomdata(3, 90, 1000);
  env_->sleepformicroseconds(micros_wait_for_flush);
  assertnumberofimmutablememtables({0, 1, 0, 1});
  assert_eq(countlivelogfiles(), 11);
  putrandomdata(1, 200, 1000);
  waitforflush(1);
  assertnumberofimmutablememtables({0, 0, 0, 1});
  assert_eq(countlivelogfiles(), 5);
  putrandomdata(3, 90*6, 1000);
  waitforflush(3);
  assertnumberofimmutablememtables({0, 0, 0, 0});
  assert_eq(countlivelogfiles(), 12);
  putrandomdata(0, 100, 1000);
  waitforflush(0);
  assertnumberofimmutablememtables({0, 0, 0, 0});
  assert_eq(countlivelogfiles(), 12);
  putrandomdata(2, 3*100, 10000);
  waitforflush(2);
  assertnumberofimmutablememtables({0, 0, 0, 0});
  assert_eq(countlivelogfiles(), 12);
  putrandomdata(1, 2*200, 1000);
  waitforflush(1);
  assertnumberofimmutablememtables({0, 0, 0, 0});
  assert_eq(countlivelogfiles(), 7);
  close();
}

test(columnfamilytest, differentmergeoperators) {
  open();
  createcolumnfamilies({"first", "second"});
  columnfamilyoptions default_cf, first, second;
  first.merge_operator = mergeoperators::createuint64addoperator();
  second.merge_operator = mergeoperators::createstringappendoperator();
  reopen({default_cf, first, second});

  std::string one, two, three;
  putfixed64(&one, 1);
  putfixed64(&two, 2);
  putfixed64(&three, 3);

  assert_ok(put(0, "foo", two));
  assert_ok(put(0, "foo", one));
  assert_true(merge(0, "foo", two).isnotsupported());
  assert_eq(get(0, "foo"), one);

  assert_ok(put(1, "foo", two));
  assert_ok(put(1, "foo", one));
  assert_ok(merge(1, "foo", two));
  assert_eq(get(1, "foo"), three);

  assert_ok(put(2, "foo", two));
  assert_ok(put(2, "foo", one));
  assert_ok(merge(2, "foo", two));
  assert_eq(get(2, "foo"), one + "," + two);
  close();
}

test(columnfamilytest, differentcompactionstyles) {
  open();
  createcolumnfamilies({"one", "two"});
  columnfamilyoptions default_cf, one, two;
  db_options_.max_open_files = 20;  // only 10 files in file cache
  db_options_.disabledatasync = true;

  default_cf.compaction_style = kcompactionstylelevel;
  default_cf.num_levels = 3;
  default_cf.write_buffer_size = 64 << 10;  // 64kb
  default_cf.target_file_size_base = 30 << 10;
  default_cf.source_compaction_factor = 100;
  blockbasedtableoptions table_options;
  table_options.no_block_cache = true;
  default_cf.table_factory.reset(newblockbasedtablefactory(table_options));

  one.compaction_style = kcompactionstyleuniversal;
  // trigger compaction if there are >= 4 files
  one.level0_file_num_compaction_trigger = 4;
  one.write_buffer_size = 100000;

  two.compaction_style = kcompactionstylelevel;
  two.num_levels = 4;
  two.max_mem_compaction_level = 0;
  two.level0_file_num_compaction_trigger = 3;
  two.write_buffer_size = 100000;

  reopen({default_cf, one, two});

  // setup column family "one" -- universal style
  for (int i = 0; i < one.level0_file_num_compaction_trigger - 1; ++i) {
    putrandomdata(1, 11, 10000);
    waitforflush(1);
    assert_eq(std::to_string(i + 1), filesperlevel(1));
  }

  // setup column family "two" -- level style with 4 levels
  for (int i = 0; i < two.level0_file_num_compaction_trigger - 1; ++i) {
    putrandomdata(2, 15, 10000);
    waitforflush(2);
    assert_eq(std::to_string(i + 1), filesperlevel(2));
  }

  // trigger compaction "one"
  putrandomdata(1, 12, 10000);

  // trigger compaction "two"
  putrandomdata(2, 10, 10000);

  // wait for compactions
  waitforcompaction();

  // verify compaction "one"
  assert_eq("1", filesperlevel(1));

  // verify compaction "two"
  assert_eq("0,1", filesperlevel(2));
  compactall(2);
  assert_eq("0,1", filesperlevel(2));

  close();
}

namespace {
std::string iterstatus(iterator* iter) {
  std::string result;
  if (iter->valid()) {
    result = iter->key().tostring() + "->" + iter->value().tostring();
  } else {
    result = "(invalid)";
  }
  return result;
}
}  // anonymous namespace

test(columnfamilytest, newiteratorstest) {
  // iter == 0 -- no tailing
  // iter == 2 -- tailing
  for (int iter = 0; iter < 2; ++iter) {
    open();
    createcolumnfamiliesandreopen({"one", "two"});
    assert_ok(put(0, "a", "b"));
    assert_ok(put(1, "b", "a"));
    assert_ok(put(2, "c", "m"));
    assert_ok(put(2, "v", "t"));
    std::vector<iterator*> iterators;
    readoptions options;
    options.tailing = (iter == 1);
    assert_ok(db_->newiterators(options, handles_, &iterators));

    for (auto it : iterators) {
      it->seektofirst();
    }
    assert_eq(iterstatus(iterators[0]), "a->b");
    assert_eq(iterstatus(iterators[1]), "b->a");
    assert_eq(iterstatus(iterators[2]), "c->m");

    assert_ok(put(1, "x", "x"));

    for (auto it : iterators) {
      it->next();
    }

    assert_eq(iterstatus(iterators[0]), "(invalid)");
    if (iter == 0) {
      // no tailing
      assert_eq(iterstatus(iterators[1]), "(invalid)");
    } else {
      // tailing
      assert_eq(iterstatus(iterators[1]), "x->x");
    }
    assert_eq(iterstatus(iterators[2]), "v->t");

    for (auto it : iterators) {
      delete it;
    }
    destroy();
  }
}

test(columnfamilytest, readonlydbtest) {
  open();
  createcolumnfamiliesandreopen({"one", "two", "three", "four"});
  assert_ok(put(0, "a", "b"));
  assert_ok(put(1, "foo", "bla"));
  assert_ok(put(2, "foo", "blabla"));
  assert_ok(put(3, "foo", "blablabla"));
  assert_ok(put(4, "foo", "blablablabla"));

  dropcolumnfamilies({2});
  close();
  // open only a subset of column families
  assertopenreadonly({"default", "one", "four"});
  assert_eq("not_found", get(0, "foo"));
  assert_eq("bla", get(1, "foo"));
  assert_eq("blablablabla", get(2, "foo"));


  // test newiterators
  {
    std::vector<iterator*> iterators;
    assert_ok(db_->newiterators(readoptions(), handles_, &iterators));
    for (auto it : iterators) {
      it->seektofirst();
    }
    assert_eq(iterstatus(iterators[0]), "a->b");
    assert_eq(iterstatus(iterators[1]), "foo->bla");
    assert_eq(iterstatus(iterators[2]), "foo->blablablabla");
    for (auto it : iterators) {
      it->next();
    }
    assert_eq(iterstatus(iterators[0]), "(invalid)");
    assert_eq(iterstatus(iterators[1]), "(invalid)");
    assert_eq(iterstatus(iterators[2]), "(invalid)");

    for (auto it : iterators) {
      delete it;
    }
  }

  close();
  // can't open dropped column family
  status s = openreadonly({"default", "one", "two"});
  assert_true(!s.ok());

  // can't open without specifying default column family
  s = openreadonly({"one", "four"});
  assert_true(!s.ok());
}

test(columnfamilytest, dontrollemptylogs) {
  open();
  createcolumnfamiliesandreopen({"one", "two", "three", "four"});

  for (size_t i = 0; i < handles_.size(); ++i) {
    putrandomdata(i, 10, 100);
  }
  int num_writable_file_start = env_->getnumberofnewwritablefilecalls();
  // this will trigger the flushes
  for (size_t i = 0; i <= 4; ++i) {
    assert_ok(flush(i));
  }

  for (int i = 0; i < 4; ++i) {
    dbfull()->test_waitforflushmemtable(handles_[i]);
  }
  int total_new_writable_files =
      env_->getnumberofnewwritablefilecalls() - num_writable_file_start;
  assert_eq(static_cast<size_t>(total_new_writable_files), handles_.size() + 1);
  close();
}

test(columnfamilytest, flushstalecolumnfamilies) {
  open();
  createcolumnfamilies({"one", "two"});
  columnfamilyoptions default_cf, one, two;
  default_cf.write_buffer_size = 100000;  // small write buffer size
  default_cf.disable_auto_compactions = true;
  one.disable_auto_compactions = true;
  two.disable_auto_compactions = true;
  db_options_.max_total_wal_size = 210000;

  reopen({default_cf, one, two});

  putrandomdata(2, 1, 10);  // 10 bytes
  for (int i = 0; i < 2; ++i) {
    putrandomdata(0, 100, 1000);  // flush
    waitforflush(0);
    assert_eq(i + 1, countlivefiles());
  }
  // third flush. now, cf [two] should be detected as stale and flushed
  // column family 1 should not be flushed since it's empty
  putrandomdata(0, 100, 1000);  // flush
  waitforflush(0);
  waitforflush(2);
  // 3 files for default column families, 1 file for column family [two], zero
  // files for column family [one], because it's empty
  assert_eq(4, countlivefiles());
  close();
}

test(columnfamilytest, createmissingcolumnfamilies) {
  status s = tryopen({"one", "two"});
  assert_true(!s.ok());
  db_options_.create_missing_column_families = true;
  s = tryopen({"default", "one", "two"});
  assert_true(s.ok());
  close();
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
