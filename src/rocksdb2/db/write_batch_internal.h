//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include "rocksdb/types.h"
#include "rocksdb/write_batch.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"

namespace rocksdb {

class memtable;

class columnfamilymemtables {
 public:
  virtual ~columnfamilymemtables() {}
  virtual bool seek(uint32_t column_family_id) = 0;
  // returns true if the update to memtable should be ignored
  // (useful when recovering from log whose updates have already
  // been processed)
  virtual uint64_t getlognumber() const = 0;
  virtual memtable* getmemtable() const = 0;
  virtual const options* getoptions() const = 0;
  virtual columnfamilyhandle* getcolumnfamilyhandle() = 0;
};

class columnfamilymemtablesdefault : public columnfamilymemtables {
 public:
  columnfamilymemtablesdefault(memtable* mem, const options* options)
      : ok_(false), mem_(mem), options_(options) {}

  bool seek(uint32_t column_family_id) override {
    ok_ = (column_family_id == 0);
    return ok_;
  }

  uint64_t getlognumber() const override { return 0; }

  memtable* getmemtable() const override {
    assert(ok_);
    return mem_;
  }

  const options* getoptions() const override {
    assert(ok_);
    return options_;
  }

  columnfamilyhandle* getcolumnfamilyhandle() override { return nullptr; }

 private:
  bool ok_;
  memtable* mem_;
  const options* const options_;
};

// writebatchinternal provides static methods for manipulating a
// writebatch that we don't want in the public writebatch interface.
class writebatchinternal {
 public:
  // writebatch methods with column_family_id instead of columnfamilyhandle*
  static void put(writebatch* batch, uint32_t column_family_id,
                  const slice& key, const slice& value);

  static void put(writebatch* batch, uint32_t column_family_id,
                  const sliceparts& key, const sliceparts& value);

  static void delete(writebatch* batch, uint32_t column_family_id,
                     const sliceparts& key);

  static void delete(writebatch* batch, uint32_t column_family_id,
                     const slice& key);

  static void merge(writebatch* batch, uint32_t column_family_id,
                    const slice& key, const slice& value);

  // return the number of entries in the batch.
  static int count(const writebatch* batch);

  // set the count for the number of entries in the batch.
  static void setcount(writebatch* batch, int n);

  // return the seqeunce number for the start of this batch.
  static sequencenumber sequence(const writebatch* batch);

  // store the specified number as the seqeunce number for the start of
  // this batch.
  static void setsequence(writebatch* batch, sequencenumber seq);

  static slice contents(const writebatch* batch) {
    return slice(batch->rep_);
  }

  static size_t bytesize(const writebatch* batch) {
    return batch->rep_.size();
  }

  static void setcontents(writebatch* batch, const slice& contents);

  // inserts batch entries into memtable
  // if dont_filter_deletes is false and options.filter_deletes is true,
  // then --> drops deletes in batch if db->keymayexist returns false
  // if ignore_missing_column_families == true. writebatch referencing
  // non-existing column family should be ignored.
  // however, if ignore_missing_column_families == false, any writebatch
  // referencing non-existing column family will return a invalidargument()
  // failure.
  //
  // if log_number is non-zero, the memtable will be updated only if
  // memtables->getlognumber() >= log_number
  static status insertinto(const writebatch* batch,
                           columnfamilymemtables* memtables,
                           bool ignore_missing_column_families = false,
                           uint64_t log_number = 0, db* db = nullptr,
                           const bool dont_filter_deletes = true);

  static void append(writebatch* dst, const writebatch* src);
};

}  // namespace rocksdb
