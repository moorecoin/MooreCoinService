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

#include "rocksdb/db.h"
#include "rocksdb/filter_policy.h"
#include "db/db_impl.h"
#include "db/filename.h"
#include "db/version_set.h"
#include "db/write_batch_internal.h"
#include "rocksdb/statistics.h"
#include "rocksdb/cache.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/env.h"
#include "rocksdb/table.h"
#include "rocksdb/table_properties.h"
#include "table/table_builder.h"
#include "util/hash.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/testharness.h"
#include "util/testutil.h"
#include "utilities/merge_operators.h"

using std::unique_ptr;

// is this file still needed?
namespace rocksdb {

// simpletable is a simple table format for unit test only. it is not built
// as production quality.
// simpletable requires the input key size to be fixed 16 bytes, value cannot
// be longer than 150000 bytes and stored data on disk in this format:
// +--------------------------------------------+  <= key1 offset
// | key1            | value_size (4 bytes) |   |
// +----------------------------------------+   |
// | value1                                     |
// |                                            |
// +----------------------------------------+---+  <= key2 offset
// | key2            | value_size (4 bytes) |   |
// +----------------------------------------+   |
// | value2                                     |
// |                                            |
// |        ......                              |
// +-----------------+--------------------------+   <= index_block_offset
// | key1            | key1 offset (8 bytes)    |
// +-----------------+--------------------------+
// | key2            | key2 offset (8 bytes)    |
// +-----------------+--------------------------+
// | key3            | key3 offset (8 bytes)    |
// +-----------------+--------------------------+
// |        ......                              |
// +-----------------+------------+-------------+
// | index_block_offset (8 bytes) |
// +------------------------------+

// simpletable is a simple table format for unit test only. it is not built
// as production quality.
class simpletablereader: public tablereader {
public:
  // attempt to open the table that is stored in bytes [0..file_size)
  // of "file", and read the metadata entries necessary to allow
  // retrieving data from the table.
  //
  // if successful, returns ok and sets "*table" to the newly opened
  // table.  the client should delete "*table" when no longer needed.
  // if there was an error while initializing the table, sets "*table"
  // to nullptr and returns a non-ok status.  does not take ownership of
  // "*source", but the client must ensure that "source" remains live
  // for the duration of the returned table's lifetime.
  //
  // *file must remain live while this table is in use.
  static status open(const options& options, const envoptions& soptions,
                     unique_ptr<randomaccessfile> && file, uint64_t file_size,
                     unique_ptr<tablereader>* table_reader);

  iterator* newiterator(const readoptions&, arena* arena) override;

  status get(const readoptions&, const slice& key, void* arg,
             bool (*handle_result)(void* arg, const parsedinternalkey& k,
                                   const slice& v),
             void (*mark_key_may_exist)(void*) = nullptr) override;

  uint64_t approximateoffsetof(const slice& key) override;

  virtual size_t approximatememoryusage() const override { return 0; }

  void setupforcompaction() override;

  std::shared_ptr<const tableproperties> gettableproperties() const override;

  ~simpletablereader();

private:
  struct rep;
  rep* rep_;

  explicit simpletablereader(rep* rep) {
    rep_ = rep;
  }
  friend class tablecache;
  friend class simpletableiterator;

  status getoffset(const slice& target, uint64_t* offset);

  // no copying allowed
  explicit simpletablereader(const tablereader&) = delete;
  void operator=(const tablereader&) = delete;
};

// iterator to iterate simpletable
class simpletableiterator: public iterator {
public:
  explicit simpletableiterator(simpletablereader* table);
  ~simpletableiterator();

  bool valid() const;

  void seektofirst();

  void seektolast();

  void seek(const slice& target);

  void next();

  void prev();

  slice key() const;

  slice value() const;

  status status() const;

private:
  simpletablereader* table_;
  uint64_t offset_;
  uint64_t next_offset_;
  slice key_;
  slice value_;
  char tmp_str_[4];
  char* key_str_;
  char* value_str_;
  int value_str_len_;
  status status_;
  // no copying allowed
  simpletableiterator(const simpletableiterator&) = delete;
  void operator=(const iterator&) = delete;
};

struct simpletablereader::rep {
  ~rep() {
  }
  rep(const envoptions& storage_options, uint64_t index_start_offset,
      int num_entries) :
      soptions(storage_options), index_start_offset(index_start_offset),
      num_entries(num_entries) {
  }

  options options;
  const envoptions& soptions;
  status status;
  unique_ptr<randomaccessfile> file;
  uint64_t index_start_offset;
  int num_entries;
  std::shared_ptr<tableproperties> table_properties;

  const static int user_key_size = 16;
  const static int offset_length = 8;
  const static int key_footer_len = 8;

  static int getinternalkeylength() {
    return user_key_size + key_footer_len;
  }
};

simpletablereader::~simpletablereader() {
  delete rep_;
}

status simpletablereader::open(const options& options,
                               const envoptions& soptions,
                               unique_ptr<randomaccessfile> && file,
                               uint64_t size,
                               unique_ptr<tablereader>* table_reader) {
  char footer_space[rep::offset_length];
  slice footer_input;
  status s = file->read(size - rep::offset_length, rep::offset_length,
                        &footer_input, footer_space);
  if (s.ok()) {
    uint64_t index_start_offset = decodefixed64(footer_space);

    int num_entries = (size - rep::offset_length - index_start_offset)
        / (rep::getinternalkeylength() + rep::offset_length);
    simpletablereader::rep* rep = new simpletablereader::rep(soptions,
                                                             index_start_offset,
                                                             num_entries);

    rep->file = std::move(file);
    rep->options = options;
    table_reader->reset(new simpletablereader(rep));
  }
  return s;
}

void simpletablereader::setupforcompaction() {
}

std::shared_ptr<const tableproperties> simpletablereader::gettableproperties()
    const {
  return rep_->table_properties;
}

iterator* simpletablereader::newiterator(const readoptions& options,
                                         arena* arena) {
  if (arena == nullptr) {
    return new simpletableiterator(this);
  } else {
    auto mem = arena->allocatealigned(sizeof(simpletableiterator));
    return new (mem) simpletableiterator(this);
  }
}

status simpletablereader::getoffset(const slice& target, uint64_t* offset) {
  uint32_t left = 0;
  uint32_t right = rep_->num_entries - 1;
  char key_chars[rep::getinternalkeylength()];
  slice tmp_slice;

  uint32_t target_offset = 0;
  while (left <= right) {
    uint32_t mid = (left + right + 1) / 2;

    uint64_t offset_to_read = rep_->index_start_offset
        + (rep::getinternalkeylength() + rep::offset_length) * mid;
    status s = rep_->file->read(offset_to_read, rep::getinternalkeylength(),
                                &tmp_slice, key_chars);
    if (!s.ok()) {
      return s;
    }

    internalkeycomparator ikc(rep_->options.comparator);
    int compare_result = ikc.compare(tmp_slice, target);

    if (compare_result < 0) {
      if (left == right) {
        target_offset = right + 1;
        break;
      }
      left = mid;
    } else {
      if (left == right) {
        target_offset = left;
        break;
      }
      right = mid - 1;
    }
  }

  if (target_offset >= (uint32_t) rep_->num_entries) {
    *offset = rep_->index_start_offset;
    return status::ok();
  }

  char value_offset_chars[rep::offset_length];

  int64_t offset_for_value_offset = rep_->index_start_offset
      + (rep::getinternalkeylength() + rep::offset_length) * target_offset
      + rep::getinternalkeylength();
  status s = rep_->file->read(offset_for_value_offset, rep::offset_length,
                              &tmp_slice, value_offset_chars);
  if (s.ok()) {
    *offset = decodefixed64(value_offset_chars);
  }
  return s;
}

status simpletablereader::get(const readoptions& options, const slice& k,
                              void* arg,
                              bool (*saver)(void*, const parsedinternalkey&,
                                            const slice&),
                              void (*mark_key_may_exist)(void*)) {
  status s;
  simpletableiterator* iter = new simpletableiterator(this);
  for (iter->seek(k); iter->valid(); iter->next()) {
    parsedinternalkey parsed_key;
    if (!parseinternalkey(iter->key(), &parsed_key)) {
      return status::corruption(slice());
    }

    if (!(*saver)(arg, parsed_key, iter->value())) {
      break;
    }
  }
  s = iter->status();
  delete iter;
  return s;
}

uint64_t simpletablereader::approximateoffsetof(const slice& key) {
  return 0;
}

simpletableiterator::simpletableiterator(simpletablereader* table) :
    table_(table) {
  key_str_ = new char[simpletablereader::rep::getinternalkeylength()];
  value_str_len_ = -1;
  seektofirst();
}

simpletableiterator::~simpletableiterator() {
 delete[] key_str_;
 if (value_str_len_ >= 0) {
   delete[] value_str_;
 }
}

bool simpletableiterator::valid() const {
  return offset_ < table_->rep_->index_start_offset;
}

void simpletableiterator::seektofirst() {
  next_offset_ = 0;
  next();
}

void simpletableiterator::seektolast() {
  assert(false);
}

void simpletableiterator::seek(const slice& target) {
  status s = table_->getoffset(target, &next_offset_);
  if (!s.ok()) {
    status_ = s;
  }
  next();
}

void simpletableiterator::next() {
  offset_ = next_offset_;
  if (offset_ >= table_->rep_->index_start_offset) {
    return;
  }
  slice result;
  int internal_key_size = simpletablereader::rep::getinternalkeylength();

  status s = table_->rep_->file->read(next_offset_, internal_key_size, &result,
                                      key_str_);
  next_offset_ += internal_key_size;
  key_ = result;

  slice value_size_slice;
  s = table_->rep_->file->read(next_offset_, 4, &value_size_slice, tmp_str_);
  next_offset_ += 4;
  uint32_t value_size = decodefixed32(tmp_str_);

  slice value_slice;
  if ((int) value_size > value_str_len_) {
    if (value_str_len_ >= 0) {
      delete[] value_str_;
    }
    value_str_ = new char[value_size];
    value_str_len_ = value_size;
  }
  s = table_->rep_->file->read(next_offset_, value_size, &value_slice,
                               value_str_);
  next_offset_ += value_size;
  value_ = value_slice;
}

void simpletableiterator::prev() {
  assert(false);
}

slice simpletableiterator::key() const {
  log(table_->rep_->options.info_log, "key!!!!");
  return key_;
}

slice simpletableiterator::value() const {
  return value_;
}

status simpletableiterator::status() const {
  return status_;
}

class simpletablebuilder: public tablebuilder {
public:
  // create a builder that will store the contents of the table it is
  // building in *file.  does not close the file.  it is up to the
  // caller to close the file after calling finish(). the output file
  // will be part of level specified by 'level'.  a value of -1 means
  // that the caller does not know which level the output file will reside.
  simpletablebuilder(const options& options, writablefile* file,
                     compressiontype compression_type);

  // requires: either finish() or abandon() has been called.
  ~simpletablebuilder();

  // add key,value to the table being constructed.
  // requires: key is after any previously added key according to comparator.
  // requires: finish(), abandon() have not been called
  void add(const slice& key, const slice& value) override;

  // return non-ok iff some error has been detected.
  status status() const override;

  // finish building the table.  stops using the file passed to the
  // constructor after this function returns.
  // requires: finish(), abandon() have not been called
  status finish() override;

  // indicate that the contents of this builder should be abandoned.  stops
  // using the file passed to the constructor after this function returns.
  // if the caller is not going to call finish(), it must call abandon()
  // before destroying this builder.
  // requires: finish(), abandon() have not been called
  void abandon() override;

  // number of calls to add() so far.
  uint64_t numentries() const override;

  // size of the file generated so far.  if invoked after a successful
  // finish() call, returns the size of the final generated file.
  uint64_t filesize() const override;

private:
  struct rep;
  rep* rep_;

  // no copying allowed
  simpletablebuilder(const simpletablebuilder&) = delete;
  void operator=(const simpletablebuilder&) = delete;
};

struct simpletablebuilder::rep {
  options options;
  writablefile* file;
  uint64_t offset = 0;
  status status;

  uint64_t num_entries = 0;

  bool closed = false;  // either finish() or abandon() has been called.

  const static int user_key_size = 16;
  const static int offset_length = 8;
  const static int key_footer_len = 8;

  static int getinternalkeylength() {
    return user_key_size + key_footer_len;
  }

  std::string index;

  rep(const options& opt, writablefile* f) :
      options(opt), file(f) {
  }
  ~rep() {
  }
};

simpletablebuilder::simpletablebuilder(const options& options,
                                       writablefile* file,
                                       compressiontype compression_type) :
    rep_(new simpletablebuilder::rep(options, file)) {
}

simpletablebuilder::~simpletablebuilder() {
  delete (rep_);
}

void simpletablebuilder::add(const slice& key, const slice& value) {
  assert((int ) key.size() == rep::getinternalkeylength());

  // update index
  rep_->index.append(key.data(), key.size());
  putfixed64(&(rep_->index), rep_->offset);

  // write key-value pair
  rep_->file->append(key);
  rep_->offset += rep::getinternalkeylength();

  std::string size;
  int value_size = value.size();
  putfixed32(&size, value_size);
  slice sizeslice(size);
  rep_->file->append(sizeslice);
  rep_->file->append(value);
  rep_->offset += value_size + 4;

  rep_->num_entries++;
}

status simpletablebuilder::status() const {
  return status::ok();
}

status simpletablebuilder::finish() {
  rep* r = rep_;
  assert(!r->closed);
  r->closed = true;

  uint64_t index_offset = rep_->offset;
  slice index_slice(rep_->index);
  rep_->file->append(index_slice);
  rep_->offset += index_slice.size();

  std::string index_offset_str;
  putfixed64(&index_offset_str, index_offset);
  slice foot_slice(index_offset_str);
  rep_->file->append(foot_slice);
  rep_->offset += foot_slice.size();

  return status::ok();
}

void simpletablebuilder::abandon() {
  rep_->closed = true;
}

uint64_t simpletablebuilder::numentries() const {
  return rep_->num_entries;
}

uint64_t simpletablebuilder::filesize() const {
  return rep_->offset;
}

class simpletablefactory: public tablefactory {
public:
  ~simpletablefactory() {
  }
  simpletablefactory() {
  }
  const char* name() const override {
    return "simpletable";
  }
  status newtablereader(const options& options, const envoptions& soptions,
                        const internalkeycomparator& internal_key,
                        unique_ptr<randomaccessfile>&& file, uint64_t file_size,
                        unique_ptr<tablereader>* table_reader) const;

  tablebuilder* newtablebuilder(const options& options,
                                const internalkeycomparator& internal_key,
                                writablefile* file,
                                compressiontype compression_type) const;

  virtual status sanitizedboptions(const dboptions* db_opts) const override {
    return status::ok();
  }

  virtual std::string getprintabletableoptions() const override {
    return std::string();
  }
};

status simpletablefactory::newtablereader(
    const options& options, const envoptions& soptions,
    const internalkeycomparator& internal_key,
    unique_ptr<randomaccessfile>&& file, uint64_t file_size,
    unique_ptr<tablereader>* table_reader) const {

  return simpletablereader::open(options, soptions, std::move(file), file_size,
                                 table_reader);
}

tablebuilder* simpletablefactory::newtablebuilder(
    const options& options, const internalkeycomparator& internal_key,
    writablefile* file, compressiontype compression_type) const {
  return new simpletablebuilder(options, file, compression_type);
}

class simpletabledbtest {
protected:
public:
  std::string dbname_;
  env* env_;
  db* db_;

  options last_options_;

  simpletabledbtest() :
      env_(env::default()) {
    dbname_ = test::tmpdir() + "/simple_table_db_test";
    assert_ok(destroydb(dbname_, options()));
    db_ = nullptr;
    reopen();
  }

  ~simpletabledbtest() {
    delete db_;
    assert_ok(destroydb(dbname_, options()));
  }

  // return the current option configuration.
  options currentoptions() {
    options options;
    options.table_factory.reset(new simpletablefactory());
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

test(simpletabledbtest, empty) {
  assert_true(db_ != nullptr);
  assert_eq("not_found", get("0000000000000foo"));
}

test(simpletabledbtest, readwrite) {
  assert_ok(put("0000000000000foo", "v1"));
  assert_eq("v1", get("0000000000000foo"));
  assert_ok(put("0000000000000bar", "v2"));
  assert_ok(put("0000000000000foo", "v3"));
  assert_eq("v3", get("0000000000000foo"));
  assert_eq("v2", get("0000000000000bar"));
}

test(simpletabledbtest, flush) {
  assert_ok(put("0000000000000foo", "v1"));
  assert_ok(put("0000000000000bar", "v2"));
  assert_ok(put("0000000000000foo", "v3"));
  dbfull()->test_flushmemtable();
  assert_eq("v3", get("0000000000000foo"));
  assert_eq("v2", get("0000000000000bar"));
}

test(simpletabledbtest, flush2) {
  assert_ok(put("0000000000000bar", "b"));
  assert_ok(put("0000000000000foo", "v1"));
  dbfull()->test_flushmemtable();

  assert_ok(put("0000000000000foo", "v2"));
  dbfull()->test_flushmemtable();
  assert_eq("v2", get("0000000000000foo"));

  assert_ok(put("0000000000000eee", "v3"));
  dbfull()->test_flushmemtable();
  assert_eq("v3", get("0000000000000eee"));

  assert_ok(delete("0000000000000bar"));
  dbfull()->test_flushmemtable();
  assert_eq("not_found", get("0000000000000bar"));

  assert_ok(put("0000000000000eee", "v5"));
  dbfull()->test_flushmemtable();
  assert_eq("v5", get("0000000000000eee"));
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

test(simpletabledbtest, compactiontrigger) {
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

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
