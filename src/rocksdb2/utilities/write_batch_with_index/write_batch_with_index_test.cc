//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.


#include <memory>
#include <map>
#include "db/column_family.h"
#include "rocksdb/utilities/write_batch_with_index.h"
#include "util/testharness.h"

namespace rocksdb {

namespace {
class columnfamilyhandleimpldummy : public columnfamilyhandleimpl {
 public:
  explicit columnfamilyhandleimpldummy(int id)
      : columnfamilyhandleimpl(nullptr, nullptr, nullptr), id_(id) {}
  uint32_t getid() const override { return id_; }

 private:
  uint32_t id_;
};

struct entry {
  std::string key;
  std::string value;
  writetype type;
};

struct testhandler : public writebatch::handler {
  std::map<uint32_t, std::vector<entry>> seen;
  virtual status putcf(uint32_t column_family_id, const slice& key,
                       const slice& value) {
    entry e;
    e.key = key.tostring();
    e.value = value.tostring();
    e.type = kputrecord;
    seen[column_family_id].push_back(e);
    return status::ok();
  }
  virtual status mergecf(uint32_t column_family_id, const slice& key,
                         const slice& value) {
    entry e;
    e.key = key.tostring();
    e.value = value.tostring();
    e.type = kmergerecord;
    seen[column_family_id].push_back(e);
    return status::ok();
  }
  virtual void logdata(const slice& blob) {}
  virtual status deletecf(uint32_t column_family_id, const slice& key) {
    entry e;
    e.key = key.tostring();
    e.value = "";
    e.type = kdeleterecord;
    seen[column_family_id].push_back(e);
    return status::ok();
  }
};
}  // namespace anonymous

class writebatchwithindextest {};

test(writebatchwithindextest, testvalueassecondaryindex) {
  entry entries[] = {{"aaa", "0005", kputrecord},
                     {"b", "0002", kputrecord},
                     {"cdd", "0002", kmergerecord},
                     {"aab", "00001", kputrecord},
                     {"cc", "00005", kputrecord},
                     {"cdd", "0002", kputrecord},
                     {"aab", "0003", kputrecord},
                     {"cc", "00005", kdeleterecord}, };

  // in this test, we insert <key, value> to column family `data`, and
  // <value, key> to column family `index`. then iterator them in order
  // and seek them by key.

  // sort entries by key
  std::map<std::string, std::vector<entry*>> data_map;
  // sort entries by value
  std::map<std::string, std::vector<entry*>> index_map;
  for (auto& e : entries) {
    data_map[e.key].push_back(&e);
    index_map[e.value].push_back(&e);
  }

  writebatchwithindex batch(bytewisecomparator(), 20);
  columnfamilyhandleimpldummy data(6), index(8);
  for (auto& e : entries) {
    if (e.type == kputrecord) {
      batch.put(&data, e.key, e.value);
      batch.put(&index, e.value, e.key);
    } else if (e.type == kmergerecord) {
      batch.merge(&data, e.key, e.value);
      batch.put(&index, e.value, e.key);
    } else {
      assert(e.type == kdeleterecord);
      std::unique_ptr<wbwiiterator> iter(batch.newiterator(&data));
      iter->seek(e.key);
      assert_ok(iter->status());
      auto& write_entry = iter->entry();
      assert_eq(e.key, write_entry.key.tostring());
      assert_eq(e.value, write_entry.value.tostring());
      batch.delete(&data, e.key);
      batch.put(&index, e.value, "");
    }
  }

  // iterator all keys
  {
    std::unique_ptr<wbwiiterator> iter(batch.newiterator(&data));
    iter->seek("");
    for (auto pair : data_map) {
      for (auto v : pair.second) {
        assert_ok(iter->status());
        assert_true(iter->valid());
        auto& write_entry = iter->entry();
        assert_eq(pair.first, write_entry.key.tostring());
        assert_eq(v->type, write_entry.type);
        if (write_entry.type != kdeleterecord) {
          assert_eq(v->value, write_entry.value.tostring());
        }
        iter->next();
      }
    }
    assert_true(!iter->valid());
  }

  // iterator all indexes
  {
    std::unique_ptr<wbwiiterator> iter(batch.newiterator(&index));
    iter->seek("");
    for (auto pair : index_map) {
      for (auto v : pair.second) {
        assert_ok(iter->status());
        assert_true(iter->valid());
        auto& write_entry = iter->entry();
        assert_eq(pair.first, write_entry.key.tostring());
        if (v->type != kdeleterecord) {
          assert_eq(v->key, write_entry.value.tostring());
          assert_eq(v->value, write_entry.key.tostring());
        }
        iter->next();
      }
    }
    assert_true(!iter->valid());
  }

  // seek to every key
  {
    std::unique_ptr<wbwiiterator> iter(batch.newiterator(&data));

    // seek the keys one by one in reverse order
    for (auto pair = data_map.rbegin(); pair != data_map.rend(); ++pair) {
      iter->seek(pair->first);
      assert_ok(iter->status());
      for (auto v : pair->second) {
        assert_true(iter->valid());
        auto& write_entry = iter->entry();
        assert_eq(pair->first, write_entry.key.tostring());
        assert_eq(v->type, write_entry.type);
        if (write_entry.type != kdeleterecord) {
          assert_eq(v->value, write_entry.value.tostring());
        }
        iter->next();
        assert_ok(iter->status());
      }
    }
  }

  // seek to every index
  {
    std::unique_ptr<wbwiiterator> iter(batch.newiterator(&index));

    // seek the keys one by one in reverse order
    for (auto pair = index_map.rbegin(); pair != index_map.rend(); ++pair) {
      iter->seek(pair->first);
      assert_ok(iter->status());
      for (auto v : pair->second) {
        assert_true(iter->valid());
        auto& write_entry = iter->entry();
        assert_eq(pair->first, write_entry.key.tostring());
        assert_eq(v->value, write_entry.key.tostring());
        if (v->type != kdeleterecord) {
          assert_eq(v->key, write_entry.value.tostring());
        }
        iter->next();
        assert_ok(iter->status());
      }
    }
  }

  // verify writebatch can be iterated
  testhandler handler;
  batch.getwritebatch()->iterate(&handler);

  // verify data column family
  {
    assert_eq(sizeof(entries) / sizeof(entry),
              handler.seen[data.getid()].size());
    size_t i = 0;
    for (auto e : handler.seen[data.getid()]) {
      auto write_entry = entries[i++];
      assert_eq(e.type, write_entry.type);
      assert_eq(e.key, write_entry.key);
      if (e.type != kdeleterecord) {
        assert_eq(e.value, write_entry.value);
      }
    }
  }

  // verify index column family
  {
    assert_eq(sizeof(entries) / sizeof(entry),
              handler.seen[index.getid()].size());
    size_t i = 0;
    for (auto e : handler.seen[index.getid()]) {
      auto write_entry = entries[i++];
      assert_eq(e.key, write_entry.value);
      if (write_entry.type != kdeleterecord) {
        assert_eq(e.value, write_entry.key);
      }
    }
  }
}

}  // namespace

int main(int argc, char** argv) { return rocksdb::test::runalltests(); }
