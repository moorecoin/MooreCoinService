//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <memory>

#include "include/rocksdb/db.h"
#include "include/rocksdb/options.h"
#include "include/rocksdb/env.h"
#include "include/rocksdb/slice.h"
#include "include/rocksdb/status.h"
#include "include/rocksdb/comparator.h"
#include "include/rocksdb/table.h"
#include "include/rocksdb/slice_transform.h"

namespace rocksdb {

class sanitytest {
 public:
  explicit sanitytest(const std::string& path)
      : env_(env::default()), path_(path) {
    env_->createdirifmissing(path);
  }
  virtual ~sanitytest() {}

  virtual std::string name() const = 0;
  virtual options getoptions() const = 0;

  status create() {
    options options = getoptions();
    options.create_if_missing = true;
    std::string dbname = path_ + name();
    destroydb(dbname, options);
    db* db;
    status s = db::open(options, dbname, &db);
    std::unique_ptr<db> db_guard(db);
    if (!s.ok()) {
      return s;
    }
    for (int i = 0; i < 1000000; ++i) {
      std::string k = "key" + std::to_string(i);
      std::string v = "value" + std::to_string(i);
      s = db->put(writeoptions(), slice(k), slice(v));
      if (!s.ok()) {
        return s;
      }
    }
    return status::ok();
  }
  status verify() {
    db* db;
    std::string dbname = path_ + name();
    status s = db::open(getoptions(), dbname, &db);
    std::unique_ptr<db> db_guard(db);
    if (!s.ok()) {
      return s;
    }
    for (int i = 0; i < 1000000; ++i) {
      std::string k = "key" + std::to_string(i);
      std::string v = "value" + std::to_string(i);
      std::string result;
      s = db->get(readoptions(), slice(k), &result);
      if (!s.ok()) {
        return s;
      }
      if (result != v) {
        return status::corruption("unexpected value for key " + k);
      }
    }
    return status::ok();
  }

 private:
  env* env_;
  std::string const path_;
};

class sanitytestbasic : public sanitytest {
 public:
  explicit sanitytestbasic(const std::string& path) : sanitytest(path) {}
  virtual options getoptions() const {
    options options;
    options.create_if_missing = true;
    return options;
  }
  virtual std::string name() const { return "basic"; }
};

class sanitytestspecialcomparator : public sanitytest {
 public:
  explicit sanitytestspecialcomparator(const std::string& path)
      : sanitytest(path) {
    options_.comparator = new newcomparator();
  }
  ~sanitytestspecialcomparator() { delete options_.comparator; }
  virtual options getoptions() const { return options_; }
  virtual std::string name() const { return "specialcomparator"; }

 private:
  class newcomparator : public comparator {
   public:
    virtual const char* name() const { return "rocksdb.newcomparator"; }
    virtual int compare(const slice& a, const slice& b) const {
      return bytewisecomparator()->compare(a, b);
    }
    virtual void findshortestseparator(std::string* s, const slice& l) const {
      bytewisecomparator()->findshortestseparator(s, l);
    }
    virtual void findshortsuccessor(std::string* key) const {
      bytewisecomparator()->findshortsuccessor(key);
    }
  };
  options options_;
};

class sanitytestzlibcompression : public sanitytest {
 public:
  explicit sanitytestzlibcompression(const std::string& path)
      : sanitytest(path) {
    options_.compression = kzlibcompression;
  }
  virtual options getoptions() const { return options_; }
  virtual std::string name() const { return "zlibcompression"; }

 private:
  options options_;
};

class sanitytestplaintablefactory : public sanitytest {
 public:
  explicit sanitytestplaintablefactory(const std::string& path)
      : sanitytest(path) {
    options_.table_factory.reset(newplaintablefactory());
    options_.prefix_extractor.reset(newfixedprefixtransform(2));
    options_.allow_mmap_reads = true;
  }
  ~sanitytestplaintablefactory() {}
  virtual options getoptions() const { return options_; }
  virtual std::string name() const { return "plaintable"; }

 private:
  options options_;
};

namespace {
bool runsanitytests(const std::string& command, const std::string& path) {
  std::vector<sanitytest*> sanity_tests = {
      new sanitytestbasic(path),
      new sanitytestspecialcomparator(path),
      new sanitytestzlibcompression(path),
      new sanitytestplaintablefactory(path)};

  if (command == "create") {
    fprintf(stderr, "creating...\n");
  } else {
    fprintf(stderr, "verifying...\n");
  }
  for (auto sanity_test : sanity_tests) {
    status s;
    fprintf(stderr, "%s -- ", sanity_test->name().c_str());
    if (command == "create") {
      s = sanity_test->create();
    } else {
      assert(command == "verify");
      s = sanity_test->verify();
    }
    fprintf(stderr, "%s\n", s.tostring().c_str());
    if (!s.ok()) {
      fprintf(stderr, "fail\n");
      return false;
    }

    delete sanity_test;
  }
  return true;
}
}  // namespace

}  // namespace rocksdb

int main(int argc, char** argv) {
  std::string path, command;
  bool ok = (argc == 3);
  if (ok) {
    path = std::string(argv[1]);
    command = std::string(argv[2]);
    ok = (command == "create" || command == "verify");
  }
  if (!ok) {
    fprintf(stderr, "usage: %s <path> [create|verify] \n", argv[0]);
    exit(1);
  }
  if (path.back() != '/') {
    path += "/";
  }

  bool sanity_ok = rocksdb::runsanitytests(command, path);

  return sanity_ok ? 0 : 1;
}
