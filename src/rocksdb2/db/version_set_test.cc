//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/version_set.h"
#include "util/logging.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

class generatefileleveltest {
 public:
  std::vector<filemetadata*> files_;
  filelevel file_level_;
  arena arena_;

  generatefileleveltest() { }

  ~generatefileleveltest() {
    for (unsigned int i = 0; i < files_.size(); i++) {
      delete files_[i];
    }
  }

  void add(const char* smallest, const char* largest,
           sequencenumber smallest_seq = 100,
           sequencenumber largest_seq = 100) {
    filemetadata* f = new filemetadata;
    f->fd = filedescriptor(files_.size() + 1, 0, 0);
    f->smallest = internalkey(smallest, smallest_seq, ktypevalue);
    f->largest = internalkey(largest, largest_seq, ktypevalue);
    files_.push_back(f);
  }

  int compare() {
    int diff = 0;
    for (size_t i = 0; i < files_.size(); i++) {
      if (file_level_.files[i].fd.getnumber() != files_[i]->fd.getnumber()) {
        diff++;
      }
    }
    return diff;
  }
};

test(generatefileleveltest, empty) {
  dogeneratefilelevel(&file_level_, files_, &arena_);
  assert_eq(0u, file_level_.num_files);
  assert_eq(0, compare());
}

test(generatefileleveltest, single) {
  add("p", "q");
  dogeneratefilelevel(&file_level_, files_, &arena_);
  assert_eq(1u, file_level_.num_files);
  assert_eq(0, compare());
}


test(generatefileleveltest, multiple) {
  add("150", "200");
  add("200", "250");
  add("300", "350");
  add("400", "450");
  dogeneratefilelevel(&file_level_, files_, &arena_);
  assert_eq(4u, file_level_.num_files);
  assert_eq(0, compare());
}

class findlevelfiletest {
 public:
  filelevel file_level_;
  bool disjoint_sorted_files_;
  arena arena_;

  findlevelfiletest() : disjoint_sorted_files_(true) { }

  ~findlevelfiletest() {
  }

  void levelfileinit(size_t num = 0) {
    char* mem = arena_.allocatealigned(num * sizeof(fdwithkeyrange));
    file_level_.files = new (mem)fdwithkeyrange[num];
    file_level_.num_files = 0;
  }

  void add(const char* smallest, const char* largest,
           sequencenumber smallest_seq = 100,
           sequencenumber largest_seq = 100) {
    internalkey smallest_key = internalkey(smallest, smallest_seq, ktypevalue);
    internalkey largest_key = internalkey(largest, largest_seq, ktypevalue);

    slice smallest_slice = smallest_key.encode();
    slice largest_slice = largest_key.encode();

    char* mem = arena_.allocatealigned(
        smallest_slice.size() + largest_slice.size());
    memcpy(mem, smallest_slice.data(), smallest_slice.size());
    memcpy(mem + smallest_slice.size(), largest_slice.data(),
        largest_slice.size());

    // add to file_level_
    size_t num = file_level_.num_files;
    auto& file = file_level_.files[num];
    file.fd = filedescriptor(num + 1, 0, 0);
    file.smallest_key = slice(mem, smallest_slice.size());
    file.largest_key = slice(mem + smallest_slice.size(),
        largest_slice.size());
    file_level_.num_files++;
  }

  int find(const char* key) {
    internalkey target(key, 100, ktypevalue);
    internalkeycomparator cmp(bytewisecomparator());
    return findfile(cmp, file_level_, target.encode());
  }

  bool overlaps(const char* smallest, const char* largest) {
    internalkeycomparator cmp(bytewisecomparator());
    slice s(smallest != nullptr ? smallest : "");
    slice l(largest != nullptr ? largest : "");
    return somefileoverlapsrange(cmp, disjoint_sorted_files_, file_level_,
                                 (smallest != nullptr ? &s : nullptr),
                                 (largest != nullptr ? &l : nullptr));
  }
};

test(findlevelfiletest, levelempty) {
  levelfileinit(0);

  assert_eq(0, find("foo"));
  assert_true(! overlaps("a", "z"));
  assert_true(! overlaps(nullptr, "z"));
  assert_true(! overlaps("a", nullptr));
  assert_true(! overlaps(nullptr, nullptr));
}

test(findlevelfiletest, levelsingle) {
  levelfileinit(1);

  add("p", "q");
  assert_eq(0, find("a"));
  assert_eq(0, find("p"));
  assert_eq(0, find("p1"));
  assert_eq(0, find("q"));
  assert_eq(1, find("q1"));
  assert_eq(1, find("z"));

  assert_true(! overlaps("a", "b"));
  assert_true(! overlaps("z1", "z2"));
  assert_true(overlaps("a", "p"));
  assert_true(overlaps("a", "q"));
  assert_true(overlaps("a", "z"));
  assert_true(overlaps("p", "p1"));
  assert_true(overlaps("p", "q"));
  assert_true(overlaps("p", "z"));
  assert_true(overlaps("p1", "p2"));
  assert_true(overlaps("p1", "z"));
  assert_true(overlaps("q", "q"));
  assert_true(overlaps("q", "q1"));

  assert_true(! overlaps(nullptr, "j"));
  assert_true(! overlaps("r", nullptr));
  assert_true(overlaps(nullptr, "p"));
  assert_true(overlaps(nullptr, "p1"));
  assert_true(overlaps("q", nullptr));
  assert_true(overlaps(nullptr, nullptr));
}

test(findlevelfiletest, levelmultiple) {
  levelfileinit(4);

  add("150", "200");
  add("200", "250");
  add("300", "350");
  add("400", "450");
  assert_eq(0, find("100"));
  assert_eq(0, find("150"));
  assert_eq(0, find("151"));
  assert_eq(0, find("199"));
  assert_eq(0, find("200"));
  assert_eq(1, find("201"));
  assert_eq(1, find("249"));
  assert_eq(1, find("250"));
  assert_eq(2, find("251"));
  assert_eq(2, find("299"));
  assert_eq(2, find("300"));
  assert_eq(2, find("349"));
  assert_eq(2, find("350"));
  assert_eq(3, find("351"));
  assert_eq(3, find("400"));
  assert_eq(3, find("450"));
  assert_eq(4, find("451"));

  assert_true(! overlaps("100", "149"));
  assert_true(! overlaps("251", "299"));
  assert_true(! overlaps("451", "500"));
  assert_true(! overlaps("351", "399"));

  assert_true(overlaps("100", "150"));
  assert_true(overlaps("100", "200"));
  assert_true(overlaps("100", "300"));
  assert_true(overlaps("100", "400"));
  assert_true(overlaps("100", "500"));
  assert_true(overlaps("375", "400"));
  assert_true(overlaps("450", "450"));
  assert_true(overlaps("450", "500"));
}

test(findlevelfiletest, levelmultiplenullboundaries) {
  levelfileinit(4);

  add("150", "200");
  add("200", "250");
  add("300", "350");
  add("400", "450");
  assert_true(! overlaps(nullptr, "149"));
  assert_true(! overlaps("451", nullptr));
  assert_true(overlaps(nullptr, nullptr));
  assert_true(overlaps(nullptr, "150"));
  assert_true(overlaps(nullptr, "199"));
  assert_true(overlaps(nullptr, "200"));
  assert_true(overlaps(nullptr, "201"));
  assert_true(overlaps(nullptr, "400"));
  assert_true(overlaps(nullptr, "800"));
  assert_true(overlaps("100", nullptr));
  assert_true(overlaps("200", nullptr));
  assert_true(overlaps("449", nullptr));
  assert_true(overlaps("450", nullptr));
}

test(findlevelfiletest, leveloverlapsequencechecks) {
  levelfileinit(1);

  add("200", "200", 5000, 3000);
  assert_true(! overlaps("199", "199"));
  assert_true(! overlaps("201", "300"));
  assert_true(overlaps("200", "200"));
  assert_true(overlaps("190", "200"));
  assert_true(overlaps("200", "210"));
}

test(findlevelfiletest, leveloverlappingfiles) {
  levelfileinit(2);

  add("150", "600");
  add("400", "500");
  disjoint_sorted_files_ = false;
  assert_true(! overlaps("100", "149"));
  assert_true(! overlaps("601", "700"));
  assert_true(overlaps("100", "150"));
  assert_true(overlaps("100", "200"));
  assert_true(overlaps("100", "300"));
  assert_true(overlaps("100", "400"));
  assert_true(overlaps("100", "500"));
  assert_true(overlaps("375", "400"));
  assert_true(overlaps("450", "450"));
  assert_true(overlaps("450", "500"));
  assert_true(overlaps("450", "700"));
  assert_true(overlaps("600", "700"));
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
