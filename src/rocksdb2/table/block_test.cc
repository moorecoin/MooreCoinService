//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include <stdio.h>
#include <string>
#include <vector>

#include "db/dbformat.h"
#include "db/memtable.h"
#include "db/write_batch_internal.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/iterator.h"
#include "rocksdb/table.h"
#include "rocksdb/slice_transform.h"
#include "table/block.h"
#include "table/block_builder.h"
#include "table/format.h"
#include "table/block_hash_index.h"
#include "util/random.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

static std::string randomstring(random* rnd, int len) {
  std::string r;
  test::randomstring(rnd, len, &r);
  return r;
}
std::string generatekey(int primary_key, int secondary_key, int padding_size,
                        random *rnd) {
  char buf[50];
  char *p = &buf[0];
  snprintf(buf, sizeof(buf), "%6d%4d", primary_key, secondary_key);
  std::string k(p);
  if (padding_size) {
    k += randomstring(rnd, padding_size);
  }

  return k;
}

// generate random key value pairs.
// the generated key will be sorted. you can tune the parameters to generated
// different kinds of test key/value pairs for different scenario.
void generaterandomkvs(std::vector<std::string> *keys,
                       std::vector<std::string> *values, const int from,
                       const int len, const int step = 1,
                       const int padding_size = 0,
                       const int keys_share_prefix = 1) {
  random rnd(302);

  // generate different prefix
  for (int i = from; i < from + len; i += step) {
    // generating keys that shares the prefix
    for (int j = 0; j < keys_share_prefix; ++j) {
      keys->emplace_back(generatekey(i, j, padding_size, &rnd));

      // 100 bytes values
      values->emplace_back(randomstring(&rnd, 100));
    }
  }
}

class blocktest {};

// block test
test(blocktest, simpletest) {
  random rnd(301);
  options options = options();
  std::unique_ptr<internalkeycomparator> ic;
  ic.reset(new test::plaininternalkeycomparator(options.comparator));

  std::vector<std::string> keys;
  std::vector<std::string> values;
  blockbuilder builder(16);
  int num_records = 100000;

  generaterandomkvs(&keys, &values, 0, num_records);
  // add a bunch of records to a block
  for (int i = 0; i < num_records; i++) {
    builder.add(keys[i], values[i]);
  }

  // read serialized contents of the block
  slice rawblock = builder.finish();

  // create block reader
  blockcontents contents;
  contents.data = rawblock;
  contents.cachable = false;
  contents.heap_allocated = false;
  block reader(contents);

  // read contents of block sequentially
  int count = 0;
  iterator* iter = reader.newiterator(options.comparator);
  for (iter->seektofirst();iter->valid(); count++, iter->next()) {

    // read kv from block
    slice k = iter->key();
    slice v = iter->value();

    // compare with lookaside array
    assert_eq(k.tostring().compare(keys[count]), 0);
    assert_eq(v.tostring().compare(values[count]), 0);
  }
  delete iter;

  // read block contents randomly
  iter = reader.newiterator(options.comparator);
  for (int i = 0; i < num_records; i++) {

    // find a random key in the lookaside array
    int index = rnd.uniform(num_records);
    slice k(keys[index]);

    // search in block for this key
    iter->seek(k);
    assert_true(iter->valid());
    slice v = iter->value();
    assert_eq(v.tostring().compare(values[index]), 0);
  }
  delete iter;
}

// return the block contents
blockcontents getblockcontents(std::unique_ptr<blockbuilder> *builder,
                               const std::vector<std::string> &keys,
                               const std::vector<std::string> &values,
                               const int prefix_group_size = 1) {
  builder->reset(new blockbuilder(1 /* restart interval */));

  // add only half of the keys
  for (size_t i = 0; i < keys.size(); ++i) {
    (*builder)->add(keys[i], values[i]);
  }
  slice rawblock = (*builder)->finish();

  blockcontents contents;
  contents.data = rawblock;
  contents.cachable = false;
  contents.heap_allocated = false;

  return contents;
}

void checkblockcontents(blockcontents contents, const int max_key,
                        const std::vector<std::string> &keys,
                        const std::vector<std::string> &values) {
  const size_t prefix_size = 6;
  // create block reader
  block reader1(contents);
  block reader2(contents);

  std::unique_ptr<const slicetransform> prefix_extractor(
      newfixedprefixtransform(prefix_size));

  {
    auto iter1 = reader1.newiterator(nullptr);
    auto iter2 = reader1.newiterator(nullptr);
    reader1.setblockhashindex(createblockhashindexonthefly(
        iter1, iter2, keys.size(), bytewisecomparator(),
        prefix_extractor.get()));

    delete iter1;
    delete iter2;
  }

  std::unique_ptr<iterator> hash_iter(
      reader1.newiterator(bytewisecomparator(), nullptr, false));

  std::unique_ptr<iterator> regular_iter(
      reader2.newiterator(bytewisecomparator()));

  // seek existent keys
  for (size_t i = 0; i < keys.size(); i++) {
    hash_iter->seek(keys[i]);
    assert_ok(hash_iter->status());
    assert_true(hash_iter->valid());

    slice v = hash_iter->value();
    assert_eq(v.tostring().compare(values[i]), 0);
  }

  // seek non-existent keys.
  // for hash index, if no key with a given prefix is not found, iterator will
  // simply be set as invalid; whereas the binary search based iterator will
  // return the one that is closest.
  for (int i = 1; i < max_key - 1; i += 2) {
    auto key = generatekey(i, 0, 0, nullptr);
    hash_iter->seek(key);
    assert_true(!hash_iter->valid());

    regular_iter->seek(key);
    assert_true(regular_iter->valid());
  }
}

// in this test case, no two key share same prefix.
test(blocktest, simpleindexhash) {
  const int kmaxkey = 100000;
  std::vector<std::string> keys;
  std::vector<std::string> values;
  generaterandomkvs(&keys, &values, 0 /* first key id */,
                    kmaxkey /* last key id */, 2 /* step */,
                    8 /* padding size (8 bytes randomly generated suffix) */);

  std::unique_ptr<blockbuilder> builder;
  auto contents = getblockcontents(&builder, keys, values);

  checkblockcontents(contents, kmaxkey, keys, values);
}

test(blocktest, indexhashwithsharedprefix) {
  const int kmaxkey = 100000;
  // for each prefix, there will be 5 keys starts with it.
  const int kprefixgroup = 5;
  std::vector<std::string> keys;
  std::vector<std::string> values;
  // generate keys with same prefix.
  generaterandomkvs(&keys, &values, 0,  // first key id
                    kmaxkey,            // last key id
                    2,                  // step
                    10,                 // padding size,
                    kprefixgroup);

  std::unique_ptr<blockbuilder> builder;
  auto contents = getblockcontents(&builder, keys, values, kprefixgroup);

  checkblockcontents(contents, kmaxkey, keys, values);
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
