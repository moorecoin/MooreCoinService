// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
#ifndef rocksdb_lite

#include "utilities/ttl/db_ttl_impl.h"

#include "rocksdb/utilities/db_ttl.h"
#include "db/filename.h"
#include "db/write_batch_internal.h"
#include "util/coding.h"
#include "rocksdb/env.h"
#include "rocksdb/iterator.h"

namespace rocksdb {

void dbwithttlimpl::sanitizeoptions(int32_t ttl, columnfamilyoptions* options,
                                    env* env) {
  if (options->compaction_filter) {
    options->compaction_filter =
        new ttlcompactionfilter(ttl, env, options->compaction_filter);
  } else {
    options->compaction_filter_factory =
        std::shared_ptr<compactionfilterfactory>(new ttlcompactionfilterfactory(
            ttl, env, options->compaction_filter_factory));
  }

  if (options->merge_operator) {
    options->merge_operator.reset(
        new ttlmergeoperator(options->merge_operator, env));
  }
}

// open the db inside dbwithttlimpl because options needs pointer to its ttl
dbwithttlimpl::dbwithttlimpl(db* db) : dbwithttl(db) {}

dbwithttlimpl::~dbwithttlimpl() { delete getoptions().compaction_filter; }

status utilitydb::openttldb(const options& options, const std::string& dbname,
                            stackabledb** dbptr, int32_t ttl, bool read_only) {
  dbwithttl* db;
  status s = dbwithttl::open(options, dbname, &db, ttl, read_only);
  if (s.ok()) {
    *dbptr = db;
  } else {
    *dbptr = nullptr;
  }
  return s;
}

status dbwithttl::open(const options& options, const std::string& dbname,
                       dbwithttl** dbptr, int32_t ttl, bool read_only) {

  dboptions db_options(options);
  columnfamilyoptions cf_options(options);
  std::vector<columnfamilydescriptor> column_families;
  column_families.push_back(
      columnfamilydescriptor(kdefaultcolumnfamilyname, cf_options));
  std::vector<columnfamilyhandle*> handles;
  status s = dbwithttl::open(db_options, dbname, column_families, &handles,
                             dbptr, {ttl}, read_only);
  if (s.ok()) {
    assert(handles.size() == 1);
    // i can delete the handle since dbimpl is always holding a reference to
    // default column family
    delete handles[0];
  }
  return s;
}

status dbwithttl::open(
    const dboptions& db_options, const std::string& dbname,
    const std::vector<columnfamilydescriptor>& column_families,
    std::vector<columnfamilyhandle*>* handles, dbwithttl** dbptr,
    std::vector<int32_t> ttls, bool read_only) {

  if (ttls.size() != column_families.size()) {
    return status::invalidargument(
        "ttls size has to be the same as number of column families");
  }

  std::vector<columnfamilydescriptor> column_families_sanitized =
      column_families;
  for (size_t i = 0; i < column_families_sanitized.size(); ++i) {
    dbwithttlimpl::sanitizeoptions(
        ttls[i], &column_families_sanitized[i].options,
        db_options.env == nullptr ? env::default() : db_options.env);
  }
  db* db;

  status st;
  if (read_only) {
    st = db::openforreadonly(db_options, dbname, column_families_sanitized,
                             handles, &db);
  } else {
    st = db::open(db_options, dbname, column_families_sanitized, handles, &db);
  }
  if (st.ok()) {
    *dbptr = new dbwithttlimpl(db);
  } else {
    *dbptr = nullptr;
  }
  return st;
}

status dbwithttlimpl::createcolumnfamilywithttl(
    const columnfamilyoptions& options, const std::string& column_family_name,
    columnfamilyhandle** handle, int ttl) {
  columnfamilyoptions sanitized_options = options;
  dbwithttlimpl::sanitizeoptions(ttl, &sanitized_options, getenv());

  return dbwithttl::createcolumnfamily(sanitized_options, column_family_name,
                                       handle);
}

status dbwithttlimpl::createcolumnfamily(const columnfamilyoptions& options,
                                         const std::string& column_family_name,
                                         columnfamilyhandle** handle) {
  return createcolumnfamilywithttl(options, column_family_name, handle, 0);
}

// appends the current timestamp to the string.
// returns false if could not get the current_time, true if append succeeds
status dbwithttlimpl::appendts(const slice& val, std::string* val_with_ts,
                               env* env) {
  val_with_ts->reserve(ktslength + val.size());
  char ts_string[ktslength];
  int64_t curtime;
  status st = env->getcurrenttime(&curtime);
  if (!st.ok()) {
    return st;
  }
  encodefixed32(ts_string, (int32_t)curtime);
  val_with_ts->append(val.data(), val.size());
  val_with_ts->append(ts_string, ktslength);
  return st;
}

// returns corruption if the length of the string is lesser than timestamp, or
// timestamp refers to a time lesser than ttl-feature release time
status dbwithttlimpl::sanitychecktimestamp(const slice& str) {
  if (str.size() < ktslength) {
    return status::corruption("error: value's length less than timestamp's\n");
  }
  // checks that ts is not lesser than kmintimestamp
  // gaurds against corruption & normal database opened incorrectly in ttl mode
  int32_t timestamp_value = decodefixed32(str.data() + str.size() - ktslength);
  if (timestamp_value < kmintimestamp) {
    return status::corruption("error: timestamp < ttl feature release time!\n");
  }
  return status::ok();
}

// checks if the string is stale or not according to ttl provided
bool dbwithttlimpl::isstale(const slice& value, int32_t ttl, env* env) {
  if (ttl <= 0) {  // data is fresh if ttl is non-positive
    return false;
  }
  int64_t curtime;
  if (!env->getcurrenttime(&curtime).ok()) {
    return false;  // treat the data as fresh if could not get current time
  }
  int32_t timestamp_value =
      decodefixed32(value.data() + value.size() - ktslength);
  return (timestamp_value + ttl) < curtime;
}

// strips the ts from the end of the string
status dbwithttlimpl::stripts(std::string* str) {
  status st;
  if (str->length() < ktslength) {
    return status::corruption("bad timestamp in key-value");
  }
  // erasing characters which hold the ts
  str->erase(str->length() - ktslength, ktslength);
  return st;
}

status dbwithttlimpl::put(const writeoptions& options,
                          columnfamilyhandle* column_family, const slice& key,
                          const slice& val) {
  writebatch batch;
  batch.put(column_family, key, val);
  return write(options, &batch);
}

status dbwithttlimpl::get(const readoptions& options,
                          columnfamilyhandle* column_family, const slice& key,
                          std::string* value) {
  status st = db_->get(options, column_family, key, value);
  if (!st.ok()) {
    return st;
  }
  st = sanitychecktimestamp(*value);
  if (!st.ok()) {
    return st;
  }
  return stripts(value);
}

std::vector<status> dbwithttlimpl::multiget(
    const readoptions& options,
    const std::vector<columnfamilyhandle*>& column_family,
    const std::vector<slice>& keys, std::vector<std::string>* values) {
  return std::vector<status>(
      keys.size(), status::notsupported("multiget not supported with ttl"));
}

bool dbwithttlimpl::keymayexist(const readoptions& options,
                                columnfamilyhandle* column_family,
                                const slice& key, std::string* value,
                                bool* value_found) {
  bool ret = db_->keymayexist(options, column_family, key, value, value_found);
  if (ret && value != nullptr && value_found != nullptr && *value_found) {
    if (!sanitychecktimestamp(*value).ok() || !stripts(value).ok()) {
      return false;
    }
  }
  return ret;
}

status dbwithttlimpl::merge(const writeoptions& options,
                            columnfamilyhandle* column_family, const slice& key,
                            const slice& value) {
  writebatch batch;
  batch.merge(column_family, key, value);
  return write(options, &batch);
}

status dbwithttlimpl::write(const writeoptions& opts, writebatch* updates) {
  class handler : public writebatch::handler {
   public:
    explicit handler(env* env) : env_(env) {}
    writebatch updates_ttl;
    status batch_rewrite_status;
    virtual status putcf(uint32_t column_family_id, const slice& key,
                         const slice& value) {
      std::string value_with_ts;
      status st = appendts(value, &value_with_ts, env_);
      if (!st.ok()) {
        batch_rewrite_status = st;
      } else {
        writebatchinternal::put(&updates_ttl, column_family_id, key,
                                value_with_ts);
      }
      return status::ok();
    }
    virtual status mergecf(uint32_t column_family_id, const slice& key,
                           const slice& value) {
      std::string value_with_ts;
      status st = appendts(value, &value_with_ts, env_);
      if (!st.ok()) {
        batch_rewrite_status = st;
      } else {
        writebatchinternal::merge(&updates_ttl, column_family_id, key,
                                  value_with_ts);
      }
      return status::ok();
    }
    virtual status deletecf(uint32_t column_family_id, const slice& key) {
      writebatchinternal::delete(&updates_ttl, column_family_id, key);
      return status::ok();
    }
    virtual void logdata(const slice& blob) { updates_ttl.putlogdata(blob); }

   private:
    env* env_;
  };
  handler handler(getenv());
  updates->iterate(&handler);
  if (!handler.batch_rewrite_status.ok()) {
    return handler.batch_rewrite_status;
  } else {
    return db_->write(opts, &(handler.updates_ttl));
  }
}

iterator* dbwithttlimpl::newiterator(const readoptions& opts,
                                     columnfamilyhandle* column_family) {
  return new ttliterator(db_->newiterator(opts, column_family));
}

}  // namespace rocksdb
#endif  // rocksdb_lite
