//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/testutil.h"

#include "port/port.h"
#include "util/random.h"

namespace rocksdb {
namespace test {

slice randomstring(random* rnd, int len, std::string* dst) {
  dst->resize(len);
  for (int i = 0; i < len; i++) {
    (*dst)[i] = static_cast<char>(' ' + rnd->uniform(95));   // ' ' .. '~'
  }
  return slice(*dst);
}

std::string randomkey(random* rnd, int len) {
  // make sure to generate a wide variety of characters so we
  // test the boundary conditions for short-key optimizations.
  static const char ktestchars[] = {
    '\0', '\1', 'a', 'b', 'c', 'd', 'e', '\xfd', '\xfe', '\xff'
  };
  std::string result;
  for (int i = 0; i < len; i++) {
    result += ktestchars[rnd->uniform(sizeof(ktestchars))];
  }
  return result;
}


extern slice compressiblestring(random* rnd, double compressed_fraction,
                                int len, std::string* dst) {
  int raw = static_cast<int>(len * compressed_fraction);
  if (raw < 1) raw = 1;
  std::string raw_data;
  randomstring(rnd, raw, &raw_data);

  // duplicate the random data until we have filled "len" bytes
  dst->clear();
  while (dst->size() < (unsigned int)len) {
    dst->append(raw_data);
  }
  dst->resize(len);
  return slice(*dst);
}

namespace {
class uint64comparatorimpl : public comparator {
 public:
  uint64comparatorimpl() { }

  virtual const char* name() const override {
    return "rocksdb.uint64comparator";
  }

  virtual int compare(const slice& a, const slice& b) const override {
    assert(a.size() == sizeof(uint64_t) && b.size() == sizeof(uint64_t));
    const uint64_t* left = reinterpret_cast<const uint64_t*>(a.data());
    const uint64_t* right = reinterpret_cast<const uint64_t*>(b.data());
    if (*left == *right) {
      return 0;
    } else if (*left < *right) {
      return -1;
    } else {
      return 1;
    }
  }

  virtual void findshortestseparator(std::string* start,
      const slice& limit) const override {
    return;
  }

  virtual void findshortsuccessor(std::string* key) const override {
    return;
  }
};
}  // namespace

static port::oncetype once = leveldb_once_init;
static const comparator* uint64comp;

static void initmodule() {
  uint64comp = new uint64comparatorimpl;
}

const comparator* uint64comparator() {
  port::initonce(&once, initmodule);
  return uint64comp;
}

}  // namespace test
}  // namespace rocksdb
