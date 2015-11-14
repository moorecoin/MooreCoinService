//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "rocksdb/db.h"
#include "db/db_impl.h"
#include "db/filename.h"
#include "db/version_set.h"
#include "db/write_batch_internal.h"
#include "util/testharness.h"
#include "util/testutil.h"
#include "rocksdb/env.h"
#include "rocksdb/transaction_log.h"
#include <vector>
#include <stdlib.h>
#include <map>
#include <string>

namespace rocksdb {

class deletefiletest {
 public:
  std::string dbname_;
  options options_;
  db* db_;
  env* env_;
  int numlevels_;

  deletefiletest() {
    db_ = nullptr;
    env_ = env::default();
    options_.write_buffer_size = 1024*1024*1000;
    options_.target_file_size_base = 1024*1024*1000;
    options_.max_bytes_for_level_base = 1024*1024*1000;
    options_.wal_ttl_seconds = 300; // used to test log files
    options_.wal_size_limit_mb = 1024; // used to test log files
    dbname_ = test::tmpdir() + "/deletefile_test";
    options_.wal_dir = dbname_ + "/wal_files";

    // clean up all the files that might have been there before
    std::vector<std::string> old_files;
    env_->getchildren(dbname_, &old_files);
    for (auto file : old_files) {
      env_->deletefile(dbname_ + "/" + file);
    }
    env_->getchildren(options_.wal_dir, &old_files);
    for (auto file : old_files) {
      env_->deletefile(options_.wal_dir + "/" + file);
    }

    destroydb(dbname_, options_);
    numlevels_ = 7;
    assert_ok(reopendb(true));
  }

  status reopendb(bool create) {
    delete db_;
    if (create) {
      destroydb(dbname_, options_);
    }
    db_ = nullptr;
    options_.create_if_missing = create;
    return db::open(options_, dbname_, &db_);
  }

  void closedb() {
    delete db_;
  }

  void addkeys(int numkeys, int startkey = 0) {
    writeoptions options;
    options.sync = false;
    readoptions roptions;
    for (int i = startkey; i < (numkeys + startkey) ; i++) {
      std::string temp = std::to_string(i);
      slice key(temp);
      slice value(temp);
      assert_ok(db_->put(options, key, value));
    }
  }

  int numkeysinlevels(
    std::vector<livefilemetadata> &metadata,
    std::vector<int> *keysperlevel = nullptr) {

    if (keysperlevel != nullptr) {
      keysperlevel->resize(numlevels_);
    }

    int numkeys = 0;
    for (size_t i = 0; i < metadata.size(); i++) {
      int startkey = atoi(metadata[i].smallestkey.c_str());
      int endkey = atoi(metadata[i].largestkey.c_str());
      int numkeysinfile = (endkey - startkey + 1);
      numkeys += numkeysinfile;
      if (keysperlevel != nullptr) {
        (*keysperlevel)[(int)metadata[i].level] += numkeysinfile;
      }
      fprintf(stderr, "level %d name %s smallest %s largest %s\n",
              metadata[i].level, metadata[i].name.c_str(),
              metadata[i].smallestkey.c_str(),
              metadata[i].largestkey.c_str());
    }
    return numkeys;
  }

  void createtwolevels() {
    addkeys(50000, 10000);
    dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);
    assert_ok(dbi->test_flushmemtable());
    assert_ok(dbi->test_waitforflushmemtable());

    addkeys(50000, 10000);
    assert_ok(dbi->test_flushmemtable());
    assert_ok(dbi->test_waitforflushmemtable());
  }

  void checkfiletypecounts(std::string& dir,
                            int required_log,
                            int required_sst,
                            int required_manifest) {
    std::vector<std::string> filenames;
    env_->getchildren(dir, &filenames);

    int log_cnt = 0, sst_cnt = 0, manifest_cnt = 0;
    for (auto file : filenames) {
      uint64_t number;
      filetype type;
      if (parsefilename(file, &number, &type)) {
        log_cnt += (type == klogfile);
        sst_cnt += (type == ktablefile);
        manifest_cnt += (type == kdescriptorfile);
      }
    }
    assert_eq(required_log, log_cnt);
    assert_eq(required_sst, sst_cnt);
    assert_eq(required_manifest, manifest_cnt);
  }

};

test(deletefiletest, addkeysandquerylevels) {
  createtwolevels();
  std::vector<livefilemetadata> metadata;
  std::vector<int> keysinlevel;
  db_->getlivefilesmetadata(&metadata);

  std::string level1file = "";
  int level1keycount = 0;
  std::string level2file = "";
  int level2keycount = 0;
  int level1index = 0;
  int level2index = 1;

  assert_eq((int)metadata.size(), 2);
  if (metadata[0].level == 2) {
    level1index = 1;
    level2index = 0;
  }

  level1file = metadata[level1index].name;
  int startkey = atoi(metadata[level1index].smallestkey.c_str());
  int endkey = atoi(metadata[level1index].largestkey.c_str());
  level1keycount = (endkey - startkey + 1);
  level2file = metadata[level2index].name;
  startkey = atoi(metadata[level2index].smallestkey.c_str());
  endkey = atoi(metadata[level2index].largestkey.c_str());
  level2keycount = (endkey - startkey + 1);

  // controlled setup. levels 1 and 2 should both have 50k files.
  // this is a little fragile as it depends on the current
  // compaction heuristics.
  assert_eq(level1keycount, 50000);
  assert_eq(level2keycount, 50000);

  status status = db_->deletefile("0.sst");
  assert_true(status.isinvalidargument());

  // intermediate level files cannot be deleted.
  status = db_->deletefile(level1file);
  assert_true(status.isinvalidargument());

  // lowest level file deletion should succeed.
  assert_ok(db_->deletefile(level2file));

  closedb();
}

test(deletefiletest, purgeobsoletefilestest) {
  createtwolevels();
  // there should be only one (empty) log file because createtwolevels()
  // flushes the memtables to disk
  checkfiletypecounts(options_.wal_dir, 1, 0, 0);
  // 2 ssts, 1 manifest
  checkfiletypecounts(dbname_, 0, 2, 1);
  std::string first("0"), last("999999");
  slice first_slice(first), last_slice(last);
  db_->compactrange(&first_slice, &last_slice, true, 2);
  // 1 sst after compaction
  checkfiletypecounts(dbname_, 0, 1, 1);

  // this time, we keep an iterator alive
  reopendb(true);
  iterator *itr = 0;
  createtwolevels();
  itr = db_->newiterator(readoptions());
  db_->compactrange(&first_slice, &last_slice, true, 2);
  // 3 sst after compaction with live iterator
  checkfiletypecounts(dbname_, 0, 3, 1);
  delete itr;
  // 1 sst after iterator deletion
  checkfiletypecounts(dbname_, 0, 1, 1);

  closedb();
}

test(deletefiletest, deletefilewithiterator) {
  createtwolevels();
  readoptions options;
  iterator* it = db_->newiterator(options);
  std::vector<livefilemetadata> metadata;
  db_->getlivefilesmetadata(&metadata);

  std::string level2file = "";

  assert_eq((int)metadata.size(), 2);
  if (metadata[0].level == 1) {
    level2file = metadata[1].name;
  } else {
    level2file = metadata[0].name;
  }

  status status = db_->deletefile(level2file);
  fprintf(stdout, "deletion status %s: %s\n",
          level2file.c_str(), status.tostring().c_str());
  assert_true(status.ok());
  it->seektofirst();
  int numkeysiterated = 0;
  while(it->valid()) {
    numkeysiterated++;
    it->next();
  }
  assert_eq(numkeysiterated, 50000);
  delete it;
  closedb();
}

test(deletefiletest, deletelogfiles) {
  addkeys(10, 0);
  vectorlogptr logfiles;
  db_->getsortedwalfiles(logfiles);
  assert_gt(logfiles.size(), 0ul);
  // take the last log file which is expected to be alive and try to delete it
  // should not succeed because live logs are not allowed to be deleted
  std::unique_ptr<logfile> alive_log = std::move(logfiles.back());
  assert_eq(alive_log->type(), kalivelogfile);
  assert_true(env_->fileexists(options_.wal_dir + "/" + alive_log->pathname()));
  fprintf(stdout, "deleting alive log file %s\n",
          alive_log->pathname().c_str());
  assert_true(!db_->deletefile(alive_log->pathname()).ok());
  assert_true(env_->fileexists(options_.wal_dir + "/" + alive_log->pathname()));
  logfiles.clear();

  // call flush to bring about a new working log file and add more keys
  // call flush again to flush out memtable and move alive log to archived log
  // and try to delete the archived log file
  flushoptions fopts;
  db_->flush(fopts);
  addkeys(10, 0);
  db_->flush(fopts);
  db_->getsortedwalfiles(logfiles);
  assert_gt(logfiles.size(), 0ul);
  std::unique_ptr<logfile> archived_log = std::move(logfiles.front());
  assert_eq(archived_log->type(), karchivedlogfile);
  assert_true(env_->fileexists(options_.wal_dir + "/" +
        archived_log->pathname()));
  fprintf(stdout, "deleting archived log file %s\n",
          archived_log->pathname().c_str());
  assert_ok(db_->deletefile(archived_log->pathname()));
  assert_true(!env_->fileexists(options_.wal_dir + "/" +
        archived_log->pathname()));
  closedb();
}

} //namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}

