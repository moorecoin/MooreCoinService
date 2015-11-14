//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.


#include <vector>

#include "util/testharness.h"
#include "util/benchharness.h"
#include "db/version_set.h"
#include "util/mutexlock.h"

namespace rocksdb {

std::string makekey(unsigned int num) {
  char buf[30];
  snprintf(buf, sizeof(buf), "%016u", num);
  return std::string(buf);
}

void bm_logandapply(int iters, int num_base_files) {
  versionset* vset;
  columnfamilydata* default_cfd;
  uint64_t fnum = 1;
  port::mutex mu;
  mutexlock l(&mu);

  benchmark_suspend {
    std::string dbname = test::tmpdir() + "/rocksdb_test_benchmark";
    assert_ok(destroydb(dbname, options()));

    db* db = nullptr;
    options opts;
    opts.create_if_missing = true;
    status s = db::open(opts, dbname, &db);
    assert_ok(s);
    assert_true(db != nullptr);

    delete db;
    db = nullptr;

    options options;
    envoptions sopt;
    // notice we are using the default options not through sanitizeoptions().
    // we might want to initialize some options manually if needed.
    options.db_paths.emplace_back(dbname, 0);
    // the parameter of table cache is passed in as null, so any file i/o
    // operation is likely to fail.
    vset = new versionset(dbname, &options, sopt, nullptr);
    std::vector<columnfamilydescriptor> dummy;
    dummy.push_back(columnfamilydescriptor());
    assert_ok(vset->recover(dummy));
    default_cfd = vset->getcolumnfamilyset()->getdefault();
    versionedit vbase;
    for (int i = 0; i < num_base_files; i++) {
      internalkey start(makekey(2 * fnum), 1, ktypevalue);
      internalkey limit(makekey(2 * fnum + 1), 1, ktypedeletion);
      vbase.addfile(2, ++fnum, 0, 1 /* file size */, start, limit, 1, 1);
    }
    assert_ok(vset->logandapply(default_cfd, &vbase, &mu));
  }

  for (int i = 0; i < iters; i++) {
    versionedit vedit;
    vedit.deletefile(2, fnum);
    internalkey start(makekey(2 * fnum), 1, ktypevalue);
    internalkey limit(makekey(2 * fnum + 1), 1, ktypedeletion);
    vedit.addfile(2, ++fnum, 0, 1 /* file size */, start, limit, 1, 1);
    vset->logandapply(default_cfd, &vedit, &mu);
  }
}

benchmark_named_param(bm_logandapply, 1000_iters_1_file, 1000, 1)
benchmark_named_param(bm_logandapply, 1000_iters_100_files, 1000, 100)
benchmark_named_param(bm_logandapply, 1000_iters_10000_files, 1000, 10000)
benchmark_named_param(bm_logandapply, 100_iters_100000_files, 100, 100000)

}  // namespace rocksdb

int main(int argc, char** argv) {
  rocksdb::benchmark::runbenchmarks();
  return 0;
}
