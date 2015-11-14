//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/version_edit.h"
#include "util/testharness.h"

namespace rocksdb {

static void testencodedecode(const versionedit& edit) {
  std::string encoded, encoded2;
  edit.encodeto(&encoded);
  versionedit parsed;
  status s = parsed.decodefrom(encoded);
  assert_true(s.ok()) << s.tostring();
  parsed.encodeto(&encoded2);
  assert_eq(encoded, encoded2);
}

class versionedittest { };

test(versionedittest, encodedecode) {
  static const uint64_t kbig = 1ull << 50;

  versionedit edit;
  for (int i = 0; i < 4; i++) {
    testencodedecode(edit);
    edit.addfile(3, kbig + 300 + i, kbig + 400 + i, 0,
                 internalkey("foo", kbig + 500 + i, ktypevalue),
                 internalkey("zoo", kbig + 600 + i, ktypedeletion),
                 kbig + 500 + i, kbig + 600 + i);
    edit.deletefile(4, kbig + 700 + i);
  }

  edit.setcomparatorname("foo");
  edit.setlognumber(kbig + 100);
  edit.setnextfile(kbig + 200);
  edit.setlastsequence(kbig + 1000);
  testencodedecode(edit);
}

test(versionedittest, columnfamilytest) {
  versionedit edit;
  edit.setcolumnfamily(2);
  edit.addcolumnfamily("column_family");
  edit.setmaxcolumnfamily(5);
  testencodedecode(edit);

  edit.clear();
  edit.setcolumnfamily(3);
  edit.dropcolumnfamily();
  testencodedecode(edit);
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
