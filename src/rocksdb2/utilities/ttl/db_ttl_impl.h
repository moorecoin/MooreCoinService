// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once

#ifndef rocksdb_lite
#include <deque>
#include <string>
#include <vector>

#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/merge_operator.h"
#include "rocksdb/utilities/utility_db.h"
#include "rocksdb/utilities/db_ttl.h"
#include "db/db_impl.h"

namespace rocksdb {

class dbwithttlimpl : public dbwithttl {
 public:
  static void sanitizeoptions(int32_t ttl, columnfamilyoptions* options,
                              env* env);

  explicit dbwithttlimpl(db* db);

  virtual ~dbwithttlimpl();

  status createcolumnfamilywithttl(const columnfamilyoptions& options,
                                   const std::string& column_family_name,
                                   columnfamilyhandle** handle,
                                   int ttl) override;

  status createcolumnfamily(const columnfamilyoptions& options,
                            const std::string& column_family_name,
                            columnfamilyhandle** handle) override;

  using stackabledb::put;
  virtual status put(const writeoptions& options,
                     columnfamilyhandle* column_family, const slice& key,
                     const slice& val) override;

  using stackabledb::get;
  virtual status get(const readoptions& options,
                     columnfamilyhandle* column_family, const slice& key,
                     std::string* value) override;

  using stackabledb::multiget;
  virtual std::vector<status> multiget(
      const readoptions& options,
      const std::vector<columnfamilyhandle*>& column_family,
      const std::vector<slice>& keys,
      std::vector<std::string>* values) override;

  using stackabledb::keymayexist;
  virtual bool keymayexist(const readoptions& options,
                           columnfamilyhandle* column_family, const slice& key,
                           std::string* value,
                           bool* value_found = nullptr) override;

  using stackabledb::merge;
  virtual status merge(const writeoptions& options,
                       columnfamilyhandle* column_family, const slice& key,
                       const slice& value) override;

  virtual status write(const writeoptions& opts, writebatch* updates) override;

  using stackabledb::newiterator;
  virtual iterator* newiterator(const readoptions& opts,
                                columnfamilyhandle* column_family) override;

  virtual db* getbasedb() { return db_; }

  static bool isstale(const slice& value, int32_t ttl, env* env);

  static status appendts(const slice& val, std::string* val_with_ts, env* env);

  static status sanitychecktimestamp(const slice& str);

  static status stripts(std::string* str);

  static const uint32_t ktslength = sizeof(int32_t);  // size of timestamp

  static const int32_t kmintimestamp = 1368146402;  // 05/09/2013:5:40pm gmt-8

  static const int32_t kmaxtimestamp = 2147483647;  // 01/18/2038:7:14pm gmt-8
};

class ttliterator : public iterator {

 public:
  explicit ttliterator(iterator* iter) : iter_(iter) { assert(iter_); }

  ~ttliterator() { delete iter_; }

  bool valid() const { return iter_->valid(); }

  void seektofirst() { iter_->seektofirst(); }

  void seektolast() { iter_->seektolast(); }

  void seek(const slice& target) { iter_->seek(target); }

  void next() { iter_->next(); }

  void prev() { iter_->prev(); }

  slice key() const { return iter_->key(); }

  int32_t timestamp() const {
    return decodefixed32(iter_->value().data() + iter_->value().size() -
                         dbwithttlimpl::ktslength);
  }

  slice value() const {
    // todo: handle timestamp corruption like in general iterator semantics
    assert(dbwithttlimpl::sanitychecktimestamp(iter_->value()).ok());
    slice trimmed_value = iter_->value();
    trimmed_value.size_ -= dbwithttlimpl::ktslength;
    return trimmed_value;
  }

  status status() const { return iter_->status(); }

 private:
  iterator* iter_;
};

class ttlcompactionfilter : public compactionfilter {
 public:
  ttlcompactionfilter(
      int32_t ttl, env* env, const compactionfilter* user_comp_filter,
      std::unique_ptr<const compactionfilter> user_comp_filter_from_factory =
          nullptr)
      : ttl_(ttl),
        env_(env),
        user_comp_filter_(user_comp_filter),
        user_comp_filter_from_factory_(
            std::move(user_comp_filter_from_factory)) {
    // unlike the merge operator, compaction filter is necessary for ttl, hence
    // this would be called even if user doesn't specify any compaction-filter
    if (!user_comp_filter_) {
      user_comp_filter_ = user_comp_filter_from_factory_.get();
    }
  }

  virtual bool filter(int level, const slice& key, const slice& old_val,
                      std::string* new_val, bool* value_changed) const
      override {
    if (dbwithttlimpl::isstale(old_val, ttl_, env_)) {
      return true;
    }
    if (user_comp_filter_ == nullptr) {
      return false;
    }
    assert(old_val.size() >= dbwithttlimpl::ktslength);
    slice old_val_without_ts(old_val.data(),
                             old_val.size() - dbwithttlimpl::ktslength);
    if (user_comp_filter_->filter(level, key, old_val_without_ts, new_val,
                                  value_changed)) {
      return true;
    }
    if (*value_changed) {
      new_val->append(
          old_val.data() + old_val.size() - dbwithttlimpl::ktslength,
          dbwithttlimpl::ktslength);
    }
    return false;
  }

  virtual const char* name() const override { return "delete by ttl"; }

 private:
  int32_t ttl_;
  env* env_;
  const compactionfilter* user_comp_filter_;
  std::unique_ptr<const compactionfilter> user_comp_filter_from_factory_;
};

class ttlcompactionfilterfactory : public compactionfilterfactory {
 public:
  ttlcompactionfilterfactory(
      int32_t ttl, env* env,
      std::shared_ptr<compactionfilterfactory> comp_filter_factory)
      : ttl_(ttl), env_(env), user_comp_filter_factory_(comp_filter_factory) {}

  virtual std::unique_ptr<compactionfilter> createcompactionfilter(
      const compactionfilter::context& context) {
    return std::unique_ptr<ttlcompactionfilter>(new ttlcompactionfilter(
        ttl_, env_, nullptr,
        std::move(user_comp_filter_factory_->createcompactionfilter(context))));
  }

  virtual const char* name() const override {
    return "ttlcompactionfilterfactory";
  }

 private:
  int32_t ttl_;
  env* env_;
  std::shared_ptr<compactionfilterfactory> user_comp_filter_factory_;
};

class ttlmergeoperator : public mergeoperator {

 public:
  explicit ttlmergeoperator(const std::shared_ptr<mergeoperator> merge_op,
                            env* env)
      : user_merge_op_(merge_op), env_(env) {
    assert(merge_op);
    assert(env);
  }

  virtual bool fullmerge(const slice& key, const slice* existing_value,
                         const std::deque<std::string>& operands,
                         std::string* new_value, logger* logger) const
      override {
    const uint32_t ts_len = dbwithttlimpl::ktslength;
    if (existing_value && existing_value->size() < ts_len) {
      log(logger, "error: could not remove timestamp from existing value.");
      return false;
    }

    // extract time-stamp from each operand to be passed to user_merge_op_
    std::deque<std::string> operands_without_ts;
    for (const auto& operand : operands) {
      if (operand.size() < ts_len) {
        log(logger, "error: could not remove timestamp from operand value.");
        return false;
      }
      operands_without_ts.push_back(operand.substr(0, operand.size() - ts_len));
    }

    // apply the user merge operator (store result in *new_value)
    bool good = true;
    if (existing_value) {
      slice existing_value_without_ts(existing_value->data(),
                                      existing_value->size() - ts_len);
      good = user_merge_op_->fullmerge(key, &existing_value_without_ts,
                                       operands_without_ts, new_value, logger);
    } else {
      good = user_merge_op_->fullmerge(key, nullptr, operands_without_ts,
                                       new_value, logger);
    }

    // return false if the user merge operator returned false
    if (!good) {
      return false;
    }

    // augment the *new_value with the ttl time-stamp
    int64_t curtime;
    if (!env_->getcurrenttime(&curtime).ok()) {
      log(logger,
          "error: could not get current time to be attached internally "
          "to the new value.");
      return false;
    } else {
      char ts_string[ts_len];
      encodefixed32(ts_string, (int32_t)curtime);
      new_value->append(ts_string, ts_len);
      return true;
    }
  }

  virtual bool partialmergemulti(const slice& key,
                                 const std::deque<slice>& operand_list,
                                 std::string* new_value, logger* logger) const
      override {
    const uint32_t ts_len = dbwithttlimpl::ktslength;
    std::deque<slice> operands_without_ts;

    for (const auto& operand : operand_list) {
      if (operand.size() < ts_len) {
        log(logger, "error: could not remove timestamp from value.");
        return false;
      }

      operands_without_ts.push_back(
          slice(operand.data(), operand.size() - ts_len));
    }

    // apply the user partial-merge operator (store result in *new_value)
    assert(new_value);
    if (!user_merge_op_->partialmergemulti(key, operands_without_ts, new_value,
                                           logger)) {
      return false;
    }

    // augment the *new_value with the ttl time-stamp
    int64_t curtime;
    if (!env_->getcurrenttime(&curtime).ok()) {
      log(logger,
          "error: could not get current time to be attached internally "
          "to the new value.");
      return false;
    } else {
      char ts_string[ts_len];
      encodefixed32(ts_string, (int32_t)curtime);
      new_value->append(ts_string, ts_len);
      return true;
    }
  }

  virtual const char* name() const override { return "merge by ttl"; }

 private:
  std::shared_ptr<mergeoperator> user_merge_op_;
  env* env_;
};
}
#endif  // rocksdb_lite
