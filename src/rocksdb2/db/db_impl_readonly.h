//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2012 facebook. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file.

#pragma once
#include "db/db_impl.h"

#include <deque>
#include <set>
#include <vector>
#include <string>
#include "db/dbformat.h"
#include "db/log_writer.h"
#include "db/snapshot.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "port/port.h"

namespace rocksdb {

class dbimplreadonly : public dbimpl {
 public:
  dbimplreadonly(const dboptions& options, const std::string& dbname);
  virtual ~dbimplreadonly();

  // implementations of the db interface
  using db::get;
  virtual status get(const readoptions& options,
                     columnfamilyhandle* column_family, const slice& key,
                     std::string* value) override;

  // todo: implement readonly multiget?

  using dbimpl::newiterator;
  virtual iterator* newiterator(const readoptions&,
                                columnfamilyhandle* column_family) override;

  virtual status newiterators(
      const readoptions& options,
      const std::vector<columnfamilyhandle*>& column_families,
      std::vector<iterator*>* iterators) override;

  using dbimpl::put;
  virtual status put(const writeoptions& options,
                     columnfamilyhandle* column_family, const slice& key,
                     const slice& value) override {
    return status::notsupported("not supported operation in read only mode.");
  }
  using dbimpl::merge;
  virtual status merge(const writeoptions& options,
                       columnfamilyhandle* column_family, const slice& key,
                       const slice& value) override {
    return status::notsupported("not supported operation in read only mode.");
  }
  using dbimpl::delete;
  virtual status delete(const writeoptions& options,
                        columnfamilyhandle* column_family,
                        const slice& key) override {
    return status::notsupported("not supported operation in read only mode.");
  }
  virtual status write(const writeoptions& options,
                       writebatch* updates) override {
    return status::notsupported("not supported operation in read only mode.");
  }
  using dbimpl::compactrange;
  virtual status compactrange(columnfamilyhandle* column_family,
                              const slice* begin, const slice* end,
                              bool reduce_level = false, int target_level = -1,
                              uint32_t target_path_id = 0) override {
    return status::notsupported("not supported operation in read only mode.");
  }

#ifndef rocksdb_lite
  virtual status disablefiledeletions() override {
    return status::notsupported("not supported operation in read only mode.");
  }
  virtual status enablefiledeletions(bool force) override {
    return status::notsupported("not supported operation in read only mode.");
  }
  virtual status getlivefiles(std::vector<std::string>&,
                              uint64_t* manifest_file_size,
                              bool flush_memtable = true) override {
    return status::notsupported("not supported operation in read only mode.");
  }
#endif  // rocksdb_lite

  using dbimpl::flush;
  virtual status flush(const flushoptions& options,
                       columnfamilyhandle* column_family) override {
    return status::notsupported("not supported operation in read only mode.");
  }

 private:
  friend class db;

  // no copying allowed
  dbimplreadonly(const dbimplreadonly&);
  void operator=(const dbimplreadonly&);
};
}
