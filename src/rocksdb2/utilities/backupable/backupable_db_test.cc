//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <string>
#include <algorithm>
#include <iostream>

#include "port/port.h"
#include "rocksdb/types.h"
#include "rocksdb/transaction_log.h"
#include "rocksdb/utilities/backupable_db.h"
#include "util/testharness.h"
#include "util/random.h"
#include "util/mutexlock.h"
#include "util/testutil.h"
#include "util/auto_roll_logger.h"

namespace rocksdb {

namespace {

using std::unique_ptr;

class dummydb : public stackabledb {
 public:
  /* implicit */
  dummydb(const options& options, const std::string& dbname)
     : stackabledb(nullptr), options_(options), dbname_(dbname),
       deletions_enabled_(true), sequence_number_(0) {}

  virtual sequencenumber getlatestsequencenumber() const {
    return ++sequence_number_;
  }

  virtual const std::string& getname() const override {
    return dbname_;
  }

  virtual env* getenv() const override {
    return options_.env;
  }

  using db::getoptions;
  virtual const options& getoptions(columnfamilyhandle* column_family) const
      override {
    return options_;
  }

  virtual status enablefiledeletions(bool force) override {
    assert_true(!deletions_enabled_);
    deletions_enabled_ = true;
    return status::ok();
  }

  virtual status disablefiledeletions() override {
    assert_true(deletions_enabled_);
    deletions_enabled_ = false;
    return status::ok();
  }

  virtual status getlivefiles(std::vector<std::string>& vec, uint64_t* mfs,
                              bool flush_memtable = true) override {
    assert_true(!deletions_enabled_);
    vec = live_files_;
    *mfs = 100;
    return status::ok();
  }

  virtual columnfamilyhandle* defaultcolumnfamily() const override {
    return nullptr;
  }

  class dummylogfile : public logfile {
   public:
    /* implicit */
     dummylogfile(const std::string& path, bool alive = true)
         : path_(path), alive_(alive) {}

    virtual std::string pathname() const override {
      return path_;
    }

    virtual uint64_t lognumber() const {
      // what business do you have calling this method?
      assert_true(false);
      return 0;
    }

    virtual walfiletype type() const override {
      return alive_ ? kalivelogfile : karchivedlogfile;
    }

    virtual sequencenumber startsequence() const {
      // backupabledb should not need this method
      assert_true(false);
      return 0;
    }

    virtual uint64_t sizefilebytes() const {
      // backupabledb should not need this method
      assert_true(false);
      return 0;
    }

   private:
    std::string path_;
    bool alive_;
  }; // dummylogfile

  virtual status getsortedwalfiles(vectorlogptr& files) override {
    assert_true(!deletions_enabled_);
    files.resize(wal_files_.size());
    for (size_t i = 0; i < files.size(); ++i) {
      files[i].reset(
          new dummylogfile(wal_files_[i].first, wal_files_[i].second));
    }
    return status::ok();
  }

  std::vector<std::string> live_files_;
  // pair<filename, alive?>
  std::vector<std::pair<std::string, bool>> wal_files_;
 private:
  options options_;
  std::string dbname_;
  bool deletions_enabled_;
  mutable sequencenumber sequence_number_;
}; // dummydb

class testenv : public envwrapper {
 public:
  explicit testenv(env* t) : envwrapper(t) {}

  class dummysequentialfile : public sequentialfile {
   public:
    dummysequentialfile() : sequentialfile(), rnd_(5) {}
    virtual status read(size_t n, slice* result, char* scratch) {
      size_t read_size = (n > size_left) ? size_left : n;
      for (size_t i = 0; i < read_size; ++i) {
        scratch[i] = rnd_.next() & 255;
      }
      *result = slice(scratch, read_size);
      size_left -= read_size;
      return status::ok();
    }

    virtual status skip(uint64_t n) {
      size_left = (n > size_left) ? size_left - n : 0;
      return status::ok();
    }
   private:
    size_t size_left = 200;
    random rnd_;
  };

  status newsequentialfile(const std::string& f,
                           unique_ptr<sequentialfile>* r,
                           const envoptions& options) {
    mutexlock l(&mutex_);
    if (dummy_sequential_file_) {
      r->reset(new testenv::dummysequentialfile());
      return status::ok();
    } else {
      return envwrapper::newsequentialfile(f, r, options);
    }
  }

  status newwritablefile(const std::string& f, unique_ptr<writablefile>* r,
                         const envoptions& options) {
    mutexlock l(&mutex_);
    written_files_.push_back(f);
    if (limit_written_files_ <= 0) {
      return status::notsupported("sorry, can't do this");
    }
    limit_written_files_--;
    return envwrapper::newwritablefile(f, r, options);
  }

  virtual status deletefile(const std::string& fname) override {
    mutexlock l(&mutex_);
    assert_gt(limit_delete_files_, 0u);
    limit_delete_files_--;
    return envwrapper::deletefile(fname);
  }

  void assertwrittenfiles(std::vector<std::string>& should_have_written) {
    mutexlock l(&mutex_);
    sort(should_have_written.begin(), should_have_written.end());
    sort(written_files_.begin(), written_files_.end());
    assert_true(written_files_ == should_have_written);
  }

  void clearwrittenfiles() {
    mutexlock l(&mutex_);
    written_files_.clear();
  }

  void setlimitwrittenfiles(uint64_t limit) {
    mutexlock l(&mutex_);
    limit_written_files_ = limit;
  }

  void setlimitdeletefiles(uint64_t limit) {
    mutexlock l(&mutex_);
    limit_delete_files_ = limit;
  }

  void setdummysequentialfile(bool dummy_sequential_file) {
    mutexlock l(&mutex_);
    dummy_sequential_file_ = dummy_sequential_file;
  }

 private:
  port::mutex mutex_;
  bool dummy_sequential_file_ = false;
  std::vector<std::string> written_files_;
  uint64_t limit_written_files_ = 1000000;
  uint64_t limit_delete_files_ = 1000000;
};  // testenv

class filemanager : public envwrapper {
 public:
  explicit filemanager(env* t) : envwrapper(t), rnd_(5) {}

  status deleterandomfileindir(const std::string dir) {
    std::vector<std::string> children;
    getchildren(dir, &children);
    if (children.size() <= 2) { // . and ..
      return status::notfound("");
    }
    while (true) {
      int i = rnd_.next() % children.size();
      if (children[i] != "." && children[i] != "..") {
        return deletefile(dir + "/" + children[i]);
      }
    }
    // should never get here
    assert(false);
    return status::notfound("");
  }

  status corruptfile(const std::string& fname, uint64_t bytes_to_corrupt) {
    uint64_t size;
    status s = getfilesize(fname, &size);
    if (!s.ok()) {
      return s;
    }
    unique_ptr<randomrwfile> file;
    envoptions env_options;
    env_options.use_mmap_writes = false;
    s = newrandomrwfile(fname, &file, env_options);
    if (!s.ok()) {
      return s;
    }

    for (uint64_t i = 0; s.ok() && i < bytes_to_corrupt; ++i) {
      std::string tmp;
      // write one random byte to a random position
      s = file->write(rnd_.next() % size, test::randomstring(&rnd_, 1, &tmp));
    }
    return s;
  }

  status corruptchecksum(const std::string& fname, bool appear_valid) {
    std::string metadata;
    status s = readfiletostring(this, fname, &metadata);
    if (!s.ok()) {
      return s;
    }
    s = deletefile(fname);
    if (!s.ok()) {
      return s;
    }

    auto pos = metadata.find("private");
    if (pos == std::string::npos) {
      return status::corruption("private file is expected");
    }
    pos = metadata.find(" crc32 ", pos + 6);
    if (pos == std::string::npos) {
      return status::corruption("checksum not found");
    }

    if (metadata.size() < pos + 7) {
      return status::corruption("bad crc32 checksum value");
    }

    if (appear_valid) {
      if (metadata[pos + 8] == '\n') {
        // single digit value, safe to insert one more digit
        metadata.insert(pos + 8, 1, '0');
      } else {
        metadata.erase(pos + 8, 1);
      }
    } else {
      metadata[pos + 7] = 'a';
    }

    return writetofile(fname, metadata);
  }

  status writetofile(const std::string& fname, const std::string& data) {
    unique_ptr<writablefile> file;
    envoptions env_options;
    env_options.use_mmap_writes = false;
    status s = envwrapper::newwritablefile(fname, &file, env_options);
    if (!s.ok()) {
      return s;
    }
    return file->append(slice(data));
  }

 private:
  random rnd_;
}; // filemanager

// utility functions
static size_t filldb(db* db, int from, int to) {
  size_t bytes_written = 0;
  for (int i = from; i < to; ++i) {
    std::string key = "testkey" + std::to_string(i);
    std::string value = "testvalue" + std::to_string(i);
    bytes_written += key.size() + value.size();

    assert_ok(db->put(writeoptions(), slice(key), slice(value)));
  }
  return bytes_written;
}

static void assertexists(db* db, int from, int to) {
  for (int i = from; i < to; ++i) {
    std::string key = "testkey" + std::to_string(i);
    std::string value;
    status s = db->get(readoptions(), slice(key), &value);
    assert_eq(value, "testvalue" + std::to_string(i));
  }
}

static void assertempty(db* db, int from, int to) {
  for (int i = from; i < to; ++i) {
    std::string key = "testkey" + std::to_string(i);
    std::string value = "testvalue" + std::to_string(i);

    status s = db->get(readoptions(), slice(key), &value);
    assert_true(s.isnotfound());
  }
}

class backupabledbtest {
 public:
  backupabledbtest() {
    // set up files
    dbname_ = test::tmpdir() + "/backupable_db";
    backupdir_ = test::tmpdir() + "/backupable_db_backup";

    // set up envs
    env_ = env::default();
    test_db_env_.reset(new testenv(env_));
    test_backup_env_.reset(new testenv(env_));
    file_manager_.reset(new filemanager(env_));

    // set up db options
    options_.create_if_missing = true;
    options_.paranoid_checks = true;
    options_.write_buffer_size = 1 << 17; // 128kb
    options_.env = test_db_env_.get();
    options_.wal_dir = dbname_;
    // set up backup db options
    createloggerfromoptions(dbname_, backupdir_, env_,
                            dboptions(), &logger_);
    backupable_options_.reset(new backupabledboptions(
        backupdir_, test_backup_env_.get(), true, logger_.get(), true));

    // delete old files in db
    destroydb(dbname_, options());
  }

  db* opendb() {
    db* db;
    assert_ok(db::open(options_, dbname_, &db));
    return db;
  }

  void openbackupabledb(bool destroy_old_data = false, bool dummy = false,
                        bool share_table_files = true,
                        bool share_with_checksums = false) {
    // reset all the defaults
    test_backup_env_->setlimitwrittenfiles(1000000);
    test_db_env_->setlimitwrittenfiles(1000000);
    test_db_env_->setdummysequentialfile(dummy);

    db* db;
    if (dummy) {
      dummy_db_ = new dummydb(options_, dbname_);
      db = dummy_db_;
    } else {
      assert_ok(db::open(options_, dbname_, &db));
    }
    backupable_options_->destroy_old_data = destroy_old_data;
    backupable_options_->share_table_files = share_table_files;
    backupable_options_->share_files_with_checksum = share_with_checksums;
    db_.reset(new backupabledb(db, *backupable_options_));
  }

  void closebackupabledb() {
    db_.reset(nullptr);
  }

  void openrestoredb() {
    backupable_options_->destroy_old_data = false;
    restore_db_.reset(
        new restorebackupabledb(test_db_env_.get(), *backupable_options_));
  }

  void closerestoredb() {
    restore_db_.reset(nullptr);
  }

  // restores backup backup_id and asserts the existence of
  // [start_exist, end_exist> and not-existence of
  // [end_exist, end>
  //
  // if backup_id == 0, it means restore from latest
  // if end == 0, don't check assertempty
  void assertbackupconsistency(backupid backup_id, uint32_t start_exist,
                               uint32_t end_exist, uint32_t end = 0,
                               bool keep_log_files = false) {
    restoreoptions restore_options(keep_log_files);
    bool opened_restore = false;
    if (restore_db_.get() == nullptr) {
      opened_restore = true;
      openrestoredb();
    }
    if (backup_id > 0) {
      assert_ok(restore_db_->restoredbfrombackup(backup_id, dbname_, dbname_,
                                                 restore_options));
    } else {
      assert_ok(restore_db_->restoredbfromlatestbackup(dbname_, dbname_,
                                                       restore_options));
    }
    db* db = opendb();
    assertexists(db, start_exist, end_exist);
    if (end != 0) {
      assertempty(db, end_exist, end);
    }
    delete db;
    if (opened_restore) {
      closerestoredb();
    }
  }

  void deletelogfiles() {
    std::vector<std::string> delete_logs;
    env_->getchildren(dbname_, &delete_logs);
    for (auto f : delete_logs) {
      uint64_t number;
      filetype type;
      bool ok = parsefilename(f, &number, &type);
      if (ok && type == klogfile) {
        env_->deletefile(dbname_ + "/" + f);
      }
    }
  }

  // files
  std::string dbname_;
  std::string backupdir_;

  // envs
  env* env_;
  unique_ptr<testenv> test_db_env_;
  unique_ptr<testenv> test_backup_env_;
  unique_ptr<filemanager> file_manager_;

  // all the dbs!
  dummydb* dummy_db_; // backupabledb owns dummy_db_
  unique_ptr<backupabledb> db_;
  unique_ptr<restorebackupabledb> restore_db_;

  // options
  options options_;
  unique_ptr<backupabledboptions> backupable_options_;
  std::shared_ptr<logger> logger_;
}; // backupabledbtest

void appendpath(const std::string& path, std::vector<std::string>& v) {
  for (auto& f : v) {
    f = path + f;
  }
}

// this will make sure that backup does not copy the same file twice
test(backupabledbtest, nodoublecopy) {
  openbackupabledb(true, true);

  // should write 5 db files + latest_backup + one meta file
  test_backup_env_->setlimitwrittenfiles(7);
  test_backup_env_->clearwrittenfiles();
  test_db_env_->setlimitwrittenfiles(0);
  dummy_db_->live_files_ = { "/00010.sst", "/00011.sst",
                             "/current",   "/manifest-01" };
  dummy_db_->wal_files_ = {{"/00011.log", true}, {"/00012.log", false}};
  assert_ok(db_->createnewbackup(false));
  std::vector<std::string> should_have_written = {
    "/shared/00010.sst.tmp",
    "/shared/00011.sst.tmp",
    "/private/1.tmp/current",
    "/private/1.tmp/manifest-01",
    "/private/1.tmp/00011.log",
    "/meta/1.tmp",
    "/latest_backup.tmp"
  };
  appendpath(dbname_ + "_backup", should_have_written);
  test_backup_env_->assertwrittenfiles(should_have_written);

  // should write 4 new db files + latest_backup + one meta file
  // should not write/copy 00010.sst, since it's already there!
  test_backup_env_->setlimitwrittenfiles(6);
  test_backup_env_->clearwrittenfiles();
  dummy_db_->live_files_ = { "/00010.sst", "/00015.sst",
                             "/current",   "/manifest-01" };
  dummy_db_->wal_files_ = {{"/00011.log", true}, {"/00012.log", false}};
  assert_ok(db_->createnewbackup(false));
  // should not open 00010.sst - it's already there
  should_have_written = {
    "/shared/00015.sst.tmp",
    "/private/2.tmp/current",
    "/private/2.tmp/manifest-01",
    "/private/2.tmp/00011.log",
    "/meta/2.tmp",
    "/latest_backup.tmp"
  };
  appendpath(dbname_ + "_backup", should_have_written);
  test_backup_env_->assertwrittenfiles(should_have_written);

  assert_ok(db_->deletebackup(1));
  assert_eq(true,
            test_backup_env_->fileexists(backupdir_ + "/shared/00010.sst"));
  // 00011.sst was only in backup 1, should be deleted
  assert_eq(false,
            test_backup_env_->fileexists(backupdir_ + "/shared/00011.sst"));
  assert_eq(true,
            test_backup_env_->fileexists(backupdir_ + "/shared/00015.sst"));

  // manifest file size should be only 100
  uint64_t size;
  test_backup_env_->getfilesize(backupdir_ + "/private/2/manifest-01", &size);
  assert_eq(100ul, size);
  test_backup_env_->getfilesize(backupdir_ + "/shared/00015.sst", &size);
  assert_eq(200ul, size);

  closebackupabledb();
}

// test various kind of corruptions that may happen:
// 1. not able to write a file for backup - that backup should fail,
//      everything else should work
// 2. corrupted/deleted latest_backup - everything should work fine
// 3. corrupted backup meta file or missing backuped file - we should
//      not be able to open that backup, but all other backups should be
//      fine
// 4. corrupted checksum value - if the checksum is not a valid uint32_t,
//      db open should fail, otherwise, it aborts during the restore process.
test(backupabledbtest, corruptionstest) {
  const int keys_iteration = 5000;
  random rnd(6);
  status s;

  openbackupabledb(true);
  // create five backups
  for (int i = 0; i < 5; ++i) {
    filldb(db_.get(), keys_iteration * i, keys_iteration * (i + 1));
    assert_ok(db_->createnewbackup(!!(rnd.next() % 2)));
  }

  // ---------- case 1. - fail a write -----------
  // try creating backup 6, but fail a write
  filldb(db_.get(), keys_iteration * 5, keys_iteration * 6);
  test_backup_env_->setlimitwrittenfiles(2);
  // should fail
  s = db_->createnewbackup(!!(rnd.next() % 2));
  assert_true(!s.ok());
  test_backup_env_->setlimitwrittenfiles(1000000);
  // latest backup should have all the keys
  closebackupabledb();
  assertbackupconsistency(0, 0, keys_iteration * 5, keys_iteration * 6);

  // ---------- case 2. - corrupt/delete latest backup -----------
  assert_ok(file_manager_->corruptfile(backupdir_ + "/latest_backup", 2));
  assertbackupconsistency(0, 0, keys_iteration * 5);
  assert_ok(file_manager_->deletefile(backupdir_ + "/latest_backup"));
  assertbackupconsistency(0, 0, keys_iteration * 5);
  // create backup 6, point latest_backup to 5
  openbackupabledb();
  filldb(db_.get(), keys_iteration * 5, keys_iteration * 6);
  assert_ok(db_->createnewbackup(false));
  closebackupabledb();
  assert_ok(file_manager_->writetofile(backupdir_ + "/latest_backup", "5"));
  assertbackupconsistency(0, 0, keys_iteration * 5, keys_iteration * 6);
  // assert that all 6 data is gone!
  assert_true(file_manager_->fileexists(backupdir_ + "/meta/6") == false);
  assert_true(file_manager_->fileexists(backupdir_ + "/private/6") == false);

  // --------- case 3. corrupted backup meta or missing backuped file ----
  assert_ok(file_manager_->corruptfile(backupdir_ + "/meta/5", 3));
  // since 5 meta is now corrupted, latest backup should be 4
  assertbackupconsistency(0, 0, keys_iteration * 4, keys_iteration * 5);
  openrestoredb();
  s = restore_db_->restoredbfrombackup(5, dbname_, dbname_);
  assert_true(!s.ok());
  closerestoredb();
  assert_ok(file_manager_->deleterandomfileindir(backupdir_ + "/private/4"));
  // 4 is corrupted, 3 is the latest backup now
  assertbackupconsistency(0, 0, keys_iteration * 3, keys_iteration * 5);
  openrestoredb();
  s = restore_db_->restoredbfrombackup(4, dbname_, dbname_);
  closerestoredb();
  assert_true(!s.ok());

  // --------- case 4. corrupted checksum value ----
  assert_ok(file_manager_->corruptchecksum(backupdir_ + "/meta/3", false));
  // checksum of backup 3 is an invalid value, this can be detected at
  // db open time, and it reverts to the previous backup automatically
  assertbackupconsistency(0, 0, keys_iteration * 2, keys_iteration * 5);
  // checksum of the backup 2 appears to be valid, this can cause checksum
  // mismatch and abort restore process
  assert_ok(file_manager_->corruptchecksum(backupdir_ + "/meta/2", true));
  assert_true(file_manager_->fileexists(backupdir_ + "/meta/2"));
  openrestoredb();
  assert_true(file_manager_->fileexists(backupdir_ + "/meta/2"));
  s = restore_db_->restoredbfrombackup(2, dbname_, dbname_);
  assert_true(!s.ok());
  assert_ok(restore_db_->deletebackup(2));
  closerestoredb();
  assertbackupconsistency(0, 0, keys_iteration * 1, keys_iteration * 5);

  // new backup should be 2!
  openbackupabledb();
  filldb(db_.get(), keys_iteration * 1, keys_iteration * 2);
  assert_ok(db_->createnewbackup(!!(rnd.next() % 2)));
  closebackupabledb();
  assertbackupconsistency(2, 0, keys_iteration * 2, keys_iteration * 5);
}

// open db, write, close db, backup, restore, repeat
test(backupabledbtest, offlineintegrationtest) {
  // has to be a big number, so that it triggers the memtable flush
  const int keys_iteration = 5000;
  const int max_key = keys_iteration * 4 + 10;
  // first iter -- flush before backup
  // second iter -- don't flush before backup
  for (int iter = 0; iter < 2; ++iter) {
    // delete old data
    destroydb(dbname_, options());
    bool destroy_data = true;

    // every iteration --
    // 1. insert new data in the db
    // 2. backup the db
    // 3. destroy the db
    // 4. restore the db, check everything is still there
    for (int i = 0; i < 5; ++i) {
      // in last iteration, put smaller amount of data,
      int fill_up_to = std::min(keys_iteration * (i + 1), max_key);
      // ---- insert new data and back up ----
      openbackupabledb(destroy_data);
      destroy_data = false;
      filldb(db_.get(), keys_iteration * i, fill_up_to);
      assert_ok(db_->createnewbackup(iter == 0));
      closebackupabledb();
      destroydb(dbname_, options());

      // ---- make sure it's empty ----
      db* db = opendb();
      assertempty(db, 0, fill_up_to);
      delete db;

      // ---- restore the db ----
      openrestoredb();
      if (i >= 3) { // test purge old backups
        // when i == 4, purge to only 1 backup
        // when i == 3, purge to 2 backups
        assert_ok(restore_db_->purgeoldbackups(5 - i));
      }
      // ---- make sure the data is there ---
      assertbackupconsistency(0, 0, fill_up_to, max_key);
      closerestoredb();
    }
  }
}

// open db, write, backup, write, backup, close, restore
test(backupabledbtest, onlineintegrationtest) {
  // has to be a big number, so that it triggers the memtable flush
  const int keys_iteration = 5000;
  const int max_key = keys_iteration * 4 + 10;
  random rnd(7);
  // delete old data
  destroydb(dbname_, options());

  openbackupabledb(true);
  // write some data, backup, repeat
  for (int i = 0; i < 5; ++i) {
    if (i == 4) {
      // delete backup number 2, online delete!
      openrestoredb();
      assert_ok(restore_db_->deletebackup(2));
      closerestoredb();
    }
    // in last iteration, put smaller amount of data,
    // so that backups can share sst files
    int fill_up_to = std::min(keys_iteration * (i + 1), max_key);
    filldb(db_.get(), keys_iteration * i, fill_up_to);
    // we should get consistent results with flush_before_backup
    // set to both true and false
    assert_ok(db_->createnewbackup(!!(rnd.next() % 2)));
  }
  // close and destroy
  closebackupabledb();
  destroydb(dbname_, options());

  // ---- make sure it's empty ----
  db* db = opendb();
  assertempty(db, 0, max_key);
  delete db;

  // ---- restore every backup and verify all the data is there ----
  openrestoredb();
  for (int i = 1; i <= 5; ++i) {
    if (i == 2) {
      // we deleted backup 2
      status s = restore_db_->restoredbfrombackup(2, dbname_, dbname_);
      assert_true(!s.ok());
    } else {
      int fill_up_to = std::min(keys_iteration * i, max_key);
      assertbackupconsistency(i, 0, fill_up_to, max_key);
    }
  }

  // delete some backups -- this should leave only backups 3 and 5 alive
  assert_ok(restore_db_->deletebackup(4));
  assert_ok(restore_db_->purgeoldbackups(2));

  std::vector<backupinfo> backup_info;
  restore_db_->getbackupinfo(&backup_info);
  assert_eq(2ul, backup_info.size());

  // check backup 3
  assertbackupconsistency(3, 0, 3 * keys_iteration, max_key);
  // check backup 5
  assertbackupconsistency(5, 0, max_key);

  closerestoredb();
}

test(backupabledbtest, failoverwritingbackups) {
  options_.write_buffer_size = 1024 * 1024 * 1024;  // 1gb
  // create backups 1, 2, 3, 4, 5
  openbackupabledb(true);
  for (int i = 0; i < 5; ++i) {
    closebackupabledb();
    deletelogfiles();
    openbackupabledb(false);
    filldb(db_.get(), 100 * i, 100 * (i + 1));
    assert_ok(db_->createnewbackup(true));
  }
  closebackupabledb();

  // restore 3
  openrestoredb();
  assert_ok(restore_db_->restoredbfrombackup(3, dbname_, dbname_));
  closerestoredb();

  openbackupabledb(false);
  filldb(db_.get(), 0, 300);
  status s = db_->createnewbackup(true);
  // the new backup fails because new table files
  // clash with old table files from backups 4 and 5
  // (since write_buffer_size is huge, we can be sure that
  // each backup will generate only one sst file and that
  // a file generated by a new backup is the same as
  // sst file generated by backup 4)
  assert_true(s.iscorruption());
  assert_ok(db_->deletebackup(4));
  assert_ok(db_->deletebackup(5));
  // now, the backup can succeed
  assert_ok(db_->createnewbackup(true));
  closebackupabledb();
}

test(backupabledbtest, nosharetablefiles) {
  const int keys_iteration = 5000;
  openbackupabledb(true, false, false);
  for (int i = 0; i < 5; ++i) {
    filldb(db_.get(), keys_iteration * i, keys_iteration * (i + 1));
    assert_ok(db_->createnewbackup(!!(i % 2)));
  }
  closebackupabledb();

  for (int i = 0; i < 5; ++i) {
    assertbackupconsistency(i + 1, 0, keys_iteration * (i + 1),
                            keys_iteration * 6);
  }
}

// verify that you can backup and restore with share_files_with_checksum on
test(backupabledbtest, sharetablefileswithchecksums) {
  const int keys_iteration = 5000;
  openbackupabledb(true, false, true, true);
  for (int i = 0; i < 5; ++i) {
    filldb(db_.get(), keys_iteration * i, keys_iteration * (i + 1));
    assert_ok(db_->createnewbackup(!!(i % 2)));
  }
  closebackupabledb();

  for (int i = 0; i < 5; ++i) {
    assertbackupconsistency(i + 1, 0, keys_iteration * (i + 1),
                            keys_iteration * 6);
  }
}

// verify that you can backup and restore using share_files_with_checksum set to
// false and then transition this option to true
test(backupabledbtest, sharetablefileswithchecksumstransition) {
  const int keys_iteration = 5000;
  // set share_files_with_checksum to false
  openbackupabledb(true, false, true, false);
  for (int i = 0; i < 5; ++i) {
    filldb(db_.get(), keys_iteration * i, keys_iteration * (i + 1));
    assert_ok(db_->createnewbackup(true));
  }
  closebackupabledb();

  for (int i = 0; i < 5; ++i) {
    assertbackupconsistency(i + 1, 0, keys_iteration * (i + 1),
                            keys_iteration * 6);
  }

  // set share_files_with_checksum to true and do some more backups
  openbackupabledb(true, false, true, true);
  for (int i = 5; i < 10; ++i) {
    filldb(db_.get(), keys_iteration * i, keys_iteration * (i + 1));
    assert_ok(db_->createnewbackup(true));
  }
  closebackupabledb();

  for (int i = 0; i < 5; ++i) {
    assertbackupconsistency(i + 1, 0, keys_iteration * (i + 5 + 1),
                            keys_iteration * 11);
  }
}

test(backupabledbtest, deletetmpfiles) {
  openbackupabledb();
  closebackupabledb();
  std::string shared_tmp = backupdir_ + "/shared/00006.sst.tmp";
  std::string private_tmp_dir = backupdir_ + "/private/10.tmp";
  std::string private_tmp_file = private_tmp_dir + "/00003.sst";
  file_manager_->writetofile(shared_tmp, "tmp");
  file_manager_->createdir(private_tmp_dir);
  file_manager_->writetofile(private_tmp_file, "tmp");
  assert_eq(true, file_manager_->fileexists(private_tmp_dir));
  openbackupabledb();
  closebackupabledb();
  assert_eq(false, file_manager_->fileexists(shared_tmp));
  assert_eq(false, file_manager_->fileexists(private_tmp_file));
  assert_eq(false, file_manager_->fileexists(private_tmp_dir));
}

test(backupabledbtest, keeplogfiles) {
  backupable_options_->backup_log_files = false;
  // basically infinite
  options_.wal_ttl_seconds = 24 * 60 * 60;
  openbackupabledb(true);
  filldb(db_.get(), 0, 100);
  assert_ok(db_->flush(flushoptions()));
  filldb(db_.get(), 100, 200);
  assert_ok(db_->createnewbackup(false));
  filldb(db_.get(), 200, 300);
  assert_ok(db_->flush(flushoptions()));
  filldb(db_.get(), 300, 400);
  assert_ok(db_->flush(flushoptions()));
  filldb(db_.get(), 400, 500);
  assert_ok(db_->flush(flushoptions()));
  closebackupabledb();

  // all data should be there if we call with keep_log_files = true
  assertbackupconsistency(0, 0, 500, 600, true);
}

test(backupabledbtest, ratelimiting) {
  uint64_t const kb = 1024 * 1024;
  size_t const kmicrospersec = 1000 * 1000ll;

  std::vector<std::pair<uint64_t, uint64_t>> limits(
      {{kb, 5 * kb}, {2 * kb, 3 * kb}});

  for (const auto& limit : limits) {
    // destroy old data
    destroydb(dbname_, options());

    backupable_options_->backup_rate_limit = limit.first;
    backupable_options_->restore_rate_limit = limit.second;
    options_.compression = knocompression;
    openbackupabledb(true);
    size_t bytes_written = filldb(db_.get(), 0, 100000);

    auto start_backup = env_->nowmicros();
    assert_ok(db_->createnewbackup(false));
    auto backup_time = env_->nowmicros() - start_backup;
    auto rate_limited_backup_time = (bytes_written * kmicrospersec) /
                                    backupable_options_->backup_rate_limit;
    assert_gt(backup_time, 0.9 * rate_limited_backup_time);

    closebackupabledb();

    openrestoredb();
    auto start_restore = env_->nowmicros();
    assert_ok(restore_db_->restoredbfromlatestbackup(dbname_, dbname_));
    auto restore_time = env_->nowmicros() - start_restore;
    closerestoredb();
    auto rate_limited_restore_time = (bytes_written * kmicrospersec) /
                                     backupable_options_->restore_rate_limit;
    assert_gt(restore_time, 0.9 * rate_limited_restore_time);

    assertbackupconsistency(0, 0, 100000, 100010);
  }
}

test(backupabledbtest, readonlybackupengine) {
  destroydb(dbname_, options());
  openbackupabledb(true);
  filldb(db_.get(), 0, 100);
  assert_ok(db_->createnewbackup(true));
  filldb(db_.get(), 100, 200);
  assert_ok(db_->createnewbackup(true));
  closebackupabledb();
  destroydb(dbname_, options());

  backupable_options_->destroy_old_data = false;
  test_backup_env_->clearwrittenfiles();
  test_backup_env_->setlimitdeletefiles(0);
  auto read_only_backup_engine =
      backupenginereadonly::newreadonlybackupengine(env_, *backupable_options_);
  std::vector<backupinfo> backup_info;
  read_only_backup_engine->getbackupinfo(&backup_info);
  assert_eq(backup_info.size(), 2u);

  restoreoptions restore_options(false);
  assert_ok(read_only_backup_engine->restoredbfromlatestbackup(
      dbname_, dbname_, restore_options));
  delete read_only_backup_engine;
  std::vector<std::string> should_have_written;
  test_backup_env_->assertwrittenfiles(should_have_written);

  db* db = opendb();
  assertexists(db, 0, 200);
  delete db;
}

}  // anon namespace

} //  namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
