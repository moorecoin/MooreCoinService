//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include <string>
#include <vector>
#include <algorithm>
#include <utility>

#include "db/dbformat.h"
#include "rocksdb/comparator.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/statistics.h"
#include "db/db_iter.h"
#include "util/testharness.h"
#include "utilities/merge_operators.h"

namespace rocksdb {

static uint32_t testgettickercount(const options& options,
                                   tickers ticker_type) {
  return options.statistics->gettickercount(ticker_type);
}

class testiterator : public iterator {
 public:
  explicit testiterator(const comparator* comparator)
      : initialized_(false),
        valid_(false),
        sequence_number_(0),
        iter_(0),
        cmp(comparator) {}

  void addmerge(std::string key, std::string value) {
    add(key, ktypemerge, value);
  }

  void adddeletion(std::string key) { add(key, ktypedeletion, std::string()); }

  void addput(std::string key, std::string value) {
    add(key, ktypevalue, value);
  }

  void add(std::string key, valuetype type, std::string value) {
    valid_ = true;
    parsedinternalkey internal_key(key, sequence_number_++, type);
    data_.push_back(std::pair<std::string, std::string>(std::string(), value));
    appendinternalkey(&data_.back().first, internal_key);
  }

  // should be called before operations with iterator
  void finish() {
    initialized_ = true;
    std::sort(data_.begin(), data_.end(),
              [this](std::pair<std::string, std::string> a,
                     std::pair<std::string, std::string> b) {
      return (cmp.compare(a.first, b.first) < 0);
    });
  }

  virtual bool valid() const override {
    assert(initialized_);
    return valid_;
  }

  virtual void seektofirst() override {
    assert(initialized_);
    valid_ = (data_.size() > 0);
    iter_ = 0;
  }

  virtual void seektolast() override {
    assert(initialized_);
    valid_ = (data_.size() > 0);
    iter_ = data_.size() - 1;
  }

  virtual void seek(const slice& target) override {
    assert(initialized_);
    seektofirst();
    if (!valid_) {
      return;
    }
    while (iter_ < data_.size() &&
           (cmp.compare(data_[iter_].first, target) < 0)) {
      ++iter_;
    }

    if (iter_ == data_.size()) {
      valid_ = false;
    }
  }

  virtual void next() override {
    assert(initialized_);
    if (data_.empty() || (iter_ == data_.size() - 1)) {
      valid_ = false;
    } else {
      ++iter_;
    }
  }

  virtual void prev() override {
    assert(initialized_);
    if (iter_ == 0) {
      valid_ = false;
    } else {
      --iter_;
    }
  }

  virtual slice key() const override {
    assert(initialized_);
    return data_[iter_].first;
  }

  virtual slice value() const override {
    assert(initialized_);
    return data_[iter_].second;
  }

  virtual status status() const override {
    assert(initialized_);
    return status::ok();
  }

 private:
  bool initialized_;
  bool valid_;
  size_t sequence_number_;
  size_t iter_;

  internalkeycomparator cmp;
  std::vector<std::pair<std::string, std::string>> data_;
};

class dbiteratortest {
 public:
  env* env_;

  dbiteratortest() : env_(env::default()) {}
};

test(dbiteratortest, dbiteratorprevnext) {
  options options;

  {
    testiterator* internal_iter = new testiterator(bytewisecomparator());
    internal_iter->adddeletion("a");
    internal_iter->adddeletion("a");
    internal_iter->adddeletion("a");
    internal_iter->adddeletion("a");
    internal_iter->addput("a", "val_a");

    internal_iter->addput("b", "val_b");
    internal_iter->finish();

    std::unique_ptr<iterator> db_iter(
        newdbiterator(env_, options, bytewisecomparator(), internal_iter, 10));

    db_iter->seektolast();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "b");
    assert_eq(db_iter->value().tostring(), "val_b");

    db_iter->prev();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "a");
    assert_eq(db_iter->value().tostring(), "val_a");

    db_iter->next();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "b");
    assert_eq(db_iter->value().tostring(), "val_b");

    db_iter->next();
    assert_true(!db_iter->valid());
  }

  {
    testiterator* internal_iter = new testiterator(bytewisecomparator());
    internal_iter->adddeletion("a");
    internal_iter->adddeletion("a");
    internal_iter->adddeletion("a");
    internal_iter->adddeletion("a");
    internal_iter->addput("a", "val_a");

    internal_iter->addput("b", "val_b");
    internal_iter->finish();

    std::unique_ptr<iterator> db_iter(
        newdbiterator(env_, options, bytewisecomparator(), internal_iter, 10));

    db_iter->seektofirst();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "a");
    assert_eq(db_iter->value().tostring(), "val_a");

    db_iter->next();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "b");
    assert_eq(db_iter->value().tostring(), "val_b");

    db_iter->prev();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "a");
    assert_eq(db_iter->value().tostring(), "val_a");

    db_iter->prev();
    assert_true(!db_iter->valid());
  }

  {
    options options;
    testiterator* internal_iter = new testiterator(bytewisecomparator());
    internal_iter->addput("a", "val_a");
    internal_iter->addput("b", "val_b");

    internal_iter->addput("a", "val_a");
    internal_iter->addput("b", "val_b");

    internal_iter->addput("a", "val_a");
    internal_iter->addput("b", "val_b");

    internal_iter->addput("a", "val_a");
    internal_iter->addput("b", "val_b");

    internal_iter->addput("a", "val_a");
    internal_iter->addput("b", "val_b");
    internal_iter->finish();

    std::unique_ptr<iterator> db_iter(
        newdbiterator(env_, options, bytewisecomparator(), internal_iter, 2));
    db_iter->seektolast();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "b");
    assert_eq(db_iter->value().tostring(), "val_b");

    db_iter->next();
    assert_true(!db_iter->valid());

    db_iter->seektolast();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "b");
    assert_eq(db_iter->value().tostring(), "val_b");
  }

  {
    options options;
    testiterator* internal_iter = new testiterator(bytewisecomparator());
    internal_iter->addput("a", "val_a");
    internal_iter->addput("a", "val_a");
    internal_iter->addput("a", "val_a");
    internal_iter->addput("a", "val_a");
    internal_iter->addput("a", "val_a");

    internal_iter->addput("b", "val_b");

    internal_iter->addput("c", "val_c");
    internal_iter->finish();

    std::unique_ptr<iterator> db_iter(
        newdbiterator(env_, options, bytewisecomparator(), internal_iter, 10));
    db_iter->seektolast();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "c");
    assert_eq(db_iter->value().tostring(), "val_c");

    db_iter->prev();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "b");
    assert_eq(db_iter->value().tostring(), "val_b");

    db_iter->next();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "c");
    assert_eq(db_iter->value().tostring(), "val_c");
  }
}

test(dbiteratortest, dbiteratorempty) {
  options options;

  {
    testiterator* internal_iter = new testiterator(bytewisecomparator());
    internal_iter->finish();

    std::unique_ptr<iterator> db_iter(
        newdbiterator(env_, options, bytewisecomparator(), internal_iter, 0));
    db_iter->seektolast();
    assert_true(!db_iter->valid());
  }

  {
    testiterator* internal_iter = new testiterator(bytewisecomparator());
    internal_iter->finish();

    std::unique_ptr<iterator> db_iter(
        newdbiterator(env_, options, bytewisecomparator(), internal_iter, 0));
    db_iter->seektofirst();
    assert_true(!db_iter->valid());
  }
}

test(dbiteratortest, dbiteratoruseskipcountskips) {
  options options;
  options.statistics = rocksdb::createdbstatistics();
  options.merge_operator = mergeoperators::createfromstringid("stringappend");

  testiterator* internal_iter = new testiterator(bytewisecomparator());
  for (size_t i = 0; i < 200; ++i) {
    internal_iter->addput("a", "a");
    internal_iter->addput("b", "b");
    internal_iter->addput("c", "c");
  }
  internal_iter->finish();

  std::unique_ptr<iterator> db_iter(
      newdbiterator(env_, options, bytewisecomparator(), internal_iter, 2));
  db_iter->seektolast();
  assert_true(db_iter->valid());
  assert_eq(db_iter->key().tostring(), "c");
  assert_eq(db_iter->value().tostring(), "c");
  assert_eq(testgettickercount(options, number_of_reseeks_in_iteration), 1u);

  db_iter->prev();
  assert_true(db_iter->valid());
  assert_eq(db_iter->key().tostring(), "b");
  assert_eq(db_iter->value().tostring(), "b");
  assert_eq(testgettickercount(options, number_of_reseeks_in_iteration), 2u);

  db_iter->prev();
  assert_true(db_iter->valid());
  assert_eq(db_iter->key().tostring(), "a");
  assert_eq(db_iter->value().tostring(), "a");
  assert_eq(testgettickercount(options, number_of_reseeks_in_iteration), 3u);

  db_iter->prev();
  assert_true(!db_iter->valid());
  assert_eq(testgettickercount(options, number_of_reseeks_in_iteration), 3u);
}

test(dbiteratortest, dbiteratoruseskip) {
  options options;
  options.merge_operator = mergeoperators::createfromstringid("stringappend");
  {
    for (size_t i = 0; i < 200; ++i) {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("b", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      for (size_t i = 0; i < 200; ++i) {
        internal_iter->addput("c", std::to_string(i));
      }
      internal_iter->finish();

      options.statistics = rocksdb::createdbstatistics();
      std::unique_ptr<iterator> db_iter(newdbiterator(
          env_, options, bytewisecomparator(), internal_iter, i + 2));
      db_iter->seektolast();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "c");
      assert_eq(db_iter->value().tostring(), std::to_string(i));
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "b");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_2");
      db_iter->prev();

      assert_true(!db_iter->valid());
    }
  }

  {
    for (size_t i = 0; i < 200; ++i) {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("b", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      for (size_t i = 0; i < 200; ++i) {
        internal_iter->adddeletion("c");
      }
      internal_iter->addput("c", "200");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(newdbiterator(
          env_, options, bytewisecomparator(), internal_iter, i + 2));
      db_iter->seektolast();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "b");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_2");
      db_iter->prev();

      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("b", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      for (size_t i = 0; i < 200; ++i) {
        internal_iter->adddeletion("c");
      }
      internal_iter->addput("c", "200");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(newdbiterator(
          env_, options, bytewisecomparator(), internal_iter, 202));
      db_iter->seektolast();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "c");
      assert_eq(db_iter->value().tostring(), "200");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "b");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_2");
      db_iter->prev();

      assert_true(!db_iter->valid());
    }
  }

  {
    for (size_t i = 0; i < 200; ++i) {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      for (size_t i = 0; i < 200; ++i) {
        internal_iter->adddeletion("c");
      }
      internal_iter->addput("c", "200");
      internal_iter->finish();
      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, i));
      db_iter->seektolast();
      assert_true(!db_iter->valid());

      db_iter->seektofirst();
      assert_true(!db_iter->valid());
    }

    testiterator* internal_iter = new testiterator(bytewisecomparator());
    for (size_t i = 0; i < 200; ++i) {
      internal_iter->adddeletion("c");
    }
    internal_iter->addput("c", "200");
    internal_iter->finish();
    std::unique_ptr<iterator> db_iter(
        newdbiterator(env_, options, bytewisecomparator(), internal_iter, 200));
    db_iter->seektolast();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "c");
    assert_eq(db_iter->value().tostring(), "200");

    db_iter->prev();
    assert_true(!db_iter->valid());

    db_iter->seektofirst();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "c");
    assert_eq(db_iter->value().tostring(), "200");

    db_iter->next();
    assert_true(!db_iter->valid());
  }

  {
    for (size_t i = 0; i < 200; ++i) {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("b", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      for (size_t i = 0; i < 200; ++i) {
        internal_iter->addput("d", std::to_string(i));
      }

      for (size_t i = 0; i < 200; ++i) {
        internal_iter->addput("c", std::to_string(i));
      }
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(newdbiterator(
          env_, options, bytewisecomparator(), internal_iter, i + 2));
      db_iter->seektolast();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "d");
      assert_eq(db_iter->value().tostring(), std::to_string(i));
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "b");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_2");
      db_iter->prev();

      assert_true(!db_iter->valid());
    }
  }

  {
    for (size_t i = 0; i < 200; ++i) {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("b", "b");
      internal_iter->addmerge("a", "a");
      for (size_t i = 0; i < 200; ++i) {
        internal_iter->addmerge("c", std::to_string(i));
      }
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(newdbiterator(
          env_, options, bytewisecomparator(), internal_iter, i + 2));
      db_iter->seektolast();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "c");
      std::string merge_result = "0";
      for (size_t j = 1; j <= i; ++j) {
        merge_result += "," + std::to_string(j);
      }
      assert_eq(db_iter->value().tostring(), merge_result);

      db_iter->prev();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "b");
      assert_eq(db_iter->value().tostring(), "b");

      db_iter->prev();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "a");

      db_iter->prev();
      assert_true(!db_iter->valid());
    }
  }
}

test(dbiteratortest, dbiterator) {
  options options;
  options.merge_operator = mergeoperators::createfromstringid("stringappend");
  {
    testiterator* internal_iter = new testiterator(bytewisecomparator());
    internal_iter->addput("a", "0");
    internal_iter->addput("b", "0");
    internal_iter->adddeletion("b");
    internal_iter->addmerge("a", "1");
    internal_iter->addmerge("b", "2");
    internal_iter->finish();

    std::unique_ptr<iterator> db_iter(
        newdbiterator(env_, options, bytewisecomparator(), internal_iter, 1));
    db_iter->seektofirst();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "a");
    assert_eq(db_iter->value().tostring(), "0");
    db_iter->next();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "b");
  }

  {
    testiterator* internal_iter = new testiterator(bytewisecomparator());
    internal_iter->addput("a", "0");
    internal_iter->addput("b", "0");
    internal_iter->adddeletion("b");
    internal_iter->addmerge("a", "1");
    internal_iter->addmerge("b", "2");
    internal_iter->finish();

    std::unique_ptr<iterator> db_iter(
        newdbiterator(env_, options, bytewisecomparator(), internal_iter, 0));
    db_iter->seektofirst();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "a");
    assert_eq(db_iter->value().tostring(), "0");
    db_iter->next();
    assert_true(!db_iter->valid());
  }

  {
    testiterator* internal_iter = new testiterator(bytewisecomparator());
    internal_iter->addput("a", "0");
    internal_iter->addput("b", "0");
    internal_iter->adddeletion("b");
    internal_iter->addmerge("a", "1");
    internal_iter->addmerge("b", "2");
    internal_iter->finish();

    std::unique_ptr<iterator> db_iter(
        newdbiterator(env_, options, bytewisecomparator(), internal_iter, 2));
    db_iter->seektofirst();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "a");
    assert_eq(db_iter->value().tostring(), "0");
    db_iter->next();
    assert_true(!db_iter->valid());
  }

  {
    testiterator* internal_iter = new testiterator(bytewisecomparator());
    internal_iter->addput("a", "0");
    internal_iter->addput("b", "0");
    internal_iter->adddeletion("b");
    internal_iter->addmerge("a", "1");
    internal_iter->addmerge("b", "2");
    internal_iter->finish();

    std::unique_ptr<iterator> db_iter(
        newdbiterator(env_, options, bytewisecomparator(), internal_iter, 4));
    db_iter->seektofirst();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "a");
    assert_eq(db_iter->value().tostring(), "0,1");
    db_iter->next();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "b");
    assert_eq(db_iter->value().tostring(), "2");
    db_iter->next();
    assert_true(!db_iter->valid());
  }

  {
    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->addput("a", "put_1");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 0));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->addput("a", "put_1");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 1));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1,merge_2");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->addput("a", "put_1");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 2));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1,merge_2,merge_3");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->addput("a", "put_1");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 3));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "put_1");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->addput("a", "put_1");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 4));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "put_1,merge_4");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->addput("a", "put_1");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 5));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "put_1,merge_4,merge_5");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->addput("a", "put_1");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 6));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "put_1,merge_4,merge_5,merge_6");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }
  }

  {
    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->adddeletion("a");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 0));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->adddeletion("a");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 1));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1,merge_2");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->adddeletion("a");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 2));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1,merge_2,merge_3");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->adddeletion("a");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 3));
      db_iter->seektolast();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->adddeletion("a");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 4));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_4");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->adddeletion("a");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 5));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_4,merge_5");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addmerge("a", "merge_2");
      internal_iter->addmerge("a", "merge_3");
      internal_iter->adddeletion("a");
      internal_iter->addmerge("a", "merge_4");
      internal_iter->addmerge("a", "merge_5");
      internal_iter->addmerge("a", "merge_6");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 6));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_4,merge_5,merge_6");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }
  }

  {
    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addput("b", "val");
      internal_iter->addmerge("b", "merge_2");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_3");

      internal_iter->addmerge("c", "merge_4");
      internal_iter->addmerge("c", "merge_5");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_6");
      internal_iter->addmerge("b", "merge_7");
      internal_iter->addmerge("b", "merge_8");
      internal_iter->addmerge("b", "merge_9");
      internal_iter->addmerge("b", "merge_10");
      internal_iter->addmerge("b", "merge_11");

      internal_iter->adddeletion("c");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 0));
      db_iter->seektolast();
      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addput("b", "val");
      internal_iter->addmerge("b", "merge_2");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_3");

      internal_iter->addmerge("c", "merge_4");
      internal_iter->addmerge("c", "merge_5");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_6");
      internal_iter->addmerge("b", "merge_7");
      internal_iter->addmerge("b", "merge_8");
      internal_iter->addmerge("b", "merge_9");
      internal_iter->addmerge("b", "merge_10");
      internal_iter->addmerge("b", "merge_11");

      internal_iter->adddeletion("c");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 2));
      db_iter->seektolast();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "b");
      assert_eq(db_iter->value().tostring(), "val,merge_2");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addput("b", "val");
      internal_iter->addmerge("b", "merge_2");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_3");

      internal_iter->addmerge("c", "merge_4");
      internal_iter->addmerge("c", "merge_5");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_6");
      internal_iter->addmerge("b", "merge_7");
      internal_iter->addmerge("b", "merge_8");
      internal_iter->addmerge("b", "merge_9");
      internal_iter->addmerge("b", "merge_10");
      internal_iter->addmerge("b", "merge_11");

      internal_iter->adddeletion("c");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 4));
      db_iter->seektolast();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "b");
      assert_eq(db_iter->value().tostring(), "merge_3");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addput("b", "val");
      internal_iter->addmerge("b", "merge_2");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_3");

      internal_iter->addmerge("c", "merge_4");
      internal_iter->addmerge("c", "merge_5");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_6");
      internal_iter->addmerge("b", "merge_7");
      internal_iter->addmerge("b", "merge_8");
      internal_iter->addmerge("b", "merge_9");
      internal_iter->addmerge("b", "merge_10");
      internal_iter->addmerge("b", "merge_11");

      internal_iter->adddeletion("c");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 5));
      db_iter->seektolast();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "c");
      assert_eq(db_iter->value().tostring(), "merge_4");
      db_iter->prev();

      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "b");
      assert_eq(db_iter->value().tostring(), "merge_3");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addput("b", "val");
      internal_iter->addmerge("b", "merge_2");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_3");

      internal_iter->addmerge("c", "merge_4");
      internal_iter->addmerge("c", "merge_5");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_6");
      internal_iter->addmerge("b", "merge_7");
      internal_iter->addmerge("b", "merge_8");
      internal_iter->addmerge("b", "merge_9");
      internal_iter->addmerge("b", "merge_10");
      internal_iter->addmerge("b", "merge_11");

      internal_iter->adddeletion("c");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 6));
      db_iter->seektolast();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "c");
      assert_eq(db_iter->value().tostring(), "merge_4,merge_5");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "b");
      assert_eq(db_iter->value().tostring(), "merge_3");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addput("b", "val");
      internal_iter->addmerge("b", "merge_2");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_3");

      internal_iter->addmerge("c", "merge_4");
      internal_iter->addmerge("c", "merge_5");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_6");
      internal_iter->addmerge("b", "merge_7");
      internal_iter->addmerge("b", "merge_8");
      internal_iter->addmerge("b", "merge_9");
      internal_iter->addmerge("b", "merge_10");
      internal_iter->addmerge("b", "merge_11");

      internal_iter->adddeletion("c");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 7));
      db_iter->seektolast();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "c");
      assert_eq(db_iter->value().tostring(), "merge_4,merge_5");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addput("b", "val");
      internal_iter->addmerge("b", "merge_2");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_3");

      internal_iter->addmerge("c", "merge_4");
      internal_iter->addmerge("c", "merge_5");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_6");
      internal_iter->addmerge("b", "merge_7");
      internal_iter->addmerge("b", "merge_8");
      internal_iter->addmerge("b", "merge_9");
      internal_iter->addmerge("b", "merge_10");
      internal_iter->addmerge("b", "merge_11");

      internal_iter->adddeletion("c");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(
          newdbiterator(env_, options, bytewisecomparator(), internal_iter, 9));
      db_iter->seektolast();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "c");
      assert_eq(db_iter->value().tostring(), "merge_4,merge_5");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "b");
      assert_eq(db_iter->value().tostring(), "merge_6,merge_7");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addput("b", "val");
      internal_iter->addmerge("b", "merge_2");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_3");

      internal_iter->addmerge("c", "merge_4");
      internal_iter->addmerge("c", "merge_5");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_6");
      internal_iter->addmerge("b", "merge_7");
      internal_iter->addmerge("b", "merge_8");
      internal_iter->addmerge("b", "merge_9");
      internal_iter->addmerge("b", "merge_10");
      internal_iter->addmerge("b", "merge_11");

      internal_iter->adddeletion("c");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(newdbiterator(
          env_, options, bytewisecomparator(), internal_iter, 13));
      db_iter->seektolast();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "c");
      assert_eq(db_iter->value().tostring(), "merge_4,merge_5");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_true(db_iter->valid());
      assert_eq(db_iter->key().tostring(), "b");
      assert_eq(db_iter->value().tostring(),
                "merge_6,merge_7,merge_8,merge_9,merge_10,merge_11");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }

    {
      testiterator* internal_iter = new testiterator(bytewisecomparator());
      internal_iter->addmerge("a", "merge_1");
      internal_iter->addput("b", "val");
      internal_iter->addmerge("b", "merge_2");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_3");

      internal_iter->addmerge("c", "merge_4");
      internal_iter->addmerge("c", "merge_5");

      internal_iter->adddeletion("b");
      internal_iter->addmerge("b", "merge_6");
      internal_iter->addmerge("b", "merge_7");
      internal_iter->addmerge("b", "merge_8");
      internal_iter->addmerge("b", "merge_9");
      internal_iter->addmerge("b", "merge_10");
      internal_iter->addmerge("b", "merge_11");

      internal_iter->adddeletion("c");
      internal_iter->finish();

      std::unique_ptr<iterator> db_iter(newdbiterator(
          env_, options, bytewisecomparator(), internal_iter, 14));
      db_iter->seektolast();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "b");
      assert_eq(db_iter->value().tostring(),
                "merge_6,merge_7,merge_8,merge_9,merge_10,merge_11");
      db_iter->prev();
      assert_true(db_iter->valid());

      assert_eq(db_iter->key().tostring(), "a");
      assert_eq(db_iter->value().tostring(), "merge_1");
      db_iter->prev();
      assert_true(!db_iter->valid());
    }
  }

  {
    options options;
    testiterator* internal_iter = new testiterator(bytewisecomparator());
    internal_iter->adddeletion("a");
    internal_iter->addput("a", "0");
    internal_iter->addput("b", "0");
    internal_iter->finish();

    std::unique_ptr<iterator> db_iter(
        newdbiterator(env_, options, bytewisecomparator(), internal_iter, 10));
    db_iter->seektolast();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "b");
    assert_eq(db_iter->value().tostring(), "0");

    db_iter->prev();
    assert_true(db_iter->valid());
    assert_eq(db_iter->key().tostring(), "a");
    assert_eq(db_iter->value().tostring(), "0");
  }
}

}  // namespace rocksdb

int main(int argc, char** argv) { return rocksdb::test::runalltests(); }
