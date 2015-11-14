//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "rocksdb/db.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "rocksdb/cache.h"
#include "rocksdb/env.h"
#include "rocksdb/table.h"
#include "rocksdb/write_batch.h"
#include "db/db_impl.h"
#include "db/filename.h"
#include "db/log_format.h"
#include "db/version_set.h"
#include "util/logging.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

static const int kvaluesize = 1000;

class corruptiontest {
 public:
  test::errorenv env_;
  std::string dbname_;
  shared_ptr<cache> tiny_cache_;
  options options_;
  db* db_;

  corruptiontest() {
    tiny_cache_ = newlrucache(100);
    options_.env = &env_;
    dbname_ = test::tmpdir() + "/corruption_test";
    destroydb(dbname_, options_);

    db_ = nullptr;
    options_.create_if_missing = true;
    blockbasedtableoptions table_options;
    table_options.block_size_deviation = 0;  // make unit test pass for now
    options_.table_factory.reset(newblockbasedtablefactory(table_options));
    reopen();
    options_.create_if_missing = false;
  }

  ~corruptiontest() {
     delete db_;
     destroydb(dbname_, options());
  }

  status tryreopen(options* options = nullptr) {
    delete db_;
    db_ = nullptr;
    options opt = (options ? *options : options_);
    opt.env = &env_;
    opt.arena_block_size = 4096;
    blockbasedtableoptions table_options;
    table_options.block_cache = tiny_cache_;
    table_options.block_size_deviation = 0;
    opt.table_factory.reset(newblockbasedtablefactory(table_options));
    return db::open(opt, dbname_, &db_);
  }

  void reopen(options* options = nullptr) {
    assert_ok(tryreopen(options));
  }

  void repairdb() {
    delete db_;
    db_ = nullptr;
    assert_ok(::rocksdb::repairdb(dbname_, options_));
  }

  void build(int n) {
    std::string key_space, value_space;
    writebatch batch;
    for (int i = 0; i < n; i++) {
      //if ((i % 100) == 0) fprintf(stderr, "@ %d of %d\n", i, n);
      slice key = key(i, &key_space);
      batch.clear();
      batch.put(key, value(i, &value_space));
      assert_ok(db_->write(writeoptions(), &batch));
    }
  }

  void check(int min_expected, int max_expected) {
    unsigned int next_expected = 0;
    int missed = 0;
    int bad_keys = 0;
    int bad_values = 0;
    int correct = 0;
    std::string value_space;
    // do not verify checksums. if we verify checksums then the
    // db itself will raise errors because data is corrupted.
    // instead, we want the reads to be successful and this test
    // will detect whether the appropriate corruptions have
    // occured.
    iterator* iter = db_->newiterator(readoptions(false, true));
    for (iter->seektofirst(); iter->valid(); iter->next()) {
      uint64_t key;
      slice in(iter->key());
      if (!consumedecimalnumber(&in, &key) ||
          !in.empty() ||
          key < next_expected) {
        bad_keys++;
        continue;
      }
      missed += (key - next_expected);
      next_expected = key + 1;
      if (iter->value() != value(key, &value_space)) {
        bad_values++;
      } else {
        correct++;
      }
    }
    delete iter;

    fprintf(stderr,
            "expected=%d..%d; got=%d; bad_keys=%d; bad_values=%d; missed=%d\n",
            min_expected, max_expected, correct, bad_keys, bad_values, missed);
    assert_le(min_expected, correct);
    assert_ge(max_expected, correct);
  }

  void corruptfile(const std::string fname, int offset, int bytes_to_corrupt) {
    struct stat sbuf;
    if (stat(fname.c_str(), &sbuf) != 0) {
      const char* msg = strerror(errno);
      assert_true(false) << fname << ": " << msg;
    }

    if (offset < 0) {
      // relative to end of file; make it absolute
      if (-offset > sbuf.st_size) {
        offset = 0;
      } else {
        offset = sbuf.st_size + offset;
      }
    }
    if (offset > sbuf.st_size) {
      offset = sbuf.st_size;
    }
    if (offset + bytes_to_corrupt > sbuf.st_size) {
      bytes_to_corrupt = sbuf.st_size - offset;
    }

    // do it
    std::string contents;
    status s = readfiletostring(env::default(), fname, &contents);
    assert_true(s.ok()) << s.tostring();
    for (int i = 0; i < bytes_to_corrupt; i++) {
      contents[i + offset] ^= 0x80;
    }
    s = writestringtofile(env::default(), contents, fname);
    assert_true(s.ok()) << s.tostring();
  }

  void corrupt(filetype filetype, int offset, int bytes_to_corrupt) {
    // pick file to corrupt
    std::vector<std::string> filenames;
    assert_ok(env_.getchildren(dbname_, &filenames));
    uint64_t number;
    filetype type;
    std::string fname;
    int picked_number = -1;
    for (unsigned int i = 0; i < filenames.size(); i++) {
      if (parsefilename(filenames[i], &number, &type) &&
          type == filetype &&
          static_cast<int>(number) > picked_number) {  // pick latest file
        fname = dbname_ + "/" + filenames[i];
        picked_number = number;
      }
    }
    assert_true(!fname.empty()) << filetype;

    corruptfile(fname, offset, bytes_to_corrupt);
  }

  // corrupts exactly one file at level `level`. if no file found at level,
  // asserts
  void corrupttablefileatlevel(int level, int offset, int bytes_to_corrupt) {
    std::vector<livefilemetadata> metadata;
    db_->getlivefilesmetadata(&metadata);
    for (const auto& m : metadata) {
      if (m.level == level) {
        corruptfile(dbname_ + "/" + m.name, offset, bytes_to_corrupt);
        return;
      }
    }
    assert_true(false) << "no file found at level";
  }


  int property(const std::string& name) {
    std::string property;
    int result;
    if (db_->getproperty(name, &property) &&
        sscanf(property.c_str(), "%d", &result) == 1) {
      return result;
    } else {
      return -1;
    }
  }

  // return the ith key
  slice key(int i, std::string* storage) {
    char buf[100];
    snprintf(buf, sizeof(buf), "%016d", i);
    storage->assign(buf, strlen(buf));
    return slice(*storage);
  }

  // return the value to associate with the specified key
  slice value(int k, std::string* storage) {
    random r(k);
    return test::randomstring(&r, kvaluesize, storage);
  }
};

test(corruptiontest, recovery) {
  build(100);
  check(100, 100);
  corrupt(klogfile, 19, 1);      // writebatch tag for first record
  corrupt(klogfile, log::kblocksize + 1000, 1);  // somewhere in second block
  reopen();

  // the 64 records in the first two log blocks are completely lost.
  check(36, 36);
}

test(corruptiontest, recoverwriteerror) {
  env_.writable_file_error_ = true;
  status s = tryreopen();
  assert_true(!s.ok());
}

test(corruptiontest, newfileerrorduringwrite) {
  // do enough writing to force minor compaction
  env_.writable_file_error_ = true;
  const int num = 3 + (options().write_buffer_size / kvaluesize);
  std::string value_storage;
  status s;
  bool failed = false;
  for (int i = 0; i < num; i++) {
    writebatch batch;
    batch.put("a", value(100, &value_storage));
    s = db_->write(writeoptions(), &batch);
    if (!s.ok()) {
      failed = true;
    }
    assert_true(!failed || !s.ok());
  }
  assert_true(!s.ok());
  assert_ge(env_.num_writable_file_errors_, 1);
  env_.writable_file_error_ = false;
  reopen();
}

test(corruptiontest, tablefile) {
  build(100);
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);
  dbi->test_flushmemtable();
  dbi->test_compactrange(0, nullptr, nullptr);
  dbi->test_compactrange(1, nullptr, nullptr);

  corrupt(ktablefile, 100, 1);
  check(99, 99);
}

test(corruptiontest, tablefileindexdata) {
  build(10000);  // enough to build multiple tables
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);
  dbi->test_flushmemtable();

  corrupt(ktablefile, -2000, 500);
  reopen();
  check(5000, 9999);
}

test(corruptiontest, missingdescriptor) {
  build(1000);
  repairdb();
  reopen();
  check(1000, 1000);
}

test(corruptiontest, sequencenumberrecovery) {
  assert_ok(db_->put(writeoptions(), "foo", "v1"));
  assert_ok(db_->put(writeoptions(), "foo", "v2"));
  assert_ok(db_->put(writeoptions(), "foo", "v3"));
  assert_ok(db_->put(writeoptions(), "foo", "v4"));
  assert_ok(db_->put(writeoptions(), "foo", "v5"));
  repairdb();
  reopen();
  std::string v;
  assert_ok(db_->get(readoptions(), "foo", &v));
  assert_eq("v5", v);
  // write something.  if sequence number was not recovered properly,
  // it will be hidden by an earlier write.
  assert_ok(db_->put(writeoptions(), "foo", "v6"));
  assert_ok(db_->get(readoptions(), "foo", &v));
  assert_eq("v6", v);
  reopen();
  assert_ok(db_->get(readoptions(), "foo", &v));
  assert_eq("v6", v);
}

test(corruptiontest, corrupteddescriptor) {
  assert_ok(db_->put(writeoptions(), "foo", "hello"));
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);
  dbi->test_flushmemtable();
  dbi->test_compactrange(0, nullptr, nullptr);

  corrupt(kdescriptorfile, 0, 1000);
  status s = tryreopen();
  assert_true(!s.ok());

  repairdb();
  reopen();
  std::string v;
  assert_ok(db_->get(readoptions(), "foo", &v));
  assert_eq("hello", v);
}

test(corruptiontest, compactioninputerror) {
  build(10);
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);
  dbi->test_flushmemtable();
  const int last = dbi->maxmemcompactionlevel();
  assert_eq(1, property("rocksdb.num-files-at-level" + numbertostring(last)));

  corrupt(ktablefile, 100, 1);
  check(9, 9);

  // force compactions by writing lots of values
  build(10000);
  check(10000, 10000);
}

test(corruptiontest, compactioninputerrorparanoid) {
  options options;
  options.paranoid_checks = true;
  options.write_buffer_size = 131072;
  options.max_write_buffer_number = 2;
  reopen(&options);
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);

  // fill levels >= 1 so memtable flush outputs to level 0
  for (int level = 1; level < dbi->numberlevels(); level++) {
    dbi->put(writeoptions(), "", "begin");
    dbi->put(writeoptions(), "~", "end");
    dbi->test_flushmemtable();
  }

  options.max_mem_compaction_level = 0;
  reopen(&options);

  dbi = reinterpret_cast<dbimpl*>(db_);
  build(10);
  dbi->test_flushmemtable();
  dbi->test_waitforcompact();
  assert_eq(1, property("rocksdb.num-files-at-level0"));

  corrupttablefileatlevel(0, 100, 1);
  check(9, 9);

  // write must eventually fail because of corrupted table
  status s;
  std::string tmp1, tmp2;
  bool failed = false;
  for (int i = 0; i < 10000; i++) {
    s = db_->put(writeoptions(), key(i, &tmp1), value(i, &tmp2));
    if (!s.ok()) {
      failed = true;
    }
    // if one write failed, every subsequent write must fail, too
    assert_true(!failed || !s.ok()) << "write did not fail in a corrupted db";
  }
  assert_true(!s.ok()) << "write did not fail in corrupted paranoid db";
}

test(corruptiontest, unrelatedkeys) {
  build(10);
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);
  dbi->test_flushmemtable();
  corrupt(ktablefile, 100, 1);

  std::string tmp1, tmp2;
  assert_ok(db_->put(writeoptions(), key(1000, &tmp1), value(1000, &tmp2)));
  std::string v;
  assert_ok(db_->get(readoptions(), key(1000, &tmp1), &v));
  assert_eq(value(1000, &tmp2).tostring(), v);
  dbi->test_flushmemtable();
  assert_ok(db_->get(readoptions(), key(1000, &tmp1), &v));
  assert_eq(value(1000, &tmp2).tostring(), v);
}

test(corruptiontest, filesystemstatecorrupted) {
  for (int iter = 0; iter < 2; ++iter) {
    options options;
    options.paranoid_checks = true;
    options.create_if_missing = true;
    reopen(&options);
    build(10);
    assert_ok(db_->flush(flushoptions()));
    dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);
    std::vector<livefilemetadata> metadata;
    dbi->getlivefilesmetadata(&metadata);
    assert_gt(metadata.size(), size_t(0));
    std::string filename = dbname_ + metadata[0].name;

    delete db_;
    db_ = nullptr;

    if (iter == 0) {  // corrupt file size
      unique_ptr<writablefile> file;
      env_.newwritablefile(filename, &file, envoptions());
      file->append(slice("corrupted sst"));
      file.reset();
    } else {  // delete the file
      env_.deletefile(filename);
    }

    status x = tryreopen(&options);
    assert_true(x.iscorruption());
    destroydb(dbname_, options_);
    reopen(&options);
  }
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
