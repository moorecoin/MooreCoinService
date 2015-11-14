//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include "rocksdb/status.h"
#include "rocksdb/env.h"

#include <vector>
#include "util/coding.h"
#include "util/testharness.h"

namespace rocksdb {

class locktest {
 public:
  static locktest* current_;
  std::string file_;
  rocksdb::env* env_;

  locktest() : file_(test::tmpdir() + "/db_testlock_file"),
               env_(rocksdb::env::default()) {
    current_ = this;
  }

  ~locktest() {
  }

  status lockfile(filelock** db_lock) {
    return env_->lockfile(file_, db_lock);
  }

  status unlockfile(filelock* db_lock) {
    return env_->unlockfile(db_lock);
  }
};
locktest* locktest::current_;

test(locktest, lockbysamethread) {
  filelock* lock1;
  filelock* lock2;

  // acquire a lock on a file
  assert_ok(lockfile(&lock1));

  // re-acquire the lock on the same file. this should fail.
  assert_true(lockfile(&lock2).isioerror());

  // release the lock
  assert_ok(unlockfile(lock1));

}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
