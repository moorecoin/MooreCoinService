// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/blob_store.h"

#include "util/testharness.h"
#include "util/testutil.h"
#include "util/random.h"

#include <cstdlib>
#include <string>

namespace rocksdb {

using namespace std;

class blobstoretest { };

test(blobstoretest, rangeparsetest) {
  blob e;
  for (int i = 0; i < 5; ++i) {
    e.chunks.push_back(blobchunk(rand(), rand(), rand()));
  }
  string x = e.tostring();
  blob nx(x);

  assert_eq(nx.tostring(), x);
}

// make sure we're reusing the freed space
test(blobstoretest, sanitytest) {
  const uint64_t block_size = 10;
  const uint32_t blocks_per_file = 20;
  random random(5);

  blobstore blob_store(test::tmpdir() + "/blob_store_test",
                       block_size,
                       blocks_per_file,
                       1000,
                       env::default());

  string buf;

  // put string of size 170
  test::randomstring(&random, 170, &buf);
  blob r1;
  assert_ok(blob_store.put(slice(buf), &r1));
  // use the first file
  for (size_t i = 0; i < r1.chunks.size(); ++i) {
    assert_eq(r1.chunks[0].bucket_id, 0u);
  }

  // put string of size 30
  test::randomstring(&random, 30, &buf);
  blob r2;
  assert_ok(blob_store.put(slice(buf), &r2));
  // use the first file
  for (size_t i = 0; i < r2.chunks.size(); ++i) {
    assert_eq(r2.chunks[0].bucket_id, 0u);
  }

  // delete blob of size 170
  assert_ok(blob_store.delete(r1));

  // put a string of size 100
  test::randomstring(&random, 100, &buf);
  blob r3;
  assert_ok(blob_store.put(slice(buf), &r3));
  // use the first file
  for (size_t i = 0; i < r3.chunks.size(); ++i) {
    assert_eq(r3.chunks[0].bucket_id, 0u);
  }

  // put a string of size 70
  test::randomstring(&random, 70, &buf);
  blob r4;
  assert_ok(blob_store.put(slice(buf), &r4));
  // use the first file
  for (size_t i = 0; i < r4.chunks.size(); ++i) {
    assert_eq(r4.chunks[0].bucket_id, 0u);
  }

  // put a string of size 5
  test::randomstring(&random, 5, &buf);
  blob r5;
  assert_ok(blob_store.put(slice(buf), &r5));
  // now you get to use the second file
  for (size_t i = 0; i < r5.chunks.size(); ++i) {
    assert_eq(r5.chunks[0].bucket_id, 1u);
  }
}

test(blobstoretest, fragmentedchunkstest) {
  const uint64_t block_size = 10;
  const uint32_t blocks_per_file = 20;
  random random(5);

  blobstore blob_store(test::tmpdir() + "/blob_store_test",
                       block_size,
                       blocks_per_file,
                       1000,
                       env::default());

  string buf;

  vector <blob> r(4);

  // put 4 strings of size 50
  for (int k = 0; k < 4; ++k)  {
    test::randomstring(&random, 50, &buf);
    assert_ok(blob_store.put(slice(buf), &r[k]));
    // use the first file
    for (size_t i = 0; i < r[k].chunks.size(); ++i) {
      assert_eq(r[k].chunks[0].bucket_id, 0u);
    }
  }

  // delete the first and third
  assert_ok(blob_store.delete(r[0]));
  assert_ok(blob_store.delete(r[2]));

  // put string of size 100. it should reuse space that we deleting
  // by deleting first and third strings of size 50
  test::randomstring(&random, 100, &buf);
  blob r2;
  assert_ok(blob_store.put(slice(buf), &r2));
  // use the first file
  for (size_t i = 0; i < r2.chunks.size(); ++i) {
    assert_eq(r2.chunks[0].bucket_id, 0u);
  }
}

test(blobstoretest, createandstoretest) {
  const uint64_t block_size = 10;
  const uint32_t blocks_per_file = 1000;
  const int max_blurb_size = 300;
  random random(5);

  blobstore blob_store(test::tmpdir() + "/blob_store_test",
                       block_size,
                       blocks_per_file,
                       10000,
                       env::default());
  vector<pair<blob, string>> ranges;

  for (int i = 0; i < 2000; ++i) {
    int decision = rand() % 5;
    if (decision <= 2 || ranges.size() == 0) {
      string buf;
      int size_blocks = (rand() % max_blurb_size + 1);
      int string_size = size_blocks * block_size - (rand() % block_size);
      test::randomstring(&random, string_size, &buf);
      blob r;
      assert_ok(blob_store.put(slice(buf), &r));
      ranges.push_back(make_pair(r, buf));
    } else if (decision == 3) {
      int ti = rand() % ranges.size();
      string out_buf;
      assert_ok(blob_store.get(ranges[ti].first, &out_buf));
      assert_eq(ranges[ti].second, out_buf);
    } else {
      int ti = rand() % ranges.size();
      assert_ok(blob_store.delete(ranges[ti].first));
      ranges.erase(ranges.begin() + ti);
    }
  }
  assert_ok(blob_store.sync());
}

test(blobstoretest, maxsizetest) {
  const uint64_t block_size = 10;
  const uint32_t blocks_per_file = 100;
  const int max_buckets = 10;
  random random(5);

  blobstore blob_store(test::tmpdir() + "/blob_store_test",
                       block_size,
                       blocks_per_file,
                       max_buckets,
                       env::default());
  string buf;
  for (int i = 0; i < max_buckets; ++i) {
    test::randomstring(&random, 1000, &buf);
    blob r;
    assert_ok(blob_store.put(slice(buf), &r));
  }

  test::randomstring(&random, 1000, &buf);
  blob r;
  // should fail because max size
  status s = blob_store.put(slice(buf), &r);
  assert_eq(s.ok(), false);
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
