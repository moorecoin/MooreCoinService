// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
#include <algorithm>
#include <set>

#include "db/db_impl.h"
#include "db/filename.h"
#include "db/version_set.h"
#include "db/write_batch_internal.h"
#include "rocksdb/cache.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/table.h"
#include "table/meta_blocks.h"
#include "table/bloom_block.h"
#include "table/plain_table_factory.h"
#include "table/plain_table_reader.h"
#include "util/hash.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/testharness.h"
#include "util/testutil.h"
#include "utilities/merge_operators.h"

using std::unique_ptr;

namespace rocksdb {

class plaintabledbtest {
 protected:
 private:
  std::string dbname_;
  env* env_;
  db* db_;

  options last_options_;

 public:
  plaintabledbtest() : env_(env::default()) {
    dbname_ = test::tmpdir() + "/plain_table_db_test";
    assert_ok(destroydb(dbname_, options()));
    db_ = nullptr;
    reopen();
  }

  ~plaintabledbtest() {
    delete db_;
    assert_ok(destroydb(dbname_, options()));
  }

  // return the current option configuration.
  options currentoptions() {
    options options;

    plaintableoptions plain_table_options;
    plain_table_options.user_key_len = 0;
    plain_table_options.bloom_bits_per_key = 2;
    plain_table_options.hash_table_ratio = 0.8;
    plain_table_options.index_sparseness = 3;
    plain_table_options.huge_page_tlb_size = 0;
    plain_table_options.encoding_type = kprefix;
    plain_table_options.full_scan_mode = false;
    plain_table_options.store_index_in_file = false;

    options.table_factory.reset(newplaintablefactory(plain_table_options));
    options.memtable_factory.reset(newhashlinklistrepfactory(4, 0, 3, true));

    options.prefix_extractor.reset(newfixedprefixtransform(8));
    options.allow_mmap_reads = true;
    return options;
  }

  dbimpl* dbfull() {
    return reinterpret_cast<dbimpl*>(db_);
  }

  void reopen(options* options = nullptr) {
    assert_ok(tryreopen(options));
  }

  void close() {
    delete db_;
    db_ = nullptr;
  }

  void destroyandreopen(options* options = nullptr) {
    //destroy using last options
    destroy(&last_options_);
    assert_ok(tryreopen(options));
  }

  void destroy(options* options) {
    delete db_;
    db_ = nullptr;
    assert_ok(destroydb(dbname_, *options));
  }

  status purereopen(options* options, db** db) {
    return db::open(*options, dbname_, db);
  }

  status tryreopen(options* options = nullptr) {
    delete db_;
    db_ = nullptr;
    options opts;
    if (options != nullptr) {
      opts = *options;
    } else {
      opts = currentoptions();
      opts.create_if_missing = true;
    }
    last_options_ = opts;

    return db::open(opts, dbname_, &db_);
  }

  status put(const slice& k, const slice& v) {
    return db_->put(writeoptions(), k, v);
  }

  status delete(const std::string& k) {
    return db_->delete(writeoptions(), k);
  }

  std::string get(const std::string& k, const snapshot* snapshot = nullptr) {
    readoptions options;
    options.snapshot = snapshot;
    std::string result;
    status s = db_->get(options, k, &result);
    if (s.isnotfound()) {
      result = "not_found";
    } else if (!s.ok()) {
      result = s.tostring();
    }
    return result;
  }


  int numtablefilesatlevel(int level) {
    std::string property;
    assert_true(
        db_->getproperty("rocksdb.num-files-at-level" + numbertostring(level),
                         &property));
    return atoi(property.c_str());
  }

  // return spread of files per level
  std::string filesperlevel() {
    std::string result;
    int last_non_zero_offset = 0;
    for (int level = 0; level < db_->numberlevels(); level++) {
      int f = numtablefilesatlevel(level);
      char buf[100];
      snprintf(buf, sizeof(buf), "%s%d", (level ? "," : ""), f);
      result += buf;
      if (f > 0) {
        last_non_zero_offset = result.size();
      }
    }
    result.resize(last_non_zero_offset);
    return result;
  }

  std::string iterstatus(iterator* iter) {
    std::string result;
    if (iter->valid()) {
      result = iter->key().tostring() + "->" + iter->value().tostring();
    } else {
      result = "(invalid)";
    }
    return result;
  }
};

test(plaintabledbtest, empty) {
  assert_true(dbfull() != nullptr);
  assert_eq("not_found", get("0000000000000foo"));
}

extern const uint64_t kplaintablemagicnumber;

class testplaintablereader : public plaintablereader {
 public:
  testplaintablereader(const envoptions& storage_options,
                       const internalkeycomparator& icomparator,
                       encodingtype encoding_type, uint64_t file_size,
                       int bloom_bits_per_key, double hash_table_ratio,
                       size_t index_sparseness,
                       const tableproperties* table_properties,
                       unique_ptr<randomaccessfile>&& file,
                       const options& options, bool* expect_bloom_not_match,
                       bool store_index_in_file)
      : plaintablereader(options, std::move(file), storage_options, icomparator,
                         encoding_type, file_size, table_properties),
        expect_bloom_not_match_(expect_bloom_not_match) {
    status s = mmapdatafile();
    assert_true(s.ok());

    s = populateindex(const_cast<tableproperties*>(table_properties),
                      bloom_bits_per_key, hash_table_ratio, index_sparseness,
                      2 * 1024 * 1024);
    assert_true(s.ok());

    tableproperties* props = const_cast<tableproperties*>(table_properties);
    if (store_index_in_file) {
      auto bloom_version_ptr = props->user_collected_properties.find(
          plaintablepropertynames::kbloomversion);
      assert_true(bloom_version_ptr != props->user_collected_properties.end());
      assert_eq(bloom_version_ptr->second, std::string("1"));
      if (options.bloom_locality > 0) {
        auto num_blocks_ptr = props->user_collected_properties.find(
            plaintablepropertynames::knumbloomblocks);
        assert_true(num_blocks_ptr != props->user_collected_properties.end());
      }
    }
  }

  virtual ~testplaintablereader() {}

 private:
  virtual bool matchbloom(uint32_t hash) const override {
    bool ret = plaintablereader::matchbloom(hash);
    if (*expect_bloom_not_match_) {
      assert_true(!ret);
    } else {
      assert_true(ret);
    }
    return ret;
  }
  bool* expect_bloom_not_match_;
};

extern const uint64_t kplaintablemagicnumber;
class testplaintablefactory : public plaintablefactory {
 public:
  explicit testplaintablefactory(bool* expect_bloom_not_match,
                                 const plaintableoptions& options)
      : plaintablefactory(options),
        bloom_bits_per_key_(options.bloom_bits_per_key),
        hash_table_ratio_(options.hash_table_ratio),
        index_sparseness_(options.index_sparseness),
        store_index_in_file_(options.store_index_in_file),
        expect_bloom_not_match_(expect_bloom_not_match) {}

  status newtablereader(const options& options, const envoptions& soptions,
                        const internalkeycomparator& internal_comparator,
                        unique_ptr<randomaccessfile>&& file, uint64_t file_size,
                        unique_ptr<tablereader>* table) const override {
    tableproperties* props = nullptr;
    auto s = readtableproperties(file.get(), file_size, kplaintablemagicnumber,
                                 options.env, options.info_log.get(), &props);
    assert_true(s.ok());

    if (store_index_in_file_) {
      blockhandle bloom_block_handle;
      s = findmetablock(file.get(), file_size, kplaintablemagicnumber,
                        options.env, bloomblockbuilder::kbloomblock,
                        &bloom_block_handle);
      assert_true(s.ok());

      blockhandle index_block_handle;
      s = findmetablock(
          file.get(), file_size, kplaintablemagicnumber, options.env,
          plaintableindexbuilder::kplaintableindexblock, &index_block_handle);
      assert_true(s.ok());
    }

    auto& user_props = props->user_collected_properties;
    auto encoding_type_prop =
        user_props.find(plaintablepropertynames::kencodingtype);
    assert(encoding_type_prop != user_props.end());
    encodingtype encoding_type = static_cast<encodingtype>(
        decodefixed32(encoding_type_prop->second.c_str()));

    std::unique_ptr<plaintablereader> new_reader(new testplaintablereader(
        soptions, internal_comparator, encoding_type, file_size,
        bloom_bits_per_key_, hash_table_ratio_, index_sparseness_, props,
        std::move(file), options, expect_bloom_not_match_,
        store_index_in_file_));

    *table = std::move(new_reader);
    return s;
  }

 private:
  int bloom_bits_per_key_;
  double hash_table_ratio_;
  size_t index_sparseness_;
  bool store_index_in_file_;
  bool* expect_bloom_not_match_;
};

test(plaintabledbtest, flush) {
  for (size_t huge_page_tlb_size = 0; huge_page_tlb_size <= 2 * 1024 * 1024;
       huge_page_tlb_size += 2 * 1024 * 1024) {
    for (encodingtype encoding_type : {kplain, kprefix}) {
    for (int bloom_bits = 0; bloom_bits <= 117; bloom_bits += 117) {
      for (int total_order = 0; total_order <= 1; total_order++) {
        for (int store_index_in_file = 0; store_index_in_file <= 1;
             ++store_index_in_file) {
          if (!bloom_bits && store_index_in_file) {
            continue;
          }

          options options = currentoptions();
          options.create_if_missing = true;
          // set only one bucket to force bucket conflict.
          // test index interval for the same prefix to be 1, 2 and 4
          if (total_order) {
            options.prefix_extractor.reset();

            plaintableoptions plain_table_options;
            plain_table_options.user_key_len = 0;
            plain_table_options.bloom_bits_per_key = bloom_bits;
            plain_table_options.hash_table_ratio = 0;
            plain_table_options.index_sparseness = 2;
            plain_table_options.huge_page_tlb_size = huge_page_tlb_size;
            plain_table_options.encoding_type = encoding_type;
            plain_table_options.full_scan_mode = false;
            plain_table_options.store_index_in_file = store_index_in_file;

            options.table_factory.reset(
                newplaintablefactory(plain_table_options));
          } else {
            plaintableoptions plain_table_options;
            plain_table_options.user_key_len = 0;
            plain_table_options.bloom_bits_per_key = bloom_bits;
            plain_table_options.hash_table_ratio = 0.75;
            plain_table_options.index_sparseness = 16;
            plain_table_options.huge_page_tlb_size = huge_page_tlb_size;
            plain_table_options.encoding_type = encoding_type;
            plain_table_options.full_scan_mode = false;
            plain_table_options.store_index_in_file = store_index_in_file;

            options.table_factory.reset(
                newplaintablefactory(plain_table_options));
          }
          destroyandreopen(&options);
          uint64_t int_num;
          assert_true(dbfull()->getintproperty(
              "rocksdb.estimate-table-readers-mem", &int_num));
          assert_eq(int_num, 0u);

          assert_ok(put("1000000000000foo", "v1"));
          assert_ok(put("0000000000000bar", "v2"));
          assert_ok(put("1000000000000foo", "v3"));
          dbfull()->test_flushmemtable();

          assert_true(dbfull()->getintproperty(
              "rocksdb.estimate-table-readers-mem", &int_num));
          assert_gt(int_num, 0u);

          tablepropertiescollection ptc;
          reinterpret_cast<db*>(dbfull())->getpropertiesofalltables(&ptc);
          assert_eq(1u, ptc.size());
          auto row = ptc.begin();
          auto tp = row->second;

          if (!store_index_in_file) {
            assert_eq(total_order ? "4" : "12",
                      (tp->user_collected_properties)
                          .at("plain_table_hash_table_size"));
            assert_eq("0", (tp->user_collected_properties)
                               .at("plain_table_sub_index_size"));
          } else {
            assert_eq("0", (tp->user_collected_properties)
                               .at("plain_table_hash_table_size"));
            assert_eq("0", (tp->user_collected_properties)
                               .at("plain_table_sub_index_size"));
          }
          assert_eq("v3", get("1000000000000foo"));
          assert_eq("v2", get("0000000000000bar"));
        }
        }
      }
    }
  }
}

test(plaintabledbtest, flush2) {
  for (size_t huge_page_tlb_size = 0; huge_page_tlb_size <= 2 * 1024 * 1024;
       huge_page_tlb_size += 2 * 1024 * 1024) {
    for (encodingtype encoding_type : {kplain, kprefix}) {
    for (int bloom_bits = 0; bloom_bits <= 117; bloom_bits += 117) {
      for (int total_order = 0; total_order <= 1; total_order++) {
        for (int store_index_in_file = 0; store_index_in_file <= 1;
             ++store_index_in_file) {
          if (encoding_type == kprefix && total_order) {
            continue;
          }
          if (!bloom_bits && store_index_in_file) {
            continue;
          }
          if (total_order && store_index_in_file) {
          continue;
        }
        bool expect_bloom_not_match = false;
        options options = currentoptions();
        options.create_if_missing = true;
        // set only one bucket to force bucket conflict.
        // test index interval for the same prefix to be 1, 2 and 4
        plaintableoptions plain_table_options;
        if (total_order) {
          options.prefix_extractor = nullptr;
          plain_table_options.hash_table_ratio = 0;
          plain_table_options.index_sparseness = 2;
        } else {
          plain_table_options.hash_table_ratio = 0.75;
          plain_table_options.index_sparseness = 16;
        }
        plain_table_options.user_key_len = kplaintablevariablelength;
        plain_table_options.bloom_bits_per_key = bloom_bits;
        plain_table_options.huge_page_tlb_size = huge_page_tlb_size;
        plain_table_options.encoding_type = encoding_type;
        plain_table_options.store_index_in_file = store_index_in_file;
        options.table_factory.reset(new testplaintablefactory(
            &expect_bloom_not_match, plain_table_options));

        destroyandreopen(&options);
        assert_ok(put("0000000000000bar", "b"));
        assert_ok(put("1000000000000foo", "v1"));
        dbfull()->test_flushmemtable();

        assert_ok(put("1000000000000foo", "v2"));
        dbfull()->test_flushmemtable();
        assert_eq("v2", get("1000000000000foo"));

        assert_ok(put("0000000000000eee", "v3"));
        dbfull()->test_flushmemtable();
        assert_eq("v3", get("0000000000000eee"));

        assert_ok(delete("0000000000000bar"));
        dbfull()->test_flushmemtable();
        assert_eq("not_found", get("0000000000000bar"));

        assert_ok(put("0000000000000eee", "v5"));
        assert_ok(put("9000000000000eee", "v5"));
        dbfull()->test_flushmemtable();
        assert_eq("v5", get("0000000000000eee"));

        // test bloom filter
        if (bloom_bits > 0) {
          // neither key nor value should exist.
          expect_bloom_not_match = true;
          assert_eq("not_found", get("5_not00000000bar"));
          // key doesn't exist any more but prefix exists.
          if (total_order) {
            assert_eq("not_found", get("1000000000000not"));
            assert_eq("not_found", get("0000000000000not"));
          }
          expect_bloom_not_match = false;
        }
      }
      }
    }
    }
  }
}

test(plaintabledbtest, iterator) {
  for (size_t huge_page_tlb_size = 0; huge_page_tlb_size <= 2 * 1024 * 1024;
       huge_page_tlb_size += 2 * 1024 * 1024) {
    for (encodingtype encoding_type : {kplain, kprefix}) {
    for (int bloom_bits = 0; bloom_bits <= 117; bloom_bits += 117) {
      for (int total_order = 0; total_order <= 1; total_order++) {
        if (encoding_type == kprefix && total_order == 1) {
          continue;
        }
        bool expect_bloom_not_match = false;
        options options = currentoptions();
        options.create_if_missing = true;
        // set only one bucket to force bucket conflict.
        // test index interval for the same prefix to be 1, 2 and 4
        if (total_order) {
          options.prefix_extractor = nullptr;

          plaintableoptions plain_table_options;
          plain_table_options.user_key_len = 16;
          plain_table_options.bloom_bits_per_key = bloom_bits;
          plain_table_options.hash_table_ratio = 0;
          plain_table_options.index_sparseness = 2;
          plain_table_options.huge_page_tlb_size = huge_page_tlb_size;
          plain_table_options.encoding_type = encoding_type;

          options.table_factory.reset(new testplaintablefactory(
              &expect_bloom_not_match, plain_table_options));
        } else {
          plaintableoptions plain_table_options;
          plain_table_options.user_key_len = 16;
          plain_table_options.bloom_bits_per_key = bloom_bits;
          plain_table_options.hash_table_ratio = 0.75;
          plain_table_options.index_sparseness = 16;
          plain_table_options.huge_page_tlb_size = huge_page_tlb_size;
          plain_table_options.encoding_type = encoding_type;

          options.table_factory.reset(new testplaintablefactory(
              &expect_bloom_not_match, plain_table_options));
        }
        destroyandreopen(&options);

        assert_ok(put("1000000000foo002", "v_2"));
        assert_ok(put("0000000000000bar", "random"));
        assert_ok(put("1000000000foo001", "v1"));
        assert_ok(put("3000000000000bar", "bar_v"));
        assert_ok(put("1000000000foo003", "v__3"));
        assert_ok(put("1000000000foo004", "v__4"));
        assert_ok(put("1000000000foo005", "v__5"));
        assert_ok(put("1000000000foo007", "v__7"));
        assert_ok(put("1000000000foo008", "v__8"));
        dbfull()->test_flushmemtable();
        assert_eq("v1", get("1000000000foo001"));
        assert_eq("v__3", get("1000000000foo003"));
        iterator* iter = dbfull()->newiterator(readoptions());
        iter->seek("1000000000foo000");
        assert_true(iter->valid());
        assert_eq("1000000000foo001", iter->key().tostring());
        assert_eq("v1", iter->value().tostring());

        iter->next();
        assert_true(iter->valid());
        assert_eq("1000000000foo002", iter->key().tostring());
        assert_eq("v_2", iter->value().tostring());

        iter->next();
        assert_true(iter->valid());
        assert_eq("1000000000foo003", iter->key().tostring());
        assert_eq("v__3", iter->value().tostring());

        iter->next();
        assert_true(iter->valid());
        assert_eq("1000000000foo004", iter->key().tostring());
        assert_eq("v__4", iter->value().tostring());

        iter->seek("3000000000000bar");
        assert_true(iter->valid());
        assert_eq("3000000000000bar", iter->key().tostring());
        assert_eq("bar_v", iter->value().tostring());

        iter->seek("1000000000foo000");
        assert_true(iter->valid());
        assert_eq("1000000000foo001", iter->key().tostring());
        assert_eq("v1", iter->value().tostring());

        iter->seek("1000000000foo005");
        assert_true(iter->valid());
        assert_eq("1000000000foo005", iter->key().tostring());
        assert_eq("v__5", iter->value().tostring());

        iter->seek("1000000000foo006");
        assert_true(iter->valid());
        assert_eq("1000000000foo007", iter->key().tostring());
        assert_eq("v__7", iter->value().tostring());

        iter->seek("1000000000foo008");
        assert_true(iter->valid());
        assert_eq("1000000000foo008", iter->key().tostring());
        assert_eq("v__8", iter->value().tostring());

        if (total_order == 0) {
          iter->seek("1000000000foo009");
          assert_true(iter->valid());
          assert_eq("3000000000000bar", iter->key().tostring());
        }

        // test bloom filter
        if (bloom_bits > 0) {
          if (!total_order) {
            // neither key nor value should exist.
            expect_bloom_not_match = true;
            iter->seek("2not000000000bar");
            assert_true(!iter->valid());
            assert_eq("not_found", get("2not000000000bar"));
            expect_bloom_not_match = false;
          } else {
            expect_bloom_not_match = true;
            assert_eq("not_found", get("2not000000000bar"));
            expect_bloom_not_match = false;
          }
        }

        delete iter;
      }
    }
    }
  }
}

namespace {
std::string makelongkey(size_t length, char c) {
  return std::string(length, c);
}
}  // namespace

test(plaintabledbtest, iteratorlargekeys) {
  options options = currentoptions();

  plaintableoptions plain_table_options;
  plain_table_options.user_key_len = 0;
  plain_table_options.bloom_bits_per_key = 0;
  plain_table_options.hash_table_ratio = 0;

  options.table_factory.reset(newplaintablefactory(plain_table_options));
  options.create_if_missing = true;
  options.prefix_extractor.reset();
  destroyandreopen(&options);

  std::string key_list[] = {
      makelongkey(30, '0'),
      makelongkey(16, '1'),
      makelongkey(32, '2'),
      makelongkey(60, '3'),
      makelongkey(90, '4'),
      makelongkey(50, '5'),
      makelongkey(26, '6')
  };

  for (size_t i = 0; i < 7; i++) {
    assert_ok(put(key_list[i], std::to_string(i)));
  }

  dbfull()->test_flushmemtable();

  iterator* iter = dbfull()->newiterator(readoptions());
  iter->seek(key_list[0]);

  for (size_t i = 0; i < 7; i++) {
    assert_true(iter->valid());
    assert_eq(key_list[i], iter->key().tostring());
    assert_eq(std::to_string(i), iter->value().tostring());
    iter->next();
  }

  assert_true(!iter->valid());

  delete iter;
}

namespace {
std::string makelongkeywithprefix(size_t length, char c) {
  return "00000000" + std::string(length - 8, c);
}
}  // namespace

test(plaintabledbtest, iteratorlargekeyswithprefix) {
  options options = currentoptions();

  plaintableoptions plain_table_options;
  plain_table_options.user_key_len = 16;
  plain_table_options.bloom_bits_per_key = 0;
  plain_table_options.hash_table_ratio = 0.8;
  plain_table_options.index_sparseness = 3;
  plain_table_options.huge_page_tlb_size = 0;
  plain_table_options.encoding_type = kprefix;

  options.table_factory.reset(newplaintablefactory(plain_table_options));
  options.create_if_missing = true;
  destroyandreopen(&options);

  std::string key_list[] = {
      makelongkeywithprefix(30, '0'), makelongkeywithprefix(16, '1'),
      makelongkeywithprefix(32, '2'), makelongkeywithprefix(60, '3'),
      makelongkeywithprefix(90, '4'), makelongkeywithprefix(50, '5'),
      makelongkeywithprefix(26, '6')};

  for (size_t i = 0; i < 7; i++) {
    assert_ok(put(key_list[i], std::to_string(i)));
  }

  dbfull()->test_flushmemtable();

  iterator* iter = dbfull()->newiterator(readoptions());
  iter->seek(key_list[0]);

  for (size_t i = 0; i < 7; i++) {
    assert_true(iter->valid());
    assert_eq(key_list[i], iter->key().tostring());
    assert_eq(std::to_string(i), iter->value().tostring());
    iter->next();
  }

  assert_true(!iter->valid());

  delete iter;
}

// a test comparator which compare two strings in this way:
// (1) first compare prefix of 8 bytes in alphabet order,
// (2) if two strings share the same prefix, sort the other part of the string
//     in the reverse alphabet order.
class simplesuffixreversecomparator : public comparator {
 public:
  simplesuffixreversecomparator() {}

  virtual const char* name() const { return "simplesuffixreversecomparator"; }

  virtual int compare(const slice& a, const slice& b) const {
    slice prefix_a = slice(a.data(), 8);
    slice prefix_b = slice(b.data(), 8);
    int prefix_comp = prefix_a.compare(prefix_b);
    if (prefix_comp != 0) {
      return prefix_comp;
    } else {
      slice suffix_a = slice(a.data() + 8, a.size() - 8);
      slice suffix_b = slice(b.data() + 8, b.size() - 8);
      return -(suffix_a.compare(suffix_b));
    }
  }
  virtual void findshortestseparator(std::string* start,
                                     const slice& limit) const {}

  virtual void findshortsuccessor(std::string* key) const {}
};

test(plaintabledbtest, iteratorreversesuffixcomparator) {
  options options = currentoptions();
  options.create_if_missing = true;
  // set only one bucket to force bucket conflict.
  // test index interval for the same prefix to be 1, 2 and 4
  simplesuffixreversecomparator comp;
  options.comparator = &comp;
  destroyandreopen(&options);

  assert_ok(put("1000000000foo002", "v_2"));
  assert_ok(put("0000000000000bar", "random"));
  assert_ok(put("1000000000foo001", "v1"));
  assert_ok(put("3000000000000bar", "bar_v"));
  assert_ok(put("1000000000foo003", "v__3"));
  assert_ok(put("1000000000foo004", "v__4"));
  assert_ok(put("1000000000foo005", "v__5"));
  assert_ok(put("1000000000foo007", "v__7"));
  assert_ok(put("1000000000foo008", "v__8"));
  dbfull()->test_flushmemtable();
  assert_eq("v1", get("1000000000foo001"));
  assert_eq("v__3", get("1000000000foo003"));
  iterator* iter = dbfull()->newiterator(readoptions());
  iter->seek("1000000000foo009");
  assert_true(iter->valid());
  assert_eq("1000000000foo008", iter->key().tostring());
  assert_eq("v__8", iter->value().tostring());

  iter->next();
  assert_true(iter->valid());
  assert_eq("1000000000foo007", iter->key().tostring());
  assert_eq("v__7", iter->value().tostring());

  iter->next();
  assert_true(iter->valid());
  assert_eq("1000000000foo005", iter->key().tostring());
  assert_eq("v__5", iter->value().tostring());

  iter->next();
  assert_true(iter->valid());
  assert_eq("1000000000foo004", iter->key().tostring());
  assert_eq("v__4", iter->value().tostring());

  iter->seek("3000000000000bar");
  assert_true(iter->valid());
  assert_eq("3000000000000bar", iter->key().tostring());
  assert_eq("bar_v", iter->value().tostring());

  iter->seek("1000000000foo005");
  assert_true(iter->valid());
  assert_eq("1000000000foo005", iter->key().tostring());
  assert_eq("v__5", iter->value().tostring());

  iter->seek("1000000000foo006");
  assert_true(iter->valid());
  assert_eq("1000000000foo005", iter->key().tostring());
  assert_eq("v__5", iter->value().tostring());

  iter->seek("1000000000foo008");
  assert_true(iter->valid());
  assert_eq("1000000000foo008", iter->key().tostring());
  assert_eq("v__8", iter->value().tostring());

  iter->seek("1000000000foo000");
  assert_true(iter->valid());
  assert_eq("3000000000000bar", iter->key().tostring());

  delete iter;
}

test(plaintabledbtest, hashbucketconflict) {
  for (size_t huge_page_tlb_size = 0; huge_page_tlb_size <= 2 * 1024 * 1024;
       huge_page_tlb_size += 2 * 1024 * 1024) {
    for (unsigned char i = 1; i <= 3; i++) {
      options options = currentoptions();
      options.create_if_missing = true;
      // set only one bucket to force bucket conflict.
      // test index interval for the same prefix to be 1, 2 and 4

      plaintableoptions plain_table_options;
      plain_table_options.user_key_len = 16;
      plain_table_options.bloom_bits_per_key = 0;
      plain_table_options.hash_table_ratio = 0;
      plain_table_options.index_sparseness = 2 ^ i;
      plain_table_options.huge_page_tlb_size = huge_page_tlb_size;

      options.table_factory.reset(newplaintablefactory(plain_table_options));

      destroyandreopen(&options);
      assert_ok(put("5000000000000fo0", "v1"));
      assert_ok(put("5000000000000fo1", "v2"));
      assert_ok(put("5000000000000fo2", "v"));
      assert_ok(put("2000000000000fo0", "v3"));
      assert_ok(put("2000000000000fo1", "v4"));
      assert_ok(put("2000000000000fo2", "v"));
      assert_ok(put("2000000000000fo3", "v"));

      dbfull()->test_flushmemtable();

      assert_eq("v1", get("5000000000000fo0"));
      assert_eq("v2", get("5000000000000fo1"));
      assert_eq("v3", get("2000000000000fo0"));
      assert_eq("v4", get("2000000000000fo1"));

      assert_eq("not_found", get("5000000000000bar"));
      assert_eq("not_found", get("2000000000000bar"));
      assert_eq("not_found", get("5000000000000fo8"));
      assert_eq("not_found", get("2000000000000fo8"));

      readoptions ro;
      iterator* iter = dbfull()->newiterator(ro);

      iter->seek("5000000000000fo0");
      assert_true(iter->valid());
      assert_eq("5000000000000fo0", iter->key().tostring());
      iter->next();
      assert_true(iter->valid());
      assert_eq("5000000000000fo1", iter->key().tostring());

      iter->seek("5000000000000fo1");
      assert_true(iter->valid());
      assert_eq("5000000000000fo1", iter->key().tostring());

      iter->seek("2000000000000fo0");
      assert_true(iter->valid());
      assert_eq("2000000000000fo0", iter->key().tostring());
      iter->next();
      assert_true(iter->valid());
      assert_eq("2000000000000fo1", iter->key().tostring());

      iter->seek("2000000000000fo1");
      assert_true(iter->valid());
      assert_eq("2000000000000fo1", iter->key().tostring());

      iter->seek("2000000000000bar");
      assert_true(iter->valid());
      assert_eq("2000000000000fo0", iter->key().tostring());

      iter->seek("5000000000000bar");
      assert_true(iter->valid());
      assert_eq("5000000000000fo0", iter->key().tostring());

      iter->seek("2000000000000fo8");
      assert_true(!iter->valid() ||
                  options.comparator->compare(iter->key(), "20000001") > 0);

      iter->seek("5000000000000fo8");
      assert_true(!iter->valid());

      iter->seek("1000000000000fo2");
      assert_true(!iter->valid());

      iter->seek("3000000000000fo2");
      assert_true(!iter->valid());

      iter->seek("8000000000000fo2");
      assert_true(!iter->valid());

      delete iter;
    }
  }
}

test(plaintabledbtest, hashbucketconflictreversesuffixcomparator) {
  for (size_t huge_page_tlb_size = 0; huge_page_tlb_size <= 2 * 1024 * 1024;
       huge_page_tlb_size += 2 * 1024 * 1024) {
    for (unsigned char i = 1; i <= 3; i++) {
      options options = currentoptions();
      options.create_if_missing = true;
      simplesuffixreversecomparator comp;
      options.comparator = &comp;
      // set only one bucket to force bucket conflict.
      // test index interval for the same prefix to be 1, 2 and 4

      plaintableoptions plain_table_options;
      plain_table_options.user_key_len = 16;
      plain_table_options.bloom_bits_per_key = 0;
      plain_table_options.hash_table_ratio = 0;
      plain_table_options.index_sparseness = 2 ^ i;
      plain_table_options.huge_page_tlb_size = huge_page_tlb_size;

      options.table_factory.reset(newplaintablefactory(plain_table_options));
      destroyandreopen(&options);
      assert_ok(put("5000000000000fo0", "v1"));
      assert_ok(put("5000000000000fo1", "v2"));
      assert_ok(put("5000000000000fo2", "v"));
      assert_ok(put("2000000000000fo0", "v3"));
      assert_ok(put("2000000000000fo1", "v4"));
      assert_ok(put("2000000000000fo2", "v"));
      assert_ok(put("2000000000000fo3", "v"));

      dbfull()->test_flushmemtable();

      assert_eq("v1", get("5000000000000fo0"));
      assert_eq("v2", get("5000000000000fo1"));
      assert_eq("v3", get("2000000000000fo0"));
      assert_eq("v4", get("2000000000000fo1"));

      assert_eq("not_found", get("5000000000000bar"));
      assert_eq("not_found", get("2000000000000bar"));
      assert_eq("not_found", get("5000000000000fo8"));
      assert_eq("not_found", get("2000000000000fo8"));

      readoptions ro;
      iterator* iter = dbfull()->newiterator(ro);

      iter->seek("5000000000000fo1");
      assert_true(iter->valid());
      assert_eq("5000000000000fo1", iter->key().tostring());
      iter->next();
      assert_true(iter->valid());
      assert_eq("5000000000000fo0", iter->key().tostring());

      iter->seek("5000000000000fo1");
      assert_true(iter->valid());
      assert_eq("5000000000000fo1", iter->key().tostring());

      iter->seek("2000000000000fo1");
      assert_true(iter->valid());
      assert_eq("2000000000000fo1", iter->key().tostring());
      iter->next();
      assert_true(iter->valid());
      assert_eq("2000000000000fo0", iter->key().tostring());

      iter->seek("2000000000000fo1");
      assert_true(iter->valid());
      assert_eq("2000000000000fo1", iter->key().tostring());

      iter->seek("2000000000000var");
      assert_true(iter->valid());
      assert_eq("2000000000000fo3", iter->key().tostring());

      iter->seek("5000000000000var");
      assert_true(iter->valid());
      assert_eq("5000000000000fo2", iter->key().tostring());

      std::string seek_key = "2000000000000bar";
      iter->seek(seek_key);
      assert_true(!iter->valid() ||
                  options.prefix_extractor->transform(iter->key()) !=
                      options.prefix_extractor->transform(seek_key));

      iter->seek("1000000000000fo2");
      assert_true(!iter->valid());

      iter->seek("3000000000000fo2");
      assert_true(!iter->valid());

      iter->seek("8000000000000fo2");
      assert_true(!iter->valid());

      delete iter;
    }
  }
}

test(plaintabledbtest, nonexistingkeytononemptybucket) {
  options options = currentoptions();
  options.create_if_missing = true;
  // set only one bucket to force bucket conflict.
  // test index interval for the same prefix to be 1, 2 and 4
  plaintableoptions plain_table_options;
  plain_table_options.user_key_len = 16;
  plain_table_options.bloom_bits_per_key = 0;
  plain_table_options.hash_table_ratio = 0;
  plain_table_options.index_sparseness = 5;

  options.table_factory.reset(newplaintablefactory(plain_table_options));
  destroyandreopen(&options);
  assert_ok(put("5000000000000fo0", "v1"));
  assert_ok(put("5000000000000fo1", "v2"));
  assert_ok(put("5000000000000fo2", "v3"));

  dbfull()->test_flushmemtable();

  assert_eq("v1", get("5000000000000fo0"));
  assert_eq("v2", get("5000000000000fo1"));
  assert_eq("v3", get("5000000000000fo2"));

  assert_eq("not_found", get("8000000000000bar"));
  assert_eq("not_found", get("1000000000000bar"));

  iterator* iter = dbfull()->newiterator(readoptions());

  iter->seek("5000000000000bar");
  assert_true(iter->valid());
  assert_eq("5000000000000fo0", iter->key().tostring());

  iter->seek("5000000000000fo8");
  assert_true(!iter->valid());

  iter->seek("1000000000000fo2");
  assert_true(!iter->valid());

  iter->seek("8000000000000fo2");
  assert_true(!iter->valid());

  delete iter;
}

static std::string key(int i) {
  char buf[100];
  snprintf(buf, sizeof(buf), "key_______%06d", i);
  return std::string(buf);
}

static std::string randomstring(random* rnd, int len) {
  std::string r;
  test::randomstring(rnd, len, &r);
  return r;
}

test(plaintabledbtest, compactiontrigger) {
  options options = currentoptions();
  options.write_buffer_size = 100 << 10; //100kb
  options.num_levels = 3;
  options.max_mem_compaction_level = 0;
  options.level0_file_num_compaction_trigger = 3;
  reopen(&options);

  random rnd(301);

  for (int num = 0; num < options.level0_file_num_compaction_trigger - 1;
      num++) {
    std::vector<std::string> values;
    // write 120kb (12 values, each 10k)
    for (int i = 0; i < 12; i++) {
      values.push_back(randomstring(&rnd, 10000));
      assert_ok(put(key(i), values[i]));
    }
    dbfull()->test_waitforflushmemtable();
    assert_eq(numtablefilesatlevel(0), num + 1);
  }

  //generate one more file in level-0, and should trigger level-0 compaction
  std::vector<std::string> values;
  for (int i = 0; i < 12; i++) {
    values.push_back(randomstring(&rnd, 10000));
    assert_ok(put(key(i), values[i]));
  }
  dbfull()->test_waitforcompact();

  assert_eq(numtablefilesatlevel(0), 0);
  assert_eq(numtablefilesatlevel(1), 1);
}

test(plaintabledbtest, adaptivetable) {
  options options = currentoptions();
  options.create_if_missing = true;

  options.table_factory.reset(newplaintablefactory());
  destroyandreopen(&options);

  assert_ok(put("1000000000000foo", "v1"));
  assert_ok(put("0000000000000bar", "v2"));
  assert_ok(put("1000000000000foo", "v3"));
  dbfull()->test_flushmemtable();

  options.create_if_missing = false;
  std::shared_ptr<tablefactory> dummy_factory;
  std::shared_ptr<tablefactory> block_based_factory(
      newblockbasedtablefactory());
  options.table_factory.reset(newadaptivetablefactory(
      block_based_factory, dummy_factory, dummy_factory));
  reopen(&options);
  assert_eq("v3", get("1000000000000foo"));
  assert_eq("v2", get("0000000000000bar"));

  assert_ok(put("2000000000000foo", "v4"));
  assert_ok(put("3000000000000bar", "v5"));
  dbfull()->test_flushmemtable();
  assert_eq("v4", get("2000000000000foo"));
  assert_eq("v5", get("3000000000000bar"));

  reopen(&options);
  assert_eq("v3", get("1000000000000foo"));
  assert_eq("v2", get("0000000000000bar"));
  assert_eq("v4", get("2000000000000foo"));
  assert_eq("v5", get("3000000000000bar"));

  options.table_factory.reset(newblockbasedtablefactory());
  reopen(&options);
  assert_ne("v3", get("1000000000000foo"));

  options.table_factory.reset(newplaintablefactory());
  reopen(&options);
  assert_ne("v5", get("3000000000000bar"));
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
