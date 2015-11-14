//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <inttypes.h>
#include <stdio.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <memory>
#include <vector>

#include "db/dbformat.h"
#include "db/memtable.h"
#include "db/write_batch_internal.h"

#include "rocksdb/cache.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/iterator.h"
#include "rocksdb/memtablerep.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/statistics.h"

#include "table/block.h"
#include "table/block_based_table_builder.h"
#include "table/block_based_table_factory.h"
#include "table/block_based_table_reader.h"
#include "table/block_builder.h"
#include "table/format.h"
#include "table/meta_blocks.h"
#include "table/plain_table_factory.h"

#include "util/random.h"
#include "util/statistics.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

extern const uint64_t klegacyblockbasedtablemagicnumber;
extern const uint64_t klegacyplaintablemagicnumber;
extern const uint64_t kblockbasedtablemagicnumber;
extern const uint64_t kplaintablemagicnumber;

namespace {

// return reverse of "key".
// used to test non-lexicographic comparators.
std::string reverse(const slice& key) {
  auto rev = key.tostring();
  std::reverse(rev.begin(), rev.end());
  return rev;
}

class reversekeycomparator : public comparator {
 public:
  virtual const char* name() const {
    return "rocksdb.reversebytewisecomparator";
  }

  virtual int compare(const slice& a, const slice& b) const {
    return bytewisecomparator()->compare(reverse(a), reverse(b));
  }

  virtual void findshortestseparator(
      std::string* start,
      const slice& limit) const {
    std::string s = reverse(*start);
    std::string l = reverse(limit);
    bytewisecomparator()->findshortestseparator(&s, l);
    *start = reverse(s);
  }

  virtual void findshortsuccessor(std::string* key) const {
    std::string s = reverse(*key);
    bytewisecomparator()->findshortsuccessor(&s);
    *key = reverse(s);
  }
};

reversekeycomparator reverse_key_comparator;

void increment(const comparator* cmp, std::string* key) {
  if (cmp == bytewisecomparator()) {
    key->push_back('\0');
  } else {
    assert(cmp == &reverse_key_comparator);
    std::string rev = reverse(*key);
    rev.push_back('\0');
    *key = reverse(rev);
  }
}

// an stl comparator that uses a comparator
struct stllessthan {
  const comparator* cmp;

  stllessthan() : cmp(bytewisecomparator()) { }
  explicit stllessthan(const comparator* c) : cmp(c) { }
  bool operator()(const std::string& a, const std::string& b) const {
    return cmp->compare(slice(a), slice(b)) < 0;
  }
};

}  // namespace

class stringsink: public writablefile {
 public:
  ~stringsink() { }

  const std::string& contents() const { return contents_; }

  virtual status close() { return status::ok(); }
  virtual status flush() { return status::ok(); }
  virtual status sync() { return status::ok(); }

  virtual status append(const slice& data) {
    contents_.append(data.data(), data.size());
    return status::ok();
  }

 private:
  std::string contents_;
};


class stringsource: public randomaccessfile {
 public:
  stringsource(const slice& contents, uint64_t uniq_id, bool mmap)
      : contents_(contents.data(), contents.size()), uniq_id_(uniq_id),
        mmap_(mmap) {
  }

  virtual ~stringsource() { }

  uint64_t size() const { return contents_.size(); }

  virtual status read(uint64_t offset, size_t n, slice* result,
                       char* scratch) const {
    if (offset > contents_.size()) {
      return status::invalidargument("invalid read offset");
    }
    if (offset + n > contents_.size()) {
      n = contents_.size() - offset;
    }
    if (!mmap_) {
      memcpy(scratch, &contents_[offset], n);
      *result = slice(scratch, n);
    } else {
      *result = slice(&contents_[offset], n);
    }
    return status::ok();
  }

  virtual size_t getuniqueid(char* id, size_t max_size) const {
    if (max_size < 20) {
      return 0;
    }

    char* rid = id;
    rid = encodevarint64(rid, uniq_id_);
    rid = encodevarint64(rid, 0);
    return static_cast<size_t>(rid-id);
  }

 private:
  std::string contents_;
  uint64_t uniq_id_;
  bool mmap_;
};

typedef std::map<std::string, std::string, stllessthan> kvmap;

// helper class for tests to unify the interface between
// blockbuilder/tablebuilder and block/table.
class constructor {
 public:
  explicit constructor(const comparator* cmp) : data_(stllessthan(cmp)) {}
  virtual ~constructor() { }

  void add(const std::string& key, const slice& value) {
    data_[key] = value.tostring();
  }

  // finish constructing the data structure with all the keys that have
  // been added so far.  returns the keys in sorted order in "*keys"
  // and stores the key/value pairs in "*kvmap"
  void finish(const options& options,
              const blockbasedtableoptions& table_options,
              const internalkeycomparator& internal_comparator,
              std::vector<std::string>* keys, kvmap* kvmap) {
    last_internal_key_ = &internal_comparator;
    *kvmap = data_;
    keys->clear();
    for (kvmap::const_iterator it = data_.begin();
         it != data_.end();
         ++it) {
      keys->push_back(it->first);
    }
    data_.clear();
    status s = finishimpl(options, table_options, internal_comparator, *kvmap);
    assert_true(s.ok()) << s.tostring();
  }

  // construct the data structure from the data in "data"
  virtual status finishimpl(const options& options,
                            const blockbasedtableoptions& table_options,
                            const internalkeycomparator& internal_comparator,
                            const kvmap& data) = 0;

  virtual iterator* newiterator() const = 0;

  virtual const kvmap& data() { return data_; }

  virtual db* db() const { return nullptr; }  // overridden in dbconstructor

 protected:
  const internalkeycomparator* last_internal_key_;

 private:
  kvmap data_;
};

class blockconstructor: public constructor {
 public:
  explicit blockconstructor(const comparator* cmp)
      : constructor(cmp),
        comparator_(cmp),
        block_(nullptr) { }
  ~blockconstructor() {
    delete block_;
  }
  virtual status finishimpl(const options& options,
                            const blockbasedtableoptions& table_options,
                            const internalkeycomparator& internal_comparator,
                            const kvmap& data) {
    delete block_;
    block_ = nullptr;
    blockbuilder builder(table_options.block_restart_interval);

    for (kvmap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      builder.add(it->first, it->second);
    }
    // open the block
    data_ = builder.finish().tostring();
    blockcontents contents;
    contents.data = data_;
    contents.cachable = false;
    contents.heap_allocated = false;
    block_ = new block(contents);
    return status::ok();
  }
  virtual iterator* newiterator() const {
    return block_->newiterator(comparator_);
  }

 private:
  const comparator* comparator_;
  std::string data_;
  block* block_;

  blockconstructor();
};

// a helper class that converts internal format keys into user keys
class keyconvertingiterator: public iterator {
 public:
  explicit keyconvertingiterator(iterator* iter) : iter_(iter) { }
  virtual ~keyconvertingiterator() { delete iter_; }
  virtual bool valid() const { return iter_->valid(); }
  virtual void seek(const slice& target) {
    parsedinternalkey ikey(target, kmaxsequencenumber, ktypevalue);
    std::string encoded;
    appendinternalkey(&encoded, ikey);
    iter_->seek(encoded);
  }
  virtual void seektofirst() { iter_->seektofirst(); }
  virtual void seektolast() { iter_->seektolast(); }
  virtual void next() { iter_->next(); }
  virtual void prev() { iter_->prev(); }

  virtual slice key() const {
    assert(valid());
    parsedinternalkey key;
    if (!parseinternalkey(iter_->key(), &key)) {
      status_ = status::corruption("malformed internal key");
      return slice("corrupted key");
    }
    return key.user_key;
  }

  virtual slice value() const { return iter_->value(); }
  virtual status status() const {
    return status_.ok() ? iter_->status() : status_;
  }

 private:
  mutable status status_;
  iterator* iter_;

  // no copying allowed
  keyconvertingiterator(const keyconvertingiterator&);
  void operator=(const keyconvertingiterator&);
};

class tableconstructor: public constructor {
 public:
  explicit tableconstructor(const comparator* cmp,
                            bool convert_to_internal_key = false)
      : constructor(cmp),
        convert_to_internal_key_(convert_to_internal_key) {}
  ~tableconstructor() { reset(); }

  virtual status finishimpl(const options& options,
                            const blockbasedtableoptions& table_options,
                            const internalkeycomparator& internal_comparator,
                            const kvmap& data) {
    reset();
    sink_.reset(new stringsink());
    unique_ptr<tablebuilder> builder;
    builder.reset(options.table_factory->newtablebuilder(
        options, internal_comparator, sink_.get(), options.compression));

    for (kvmap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      if (convert_to_internal_key_) {
        parsedinternalkey ikey(it->first, kmaxsequencenumber, ktypevalue);
        std::string encoded;
        appendinternalkey(&encoded, ikey);
        builder->add(encoded, it->second);
      } else {
        builder->add(it->first, it->second);
      }
      assert_true(builder->status().ok());
    }
    status s = builder->finish();
    assert_true(s.ok()) << s.tostring();

    assert_eq(sink_->contents().size(), builder->filesize());

    // open the table
    uniq_id_ = cur_uniq_id_++;
    source_.reset(new stringsource(sink_->contents(), uniq_id_,
                                   options.allow_mmap_reads));
    return options.table_factory->newtablereader(
        options, soptions, internal_comparator, std::move(source_),
        sink_->contents().size(), &table_reader_);
  }

  virtual iterator* newiterator() const {
    readoptions ro;
    iterator* iter = table_reader_->newiterator(ro);
    if (convert_to_internal_key_) {
      return new keyconvertingiterator(iter);
    } else {
      return iter;
    }
  }

  uint64_t approximateoffsetof(const slice& key) const {
    return table_reader_->approximateoffsetof(key);
  }

  virtual status reopen(const options& options) {
    source_.reset(
        new stringsource(sink_->contents(), uniq_id_,
                         options.allow_mmap_reads));
    return options.table_factory->newtablereader(
        options, soptions, *last_internal_key_, std::move(source_),
        sink_->contents().size(), &table_reader_);
  }

  virtual tablereader* gettablereader() {
    return table_reader_.get();
  }

 private:
  void reset() {
    uniq_id_ = 0;
    table_reader_.reset();
    sink_.reset();
    source_.reset();
  }
  bool convert_to_internal_key_;

  uint64_t uniq_id_;
  unique_ptr<stringsink> sink_;
  unique_ptr<stringsource> source_;
  unique_ptr<tablereader> table_reader_;

  tableconstructor();

  static uint64_t cur_uniq_id_;
  const envoptions soptions;
};
uint64_t tableconstructor::cur_uniq_id_ = 1;

class memtableconstructor: public constructor {
 public:
  explicit memtableconstructor(const comparator* cmp)
      : constructor(cmp),
        internal_comparator_(cmp),
        table_factory_(new skiplistfactory) {
    options options;
    options.memtable_factory = table_factory_;
    memtable_ = new memtable(internal_comparator_, options);
    memtable_->ref();
  }
  ~memtableconstructor() {
    delete memtable_->unref();
  }
  virtual status finishimpl(const options& options,
                            const blockbasedtableoptions& table_options,
                            const internalkeycomparator& internal_comparator,
                            const kvmap& data) {
    delete memtable_->unref();
    options memtable_options;
    memtable_options.memtable_factory = table_factory_;
    memtable_ = new memtable(internal_comparator_, memtable_options);
    memtable_->ref();
    int seq = 1;
    for (kvmap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      memtable_->add(seq, ktypevalue, it->first, it->second);
      seq++;
    }
    return status::ok();
  }
  virtual iterator* newiterator() const {
    return new keyconvertingiterator(memtable_->newiterator(readoptions()));
  }

 private:
  internalkeycomparator internal_comparator_;
  memtable* memtable_;
  std::shared_ptr<skiplistfactory> table_factory_;
};

class dbconstructor: public constructor {
 public:
  explicit dbconstructor(const comparator* cmp)
      : constructor(cmp),
        comparator_(cmp) {
    db_ = nullptr;
    newdb();
  }
  ~dbconstructor() {
    delete db_;
  }
  virtual status finishimpl(const options& options,
                            const blockbasedtableoptions& table_options,
                            const internalkeycomparator& internal_comparator,
                            const kvmap& data) {
    delete db_;
    db_ = nullptr;
    newdb();
    for (kvmap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      writebatch batch;
      batch.put(it->first, it->second);
      assert_true(db_->write(writeoptions(), &batch).ok());
    }
    return status::ok();
  }
  virtual iterator* newiterator() const {
    return db_->newiterator(readoptions());
  }

  virtual db* db() const { return db_; }

 private:
  void newdb() {
    std::string name = test::tmpdir() + "/table_testdb";

    options options;
    options.comparator = comparator_;
    status status = destroydb(name, options);
    assert_true(status.ok()) << status.tostring();

    options.create_if_missing = true;
    options.error_if_exists = true;
    options.write_buffer_size = 10000;  // something small to force merging
    status = db::open(options, name, &db_);
    assert_true(status.ok()) << status.tostring();
  }

  const comparator* comparator_;
  db* db_;
};

static bool snappycompressionsupported() {
#ifdef snappy
  std::string out;
  slice in = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  return port::snappy_compress(options().compression_opts,
                               in.data(), in.size(),
                               &out);
#else
  return false;
#endif
}

static bool zlibcompressionsupported() {
#ifdef zlib
  std::string out;
  slice in = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  return port::zlib_compress(options().compression_opts,
                             in.data(), in.size(),
                             &out);
#else
  return false;
#endif
}

static bool bzip2compressionsupported() {
#ifdef bzip2
  std::string out;
  slice in = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  return port::bzip2_compress(options().compression_opts,
                              in.data(), in.size(),
                              &out);
#else
  return false;
#endif
}

static bool lz4compressionsupported() {
#ifdef lz4
  std::string out;
  slice in = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  return port::lz4_compress(options().compression_opts, in.data(), in.size(),
                            &out);
#else
  return false;
#endif
}

static bool lz4hccompressionsupported() {
#ifdef lz4
  std::string out;
  slice in = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  return port::lz4hc_compress(options().compression_opts, in.data(), in.size(),
                              &out);
#else
  return false;
#endif
}

enum testtype {
  block_based_table_test,
  plain_table_semi_fixed_prefix,
  plain_table_full_str_prefix,
  plain_table_total_order,
  block_test,
  memtable_test,
  db_test
};

struct testargs {
  testtype type;
  bool reverse_compare;
  int restart_interval;
  compressiontype compression;
};

static std::vector<testargs> generatearglist() {
  std::vector<testargs> test_args;
  std::vector<testtype> test_types = {
      block_based_table_test,      plain_table_semi_fixed_prefix,
      plain_table_full_str_prefix, plain_table_total_order,
      block_test,                  memtable_test,
      db_test};
  std::vector<bool> reverse_compare_types = {false, true};
  std::vector<int> restart_intervals = {16, 1, 1024};

  // only add compression if it is supported
  std::vector<compressiontype> compression_types;
  compression_types.push_back(knocompression);
  if (snappycompressionsupported()) {
    compression_types.push_back(ksnappycompression);
  }
  if (zlibcompressionsupported()) {
    compression_types.push_back(kzlibcompression);
  }
  if (bzip2compressionsupported()) {
    compression_types.push_back(kbzip2compression);
  }
  if (lz4compressionsupported()) {
    compression_types.push_back(klz4compression);
  }
  if (lz4hccompressionsupported()) {
    compression_types.push_back(klz4hccompression);
  }

  for (auto test_type : test_types) {
    for (auto reverse_compare : reverse_compare_types) {
      if (test_type == plain_table_semi_fixed_prefix ||
          test_type == plain_table_full_str_prefix) {
        // plain table doesn't use restart index or compression.
        testargs one_arg;
        one_arg.type = test_type;
        one_arg.reverse_compare = reverse_compare;
        one_arg.restart_interval = restart_intervals[0];
        one_arg.compression = compression_types[0];
        test_args.push_back(one_arg);
        continue;
      }

      for (auto restart_interval : restart_intervals) {
        for (auto compression_type : compression_types) {
          testargs one_arg;
          one_arg.type = test_type;
          one_arg.reverse_compare = reverse_compare;
          one_arg.restart_interval = restart_interval;
          one_arg.compression = compression_type;
          test_args.push_back(one_arg);
        }
      }
    }
  }
  return test_args;
}

// in order to make all tests run for plain table format, including
// those operating on empty keys, create a new prefix transformer which
// return fixed prefix if the slice is not shorter than the prefix length,
// and the full slice if it is shorter.
class fixedorlessprefixtransform : public slicetransform {
 private:
  const size_t prefix_len_;

 public:
  explicit fixedorlessprefixtransform(size_t prefix_len) :
      prefix_len_(prefix_len) {
  }

  virtual const char* name() const {
    return "rocksdb.fixedprefix";
  }

  virtual slice transform(const slice& src) const {
    assert(indomain(src));
    if (src.size() < prefix_len_) {
      return src;
    }
    return slice(src.data(), prefix_len_);
  }

  virtual bool indomain(const slice& src) const {
    return true;
  }

  virtual bool inrange(const slice& dst) const {
    return (dst.size() <= prefix_len_);
  }
};

class harness {
 public:
  harness() : constructor_(nullptr) { }

  void init(const testargs& args) {
    delete constructor_;
    constructor_ = nullptr;
    options_ = options();
    options_.compression = args.compression;
    // use shorter block size for tests to exercise block boundary
    // conditions more.
    if (args.reverse_compare) {
      options_.comparator = &reverse_key_comparator;
    }

    internal_comparator_.reset(
        new test::plaininternalkeycomparator(options_.comparator));

    support_prev_ = true;
    only_support_prefix_seek_ = false;
    switch (args.type) {
      case block_based_table_test:
        table_options_.flush_block_policy_factory.reset(
            new flushblockbysizepolicyfactory());
        table_options_.block_size = 256;
        table_options_.block_restart_interval = args.restart_interval;
        options_.table_factory.reset(
            new blockbasedtablefactory(table_options_));
        constructor_ = new tableconstructor(options_.comparator);
        break;
      case plain_table_semi_fixed_prefix:
        support_prev_ = false;
        only_support_prefix_seek_ = true;
        options_.prefix_extractor.reset(new fixedorlessprefixtransform(2));
        options_.allow_mmap_reads = true;
        options_.table_factory.reset(newplaintablefactory());
        constructor_ = new tableconstructor(options_.comparator, true);
        internal_comparator_.reset(
            new internalkeycomparator(options_.comparator));
        break;
      case plain_table_full_str_prefix:
        support_prev_ = false;
        only_support_prefix_seek_ = true;
        options_.prefix_extractor.reset(newnooptransform());
        options_.allow_mmap_reads = true;
        options_.table_factory.reset(newplaintablefactory());
        constructor_ = new tableconstructor(options_.comparator, true);
        internal_comparator_.reset(
            new internalkeycomparator(options_.comparator));
        break;
      case plain_table_total_order:
        support_prev_ = false;
        only_support_prefix_seek_ = false;
        options_.prefix_extractor = nullptr;
        options_.allow_mmap_reads = true;

        {
          plaintableoptions plain_table_options;
          plain_table_options.user_key_len = kplaintablevariablelength;
          plain_table_options.bloom_bits_per_key = 0;
          plain_table_options.hash_table_ratio = 0;

          options_.table_factory.reset(
              newplaintablefactory(plain_table_options));
        }
        constructor_ = new tableconstructor(options_.comparator, true);
        internal_comparator_.reset(
            new internalkeycomparator(options_.comparator));
        break;
      case block_test:
        table_options_.block_size = 256;
        options_.table_factory.reset(
            new blockbasedtablefactory(table_options_));
        constructor_ = new blockconstructor(options_.comparator);
        break;
      case memtable_test:
        table_options_.block_size = 256;
        options_.table_factory.reset(
            new blockbasedtablefactory(table_options_));
        constructor_ = new memtableconstructor(options_.comparator);
        break;
      case db_test:
        table_options_.block_size = 256;
        options_.table_factory.reset(
            new blockbasedtablefactory(table_options_));
        constructor_ = new dbconstructor(options_.comparator);
        break;
    }
  }

  ~harness() {
    delete constructor_;
  }

  void add(const std::string& key, const std::string& value) {
    constructor_->add(key, value);
  }

  void test(random* rnd) {
    std::vector<std::string> keys;
    kvmap data;
    constructor_->finish(options_, table_options_, *internal_comparator_,
                         &keys, &data);

    testforwardscan(keys, data);
    if (support_prev_) {
      testbackwardscan(keys, data);
    }
    testrandomaccess(rnd, keys, data);
  }

  void testforwardscan(const std::vector<std::string>& keys,
                       const kvmap& data) {
    iterator* iter = constructor_->newiterator();
    assert_true(!iter->valid());
    iter->seektofirst();
    for (kvmap::const_iterator model_iter = data.begin();
         model_iter != data.end();
         ++model_iter) {
      assert_eq(tostring(data, model_iter), tostring(iter));
      iter->next();
    }
    assert_true(!iter->valid());
    delete iter;
  }

  void testbackwardscan(const std::vector<std::string>& keys,
                        const kvmap& data) {
    iterator* iter = constructor_->newiterator();
    assert_true(!iter->valid());
    iter->seektolast();
    for (kvmap::const_reverse_iterator model_iter = data.rbegin();
         model_iter != data.rend();
         ++model_iter) {
      assert_eq(tostring(data, model_iter), tostring(iter));
      iter->prev();
    }
    assert_true(!iter->valid());
    delete iter;
  }

  void testrandomaccess(random* rnd,
                        const std::vector<std::string>& keys,
                        const kvmap& data) {
    static const bool kverbose = false;
    iterator* iter = constructor_->newiterator();
    assert_true(!iter->valid());
    kvmap::const_iterator model_iter = data.begin();
    if (kverbose) fprintf(stderr, "---\n");
    for (int i = 0; i < 200; i++) {
      const int toss = rnd->uniform(support_prev_ ? 5 : 3);
      switch (toss) {
        case 0: {
          if (iter->valid()) {
            if (kverbose) fprintf(stderr, "next\n");
            iter->next();
            ++model_iter;
            assert_eq(tostring(data, model_iter), tostring(iter));
          }
          break;
        }

        case 1: {
          if (kverbose) fprintf(stderr, "seektofirst\n");
          iter->seektofirst();
          model_iter = data.begin();
          assert_eq(tostring(data, model_iter), tostring(iter));
          break;
        }

        case 2: {
          std::string key = pickrandomkey(rnd, keys);
          model_iter = data.lower_bound(key);
          if (kverbose) fprintf(stderr, "seek '%s'\n",
                                escapestring(key).c_str());
          iter->seek(slice(key));
          assert_eq(tostring(data, model_iter), tostring(iter));
          break;
        }

        case 3: {
          if (iter->valid()) {
            if (kverbose) fprintf(stderr, "prev\n");
            iter->prev();
            if (model_iter == data.begin()) {
              model_iter = data.end();   // wrap around to invalid value
            } else {
              --model_iter;
            }
            assert_eq(tostring(data, model_iter), tostring(iter));
          }
          break;
        }

        case 4: {
          if (kverbose) fprintf(stderr, "seektolast\n");
          iter->seektolast();
          if (keys.empty()) {
            model_iter = data.end();
          } else {
            std::string last = data.rbegin()->first;
            model_iter = data.lower_bound(last);
          }
          assert_eq(tostring(data, model_iter), tostring(iter));
          break;
        }
      }
    }
    delete iter;
  }

  std::string tostring(const kvmap& data, const kvmap::const_iterator& it) {
    if (it == data.end()) {
      return "end";
    } else {
      return "'" + it->first + "->" + it->second + "'";
    }
  }

  std::string tostring(const kvmap& data,
                       const kvmap::const_reverse_iterator& it) {
    if (it == data.rend()) {
      return "end";
    } else {
      return "'" + it->first + "->" + it->second + "'";
    }
  }

  std::string tostring(const iterator* it) {
    if (!it->valid()) {
      return "end";
    } else {
      return "'" + it->key().tostring() + "->" + it->value().tostring() + "'";
    }
  }

  std::string pickrandomkey(random* rnd, const std::vector<std::string>& keys) {
    if (keys.empty()) {
      return "foo";
    } else {
      const int index = rnd->uniform(keys.size());
      std::string result = keys[index];
      switch (rnd->uniform(support_prev_ ? 3 : 1)) {
        case 0:
          // return an existing key
          break;
        case 1: {
          // attempt to return something smaller than an existing key
          if (result.size() > 0 && result[result.size() - 1] > '\0'
              && (!only_support_prefix_seek_
                  || options_.prefix_extractor->transform(result).size()
                  < result.size())) {
            result[result.size() - 1]--;
          }
          break;
      }
        case 2: {
          // return something larger than an existing key
          increment(options_.comparator, &result);
          break;
        }
      }
      return result;
    }
  }

  // returns nullptr if not running against a db
  db* db() const { return constructor_->db(); }

 private:
  options options_ = options();
  blockbasedtableoptions table_options_ = blockbasedtableoptions();
  constructor* constructor_;
  bool support_prev_;
  bool only_support_prefix_seek_;
  shared_ptr<internalkeycomparator> internal_comparator_;
};

static bool between(uint64_t val, uint64_t low, uint64_t high) {
  bool result = (val >= low) && (val <= high);
  if (!result) {
    fprintf(stderr, "value %llu is not in range [%llu, %llu]\n",
            (unsigned long long)(val),
            (unsigned long long)(low),
            (unsigned long long)(high));
  }
  return result;
}

// tests against all kinds of tables
class tabletest {
 public:
  const internalkeycomparator& getplaininternalcomparator(
      const comparator* comp) {
    if (!plain_internal_comparator) {
      plain_internal_comparator.reset(
          new test::plaininternalkeycomparator(comp));
    }
    return *plain_internal_comparator;
  }

 private:
  std::unique_ptr<internalkeycomparator> plain_internal_comparator;
};

class generaltabletest : public tabletest {};
class blockbasedtabletest : public tabletest {};
class plaintabletest : public tabletest {};
class tablepropertytest {};

// this test serves as the living tutorial for the prefix scan of user collected
// properties.
test(tablepropertytest, prefixscantest) {
  usercollectedproperties props{{"num.111.1", "1"},
                                {"num.111.2", "2"},
                                {"num.111.3", "3"},
                                {"num.333.1", "1"},
                                {"num.333.2", "2"},
                                {"num.333.3", "3"},
                                {"num.555.1", "1"},
                                {"num.555.2", "2"},
                                {"num.555.3", "3"}, };

  // prefixes that exist
  for (const std::string& prefix : {"num.111", "num.333", "num.555"}) {
    int num = 0;
    for (auto pos = props.lower_bound(prefix);
         pos != props.end() &&
             pos->first.compare(0, prefix.size(), prefix) == 0;
         ++pos) {
      ++num;
      auto key = prefix + "." + std::to_string(num);
      assert_eq(key, pos->first);
      assert_eq(std::to_string(num), pos->second);
    }
    assert_eq(3, num);
  }

  // prefixes that don't exist
  for (const std::string& prefix :
       {"num.000", "num.222", "num.444", "num.666"}) {
    auto pos = props.lower_bound(prefix);
    assert_true(pos == props.end() ||
                pos->first.compare(0, prefix.size(), prefix) != 0);
  }
}

// this test include all the basic checks except those for index size and block
// size, which will be conducted in separated unit tests.
test(blockbasedtabletest, basicblockbasedtableproperties) {
  tableconstructor c(bytewisecomparator());

  c.add("a1", "val1");
  c.add("b2", "val2");
  c.add("c3", "val3");
  c.add("d4", "val4");
  c.add("e5", "val5");
  c.add("f6", "val6");
  c.add("g7", "val7");
  c.add("h8", "val8");
  c.add("j9", "val9");

  std::vector<std::string> keys;
  kvmap kvmap;
  options options;
  options.compression = knocompression;
  blockbasedtableoptions table_options;
  table_options.block_restart_interval = 1;
  options.table_factory.reset(newblockbasedtablefactory(table_options));

  c.finish(options, table_options,
           getplaininternalcomparator(options.comparator), &keys, &kvmap);

  auto& props = *c.gettablereader()->gettableproperties();
  assert_eq(kvmap.size(), props.num_entries);

  auto raw_key_size = kvmap.size() * 2ul;
  auto raw_value_size = kvmap.size() * 4ul;

  assert_eq(raw_key_size, props.raw_key_size);
  assert_eq(raw_value_size, props.raw_value_size);
  assert_eq(1ul, props.num_data_blocks);
  assert_eq("", props.filter_policy_name);  // no filter policy is used

  // verify data size.
  blockbuilder block_builder(1);
  for (const auto& item : kvmap) {
    block_builder.add(item.first, item.second);
  }
  slice content = block_builder.finish();
  assert_eq(content.size() + kblocktrailersize, props.data_size);
}

test(blockbasedtabletest, filterpolicynameproperties) {
  tableconstructor c(bytewisecomparator(), true);
  c.add("a1", "val1");
  std::vector<std::string> keys;
  kvmap kvmap;
  blockbasedtableoptions table_options;
  table_options.filter_policy.reset(newbloomfilterpolicy(10));
  options options;
  options.table_factory.reset(newblockbasedtablefactory(table_options));

  c.finish(options, table_options,
           getplaininternalcomparator(options.comparator), &keys, &kvmap);
  auto& props = *c.gettablereader()->gettableproperties();
  assert_eq("rocksdb.builtinbloomfilter", props.filter_policy_name);
}

test(blockbasedtabletest, totalorderseekonhashindex) {
  blockbasedtableoptions table_options;
  for (int i = 0; i < 4; ++i) {
    options options;
    // make each key/value an individual block
    table_options.block_size = 64;
    switch (i) {
    case 0:
      // binary search index
      table_options.index_type = blockbasedtableoptions::kbinarysearch;
      options.table_factory.reset(new blockbasedtablefactory(table_options));
      break;
    case 1:
      // hash search index
      table_options.index_type = blockbasedtableoptions::khashsearch;
      options.table_factory.reset(new blockbasedtablefactory(table_options));
      options.prefix_extractor.reset(newfixedprefixtransform(4));
      break;
    case 2:
      // hash search index with hash_index_allow_collision
      table_options.index_type = blockbasedtableoptions::khashsearch;
      table_options.hash_index_allow_collision = true;
      options.table_factory.reset(new blockbasedtablefactory(table_options));
      options.prefix_extractor.reset(newfixedprefixtransform(4));
      break;
    case 3:
    default:
      // hash search index with filter policy
      table_options.index_type = blockbasedtableoptions::khashsearch;
      table_options.filter_policy.reset(newbloomfilterpolicy(10));
      options.table_factory.reset(new blockbasedtablefactory(table_options));
      options.prefix_extractor.reset(newfixedprefixtransform(4));
      break;
    }

    tableconstructor c(bytewisecomparator(), true);
    c.add("aaaa1", std::string('a', 56));
    c.add("bbaa1", std::string('a', 56));
    c.add("cccc1", std::string('a', 56));
    c.add("bbbb1", std::string('a', 56));
    c.add("baaa1", std::string('a', 56));
    c.add("abbb1", std::string('a', 56));
    c.add("cccc2", std::string('a', 56));
    std::vector<std::string> keys;
    kvmap kvmap;
    c.finish(options, table_options,
             getplaininternalcomparator(options.comparator), &keys, &kvmap);
    auto props = c.gettablereader()->gettableproperties();
    assert_eq(7u, props->num_data_blocks);
    auto* reader = c.gettablereader();
    readoptions ro;
    ro.total_order_seek = true;
    std::unique_ptr<iterator> iter(reader->newiterator(ro));

    iter->seek(internalkey("b", 0, ktypevalue).encode());
    assert_ok(iter->status());
    assert_true(iter->valid());
    assert_eq("baaa1", extractuserkey(iter->key()).tostring());
    iter->next();
    assert_ok(iter->status());
    assert_true(iter->valid());
    assert_eq("bbaa1", extractuserkey(iter->key()).tostring());

    iter->seek(internalkey("bb", 0, ktypevalue).encode());
    assert_ok(iter->status());
    assert_true(iter->valid());
    assert_eq("bbaa1", extractuserkey(iter->key()).tostring());
    iter->next();
    assert_ok(iter->status());
    assert_true(iter->valid());
    assert_eq("bbbb1", extractuserkey(iter->key()).tostring());

    iter->seek(internalkey("bbb", 0, ktypevalue).encode());
    assert_ok(iter->status());
    assert_true(iter->valid());
    assert_eq("bbbb1", extractuserkey(iter->key()).tostring());
    iter->next();
    assert_ok(iter->status());
    assert_true(iter->valid());
    assert_eq("cccc1", extractuserkey(iter->key()).tostring());
  }
}

static std::string randomstring(random* rnd, int len) {
  std::string r;
  test::randomstring(rnd, len, &r);
  return r;
}

void addinternalkey(tableconstructor* c, const std::string prefix,
                    int suffix_len = 800) {
  static random rnd(1023);
  internalkey k(prefix + randomstring(&rnd, 800), 0, ktypevalue);
  c->add(k.encode().tostring(), "v");
}

test(tabletest, hashindextest) {
  tableconstructor c(bytewisecomparator());

  // keys with prefix length 3, make sure the key/value is big enough to fill
  // one block
  addinternalkey(&c, "0015");
  addinternalkey(&c, "0035");

  addinternalkey(&c, "0054");
  addinternalkey(&c, "0055");

  addinternalkey(&c, "0056");
  addinternalkey(&c, "0057");

  addinternalkey(&c, "0058");
  addinternalkey(&c, "0075");

  addinternalkey(&c, "0076");
  addinternalkey(&c, "0095");

  std::vector<std::string> keys;
  kvmap kvmap;
  options options;
  options.prefix_extractor.reset(newfixedprefixtransform(3));
  blockbasedtableoptions table_options;
  table_options.index_type = blockbasedtableoptions::khashsearch;
  table_options.hash_index_allow_collision = true;
  table_options.block_size = 1700;
  table_options.block_cache = newlrucache(1024);
  options.table_factory.reset(newblockbasedtablefactory(table_options));

  std::unique_ptr<internalkeycomparator> comparator(
      new internalkeycomparator(bytewisecomparator()));
  c.finish(options, table_options, *comparator, &keys, &kvmap);
  auto reader = c.gettablereader();

  auto props = reader->gettableproperties();
  assert_eq(5u, props->num_data_blocks);

  std::unique_ptr<iterator> hash_iter(reader->newiterator(readoptions()));

  // -- find keys do not exist, but have common prefix.
  std::vector<std::string> prefixes = {"001", "003", "005", "007", "009"};
  std::vector<std::string> lower_bound = {keys[0], keys[1], keys[2],
                                          keys[7], keys[9], };

  // find the lower bound of the prefix
  for (size_t i = 0; i < prefixes.size(); ++i) {
    hash_iter->seek(internalkey(prefixes[i], 0, ktypevalue).encode());
    assert_ok(hash_iter->status());
    assert_true(hash_iter->valid());

    // seek the first element in the block
    assert_eq(lower_bound[i], hash_iter->key().tostring());
    assert_eq("v", hash_iter->value().tostring());
  }

  // find the upper bound of prefixes
  std::vector<std::string> upper_bound = {keys[1], keys[2], keys[7], keys[9], };

  // find existing keys
  for (const auto& item : kvmap) {
    auto ukey = extractuserkey(item.first).tostring();
    hash_iter->seek(ukey);

    // assert_ok(regular_iter->status());
    assert_ok(hash_iter->status());

    // assert_true(regular_iter->valid());
    assert_true(hash_iter->valid());

    assert_eq(item.first, hash_iter->key().tostring());
    assert_eq(item.second, hash_iter->value().tostring());
  }

  for (size_t i = 0; i < prefixes.size(); ++i) {
    // the key is greater than any existing keys.
    auto key = prefixes[i] + "9";
    hash_iter->seek(internalkey(key, 0, ktypevalue).encode());

    assert_ok(hash_iter->status());
    if (i == prefixes.size() - 1) {
      // last key
      assert_true(!hash_iter->valid());
    } else {
      assert_true(hash_iter->valid());
      // seek the first element in the block
      assert_eq(upper_bound[i], hash_iter->key().tostring());
      assert_eq("v", hash_iter->value().tostring());
    }
  }

  // find keys with prefix that don't match any of the existing prefixes.
  std::vector<std::string> non_exist_prefixes = {"002", "004", "006", "008"};
  for (const auto& prefix : non_exist_prefixes) {
    hash_iter->seek(internalkey(prefix, 0, ktypevalue).encode());
    // regular_iter->seek(prefix);

    assert_ok(hash_iter->status());
    // seek to non-existing prefixes should yield either invalid, or a
    // key with prefix greater than the target.
    if (hash_iter->valid()) {
      slice ukey = extractuserkey(hash_iter->key());
      slice ukey_prefix = options.prefix_extractor->transform(ukey);
      assert_true(bytewisecomparator()->compare(prefix, ukey_prefix) < 0);
    }
  }
}

// it's very hard to figure out the index block size of a block accurately.
// to make sure we get the index size, we just make sure as key number
// grows, the filter block size also grows.
test(blockbasedtabletest, indexsizestat) {
  uint64_t last_index_size = 0;

  // we need to use random keys since the pure human readable texts
  // may be well compressed, resulting insignifcant change of index
  // block size.
  random rnd(test::randomseed());
  std::vector<std::string> keys;

  for (int i = 0; i < 100; ++i) {
    keys.push_back(randomstring(&rnd, 10000));
  }

  // each time we load one more key to the table. the table index block
  // size is expected to be larger than last time's.
  for (size_t i = 1; i < keys.size(); ++i) {
    tableconstructor c(bytewisecomparator());
    for (size_t j = 0; j < i; ++j) {
      c.add(keys[j], "val");
    }

    std::vector<std::string> ks;
    kvmap kvmap;
    options options;
    options.compression = knocompression;
    blockbasedtableoptions table_options;
    table_options.block_restart_interval = 1;
    options.table_factory.reset(newblockbasedtablefactory(table_options));

    c.finish(options, table_options,
             getplaininternalcomparator(options.comparator), &ks, &kvmap);
    auto index_size = c.gettablereader()->gettableproperties()->index_size;
    assert_gt(index_size, last_index_size);
    last_index_size = index_size;
  }
}

test(blockbasedtabletest, numblockstat) {
  random rnd(test::randomseed());
  tableconstructor c(bytewisecomparator());
  options options;
  options.compression = knocompression;
  blockbasedtableoptions table_options;
  table_options.block_restart_interval = 1;
  table_options.block_size = 1000;
  options.table_factory.reset(newblockbasedtablefactory(table_options));

  for (int i = 0; i < 10; ++i) {
    // the key/val are slightly smaller than block size, so that each block
    // holds roughly one key/value pair.
    c.add(randomstring(&rnd, 900), "val");
  }

  std::vector<std::string> ks;
  kvmap kvmap;
  c.finish(options, table_options,
           getplaininternalcomparator(options.comparator), &ks, &kvmap);
  assert_eq(kvmap.size(),
            c.gettablereader()->gettableproperties()->num_data_blocks);
}

// a simple tool that takes the snapshot of block cache statistics.
class blockcachepropertiessnapshot {
 public:
  explicit blockcachepropertiessnapshot(statistics* statistics) {
    block_cache_miss = statistics->gettickercount(block_cache_miss);
    block_cache_hit = statistics->gettickercount(block_cache_hit);
    index_block_cache_miss = statistics->gettickercount(block_cache_index_miss);
    index_block_cache_hit = statistics->gettickercount(block_cache_index_hit);
    data_block_cache_miss = statistics->gettickercount(block_cache_data_miss);
    data_block_cache_hit = statistics->gettickercount(block_cache_data_hit);
    filter_block_cache_miss =
        statistics->gettickercount(block_cache_filter_miss);
    filter_block_cache_hit = statistics->gettickercount(block_cache_filter_hit);
  }

  void assertindexblockstat(int64_t index_block_cache_miss,
                            int64_t index_block_cache_hit) {
    assert_eq(index_block_cache_miss, this->index_block_cache_miss);
    assert_eq(index_block_cache_hit, this->index_block_cache_hit);
  }

  void assertfilterblockstat(int64_t filter_block_cache_miss,
                             int64_t filter_block_cache_hit) {
    assert_eq(filter_block_cache_miss, this->filter_block_cache_miss);
    assert_eq(filter_block_cache_hit, this->filter_block_cache_hit);
  }

  // check if the fetched props matches the expected ones.
  // todo(kailiu) use this only when you disabled filter policy!
  void assertequal(int64_t index_block_cache_miss,
                   int64_t index_block_cache_hit, int64_t data_block_cache_miss,
                   int64_t data_block_cache_hit) const {
    assert_eq(index_block_cache_miss, this->index_block_cache_miss);
    assert_eq(index_block_cache_hit, this->index_block_cache_hit);
    assert_eq(data_block_cache_miss, this->data_block_cache_miss);
    assert_eq(data_block_cache_hit, this->data_block_cache_hit);
    assert_eq(index_block_cache_miss + data_block_cache_miss,
              this->block_cache_miss);
    assert_eq(index_block_cache_hit + data_block_cache_hit,
              this->block_cache_hit);
  }

 private:
  int64_t block_cache_miss = 0;
  int64_t block_cache_hit = 0;
  int64_t index_block_cache_miss = 0;
  int64_t index_block_cache_hit = 0;
  int64_t data_block_cache_miss = 0;
  int64_t data_block_cache_hit = 0;
  int64_t filter_block_cache_miss = 0;
  int64_t filter_block_cache_hit = 0;
};

// make sure, by default, index/filter blocks were pre-loaded (meaning we won't
// use block cache to store them).
test(blockbasedtabletest, blockcachedisabledtest) {
  options options;
  options.create_if_missing = true;
  options.statistics = createdbstatistics();
  blockbasedtableoptions table_options;
  // intentionally commented out: table_options.cache_index_and_filter_blocks =
  // true;
  table_options.block_cache = newlrucache(1024);
  table_options.filter_policy.reset(newbloomfilterpolicy(10));
  options.table_factory.reset(new blockbasedtablefactory(table_options));
  std::vector<std::string> keys;
  kvmap kvmap;

  tableconstructor c(bytewisecomparator(), true);
  c.add("key", "value");
  c.finish(options, table_options,
           getplaininternalcomparator(options.comparator), &keys, &kvmap);

  // preloading filter/index blocks is enabled.
  auto reader = dynamic_cast<blockbasedtable*>(c.gettablereader());
  assert_true(reader->test_filter_block_preloaded());
  assert_true(reader->test_index_reader_preloaded());

  {
    // nothing happens in the beginning
    blockcachepropertiessnapshot props(options.statistics.get());
    props.assertindexblockstat(0, 0);
    props.assertfilterblockstat(0, 0);
  }

  {
    // a hack that just to trigger blockbasedtable::getfilter.
    reader->get(readoptions(), "non-exist-key", nullptr, nullptr, nullptr);
    blockcachepropertiessnapshot props(options.statistics.get());
    props.assertindexblockstat(0, 0);
    props.assertfilterblockstat(0, 0);
  }
}

// due to the difficulities of the intersaction between statistics, this test
// only tests the case when "index block is put to block cache"
test(blockbasedtabletest, filterblockinblockcache) {
  // -- table construction
  options options;
  options.create_if_missing = true;
  options.statistics = createdbstatistics();

  // enable the cache for index/filter blocks
  blockbasedtableoptions table_options;
  table_options.block_cache = newlrucache(1024);
  table_options.cache_index_and_filter_blocks = true;
  options.table_factory.reset(new blockbasedtablefactory(table_options));
  std::vector<std::string> keys;
  kvmap kvmap;

  tableconstructor c(bytewisecomparator());
  c.add("key", "value");
  c.finish(options, table_options,
           getplaininternalcomparator(options.comparator), &keys, &kvmap);
  // preloading filter/index blocks is prohibited.
  auto reader = dynamic_cast<blockbasedtable*>(c.gettablereader());
  assert_true(!reader->test_filter_block_preloaded());
  assert_true(!reader->test_index_reader_preloaded());

  // -- part 1: open with regular block cache.
  // since block_cache is disabled, no cache activities will be involved.
  unique_ptr<iterator> iter;

  // at first, no block will be accessed.
  {
    blockcachepropertiessnapshot props(options.statistics.get());
    // index will be added to block cache.
    props.assertequal(1,  // index block miss
                      0, 0, 0);
  }

  // only index block will be accessed
  {
    iter.reset(c.newiterator());
    blockcachepropertiessnapshot props(options.statistics.get());
    // note: to help better highlight the "detla" of each ticker, i use
    // <last_value> + <added_value> to indicate the increment of changed
    // value; other numbers remain the same.
    props.assertequal(1, 0 + 1,  // index block hit
                      0, 0);
  }

  // only data block will be accessed
  {
    iter->seektofirst();
    blockcachepropertiessnapshot props(options.statistics.get());
    props.assertequal(1, 1, 0 + 1,  // data block miss
                      0);
  }

  // data block will be in cache
  {
    iter.reset(c.newiterator());
    iter->seektofirst();
    blockcachepropertiessnapshot props(options.statistics.get());
    props.assertequal(1, 1 + 1, /* index block hit */
                      1, 0 + 1 /* data block hit */);
  }
  // release the iterator so that the block cache can reset correctly.
  iter.reset();

  // -- part 2: open without block cache
  table_options.no_block_cache = true;
  table_options.block_cache.reset();
  options.table_factory.reset(new blockbasedtablefactory(table_options));
  options.statistics = createdbstatistics();  // reset the stats
  c.reopen(options);
  table_options.no_block_cache = false;

  {
    iter.reset(c.newiterator());
    iter->seektofirst();
    assert_eq("key", iter->key().tostring());
    blockcachepropertiessnapshot props(options.statistics.get());
    // nothing is affected at all
    props.assertequal(0, 0, 0, 0);
  }

  // -- part 3: open with very small block cache
  // in this test, no block will ever get hit since the block cache is
  // too small to fit even one entry.
  table_options.block_cache = newlrucache(1);
  options.table_factory.reset(new blockbasedtablefactory(table_options));
  c.reopen(options);
  {
    blockcachepropertiessnapshot props(options.statistics.get());
    props.assertequal(1,  // index block miss
                      0, 0, 0);
  }


  {
    // both index and data block get accessed.
    // it first cache index block then data block. but since the cache size
    // is only 1, index block will be purged after data block is inserted.
    iter.reset(c.newiterator());
    blockcachepropertiessnapshot props(options.statistics.get());
    props.assertequal(1 + 1,  // index block miss
                      0, 0,   // data block miss
                      0);
  }

  {
    // seektofirst() accesses data block. with similar reason, we expect data
    // block's cache miss.
    iter->seektofirst();
    blockcachepropertiessnapshot props(options.statistics.get());
    props.assertequal(2, 0, 0 + 1,  // data block miss
                      0);
  }
}

test(blockbasedtabletest, blockcacheleak) {
  // check that when we reopen a table we don't lose access to blocks already
  // in the cache. this test checks whether the table actually makes use of the
  // unique id from the file.

  options opt;
  unique_ptr<internalkeycomparator> ikc;
  ikc.reset(new test::plaininternalkeycomparator(opt.comparator));
  opt.compression = knocompression;
  blockbasedtableoptions table_options;
  table_options.block_size = 1024;
  // big enough so we don't ever lose cached values.
  table_options.block_cache = newlrucache(16 * 1024 * 1024);
  opt.table_factory.reset(newblockbasedtablefactory(table_options));

  tableconstructor c(bytewisecomparator());
  c.add("k01", "hello");
  c.add("k02", "hello2");
  c.add("k03", std::string(10000, 'x'));
  c.add("k04", std::string(200000, 'x'));
  c.add("k05", std::string(300000, 'x'));
  c.add("k06", "hello3");
  c.add("k07", std::string(100000, 'x'));
  std::vector<std::string> keys;
  kvmap kvmap;
  c.finish(opt, table_options, *ikc, &keys, &kvmap);

  unique_ptr<iterator> iter(c.newiterator());
  iter->seektofirst();
  while (iter->valid()) {
    iter->key();
    iter->value();
    iter->next();
  }
  assert_ok(iter->status());

  assert_ok(c.reopen(opt));
  auto table_reader = dynamic_cast<blockbasedtable*>(c.gettablereader());
  for (const std::string& key : keys) {
    assert_true(table_reader->test_keyincache(readoptions(), key));
  }

  // rerun with different block cache
  table_options.block_cache = newlrucache(16 * 1024 * 1024);
  opt.table_factory.reset(newblockbasedtablefactory(table_options));
  assert_ok(c.reopen(opt));
  table_reader = dynamic_cast<blockbasedtable*>(c.gettablereader());
  for (const std::string& key : keys) {
    assert_true(!table_reader->test_keyincache(readoptions(), key));
  }
}

test(plaintabletest, basicplaintableproperties) {
  plaintableoptions plain_table_options;
  plain_table_options.user_key_len = 8;
  plain_table_options.bloom_bits_per_key = 8;
  plain_table_options.hash_table_ratio = 0;

  plaintablefactory factory(plain_table_options);
  stringsink sink;
  options options;
  internalkeycomparator ikc(options.comparator);
  std::unique_ptr<tablebuilder> builder(
      factory.newtablebuilder(options, ikc, &sink, knocompression));

  for (char c = 'a'; c <= 'z'; ++c) {
    std::string key(8, c);
    key.append("\1       ");  // plaintable expects internal key structure
    std::string value(28, c + 42);
    builder->add(key, value);
  }
  assert_ok(builder->finish());

  stringsource source(sink.contents(), 72242, true);

  tableproperties* props = nullptr;
  auto s = readtableproperties(&source, sink.contents().size(),
                               kplaintablemagicnumber, env::default(), nullptr,
                               &props);
  std::unique_ptr<tableproperties> props_guard(props);
  assert_ok(s);

  assert_eq(0ul, props->index_size);
  assert_eq(0ul, props->filter_size);
  assert_eq(16ul * 26, props->raw_key_size);
  assert_eq(28ul * 26, props->raw_value_size);
  assert_eq(26ul, props->num_entries);
  assert_eq(1ul, props->num_data_blocks);
}

test(generaltabletest, approximateoffsetofplain) {
  tableconstructor c(bytewisecomparator());
  c.add("k01", "hello");
  c.add("k02", "hello2");
  c.add("k03", std::string(10000, 'x'));
  c.add("k04", std::string(200000, 'x'));
  c.add("k05", std::string(300000, 'x'));
  c.add("k06", "hello3");
  c.add("k07", std::string(100000, 'x'));
  std::vector<std::string> keys;
  kvmap kvmap;
  options options;
  test::plaininternalkeycomparator internal_comparator(options.comparator);
  options.compression = knocompression;
  blockbasedtableoptions table_options;
  table_options.block_size = 1024;
  c.finish(options, table_options, internal_comparator, &keys, &kvmap);

  assert_true(between(c.approximateoffsetof("abc"),       0,      0));
  assert_true(between(c.approximateoffsetof("k01"),       0,      0));
  assert_true(between(c.approximateoffsetof("k01a"),      0,      0));
  assert_true(between(c.approximateoffsetof("k02"),       0,      0));
  assert_true(between(c.approximateoffsetof("k03"),       0,      0));
  assert_true(between(c.approximateoffsetof("k04"),   10000,  11000));
  assert_true(between(c.approximateoffsetof("k04a"), 210000, 211000));
  assert_true(between(c.approximateoffsetof("k05"),  210000, 211000));
  assert_true(between(c.approximateoffsetof("k06"),  510000, 511000));
  assert_true(between(c.approximateoffsetof("k07"),  510000, 511000));
  assert_true(between(c.approximateoffsetof("xyz"),  610000, 612000));
}

static void docompressiontest(compressiontype comp) {
  random rnd(301);
  tableconstructor c(bytewisecomparator());
  std::string tmp;
  c.add("k01", "hello");
  c.add("k02", test::compressiblestring(&rnd, 0.25, 10000, &tmp));
  c.add("k03", "hello3");
  c.add("k04", test::compressiblestring(&rnd, 0.25, 10000, &tmp));
  std::vector<std::string> keys;
  kvmap kvmap;
  options options;
  test::plaininternalkeycomparator ikc(options.comparator);
  options.compression = comp;
  blockbasedtableoptions table_options;
  table_options.block_size = 1024;
  c.finish(options, table_options, ikc, &keys, &kvmap);

  assert_true(between(c.approximateoffsetof("abc"),       0,      0));
  assert_true(between(c.approximateoffsetof("k01"),       0,      0));
  assert_true(between(c.approximateoffsetof("k02"),       0,      0));
  assert_true(between(c.approximateoffsetof("k03"),    2000,   3000));
  assert_true(between(c.approximateoffsetof("k04"),    2000,   3000));
  assert_true(between(c.approximateoffsetof("xyz"),    4000,   6100));
}

test(generaltabletest, approximateoffsetofcompressed) {
  std::vector<compressiontype> compression_state;
  if (!snappycompressionsupported()) {
    fprintf(stderr, "skipping snappy compression tests\n");
  } else {
    compression_state.push_back(ksnappycompression);
  }

  if (!zlibcompressionsupported()) {
    fprintf(stderr, "skipping zlib compression tests\n");
  } else {
    compression_state.push_back(kzlibcompression);
  }

  // todo(kailiu) docompressiontest() doesn't work with bzip2.
  /*
  if (!bzip2compressionsupported()) {
    fprintf(stderr, "skipping bzip2 compression tests\n");
  } else {
    compression_state.push_back(kbzip2compression);
  }
  */

  if (!lz4compressionsupported()) {
    fprintf(stderr, "skipping lz4 compression tests\n");
  } else {
    compression_state.push_back(klz4compression);
  }

  if (!lz4hccompressionsupported()) {
    fprintf(stderr, "skipping lz4hc compression tests\n");
  } else {
    compression_state.push_back(klz4hccompression);
  }

  for (auto state : compression_state) {
    docompressiontest(state);
  }
}

test(harness, randomized) {
  std::vector<testargs> args = generatearglist();
  for (unsigned int i = 0; i < args.size(); i++) {
    init(args[i]);
    random rnd(test::randomseed() + 5);
    for (int num_entries = 0; num_entries < 2000;
         num_entries += (num_entries < 50 ? 1 : 200)) {
      if ((num_entries % 10) == 0) {
        fprintf(stderr, "case %d of %d: num_entries = %d\n", (i + 1),
                static_cast<int>(args.size()), num_entries);
      }
      for (int e = 0; e < num_entries; e++) {
        std::string v;
        add(test::randomkey(&rnd, rnd.skewed(4)),
            test::randomstring(&rnd, rnd.skewed(5), &v).tostring());
      }
      test(&rnd);
    }
  }
}

test(harness, randomizedlongdb) {
  random rnd(test::randomseed());
  testargs args = { db_test, false, 16, knocompression };
  init(args);
  int num_entries = 100000;
  for (int e = 0; e < num_entries; e++) {
    std::string v;
    add(test::randomkey(&rnd, rnd.skewed(4)),
        test::randomstring(&rnd, rnd.skewed(5), &v).tostring());
  }
  test(&rnd);

  // we must have created enough data to force merging
  int files = 0;
  for (int level = 0; level < db()->numberlevels(); level++) {
    std::string value;
    char name[100];
    snprintf(name, sizeof(name), "rocksdb.num-files-at-level%d", level);
    assert_true(db()->getproperty(name, &value));
    files += atoi(value.c_str());
  }
  assert_gt(files, 0);
}

class memtabletest { };

test(memtabletest, simple) {
  internalkeycomparator cmp(bytewisecomparator());
  auto table_factory = std::make_shared<skiplistfactory>();
  options options;
  options.memtable_factory = table_factory;
  memtable* memtable = new memtable(cmp, options);
  memtable->ref();
  writebatch batch;
  writebatchinternal::setsequence(&batch, 100);
  batch.put(std::string("k1"), std::string("v1"));
  batch.put(std::string("k2"), std::string("v2"));
  batch.put(std::string("k3"), std::string("v3"));
  batch.put(std::string("largekey"), std::string("vlarge"));
  columnfamilymemtablesdefault cf_mems_default(memtable, &options);
  assert_true(writebatchinternal::insertinto(&batch, &cf_mems_default).ok());

  iterator* iter = memtable->newiterator(readoptions());
  iter->seektofirst();
  while (iter->valid()) {
    fprintf(stderr, "key: '%s' -> '%s'\n",
            iter->key().tostring().c_str(),
            iter->value().tostring().c_str());
    iter->next();
  }

  delete iter;
  delete memtable->unref();
}

// test the empty key
test(harness, simpleemptykey) {
  auto args = generatearglist();
  for (const auto& arg : args) {
    init(arg);
    random rnd(test::randomseed() + 1);
    add("", "v");
    test(&rnd);
  }
}

test(harness, simplesingle) {
  auto args = generatearglist();
  for (const auto& arg : args) {
    init(arg);
    random rnd(test::randomseed() + 2);
    add("abc", "v");
    test(&rnd);
  }
}

test(harness, simplemulti) {
  auto args = generatearglist();
  for (const auto& arg : args) {
    init(arg);
    random rnd(test::randomseed() + 3);
    add("abc", "v");
    add("abcd", "v");
    add("ac", "v2");
    test(&rnd);
  }
}

test(harness, simplespecialkey) {
  auto args = generatearglist();
  for (const auto& arg : args) {
    init(arg);
    random rnd(test::randomseed() + 4);
    add("\xff\xff", "v3");
    test(&rnd);
  }
}

test(harness, footertests) {
  {
    // upconvert legacy block based
    std::string encoded;
    footer footer(klegacyblockbasedtablemagicnumber);
    blockhandle meta_index(10, 5), index(20, 15);
    footer.set_metaindex_handle(meta_index);
    footer.set_index_handle(index);
    footer.encodeto(&encoded);
    footer decoded_footer;
    slice encoded_slice(encoded);
    decoded_footer.decodefrom(&encoded_slice);
    assert_eq(decoded_footer.table_magic_number(), kblockbasedtablemagicnumber);
    assert_eq(decoded_footer.checksum(), kcrc32c);
    assert_eq(decoded_footer.metaindex_handle().offset(), meta_index.offset());
    assert_eq(decoded_footer.metaindex_handle().size(), meta_index.size());
    assert_eq(decoded_footer.index_handle().offset(), index.offset());
    assert_eq(decoded_footer.index_handle().size(), index.size());
  }
  {
    // xxhash block based
    std::string encoded;
    footer footer(kblockbasedtablemagicnumber);
    blockhandle meta_index(10, 5), index(20, 15);
    footer.set_metaindex_handle(meta_index);
    footer.set_index_handle(index);
    footer.set_checksum(kxxhash);
    footer.encodeto(&encoded);
    footer decoded_footer;
    slice encoded_slice(encoded);
    decoded_footer.decodefrom(&encoded_slice);
    assert_eq(decoded_footer.table_magic_number(), kblockbasedtablemagicnumber);
    assert_eq(decoded_footer.checksum(), kxxhash);
    assert_eq(decoded_footer.metaindex_handle().offset(), meta_index.offset());
    assert_eq(decoded_footer.metaindex_handle().size(), meta_index.size());
    assert_eq(decoded_footer.index_handle().offset(), index.offset());
    assert_eq(decoded_footer.index_handle().size(), index.size());
  }
  {
    // upconvert legacy plain table
    std::string encoded;
    footer footer(klegacyplaintablemagicnumber);
    blockhandle meta_index(10, 5), index(20, 15);
    footer.set_metaindex_handle(meta_index);
    footer.set_index_handle(index);
    footer.encodeto(&encoded);
    footer decoded_footer;
    slice encoded_slice(encoded);
    decoded_footer.decodefrom(&encoded_slice);
    assert_eq(decoded_footer.table_magic_number(), kplaintablemagicnumber);
    assert_eq(decoded_footer.checksum(), kcrc32c);
    assert_eq(decoded_footer.metaindex_handle().offset(), meta_index.offset());
    assert_eq(decoded_footer.metaindex_handle().size(), meta_index.size());
    assert_eq(decoded_footer.index_handle().offset(), index.offset());
    assert_eq(decoded_footer.index_handle().size(), index.size());
  }
  {
    // xxhash block based
    std::string encoded;
    footer footer(kplaintablemagicnumber);
    blockhandle meta_index(10, 5), index(20, 15);
    footer.set_metaindex_handle(meta_index);
    footer.set_index_handle(index);
    footer.set_checksum(kxxhash);
    footer.encodeto(&encoded);
    footer decoded_footer;
    slice encoded_slice(encoded);
    decoded_footer.decodefrom(&encoded_slice);
    assert_eq(decoded_footer.table_magic_number(), kplaintablemagicnumber);
    assert_eq(decoded_footer.checksum(), kxxhash);
    assert_eq(decoded_footer.metaindex_handle().offset(), meta_index.offset());
    assert_eq(decoded_footer.metaindex_handle().size(), meta_index.size());
    assert_eq(decoded_footer.index_handle().offset(), index.offset());
    assert_eq(decoded_footer.index_handle().size(), index.size());
  }
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
