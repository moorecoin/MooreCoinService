// copyright (c) 2014, facebook, inc. all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#include <vector>
#include <string>
#include <map>
#include <utility>

#include "table/meta_blocks.h"
#include "table/cuckoo_table_builder.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {
extern const uint64_t kcuckootablemagicnumber;

namespace {
std::unordered_map<std::string, std::vector<uint64_t>> hash_map;

uint64_t getslicehash(const slice& s, uint32_t index,
    uint64_t max_num_buckets) {
  return hash_map[s.tostring()][index];
}
}  // namespace

class cuckoobuildertest {
 public:
  cuckoobuildertest() {
    env_ = env::default();
    options options;
    options.allow_mmap_reads = true;
    env_options_ = envoptions(options);
  }

  void checkfilecontents(const std::vector<std::string>& keys,
      const std::vector<std::string>& values,
      const std::vector<uint64_t>& expected_locations,
      std::string expected_unused_bucket, uint64_t expected_table_size,
      uint32_t expected_num_hash_func, bool expected_is_last_level,
      uint32_t expected_cuckoo_block_size = 1) {
    // read file
    unique_ptr<randomaccessfile> read_file;
    assert_ok(env_->newrandomaccessfile(fname, &read_file, env_options_));
    uint64_t read_file_size;
    assert_ok(env_->getfilesize(fname, &read_file_size));

    // assert table properties.
    tableproperties* props = nullptr;
    assert_ok(readtableproperties(read_file.get(), read_file_size,
          kcuckootablemagicnumber, env_, nullptr, &props));
    assert_eq(props->num_entries, keys.size());
    assert_eq(props->fixed_key_len, keys.empty() ? 0 : keys[0].size());
    assert_eq(props->data_size, expected_unused_bucket.size() *
        (expected_table_size + expected_cuckoo_block_size - 1));
    assert_eq(props->raw_key_size, keys.size()*props->fixed_key_len);

    // check unused bucket.
    std::string unused_key = props->user_collected_properties[
      cuckootablepropertynames::kemptykey];
    assert_eq(expected_unused_bucket.substr(0,
          props->fixed_key_len), unused_key);

    uint32_t value_len_found =
      *reinterpret_cast<const uint32_t*>(props->user_collected_properties[
                cuckootablepropertynames::kvaluelength].data());
    assert_eq(values.empty() ? 0 : values[0].size(), value_len_found);
    assert_eq(props->raw_value_size, values.size()*value_len_found);
    const uint64_t table_size =
      *reinterpret_cast<const uint64_t*>(props->user_collected_properties[
                cuckootablepropertynames::khashtablesize].data());
    assert_eq(expected_table_size, table_size);
    const uint32_t num_hash_func_found =
      *reinterpret_cast<const uint32_t*>(props->user_collected_properties[
                cuckootablepropertynames::knumhashfunc].data());
    assert_eq(expected_num_hash_func, num_hash_func_found);
    const uint32_t cuckoo_block_size =
      *reinterpret_cast<const uint32_t*>(props->user_collected_properties[
                cuckootablepropertynames::kcuckooblocksize].data());
    assert_eq(expected_cuckoo_block_size, cuckoo_block_size);
    const bool is_last_level_found =
      *reinterpret_cast<const bool*>(props->user_collected_properties[
                cuckootablepropertynames::kislastlevel].data());
    assert_eq(expected_is_last_level, is_last_level_found);
    delete props;

    // check contents of the bucket.
    std::vector<bool> keys_found(keys.size(), false);
    uint32_t bucket_size = expected_unused_bucket.size();
    for (uint32_t i = 0; i < table_size + cuckoo_block_size - 1; ++i) {
      slice read_slice;
      assert_ok(read_file->read(i*bucket_size, bucket_size,
            &read_slice, nullptr));
      uint32_t key_idx = std::find(expected_locations.begin(),
          expected_locations.end(), i) - expected_locations.begin();
      if (key_idx == keys.size()) {
        // i is not one of the expected locaitons. empty bucket.
        assert_eq(read_slice.compare(expected_unused_bucket), 0);
      } else {
        keys_found[key_idx] = true;
        assert_eq(read_slice.compare(keys[key_idx] + values[key_idx]), 0);
      }
    }
    for (auto key_found : keys_found) {
      // check that all keys were found.
      assert_true(key_found);
    }
  }

  std::string getinternalkey(slice user_key, bool zero_seqno) {
    iterkey ikey;
    ikey.setinternalkey(user_key, zero_seqno ? 0 : 1000, ktypevalue);
    return ikey.getkey().tostring();
  }

  uint64_t nextpowof2(uint64_t num) {
    uint64_t n = 2;
    while (n <= num) {
      n *= 2;
    }
    return n;
  }

  env* env_;
  envoptions env_options_;
  std::string fname;
  const double khashtableratio = 0.9;
};

test(cuckoobuildertest, successwithemptyfile) {
  unique_ptr<writablefile> writable_file;
  fname = test::tmpdir() + "/emptyfile";
  assert_ok(env_->newwritablefile(fname, &writable_file, env_options_));
  cuckootablebuilder builder(writable_file.get(), khashtableratio,
      4, 100, bytewisecomparator(), 1, getslicehash);
  assert_ok(builder.status());
  assert_ok(builder.finish());
  assert_ok(writable_file->close());
  checkfilecontents({}, {}, {}, "", 0, 2, false);
}

test(cuckoobuildertest, writesuccessnocollisionfullkey) {
  uint32_t num_hash_fun = 4;
  std::vector<std::string> user_keys = {"key01", "key02", "key03", "key04"};
  std::vector<std::string> values = {"v01", "v02", "v03", "v04"};
  hash_map = {
    {user_keys[0], {0, 1, 2, 3}},
    {user_keys[1], {1, 2, 3, 4}},
    {user_keys[2], {2, 3, 4, 5}},
    {user_keys[3], {3, 4, 5, 6}}
  };
  std::vector<uint64_t> expected_locations = {0, 1, 2, 3};
  std::vector<std::string> keys;
  for (auto& user_key : user_keys) {
    keys.push_back(getinternalkey(user_key, false));
  }

  unique_ptr<writablefile> writable_file;
  fname = test::tmpdir() + "/nocollisionfullkey";
  assert_ok(env_->newwritablefile(fname, &writable_file, env_options_));
  cuckootablebuilder builder(writable_file.get(), khashtableratio,
      num_hash_fun, 100, bytewisecomparator(), 1, getslicehash);
  assert_ok(builder.status());
  for (uint32_t i = 0; i < user_keys.size(); i++) {
    builder.add(slice(keys[i]), slice(values[i]));
    assert_eq(builder.numentries(), i + 1);
    assert_ok(builder.status());
  }
  assert_ok(builder.finish());
  assert_ok(writable_file->close());

  uint32_t expected_table_size = nextpowof2(keys.size() / khashtableratio);
  std::string expected_unused_bucket = getinternalkey("key00", true);
  expected_unused_bucket += std::string(values[0].size(), 'a');
  checkfilecontents(keys, values, expected_locations,
      expected_unused_bucket, expected_table_size, 2, false);
}

test(cuckoobuildertest, writesuccesswithcollisionfullkey) {
  uint32_t num_hash_fun = 4;
  std::vector<std::string> user_keys = {"key01", "key02", "key03", "key04"};
  std::vector<std::string> values = {"v01", "v02", "v03", "v04"};
  hash_map = {
    {user_keys[0], {0, 1, 2, 3}},
    {user_keys[1], {0, 1, 2, 3}},
    {user_keys[2], {0, 1, 2, 3}},
    {user_keys[3], {0, 1, 2, 3}},
  };
  std::vector<uint64_t> expected_locations = {0, 1, 2, 3};
  std::vector<std::string> keys;
  for (auto& user_key : user_keys) {
    keys.push_back(getinternalkey(user_key, false));
  }

  unique_ptr<writablefile> writable_file;
  fname = test::tmpdir() + "/withcollisionfullkey";
  assert_ok(env_->newwritablefile(fname, &writable_file, env_options_));
  cuckootablebuilder builder(writable_file.get(), khashtableratio,
      num_hash_fun, 100, bytewisecomparator(), 1, getslicehash);
  assert_ok(builder.status());
  for (uint32_t i = 0; i < user_keys.size(); i++) {
    builder.add(slice(keys[i]), slice(values[i]));
    assert_eq(builder.numentries(), i + 1);
    assert_ok(builder.status());
  }
  assert_ok(builder.finish());
  assert_ok(writable_file->close());

  uint32_t expected_table_size = nextpowof2(keys.size() / khashtableratio);
  std::string expected_unused_bucket = getinternalkey("key00", true);
  expected_unused_bucket += std::string(values[0].size(), 'a');
  checkfilecontents(keys, values, expected_locations,
      expected_unused_bucket, expected_table_size, 4, false);
}

test(cuckoobuildertest, writesuccesswithcollisionandcuckooblock) {
  uint32_t num_hash_fun = 4;
  std::vector<std::string> user_keys = {"key01", "key02", "key03", "key04"};
  std::vector<std::string> values = {"v01", "v02", "v03", "v04"};
  hash_map = {
    {user_keys[0], {0, 1, 2, 3}},
    {user_keys[1], {0, 1, 2, 3}},
    {user_keys[2], {0, 1, 2, 3}},
    {user_keys[3], {0, 1, 2, 3}},
  };
  std::vector<uint64_t> expected_locations = {0, 1, 2, 3};
  std::vector<std::string> keys;
  for (auto& user_key : user_keys) {
    keys.push_back(getinternalkey(user_key, false));
  }

  unique_ptr<writablefile> writable_file;
  uint32_t cuckoo_block_size = 2;
  fname = test::tmpdir() + "/withcollisionfullkey2";
  assert_ok(env_->newwritablefile(fname, &writable_file, env_options_));
  cuckootablebuilder builder(writable_file.get(), khashtableratio,
      num_hash_fun, 100, bytewisecomparator(), cuckoo_block_size, getslicehash);
  assert_ok(builder.status());
  for (uint32_t i = 0; i < user_keys.size(); i++) {
    builder.add(slice(keys[i]), slice(values[i]));
    assert_eq(builder.numentries(), i + 1);
    assert_ok(builder.status());
  }
  assert_ok(builder.finish());
  assert_ok(writable_file->close());

  uint32_t expected_table_size = nextpowof2(keys.size() / khashtableratio);
  std::string expected_unused_bucket = getinternalkey("key00", true);
  expected_unused_bucket += std::string(values[0].size(), 'a');
  checkfilecontents(keys, values, expected_locations,
      expected_unused_bucket, expected_table_size, 3, false, cuckoo_block_size);
}

test(cuckoobuildertest, withcollisionpathfullkey) {
  // have two hash functions. insert elements with overlapping hashes.
  // finally insert an element with hash value somewhere in the middle
  // so that it displaces all the elements after that.
  uint32_t num_hash_fun = 2;
  std::vector<std::string> user_keys = {"key01", "key02", "key03",
    "key04", "key05"};
  std::vector<std::string> values = {"v01", "v02", "v03", "v04", "v05"};
  hash_map = {
    {user_keys[0], {0, 1}},
    {user_keys[1], {1, 2}},
    {user_keys[2], {2, 3}},
    {user_keys[3], {3, 4}},
    {user_keys[4], {0, 2}},
  };
  std::vector<uint64_t> expected_locations = {0, 1, 3, 4, 2};
  std::vector<std::string> keys;
  for (auto& user_key : user_keys) {
    keys.push_back(getinternalkey(user_key, false));
  }

  unique_ptr<writablefile> writable_file;
  fname = test::tmpdir() + "/withcollisionpathfullkey";
  assert_ok(env_->newwritablefile(fname, &writable_file, env_options_));
  cuckootablebuilder builder(writable_file.get(), khashtableratio,
      num_hash_fun, 100, bytewisecomparator(), 1, getslicehash);
  assert_ok(builder.status());
  for (uint32_t i = 0; i < user_keys.size(); i++) {
    builder.add(slice(keys[i]), slice(values[i]));
    assert_eq(builder.numentries(), i + 1);
    assert_ok(builder.status());
  }
  assert_ok(builder.finish());
  assert_ok(writable_file->close());

  uint32_t expected_table_size = nextpowof2(keys.size() / khashtableratio);
  std::string expected_unused_bucket = getinternalkey("key00", true);
  expected_unused_bucket += std::string(values[0].size(), 'a');
  checkfilecontents(keys, values, expected_locations,
      expected_unused_bucket, expected_table_size, 2, false);
}

test(cuckoobuildertest, withcollisionpathfullkeyandcuckooblock) {
  uint32_t num_hash_fun = 2;
  std::vector<std::string> user_keys = {"key01", "key02", "key03",
    "key04", "key05"};
  std::vector<std::string> values = {"v01", "v02", "v03", "v04", "v05"};
  hash_map = {
    {user_keys[0], {0, 1}},
    {user_keys[1], {1, 2}},
    {user_keys[2], {3, 4}},
    {user_keys[3], {4, 5}},
    {user_keys[4], {0, 3}},
  };
  std::vector<uint64_t> expected_locations = {2, 1, 3, 4, 0};
  std::vector<std::string> keys;
  for (auto& user_key : user_keys) {
    keys.push_back(getinternalkey(user_key, false));
  }

  unique_ptr<writablefile> writable_file;
  fname = test::tmpdir() + "/withcollisionpathfullkeyandcuckooblock";
  assert_ok(env_->newwritablefile(fname, &writable_file, env_options_));
  cuckootablebuilder builder(writable_file.get(), khashtableratio,
      num_hash_fun, 100, bytewisecomparator(), 2, getslicehash);
  assert_ok(builder.status());
  for (uint32_t i = 0; i < user_keys.size(); i++) {
    builder.add(slice(keys[i]), slice(values[i]));
    assert_eq(builder.numentries(), i + 1);
    assert_ok(builder.status());
  }
  assert_ok(builder.finish());
  assert_ok(writable_file->close());

  uint32_t expected_table_size = nextpowof2(keys.size() / khashtableratio);
  std::string expected_unused_bucket = getinternalkey("key00", true);
  expected_unused_bucket += std::string(values[0].size(), 'a');
  checkfilecontents(keys, values, expected_locations,
      expected_unused_bucket, expected_table_size, 2, false, 2);
}

test(cuckoobuildertest, writesuccessnocollisionuserkey) {
  uint32_t num_hash_fun = 4;
  std::vector<std::string> user_keys = {"key01", "key02", "key03", "key04"};
  std::vector<std::string> values = {"v01", "v02", "v03", "v04"};
  hash_map = {
    {user_keys[0], {0, 1, 2, 3}},
    {user_keys[1], {1, 2, 3, 4}},
    {user_keys[2], {2, 3, 4, 5}},
    {user_keys[3], {3, 4, 5, 6}}
  };
  std::vector<uint64_t> expected_locations = {0, 1, 2, 3};

  unique_ptr<writablefile> writable_file;
  fname = test::tmpdir() + "/nocollisionuserkey";
  assert_ok(env_->newwritablefile(fname, &writable_file, env_options_));
  cuckootablebuilder builder(writable_file.get(), khashtableratio,
      num_hash_fun, 100, bytewisecomparator(), 1, getslicehash);
  assert_ok(builder.status());
  for (uint32_t i = 0; i < user_keys.size(); i++) {
    builder.add(slice(getinternalkey(user_keys[i], true)), slice(values[i]));
    assert_eq(builder.numentries(), i + 1);
    assert_ok(builder.status());
  }
  assert_ok(builder.finish());
  assert_ok(writable_file->close());

  uint32_t expected_table_size = nextpowof2(user_keys.size() / khashtableratio);
  std::string expected_unused_bucket = "key00";
  expected_unused_bucket += std::string(values[0].size(), 'a');
  checkfilecontents(user_keys, values, expected_locations,
      expected_unused_bucket, expected_table_size, 2, true);
}

test(cuckoobuildertest, writesuccesswithcollisionuserkey) {
  uint32_t num_hash_fun = 4;
  std::vector<std::string> user_keys = {"key01", "key02", "key03", "key04"};
  std::vector<std::string> values = {"v01", "v02", "v03", "v04"};
  hash_map = {
    {user_keys[0], {0, 1, 2, 3}},
    {user_keys[1], {0, 1, 2, 3}},
    {user_keys[2], {0, 1, 2, 3}},
    {user_keys[3], {0, 1, 2, 3}},
  };
  std::vector<uint64_t> expected_locations = {0, 1, 2, 3};

  unique_ptr<writablefile> writable_file;
  fname = test::tmpdir() + "/withcollisionuserkey";
  assert_ok(env_->newwritablefile(fname, &writable_file, env_options_));
  cuckootablebuilder builder(writable_file.get(), khashtableratio,
      num_hash_fun, 100, bytewisecomparator(), 1, getslicehash);
  assert_ok(builder.status());
  for (uint32_t i = 0; i < user_keys.size(); i++) {
    builder.add(slice(getinternalkey(user_keys[i], true)), slice(values[i]));
    assert_eq(builder.numentries(), i + 1);
    assert_ok(builder.status());
  }
  assert_ok(builder.finish());
  assert_ok(writable_file->close());

  uint32_t expected_table_size = nextpowof2(user_keys.size() / khashtableratio);
  std::string expected_unused_bucket = "key00";
  expected_unused_bucket += std::string(values[0].size(), 'a');
  checkfilecontents(user_keys, values, expected_locations,
      expected_unused_bucket, expected_table_size, 4, true);
}

test(cuckoobuildertest, withcollisionpathuserkey) {
  uint32_t num_hash_fun = 2;
  std::vector<std::string> user_keys = {"key01", "key02", "key03",
    "key04", "key05"};
  std::vector<std::string> values = {"v01", "v02", "v03", "v04", "v05"};
  hash_map = {
    {user_keys[0], {0, 1}},
    {user_keys[1], {1, 2}},
    {user_keys[2], {2, 3}},
    {user_keys[3], {3, 4}},
    {user_keys[4], {0, 2}},
  };
  std::vector<uint64_t> expected_locations = {0, 1, 3, 4, 2};

  unique_ptr<writablefile> writable_file;
  fname = test::tmpdir() + "/withcollisionpathuserkey";
  assert_ok(env_->newwritablefile(fname, &writable_file, env_options_));
  cuckootablebuilder builder(writable_file.get(), khashtableratio,
      num_hash_fun, 2, bytewisecomparator(), 1, getslicehash);
  assert_ok(builder.status());
  for (uint32_t i = 0; i < user_keys.size(); i++) {
    builder.add(slice(getinternalkey(user_keys[i], true)), slice(values[i]));
    assert_eq(builder.numentries(), i + 1);
    assert_ok(builder.status());
  }
  assert_ok(builder.finish());
  assert_ok(writable_file->close());

  uint32_t expected_table_size = nextpowof2(user_keys.size() / khashtableratio);
  std::string expected_unused_bucket = "key00";
  expected_unused_bucket += std::string(values[0].size(), 'a');
  checkfilecontents(user_keys, values, expected_locations,
      expected_unused_bucket, expected_table_size, 2, true);
}

test(cuckoobuildertest, failwhencollisionpathtoolong) {
  // have two hash functions. insert elements with overlapping hashes.
  // finally try inserting an element with hash value somewhere in the middle
  // and it should fail because the no. of elements to displace is too high.
  uint32_t num_hash_fun = 2;
  std::vector<std::string> user_keys = {"key01", "key02", "key03",
    "key04", "key05"};
  hash_map = {
    {user_keys[0], {0, 1}},
    {user_keys[1], {1, 2}},
    {user_keys[2], {2, 3}},
    {user_keys[3], {3, 4}},
    {user_keys[4], {0, 1}},
  };

  unique_ptr<writablefile> writable_file;
  fname = test::tmpdir() + "/withcollisionpathuserkey";
  assert_ok(env_->newwritablefile(fname, &writable_file, env_options_));
  cuckootablebuilder builder(writable_file.get(), khashtableratio,
      num_hash_fun, 2, bytewisecomparator(), 1, getslicehash);
  assert_ok(builder.status());
  for (uint32_t i = 0; i < user_keys.size(); i++) {
    builder.add(slice(getinternalkey(user_keys[i], false)), slice("value"));
    assert_eq(builder.numentries(), i + 1);
    assert_ok(builder.status());
  }
  assert_true(builder.finish().isnotsupported());
  assert_ok(writable_file->close());
}

test(cuckoobuildertest, failwhensamekeyinserted) {
  hash_map = {{"repeatedkey", {0, 1, 2, 3}}};
  uint32_t num_hash_fun = 4;
  std::string user_key = "repeatedkey";

  unique_ptr<writablefile> writable_file;
  fname = test::tmpdir() + "/failwhensamekeyinserted";
  assert_ok(env_->newwritablefile(fname, &writable_file, env_options_));
  cuckootablebuilder builder(writable_file.get(), khashtableratio,
      num_hash_fun, 100, bytewisecomparator(), 1, getslicehash);
  assert_ok(builder.status());

  builder.add(slice(getinternalkey(user_key, false)), slice("value1"));
  assert_eq(builder.numentries(), 1u);
  assert_ok(builder.status());
  builder.add(slice(getinternalkey(user_key, true)), slice("value2"));
  assert_eq(builder.numentries(), 2u);
  assert_ok(builder.status());

  assert_true(builder.finish().isnotsupported());
  assert_ok(writable_file->close());
}
}  // namespace rocksdb

int main(int argc, char** argv) { return rocksdb::test::runalltests(); }
