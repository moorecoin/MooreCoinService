//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/filename.h"

#include "db/dbformat.h"
#include "port/port.h"
#include "util/logging.h"
#include "util/testharness.h"

namespace rocksdb {

class filenametest { };

test(filenametest, parse) {
  slice db;
  filetype type;
  uint64_t number;

  char kdefautinfologdir = 1;
  char kdifferentinfologdir = 2;
  char knochecklogdir = 4;
  char kallmode = kdefautinfologdir | kdifferentinfologdir | knochecklogdir;

  // successful parses
  static struct {
    const char* fname;
    uint64_t number;
    filetype type;
    char mode;
  } cases[] = {
        {"100.log", 100, klogfile, kallmode},
        {"0.log", 0, klogfile, kallmode},
        {"0.sst", 0, ktablefile, kallmode},
        {"current", 0, kcurrentfile, kallmode},
        {"lock", 0, kdblockfile, kallmode},
        {"manifest-2", 2, kdescriptorfile, kallmode},
        {"manifest-7", 7, kdescriptorfile, kallmode},
        {"metadb-2", 2, kmetadatabase, kallmode},
        {"metadb-7", 7, kmetadatabase, kallmode},
        {"log", 0, kinfologfile, kdefautinfologdir},
        {"log.old", 0, kinfologfile, kdefautinfologdir},
        {"log.old.6688", 6688, kinfologfile, kdefautinfologdir},
        {"rocksdb_dir_log", 0, kinfologfile, kdifferentinfologdir},
        {"rocksdb_dir_log.old", 0, kinfologfile, kdifferentinfologdir},
        {"rocksdb_dir_log.old.6688", 6688, kinfologfile, kdifferentinfologdir},
        {"18446744073709551615.log", 18446744073709551615ull, klogfile,
         kallmode}, };
  for (char mode : {kdifferentinfologdir, kdefautinfologdir, knochecklogdir}) {
    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
      infologprefix info_log_prefix(mode != kdefautinfologdir, "/rocksdb/dir");
      if (cases[i].mode & mode) {
        std::string f = cases[i].fname;
        if (mode == knochecklogdir) {
          assert_true(parsefilename(f, &number, &type)) << f;
        } else {
          assert_true(parsefilename(f, &number, info_log_prefix.prefix, &type))
              << f;
        }
        assert_eq(cases[i].type, type) << f;
        assert_eq(cases[i].number, number) << f;
      }
    }
  }

  // errors
  static const char* errors[] = {
    "",
    "foo",
    "foo-dx-100.log",
    ".log",
    "",
    "manifest",
    "curren",
    "currentx",
    "manifes",
    "manifest",
    "manifest-",
    "xmanifest-3",
    "manifest-3x",
    "meta",
    "metadb",
    "metadb-",
    "xmetadb-3",
    "metadb-3x",
    "loc",
    "lockx",
    "lo",
    "logx",
    "18446744073709551616.log",
    "184467440737095516150.log",
    "100",
    "100.",
    "100.lop"
  };
  for (unsigned int i = 0; i < sizeof(errors) / sizeof(errors[0]); i++) {
    std::string f = errors[i];
    assert_true(!parsefilename(f, &number, &type)) << f;
  };
}

test(filenametest, infologfilename) {
  std::string dbname = ("/data/rocksdb");
  std::string db_absolute_path;
  env::default()->getabsolutepath(dbname, &db_absolute_path);

  assert_eq("/data/rocksdb/log", infologfilename(dbname, db_absolute_path, ""));
  assert_eq("/data/rocksdb/log.old.666",
            oldinfologfilename(dbname, 666u, db_absolute_path, ""));

  assert_eq("/data/rocksdb_log/data_rocksdb_log",
            infologfilename(dbname, db_absolute_path, "/data/rocksdb_log"));
  assert_eq(
      "/data/rocksdb_log/data_rocksdb_log.old.666",
      oldinfologfilename(dbname, 666u, db_absolute_path, "/data/rocksdb_log"));
}

test(filenametest, construction) {
  uint64_t number;
  filetype type;
  std::string fname;

  fname = currentfilename("foo");
  assert_eq("foo/", std::string(fname.data(), 4));
  assert_true(parsefilename(fname.c_str() + 4, &number, &type));
  assert_eq(0u, number);
  assert_eq(kcurrentfile, type);

  fname = lockfilename("foo");
  assert_eq("foo/", std::string(fname.data(), 4));
  assert_true(parsefilename(fname.c_str() + 4, &number, &type));
  assert_eq(0u, number);
  assert_eq(kdblockfile, type);

  fname = logfilename("foo", 192);
  assert_eq("foo/", std::string(fname.data(), 4));
  assert_true(parsefilename(fname.c_str() + 4, &number, &type));
  assert_eq(192u, number);
  assert_eq(klogfile, type);

  fname = tablefilename({dbpath("bar", 0)}, 200, 0);
  std::string fname1 =
      tablefilename({dbpath("foo", 0), dbpath("bar", 0)}, 200, 1);
  assert_eq(fname, fname1);
  assert_eq("bar/", std::string(fname.data(), 4));
  assert_true(parsefilename(fname.c_str() + 4, &number, &type));
  assert_eq(200u, number);
  assert_eq(ktablefile, type);

  fname = descriptorfilename("bar", 100);
  assert_eq("bar/", std::string(fname.data(), 4));
  assert_true(parsefilename(fname.c_str() + 4, &number, &type));
  assert_eq(100u, number);
  assert_eq(kdescriptorfile, type);

  fname = tempfilename("tmp", 999);
  assert_eq("tmp/", std::string(fname.data(), 4));
  assert_true(parsefilename(fname.c_str() + 4, &number, &type));
  assert_eq(999u, number);
  assert_eq(ktempfile, type);

  fname = metadatabasename("met", 100);
  assert_eq("met/", std::string(fname.data(), 4));
  assert_true(parsefilename(fname.c_str() + 4, &number, &type));
  assert_eq(100u, number);
  assert_eq(kmetadatabase, type);
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
