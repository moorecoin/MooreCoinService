//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <string>
#include "db/file_indexer.h"
#include "db/dbformat.h"
#include "db/version_edit.h"
#include "rocksdb/comparator.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

class intcomparator : public comparator {
 public:
  int compare(const slice& a, const slice& b) const {
    assert(a.size() == 8);
    assert(b.size() == 8);
    return *reinterpret_cast<const int64_t*>(a.data()) -
      *reinterpret_cast<const int64_t*>(b.data());
  }

  const char* name() const {
    return "intcomparator";
  }

  void findshortestseparator(std::string* start, const slice& limit) const {}

  void findshortsuccessor(std::string* key) const {}
};

struct fileindexertest {
 public:
  fileindexertest()
      : knumlevels(4), files(new std::vector<filemetadata*>[knumlevels]) {}

  ~fileindexertest() {
    clearfiles();
    delete[] files;
  }

  void addfile(int level, int64_t smallest, int64_t largest) {
    auto* f = new filemetadata();
    f->smallest = intkey(smallest);
    f->largest = intkey(largest);
    files[level].push_back(f);
  }

  internalkey intkey(int64_t v) {
    return internalkey(slice(reinterpret_cast<char*>(&v), 8), 0, ktypevalue);
  }

  void clearfiles() {
    for (uint32_t i = 0; i < knumlevels; ++i) {
      for (auto* f : files[i]) {
        delete f;
      }
      files[i].clear();
    }
  }

  void getnextlevelindex(const uint32_t level, const uint32_t file_index,
      const int cmp_smallest, const int cmp_largest, int32_t* left_index,
      int32_t* right_index) {
    *left_index = 100;
    *right_index = 100;
    indexer->getnextlevelindex(level, file_index, cmp_smallest, cmp_largest,
                               left_index, right_index);
  }

  int32_t left = 100;
  int32_t right = 100;
  const uint32_t knumlevels;
  intcomparator ucmp;
  fileindexer* indexer;

  std::vector<filemetadata*>* files;
};

// case 0: empty
test(fileindexertest, empty) {
  arena arena;
  indexer = new fileindexer(&ucmp);
  indexer->updateindex(&arena, 0, files);
  delete indexer;
}

// case 1: no overlap, files are on the left of next level files
test(fileindexertest, no_overlap_left) {
  arena arena;
  uint32_t knumlevels = 4;
  indexer = new fileindexer(&ucmp);
  // level 1
  addfile(1, 100, 200);
  addfile(1, 300, 400);
  addfile(1, 500, 600);
  // level 2
  addfile(2, 1500, 1600);
  addfile(2, 1601, 1699);
  addfile(2, 1700, 1800);
  // level 3
  addfile(3, 2500, 2600);
  addfile(3, 2601, 2699);
  addfile(3, 2700, 2800);
  indexer->updateindex(&arena, knumlevels, files);
  for (uint32_t level = 1; level < 3; ++level) {
    for (uint32_t f = 0; f < 3; ++f) {
      getnextlevelindex(level, f, -1, -1, &left, &right);
      assert_eq(0, left);
      assert_eq(-1, right);
      getnextlevelindex(level, f, 0, -1, &left, &right);
      assert_eq(0, left);
      assert_eq(-1, right);
      getnextlevelindex(level, f, 1, -1, &left, &right);
      assert_eq(0, left);
      assert_eq(-1, right);
      getnextlevelindex(level, f, 1, 0, &left, &right);
      assert_eq(0, left);
      assert_eq(-1, right);
      getnextlevelindex(level, f, 1, 1, &left, &right);
      assert_eq(0, left);
      assert_eq(2, right);
    }
  }
  delete indexer;
  clearfiles();
}

// case 2: no overlap, files are on the right of next level files
test(fileindexertest, no_overlap_right) {
  arena arena;
  uint32_t knumlevels = 4;
  indexer = new fileindexer(&ucmp);
  // level 1
  addfile(1, 2100, 2200);
  addfile(1, 2300, 2400);
  addfile(1, 2500, 2600);
  // level 2
  addfile(2, 1500, 1600);
  addfile(2, 1501, 1699);
  addfile(2, 1700, 1800);
  // level 3
  addfile(3, 500, 600);
  addfile(3, 501, 699);
  addfile(3, 700, 800);
  indexer->updateindex(&arena, knumlevels, files);
  for (uint32_t level = 1; level < 3; ++level) {
    for (uint32_t f = 0; f < 3; ++f) {
      getnextlevelindex(level, f, -1, -1, &left, &right);
      assert_eq(f == 0 ? 0 : 3, left);
      assert_eq(2, right);
      getnextlevelindex(level, f, 0, -1, &left, &right);
      assert_eq(3, left);
      assert_eq(2, right);
      getnextlevelindex(level, f, 1, -1, &left, &right);
      assert_eq(3, left);
      assert_eq(2, right);
      getnextlevelindex(level, f, 1, -1, &left, &right);
      assert_eq(3, left);
      assert_eq(2, right);
      getnextlevelindex(level, f, 1, 0, &left, &right);
      assert_eq(3, left);
      assert_eq(2, right);
      getnextlevelindex(level, f, 1, 1, &left, &right);
      assert_eq(3, left);
      assert_eq(2, right);
    }
  }
  delete indexer;
}

// case 3: empty l2
test(fileindexertest, empty_l2) {
  arena arena;
  uint32_t knumlevels = 4;
  indexer = new fileindexer(&ucmp);
  for (uint32_t i = 1; i < knumlevels; ++i) {
    assert_eq(0u, indexer->levelindexsize(i));
  }
  // level 1
  addfile(1, 2100, 2200);
  addfile(1, 2300, 2400);
  addfile(1, 2500, 2600);
  // level 3
  addfile(3, 500, 600);
  addfile(3, 501, 699);
  addfile(3, 700, 800);
  indexer->updateindex(&arena, knumlevels, files);
  for (uint32_t f = 0; f < 3; ++f) {
    getnextlevelindex(1, f, -1, -1, &left, &right);
    assert_eq(0, left);
    assert_eq(-1, right);
    getnextlevelindex(1, f, 0, -1, &left, &right);
    assert_eq(0, left);
    assert_eq(-1, right);
    getnextlevelindex(1, f, 1, -1, &left, &right);
    assert_eq(0, left);
    assert_eq(-1, right);
    getnextlevelindex(1, f, 1, -1, &left, &right);
    assert_eq(0, left);
    assert_eq(-1, right);
    getnextlevelindex(1, f, 1, 0, &left, &right);
    assert_eq(0, left);
    assert_eq(-1, right);
    getnextlevelindex(1, f, 1, 1, &left, &right);
    assert_eq(0, left);
    assert_eq(-1, right);
  }
  delete indexer;
  clearfiles();
}

// case 4: mixed
test(fileindexertest, mixed) {
  arena arena;
  indexer = new fileindexer(&ucmp);
  // level 1
  addfile(1, 100, 200);
  addfile(1, 250, 400);
  addfile(1, 450, 500);
  // level 2
  addfile(2, 100, 150);  // 0
  addfile(2, 200, 250);  // 1
  addfile(2, 251, 300);  // 2
  addfile(2, 301, 350);  // 3
  addfile(2, 500, 600);  // 4
  // level 3
  addfile(3, 0, 50);
  addfile(3, 100, 200);
  addfile(3, 201, 250);
  indexer->updateindex(&arena, knumlevels, files);
  // level 1, 0
  getnextlevelindex(1, 0, -1, -1, &left, &right);
  assert_eq(0, left);
  assert_eq(0, right);
  getnextlevelindex(1, 0, 0, -1, &left, &right);
  assert_eq(0, left);
  assert_eq(0, right);
  getnextlevelindex(1, 0, 1, -1, &left, &right);
  assert_eq(0, left);
  assert_eq(1, right);
  getnextlevelindex(1, 0, 1, 0, &left, &right);
  assert_eq(1, left);
  assert_eq(1, right);
  getnextlevelindex(1, 0, 1, 1, &left, &right);
  assert_eq(1, left);
  assert_eq(4, right);
  // level 1, 1
  getnextlevelindex(1, 1, -1, -1, &left, &right);
  assert_eq(1, left);
  assert_eq(1, right);
  getnextlevelindex(1, 1, 0, -1, &left, &right);
  assert_eq(1, left);
  assert_eq(1, right);
  getnextlevelindex(1, 1, 1, -1, &left, &right);
  assert_eq(1, left);
  assert_eq(3, right);
  getnextlevelindex(1, 1, 1, 0, &left, &right);
  assert_eq(4, left);
  assert_eq(3, right);
  getnextlevelindex(1, 1, 1, 1, &left, &right);
  assert_eq(4, left);
  assert_eq(4, right);
  // level 1, 2
  getnextlevelindex(1, 2, -1, -1, &left, &right);
  assert_eq(4, left);
  assert_eq(3, right);
  getnextlevelindex(1, 2, 0, -1, &left, &right);
  assert_eq(4, left);
  assert_eq(3, right);
  getnextlevelindex(1, 2, 1, -1, &left, &right);
  assert_eq(4, left);
  assert_eq(4, right);
  getnextlevelindex(1, 2, 1, 0, &left, &right);
  assert_eq(4, left);
  assert_eq(4, right);
  getnextlevelindex(1, 2, 1, 1, &left, &right);
  assert_eq(4, left);
  assert_eq(4, right);
  // level 2, 0
  getnextlevelindex(2, 0, -1, -1, &left, &right);
  assert_eq(0, left);
  assert_eq(1, right);
  getnextlevelindex(2, 0, 0, -1, &left, &right);
  assert_eq(1, left);
  assert_eq(1, right);
  getnextlevelindex(2, 0, 1, -1, &left, &right);
  assert_eq(1, left);
  assert_eq(1, right);
  getnextlevelindex(2, 0, 1, 0, &left, &right);
  assert_eq(1, left);
  assert_eq(1, right);
  getnextlevelindex(2, 0, 1, 1, &left, &right);
  assert_eq(1, left);
  assert_eq(2, right);
  // level 2, 1
  getnextlevelindex(2, 1, -1, -1, &left, &right);
  assert_eq(1, left);
  assert_eq(1, right);
  getnextlevelindex(2, 1, 0, -1, &left, &right);
  assert_eq(1, left);
  assert_eq(1, right);
  getnextlevelindex(2, 1, 1, -1, &left, &right);
  assert_eq(1, left);
  assert_eq(2, right);
  getnextlevelindex(2, 1, 1, 0, &left, &right);
  assert_eq(2, left);
  assert_eq(2, right);
  getnextlevelindex(2, 1, 1, 1, &left, &right);
  assert_eq(2, left);
  assert_eq(2, right);
  // level 2, [2 - 4], no overlap
  for (uint32_t f = 2; f <= 4; ++f) {
    getnextlevelindex(2, f, -1, -1, &left, &right);
    assert_eq(f == 2 ? 2 : 3, left);
    assert_eq(2, right);
    getnextlevelindex(2, f, 0, -1, &left, &right);
    assert_eq(3, left);
    assert_eq(2, right);
    getnextlevelindex(2, f, 1, -1, &left, &right);
    assert_eq(3, left);
    assert_eq(2, right);
    getnextlevelindex(2, f, 1, 0, &left, &right);
    assert_eq(3, left);
    assert_eq(2, right);
    getnextlevelindex(2, f, 1, 1, &left, &right);
    assert_eq(3, left);
    assert_eq(2, right);
  }
  delete indexer;
  clearfiles();
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
