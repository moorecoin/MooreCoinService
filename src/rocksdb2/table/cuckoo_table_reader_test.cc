// copyright (c) 2014, facebook, inc. all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#ifndef gflags
#include <cstdio>
int main() {
  fprintf(stderr, "please install gflags to run this test\n");
  return 1;
}
#else

#define __stdc_format_macros
#include <inttypes.h>
#include <gflags/gflags.h>
#include <vector>
#include <string>
#include <map>

#include "table/meta_blocks.h"
#include "table/cuckoo_table_builder.h"
#include "table/cuckoo_table_reader.h"
#include "table/cuckoo_table_factory.h"
#include "util/arena.h"
#include "util/random.h"
#include "util/testharness.h"
#include "util/testutil.h"

using gflags::parsecommandlineflags;
using gflags::setusagemessage;

define_string(file_dir, "", "directory where the files will be created"
    " for benchmark. added for using tmpfs.");
define_bool(enable_perf, false, "run benchmark tests too.");
define_bool(write, false,
    "should write new values to file in performance tests?");

namespace rocksdb {

namespace {
const uint32_t knumhashfunc = 10;
// methods, variables related to hash functions.
std::unordered_map<std::string, std::vector<uint64_t>> hash_map;

void addhashlookups(const std::string& s, uint64_t bucket_id,
        uint32_t num_hash_fun) {
  std::vector<uint64_t> v;
  for (uint32_t i = 0; i < num_hash_fun; i++) {
    v.push_back(bucket_id + i);
  }
  hash_map[s] = v;
}

uint64_t getslicehash(const slice& s, uint32_t index,
    uint64_t max_num_buckets) {
  return hash_map[s.tostring()][index];
}

// methods, variables for checking key and values read.
struct valuestoassert {
  valuestoassert(const std::string& key, const slice& value)
    : expected_user_key(key),
      expected_value(value),
      call_count(0) {}
  std::string expected_user_key;
  slice expected_value;
  int call_count;
};

bool assertvalues(void* assert_obj,
    const parsedinternalkey& k, const slice& v) {
  valuestoassert *ptr = reinterpret_cast<valuestoassert*>(assert_obj);
  assert_eq(ptr->expected_value.tostring(), v.tostring());
  assert_eq(ptr->expected_user_key, k.user_key.tostring());
  ++ptr->call_count;
  return false;
}
}  // namespace

class cuckooreadertest {
 public:
  cuckooreadertest() {
    options.allow_mmap_reads = true;
    env = options.env;
    env_options = envoptions(options);
  }

  void setup(int num_items) {
    this->num_items = num_items;
    hash_map.clear();
    keys.clear();
    keys.resize(num_items);
    user_keys.clear();
    user_keys.resize(num_items);
    values.clear();
    values.resize(num_items);
  }

  std::string numtostr(int64_t i) {
    return std::string(reinterpret_cast<char*>(&i), sizeof(i));
  }

  void createcuckoofileandcheckreader(
      const comparator* ucomp = bytewisecomparator()) {
    std::unique_ptr<writablefile> writable_file;
    assert_ok(env->newwritablefile(fname, &writable_file, env_options));
    cuckootablebuilder builder(
        writable_file.get(), 0.9, knumhashfunc, 100, ucomp, 2, getslicehash);
    assert_ok(builder.status());
    for (uint32_t key_idx = 0; key_idx < num_items; ++key_idx) {
      builder.add(slice(keys[key_idx]), slice(values[key_idx]));
      assert_ok(builder.status());
      assert_eq(builder.numentries(), key_idx + 1);
    }
    assert_ok(builder.finish());
    assert_eq(num_items, builder.numentries());
    file_size = builder.filesize();
    assert_ok(writable_file->close());

    // check reader now.
    std::unique_ptr<randomaccessfile> read_file;
    assert_ok(env->newrandomaccessfile(fname, &read_file, env_options));
    cuckootablereader reader(
        options,
        std::move(read_file),
        file_size,
        ucomp,
        getslicehash);
    assert_ok(reader.status());
    for (uint32_t i = 0; i < num_items; ++i) {
      valuestoassert v(user_keys[i], values[i]);
      assert_ok(reader.get(
            readoptions(), slice(keys[i]), &v, assertvalues, nullptr));
      assert_eq(1, v.call_count);
    }
  }
  void updatekeys(bool with_zero_seqno) {
    for (uint32_t i = 0; i < num_items; i++) {
      parsedinternalkey ikey(user_keys[i],
          with_zero_seqno ? 0 : i + 1000, ktypevalue);
      keys[i].clear();
      appendinternalkey(&keys[i], ikey);
    }
  }

  void checkiterator(const comparator* ucomp = bytewisecomparator()) {
    std::unique_ptr<randomaccessfile> read_file;
    assert_ok(env->newrandomaccessfile(fname, &read_file, env_options));
    cuckootablereader reader(
        options,
        std::move(read_file),
        file_size,
        ucomp,
        getslicehash);
    assert_ok(reader.status());
    iterator* it = reader.newiterator(readoptions(), nullptr);
    assert_ok(it->status());
    assert_true(!it->valid());
    it->seektofirst();
    int cnt = 0;
    while (it->valid()) {
      assert_ok(it->status());
      assert_true(slice(keys[cnt]) == it->key());
      assert_true(slice(values[cnt]) == it->value());
      ++cnt;
      it->next();
    }
    assert_eq(static_cast<uint32_t>(cnt), num_items);

    it->seektolast();
    cnt = num_items - 1;
    assert_true(it->valid());
    while (it->valid()) {
      assert_ok(it->status());
      assert_true(slice(keys[cnt]) == it->key());
      assert_true(slice(values[cnt]) == it->value());
      --cnt;
      it->prev();
    }
    assert_eq(cnt, -1);

    cnt = num_items / 2;
    it->seek(keys[cnt]);
    while (it->valid()) {
      assert_ok(it->status());
      assert_true(slice(keys[cnt]) == it->key());
      assert_true(slice(values[cnt]) == it->value());
      ++cnt;
      it->next();
    }
    assert_eq(static_cast<uint32_t>(cnt), num_items);
    delete it;

    arena arena;
    it = reader.newiterator(readoptions(), &arena);
    assert_ok(it->status());
    assert_true(!it->valid());
    it->seek(keys[num_items/2]);
    assert_true(it->valid());
    assert_ok(it->status());
    assert_true(keys[num_items/2] == it->key());
    assert_true(values[num_items/2] == it->value());
    assert_ok(it->status());
    it->~iterator();
  }

  std::vector<std::string> keys;
  std::vector<std::string> user_keys;
  std::vector<std::string> values;
  uint64_t num_items;
  std::string fname;
  uint64_t file_size;
  options options;
  env* env;
  envoptions env_options;
};

test(cuckooreadertest, whenkeyexists) {
  setup(knumhashfunc);
  fname = test::tmpdir() + "/cuckooreader_whenkeyexists";
  for (uint64_t i = 0; i < num_items; i++) {
    user_keys[i] = "key" + numtostr(i);
    parsedinternalkey ikey(user_keys[i], i + 1000, ktypevalue);
    appendinternalkey(&keys[i], ikey);
    values[i] = "value" + numtostr(i);
    // give disjoint hash values.
    addhashlookups(user_keys[i], i, knumhashfunc);
  }
  createcuckoofileandcheckreader();
  // last level file.
  updatekeys(true);
  createcuckoofileandcheckreader();
  // test with collision. make all hash values collide.
  hash_map.clear();
  for (uint32_t i = 0; i < num_items; i++) {
    addhashlookups(user_keys[i], 0, knumhashfunc);
  }
  updatekeys(false);
  createcuckoofileandcheckreader();
  // last level file.
  updatekeys(true);
  createcuckoofileandcheckreader();
}

test(cuckooreadertest, whenkeyexistswithuint64comparator) {
  setup(knumhashfunc);
  fname = test::tmpdir() + "/cuckooreaderuint64_whenkeyexists";
  for (uint64_t i = 0; i < num_items; i++) {
    user_keys[i].resize(8);
    memcpy(&user_keys[i][0], static_cast<void*>(&i), 8);
    parsedinternalkey ikey(user_keys[i], i + 1000, ktypevalue);
    appendinternalkey(&keys[i], ikey);
    values[i] = "value" + numtostr(i);
    // give disjoint hash values.
    addhashlookups(user_keys[i], i, knumhashfunc);
  }
  createcuckoofileandcheckreader(test::uint64comparator());
  // last level file.
  updatekeys(true);
  createcuckoofileandcheckreader(test::uint64comparator());
  // test with collision. make all hash values collide.
  hash_map.clear();
  for (uint32_t i = 0; i < num_items; i++) {
    addhashlookups(user_keys[i], 0, knumhashfunc);
  }
  updatekeys(false);
  createcuckoofileandcheckreader(test::uint64comparator());
  // last level file.
  updatekeys(true);
  createcuckoofileandcheckreader(test::uint64comparator());
}

test(cuckooreadertest, checkiterator) {
  setup(2*knumhashfunc);
  fname = test::tmpdir() + "/cuckooreader_checkiterator";
  for (uint64_t i = 0; i < num_items; i++) {
    user_keys[i] = "key" + numtostr(i);
    parsedinternalkey ikey(user_keys[i], 1000, ktypevalue);
    appendinternalkey(&keys[i], ikey);
    values[i] = "value" + numtostr(i);
    // give disjoint hash values, in reverse order.
    addhashlookups(user_keys[i], num_items-i-1, knumhashfunc);
  }
  createcuckoofileandcheckreader();
  checkiterator();
  // last level file.
  updatekeys(true);
  createcuckoofileandcheckreader();
  checkiterator();
}

test(cuckooreadertest, checkiteratoruint64) {
  setup(2*knumhashfunc);
  fname = test::tmpdir() + "/cuckooreader_checkiterator";
  for (uint64_t i = 0; i < num_items; i++) {
    user_keys[i].resize(8);
    memcpy(&user_keys[i][0], static_cast<void*>(&i), 8);
    parsedinternalkey ikey(user_keys[i], 1000, ktypevalue);
    appendinternalkey(&keys[i], ikey);
    values[i] = "value" + numtostr(i);
    // give disjoint hash values, in reverse order.
    addhashlookups(user_keys[i], num_items-i-1, knumhashfunc);
  }
  createcuckoofileandcheckreader(test::uint64comparator());
  checkiterator(test::uint64comparator());
  // last level file.
  updatekeys(true);
  createcuckoofileandcheckreader(test::uint64comparator());
  checkiterator(test::uint64comparator());
}

test(cuckooreadertest, whenkeynotfound) {
  // add keys with colliding hash values.
  setup(knumhashfunc);
  fname = test::tmpdir() + "/cuckooreader_whenkeynotfound";
  for (uint64_t i = 0; i < num_items; i++) {
    user_keys[i] = "key" + numtostr(i);
    parsedinternalkey ikey(user_keys[i], i + 1000, ktypevalue);
    appendinternalkey(&keys[i], ikey);
    values[i] = "value" + numtostr(i);
    // make all hash values collide.
    addhashlookups(user_keys[i], 0, knumhashfunc);
  }
  createcuckoofileandcheckreader();
  std::unique_ptr<randomaccessfile> read_file;
  assert_ok(env->newrandomaccessfile(fname, &read_file, env_options));
  cuckootablereader reader(
      options,
      std::move(read_file),
      file_size,
      bytewisecomparator(),
      getslicehash);
  assert_ok(reader.status());
  // search for a key with colliding hash values.
  std::string not_found_user_key = "key" + numtostr(num_items);
  std::string not_found_key;
  addhashlookups(not_found_user_key, 0, knumhashfunc);
  parsedinternalkey ikey(not_found_user_key, 1000, ktypevalue);
  appendinternalkey(&not_found_key, ikey);
  valuestoassert v("", "");
  assert_ok(reader.get(
        readoptions(), slice(not_found_key), &v, assertvalues, nullptr));
  assert_eq(0, v.call_count);
  assert_ok(reader.status());
  // search for a key with an independent hash value.
  std::string not_found_user_key2 = "key" + numtostr(num_items + 1);
  addhashlookups(not_found_user_key2, knumhashfunc, knumhashfunc);
  parsedinternalkey ikey2(not_found_user_key2, 1000, ktypevalue);
  std::string not_found_key2;
  appendinternalkey(&not_found_key2, ikey2);
  assert_ok(reader.get(
        readoptions(), slice(not_found_key2), &v, assertvalues, nullptr));
  assert_eq(0, v.call_count);
  assert_ok(reader.status());

  // test read when key is unused key.
  std::string unused_key =
    reader.gettableproperties()->user_collected_properties.at(
    cuckootablepropertynames::kemptykey);
  // add hash values that map to empty buckets.
  addhashlookups(extractuserkey(unused_key).tostring(),
      knumhashfunc, knumhashfunc);
  assert_ok(reader.get(
        readoptions(), slice(unused_key), &v, assertvalues, nullptr));
  assert_eq(0, v.call_count);
  assert_ok(reader.status());
}

// performance tests
namespace {
bool donothing(void* arg, const parsedinternalkey& k, const slice& v) {
  // deliberately empty.
  return false;
}

bool checkvalue(void* cnt_ptr, const parsedinternalkey& k, const slice& v) {
  ++*reinterpret_cast<int*>(cnt_ptr);
  std::string expected_value;
  appendinternalkey(&expected_value, k);
  assert_eq(0, v.compare(slice(&expected_value[0], v.size())));
  return false;
}

void getkeys(uint64_t num, std::vector<std::string>* keys) {
  iterkey k;
  k.setinternalkey("", 0, ktypevalue);
  std::string internal_key_suffix = k.getkey().tostring();
  assert_eq(static_cast<size_t>(8), internal_key_suffix.size());
  for (uint64_t key_idx = 0; key_idx < num; ++key_idx) {
    std::string new_key(reinterpret_cast<char*>(&key_idx), sizeof(key_idx));
    new_key += internal_key_suffix;
    keys->push_back(new_key);
  }
}

std::string getfilename(uint64_t num) {
  if (flags_file_dir.empty()) {
    flags_file_dir = test::tmpdir();
  }
  return flags_file_dir + "/cuckoo_read_benchmark" +
    std::to_string(num/1000000) + "mkeys";
}

// create last level file as we are interested in measuring performance of
// last level file only.
void writefile(const std::vector<std::string>& keys,
    const uint64_t num, double hash_ratio) {
  options options;
  options.allow_mmap_reads = true;
  env* env = options.env;
  envoptions env_options = envoptions(options);
  std::string fname = getfilename(num);

  std::unique_ptr<writablefile> writable_file;
  assert_ok(env->newwritablefile(fname, &writable_file, env_options));
  cuckootablebuilder builder(
      writable_file.get(), hash_ratio,
      64, 1000, test::uint64comparator(), 5, nullptr);
  assert_ok(builder.status());
  for (uint64_t key_idx = 0; key_idx < num; ++key_idx) {
    // value is just a part of key.
    builder.add(slice(keys[key_idx]), slice(&keys[key_idx][0], 4));
    assert_eq(builder.numentries(), key_idx + 1);
    assert_ok(builder.status());
  }
  assert_ok(builder.finish());
  assert_eq(num, builder.numentries());
  assert_ok(writable_file->close());

  uint64_t file_size;
  env->getfilesize(fname, &file_size);
  std::unique_ptr<randomaccessfile> read_file;
  assert_ok(env->newrandomaccessfile(fname, &read_file, env_options));

  cuckootablereader reader(
      options, std::move(read_file), file_size,
      test::uint64comparator(), nullptr);
  assert_ok(reader.status());
  readoptions r_options;
  for (uint64_t i = 0; i < num; ++i) {
    int cnt = 0;
    assert_ok(reader.get(r_options, slice(keys[i]), &cnt, checkvalue, nullptr));
    if (cnt != 1) {
      fprintf(stderr, "%" priu64 " not found.\n", i);
      assert_eq(1, cnt);
    }
  }
}

void readkeys(uint64_t num, uint32_t batch_size) {
  options options;
  options.allow_mmap_reads = true;
  env* env = options.env;
  envoptions env_options = envoptions(options);
  std::string fname = getfilename(num);

  uint64_t file_size;
  env->getfilesize(fname, &file_size);
  std::unique_ptr<randomaccessfile> read_file;
  assert_ok(env->newrandomaccessfile(fname, &read_file, env_options));

  cuckootablereader reader(
      options, std::move(read_file), file_size, test::uint64comparator(),
      nullptr);
  assert_ok(reader.status());
  const usercollectedproperties user_props =
    reader.gettableproperties()->user_collected_properties;
  const uint32_t num_hash_fun = *reinterpret_cast<const uint32_t*>(
      user_props.at(cuckootablepropertynames::knumhashfunc).data());
  const uint64_t table_size = *reinterpret_cast<const uint64_t*>(
      user_props.at(cuckootablepropertynames::khashtablesize).data());
  fprintf(stderr, "with %" priu64 " items, utilization is %.2f%%, number of"
      " hash functions: %u.\n", num, num * 100.0 / (table_size), num_hash_fun);
  readoptions r_options;

  uint64_t start_time = env->nowmicros();
  if (batch_size > 0) {
    for (uint64_t i = 0; i < num; i += batch_size) {
      for (uint64_t j = i; j < i+batch_size && j < num; ++j) {
        reader.prepare(slice(reinterpret_cast<char*>(&j), 16));
      }
      for (uint64_t j = i; j < i+batch_size && j < num; ++j) {
        reader.get(r_options, slice(reinterpret_cast<char*>(&j), 16),
            nullptr, donothing, nullptr);
      }
    }
  } else {
    for (uint64_t i = 0; i < num; i++) {
      reader.get(r_options, slice(reinterpret_cast<char*>(&i), 16), nullptr,
          donothing, nullptr);
    }
  }
  float time_per_op = (env->nowmicros() - start_time) * 1.0 / num;
  fprintf(stderr,
      "time taken per op is %.3fus (%.1f mqps) with batch size of %u\n",
      time_per_op, 1.0 / time_per_op, batch_size);
}
}  // namespace.

test(cuckooreadertest, testreadperformance) {
  if (!flags_enable_perf) {
    return;
  }
  double hash_ratio = 0.95;
  // these numbers are chosen to have a hash utilizaiton % close to
  // 0.9, 0.75, 0.6 and 0.5 respectively.
  // they all create 128 m buckets.
  std::vector<uint64_t> nums = {120*1000*1000, 100*1000*1000, 80*1000*1000,
    70*1000*1000};
#ifndef ndebug
  fprintf(stdout,
      "warning: not compiled with dndebug. performance tests may be slow.\n");
#endif
  std::vector<std::string> keys;
  getkeys(*std::max_element(nums.begin(), nums.end()), &keys);
  for (uint64_t num : nums) {
    if (flags_write || !env::default()->fileexists(getfilename(num))) {
      writefile(keys, num, hash_ratio);
    }
    readkeys(num, 0);
    readkeys(num, 10);
    readkeys(num, 25);
    readkeys(num, 50);
    readkeys(num, 100);
    fprintf(stderr, "\n");
  }
}
}  // namespace rocksdb

int main(int argc, char** argv) {
  parsecommandlineflags(&argc, &argv, true);
  rocksdb::test::runalltests();
  return 0;
}

#endif  // gflags.
